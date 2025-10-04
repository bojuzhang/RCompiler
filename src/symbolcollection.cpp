#include "symbolcollection.hpp"
#include "astnodes.hpp"
#include "scope.hpp"
#include <iostream>
#include <utility>

SymbolCollector::SymbolCollector() {
    root = std::make_shared<ScopeTree>();
}

void SymbolCollector::beginCollection() {
    auto globalScope = root->getCurrentScope();
    
    auto builtinTypes = {
        "i32", "i64", "f32", "f64", "bool", "char", "str", 
        "usize", "isize", "unit", "Self"
    };
    
    for (const auto& typeName : builtinTypes) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::BuiltinType, createSimpleType(typeName)
        );
        globalScope->insert(typeName, typeSymbol);
    }
}

void SymbolCollector::visit(Crate& node) {
    pushNode(node);
    
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    popNode();
}

void SymbolCollector::visit(Item& node) {
    pushNode(node);
    
    // 根据item的具体类型分发处理
    auto itemContent = std::move(node.item);
    if (itemContent) {
        itemContent->accept(*this);
    }
    
    popNode();
}

void SymbolCollector::visit(Function& node) {
    pushNode(node);
    
    bool wasInFunction = inFunction;
    std::string previousFunctionName = currentFunctionName;
    
    inFunction = true;
    currentFunctionName = node.identifier_name;
    
    root->enterScope(Scope::ScopeType::Function, &node);
    
    collectFunctionSymbol(node);
    
    collectParameterSymbols(node);
    
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
    }
    
    root->exitScope();
    
    inFunction = wasInFunction;
    currentFunctionName = previousFunctionName;
    popNode();
}

void SymbolCollector::visit(ConstantItem& node) {
    pushNode(node);
    
    collectConstantSymbol(node);
    
    auto expr = std::move(node.expression);
    if (expr) {
        expr->accept(*this);
    }
    
    popNode();
}

void SymbolCollector::visit(StructStruct& node) {
    pushNode(node);
    
    root->enterScope(Scope::ScopeType::Struct, &node);
    
    collectStructSymbol(node);
    
    collectFieldSymbols(node);
    
    root->exitScope();
    
    popNode();
}

void SymbolCollector::visit(Enumeration& node) {
    pushNode(node);
    
    root->enterScope(Scope::ScopeType::Enum, &node);
    
    collectEnumSymbol(node);
    
    collectVariantSymbols(node);
    
    root->exitScope();
    
    popNode();
}

void SymbolCollector::visit(InherentImpl& node) {
    pushNode(node);
    
    bool wasInImpl = inImpl;
    inImpl = true;
    
    root->enterScope(Scope::ScopeType::Impl, &node);
    
    collectImplSymbol(node);
    
    auto items = std::move(node.associateditems);
    for (const auto& item : items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    root->exitScope();
    
    inImpl = wasInImpl;
    popNode();
}

void SymbolCollector::visit(BlockExpression& node) {
    pushNode(node);
    
    root->enterScope(Scope::ScopeType::Block, &node);
    
    auto stmt = std::move(node.statement);
    if (stmt) {
        stmt->accept(*this);
    }
    
    root->exitScope();
    
    popNode();
}

void SymbolCollector::visit(InfiniteLoopExpression& node) {
    pushNode(node);
    
    bool wasInLoop = inLoop;
    inLoop = true;
    
    root->enterScope(Scope::ScopeType::Loop, &node);
    
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
    }
    
    root->exitScope();
    
    inLoop = wasInLoop;
    popNode();
}

void SymbolCollector::visit(PredicateLoopExpression& node) {
    pushNode(node);
    
    bool wasInLoop = inLoop;
    inLoop = true;
    
    root->enterScope(Scope::ScopeType::Loop, &node);
    
    auto conditions = std::move(node.conditions);
    if (conditions) {
        conditions->accept(*this);
    }
    
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
    }
    
    root->exitScope();
    
    inLoop = wasInLoop;
    popNode();
}

void SymbolCollector::collectFunctionSymbol(Function& node) {
    std::string funcName = node.identifier_name;
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},  // 参数稍后添加
        resolveTypeFromNode(*node.functionreturntype->type),
        false
    );
    
    root->insertSymbol(funcName, funcSymbol);
}

void SymbolCollector::collectConstantSymbol(ConstantItem& node) {
    std::string constName = node.identifier;
    
    auto constSymbol = std::make_shared<ConstantSymbol>(
        constName,
        resolveTypeFromNode(*node.type)
    );
    
    root->insertSymbol(constName, constSymbol);
}

void SymbolCollector::collectStructSymbol(StructStruct& node) {
    std::string structName = node.identifier;
    
    auto structSymbol = std::make_shared<StructSymbol>(structName);
    
    root->insertSymbol(structName, structSymbol);
}

void SymbolCollector::collectParameterSymbols(Function& node) {
    auto params = std::move(node.functionparameters);
    if (!params) return;
    
    for (const auto& param : params->functionparams) {  // 需要添加getter
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(param->patternnotopalt.get())) {
            std::string paramName = identPattern->identifier;
            
            auto paramSymbol = std::make_shared<Symbol>(
                paramName,
                SymbolKind::Variable,
                resolveTypeFromNode(*param->type),
                false,
                &node
            );
            
            root->insertSymbol(paramName, paramSymbol);
        }
    }
}

void SymbolCollector::collectFieldSymbols(StructStruct& node) {
    auto fields = std::move(node.structfileds);
    if (!fields) return;
    
    for (const auto& field : fields->structfields) {  // 需要添加getter
        std::string fieldName = field->identifier;
        
        auto fieldSymbol = std::make_shared<Symbol>(
            fieldName,
            SymbolKind::Variable,
            resolveTypeFromNode(*field->type),
            false,
            &node
        );
        
        root->insertSymbol(fieldName, fieldSymbol);
    }
}

void SymbolCollector::pushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void SymbolCollector::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* SymbolCollector::getCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

std::shared_ptr<Type> SymbolCollector::resolveTypeFromNode(Type& node) {
    return createSimpleType("unknown");
}

std::shared_ptr<Type> SymbolCollector::createSimpleType(const std::string& name) {
    // return std::make_shared<SimpleType>(name);
}
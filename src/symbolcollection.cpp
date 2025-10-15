#include "symbolcollection.hpp"
#include "astnodes.hpp"
#include "scope.hpp"
#include <utility>
#include <iostream>

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
    if (node.item) {
        node.item->accept(*this);
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
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->exitScope();
    
    inFunction = wasInFunction;
    currentFunctionName = previousFunctionName;
    popNode();
}

void SymbolCollector::visit(ConstantItem& node) {
    pushNode(node);
    
    collectConstantSymbol(node);
    
    if (node.expression) {
        node.expression->accept(*this);
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
    
    for (const auto& item : node.associateditems) {
        if (item) {
            item->accept(*this);
        }
    }
    
    root->exitScope();
    
    inImpl = wasInImpl;
    popNode();
}

void SymbolCollector::visit(Statement& node) {
    pushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    
    popNode();
}

void SymbolCollector::visit(LetStatement& node) {
    pushNode(node);
    
    // 收集变量符号
    if (node.patternnotopalt) {
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
            std::string varName = identPattern->identifier;
            
            // 获取变量类型
            std::shared_ptr<SemanticType> varType;
            if (node.type) {
                varType = resolveTypeFromNode(*node.type);
            } else {
                varType = createSimpleType("inferred");
            }
            
            auto varSymbol = std::make_shared<Symbol>(
                varName,
                SymbolKind::Variable,
                varType,
                identPattern->hasmut,  // 使用模式中的可变性标志
                &node
            );
            
            root->insertSymbol(varName, varSymbol);
        }
    }
    
    // 访问初始化表达式
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    popNode();
}

void SymbolCollector::visit(BlockExpression& node) {
    pushNode(node);
    
    root->enterScope(Scope::ScopeType::Block, &node);

    for (const auto &stmt : node.statements) {
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
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
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
    
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->exitScope();
    
    inLoop = wasInLoop;
    popNode();
}

void SymbolCollector::collectFunctionSymbol(Function& node) {
    std::string funcName = node.identifier_name;
    
    std::shared_ptr<SemanticType> returnType;
    if (node.functionreturntype != nullptr && node.functionreturntype->type != nullptr) {
        returnType = resolveTypeFromNode(*node.functionreturntype->type);
    } else {
        returnType = createSimpleType("unit");
    }
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},  // 参数稍后添加
        returnType,
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

void SymbolCollector::collectEnumSymbol(Enumeration& node) {
    std::string enumName = node.identifier;
    
    auto enumSymbol = std::make_shared<EnumSymbol>(enumName);
    
    if (!root->insertSymbol(enumName, enumSymbol)) {
        // reportError("Enum '" + enumName + "' is already defined in this scope");
        return;
    }
        
    root->enterScope(Scope::ScopeType::Enum, &node);
    
    collectVariantSymbols(node);
    
    root->exitScope();
}

void SymbolCollector::collectVariantSymbols(Enumeration& node) {
    if (!node.enumvariants) {
        return;
    }
    
    std::string enumName = node.identifier;
    
    for (const auto& variant : node.enumvariants->enumvariants) {
        std::string variantName = variant->identifier;
        
        VariantSymbol::VariantKind variantKind = VariantSymbol::VariantKind::Unit;
        
        variantKind = VariantSymbol::VariantKind::Unit;
        
        auto variantSymbol = std::make_shared<VariantSymbol>(variantName, variantKind);
        
        if (!root->insertSymbol(variantName, variantSymbol)) {
            // reportError("Variant '" + variantName + "' is already defined in enum '" + enumName + "'");
            continue;
        }
        
        auto enumSymbol = std::dynamic_pointer_cast<EnumSymbol>(root->lookupSymbol(enumName));
        if (enumSymbol) {
            enumSymbol->variants.push_back(variantSymbol);
        }
    }
}

void SymbolCollector::collectImplSymbol(InherentImpl& node) {
    auto targetType = getImplTargetType(node);
    
    std::string targetTypeName = targetType->tostring();
    
    std::string implName = "impl_" + targetTypeName + "_" + 
                          std::to_string(reinterpret_cast<uintptr_t>(&node));
    
    auto implSymbol = std::make_shared<ImplSymbol>(implName, targetType);
    
    std::string traitName = getTraitNameFromImpl(node);
    if (!traitName.empty()) {
        implSymbol->traitName = traitName;
        implSymbol->isTraitImpl = true;
    }
    
    root->insertSymbol(implName, implSymbol);
    
    root->enterScope(Scope::ScopeType::Impl, &node);
    
    auto selfTypeSymbol = std::make_shared<Symbol>(
        "Self", SymbolKind::TypeAlias, targetType
    );
    root->insertSymbol("Self", selfTypeSymbol);
    
    auto items = std::move(node.associateditems);
    for (const auto& item : items) {
        if (item) {
            collectAssociatedItem(*item, implSymbol);
        }
    }
    
    root->exitScope();
}

void SymbolCollector::collectAssociatedItem(AssociatedItem& item, 
                                            std::shared_ptr<ImplSymbol> implSymbol) {
    if (!item.consttantitem_or_function) return;
    
    if (auto function = dynamic_cast<Function*>(item.consttantitem_or_function.get())) {
        collectAssociatedFunction(*function, implSymbol);
    } else if (auto constant = dynamic_cast<ConstantItem*>(item.consttantitem_or_function.get())) {
        collectAssociatedConstant(*constant, implSymbol);
    }
}

 void SymbolCollector::collectAssociatedFunction(Function& function, 
                                                std::shared_ptr<ImplSymbol> implSymbol) {
    std::string funcName = function.identifier_name;
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},
        createSimpleType("unknown"),
        true
    );
    
    if (!root->insertSymbol(funcName, funcSymbol)) {
        // reportError("Method '" + funcName + "' is already defined in this impl");
        return;
    }
    
    implSymbol->items.push_back(funcSymbol);
    root->enterScope(Scope::ScopeType::Function, &function);
    if (function.functionparameters) {
        for (const auto& param : function.functionparameters->functionparams) {
            param->accept(*this);
        }
    }
    root->exitScope();
}

void SymbolCollector::collectAssociatedConstant(ConstantItem& constant, 
                                                std::shared_ptr<ImplSymbol> implSymbol) {
    std::string constName = constant.identifier;
    
    auto constSymbol = std::make_shared<ConstantSymbol>(
        constName,
        createSimpleType("unknown")  // 类型稍后处理
    );
    
    if (!root->insertSymbol(constName, constSymbol)) {
        // reportError("Associated constant '" + constName + "' is already defined in this impl");
        return;
    }
    
    implSymbol->items.push_back(constSymbol);
    
}

std::shared_ptr<SemanticType> SymbolCollector::getImplTargetType(InherentImpl& node) {
    // 从impl节点中提取目标类型
    // 这里需要根据你的AST结构实现具体逻辑
    // 简化实现：假设可以从node中获取类型信息
    return createSimpleType("UnknownType");
}

std::string SymbolCollector::getTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
    // 这里需要根据你的AST结构实现具体逻辑
    // 简化实现：返回空字符串表示固有实现
    return "";
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

std::shared_ptr<SemanticType> SymbolCollector::resolveTypeFromNode(Type& node) {
    return createSimpleType("unknown");
}

std::shared_ptr<SemanticType> SymbolCollector::createSimpleType(const std::string& name) {
    return std::make_shared<SimpleType>(name);
}
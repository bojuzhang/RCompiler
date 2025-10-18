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
    
    // 添加内置类型
    auto builtinTypes = {
        "i32", "i64", "u32", "u64", "bool", "char", "str", "usize", "isize", "unit"
    };
    for (const auto& typeName : builtinTypes) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::BuiltinType, nullptr, false, nullptr
        );
        globalScope->insert(typeName, typeSymbol);
    }
    
    // 添加内置函数
    auto builtinFunctions = {
        "print", "println", "printInt", "printlnInt", "getString", "getInt", "exit"
    };
    for (const auto& typeName : builtinFunctions) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::Function, nullptr, false, nullptr
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
    
    collectFunctionSymbol(node);
    root->enterScope(Scope::ScopeType::Function, &node);
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
            
            // 检查是否是 const 声明（在 Rust 中，const 可以在函数内部声明）
            // 这里我们需要从上下文判断，因为 LetStatement 本身没有 const 字段
            // 临时解决方案：检查变量名是否以 "const_" 开头或者检查初始化表达式是否是编译时常量
            bool isConstant = false;
            // 检查初始化表达式是否是字面量（简单的启发式方法）
            if (node.expression && dynamic_cast<LiteralExpression*>(node.expression.get())) {
                isConstant = true;
            }
            
            // 如果是常量，创建常量符号
            std::shared_ptr<Symbol> varSymbol;
            if (isConstant) {
                varSymbol = std::make_shared<ConstantSymbol>(
                    varName,
                    varType
                );
            } else {
                varSymbol = std::make_shared<Symbol>(
                    varName,
                    SymbolKind::Variable,
                    varType,
                    identPattern->hasmut,
                    &node
                );
            }
            
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
        std::vector<std::shared_ptr<Symbol>>{},
        returnType,
        false
    );
    
    // 确保函数符号的type字段也被设置
    funcSymbol->type = returnType;
    bool insertSuccess = root->insertSymbol(funcName, funcSymbol);
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
    auto params = node.functionparameters;
    if (!params) return;
    
    // 获取刚刚创建的函数符号，以便添加参数信息
    std::string funcName = node.identifier_name;
    auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(root->lookupSymbol(funcName));
    if (!funcSymbol) {
        // 如果找不到函数符号，创建一个临时的参数列表
        std::cerr << "Warning: Could not find function symbol for " << funcName << std::endl;
        return;
    }
    
    funcSymbol->parameters.clear();
    funcSymbol->parameterTypes.clear();
    for (const auto& param : params->functionparams) {
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(param->patternnotopalt.get())) {
            std::string paramName = identPattern->identifier;
            auto paramType = resolveTypeFromNode(*param->type);
            auto paramSymbol = std::make_shared<Symbol>(
                paramName,
                SymbolKind::Variable,
                paramType,
                false,
                &node
            );
            root->insertSymbol(paramName, paramSymbol);
            
            funcSymbol->parameters.push_back(paramSymbol);
            funcSymbol->parameterTypes.push_back(paramType);
        }
    }
}

void SymbolCollector::collectFieldSymbols(StructStruct& node) {
    auto fields = node.structfileds;
    if (!fields) return;
    
    for (const auto& field : fields->structfields) {
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
    
    auto items = node.associateditems;
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
    // 简化实现：假设可以从node中获取类型信息
    return createSimpleType("UnknownType");
}

std::string SymbolCollector::getTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
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
    if (auto typePath = dynamic_cast<TypePath*>(&node)) {
        if (typePath->simplepathsegement) {
            std::string typeName = typePath->simplepathsegement->identifier;
            if (!typeName.empty()) {
                return createSimpleType(typeName);
            }
        }
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&node)) {
        auto elementType = resolveTypeFromNode(*arrayType->type);
        if (elementType) {
            return std::make_shared<ArrayTypeWrapper>(elementType, arrayType->expression.get());
        }
    } else if (auto refType = dynamic_cast<ReferenceType*>(&node)) {
        auto targetType = resolveTypeFromNode(*refType->type);
        if (targetType) {
            return std::make_shared<ReferenceTypeWrapper>(targetType, refType->ismut);
        }
    }
    return createSimpleType("unit");
}

std::shared_ptr<SemanticType> SymbolCollector::createSimpleType(const std::string& name) {
    return std::make_shared<SimpleType>(name);
}
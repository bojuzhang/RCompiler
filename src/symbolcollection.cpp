#include "symbolcollection.hpp"
#include "astnodes.hpp"
#include "scope.hpp"
#include <utility>
#include <iostream>

SymbolCollector::SymbolCollector() {
    root = std::make_shared<ScopeTree>();
    hasError = false;
}

void SymbolCollector::BeginCollection() {
    auto globalScope = root->GetCurrentScope();
    
    // 添加内置类型
    auto builtinTypes = {
        "i32", "i64", "u32", "u64", "bool", "char", "str", "usize", "isize", "unit"
    };
    for (const auto& typeName : builtinTypes) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::BuiltinType, nullptr, false, nullptr
        );
        globalScope->Insert(typeName, typeSymbol);
    }
    
    // 添加内置函数
    auto builtinFunctions = {
        "print", "println", "printInt", "printlnInt", "getString", "getInt", "exit"
    };
    for (const auto& typeName : builtinFunctions) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::Function, nullptr, false, nullptr
        );
        globalScope->Insert(typeName, typeSymbol);
    }
}

void SymbolCollector::visit(Crate& node) {
    PushNode(node);
    
    // 第一遍：收集所有结构体、枚举、函数等符号
    for (const auto& item : node.items) {
        if (item) {
            if (auto structItem = dynamic_cast<StructStruct*>(item->item.get())) {
                structItem->accept(*this);
            } else if (auto enumItem = dynamic_cast<Enumeration*>(item->item.get())) {
                enumItem->accept(*this);
            } else if (auto funcItem = dynamic_cast<Function*>(item->item.get())) {
                funcItem->accept(*this);
            } else if (auto constItem = dynamic_cast<ConstantItem*>(item->item.get())) {
                constItem->accept(*this);
            }
        }
    }
    
    // 第二遍：处理 impl 块
    for (const auto& item : node.items) {
        if (item) {
            if (auto implItem = dynamic_cast<InherentImpl*>(item->item.get())) {
                implItem->accept(*this);
            }
        }
    }
    
    PopNode();
}

void SymbolCollector::visit(Item& node) {
    PushNode(node);
    if (node.item) {
        node.item->accept(*this);
    }
    PopNode();
}

void SymbolCollector::visit(Function& node) {
    PushNode(node);
    
    bool wasInFunction = inFunction;
    std::string previousFunctionName = currentFunctionName;
    inFunction = true;
    currentFunctionName = node.identifier_name;
    
    CollectFunctionSymbol(node);
    root->EnterScope(Scope::ScopeType::Function, &node);
    CollectParameterSymbols(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->ExitScope();
    inFunction = wasInFunction;
    currentFunctionName = previousFunctionName;
    PopNode();
}

void SymbolCollector::visit(ConstantItem& node) {
    PushNode(node);
    
    CollectConstantSymbol(node);
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(StructStruct& node) {
    PushNode(node);
    
    // 修复：先在全局作用域中插入结构体符号，然后再进入结构体作用域
    CollectStructSymbol(node);
    
    root->EnterScope(Scope::ScopeType::Struct, &node);
    CollectFieldSymbols(node);
    
    root->ExitScope();
    PopNode();
}

void SymbolCollector::visit(Enumeration& node) {
    PushNode(node);
    
    root->EnterScope(Scope::ScopeType::Enum, &node);
    CollectEnumSymbol(node);
    CollectVariantSymbols(node);
    
    root->ExitScope();
    PopNode();
}

void SymbolCollector::visit(InherentImpl& node) {
    PushNode(node);
    
    bool wasInImpl = inImpl;
    inImpl = true;
    
    root->EnterScope(Scope::ScopeType::Impl, &node);
    
    CollectImplSymbol(node);
    for (const auto& item : node.associateditems) {
        if (item) {
            item->accept(*this);
        }
    }
    
    root->ExitScope();
    inImpl = wasInImpl;
    PopNode();
}

void SymbolCollector::visit(Statement& node) {
    PushNode(node);
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    PopNode();
}

void SymbolCollector::visit(LetStatement& node) {
    PushNode(node);
    
    // 收集变量符号
    if (node.patternnotopalt) {
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
            std::string varName = identPattern->identifier;
            // 获取变量类型
            std::shared_ptr<SemanticType> varType;
            if (node.type) {
                varType = ResolveTypeFromNode(*node.type);
            } else {
                varType = CreateSimpleType("inferred");
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
            
            root->InsertSymbol(varName, varSymbol);
        }
    }
    
    // 访问初始化表达式
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(BlockExpression& node) {
    PushNode(node);
    
    root->EnterScope(Scope::ScopeType::Block, &node);
    for (const auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    root->ExitScope();
    
    PopNode();
}

void SymbolCollector::visit(InfiniteLoopExpression& node) {
    PushNode(node);
    
    bool wasInLoop = inLoop;
    inLoop = true;
    root->EnterScope(Scope::ScopeType::Loop, &node);
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    root->ExitScope();
    
    inLoop = wasInLoop;
    PopNode();
}

void SymbolCollector::visit(PredicateLoopExpression& node) {
    PushNode(node);
    
    bool wasInLoop = inLoop;
    inLoop = true;
    root->EnterScope(Scope::ScopeType::Loop, &node);
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->ExitScope();
    inLoop = wasInLoop;
    PopNode();
}

void SymbolCollector::CollectFunctionSymbol(Function& node) {
    std::string funcName = node.identifier_name;
    
    std::shared_ptr<SemanticType> returnType;
    if (node.functionreturntype != nullptr && node.functionreturntype->type != nullptr) {
        returnType = ResolveTypeFromNode(*node.functionreturntype->type);
    } else {
        returnType = CreateSimpleType("unit");
    }
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},
        returnType,
        false
    );
    
    // 确保函数符号的type字段也被设置
    funcSymbol->type = returnType;
    bool insertSuccess = root->InsertSymbol(funcName, funcSymbol);
}

void SymbolCollector::CollectConstantSymbol(ConstantItem& node) {
    std::string constName = node.identifier;
    auto constSymbol = std::make_shared<ConstantSymbol>(
        constName,
        ResolveTypeFromNode(*node.type)
    );
    root->InsertSymbol(constName, constSymbol);
}

void SymbolCollector::CollectStructSymbol(StructStruct& node) {
    std::string structName = node.identifier;
    auto structSymbol = std::make_shared<StructSymbol>(structName);
    root->InsertSymbol(structName, structSymbol);
}

void SymbolCollector::CollectParameterSymbols(Function& node) {
    auto params = node.functionparameters;
    if (!params) return;
    
    // 获取刚刚创建的函数符号，以便添加参数信息
    std::string funcName = node.identifier_name;
    auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(root->LookupSymbol(funcName));
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
            auto paramType = ResolveTypeFromNode(*param->type);
            // 检查参数模式是否是可变的
            bool isMutable = identPattern->hasmut;
            // 对于引用类型，需要检查引用本身是否可变
            if (auto refType = dynamic_cast<ReferenceType*>(param->type.get())) {
                isMutable = refType->ismut;
            }
            auto paramSymbol = std::make_shared<Symbol>(
                paramName,
                SymbolKind::Variable,
                paramType,
                isMutable,
                &node
            );
            root->InsertSymbol(paramName, paramSymbol);
            
            funcSymbol->parameters.push_back(paramSymbol);
            funcSymbol->parameterTypes.push_back(paramType);
        }
    }
}

void SymbolCollector::CollectFieldSymbols(StructStruct& node) {
    auto fields = node.structfileds;
    if (!fields) return;
    
    // 获取刚创建的StructSymbol
    std::string structName = node.identifier;
    auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(root->LookupSymbol(structName));
    if (!structSymbol) {
        ReportError("Could not find struct symbol for " + structName);
        return;
    }
    
    for (const auto& field : fields->structfields) {
        std::string fieldName = field->identifier;
        
        // 检查字段类型存在性
        auto fieldType = ResolveTypeFromNode(*field->type);
        if (!ValidateTypeExistence(*field->type)) {
            ReportError("Field type '" + fieldType->tostring() + "' does not exist for field '" + fieldName + "' in struct '" + structName + "'");
        }
        
        auto fieldSymbol = std::make_shared<Symbol>(
            fieldName,
            SymbolKind::Variable,
            fieldType,
            false,
            &node
        );
        
        // 插入到作用域
        root->InsertSymbol(fieldName, fieldSymbol);
        
        // 关键修复：添加到StructSymbol的fields列表
        structSymbol->fields.push_back(fieldSymbol);
    }
}

void SymbolCollector::CollectEnumSymbol(Enumeration& node) {
    std::string enumName = node.identifier;
    
    auto enumSymbol = std::make_shared<EnumSymbol>(enumName);
    
    if (!root->InsertSymbol(enumName, enumSymbol)) {
        // reportError("Enum '" + enumName + "' is already defined in this scope");
        return;
    }
        
    root->EnterScope(Scope::ScopeType::Enum, &node);
    CollectVariantSymbols(node);
    root->ExitScope();
}

void SymbolCollector::CollectVariantSymbols(Enumeration& node) {
    if (!node.enumvariants) {
        return;
    }
    
    std::string enumName = node.identifier;
    for (const auto& variant : node.enumvariants->enumvariants) {
        std::string variantName = variant->identifier;
        VariantSymbol::VariantKind variantKind = VariantSymbol::VariantKind::Unit;
        variantKind = VariantSymbol::VariantKind::Unit;
        
        auto variantSymbol = std::make_shared<VariantSymbol>(variantName, variantKind);
        if (!root->InsertSymbol(variantName, variantSymbol)) {
            // reportError("Variant '" + variantName + "' is already defined in enum '" + enumName + "'");
            continue;
        }
        
        auto enumSymbol = std::dynamic_pointer_cast<EnumSymbol>(root->LookupSymbol(enumName));
        if (enumSymbol) {
            enumSymbol->variants.push_back(variantSymbol);
        }
    }
}

void SymbolCollector::CollectImplSymbol(InherentImpl& node) {
    auto targetType = GetImplTargetType(node);
    if (!targetType) {
        ReportError("Invalid target type in impl");
        return;
    }
    
    std::string targetTypeName = targetType->tostring();
    std::string implName = "impl_" + targetTypeName + "_" +
                          std::to_string(reinterpret_cast<uintptr_t>(&node));
    auto implSymbol = std::make_shared<ImplSymbol>(implName, targetType);
    
    std::string traitName = GetTraitNameFromImpl(node);
    if (!traitName.empty()) {
        implSymbol->traitName = traitName;
        implSymbol->isTraitImpl = true;
    }
    root->InsertSymbol(implName, implSymbol);
    
    // 修复：在全局作用域中查找对应的StructSymbol
    auto globalScope = root->GetRootScope();
    auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(globalScope->Lookup(targetTypeName));
    if (!structSymbol) {
        ReportError("Cannot find struct '" + targetTypeName + "' for impl");
        return;
    }
    
    root->EnterScope(Scope::ScopeType::Impl, &node);
    
    // 修复：创建 Self 类型别名，指向目标类型
    auto selfTypeSymbol = std::make_shared<Symbol>(
        "Self", SymbolKind::TypeAlias, targetType
    );
    root->InsertSymbol("Self", selfTypeSymbol);
    
    auto items = node.associateditems;
    for (const auto& item : items) {
        if (item) {
            CollectAssociatedItem(*item, implSymbol, structSymbol);
        }
    }
    
    root->ExitScope();
}

void SymbolCollector::CollectAssociatedItem(AssociatedItem& item,
                                            std::shared_ptr<ImplSymbol> implSymbol,
                                            std::shared_ptr<StructSymbol> structSymbol) {
    if (!item.consttantitem_or_function) return;

    if (auto function = dynamic_cast<Function*>(item.consttantitem_or_function.get())) {
        CollectAssociatedFunction(*function, implSymbol, structSymbol);
    } else if (auto constant = dynamic_cast<ConstantItem*>(item.consttantitem_or_function.get())) {
        CollectAssociatedConstant(*constant, implSymbol, structSymbol);
    }
}

 void SymbolCollector::CollectAssociatedFunction(Function& function,
                                                 std::shared_ptr<ImplSymbol> implSymbol,
                                                 std::shared_ptr<StructSymbol> structSymbol) {
    std::string funcName = function.identifier_name;
    
    // 检查返回类型
    std::shared_ptr<SemanticType> returnType;
    if (function.functionreturntype && function.functionreturntype->type) {
        returnType = ResolveTypeFromNode(*function.functionreturntype->type);
    } else {
        returnType = CreateSimpleType("unit");
    }
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},
        returnType,
        true  // 是方法
    );
    
    // 修复：检查第一个参数是否为 self 参数
    if (function.functionparameters && !function.functionparameters->functionparams.empty()) {
        auto firstParam = function.functionparameters->functionparams[0];
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(firstParam->patternnotopalt.get())) {
            if (identPattern->identifier == "self") {
                // 这是 self 参数，标记为方法
                funcSymbol->isMethod = true;
                // self 参数的类型应该是 impl 的目标类型
                auto selfType = GetImplTargetType(*static_cast<InherentImpl*>(GetCurrentNode()));
                if (selfType) {
                    auto selfSymbol = std::make_shared<Symbol>(
                        "self", SymbolKind::Variable, selfType, false, &function
                    );
                    funcSymbol->parameters.push_back(selfSymbol);
                    funcSymbol->parameterTypes.push_back(selfType);
                }
            }
        }
    }
    
    if (!root->InsertSymbol(funcName, funcSymbol)) {
        ReportError("Method '" + funcName + "' is already defined in this impl");
        return;
    }
    
    implSymbol->items.push_back(funcSymbol);
    
    // 关键增强：如果有StructSymbol，也添加到其methods列表中
    if (structSymbol) {
        structSymbol->methods.push_back(funcSymbol);
    }
    
    root->EnterScope(Scope::ScopeType::Function, &function);
    if (function.functionparameters) {
        for (const auto& param : function.functionparameters->functionparams) {
            param->accept(*this);
        }
    }
    root->ExitScope();
}

void SymbolCollector::CollectAssociatedConstant(ConstantItem& constant,
                                                 std::shared_ptr<ImplSymbol> implSymbol,
                                                 std::shared_ptr<StructSymbol> structSymbol) {
    std::string constName = constant.identifier;
    
    auto constSymbol = std::make_shared<ConstantSymbol>(
        constName,
        CreateSimpleType("unknown")  // 类型稍后处理
    );
    
    if (!root->InsertSymbol(constName, constSymbol)) {
        // reportError("Associated constant '" + constName + "' is already defined in this impl");
        return;
    }
    implSymbol->items.push_back(constSymbol);
}

std::shared_ptr<SemanticType> SymbolCollector::GetImplTargetType(InherentImpl& node) {
    // 从impl节点中提取目标类型
    if (node.type) {
        return ResolveTypeFromNode(*node.type);
    }
    return nullptr;
}

std::string SymbolCollector::GetTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
    // 简化实现：返回空字符串表示固有实现
    return "";
}

void SymbolCollector::PushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void SymbolCollector::PopNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* SymbolCollector::GetCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

std::shared_ptr<SemanticType> SymbolCollector::ResolveTypeFromNode(Type& node) {
    if (auto typePath = dynamic_cast<TypePath*>(&node)) {
        if (typePath->simplepathsegement) {
            std::string typeName = typePath->simplepathsegement->identifier;
            if (!typeName.empty()) {
                // 修复：在符号收集阶段，不要检查类型是否存在，因为可能正在收集中
                return CreateSimpleType(typeName);
            }
        }
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&node)) {
        auto elementType = ResolveTypeFromNode(*arrayType->type);
        if (elementType) {
            return std::make_shared<ArrayTypeWrapper>(elementType, arrayType->expression.get());
        }
    } else if (auto refType = dynamic_cast<ReferenceType*>(&node)) {
        auto targetType = ResolveTypeFromNode(*refType->type);
        if (targetType) {
            return std::make_shared<ReferenceTypeWrapper>(targetType, refType->ismut);
        }
    }
    return CreateSimpleType("unit");
}

std::shared_ptr<SemanticType> SymbolCollector::CreateSimpleType(const std::string& name) {
    return std::make_shared<SimpleType>(name);
}

bool SymbolCollector::ValidateTypeExistence(const std::string& typeName) {
    auto symbol = root->LookupSymbol(typeName);
    return symbol && (symbol->kind == SymbolKind::Struct ||
                     symbol->kind == SymbolKind::Enum ||
                     symbol->kind == SymbolKind::BuiltinType ||
                     symbol->kind == SymbolKind::TypeAlias);
}

bool SymbolCollector::ValidateTypeExistence(Type& typeNode) {
    auto type = ResolveTypeFromNode(typeNode);
    if (!type) return false;
    
    std::string typeName = type->tostring();
    return ValidateTypeExistence(typeName);
}

void SymbolCollector::ReportError(const std::string& message) {
    hasError = true;
    std::cerr << "Symbol Collection Error: " << message << std::endl;
}

bool SymbolCollector::HasErrors() const {
    return hasError;
}
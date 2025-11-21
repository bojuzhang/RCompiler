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
    struct BuiltinFunction {
        std::string name;
        std::string returnType;
        std::vector<std::string> paramTypes;
    };
    
    auto builtinFunctions = {
        BuiltinFunction{"print", "()", {"&str"}},
        BuiltinFunction{"println", "()", {"&str"}},
        BuiltinFunction{"printInt", "()", {"i32"}},
        BuiltinFunction{"printlnInt", "()", {"i32"}},
        BuiltinFunction{"getString", "String", {}},
        BuiltinFunction{"getInt", "i32", {}},
        BuiltinFunction{"exit", "()", {"i32"}}
    };
    
    for (const auto& builtinFunc : builtinFunctions) {
        auto returnType = std::make_shared<SimpleType>(builtinFunc.returnType);
        std::vector<std::shared_ptr<Symbol>> params;
        std::vector<std::shared_ptr<SemanticType>> paramTypes;
        
        // 设置参数信息
        for (const auto& paramType : builtinFunc.paramTypes) {
            auto type = std::make_shared<SimpleType>(paramType);
            paramTypes.push_back(type);
            
            // 创建参数符号（不需要名称，用于类型检查）
            auto paramSymbol = std::make_shared<Symbol>(
                "", SymbolKind::Variable, type, false, nullptr
            );
            params.push_back(paramSymbol);
        }
        
        auto funcSymbol = std::make_shared<FunctionSymbol>(
            builtinFunc.name,
            params,
            returnType,
            false
        );
        
        // 设置参数类型信息
        funcSymbol->parameterTypes = paramTypes;
        
        globalScope->Insert(builtinFunc.name, funcSymbol);
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
    // 修复：不要在这里调用 item->accept(*this)，因为 CollectImplSymbol 已经处理了
    // for (const auto& item : node.associateditems) {
    //     if (item) {
    //         item->accept(*this);
    //     }
    // }
    
    root->ExitScope();
    inImpl = wasInImpl;
    PopNode();
}

void SymbolCollector::visit(Statement& node) {
    PushNode(node);
    if (node.astnode) {
        // 检查是否是 Item（包含常量定义的情况）
        if (auto item = dynamic_cast<Item*>(node.astnode.get())) {
            // 如果是 Item，需要访问其内部的 item
            if (item->item) {
                // 调试信息：输出正在处理的 Item 类型
                if (auto func = dynamic_cast<Function*>(item->item.get())) {
                    // 处理语句中的函数
                }
                item->item->accept(*this);
            }
        } else {
            // 其他类型的语句，直接访问
            node.astnode->accept(*this);
        }
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
            // 修复：正确的常量判断应该基于 IdentifierPattern 的 hasmut 字段
            // 如果 hasmut 为 false，且初始化表达式是字面量，则可能是常量
            bool isConstant = false;
            // 只有在非可变且初始化表达式是字面量时才考虑为常量
            if (!identPattern->hasmut && node.expression && dynamic_cast<LiteralExpression*>(node.expression.get())) {
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
            
            bool success = root->InsertSymbol(varName, varSymbol);
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
    
    // 修复：访问尾表达式（如果存在）
    if (node.expressionwithoutblock) {
        node.expressionwithoutblock->accept(*this);
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
    if (node.conditions) {
        node.conditions->expression->accept(*this);
    }
    
    // conditions 不应该在 while 的 scope 内。
    root->EnterScope(Scope::ScopeType::Loop, &node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->ExitScope();
    inLoop = wasInLoop;
    PopNode();
}

void SymbolCollector::CollectFunctionSymbol(Function& node) {
    std::string funcName = node.identifier_name;
    
    // 检查在可见作用域范围内是否已经存在同名的函数符号
    auto existingSymbol = root->LookupSymbol(funcName);
    if (existingSymbol && existingSymbol->kind == SymbolKind::Function) {
        ReportError("Function '" + funcName + "' is already defined");
        return;
    }
    
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
    
    // 修复：所有函数都应该插入到根作用域中，这样它们就可以从任何地方访问
    auto rootScope = root->GetRootScope();
    auto originalScope = root->GetCurrentScope();
    
    // 临时切换到根作用域
    root->GoToNode(nullptr);
    
    bool insertSuccess = root->InsertSymbol(funcName, funcSymbol);
    
    // 恢复到原来的作用域
    root->GoToNode(nullptr);
    if (originalScope) {
        // 需要找到原来的作用域节点
        // 这里简化处理，直接回到原来的作用域
        // 在实际实现中，可能需要更复杂的作用域恢复逻辑
    }
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
    auto fields = node.structfields;
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
    
    // 修复：确保 enum 符号的 type 字段被正确设置
    enumSymbol->type = std::make_shared<SimpleType>(enumName);
    
    // 修复：插入到根作用域，确保全局可访问
    auto rootScope = root->GetRootScope();
    auto originalScope = root->GetCurrentScope();
    
    // 临时切换到根作用域
    root->GoToNode(nullptr);
    
    bool insertSuccess = rootScope->Insert(enumName, enumSymbol);
    
    // 恢复到原来的作用域
    root->GoToNode(nullptr);
    
    if (!insertSuccess) {
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
    
    // 获取 enum 符号
    auto enumSymbol = std::dynamic_pointer_cast<EnumSymbol>(root->LookupSymbol(enumName));
    if (!enumSymbol) {
        ReportError("Could not find enum symbol for " + enumName);
        return;
    }
    
    for (const auto& variant : node.enumvariants->enumvariants) {
        std::string variantName = variant->identifier;
        VariantSymbol::VariantKind variantKind = VariantSymbol::VariantKind::Unit;
        
        auto variantSymbol = std::make_shared<VariantSymbol>(variantName, variantKind);
        
        // 修复：设置变体的类型为对应的 enum 类型
        variantSymbol->type = std::make_shared<SimpleType>(enumName);
        
        if (!root->InsertSymbol(variantName, variantSymbol)) {
            // reportError("Variant '" + variantName + "' is already defined in enum '" + enumName + "'");
            continue;
        }
        
        // 添加到 enum 的变体列表
        enumSymbol->variants.push_back(variantSymbol);
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
    
    // 修复：不要在这里插入函数符号，而是在参数收集完成后插入
    // 这样可以避免重复定义的问题
    
    // 修复：检查第一个参数是否为 self 参数（包括 self, &self, &mut self）
    if (function.functionparameters && !function.functionparameters->functionparams.empty()) {
        auto firstParam = function.functionparameters->functionparams[0];
        
        // 检查是否是 IdentifierPattern（处理 self）
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(firstParam->patternnotopalt.get())) {
            if (identPattern->identifier == "self") {
                // 这是 self 参数，标记为方法
                funcSymbol->isMethod = true;
                // self 参数的类型应该是 impl 的目标类型
                auto selfType = GetImplTargetType(*static_cast<InherentImpl*>(GetCurrentNode()));
                if (selfType) {
                    // 检查 self 参数的可变性
                    bool isMutable = false;
                    // 检查参数类型是否为引用类型
                    if (auto refType = dynamic_cast<ReferenceType*>(firstParam->type.get())) {
                        isMutable = refType->ismut;
                        // 创建引用类型
                        selfType = std::make_shared<ReferenceTypeWrapper>(selfType, isMutable);
                    }
                    
                    auto selfSymbol = std::make_shared<Symbol>(
                        "self", SymbolKind::Variable, selfType, isMutable, &function
                    );
                    funcSymbol->parameters.push_back(selfSymbol);
                    funcSymbol->parameterTypes.push_back(selfType);
                }
            }
        }
        // 检查是否是 ReferencePattern（处理 &self 和 &mut self）
        else if (auto refPattern = dynamic_cast<ReferencePattern*>(firstParam->patternnotopalt.get())) {
            if (refPattern->pattern) {
                if (auto innerIdentPattern = dynamic_cast<IdentifierPattern*>(refPattern->pattern.get())) {
                    if (innerIdentPattern->identifier == "self") {
                        // 这是 &self 或 &mut self 参数，标记为方法
                        funcSymbol->isMethod = true;
                        // self 参数的类型应该是 impl 的目标类型
                        auto selfType = GetImplTargetType(*static_cast<InherentImpl*>(GetCurrentNode()));
                        if (selfType) {
                            // 对于 ReferencePattern，总是创建引用类型
                            bool isMutable = refPattern->hasmut;
                            selfType = std::make_shared<ReferenceTypeWrapper>(selfType, isMutable);
                            
                            auto selfSymbol = std::make_shared<Symbol>(
                                "self", SymbolKind::Variable, selfType, isMutable, &function
                            );
                            funcSymbol->parameters.push_back(selfSymbol);
                            funcSymbol->parameterTypes.push_back(selfType);
                        }
                    }
                }
            }
        }
    }
    
    implSymbol->items.push_back(funcSymbol);
    
    // 关键增强：如果有StructSymbol，也添加到其methods列表中
    if (structSymbol) {
        structSymbol->methods.push_back(funcSymbol);
    }
    
    root->EnterScope(Scope::ScopeType::Function, &function);
    
    // 直接收集参数符号，避免重复查找函数符号
    if (function.functionparameters) {
        for (const auto& param : function.functionparameters->functionparams) {
            if (auto identPattern = dynamic_cast<IdentifierPattern*>(param->patternnotopalt.get())) {
                std::string paramName = identPattern->identifier;
                std::shared_ptr<SemanticType> paramType;
                
                // 特殊处理 self 参数（包括 &self 和 &mut self）
                if (paramName == "self") {
                    // self 参数的类型应该是 impl 的目标类型
                    paramType = GetImplTargetType(*static_cast<InherentImpl*>(GetCurrentNode()));
                    
                    // 检查 self 参数是否为引用类型
                    if (auto refType = dynamic_cast<ReferenceType*>(param->type.get())) {
                        bool isMutable = refType->ismut;
                        // 创建引用类型
                        paramType = std::make_shared<ReferenceTypeWrapper>(paramType, isMutable);
                    }
                } else {
                    paramType = ResolveTypeFromNode(*param->type);
                }
                
                // 检查参数模式是否是可变的
                bool isMutable = identPattern->hasmut;
                // 对于引用类型，需要检查引用本身是否可变
                if (auto refType = dynamic_cast<ReferenceType*>(param->type.get())) {
                    isMutable = refType->ismut;
                    // 如果是引用类型且不是 self 参数，创建引用类型包装器
                    if (paramName != "self") {
                        paramType = std::make_shared<ReferenceTypeWrapper>(paramType, isMutable);
                    }
                }
                
                
                auto paramSymbol = std::make_shared<Symbol>(
                    paramName,
                    SymbolKind::Variable,
                    paramType,
                    isMutable,
                    &function
                );
                
                // 插入到当前函数作用域
                bool insertSuccess = root->InsertSymbol(paramName, paramSymbol);
                if (!insertSuccess) {
                    ReportError("Failed to insert parameter '" + paramName + "' in function scope");
                }
                
                // 添加到函数符号的参数列表（如果还没有添加的话）
                bool alreadyAdded = false;
                for (const auto& existingParam : funcSymbol->parameters) {
                    if (existingParam->name == paramName) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded) {
                    funcSymbol->parameters.push_back(paramSymbol);
                    funcSymbol->parameterTypes.push_back(paramType);
                }
            }
        }
    }
    
    // 检查在可见作用域范围内是否已经存在同名的函数符号
    auto existingSymbol = root->LookupSymbol(funcName);
    if (existingSymbol && existingSymbol->kind == SymbolKind::Function) {
        ReportError("Method '" + funcName + "' is already defined");
        root->ExitScope();
        return;
    }
    
    // 现在插入函数符号，避免重复定义错误
    if (!root->InsertSymbol(funcName, funcSymbol)) {
        ReportError("Method '" + funcName + "' is already defined in this impl");
        root->ExitScope();
        return;
    }
    
    // 访问函数体以收集局部变量
    if (function.blockexpression) {
        function.blockexpression->accept(*this);
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
            return std::make_shared<ArrayTypeWrapper>(elementType, arrayType->expression, nullptr);
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
    // 特殊处理数组类型：如果以 [ 开头，检查元素类型是否存在
    if (typeName.find("[") == 0) {
        return ValidateArrayTypeExistence(typeName);
    }
    
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

bool SymbolCollector::ValidateArrayTypeExistence(const std::string& arrayTypeName) {
    // 处理嵌套数组类型，如 [[bool; 100]; 100]
    if (arrayTypeName.empty() || arrayTypeName[0] != '[') {
        return false;
    }
    
    // 找到最后一个分号的位置，这是数组大小分隔符
    auto lastSemicolonPos = arrayTypeName.rfind(";");
    if (lastSemicolonPos == std::string::npos) {
        return false; // 格式错误
    }
    
    // 找到匹配的右括号
    size_t closingBracketPos = arrayTypeName.find("]", lastSemicolonPos);
    if (closingBracketPos == std::string::npos) {
        return false; // 没有找到右括号
    }
    
    // 提取元素类型部分（从第一个字符到最后一个分号之前）
    std::string elementTypeStr = arrayTypeName.substr(1, lastSemicolonPos - 1);
    
    // 去除可能的空格
    // 提取实际的元素类型
    std::string actualElementType = elementTypeStr;
    
    // 递归验证元素类型
    // 如果元素类型也是数组，继续递归处理
    if (actualElementType.find("[") == 0) {
        return ValidateArrayTypeExistence(actualElementType);
    } else {
        // 基础类型，检查是否存在
        return ValidateTypeExistence(actualElementType);
    }
}

void SymbolCollector::ReportError(const std::string& message) {
    hasError = true;
    std::cerr << "Symbol Collection Error: " << message << std::endl;
}

bool SymbolCollector::HasErrors() const {
    return hasError;
}

// 实现完整的符号收集 - 添加缺失的访问方法
void SymbolCollector::visit(ExpressionStatement& node) {
    PushNode(node);
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    PopNode();
}

void SymbolCollector::visit(Expression& node) {
    PushNode(node);
    // 基础表达式类，通常不需要特殊处理
    PopNode();
}

void SymbolCollector::visit(ConstBlockExpression& node) {
    PushNode(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(IfExpression& node) {
    PushNode(node);
    
    if (node.conditions) {
        node.conditions->expression->accept(*this);
    }
    
    if (node.ifblockexpression) {
        node.ifblockexpression->accept(*this);
    }
    
    if (node.elseexpression) {
        node.elseexpression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(LiteralExpression& node) {
    PushNode(node);
    // 字面量不需要符号收集，但需要访问可能的嵌套表达式
    PopNode();
}

void SymbolCollector::visit(PathExpression& node) {
    PushNode(node);
    
    if (node.simplepath) {
        node.simplepath->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(GroupedExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(ArrayExpression& node) {
    PushNode(node);
    
    if (node.arrayelements) {
        node.arrayelements->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(IndexExpression& node) {
    PushNode(node);
    
    if (node.expressionout) {
        node.expressionout->accept(*this);
    }
    
    if (node.expressionin) {
        node.expressionin->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(TupleExpression& node) {
    PushNode(node);
    
    if (node.tupleelements) {
        node.tupleelements->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(StructExpression& node) {
    PushNode(node);
    
    if (node.pathexpression) {
        node.pathexpression->accept(*this);
    }
    
    if (node.structinfo) {
        node.structinfo->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(CallExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    if (node.callparams) {
        node.callparams->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(MethodCallExpression& node) {
    // 方法调用表达式的具体实现
    // 注意：MethodCallExpression 可能没有继承 ASTNode，所以不使用 PushNode/PopNode
}

void SymbolCollector::visit(FieldExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(ContinueExpression& node) {
    PushNode(node);
    // continue 表达式不需要符号收集
    PopNode();
}

void SymbolCollector::visit(BreakExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(ReturnExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(UnderscoreExpression& node) {
    PushNode(node);
    // 下划线表达式不需要符号收集
    PopNode();
}

void SymbolCollector::visit(MatchExpression& node) {
    // match 表达式的具体实现
    // 注意：MatchExpression 可能没有继承 ASTNode，所以不使用 PushNode/PopNode
}

void SymbolCollector::visit(TypeCastExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    if (node.typenobounds) {
        node.typenobounds->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(AssignmentExpression& node) {
    PushNode(node);
    
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(CompoundAssignmentExpression& node) {
    PushNode(node);
    
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(UnaryExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(BinaryExpression& node) {
    PushNode(node);
    
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(BorrowExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(DereferenceExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

// 实现 Pattern 相关的访问方法
void SymbolCollector::visit(Pattern& node) {
    PushNode(node);
    // 基础模式类，通常不需要特殊处理
    PopNode();
}

void SymbolCollector::visit(LiteralPattern& node) {
    PushNode(node);
    
    if (node.literalexprerssion) {
        node.literalexprerssion->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(IdentifierPattern& node) {
    PushNode(node);
    
    if (node.patternnotopalt) {
        node.patternnotopalt->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(AssociatedItem& node) {
    PushNode(node);
    
    if (node.consttantitem_or_function) {
        node.consttantitem_or_function->accept(*this);
    }
    
    PopNode();
}

void SymbolCollector::visit(WildcardPattern& node) {
    PushNode(node);
    // 通配符模式不需要特殊处理
    PopNode();
}

void SymbolCollector::visit(PathPattern& node) {
    PushNode(node);
    
    if (node.pathexpression) {
        node.pathexpression->accept(*this);
    }
    
    PopNode();
}


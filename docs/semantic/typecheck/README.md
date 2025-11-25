# TypeChecker（类型检查）组件分析

## 概述

TypeChecker 是语义分析的第四个也是最后一个阶段，负责进行完整的类型检查和类型推断。这个组件利用前面阶段收集的符号信息、常量值和控制流信息，对整个程序进行类型系统的验证。

## 整体工作逻辑

### 主要职责

1. **类型检查** - 验证表达式和语句的类型正确性
2. **类型推断** - 推断表达式的类型
3. **类型兼容性检查** - 检查类型之间的兼容性
4. **可变性检查** - 验证变量赋值和修改的可变性规则
5. **函数调用检查** - 验证函数调用的参数类型和数量
6. **方法调用检查** - 验证方法调用和 self 参数
7. **数组类型检查** - 验证数组类型和大小匹配

### 处理流程

```
开始 → 遍历AST → 类型检查 → 类型推断 → 兼容性验证 → 存储 → 完成
```

## 核心接口分析

### 主要类型检查器类

```cpp
class TypeChecker : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;                              // 作用域树
    std::shared_ptr<ConstantEvaluator> constantEvaluator;                // 常量求值器
    std::stack<ASTNode*> nodeStack;                                   // AST节点栈
    std::stack<std::shared_ptr<SemanticType>> expectedTypeStack;       // 期望类型栈
    
    bool hasErrors = false;                                             // 错误标志
    std::string currentStruct;                                          // 当前结构体名
    std::string currentTrait;                                           // 当前trait名
    std::string currentImpl;                                            // 当前impl名
    std::unordered_set<std::string> currentTraitRequirements;           // 当前trait要求
    
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeCache; // 类型缓存
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap; // 节点类型映射
    
    std::unordered_map<std::string, std::unordered_set<std::string>> structMethods; // 结构体方法
    std::unordered_map<std::string, std::unordered_set<std::string>> traitRequirements; // trait要求
    std::unordered_map<std::string, std::unordered_set<std::string>> traitImplementations; // trait实现
    std::unordered_map<std::string, std::string> implToTraitMap; // impl到trait的映射
};
```

### 核心接口方法

#### 1. 初始化和状态查询接口

```cpp
TypeChecker(std::shared_ptr<ScopeTree> scopeTree, 
           std::shared_ptr<ConstantEvaluator> constantEvaluator = nullptr);
bool CheckTypes();
bool HasTypeErrors() const;
```

**功能**：初始化类型检查器并提供状态查询

**实现细节**：
- 构造函数接收前面阶段的作用域树和常量求值器
- `CheckTypes()` 启动类型检查过程
- `HasTypeErrors()` 返回是否有类型错误

#### 2. AST访问接口

##### 程序根访问
```cpp
void visit(Crate& node) override;
```

**功能**：处理程序根节点，遍历所有顶级项

**实现细节**：
- 遍历所有顶级项
- 检查 trait 实现的完整性

**代码示例**：
```cpp
void TypeChecker::visit(Crate& node) {
    PushNode(node);
        
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }

    for (const auto& [traitName, implementations] : traitImplementations) {
        auto requirementsIt = traitRequirements.find(traitName);
        if (requirementsIt != traitRequirements.end()) {
            const auto& requirements = requirementsIt->second;
            for (const auto& requirement : requirements) {
                if (implementations.find(requirement) == implementations.end()) {
                    ReportMissingTraitImplementation(traitName, requirement);
                }
            }
        }
    }
    
    PopNode();
}
```

##### 函数访问
```cpp
void visit(Function& node) override;
```

**功能**：处理函数定义，进行完整的类型检查

**实现细节**：
- 检查函数签名
- 进入函数作用域
- 检查参数类型
- 检查函数体和返回类型
- 分析返回语句

**代码示例**：
```cpp
void TypeChecker::visit(Function& node) {
    PushNode(node);
    
    // 进入函数作用域，使用符号收集阶段已经创建的作用域
    scopeTree->EnterExistingScope(&node);

    if (node.functionparameters) {
        CheckFunctionParameters(*node.functionparameters);
    }
    
    // 检查返回类型
    if (node.functionreturntype) {
        CheckFunctionReturnType(*node.functionreturntype);
    }
    
    // 检查函数体
    CheckFunctionBody(node);
    
    // 退出函数作用域
    scopeTree->ExitScope();
    
    PopNode();
}
```

##### Let 语句访问
```cpp
void visit(LetStatement& node) override;
```

**功能**：处理变量声明和初始化

**实现细节**：
- 检查声明的类型（如果有）
- 推断初始化表达式的类型
- 检查类型兼容性
- 创建变量符号并插入到作用域

**代码示例**：
```cpp
void TypeChecker::visit(LetStatement& node) {
    PushNode(node);
    
    std::shared_ptr<SemanticType> declaredType = nullptr;
    if (node.type) {
        declaredType = CheckType(*node.type);
        if (!declaredType) {
            ReportError("Invalid type in let statement");
            PopNode();
            return;
        }
    }
    
    // 首先访问初始化表达式，这样其中的 CallExpression 会被正确处理
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    // 检查初始化表达式
    if (node.expression) {
        if (declaredType) {
            // 对于有明确类型声明的let语句，进行完整的类型检查，包括数组长度验证
            std::shared_ptr<SemanticType> initType = nullptr;
            
            // 如果是数组表达式，使用带期望类型的推断方法
            if (auto arrayExpr = dynamic_cast<ArrayExpression*>(node.expression.get())) {
                initType = InferArrayExpressionTypeWithExpected(*arrayExpr, declaredType);
            } else {
                initType = InferExpressionType(*node.expression);
            }
            
            // 检查类型兼容性
            if (!AreTypesCompatible(declaredType, initType)) {
                ReportError("Type mismatch in let statement: expected '" + declaredType->tostring() +
                           "', found '" + initType->tostring() + "'");
                PopNode();
                return;
            }
        } else {
            // 没有声明类型，使用普通推断
            auto initType = InferExpressionType(*node.expression);
            if (!initType) {
                ReportError("Unable to infer type for let statement");
                PopNode();
                return;
            }
        }
    }
    
    // 在类型检查阶段收集变量符号
    if (node.patternnotopalt) {
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
            std::string varName = identPattern->identifier;
            
            // 获取变量类型
            std::shared_ptr<SemanticType> varType;
            if (node.type) {
                varType = declaredType;
            } else if (node.expression) {
                // 如果没有声明类型，从初始化表达式推断
                varType = InferExpressionType(*node.expression);
                if (!varType) {
                    ReportError("Unable to infer type for let statement for variable: " + varName);
                    PopNode();
                    return;
                }
            } else {
                ReportError("Let statement without type annotation or initialization expression");
                PopNode();
                return;
            }
            
            // 创建变量符号
            std::shared_ptr<Symbol> varSymbol;
            varSymbol = std::make_shared<Symbol>(
                varName,
                SymbolKind::Variable,
                varType,
                identPattern->hasmut,
                &node
            );
            
            // 插入到符号表中，变量可以覆盖同名变量，标记为来自 typecheck
            bool success = scopeTree->InsertSymbol(varName, varSymbol, true);
            if (!success) {
                auto existingSymbol = scopeTree->LookupSymbolInCurrentScope(varName);
                if (existingSymbol && existingSymbol->kind != SymbolKind::Variable) {
                    ReportError("Cannot shadow non-variable symbol: " + varName);
                }
            }
        }
    }
    
    PopNode();
}
```

#### 3. 表达式类型推断接口

##### 通用表达式推断
```cpp
std::shared_ptr<SemanticType> InferExpressionType(Expression& expr);
```

**功能**：对表达式进行类型推断的统一入口

**实现细节**：
- 检查类型缓存避免重复推断
- 根据表达式类型分发到具体的推断方法
- 处理循环依赖问题

**代码示例**：
```cpp
std::shared_ptr<SemanticType> TypeChecker::InferExpressionType(Expression& expr) {
    // 检查缓存
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        // 如果缓存结果是占位符，说明正在处理中，需要继续推断而不是返回占位符
        if (nodeIt->second->tostring() == "inferring") {
            nodeTypeMap.erase(&expr);
        } else {
            return nodeIt->second;
        }
    }
    
    // 防止无限递归，先设置一个占位符
    auto placeholder = std::make_shared<SimpleType>("inferring");
    nodeTypeMap[&expr] = placeholder;
    
    std::shared_ptr<SemanticType> type;
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        type = InferLiteralExpressionType(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        type = InferBinaryExpressionType(*binary);
    } else if (auto call = dynamic_cast<CallExpression*>(&expr)) {
        type = InferCallExpressionType(*call);
    } else if (auto pathExpr = dynamic_cast<PathExpression*>(&expr)) {
        // 处理路径表达式（变量引用）
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string varName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            
            // 处理 self 关键字
            if (varName == "self") {
                // self 的类型由当前方法的 self 参数决定
                // ... self 处理逻辑
            } else {
                auto symbol = FindSymbol(varName);
                if (symbol && symbol->type) {
                    type = symbol->type;
                } else {
                    // 如果是常量，从常量求值器获取类型
                    if (constantEvaluator) {
                        auto constValue = constantEvaluator->GetConstantValue(varName);
                        if (constValue) {
                            if (dynamic_cast<IntConstant*>(constValue.get())) {
                                type = std::make_shared<SimpleType>("usize");
                            } else if (dynamic_cast<BoolConstant*>(constValue.get())) {
                                type = std::make_shared<SimpleType>("bool");
                            }
                            // ... 其他常量类型
                        }
                    }
                    
                    if (!type) {
                        ReportError("Undefined variable: " + varName);
                        type = nullptr;
                    }
                }
            }
        }
    }
    // ... 其他表达式类型
    
    // 更新缓存
    if (type) {
        nodeTypeMap[&expr] = type;
    } else {
        // 类型推断失败，移除占位符以允许重试
        nodeTypeMap.erase(&expr);
    }
    
    return type;
}
```

##### 字面量类型推断
```cpp
std::shared_ptr<SemanticType> InferLiteralExpressionType(LiteralExpression& expr);
```

**功能**：推断字面量表达式的类型

**实现细节**：
- 根据字面量 token 类型确定基础类型
- 处理整数类型的后缀（i32, u32, usize, isize）
- 根据数值大小推断整数类型（Int, SignedInt, UnsignedInt）

**代码示例**：
```cpp
std::shared_ptr<SemanticType> TypeChecker::InferLiteralExpressionType(LiteralExpression& expr) {
    switch (expr.tokentype) {
        case Token::kINTEGER_LITERAL: {
            const std::string& literal = expr.literal;
            
            // 检查是否有显式类型后缀
            if (literal.length() >= 5) {
                if (literal.substr(literal.length() - 5) == "usize") {
                    return std::make_shared<SimpleType>("usize");
                }
                if (literal.substr(literal.length() - 5) == "isize") {
                    return std::make_shared<SimpleType>("isize");
                }
            }
            
            if (literal.length() >= 3) {
                std::string suffix = literal.substr(literal.length() - 3);
                if (suffix == "u32") {
                    return std::make_shared<SimpleType>("u32");
                }
                if (suffix == "i32") {
                    return std::make_shared<SimpleType>("i32");
                }
            }
            
            // 没有显式后缀，需要根据数值大小进行推断
            std::string numStr = literal;
            
            // 移除可能的下划线
            numStr.erase(std::remove(numStr.begin(), numStr.end(), '_'), numStr.end());
            
            // 处理不同进制
            int64_t value = 0;
            if (numStr.length() >= 2 && numStr.substr(0, 2) == "0b") {
                // 二进制
                std::string binStr = numStr.substr(2);
                for (char c : binStr) {
                    if (c == '0' || c == '1') {
                        value = value * 2 + (c - '0');
                    }
                }
            } else if (numStr.length() >= 2 && numStr.substr(0, 2) == "0o") {
                // 八进制
                std::string octStr = numStr.substr(2);
                for (char c : octStr) {
                    if (c >= '0' && c <= '7') {
                        value = value * 8 + (c - '0');
                    }
                }
            } else if (numStr.length() >= 2 && numStr.substr(0, 2) == "0x") {
                // 十六进制
                std::string hexStr = numStr.substr(2);
                for (char c : hexStr) {
                    if (c >= '0' && c <= '9') {
                        value = value * 16 + (c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        value = value * 16 + (c - 'a' + 10);
                    } else if (c >= 'A' && c <= 'F') {
                        value = value * 16 + (c - 'A' + 10);
                    }
                }
            } else {
                // 十进制
                try {
                    value = std::stoll(numStr);
                } catch (const std::exception&) {
                    return std::make_shared<IntType>();
                }
            }
            
            // 根据数值范围推断类型
            if (value < 0 && value >= -2147483648LL) {
                return std::make_shared<SignedIntType>();
            }
            
            if (value > 2147483647LL) {
                return std::make_shared<UnsignedIntType>();
            }
            
            return std::make_shared<IntType>();
        }
        case Token::kCHAR_LITERAL:
            return std::make_shared<SimpleType>("char");
        case Token::kSTRING_LITERAL:
        case Token::kRAW_STRING_LITERAL:
            return std::make_shared<ReferenceTypeWrapper>(std::make_shared<SimpleType>("str"), false);
        case Token::ktrue:
        case Token::kfalse:
            return std::make_shared<SimpleType>("bool");
        default:
            return std::make_shared<SimpleType>("unknown");
    }
}
```

##### 二元表达式类型推断
```cpp
std::shared_ptr<SemanticType> InferBinaryExpressionType(BinaryExpression& expr);
```

**功能**：推断二元表达式的类型

**实现细节**：
- 推断左右操作数的类型
- 检查是否可以进行二元运算
- 根据运算符类型确定结果类型

**代码示例**：
```cpp
std::shared_ptr<SemanticType> TypeChecker::InferBinaryExpressionType(BinaryExpression& expr) {
    auto leftType = InferExpressionType(*expr.leftexpression);
    auto rightType = InferExpressionType(*expr.rightexpression);
    
    if (!leftType || !rightType) {
        return nullptr;
    }
    
    // 检查是否可以进行二元运算
    if (!CanPerformBinaryOperation(leftType, rightType, expr.binarytype)) {
        ReportError("Cannot perform binary operation '" + to_string(expr.binarytype) +
                   "' between types '" + leftType->tostring() + "' and '" + rightType->tostring() + "'");
        return nullptr;
    }
    
    // 返回运算结果类型
    return GetBinaryOperationResultType(leftType, rightType, expr.binarytype);
}
```

##### 数组表达式类型推断
```cpp
std::shared_ptr<SemanticType> InferArrayExpressionType(ArrayExpression& expr);
std::shared_ptr<SemanticType> InferArrayExpressionTypeWithExpected(ArrayExpression& expr, 
                                                             std::shared_ptr<SemanticType> expectedType);
```

**功能**：推断数组表达式的类型

**实现细节**：
- 处理重复元素语法 `[value; count]`
- 处理逗号分隔语法 `[value1, value2, ...]`
- 检查所有元素类型的兼容性
- 创建数组类型包装器

**代码示例**：
```cpp
std::shared_ptr<SemanticType> TypeChecker::InferArrayExpressionType(ArrayExpression& expr) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        if (nodeIt->second->tostring() == "array_expr_placeholder" || nodeIt->second->tostring() == "inferring") {
            nodeTypeMap.erase(&expr);
        } else {
            return nodeIt->second;
        }
    }
    
    // 先设置占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("array_expr_placeholder");
    nodeTypeMap[&expr] = placeholder;
    
    if (!expr.arrayelements) {
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    std::shared_ptr<SemanticType> elementType = nullptr;
    
    // 检查是否是重复元素语法 [value; count]
    if (expr.arrayelements->istwo) {
        // 对于重复元素语法，只使用第一个表达式作为元素类型
        if (expr.arrayelements->expressions.empty()) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        const auto& element = expr.arrayelements->expressions[0];
        if (!element) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        std::shared_ptr<SemanticType> elemType;
        if (auto literal = dynamic_cast<LiteralExpression*>(element.get())) {
            elemType = InferLiteralExpressionType(*literal);
        } else {
            elemType = InferExpressionType(*element);
        }
        
        if (!elemType) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        elementType = elemType;
    } else {
        // 对于逗号分隔语法，检查所有数组元素的类型
        for (const auto& element : expr.arrayelements->expressions) {
            if (element) {
                std::shared_ptr<SemanticType> elemType;
                if (auto literal = dynamic_cast<LiteralExpression*>(element.get())) {
                    elemType = InferLiteralExpressionType(*literal);
                } else {
                    elemType = InferExpressionType(*element);
                }
                
                if (!elemType) {
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
                if (!elementType) {
                    elementType = elemType;
                } else if (!AreTypesCompatible(elementType, elemType)) {
                    ReportError("Array elements must have same type");
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
            }
        }
    }
    
    if (!elementType) {
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    // 创建大小表达式
    std::shared_ptr<Expression> sizeExpr = nullptr;
    if (expr.arrayelements) {
        if (expr.arrayelements->istwo) {
            // 对于重复元素语法，使用第二个表达式作为大小
            if (expr.arrayelements->expressions.size() >= 2) {
                sizeExpr = expr.arrayelements->expressions[1];
            }
        } else {
            // 对于逗号分隔语法，创建一个字面量表达式表示大小
            auto literal = std::make_shared<LiteralExpression>(
                std::to_string(expr.arrayelements->expressions.size()),
                Token::kINTEGER_LITERAL
            );
            sizeExpr = literal;
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, sizeExpr, constantEvaluator.get());
    nodeTypeMap[&expr] = result;
    
    return result;
}
```

#### 4. 函数调用检查接口

##### 函数调用访问
```cpp
void visit(CallExpression& node) override;
std::shared_ptr<SemanticType> InferCallExpressionType(CallExpression& expr);
```

**功能**：处理函数调用，检查参数类型和数量

**实现细节**：
- 区分关联函数调用（`Type::function`）和普通函数调用
- 检查参数数量匹配
- 检查参数类型兼容性
- 推断返回类型

**代码示例**：
```cpp
void TypeChecker::visit(CallExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    // 访问参数
    if (node.callparams) {
        for (const auto& arg : node.callparams->expressions) {
            if (arg) {
                arg->accept(*this);
            }
        }
    }
    
    // 添加参数类型检查
    if (auto pathExpr = dynamic_cast<PathExpression*>(node.expression.get())) {
        if (pathExpr->simplepath) {
            if (!pathExpr->simplepath->simplepathsegements.empty()) {
                // 检查是否是多段路径（如 Stack::new）
                if (pathExpr->simplepath->simplepathsegements.size() >= 2) {
                    // 处理关联函数调用
                    std::string typeName = pathExpr->simplepath->simplepathsegements[0]->identifier;
                    std::string functionName = pathExpr->simplepath->simplepathsegements[1]->identifier;
                    
                    // 检查第一段是否是有效的类型
                    auto typeSymbol = FindSymbol(typeName);
                    if (typeSymbol && (typeSymbol->kind == SymbolKind::Struct ||
                                      typeSymbol->kind == SymbolKind::Enum ||
                                      typeSymbol->kind == SymbolKind::BuiltinType)) {
                        
                        // 查找该类型的关联函数
                        if (auto structSymbol = FindStruct(typeName)) {
                            for (const auto& method : structSymbol->methods) {
                                if (method->name == functionName) {
                                    // 找到关联函数，检查参数类型
                                    if (node.callparams) {
                                        const auto& params = method->parameters;
                                        const auto& args = node.callparams->expressions;
                                        
                                        // 检查参数数量是否匹配
                                        if (params.size() != args.size()) {
                                            ReportError("Function '" + typeName + "::" + functionName + "' expects " +
                                                       std::to_string(params.size()) + " arguments, but " +
                                                       std::to_string(args.size()) + " were provided");
                                        } else {
                                            // 按顺序检查每个参数的类型
                                            for (size_t i = 0; i < params.size(); ++i) {
                                                if (i < args.size() && args[i]) {
                                                    auto paramType = params[i]->type;
                                                    auto argType = InferExpressionType(*args[i]);
                                                    
                                                    if (paramType && argType) {
                                                        if (!AreTypesCompatible(paramType, argType)) {
                                                            ReportError("Type mismatch in function call '" + typeName + "::" + functionName +
                                                                       "': parameter " + std::to_string(i + 1) +
                                                                       " expects type '" + paramType->tostring() +
                                                                       "', but found type '" + argType->tostring() + "'");
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    // 单段路径的情况，普通函数调用
                    std::string functionName = pathExpr->simplepath->simplepathsegements[0]->identifier;
                    
                    auto functionSymbol = FindFunction(functionName);
                    if (functionSymbol && node.callparams) {
                        const auto& params = functionSymbol->parameters;
                        const auto& args = node.callparams->expressions;
                        
                        // 检查参数数量是否匹配
                        if (params.size() != args.size()) {
                            ReportError("Function '" + functionName + "' expects " +
                                       std::to_string(params.size()) + " arguments, but " +
                                       std::to_string(args.size()) + " were provided");
                        } else {
                            // 按顺序检查每个参数的类型
                            for (size_t i = 0; i < params.size(); ++i) {
                                if (i < args.size() && args[i]) {
                                    auto paramType = params[i]->type;
                                    auto argType = InferExpressionType(*args[i]);
                                    
                                    if (paramType && argType) {
                                        if (!AreTypesCompatible(paramType, argType)) {
                                            ReportError("Type mismatch in function call '" + functionName +
                                                       "': parameter " + std::to_string(i + 1) +
                                                       " expects type '" + paramType->tostring() +
                                                       "', but found type '" + argType->tostring() + "'");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 推断调用表达式的类型
    auto callType = InferCallExpressionType(node);
    if (callType) {
        nodeTypeMap[&node] = callType;
    }
    
    PopNode();
}
```

#### 5. 类型兼容性检查接口

##### 类型兼容性检查
```cpp
bool AreTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual);
bool AreTypesCompatibleNonRecursive(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual);
```

**功能**：检查两个类型是否兼容

**实现细节**：
- 处理引用类型的兼容性规则
- 实现隐式类型转换规则
- 处理数组类型的兼容性
- 支持多层引用的解引用

**代码示例**：
```cpp
bool TypeChecker::AreTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual) {
    if (!expected || !actual) return false;
    
    std::string expectedStr = expected->tostring();
    std::string actualStr = actual->tostring();
    
    // 简化类型兼容性检查
    if (expectedStr == actualStr) {
        return true;
    }
    // 推断类型与任何类型兼容
    if (actualStr == "_" || actualStr == "!") {
        return true;
    }
    
    // 获取引用层级信息
    auto expectedRefInfo = GetReferenceInfo(expected);
    auto actualRefInfo = GetReferenceInfo(actual);
    
    // 完全解引用两个类型，得到最底层的类型
    auto expectedDeref = FullyDereference(expected);
    auto actualDeref = FullyDereference(actual);
    
    // 检查底层类型兼容性（避免递归调用，直接比较字符串）
    std::string expectedDerefStr = expectedDeref ? expectedDeref->tostring() : "";
    std::string actualDerefStr = actualDeref ? actualDeref->tostring() : "";
    
    if (expectedDerefStr != actualDerefStr) {
        if (!AreTypesCompatibleNonRecursive(expectedDeref, actualDeref)) {
            return false;
        }
    }
    
    // 检查引用兼容性：&mut T 可以传递给 &T，也可以传递给 &&T
    if (expectedRefInfo.isReference && actualRefInfo.isReference) {
        if (expectedRefInfo.isMutable && !actualRefInfo.isMutable) {
            return false;
        }
        return true;
    }
    
    // 处理期望是引用，实际不是引用的情况
    if (expectedRefInfo.isReference && !actualRefInfo.isReference) {
        return false;
    }
    
    // 处理期望不是引用，实际是引用的情况
    if (!expectedRefInfo.isReference && actualRefInfo.isReference) {
        return AreTypesCompatibleNonRecursive(expected, actualDeref);
    }
    
    // 实现隐式转换规则
    if (expectedStr == "Int") {
        return (actualStr == "usize" || actualStr == "isize" || actualStr == "i32" || actualStr == "u32" || actualStr == "SignedInt" || actualStr == "UnsignedInt");
    }
    
    if (actualStr == "Int") {
        return (expectedStr == "usize" || expectedStr == "isize" || expectedStr == "i32" || expectedStr == "u32" || expectedStr == "SignedInt" || actualStr == "UnsignedInt");
    }
    
    // ... 其他隐式转换规则
    
    return false;
}
```

#### 6. 可变性检查接口

##### 赋值可变性检查
```cpp
void visit(AssignmentExpression& node) override;
void CheckAssignmentMutability(Expression& lhs);
```

**功能**：检查赋值操作的可变性

**实现细节**：
- 检查左值是否可变
- 处理不同类型的左值（变量、字段、数组索引）
- 检查引用类型的可变性规则

**代码示例**：
```cpp
void TypeChecker::visit(AssignmentExpression& node) {
    PushNode(node);
    
    // 检查左值的可变性
    if (node.leftexpression) {
        CheckAssignmentMutability(*node.leftexpression);
    }
    
    // 访问右表达式以触发其中的类型检查（如 CallExpression 的参数检查）
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    
    // 检查赋值类型兼容性
    if (node.leftexpression && node.rightexpression) {
        auto leftType = InferExpressionType(*node.leftexpression);
        auto rightType = InferExpressionType(*node.rightexpression);
        
        if (leftType && rightType) {
            if (!AreTypesCompatible(leftType, rightType)) {
                ReportError("Type mismatch in assignment: expected '" + leftType->tostring() +
                           "', found '" + rightType->tostring() + "'");
            }
        }
    }
    
    PopNode();
}

void TypeChecker::CheckAssignmentMutability(Expression& lhs) {
    if (auto pathExpr = dynamic_cast<PathExpression*>(&lhs)) {
        CheckVariableMutability(*pathExpr);
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(&lhs)) {
        CheckFieldMutability(*fieldExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(&lhs)) {
        CheckIndexMutability(*indexExpr);
    }
}

void TypeChecker::CheckVariableMutability(PathExpression& pathExpr) {
    if (!pathExpr.simplepath || pathExpr.simplepath->simplepathsegements.empty()) {
        return;
    }
    
    std::string varName = pathExpr.simplepath->simplepathsegements[0]->identifier;
    
    // 特殊处理 self：self 是特殊关键字，不应该从符号表中查找
    if (varName == "self") {
        return;
    }
    
    auto symbol = scopeTree->LookupSymbol(varName);
    if (!symbol) {
        ReportError("Undefined variable: " + varName);
        return;
    }
    
    // 特殊处理：如果变量是引用类型，检查引用本身是否可变
    bool isMutable = symbol->ismutable;
    if (symbol->type) {
        if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(symbol->type.get())) {
            isMutable = refType->GetIsMutable();
        }
    }
    
    if (!isMutable) {
        ReportMutabilityError(varName, "variable", &pathExpr);
    }
}
```

#### 7. 特殊功能接口

##### 内置方法支持
```cpp
bool IsBuiltinMethodCall(const std::string& receiverType, const std::string& methodName);
std::shared_ptr<SemanticType> GetBuiltinMethodReturnType(const std::string& receiverType, const std::string& methodName);
```

**功能**：支持内置方法的类型检查

**实现细节**：
- 支持 `to_string` 方法
- 支持 `as_str` 和 `as_mut_str` 方法
- 支持 `len` 方法

##### exit 函数检查
```cpp
void CheckExitFunctionUsage(CallExpression& expr);
void CheckMainFunctionExitRequirement(BlockExpression& blockExpr);
```

**功能**：检查 exit 函数的使用规则

**实现细节**：
- exit 只能在 main 函数中使用
- exit 不能在 impl 块内的方法中使用
- exit 必须是 main 函数的最后一条语句

## 与其他组件的交互

### 输入依赖

1. **ScopeTree** - 来自符号收集阶段的作用域树，用于符号查找
2. **ConstantEvaluator** - 常量求值器，用于获取常量值
3. **AST** - 抽象语法树，作为类型检查的输入

### 输出接口

```cpp
bool HasTypeErrors() const;
std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
```

### 为后续阶段提供的信息

1. **类型信息** - 每个AST节点的推断类型
2. **错误信息** - 类型检查阶段的详细错误报告
3. **类型映射** - 节点到类型的映射关系

### 使用约定

- 类型检查是语义分析的最后一个阶段
- 类型信息可以用于代码生成阶段
- 所有类型错误都会阻止编译继续

## 错误处理

### 错误类型

1. **类型不匹配** - 赋值、函数调用、返回值等类型不匹配
2. **未定义类型** - 引用了未定义的类型
3. **未定义变量** - 引用了未定义的变量
4. **未定义函数** - 调用了未定义的函数
5. **参数数量错误** - 函数调用参数数量不匹配
6. **可变性错误** - 试图修改不可变变量
7. **数组类型错误** - 数组元素类型不一致或大小不匹配

### 错误报告机制

```cpp
void ReportError(const std::string& message);
void ReportUndefinedType(const std::string& typeName, ASTNode* context);
void ReportMissingTraitImplementation(const std::string& traitName, const std::string& missingItem);
void ReportMutabilityError(const std::string& name, const std::string& errorType, ASTNode* context);
```

所有错误通过标准错误输出报告，并设置 `hasErrors` 标志。

## 设计特点

### 1. 分层类型检查

采用分层检查确保正确性：
- 首先进行基础类型推断
- 然后检查类型兼容性
- 最后验证可变性规则

### 2. 缓存机制

使用类型缓存避免重复推断：
- 节点类型映射避免重复计算
- 占位符机制防止循环依赖
- 类型缓存提高性能

### 3. 引用类型支持

完整支持引用类型系统：
- 多层引用的解引用
- 可变性和不可变性检查
- 引用类型的兼容性规则

### 4. 隐式类型转换

实现 Rx 语言的隐式转换规则：
- Int 类型可以转换为具体整数类型
- SignedInt 和 UnsignedInt 的转换规则
- 双向兼容性检查

### 5. 面向对象支持

支持结构体和方法的类型检查：
- self 参数的类型推断
- 方法的可变性检查
- 关联函数的类型验证

### 6. 可扩展架构

设计支持未来扩展：
- 新的类型可以轻松添加
- 新的运算符可以扩展支持
- 模块化设计便于维护

## 语法检查详细分析

### 检查问题分类

TypeChecker 组件对 Rx 语言程序进行全面的语法检查，主要检查以下类型的问题：

#### 1. 类型兼容性问题
- **基本类型不匹配**：赋值、函数调用、返回值等场景中的类型不一致
- **引用类型不兼容**：引用层级、可变性不匹配
- **隐式转换失败**：不符合隐式转换规则的类型转换

#### 2. 变量和函数使用问题
- **未定义符号**：使用未声明的变量、函数、类型
- **重复定义**：在同一作用域内重复定义符号
- **作用域错误**：在错误的作用域中使用符号

#### 3. 可变性违规问题
- **不可变赋值**：尝试修改不可变变量
- **引用可变性错误**：可变引用传递给不可变参数
- **字段修改错误**：修改不可变结构体的字段

#### 4. 函数调用问题
- **参数数量错误**：函数调用时参数数量不匹配
- **参数类型错误**：参数类型与函数签名不匹配
- **返回类型错误**：返回值类型与函数声明不匹配

#### 5. 数组操作问题
- **元素类型不一致**：数组中元素类型不统一
- **索引类型错误**：使用非整数类型作为数组索引
- **数组大小不匹配**：数组大小与期望不符

#### 6. 控制流问题
- **break/continue 位置错误**：在非循环结构中使用控制语句
- **return 类型不一致**：函数内 return 语句类型不统一
- **发散表达式误用**：在不期望发散的地方使用发散表达式

### 具体检查实现

#### 1. 类型兼容性检查

**检查内容**：
- 基本类型的精确匹配或隐式转换
- 引用类型的层级和可变性匹配
- 数组类型的元素类型和维度匹配

**实现方式**：
```cpp
bool AreTypesCompatible(expected, actual) {
    // 检查 1：完全相同类型
    if (expected->ToString() == actual->ToString()) return true;
    
    // 检查 2：隐式转换规则
    if (IsImplicitConversionAllowed(expected, actual)) return true;
    
    // 检查 3：引用类型兼容性
    if (IsReferenceType(expected) && IsReferenceType(actual)) {
        return CheckReferenceCompatibility(expected, actual);
    }
    
    // 检查 4：数组类型兼容性
    if (IsArrayType(expected) && IsArrayType(actual)) {
        return CheckArrayCompatibility(expected, actual);
    }
    
    return false;
}
```

**检查的错误类型**：
- `Type mismatch in assignment: expected 'i32', found 'bool'`
- `Cannot pass immutable reference to mutable parameter`
- `Array elements must have same type`

#### 2. 变量声明和使用检查

**检查内容**：
- 变量声明的类型注解正确性
- 初始化表达式类型与声明类型兼容性
- 变量使用时的作用域和可变性

**实现方式**：
```cpp
void CheckVariableDeclaration(LetStatement& node) {
    // 检查 1：类型注解有效性
    if (node.typeAnnotation) {
        auto declaredType = ResolveType(*node.typeAnnotation);
        if (!declaredType) {
            ReportError("Invalid type annotation: " + node.typeAnnotation->ToString());
        }
    }
    
    // 检查 2：初始化表达式类型兼容性
    if (node.initializer && declaredType) {
        auto initType = InferExpressionType(*node.initializer);
        if (!AreTypesCompatible(declaredType, initType)) {
            ReportError("Type mismatch: expected '" + declaredType->ToString() + 
                       "', found '" + initType->ToString() + "'");
        }
    }
    
    // 检查 3：符号重定义
    if (IsSymbolDefinedInCurrentScope(node.name)) {
        ReportError("Variable '" + node.name + "' already defined in this scope");
    }
}
```

**检查的错误类型**：
- `Invalid type annotation: unknown_type`
- `Type mismatch in let statement: expected 'i32', found 'string'`
- `Variable 'x' already defined in this scope`

#### 3. 函数调用检查

**检查内容**：
- 函数存在性验证
- 参数数量匹配检查
- 参数类型兼容性检查
- 返回类型推断和验证

**实现方式**：
```cpp
void CheckFunctionCall(CallExpression& node) {
    // 检查 1：函数存在性
    auto functionSymbol = ResolveFunction(node.functionName);
    if (!functionSymbol) {
        ReportError("Undefined function: " + node.functionName);
        return;
    }
    
    // 检查 2：参数数量
    if (node.arguments.size() != functionSymbol->parameters.size()) {
        ReportError("Function '" + node.functionName + "' expects " + 
                   std::to_string(functionSymbol->parameters.size()) + 
                   " arguments, got " + std::to_string(node.arguments.size()));
        return;
    }
    
    // 检查 3：参数类型
    for (size_t i = 0; i < node.arguments.size(); ++i) {
        auto paramType = functionSymbol->parameters[i]->type;
        auto argType = InferExpressionType(*node.arguments[i]);
        
        if (!AreTypesCompatible(paramType, argType)) {
            ReportError("Parameter " + std::to_string(i + 1) + " type mismatch: expected '" +
                       paramType->ToString() + "', found '" + argType->ToString() + "'");
        }
    }
}
```

**检查的错误类型**：
- `Undefined function: my_function`
- `Function 'foo' expects 2 arguments, got 3`
- `Parameter 1 type mismatch: expected 'i32', found 'bool'`

#### 4. 赋值操作检查

**检查内容**：
- 左值的有效性（必须是可修改的左值）
- 赋值类型的兼容性
- 可变性规则验证

**实现方式**：
```cpp
void CheckAssignment(AssignmentExpression& node) {
    // 检查 1：左值有效性
    if (!IsValidLValue(*node.left)) {
        ReportError("Invalid assignment target");
        return;
    }
    
    // 检查 2：可变性
    if (IsImmutableLValue(*node.left)) {
        ReportError("Cannot assign to immutable expression");
        return;
    }
    
    // 检查 3：类型兼容性
    auto leftType = InferExpressionType(*node.left);
    auto rightType = InferExpressionType(*node.right);
    
    if (!AreTypesCompatible(leftType, rightType)) {
        ReportError("Type mismatch in assignment: expected '" + leftType->ToString() +
                   "', found '" + rightType->ToString() + "'");
    }
}
```

**检查的错误类型**：
- `Invalid assignment target`
- `Cannot assign to immutable variable 'x'`
- `Type mismatch in assignment: expected 'i32', found 'string'`

#### 5. 数组操作检查

**检查内容**：
- 数组字面量的元素类型一致性
- 数组索引的整数类型要求
- 数组大小的常量表达式要求

**实现方式**：
```cpp
void CheckArrayOperations() {
    // 检查数组字面量
    void CheckArrayLiteral(ArrayLiteral& node) {
        if (node.elements.empty()) return;
        
        auto firstElementType = InferExpressionType(*node.elements[0]);
        
        // 检查所有元素类型一致
        for (size_t i = 1; i < node.elements.size(); ++i) {
            auto elementType = InferExpressionType(*node.elements[i]);
            if (!AreTypesCompatible(firstElementType, elementType)) {
                ReportError("Array element type mismatch at index " + std::to_string(i) +
                           ": expected '" + firstElementType->ToString() + 
                           "', found '" + elementType->ToString() + "'");
            }
        }
    }
    
    // 检查数组索引
    void CheckArrayIndex(IndexExpression& node) {
        auto indexType = InferExpressionType(*node.index);
        if (!IsIntegerType(indexType)) {
            ReportError("Array index must be integer type, found '" + indexType->ToString() + "'");
        }
    }
}
```

**检查的错误类型**：
- `Array element type mismatch at index 1: expected 'i32', found 'bool'`
- `Array index must be integer type, found 'string'`

#### 6. 控制流检查

**检查内容**：
- break/continue 语句的合法位置
- return 语句的类型一致性
- loop 表达式的 break 值类型

**实现方式**：
```cpp
void CheckControlFlow() {
    // 检查 break/continue 位置
    void CheckBreakContinue(BreakExpression& node) {
        if (!IsInsideLoop(&node)) {
            ReportError("Break can only be used inside loops");
        }
    }
    
    // 检查 return 类型一致性
    void CheckReturnTypes(Function& function) {
        std::vector<std::shared_ptr<Type>> returnTypes;
        CollectReturnTypes(function.body, returnTypes);
        
        if (!returnTypes.empty()) {
            auto firstType = returnTypes[0];
            for (size_t i = 1; i < returnTypes.size(); ++i) {
                if (!AreTypesCompatible(firstType, returnTypes[i])) {
                    ReportError("Inconsistent return types in function");
                }
            }
        }
    }
}
```

**检查的错误类型**：
- `Break can only be used inside loops`
- `Continue can only be used inside loops`
- `Inconsistent return types: some return 'i32', others return 'bool'`

#### 7. 面向对象特性检查

**检查内容**：
- self 参数的正确使用
- 方法调用的接收者类型验证
- impl 块的类型正确性

**实现方式**：
```cpp
void CheckObjectOrientedFeatures() {
    // 检查 self 使用
    void CheckSelfUsage(SelfExpression& node) {
        if (!IsInsideImplBlock(&node)) {
            ReportError("Self can only be used inside impl blocks");
        }
        
        auto implContext = GetCurrentImplContext();
        if (node.isMutable && !implContext->isMutable) {
            ReportError("Cannot use mutable Self in immutable impl");
        }
    }
    
    // 检查方法调用
    void CheckMethodCall(MethodCallExpression& node) {
        auto receiverType = InferExpressionType(*node.receiver);
        auto methodSymbol = FindMethod(receiverType, node.methodName);
        
        if (!methodSymbol) {
            ReportError("Type '" + receiverType->ToString() + 
                       "' has no method named '" + node.methodName + "'");
        }
    }
}
```

**检查的错误类型**：
- `Self can only be used inside impl blocks`
- `Cannot use mutable Self in immutable impl`
- `Type 'MyStruct' has no method named 'unknown_method'`

### 错误恢复策略

#### 1. 单错误继续处理
- 遇到错误时不立即停止
- 设置默认类型或符号以继续分析
- 尽可能收集更多错误信息

#### 2. 错误上下文保持
- 保持完整的错误位置信息
- 提供期望类型和实际类型的对比
- 给出具体的修复建议

#### 3. 类型推断恢复
- 类型推断失败时使用默认类型
- 循环依赖检测和处理
- 占位符机制防止无限递归

### 检查性能优化

#### 1. 缓存机制
- 类型推断结果缓存
- 符号查找结果缓存
- 兼容性检查结果缓存

#### 2. 延迟检查
- 按需进行类型检查
- 避免重复的兼容性验证
- 智能的检查顺序优化

#### 3. 早期退出
- 严重错误时提前退出
- 无关错误的跳过策略
- 分层检查的优化

通过这些详细的语法检查机制，TypeChecker 确保了 Rx 语言程序的类型安全性和语法正确性，为后续的代码生成阶段提供了可靠的语义信息基础。
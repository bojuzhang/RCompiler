# ExpressionGenerator 组件设计文档

## 概述

ExpressionGenerator 组件负责将 Rx 语言的表达式转换为 LLVM IR 文本。它是 IR 生成阶段的核心组件之一，处理各种类型的表达式生成，包括字面量、变量访问、函数调用、二元运算、一元运算等。

## 设计目标

1. **完整的表达式支持**：支持所有 Rx 语言表达式类型的 IR 生成
2. **类型安全**：确保生成的 IR 符合类型系统和语义约束
3. **寄存器管理**：高效管理临时寄存器的分配和生命周期
4. **语义集成**：与语义分析阶段的结果完全集成
5. **错误处理**：提供完善的错误检测和恢复机制

## 核心架构

### 表达式分类

ExpressionGenerator 将表达式分为以下几类进行处理：

1. **字面量表达式**（LiteralExpression）
   - 整数字面量：`42`, `0x1F`, `0b1010`
   - 字符字面量：`'a'`, `'\n'`
   - 布尔字面量：`true`, `false`
   - 字符串字面量：`"hello"`

2. **路径表达式**（PathExpression）
   - 变量访问：`x`, `y`
   - 常量访问：`CONST_VALUE`
   - 函数名：`function_name`

3. **数组表达式**（ArrayExpression）
   - 数组初始化：`[1, 2, 3]`
   - 重复初始化：`[0; 10]`

4. **索引表达式**（IndexExpression）
   - 数组索引：`arr[i]`
   - 指针索引：`ptr[i]`

5. **元组表达式**（TupleExpression）
   - 元组创建：`(1, "hello", true)`

6. **结构体表达式**（StructExpression）
   - 结构体创建：`Point { x: 1, y: 2 }`
   - 结构体更新：`Point { ..base, x: 1 }`

7. **函数调用表达式**（CallExpression）
   - 函数调用：`func(arg1, arg2)`
   - 关联函数调用：`Type::associated_func()`

8. **方法调用表达式**（MethodCallExpression）
   - 实例方法调用：`obj.method(arg)`
   - 静态方法调用：`Type::method(arg)`

9. **字段访问表达式**（FieldExpression）
   - 结构体字段访问：`obj.field`
   - 元组索引访问：`tuple.0`

10. **控制流表达式**
    - if 表达式：`if condition { value1 } else { value2 }`
    - loop 表达式：`loop { break value }`
    - while 表达式：`while condition { body }`

11. **一元表达式**（UnaryExpression）
    - 取负：`-x`
    - 逻辑非：`!expr`
    - 解引用：`*ptr`
    - 取引用：`&expr`, `&mut expr`

12. **二元表达式**（BinaryExpression）
    - 算术运算：`+`, `-`, `*`, `/`, `%`
    - 位运算：`&`, `|`, `^`, `<<`, `>>`
    - 比较运算：`==`, `!=`, `<`, `<=`, `>`, `>=`
    - 逻辑运算：`&&`, `||`

13. **赋值表达式**
    - 简单赋值：`x = value`
    - 复合赋值：`x += value`, `x *= value`

14. **类型转换表达式**（TypeCastExpression）
    - 显式类型转换：`expr as target_type`

### 组件接口设计

```cpp
// 前向声明
class StatementGenerator;

class ExpressionGenerator {
public:
    ExpressionGenerator(std::shared_ptr<IRBuilder> irBuilder,
                      std::shared_ptr<TypeMapper> typeMapper,
                      std::shared_ptr<ScopeTree> scopeTree,
                      const NodeTypeMap& nodeTypeMap);
    
    // 设置 StatementGenerator 引用（解决循环依赖）
    void setStatementGenerator(std::shared_ptr<StatementGenerator> stmtGen);
    
    // 主要生成接口
    std::string generateExpression(std::shared_ptr<Expression> expr);
    
    // 各类表达式的生成方法
    std::string generateLiteralExpression(std::shared_ptr<LiteralExpression> expr);
    std::string generatePathExpression(std::shared_ptr<PathExpression> expr);
    std::string generateArrayExpression(std::shared_ptr<ArrayExpression> expr);
    std::string generateIndexExpression(std::shared_ptr<IndexExpression> expr);
    std::string generateTupleExpression(std::shared_ptr<TupleExpression> expr);
    std::string generateStructExpression(std::shared_ptr<StructExpression> expr);
    std::string generateCallExpression(std::shared_ptr<CallExpression> expr);
    std::string generateMethodCallExpression(std::shared_ptr<MethodCallExpression> expr);
    std::string generateFieldExpression(std::shared_ptr<FieldExpression> expr);
    std::string generateUnaryExpression(std::shared_ptr<UnaryExpression> expr);
    std::string generateBinaryExpression(std::shared_ptr<BinaryExpression> expr);
    std::string generateAssignmentExpression(std::shared_ptr<AssignmentExpression> expr);
    std::string generateCompoundAssignmentExpression(std::shared_ptr<CompoundAssignmentExpression> expr);
    std::string generateTypeCastExpression(std::shared_ptr<TypeCastExpression> expr);
    
    // 控制流表达式
    std::string generateIfExpression(std::shared_ptr<IfExpression> expr);
    std::string generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> expr);
    std::string generateWhileExpression(std::shared_ptr<PredicateLoopExpression> expr);
    std::string generateBreakExpression(std::shared_ptr<BreakExpression> expr);
    std::string generateContinueExpression(std::shared_ptr<ContinueExpression> expr);
    std::string generateReturnExpression(std::shared_ptr<ReturnExpression> expr);
    
    // BlockExpression 处理（需要调用 StatementGenerator）
    std::string generateBlockExpression(std::shared_ptr<BlockExpression> expr);
    
    // 工具方法
    std::string getExpressionType(std::shared_ptr<Expression> expr);
    bool isLValue(std::shared_ptr<Expression> expr);
    std::string getLValuePointer(std::shared_ptr<Expression> expr);

private:
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    const NodeTypeMap& nodeTypeMap;
    std::shared_ptr<StatementGenerator> statementGenerator;  // 解决循环依赖
    
    // 辅助方法
    std::string generateIntegerLiteral(const std::string& value, const std::string& type);
    std::string generateStringLiteral(const std::string& value);
    std::string generateBooleanLiteral(bool value);
    std::string generateCharacterLiteral(const std::string& value);
    
    // 运算符映射
    std::string mapBinaryOperator(Token op);
    std::string mapUnaryOperator(Token op);
    std::string mapComparisonOperator(Token op);
    
    // 类型转换
    std::string generateImplicitConversion(const std::string& value,
                                         const std::string& fromType,
                                         const std::string& toType);
    std::string generateExplicitConversion(const std::string& value,
                                         const std::string& fromType,
                                         const std::string& toType);
};
```

## 实现策略

### 字面量表达式生成

#### 整数字面量

```cpp
std::string ExpressionGenerator::generateLiteralExpression(std::shared_ptr<LiteralExpression> expr) {
    std::string llvmType = getExpressionType(expr);
    std::string value = expr->literal;
    
    switch (expr->tokentype) {
        case Token::kINTEGER_LITERAL:
            return generateIntegerLiteral(value, llvmType);
        case Token::kSTRING_LITERAL:
            return generateStringLiteral(value);
        case Token::kTRUE:
        case Token::kFALSE:
            return generateBooleanLiteral(expr->tokentype == Token::kTRUE);
        case Token::kCHAR_LITERAL:
            return generateCharacterLiteral(value);
        default:
            return generateIntegerLiteral("0", "i32");
    }
}

std::string ExpressionGenerator::generateIntegerLiteral(const std::string& value, const std::string& type) {
    // 直接生成 LLVM 整数字面量
    if (type == "i32") {
        return value;
    } else if (type == "i1") {
        return (value == "0" || value == "false") ? "false" : "true";
    } else {
        // 其他整数类型，可能需要类型转换
        std::string reg = irBuilder->newRegister();
        irBuilder->emitInstruction("%" + reg + " = add " + type + " " + value + ", 0");
        return reg;
    }
}
```

#### 字符串字面量

```cpp
std::string ExpressionGenerator::generateStringLiteral(const std::string& value) {
    // 创建全局字符串常量
    std::string globalName = ".str" + std::to_string(regCounter++);
    std::string escapedValue = escapeString(value);
    
    // 声明全局字符串常量
    irBuilder->emitGlobalVariable(globalName, "[" + std::to_string(value.length() + 1) + " x i8]", 
                                 "c\"" + escapedValue + "\"", true);
    
    // 返回指向字符串的指针
    std::string reg = irBuilder->newRegister();
    irBuilder->emitInstruction("%" + reg + " = getelementptr [" + std::to_string(value.length() + 1) +
                              " x i8], [" + std::to_string(value.length() + 1) + " x i8]* " +
                              globalName + ", i32 0, i32 0");
    
    return reg;
}
```

### 路径表达式生成

```cpp
std::string ExpressionGenerator::generatePathExpression(std::shared_ptr<PathExpression> expr) {
    std::string pathName = getPathName(expr->simplepath);
    
    // 查找符号
    auto symbol = scopeTree->LookupSymbol(pathName);
    if (!symbol) {
        reportError("Undefined symbol: " + pathName);
        return "null";
    }
    
    switch (symbol->kind) {
        case SymbolKind::Variable:
        case SymbolKind::Parameter: {
            // 加载变量值
            std::string ptrReg = getVariablePointer(pathName);
            std::string valueReg = irBuilder->newRegister();
            std::string llvmType = typeMapper->mapSemanticTypeToLLVM(symbol->type);
            irBuilder->emitLoad(valueReg, ptrReg, llvmType);
            return valueReg;
        }
        
        case SymbolKind::Constant: {
            // 常量值
            if (auto constValue = getConstantValue(symbol)) {
                return generateConstantValue(constValue);
            }
            break;
        }
        
        case SymbolKind::Function: {
            // 函数指针
            return "@" + pathName;
        }
        
        default:
            reportError("Invalid symbol kind for path expression: " + pathName);
            return "null";
    }
    
    return "null";
}
```

### 数组表达式生成

```cpp
std::string ExpressionGenerator::generateArrayExpression(std::shared_ptr<ArrayExpression> expr) {
    auto elements = expr->arrayelements->expressions;
    
    // 检查是否为多维数组（元素本身也是数组）
    if (isMultiDimensionalArray(expr)) {
        // 处理多维数组：[[1, 2, 3], [4, 5, 6], [7, 8, 9]]
        return generateMultiDimensionalArray(expr);
    }
    
    // 处理复杂嵌套数组：[func_call(), obj.field, arr[i] + 1, x * 2]
    std::string elementType = inferCommonElementType(elements);
    std::string arrayType = "[" + std::to_string(elements.size()) + " x " + elementType + "]";
    
    // 分配数组空间
    std::string arrayReg = irBuilder->newRegister();
    irBuilder->emitAlloca(arrayReg, arrayType);
    
    // 初始化每个元素（元素可能是任意复杂表达式）
    for (size_t i = 0; i < elements.size(); ++i) {
        // 递归生成元素表达式（可能是函数调用、字段访问、索引访问等）
        std::string elementReg = generateExpression(elements[i]);
        
        // 类型转换（如果需要）
        std::string elementTypeLLVM = typeMapper->mapRxTypeToLLVM(elementType);
        std::string actualElementType = getExpressionType(elements[i]);
        if (actualElementType != elementTypeLLVM) {
            elementReg = generateImplicitConversion(elementReg, actualElementType, elementTypeLLVM);
        }
        
        // 获取元素指针
        std::string elemPtrReg = irBuilder->newRegister();
        irBuilder->emitGetElementPtr(elemPtrReg, arrayReg, "0", std::to_string(i), elementTypeLLVM + "*");
        
        // 存储元素值
        irBuilder->emitStore(elementReg, elemPtrReg, elementTypeLLVM);
    }
    
    // 如果是表达式值，需要加载整个数组
    if (isExpressionValue(expr)) {
        std::string loadedReg = irBuilder->newRegister();
        irBuilder->emitLoad(loadedReg, arrayReg, arrayType);
        return loadedReg;
    }
    
    return arrayReg;
}

// 多维数组处理的辅助方法
std::string ExpressionGenerator::generateMultiDimensionalArray(std::shared_ptr<ArrayExpression> expr) {
    auto elements = expr->arrayelements->expressions;
    
    // 获取内层数组类型
    auto innerArrayExpr = std::dynamic_pointer_cast<ArrayExpression>(elements[0]);
    std::string innerElementType = getArrayElementType(innerArrayExpr);
    std::string innerArrayType = "[" + std::to_string(innerArrayExpr->arrayelements->expressions.size()) +
                                 " x " + innerElementType + "]";
    
    // 外层数组类型
    std::string outerArrayType = "[" + std::to_string(elements.size()) + " x " + innerArrayType + "]";
    
    // 分配外层数组空间
    std::string arrayReg = irBuilder->newRegister();
    irBuilder->emitAlloca(arrayReg, outerArrayType);
    
    // 初始化每个内层数组
    for (size_t i = 0; i < elements.size(); ++i) {
        // 递归生成内层数组
        std::string innerArrayReg = generateArrayExpression(elements[i]);
        
        // 获取外层数组元素指针
        std::string elemPtrReg = irBuilder->newRegister();
        irBuilder->emitGetElementPtr(elemPtrReg, arrayReg, "0", std::to_string(i), innerArrayType + "*");
        
        // 存储内层数组
        irBuilder->emitStore(innerArrayReg, elemPtrReg, innerArrayType);
    }
    
    return arrayReg;
}
```

### 索引表达式生成

```cpp
std::string ExpressionGenerator::generateIndexExpression(std::shared_ptr<IndexExpression> expr) {
    // 生成数组/指针表达式
    std::string arrayReg = generateExpression(expr->expressionout);
    
    // 生成索引表达式
    std::string indexReg = generateExpression(expr->expressionin);
    
    // 获取元素类型
    std::string arrayType = getExpressionType(expr->expressionout);
    std::string elementType = getElementType(arrayType);
    
    // 生成 GEP 指令
    std::string elemPtrReg = irBuilder->newRegister();
    if (isPointerType(arrayType)) {
        // 指针索引：ptr[i]
        irBuilder->emitGetElementPtr(elemPtrReg, arrayReg, indexReg, "", elementType + "*");
    } else {
        // 数组索引：arr[i]
        irBuilder->emitGetElementPtr(elemPtrReg, arrayReg, "0", indexReg, elementType + "*");
    }
    
    // 加载元素值
    std::string valueReg = irBuilder->newRegister();
    irBuilder->emitLoad(valueReg, elemPtrReg, elementType);
    
    return valueReg;
}
```

### 二元表达式生成

```cpp
std::string ExpressionGenerator::generateBinaryExpression(std::shared_ptr<BinaryExpression> expr) {
    // 生成左右操作数
    std::string leftReg = generateExpression(expr->leftexpression);
    std::string rightReg = generateExpression(expr->rightexpression);
    
    // 获取操作数类型
    std::string leftType = getExpressionType(expr->leftexpression);
    std::string rightType = getExpressionType(expr->rightexpression);
    std::string resultType = getExpressionType(expr);
    
    // 类型转换（如果需要）
    if (leftType != resultType) {
        leftReg = generateImplicitConversion(leftReg, leftType, resultType);
    }
    if (rightType != resultType) {
        rightReg = generateImplicitConversion(rightReg, rightType, resultType);
    }
    
    // 通过 IRBuilder 分配结果寄存器
    std::string resultReg = irBuilder->newRegister();
    std::string op = mapBinaryOperator(expr->binarytype);
    
    if (isArithmeticOperator(expr->binarytype)) {
        // 算术运算
        irBuilder->emitArithmetic(resultReg, op, leftReg, rightReg, resultType);
    } else if (isComparisonOperator(expr->binarytype)) {
        // 比较运算
        std::string cmpReg = irBuilder->newRegister();
        std::string cmpOp = mapComparisonOperator(expr->binarytype);
        irBuilder->emitIcmp(cmpReg, cmpOp, leftReg, rightReg, resultType);
        
        // 扩展到目标类型
        resultReg = generateImplicitConversion(cmpReg, "i1", resultType);
    } else if (isLogicalOperator(expr->binarytype)) {
        // 逻辑运算
        if (expr->binarytype == Token::kAndAnd) {
            resultReg = generateLogicalAnd(leftReg, rightReg);
        } else if (expr->binarytype == Token::kOrOr) {
            resultReg = generateLogicalOr(leftReg, rightReg);
        }
    }
    
    return resultReg;
}
```

### 函数调用表达式生成

```cpp
std::string ExpressionGenerator::generateCallExpression(std::shared_ptr<CallExpression> expr) {
    // 获取函数名
    std::string functionName = getFunctionName(expr->expression);
    
    // 查找函数符号
    auto functionSymbol = scopeTree->LookupSymbol(functionName);
    if (!functionSymbol || functionSymbol->kind != SymbolKind::Function) {
        reportError("Undefined function: " + functionName);
        return "null";
    }
    
    // 生成参数表达式
    std::vector<std::string> argRegs;
    auto params = getFunctionParameters(functionSymbol);
    auto callParams = expr->callparams->expressions;
    
    for (size_t i = 0; i < callParams.size(); ++i) {
        std::string argReg = generateExpression(callParams[i]);
        std::string argType = getExpressionType(callParams[i]);
        std::string paramType = typeMapper->mapSemanticTypeToLLVM(params[i]->type);
        
        // 类型转换（如果需要）
        if (argType != paramType) {
            argReg = generateImplicitConversion(argReg, argType, paramType);
        }
        
        argRegs.push_back(argReg);
    }
    
    // 通过 IRBuilder 分配结果寄存器并生成函数调用
    std::string resultReg = irBuilder->newRegister();
    std::string returnType = typeMapper->mapSemanticTypeToLLVM(getFunctionReturnType(functionSymbol));
    
    irBuilder->emitCall(resultReg, functionName, argRegs, returnType);
    
    return resultReg;
}
```

### 方法调用表达式生成

```cpp
std::string ExpressionGenerator::generateMethodCallExpression(std::shared_ptr<MethodCallExpression> expr) {
    // 生成接收者表达式（可能是复杂的嵌套表达式，包括字段访问）
    std::string receiverReg = generateExpression(expr->receiver);
    std::string receiverType = getExpressionType(expr->receiver);
    
    // 查找方法
    std::string methodName = expr->method_name;
    auto methodSymbol = findMethod(receiverType, methodName);
    
    if (!methodSymbol) {
        reportError("Method not found: " + receiverType + "::" + methodName);
        return "null";
    }
    
    // 生成参数表达式
    std::vector<std::string> argRegs;
    
    // 第一个参数是 self
    if (isReferenceType(methodSymbol->parameters[0]->type)) {
        // self 是引用类型
        argRegs.push_back(receiverReg);
    } else {
        // self 是值类型，需要解引用
        std::string derefReg = irBuilder->newRegister();
        irBuilder->emitLoad(derefReg, receiverReg, receiverType);
        argRegs.push_back(derefReg);
    }
    
    // 生成其他参数
    auto callParams = expr->callparams->expressions;
    for (size_t i = 0; i < callParams.size(); ++i) {
        std::string argReg = generateExpression(callParams[i]);
        argRegs.push_back(argReg);
    }
    
    // 生成方法调用
    std::string resultReg = irBuilder->newRegister();
    std::string mangledName = mangleMethodName(receiverType, methodName);
    std::string returnType = typeMapper->mapSemanticTypeToLLVM(methodSymbol->returnType);
    
    irBuilder->emitCall(resultReg, mangledName, argRegs, returnType);
    
    return resultReg;
}
```

### 字段访问表达式生成

```cpp
std::string ExpressionGenerator::generateFieldExpression(std::shared_ptr<FieldExpression> expr) {
    // 生成接收者表达式（支持嵌套字段访问：obj.field1.field2）
    std::string receiverReg = generateExpression(expr->expression);
    std::string receiverType = getExpressionType(expr->expression);
    
    // 获取字段信息
    std::string fieldName = expr->identifier;
    auto fieldInfo = getFieldInfo(receiverType, fieldName);
    
    if (!fieldInfo) {
        reportError("Field not found: " + receiverType + "." + fieldName);
        return "null";
    }
    
    // 生成 GEP 指令获取字段地址
    std::string fieldPtrReg = irBuilder->newRegister();
    std::vector<std::pair<std::string, std::string>> indices = {
        {"0", "i32"},  // 结构体第一个元素
        {std::to_string(fieldInfo->index), "i32"}  // 字段索引
    };
    
    irBuilder->emitGetElementPtr(fieldPtrReg, receiverReg, indices);
    
    // 加载字段值
    std::string fieldReg = irBuilder->newRegister();
    std::string fieldType = typeMapper->mapSemanticTypeToLLVM(fieldInfo->type);
    irBuilder->emitLoad(fieldReg, fieldPtrReg, fieldType);
    
    return fieldReg;
}
```

### 字段访问内套方法调用的处理

在 `generateMethodCallExpression` 中，当接收者是 `FieldExpression` 时，自动处理 `obj.field.method()` 的情况：

```cpp
// 在 generateMethodCallExpression 中的特殊处理
if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(expr->receiver)) {
    // 接收者是字段访问：obj.field.method()
    // 字段访问已经在上面通过 generateExpression(fieldExpr) 完成
    // receiverReg 现在包含字段的值，receiverType 是字段类型
    // 继续正常的方法调用流程...
}
```

### 控制流表达式生成

#### if 表达式

```cpp
std::string ExpressionGenerator::generateIfExpression(std::shared_ptr<IfExpression> expr) {
    // 生成条件表达式
    std::string condReg = generateExpression(expr->conditions->expression);
    
    // 创建基本块
    std::string thenBB = irBuilder->newBasicBlock("if.then");
    std::string elseBB = irBuilder->newBasicBlock("if.else");
    std::string endBB = irBuilder->newBasicBlock("if.end");
    
    // 生成条件跳转
    irBuilder->emitCondBr(condReg, thenBB, elseBB);
    
    // 生成 then 分支
    irBuilder->emitLabel(thenBB);
    std::string thenReg = generateExpression(expr->ifblockexpression);
    irBuilder->emitBr(endBB);
    
    // 生成 else 分支
    irBuilder->emitLabel(elseBB);
    std::string elseReg;
    if (expr->elseexpression) {
        elseReg = generateExpression(expr->elseexpression);
    } else {
        // 没有 else 分支，使用单元类型
        elseReg = generateUnitValue();
    }
    irBuilder->emitBr(endBB);
    
    // 生成 phi 节点
    irBuilder->emitLabel(endBB);
    std::string resultReg = irBuilder->newRegister();
    std::string resultType = getExpressionType(expr);
    std::string llvmType = typeMapper->mapRxTypeToLLVM(resultType);
    irBuilder->emitPhi(resultReg, llvmType, {thenReg, elseReg}, {thenBB, elseBB});
    
    return resultReg;
}
```

#### loop 表达式

```cpp
std::string ExpressionGenerator::generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> expr) {
    // 创建基本块
    std::string loopBB = irBuilder->newBasicBlock("loop.start");
    std::string endBB = irBuilder->newBasicBlock("loop.end");
    
    // 设置循环上下文
    pushLoopContext(endBB);
    
    // 跳转到循环开始
    irBuilder->emitBr(loopBB);
    
    // 生成循环体
    irBuilder->emitLabel(loopBB);
    std::string bodyReg = generateExpression(expr->blockexpression);
    
    // 如果循环体没有 break，继续循环
    irBuilder->emitBr(loopBB);
    
    // 循环结束
    irBuilder->emitLabel(endBB);
    
    // 清理循环上下文
    popLoopContext();
    
    // 返回 break 值或单元类型
    return getLoopBreakValue(bodyReg);
}
```

### 类型转换生成

```cpp
std::string ExpressionGenerator::generateTypeCastExpression(std::shared_ptr<TypeCastExpression> expr) {
    std::string valueReg = generateExpression(expr->expression);
    std::string fromType = getExpressionType(expr->expression);
    std::string toType = typeMapper->mapRxTypeToLLVM(expr->typenobounds);
    
    return generateExplicitConversion(valueReg, fromType, toType);
}

std::string ExpressionGenerator::generateExplicitConversion(const std::string& value,
                                                         const std::string& fromType,
                                                         const std::string& toType) {
    if (fromType == toType) {
        return value;
    }
    
    std::string resultReg = irBuilder->newRegister();
    
    if (isIntegerType(fromType) && isIntegerType(toType)) {
        // 整数到整数转换
        if (getIntegerBitWidth(fromType) < getIntegerBitWidth(toType)) {
            irBuilder->emitSext(resultReg, value, fromType, toType);
        } else {
            irBuilder->emitTrunc(resultReg, value, fromType, toType);
        }
    } else if (isPointerType(fromType) && isIntegerType(toType)) {
        // 指针到整数转换
        irBuilder->emitPtrtoint(resultReg, value, fromType, toType);
    } else if (isIntegerType(fromType) && isPointerType(toType)) {
        // 整数到指针转换
        irBuilder->emitInttoptr(resultReg, value, fromType, toType);
    } else {
        reportError("Unsupported type conversion from " + fromType + " to " + toType);
        return value;
    }
    
    return resultReg;
}
```

## 寄存器管理与 IRBuilder 集成

### 寄存器分配策略

ExpressionGenerator **不直接管理寄存器**，而是通过 IRBuilder 的寄存器管理系统进行分配：

```cpp
class ExpressionGenerator {
public:
    // ExpressionGenerator 通过 IRBuilder 获取寄存器
    std::string generateExpression(std::shared_ptr<Expression> expr) {
        // 使用 IRBuilder 的寄存器分配
        std::string resultReg = irBuilder->newRegister();
        // ... 生成表达式逻辑 ...
        return resultReg;
    }

private:
    std::shared_ptr<IRBuilder> irBuilder;
};
```

### 与 IRBuilder 的寄存器接口集成

```cpp
// ExpressionGenerator 中的寄存器使用示例
std::string ExpressionGenerator::generateBinaryExpression(std::shared_ptr<BinaryExpression> expr) {
    // 生成左右操作数（递归调用，每个都会通过 IRBuilder 分配寄存器）
    std::string leftReg = generateExpression(expr->leftexpression);
    std::string rightReg = generateExpression(expr->rightexpression);
    
    // 通过 IRBuilder 分配结果寄存器
    std::string resultReg = irBuilder->newRegister();
    std::string resultType = getExpressionType(expr);
    std::string llvmType = typeMapper->mapRxTypeToLLVM(resultType);
    
    // 通过 IRBuilder 生成算术指令
    irBuilder->emitAdd(resultReg, leftReg, rightReg, llvmType);
    
    return resultReg;
}
```

### 变量寄存器的获取与管理

```cpp
std::string ExpressionGenerator::generatePathExpression(std::shared_ptr<PathExpression> expr) {
    std::string pathName = getPathName(expr->simplepath);
    
    // 通过 IRBuilder 获取变量寄存器（基于作用域）
    std::string varPtr = irBuilder->getVariableRegister(pathName);
    if (varPtr.empty()) {
        reportError("Undefined variable: " + pathName);
        return "null";
    }
    
    // 获取变量类型
    auto symbol = scopeTree->LookupSymbol(pathName);
    std::string llvmType = typeMapper->mapSemanticTypeToLLVM(symbol->type);
    
    // 通过 IRBuilder 分配值寄存器并加载变量值
    std::string valueReg = irBuilder->newRegister();
    irBuilder->emitLoad(valueReg, varPtr, llvmType);
    
    return valueReg;
}
```

### 作用域感知的寄存器管理

```cpp
class ExpressionGenerator {
public:
    // 在进入新作用域时同步 IRBuilder
    void enterScope(std::shared_ptr<Scope> scope) {
        scopeTree->GoToNode(scope);
        irBuilder->syncWithScopeTree();
    }
    
    // 在退出作用域时清理寄存器
    void exitScope() {
        auto currentScope = irBuilder->getCurrentScope();
        irBuilder->cleanupScopeRegisters(currentScope);
        scopeTree->ExitScope();
    }

private:
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<ScopeTree> scopeTree;
};
```

### 类型区分的寄存器使用

```cpp
std::string ExpressionGenerator::generateVariableAccess(const std::string& varName) {
    // 通过 IRBuilder 获取不同类型的寄存器
    std::string ptrReg = irBuilder->getVariableRegister(varName + "_ptr");
    std::string valReg = irBuilder->newRegister(varName, "val");
    
    // 加载值到值寄存器
    auto symbol = scopeTree->LookupSymbol(varName);
    std::string llvmType = typeMapper->mapSemanticTypeToLLVM(symbol->type);
    irBuilder->emitLoad(valReg, ptrReg, llvmType);
    
    return valReg;
}
```

## 错误处理

### 表达式错误检测

```cpp
class ExpressionGenerator {
private:
    bool hasErrors;
    std::vector<std::string> errorMessages;
    
public:
    void reportError(const std::string& message) {
        hasErrors = true;
        errorMessages.push_back(message);
        std::cerr << "Expression Error: " << message << std::endl;
    }
    
    void reportTypeError(const std::string& expected, const std::string& actual, 
                        const std::string& context) {
        reportError("Type error in " + context + 
                   ": expected '" + expected + "', found '" + actual + "'");
    }
    
    bool hasGenerationErrors() const {
        return hasErrors;
    }
    
    const std::vector<std::string>& getErrorMessages() const {
        return errorMessages;
    }
};
```

### 错误恢复策略

```cpp
std::string ExpressionGenerator::generateExpressionWithErrorRecovery(std::shared_ptr<Expression> expr) {
    try {
        return generateExpression(expr);
    } catch (const GenerationError& error) {
        reportError(error.what());
        
        // 生成默认值
        std::string exprType = getExpressionType(expr);
        return generateDefaultValue(exprType);
    }
}

std::string ExpressionGenerator::generateDefaultValue(const std::string& type) {
    if (type == "i32" || type == "i1") {
        return "0";
    } else if (type == "bool") {
        return "false";
    } else if (isPointerType(type)) {
        return "null";
    } else {
        // 对于复合类型，返回未初始化的值
        std::string reg = irBuilder->newRegister();
        irBuilder->emitAlloca(reg, type);
        return reg;
    }
}
```

## 性能优化

### 常量折叠

```cpp
std::string ExpressionGenerator::tryConstantFolding(std::shared_ptr<BinaryExpression> expr) {
    if (!isConstantExpression(expr->leftexpression) || 
        !isConstantExpression(expr->rightexpression)) {
        return "";
    }
    
    std::string leftValue = getConstantValue(expr->leftexpression);
    std::string rightValue = getConstantValue(expr->rightexpression);
    
    // 执行常量运算
    std::string result = evaluateConstantBinaryOp(leftValue, rightValue, expr->binarytype);
    
    if (!result.empty()) {
        return result;
    }
    
    return "";
}
```

### 公共子表达式消除

```cpp
class ExpressionGenerator {
private:
    std::unordered_map<std::string, std::string> expressionCache;
    
public:
    std::string generateExpressionWithCaching(std::shared_ptr<Expression> expr) {
        std::string exprKey = getExpressionKey(expr);
        
        auto it = expressionCache.find(exprKey);
        if (it != expressionCache.end()) {
            return it->second;
        }
        
        std::string result = generateExpression(expr);
        expressionCache[exprKey] = result;
        
        return result;
    }
};
```

## 测试策略

### 单元测试

```cpp
// 字面量表达式测试
TEST(ExpressionGeneratorTest, LiteralExpression) {
    setupGenerator();
    
    // 整数字面量
    auto intLiteral = std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL);
    std::string result = generator->generateExpression(intLiteral);
    EXPECT_EQ(result, "42");
    
    // 布尔字面量
    auto boolLiteral = std::make_shared<LiteralExpression>("true", Token::kTRUE);
    result = generator->generateExpression(boolLiteral);
    EXPECT_EQ(result, "true");
}

// 二元表达式测试
TEST(ExpressionGeneratorTest, BinaryExpression) {
    setupGenerator();
    
    auto left = std::make_shared<LiteralExpression>("10", Token::kINTEGER_LITERAL);
    auto right = std::make_shared<LiteralExpression>("20", Token::kINTEGER_LITERAL);
    auto binaryExpr = std::make_shared<BinaryExpression>(left, right, Token::kPlus);
    
    std::string result = generator->generateExpression(binaryExpr);
    EXPECT_TRUE(result.find("_") == 0); // 应该是寄存器名
}

// 函数调用测试
TEST(ExpressionGeneratorTest, FunctionCall) {
    setupGenerator();
    
    auto funcPath = std::make_shared<PathExpression>(
        std::make_shared<SimplePath>("test_func", false, false));
    auto callParams = std::make_shared<CallParams>(
        std::vector<std::shared_ptr<Expression>>{
            std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL)
        });
    auto callExpr = std::make_shared<CallExpression>(funcPath, callParams);
    
    std::string result = generator->generateExpression(callExpr);
    EXPECT_TRUE(result.find("_") == 0); // 应该是寄存器名
}
```

### 集成测试

```cpp
// 复杂表达式测试
TEST(ExpressionGeneratorIntegrationTest, ComplexExpression) {
    setupGenerator();
    
    // 测试表达式：func(arr[i] + 10, obj.method(x * 2))
    auto arrIndex = std::make_shared<IndexExpression>(
        std::make_shared<PathExpression>(std::make_shared<SimplePath>("arr", false, false)),
        std::make_shared<LiteralExpression>("i", Token::kINTEGER_LITERAL));
    
    auto addExpr = std::make_shared<BinaryExpression>(arrIndex, 
        std::make_shared<LiteralExpression>("10", Token::kINTEGER_LITERAL), Token::kPlus);
    
    // ... 构建完整表达式
    
    std::string result = generator->generateExpression(complexExpr);
    EXPECT_FALSE(result.empty());
}
```

## 使用示例

### 基本使用

```cpp
// 创建 ExpressionGenerator
auto irBuilder = std::make_shared<IRBuilder>(output);
auto typeMapper = std::make_shared<TypeMapper>(scopeTree);
auto scopeTree = semanticAnalyzer->getScopeTree();
auto nodeTypeMap = typeChecker->getNodeTypeMap();

ExpressionGenerator exprGen(irBuilder, typeMapper, scopeTree, nodeTypeMap);

// 生成简单表达式
auto intLiteral = std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL);
std::string result = exprGen.generateExpression(intLiteral);
// 输出：42

// 生成二元表达式
auto left = std::make_shared<LiteralExpression>("10", Token::kINTEGER_LITERAL);
auto right = std::make_shared<LiteralExpression>("20", Token::kINTEGER_LITERAL);
auto binaryExpr = std::make_shared<BinaryExpression>(left, right, Token::kPlus);
result = exprGen.generateExpression(binaryExpr);
// 输出：%_1 = add i32 10, 20
```

### 高级使用

```cpp
// 生成函数调用
auto funcPath = std::make_shared<PathExpression>(
    std::make_shared<SimplePath>("calculate", false, false));
auto callParams = std::make_shared<CallParams>(
    std::vector<std::shared_ptr<Expression>>{
        std::make_shared<LiteralExpression>("100", Token::kINTEGER_LITERAL),
        std::make_shared<LiteralExpression>("200", Token::kINTEGER_LITERAL)
    });
auto callExpr = std::make_shared<CallExpression>(funcPath, callParams);

std::string result = exprGen.generateExpression(callExpr);
// 输出：%_2 = call i32 @calculate(i32 100, i32 200)

// 生成方法调用
auto receiver = std::make_shared<PathExpression>(
    std::make_shared<SimplePath>("obj", false, false));
auto methodParams = std::make_shared<CallParams>(
    std::vector<std::shared_ptr<Expression>>{
        std::make_shared<LiteralExpression>("5", Token::kINTEGER_LITERAL)
    });
auto methodCall = std::make_shared<MethodCallExpression>(receiver, "update", methodParams);

result = exprGen.generateExpression(methodCall);
// 输出：%_3 = call i32 @Struct_update(%Struct* %obj, i32 5)
```

## 总结

ExpressionGenerator 组件是 IR 生成阶段的核心组件，提供了完整的表达式生成功能：

1. **完整的表达式支持**：支持所有 Rx 语言表达式类型的 IR 生成
2. **类型安全**：与 TypeMapper 紧密集成，确保类型正确性
3. **寄存器管理**：高效的寄存器分配和生命周期管理
4. **错误处理**：完善的错误检测和恢复机制
5. **性能优化**：常量折叠、公共子表达式消除等优化
6. **语义集成**：与语义分析结果完全集成
7. **易于扩展**：清晰的接口设计，便于添加新的表达式类型

通过 ExpressionGenerator，IR 生成器可以将复杂的 Rx 语言表达式正确地转换为高效的 LLVM IR 代码。
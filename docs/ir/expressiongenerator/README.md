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

10. **块表达式**（BlockExpression）
    - 语句块：`{ stmt1; stmt2; expr }`
    - 无尾表达式块：`{ stmt1; stmt2; }`（值为 unit）

11. **控制流表达式**
    - if 表达式：`if condition { value1 } else { value2 }`
    - loop 表达式：`loop { break value }`
    - while 表达式：`while condition { body }`

12. **一元表达式**（UnaryExpression）
    - 取负：`-x`
    - 逻辑非：`!expr`
    - 解引用：`*ptr`
    - 取引用：`&expr`, `&mut expr`

13. **二元表达式**（BinaryExpression）
    - 算术运算：`+`, `-`, `*`, `/`, `%`
    - 位运算：`&`, `|`, `^`, `<<`, `>>`
    - 比较运算：`==`, `!=`, `<`, `<=`, `>`, `>=`
    - 逻辑运算：`&&`, `||`

14. **赋值表达式**
    - 简单赋值：`x = value`
    - 复合赋值：`x += value`, `x *= value`
    - 成员访问赋值：`obj.field = value`
    - 数组索引赋值：`arr[i] = value`
    - 嵌套赋值：`obj.arr[i].field = value`

15. **类型转换表达式**（TypeCastExpression）
    - 显式类型转换：`expr as target_type`

### 组件接口设计

```cpp
// 前向声明
class StatementGenerator;
class FunctionCodegen;

class ExpressionGenerator {
public:
    ExpressionGenerator(std::shared_ptr<IRBuilder> irBuilder,
                      std::shared_ptr<TypeMapper> typeMapper,
                      std::shared_ptr<ScopeTree> scopeTree,
                      const NodeTypeMap& nodeTypeMap);
    
    // 设置依赖组件
    void setStatementGenerator(std::shared_ptr<StatementGenerator> stmtGen);
    void setFunctionCodegen(std::shared_ptr<FunctionCodegen> funcGen);
    
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
    
    // 块表达式
    std::string generateBlockExpression(std::shared_ptr<BlockExpression> expr);
    
    // 控制流表达式
    std::string generateIfExpression(std::shared_ptr<IfExpression> expr);
    std::string generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> expr);
    std::string generateWhileExpression(std::shared_ptr<PredicateLoopExpression> expr);
    std::string generateBreakExpression(std::shared_ptr<BreakExpression> expr);
    std::string generateContinueExpression(std::shared_ptr<ContinueExpression> expr);
    std::string generateReturnExpression(std::shared_ptr<ReturnExpression> expr);
    
    // 辅助方法：生成 BlockExpression 中的语句
    void generateBlockStatements(std::shared_ptr<BlockExpression> block);
    
    // 辅助方法：生成单元类型值
    std::string generateUnitValue();
    
    // 工具方法
    std::string getExpressionType(std::shared_ptr<Expression> expr);
    bool isLValue(std::shared_ptr<Expression> expr);
    std::string getLValuePointer(std::shared_ptr<Expression> expr);
    
    // BlockExpression 辅助方法
    void generateBlockStatements(std::shared_ptr<BlockExpression> block);
    std::string generateUnitValue();

private:
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    const NodeTypeMap& nodeTypeMap;
    std::shared_ptr<StatementGenerator> statementGenerator;  // 解决循环依赖
    std::shared_ptr<FunctionCodegen> functionCodegen;       // 用于处理函数调用
    
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
    
    // 函数调用辅助方法
    std::string generateRegularFunctionCall(std::shared_ptr<CallExpression> expr);
};
```

## 实现策略

### 赋值表达式生成

赋值表达式是Rx语言中的核心操作，负责将值存储到可变的位置。ExpressionGenerator必须智能地处理从简单标量赋值到复杂聚合类型赋值的各种场景，特别是要解决大型聚合类型的性能优化问题。

#### 赋值表达式分类与处理策略

赋值表达式根据左值（LValue）的复杂程度分为以下几类：

1. **简单变量赋值**：`x = value`
2. **成员访问赋值**：`obj.field = value`
3. **数组索引赋值**：`arr[i] = value`
4. **嵌套访问赋值**：`obj.arr[i].field = value`

每种类型的赋值都需要不同的地址计算和存储策略。

#### 核心设计：智能赋值策略选择

为了解决大型聚合类型的性能问题，ExpressionGenerator实现了智能的赋值策略选择机制：

```cpp
enum class AssignmentStrategy {
    DIRECT_LOAD_STORE,    // 直接的 load/store 序列
    MEMORY_COPY,          // 使用 memcpy
};
```

**策略选择的核心原则**：
- **小型聚合类型**（≤ 16字节）：使用直接的 load/store 序列
- **大型聚合类型**（> 16字节）：使用直接的 memcpy 调用

#### 类型内省与大小分析系统

为了实现智能策略选择，ExpressionGenerator需要精确的类型内省能力：

```cpp
class TypeInfoAnalyzer {
public:
    // 获取类型的精确字节大小
    static uint64_t getTypeByteSize(std::shared_ptr<SemanticType> type);
    
    // 检查类型是否为聚合类型
    static bool isAggregateType(std::shared_ptr<SemanticType> type);
    
    // 检查类型是否为平凡可复制类型
    static bool isTriviallyCopyable(std::shared_ptr<SemanticType> type);
    
    // 获取类型的对齐要求
    static uint64_t getTypeAlignment(std::shared_ptr<SemanticType> type);
    
    // 分析类型的内存布局
    static TypeLayoutInfo analyzeTypeLayout(std::shared_ptr<SemanticType> type);
};

struct TypeLayoutInfo {
    uint64_t byteSize;           // 类型的字节大小
    uint64_t alignment;          // 对齐要求
    bool isAggregate;            // 是否为聚合类型
    bool isTriviallyCopyable;    // 是否为平凡可复制
    bool hasPadding;             // 是否包含填充字节
    std::vector<FieldInfo> fields; // 字段信息（仅结构体）
};
```

**类型内省的实现细节**：

```cpp
uint64_t TypeInfoAnalyzer::getTypeByteSize(std::shared_ptr<SemanticType> type) {
    if (!type) return 0;
    
    // 基础类型的大小查询
    if (auto simpleType = dynamic_cast<SimpleType*>(type.get())) {
        return getBasicTypeSize(simpleType->typeName);
    }
    
    // 数组类型：元素大小 × 元素数量
    if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        uint64_t elementSize = getTypeByteSize(arrayType->GetElementType());
        uint64_t elementCount = getArrayElementCount(arrayType);
        return elementSize * elementCount;
    }
    
    // 结构体类型：递归计算所有字段大小
    if (auto structType = dynamic_cast<StructType*>(type.get())) {
        return calculateStructSize(structType);
    }
    
    // 引用类型：指针大小（32位系统为4字节）
    if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(type.get())) {
        return 4; // 指针大小
    }
    
    return 0;
}

bool TypeInfoAnalyzer::isAggregateType(std::shared_ptr<SemanticType> type) {
    if (!type) return false;
    
    // 数组和结构体都是聚合类型
    return (dynamic_cast<ArrayTypeWrapper*>(type.get()) != nullptr) ||
           (dynamic_cast<StructType*>(type.get()) != nullptr);
}
```

#### GenerateAssignment函数的完整实现

```cpp
std::string ExpressionGenerator::generateAssignmentExpression(
    std::shared_ptr<AssignmentExpression> expr) {
    
    // 1. 获取左右表达式的类型信息
    auto leftType = getExpressionType(expr->leftexpression);
    auto rightType = getExpressionType(expr->rightexpression);
    
    // 2. 分析左值的地址计算策略
    LValueInfo lvalueInfo = analyzeLValue(expr->leftexpression);
    
    // 3. 选择赋值策略
    AssignmentStrategy strategy = selectAssignmentStrategy(leftType, lvalueInfo);
    
    // 4. 根据策略生成相应的IR代码
    switch (strategy) {
        case AssignmentStrategy::DIRECT_LOAD_STORE:
            return generateDirectStoreAssignment(expr, lvalueInfo);
        case AssignmentStrategy::MEMORY_COPY:
            return generateMemcpyAssignment(expr, lvalueInfo);
    }
    
    return generateDefaultValue(leftType);
}
```

#### 左值分析与地址计算

左值分析是赋值表达式的关键步骤，需要确定如何计算目标地址：

```cpp
struct LValueInfo {
    enum class LValueKind {
        VARIABLE,          // 简单变量
        FIELD_ACCESS,      // 字段访问
        ARRAY_INDEX,       // 数组索引
        NESTED_ACCESS      // 嵌套访问
    } kind;
    
    std::string basePointer;      // 基础指针寄存器
    std::vector<std::string> indices; // 索引序列
    std::string finalAddress;     // 最终地址寄存器
    std::shared_ptr<SemanticType> targetType; // 目标类型
};

LValueInfo ExpressionGenerator::analyzeLValue(std::shared_ptr<Expression> lvalue) {
    LValueInfo info;
    
    if (auto pathExpr = dynamic_cast<PathExpression*>(lvalue.get())) {
        // 简单变量访问
        info.kind = LValueInfo::LValueKind::VARIABLE;
        info.basePointer = getVariablePointer(pathExpr->simplepath);
        info.finalAddress = info.basePointer;
        info.targetType = getExpressionType(lvalue);
        
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(lvalue.get())) {
        // 字段访问：obj.field
        info.kind = LValueInfo::LValueKind::FIELD_ACCESS;
        info = analyzeFieldAccess(fieldExpr);
        
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(lvalue.get())) {
        // 数组索引：arr[i]
        info.kind = LValueInfo::LValueKind::ARRAY_INDEX;
        info = analyzeArrayIndex(indexExpr);
        
    } else {
        reportError("Invalid lvalue in assignment");
    }
    
    return info;
}
```

#### 字段访问的地址计算

```cpp
LValueInfo ExpressionGenerator::analyzeFieldAccess(
    std::shared_ptr<FieldExpression> fieldExpr) {
    
    LValueInfo info;
    info.kind = LValueInfo::LValueKind::FIELD_ACCESS;
    
    // 递归分析接收者表达式
    auto receiverInfo = analyzeLValue(fieldExpr->expression);
    info.basePointer = receiverInfo.finalAddress;
    
    // 获取字段信息
    std::string fieldName = fieldExpr->identifier;
    auto fieldType = getFieldType(receiverInfo.targetType, fieldName);
    
    // 生成 getelementptr 指令计算字段地址
    std::string fieldPtrReg = irBuilder->newRegister();
    uint32_t fieldIndex = getFieldIndex(receiverInfo.targetType, fieldName);
    
    std::vector<std::pair<std::string, std::string>> indices = {
        {"0", "i32"},  // 结构体第一个元素
        {std::to_string(fieldIndex), "i32"}  // 字段索引
    };
    
    irBuilder->emitGetElementPtr(fieldPtrReg, receiverInfo.finalAddress, indices);
    
    info.finalAddress = fieldPtrReg;
    info.targetType = fieldType;
    info.indices = receiverInfo.indices;
    info.indices.push_back(std::to_string(fieldIndex));
    
    return info;
}
```

#### 数组索引的地址计算

```cpp
LValueInfo ExpressionGenerator::analyzeArrayIndex(
    std::shared_ptr<IndexExpression> indexExpr) {
    
    LValueInfo info;
    info.kind = LValueInfo::LValueKind::ARRAY_INDEX;
    
    // 分析数组基础表达式
    auto arrayInfo = analyzeLValue(indexExpr->expressionout);
    info.basePointer = arrayInfo.finalAddress;
    
    // 生成索引表达式
    std::string indexReg = generateExpression(indexExpr->expressionin);
    
    // 获取元素类型
    auto elementType = getElementType(arrayInfo.targetType);
    
    // 生成 getelementptr 指令计算元素地址
    std::string elemPtrReg = irBuilder->newRegister();
    
    if (isPointerType(arrayInfo.targetType)) {
        // 指针索引：ptr[i]
        irBuilder->emitGetElementPtr(elemPtrReg, arrayInfo.finalAddress,
                                   indexReg, "", elementType + "*");
    } else {
        // 数组索引：arr[i]
        irBuilder->emitGetElementPtr(elemPtrReg, arrayInfo.finalAddress,
                                   "0", indexReg, elementType + "*");
    }
    
    info.finalAddress = elemPtrReg;
    info.targetType = elementType;
    info.indices = arrayInfo.indices;
    info.indices.push_back(indexReg);
    
    return info;
}
```

#### 直接存储策略的实现

```cpp
std::string ExpressionGenerator::generateDirectStoreAssignment(
    std::shared_ptr<AssignmentExpression> expr,
    const LValueInfo& lvalueInfo) {
    
    // 生成右值表达式
    std::string valueReg = generateExpression(expr->rightexpression);
    std::string valueType = getExpressionType(expr->rightexpression);
    
    // 类型转换（如果需要）
    std::string targetLLVMType = typeMapper->mapSemanticTypeToLLVM(lvalueInfo.targetType);
    if (valueType != targetLLVMType) {
        valueReg = generateImplicitConversion(valueReg, valueType, targetLLVMType);
    }
    
    // 直接存储到目标地址
    irBuilder->emitStore(valueReg, lvalueInfo.finalAddress, targetLLVMType);
    
    // 返回赋值表达式的值（右值）
    return valueReg;
}
```

#### memcpy优化策略的实现

```cpp
std::string ExpressionGenerator::generateMemcpyAssignment(
    std::shared_ptr<AssignmentExpression> expr,
    const LValueInfo& lvalueInfo) {
    
    // 为右值创建临时存储空间
    std::string tempReg = irBuilder->newRegister();
    std::string llvmType = typeMapper->mapSemanticTypeToLLVM(lvalueInfo.targetType);
    irBuilder->emitAlloca(tempReg, llvmType);
    
    // 生成右值表达式并存储到临时空间
    std::string valueReg = generateExpression(expr->rightexpression);
    irBuilder->emitStore(valueReg, tempReg, llvmType);
    
    // 计算类型大小
    uint64_t typeSize = TypeInfoAnalyzer::getTypeByteSize(lvalueInfo.targetType);
    std::string sizeReg = irBuilder->newRegister();
    irBuilder->emitInstruction("%" + sizeReg + " = add i32 " +
                             std::to_string(typeSize) + ", 0");
    
    // 生成 memcpy 调用
    std::string memcpyResult = irBuilder->newRegister();
    std::vector<std::string> memcpyArgs = {
        lvalueInfo.finalAddress,  // 目标地址
        tempReg,                  // 源地址
        sizeReg                   // 大小
    };
    irBuilder->emitCall(memcpyResult, "llvm.memcpy.p0i8.p0i8.i32",
                       memcpyArgs, "void");
    
    // 返回赋值表达式的值
    return valueReg;
}
```


#### 复合赋值表达式的处理

```cpp
std::string ExpressionGenerator::generateCompoundAssignmentExpression(
    std::shared_ptr<CompoundAssignmentExpression> expr) {
    
    // 生成左值地址
    LValueInfo lvalueInfo = analyzeLValue(expr->leftexpression);
    
    // 加载当前值
    std::string currentValReg = irBuilder->newRegister();
    std::string llvmType = typeMapper->mapSemanticTypeToLLVM(lvalueInfo.targetType);
    irBuilder->emitLoad(currentValReg, lvalueInfo.finalAddress, llvmType);
    
    // 生成右值表达式
    std::string rightValReg = generateExpression(expr->rightexpression);
    
    // 执行二元运算
    std::string resultReg = irBuilder->newRegister();
    std::string op = mapBinaryOperator(expr->type);
    
    switch (expr->type) {
        case Token::kPlusEq:
            irBuilder->emitAdd(resultReg, currentValReg, rightValReg, llvmType);
            break;
        case Token::kMinusEq:
            irBuilder->emitSub(resultReg, currentValReg, rightValReg, llvmType);
            break;
        case Token::kMulEq:
            irBuilder->emitMul(resultReg, currentValReg, rightValReg, llvmType);
            break;
        case Token::kDivEq:
            irBuilder->emitDiv(resultReg, currentValReg, rightValReg, llvmType);
            break;
        // ... 其他复合运算符
    }
    
    // 存储结果
    irBuilder->emitStore(resultReg, lvalueInfo.finalAddress, llvmType);
    
    return resultReg;
}
```

#### 嵌套赋值表达式的优化

对于复杂的嵌套赋值表达式，如 `obj.arr[i].field = value`，ExpressionGenerator采用递归分析方法：

```cpp
LValueInfo ExpressionGenerator::analyzeNestedAccess(
    std::shared_ptr<Expression> expr) {
    
    if (auto fieldExpr = dynamic_cast<FieldExpression*>(expr.get())) {
        // 字段访问：递归分析接收者
        auto receiverInfo = analyzeNestedAccess(fieldExpr->expression);
        
        // 计算字段地址
        std::string fieldPtrReg = irBuilder->newRegister();
        uint32_t fieldIndex = getFieldIndex(receiverInfo.targetType,
                                         fieldExpr->identifier);
        
        std::vector<std::pair<std::string, std::string>> indices = {
            {"0", "i32"},
            {std::to_string(fieldIndex), "i32"}
        };
        
        irBuilder->emitGetElementPtr(fieldPtrReg, receiverInfo.finalAddress, indices);
        
        LValueInfo info;
        info.kind = LValueInfo::LValueKind::NESTED_ACCESS;
        info.basePointer = receiverInfo.basePointer;
        info.finalAddress = fieldPtrReg;
        info.targetType = getFieldType(receiverInfo.targetType,
                                    fieldExpr->identifier);
        info.indices = receiverInfo.indices;
        info.indices.push_back(std::to_string(fieldIndex));
        
        return info;
        
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(expr.get())) {
        // 数组索引：递归分析数组基础
        auto arrayInfo = analyzeNestedAccess(indexExpr->expressionout);
        
        // 计算索引地址
        std::string indexReg = generateExpression(indexExpr->expressionin);
        std::string elemPtrReg = irBuilder->newRegister();
        auto elementType = getElementType(arrayInfo.targetType);
        
        irBuilder->emitGetElementPtr(elemPtrReg, arrayInfo.finalAddress,
                                   "0", indexReg, elementType + "*");
        
        LValueInfo info;
        info.kind = LValueInfo::LValueKind::NESTED_ACCESS;
        info.basePointer = arrayInfo.basePointer;
        info.finalAddress = elemPtrReg;
        info.targetType = elementType;
        info.indices = arrayInfo.indices;
        info.indices.push_back(indexReg);
        
        return info;
        
    } else {
        // 基础情况：简单变量
        return analyzeLValue(expr);
    }
}
```



通过这些实现，ExpressionGenerator的赋值表达式生成功能解决了大型聚合类型的处理问题，确保生成的IR代码正确高效。

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
    
    // 检查是否为内置函数
    if (functionCodegen && functionCodegen->isBuiltinFunction(functionName)) {
        return functionCodegen->generateBuiltinCall(expr);
    }
    
    // 处理普通函数调用
    return generateRegularFunctionCall(expr);
}

std::string ExpressionGenerator::generateRegularFunctionCall(std::shared_ptr<CallExpression> expr) {
    // 获取函数名
    std::string functionName = getFunctionName(expr->expression);
    
    // 查找函数符号
    auto functionSymbol = scopeTree->LookupSymbol(functionName);
    if (!functionSymbol || functionSymbol->kind != SymbolKind::Function) {
        return "null";
    }
    
    // 生成参数表达式
    std::vector<std::string> argRegs;
    auto params = getFunctionParameters(functionSymbol);
    auto callParams = expr->callparams->expressions;
    
    for (size_t i = 0; i < callParams.size(); ++i) {
        // 调用 FunctionCodegen 处理参数
        std::string argReg = functionCodegen->generateArgument(callParams[i], params[i]->type);
        argRegs.push_back(argReg);
    }
    
    // 通过 IRBuilder 分配结果寄存器并生成函数调用
    std::string resultReg = irBuilder->newRegister();
    std::string returnType = typeMapper->mapTypeToLLVM(getFunctionReturnType(functionSymbol));
    
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

loop 表达式是一种无限循环，只能通过 break 语句退出。它的值来自于 break 语句提供的值，如果没有 break 语句或 break 不带值，则为 unit 类型。

```cpp
std::string ExpressionGenerator::generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> expr) {
    // 创建基本块
    std::string loopBB = irBuilder->newBasicBlock("loop.start");
    std::string endBB = irBuilder->newBasicBlock("loop.end");
    
    // 创建循环上下文，用于处理 break/continue
    LoopContext context;
    context.loopStartBB = loopBB;
    context.loopEndBB = endBB;
    context.hasBreak = false;
    context.breakValueReg = "";  // 初始化为空，表示没有 break 值
    context.breakValueType = "unit";  // 默认为 unit 类型
    
    // 将循环上下文压入栈
    pushLoopContext(context);
    
    // 跳转到循环开始
    irBuilder->emitBr(loopBB);
    
    // 生成循环体
    irBuilder->emitLabel(loopBB);
    
    // 注意：循环体的值不直接使用，因为 loop 表达式的值来自 break
    generateExpression(expr->blockexpression);
    
    // 如果循环体执行到这里，说明没有遇到 break，继续循环
    irBuilder->emitBr(loopBB);
    
    // 循环结束标签
    irBuilder->emitLabel(endBB);
    
    // 获取循环上下文中的 break 信息
    context = getCurrentLoopContext();
    
    // 清理循环上下文
    popLoopContext();
    
    // 处理 loop 表达式的返回值
    if (context.hasBreak) {
        // 有 break 语句，返回 break 提供的值
        if (context.breakValueReg.empty()) {
            // break 不带值，返回 unit 类型
            return generateUnitValue();
        } else {
            // break 带值，返回 break 的值
            return context.breakValueReg;
        }
    } else {
        // 没有 break 语句，返回 unit 类型
        return generateUnitValue();
    }
}
```

##### break 表达式的值处理

break 表达式可以带值或不带值，它的值成为整个 loop 表达式的返回值：

```cpp
std::string ExpressionGenerator::generateBreakExpression(std::shared_ptr<BreakExpression> expr) {
    // 获取当前循环上下文
    LoopContext& context = getCurrentLoopContext();
    context.hasBreak = true;
    
    // 处理 break 表达式的值（如果有）
    if (expr->expression) {
        // break 带值：break value
        std::string breakValueReg = generateExpression(expr->expression);
        context.breakValueReg = breakValueReg;
        context.breakValueType = getExpressionType(expr->expression);
        
        // 跳转到循环结束
        irBuilder->emitBr(context.loopEndBB);
    } else {
        // break 不带值
        context.breakValueReg = "";  // 空字符串表示没有值
        context.breakValueType = "unit";
        
        // 跳转到循环结束
        irBuilder->emitBr(context.loopEndBB);
    }
    
    // break 表达式本身不产生值，因为它会中断控制流
    // 这里返回一个哑值，实际不会使用
    return generateUnitValue();
}
```

##### continue 表达式的跳转处理

continue 表达式用于跳过当前迭代，直接进入下一次迭代：

```cpp
std::string ExpressionGenerator::generateContinueExpression(std::shared_ptr<ContinueExpression> expr) {
    // 获取当前循环上下文
    LoopContext& context = getCurrentLoopContext();
    
    // 跳转到循环开始
    irBuilder->emitBr(context.loopStartBB);
    
    // continue 表达式本身不产生值，因为它会中断控制流
    // 这里返回一个哑值，实际不会使用
    return generateUnitValue();
}
```

##### 循环上下文管理

为了正确处理 break/continue 的跳转，需要维护循环上下文栈：

```cpp
class ExpressionGenerator {
private:
    struct LoopContext {
        std::string loopStartBB;      // 循环开始基本块
        std::string loopEndBB;        // 循环结束基本块
        bool hasBreak;                // 是否有 break 语句
        std::string breakValueReg;    // break 语句的值寄存器
        std::string breakValueType;   // break 值的类型
    };
    
    std::vector<LoopContext> loopContextStack;  // 循环上下文栈
    
    // 压入新的循环上下文
    void pushLoopContext(const LoopContext& context) {
        loopContextStack.push_back(context);
    }
    
    // 弹出当前循环上下文
    void popLoopContext() {
        if (!loopContextStack.empty()) {
            loopContextStack.pop_back();
        }
    }
    
    // 获取当前循环上下文
    LoopContext& getCurrentLoopContext() {
        if (loopContextStack.empty()) {
            reportError("No loop context available for break/continue");
            // 返回一个默认上下文，避免崩溃
            static LoopContext defaultContext;
            return defaultContext;
        }
        return loopContextStack.back();
    }
};
```

#### while 表达式

while 表达式是条件循环，当条件为真时重复执行循环体，条件为假时退出。与 loop 不同，while 表达式的值**恒定为 unit 类型**，无论是否有 break 语句。

```cpp
std::string ExpressionGenerator::generateWhileExpression(std::shared_ptr<PredicateLoopExpression> expr) {
    // 创建基本块
    std::string condBB = irBuilder->newBasicBlock("while.cond");
    std::string bodyBB = irBuilder->newBasicBlock("while.body");
    std::string endBB = irBuilder->newBasicBlock("while.end");
    
    // 创建循环上下文，用于处理 break/continue
    LoopContext context;
    context.loopStartBB = condBB;  // while 循环的"开始"是条件检查
    context.loopEndBB = endBB;
    context.hasBreak = false;
    context.breakValueReg = "";    // while 不需要 break 值，因为返回值始终是 unit
    context.breakValueType = "unit";
    
    // 将循环上下文压入栈
    pushLoopContext(context);
    
    // 跳转到条件检查
    irBuilder->emitBr(condBB);
    
    // 生成条件检查
    irBuilder->emitLabel(condBB);
    std::string condReg = generateExpression(expr->conditions->expression);
    
    // 条件为假时跳转到循环结束
    irBuilder->emitCondBr(condReg, bodyBB, endBB);
    
    // 生成循环体
    irBuilder->emitLabel(bodyBB);
    generateExpression(expr->blockexpression);
    
    // 循环体执行完毕，跳回条件检查
    irBuilder->emitBr(condBB);
    
    // 循环结束标签
    irBuilder->emitLabel(endBB);
    
    // 清理循环上下文
    popLoopContext();
    
    // while 表达式的值恒定为 unit 类型
    return generateUnitValue();
}
```

##### while 循环中的 break 表达式

在 while 循环中，break 语句可以提前退出循环，但不会影响 while 表达式的返回值（始终为 unit）：

```cpp
std::string ExpressionGenerator::generateBreakExpression(std::shared_ptr<BreakExpression> expr) {
    // 获取当前循环上下文
    LoopContext& context = getCurrentLoopContext();
    context.hasBreak = true;
    
    // 处理 break 表达式的值（如果有）
    if (expr->expression) {
        // break 带值：break value
        // 在 while 循环中，break 的值被忽略，但仍需生成表达式以处理副作用
        std::string breakValueReg = generateExpression(expr->expression);
        context.breakValueReg = breakValueReg;
        context.breakValueType = getExpressionType(expr->expression);
    } else {
        // break 不带值
        context.breakValueReg = "";
        context.breakValueType = "unit";
    }
    
    // 跳转到循环结束
    irBuilder->emitBr(context.loopEndBB);
    
    // break 表达式本身不产生值，因为它会中断控制流
    return generateUnitValue();
}
```

##### while 循环中的 continue 表达式

在 while 循环中，continue 语句会跳转到条件检查，而不是循环体开始：

```cpp
std::string ExpressionGenerator::generateContinueExpression(std::shared_ptr<ContinueExpression> expr) {
    // 获取当前循环上下文
    LoopContext& context = getCurrentLoopContext();
    
    // 对于 while 循环，continue 跳转到条件检查
    irBuilder->emitBr(context.loopStartBB);
    
    // continue 表达式本身不产生值，因为它会中断控制流
    return generateUnitValue();
}
```

##### loop 与 while 的关键区别

1. **返回值**：
   - `loop` 表达式的值来自 `break` 语句，没有 `break` 时为 `unit`
   - `while` 表达式的值**恒定为 `unit`**，不受 `break` 影响

2. **跳转目标**：
   - `loop` 中 `continue` 跳转到循环体开始
   - `while` 中 `continue` 跳转到条件检查

3. **break 值的处理**：
   - `loop` 中 `break` 的值成为整个表达式的返回值
   - `while` 中 `break` 的值被忽略（但仍需生成以处理副作用）

##### 嵌套循环的处理

对于嵌套循环，每个循环都有自己的上下文，break/continue 只影响最内层的循环：

```cpp
// 示例：loop { while condition { break; } break 42; }

// 外层 loop 上下文
LoopContext outerContext;
outerContext.loopStartBB = outerLoopBB;
outerContext.loopEndBB = outerEndBB;
pushLoopContext(outerContext);

// 内层 while 上下文
LoopContext innerContext;
innerContext.loopStartBB = whileCondBB;
innerContext.loopEndBB = whileEndBB;
pushLoopContext(innerContext);

// 内层 break 只退出 while 循环
// 外层 break 退出 loop 循环并返回值 42
```

##### 循环中的值传递与优化

在循环中，特别是 loop 表达式中，break 值的传递需要特别处理：

```cpp
// 处理不同类型的 break 值
std::string ExpressionGenerator::handleLoopBreakValue(const LoopContext& context) {
    if (!context.hasBreak) {
        return generateUnitValue();
    }
    
    if (context.breakValueReg.empty()) {
        // break 不带值
        return generateUnitValue();
    }
    
    // break 带值，可能需要类型转换
    std::string expectedType = getExpectedLoopReturnType();
    std::string actualType = context.breakValueType;
    
    if (expectedType != actualType) {
        // 需要类型转换
        return generateImplicitConversion(context.breakValueReg, actualType, expectedType);
    }
    
    return context.breakValueReg;
}
```

##### 循环展开与优化

对于某些简单的循环，可以在编译时进行循环展开：

```cpp
std::string ExpressionGenerator::tryLoopUnrolling(std::shared_ptr<InfiniteLoopExpression> expr) {
    // 检查是否是简单的计数循环
    if (isSimpleCountingLoop(expr)) {
        int iterations = getLoopIterationCount(expr);
        if (iterations > 0 && iterations <= MAX_UNROLL_COUNT) {
            return generateUnrolledLoop(expr, iterations);
        }
    }
    
    // 无法展开，使用标准循环生成
    return generateLoopExpression(expr);
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

### BlockExpression 生成

BlockExpression 是一种特殊的表达式，它包含一系列语句和一个可选的尾表达式。BlockExpression 的值是尾表达式的值，如果没有尾表达式，则为单元类型值。

```cpp
std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> expr) {
    if (!statementGenerator) {
        reportError("StatementGenerator not set for BlockExpression");
        return generateUnitValue();
    }
    
    // 创建新的作用域
    scopeTree->EnterScope();
    irBuilder->enterScope();
    
    // 生成块内的所有语句
    generateBlockStatements(expr);
    
    // 处理尾表达式（如果有）
    std::string resultValue;
    if (expr->expressionwithoutblock) {
        resultValue = generateExpression(expr->expressionwithoutblock);
    } else {
        // 没有尾表达式，返回单元类型
        resultValue = generateUnitValue();
    }
    
    // 退出作用域
    irBuilder->exitScope();
    scopeTree->ExitScope();
    
    return resultValue;
}

void ExpressionGenerator::generateBlockStatements(std::shared_ptr<BlockExpression> block) {
    // 遍历块内的所有语句，使用 StatementGenerator 生成
    for (const auto& stmt : block->statements) {
        statementGenerator->generateStatement(stmt);
    }
}

std::string ExpressionGenerator::generateUnitValue() {
    // 生成单元类型的值
    std::string unitReg = irBuilder->newRegister();
    irBuilder->emitInstruction("%" + unitReg + " = add i8 0, 0  ; unit value");
    return unitReg;
}
```

#### BlockExpression 的值处理

BlockExpression 作为表达式，必须有返回值：

1. **有尾表达式的情况**：
   ```cpp
   // 示例：{ let x = 1; let y = 2; x + y }
   // BlockExpression 的值是 x + y 的结果
   std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> expr) {
       // ... 生成语句 ...
       
       if (expr->expressionwithoutblock) {
           // 返回尾表达式的值
           return generateExpression(expr->expressionwithoutblock);
       }
       // ...
   }
   ```

2. **无尾表达式的情况**：
   ```cpp
   // 示例：{ let x = 1; printlnInt(x); }
   // BlockExpression 的值是单元类型
   std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> expr) {
       // ... 生成语句 ...
       
       if (!expr->expressionwithoutblock) {
           // 返回单元类型值
           return generateUnitValue();
       }
       // ...
   }
   ```

#### 嵌套 BlockExpression 处理

BlockExpression 可以嵌套，每个块都有自己的作用域：

```cpp
std::string ExpressionGenerator::generateNestedBlockExpression(std::shared_ptr<BlockExpression> expr) {
    // 外层块作用域
    scopeTree->EnterScope();
    irBuilder->enterScope();
    
    // 生成外层块语句
    generateBlockStatements(expr);
    
    // 处理内层块表达式
    std::string result;
    if (expr->expressionwithoutblock) {
        if (auto innerBlock = std::dynamic_pointer_cast<BlockExpression>(expr->expressionwithoutblock)) {
            // 递归处理内层块
            result = generateBlockExpression(innerBlock);
        } else {
            // 普通表达式
            result = generateExpression(expr->expressionwithoutblock);
        }
    } else {
        result = generateUnitValue();
    }
    
    // 退出外层块作用域
    irBuilder->exitScope();
    scopeTree->ExitScope();
    
    return result;
}
```

#### BlockExpression 在控制流中的使用

BlockExpression 常用于控制流表达式中：

```cpp
// if 表达式中的 BlockExpression
std::string ExpressionGenerator::generateIfExpression(std::shared_ptr<IfExpression> expr) {
    // 生成条件
    std::string condReg = generateExpression(expr->conditions->expression);
    
    // 创建基本块
    std::string thenBB = irBuilder->newBasicBlock("if.then");
    std::string elseBB = irBuilder->newBasicBlock("if.else");
    std::string endBB = irBuilder->newBasicBlock("if.end");
    
    // 生成条件跳转
    irBuilder->emitCondBr(condReg, thenBB, elseBB);
    
    // 生成 then 分支（BlockExpression）
    irBuilder->emitLabel(thenBB);
    std::string thenReg = generateBlockExpression(expr->ifblockexpression);
    irBuilder->emitBr(endBB);
    
    // 生成 else 分支（BlockExpression 或其他表达式）
    irBuilder->emitLabel(elseBB);
    std::string elseReg;
    if (expr->elseexpression) {
        elseReg = generateExpression(expr->elseexpression);
    } else {
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

### 组件协作使用

```cpp
// 创建 FunctionCodegen 并设置组件间通信
auto functionCodegen = std::make_shared<FunctionCodegen>(irBuilder, typeMapper, scopeTree);
exprGen.setFunctionCodegen(functionCodegen);
functionCodegen->setExpressionGenerator(exprGen);

// 生成普通函数调用
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

// 生成内置函数调用（自动调用 FunctionCodegen）
auto builtinCall = std::make_shared<PathExpression>(
    std::make_shared<SimplePath>("printlnInt", false, false));
auto builtinParams = std::make_shared<CallParams>(
    std::vector<std::shared_ptr<Expression>>{
        std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL)
    });
auto builtinExpr = std::make_shared<CallExpression>(builtinCall, builtinParams);

result = exprGen.generateExpression(builtinExpr);
// 输出：call void @printlnInt(i32 42)

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

1. **完整的表达式支持**：支持所有 Rx 语言表达式类型的 IR 生成，包括 BlockExpression
2. **BlockExpression 处理**：完整支持块表达式的语句生成和值计算，正确处理尾表达式和单元类型
3. **循环表达式处理**：
   - **loop 表达式**：详细说明如何从 break 处获取表达式的值（没有则为 unit）
   - **while 表达式**：明确其值恒定为 unit，不受 break 影响
   - **break/continue 跳转处理**：完善的循环上下文管理和控制流处理
   - **嵌套循环支持**：正确处理多层嵌套循环中的 break/continue 语义
4. **类型安全**：与 TypeMapper 紧密集成，确保类型正确性
5. **寄存器管理**：高效的寄存器分配和生命周期管理
6. **语义集成**：与语义分析结果完全集成
7. **易于扩展**：清晰的接口设计，便于添加新的表达式类型

通过 ExpressionGenerator，IR 生成器可以将复杂的 Rx 语言表达式正确地转换为高效的 LLVM IR 代码，并正确处理 BlockExpression 和循环表达式的特殊语义。
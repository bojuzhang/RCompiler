# ConstantEvaluator（常量求值）组件分析

## 概述

ConstantEvaluator 是语义分析的第二阶段，负责在编译时对常量表达式进行求值。这个组件识别并计算那些可以在编译时确定值的表达式，将结果存储起来供后续阶段使用。

## 整体工作逻辑

### 主要职责

1. **常量表达式识别** - 识别哪些表达式可以在编译时求值
2. **编译时计算** - 对常量表达式进行求值计算
3. **常量值存储** - 将求值结果存储为常量值对象
4. **错误检测** - 检测常量表达式中的错误

### 处理流程

```
开始 → 遍历AST → 识别常量上下文 → 表达式求值 → 存储结果 → 完成
```

## 核心接口分析

### 常量值类型系统

#### 基础常量值接口
```cpp
class ConstantValue {
public:
    virtual ~ConstantValue() = default;
    virtual std::string toString() const = 0;
};
```

**功能**：所有常量值的基类，提供统一的接口

#### 具体常量值类型

##### 整数常量
```cpp
class IntConstant : public ConstantValue {
private:
    int64_t value;
public:
    IntConstant(int64_t value) : value(value) {}
    int64_t getValue() const { return value; }
    std::string toString() const override { return std::to_string(value); }
};
```

##### 布尔常量
```cpp
class BoolConstant : public ConstantValue {
private:
    bool value;
public:
    BoolConstant(bool value) : value(value) {}
    bool getValue() const { return value; }
    std::string toString() const override { return value ? "true" : "false"; }
};
```

##### 字符常量
```cpp
class CharConstant : public ConstantValue {
private:
    char value;
public:
    CharConstant(char value) : value(value) {}
    char getValue() const { return value; }
    std::string toString() const override { 
        return std::string("'") + value + "'"; 
    }
};
```

##### 字符串常量
```cpp
class StringConstant : public ConstantValue {
private:
    std::string value;
public:
    StringConstant(const std::string& value) : value(value) {}
    const std::string& getValue() const { return value; }
    std::string toString() const override { 
        return "\"" + value + "\""; 
    }
};
```

##### 数组常量
```cpp
class ArrayConstant : public ConstantValue {
private:
    std::vector<std::shared_ptr<ConstantValue>> elements;
public:
    ArrayConstant(std::vector<std::shared_ptr<ConstantValue>> elements) 
        : elements(std::move(elements)) {}
    
    const std::vector<std::shared_ptr<ConstantValue>>& getElements() const { 
        return elements; 
    }
    
    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += elements[i]->toString();
        }
        result += "]";
        return result;
    }
};
```

### 主要求值器类

```cpp
class ConstantEvaluator : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;                              // 作用域树
    std::unordered_map<std::string, std::shared_ptr<ConstantValue>> constantValues; // 常量值存储
    std::stack<std::shared_ptr<ConstantValue>> evaluationStack;          // 求值栈
    std::stack<ASTNode*> nodeStack;                                   // AST节点栈
    
    bool hasErrors = false;                                             // 错误标志
    bool inConstContext = false;                                        // 常量上下文标志
};
```

### 核心接口方法

#### 1. 初始化和状态查询接口

```cpp
ConstantEvaluator(std::shared_ptr<ScopeTree> scopeTree);
bool EvaluateConstants();
bool HasEvaluationErrors() const;
std::shared_ptr<ConstantValue> GetConstantValue(const std::string& name);
```

**功能**：初始化求值器并提供状态查询

**实现细节**：
- 构造函数接收符号收集阶段建立的作用域树
- `EvaluateConstants()` 启动求值过程
- `GetConstantValue()` 提供常量值查询接口

#### 2. AST访问接口

##### 常量项访问
```cpp
void visit(ConstantItem& node) override;
```

**功能**：处理常量定义，进行编译时求值

**实现细节**：
- 设置常量上下文标志
- 对常量表达式进行求值
- 存储求值结果

**代码示例**：
```cpp
void ConstantEvaluator::visit(ConstantItem& node) {
    PushNode(node);
    
    std::string constName = node.identifier;
    
    bool previousConstContext = inConstContext;
    inConstContext = true;
    
    // 求值常量表达式
    auto value = EvaluateExpression(*node.expression);
    if (value) {
        constantValues[constName] = value;
    } else {
        ReportError("Cannot evaluate constant '" + constName + "' at compile time");
    }
    
    inConstContext = previousConstContext;
    PopNode();
}
```

##### 块表达式访问
```cpp
void visit(BlockExpression& node) override;
```

**功能**：处理块表达式中的常量求值

**实现细节**：
- 在常量上下文中遍历块内语句
- 处理尾表达式
- 返回最后一个表达式的值

**代码示例**：
```cpp
void ConstantEvaluator::visit(BlockExpression& node) {
    PushNode(node);
    
    if (inConstContext) {
        for (const auto& stmt : node.statements) {
            if (stmt) {
                stmt->accept(*this);
            }
        }
        
        // 处理尾表达式（如果有）
        if (node.expressionwithoutblock) {
            node.expressionwithoutblock->accept(*this);
        }
    } else {
        // 即使不在常量上下文中，也要处理函数内部的常量声明
        EvaluateBlockExpression(node);
    }
    
    PopNode();
}
```

#### 3. 表达式求值接口

##### 通用表达式求值
```cpp
std::shared_ptr<ConstantValue> EvaluateExpression(Expression& expr);
```

**功能**：对表达式进行求值的统一入口

**实现细节**：
- 首先检查表达式是否为编译时常量
- 根据表达式类型分发到具体的求值方法
- 返回求值结果或 nullptr（如果不是常量）

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateExpression(Expression& expr) {
    // 检查是否已经是编译时常量
    if (!IsCompileTimeConstant(expr)) {
        return nullptr;
    }
    
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        return EvaluateLiteral(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        return EvaluateBinaryExpression(*binary);
    } else if (auto unary = dynamic_cast<UnaryExpression*>(&expr)) {
        return EvaluateUnaryExpression(*unary);
    } else if (auto path = dynamic_cast<PathExpression*>(&expr)) {
        return EvaluatePathExpression(*path);
    }
    // ... 其他表达式类型
    
    return nullptr;
}
```

##### 字面量求值
```cpp
std::shared_ptr<ConstantValue> EvaluateLiteral(LiteralExpression& expr);
```

**功能**：对字面量表达式进行求值

**实现细节**：
- 根据字面量类型创建对应的常量值对象
- 处理不同进制的整数字面量
- 处理字符和字符串字面量的转义

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateLiteral(LiteralExpression& expr) {
    Token tokenType = expr.tokentype;
    const std::string& valueStr = expr.literal;
    
    switch (tokenType) {
        case Token::kINTEGER_LITERAL:
            try {
                int64_t value = std::stoll(valueStr);
                return std::make_shared<IntConstant>(value);
            } catch (const std::exception& e) {
                ReportError("Invalid integer literal: " + valueStr);
                return nullptr;
            }
            
        case Token::kCHAR_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '\'' && valueStr.back() == '\'') {
                if (valueStr.length() == 3) { // 'x'
                    return std::make_shared<CharConstant>(valueStr[1]);
                }
            }
            ReportError("Invalid char literal: " + valueStr);
            return nullptr;
            
        case Token::kSTRING_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '"' && valueStr.back() == '"') {
                std::string content = valueStr.substr(1, valueStr.length() - 2);
                return std::make_shared<StringConstant>(content);
            }
            ReportError("Invalid string literal: " + valueStr);
            return nullptr;
            
        case Token::ktrue:
            return std::make_shared<BoolConstant>(true);
            
        case Token::kfalse:
            return std::make_shared<BoolConstant>(false);
            
        default:
            ReportError("Unsupported literal type for constant evaluation");
            return nullptr;
    }
}
```

##### 二元表达式求值
```cpp
std::shared_ptr<ConstantValue> EvaluateBinaryExpression(BinaryExpression& expr);
```

**功能**：对二元表达式进行求值

**实现细节**：
- 递归求值左右操作数
- 检查操作数类型兼容性
- 根据运算符类型执行相应计算
- 处理除零等错误情况

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateBinaryExpression(BinaryExpression& expr) {
    auto leftValue = EvaluateExpression(*expr.leftexpression);
    auto rightValue = EvaluateExpression(*expr.rightexpression);
    
    if (!leftValue || !rightValue) {
        return nullptr;
    }
    
    // 类型检查：左右操作数类型必须兼容
    if (typeid(*leftValue) != typeid(*rightValue)) {
        ReportError("Type mismatch in binary expression");
        return nullptr;
    }
    
    Token op = expr.binarytype;
    
    if (auto leftInt = dynamic_cast<IntConstant*>(leftValue.get())) {
        auto rightInt = dynamic_cast<IntConstant*>(rightValue.get());
        int64_t left = leftInt->getValue();
        int64_t right = rightInt->getValue();
        
        switch (op) {
            case Token::kPlus:
                return std::make_shared<IntConstant>(left + right);
            case Token::kMinus:
                return std::make_shared<IntConstant>(left - right);
            case Token::kStar:
                return std::make_shared<IntConstant>(left * right);
            case Token::kSlash:
                if (right == 0) {
                    ReportError("Division by zero in constant expression");
                    return nullptr;
                }
                return std::make_shared<IntConstant>(left / right);
            // ... 其他运算符
        }
    } else if (auto leftBool = dynamic_cast<BoolConstant*>(leftValue.get())) {
        auto rightBool = dynamic_cast<BoolConstant*>(rightValue.get());
        bool left = leftBool->getValue();
        bool right = rightBool->getValue();
        
        switch (op) {
            case Token::kAndAnd:
                return std::make_shared<BoolConstant>(left && right);
            case Token::kOrOr:
                return std::make_shared<BoolConstant>(left || right);
            case Token::kEqEq:
                return std::make_shared<BoolConstant>(left == right);
            case Token::kNe:
                return std::make_shared<BoolConstant>(left != right);
            // ... 其他布尔运算符
        }
    }
    
    ReportError("Unsupported types for binary operation");
    return nullptr;
}
```

##### 一元表达式求值
```cpp
std::shared_ptr<ConstantValue> EvaluateUnaryExpression(UnaryExpression& expr);
```

**功能**：对一元表达式进行求值

**实现细节**：
- 求值操作数
- 根据一元运算符执行相应操作
- 支持算术和逻辑一元运算符

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateUnaryExpression(UnaryExpression& expr) {
    auto operandValue = EvaluateExpression(*expr.expression);
    if (!operandValue) {
        return nullptr;
    }
    
    Token op = expr.unarytype;
    
    if (auto intVal = dynamic_cast<IntConstant*>(operandValue.get())) {
        int64_t value = intVal->getValue();
        switch (op) {
            case Token::kMinus:
                return std::make_shared<IntConstant>(-value);
            case Token::kNot:
                return std::make_shared<IntConstant>(~value);
            default:
                ReportError("Unsupported unary operator for integers: " + to_string(op));
                return nullptr;
        }
    } else if (auto boolVal = dynamic_cast<BoolConstant*>(operandValue.get())) {
        bool value = boolVal->getValue();
        switch (op) {
            case Token::kNot:
                return std::make_shared<BoolConstant>(!value);
            default:
                ReportError("Unsupported unary operator for booleans: " + to_string(op));
                return nullptr;
        }
    }
    
    ReportError("Unsupported type for unary operation");
    return nullptr;
}
```

##### 数组表达式求值
```cpp
std::shared_ptr<ConstantValue> EvaluateArrayExpression(ArrayExpression& expr);
```

**功能**：对数组表达式进行求值

**实现细节**：
- 求值所有数组元素
- 检查所有元素是否为常量
- 创建数组常量对象

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateArrayExpression(ArrayExpression& expr) {
    if (!expr.arrayelements) {
        return nullptr;
    }
    
    std::vector<std::shared_ptr<ConstantValue>> evaluatedElements;
    for (const auto& element : expr.arrayelements->expressions) {
        auto elementValue = EvaluateExpression(*element);
        if (!elementValue) {
            return nullptr;
        }
        evaluatedElements.push_back(elementValue);
    }
    
    return std::make_shared<ArrayConstant>(std::move(evaluatedElements));
}
```

##### 路径表达式求值
```cpp
std::shared_ptr<ConstantValue> EvaluatePathExpression(PathExpression& expr);
```

**功能**：对路径表达式（变量引用）进行求值

**实现细节**：
- 从常量值存储中查找已定义的常量
- 支持简单的常量名查找
- 处理未定义常量的错误

**代码示例**：
```cpp
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluatePathExpression(PathExpression& expr) {
    if (!expr.simplepath || expr.simplepath->simplepathsegements.empty()) {
        return nullptr;
    }
    
    // 简化处理：只处理单段路径（常量名）
    std::string constName = expr.simplepath->simplepathsegements[0]->identifier;
    
    auto it = constantValues.find(constName);
    if (it != constantValues.end()) {
        return it->second;
    }
    
    // 尝试从作用域树中查找符号
    if (scopeTree) {
        auto symbol = scopeTree->LookupSymbol(constName);
        if (symbol && symbol->kind == SymbolKind::Constant) {
            ReportError("Constant '" + constName + "' found but not evaluated yet");
            return nullptr;
        }
    }
    
    ReportError("Undefined constant in constant expression: " + constName);
    return nullptr;
}
```

#### 4. 常量性检查接口

```cpp
bool IsCompileTimeConstant(Expression& expr);
```

**功能**：检查表达式是否可以在编译时求值

**实现细节**：
- 递归检查表达式结构
- 只有字面量、常量引用和它们的组合才被认为是编译时常量
- 排除函数调用、变量引用等运行时表达式

**代码示例**：
```cpp
bool ConstantEvaluator::IsCompileTimeConstant(Expression& expr) {
    // 检查表达式是否可以在编译时求值
    if (dynamic_cast<LiteralExpression*>(&expr)) {
        return true;
    } else if (auto path = dynamic_cast<PathExpression*>(&expr)) {
        // 路径表达式必须是常量
        if (path->simplepath) {
            auto segments = path->simplepath->simplepathsegements;
            if (segments.size() == 1) {
                std::string name = segments[0]->identifier;
                return constantValues.find(name) != constantValues.end();
            }
        }
        return false;
    } else if (dynamic_cast<BinaryExpression*>(&expr)) {
        auto binary = dynamic_cast<BinaryExpression*>(&expr);
        return IsCompileTimeConstant(*binary->leftexpression) &&
               IsCompileTimeConstant(*binary->rightexpression);
    } else if (dynamic_cast<UnaryExpression*>(&expr)) {
        auto unary = dynamic_cast<UnaryExpression*>(&expr);
        return IsCompileTimeConstant(*unary->expression);
    }
    // ... 其他表达式类型检查
    
    return false;
}
```

## 与其他组件的交互

### 输入依赖

1. **ScopeTree** - 来自符号收集阶段的作用域树，用于查找符号信息
2. **AST** - 抽象语法树，作为求值的输入

### 输出接口

```cpp
std::shared_ptr<ConstantValue> GetConstantValue(const std::string& name);
bool HasEvaluationErrors() const;
```

### 为后续阶段提供的信息

1. **常量值** - 为类型检查提供编译时常量值
2. **常量表达式结果** - 用于数组大小检查等
3. **错误信息** - 常量求值阶段的错误报告

### 使用约定

- 后续阶段可以通过 `GetConstantValue()` 查询常量值
- 常量值不可修改，只能查询
- 求值错误会阻止编译继续进行

## 错误处理

### 错误类型

1. **非常量表达式** - 试图对运行时表达式进行编译时求值
2. **类型不匹配** - 二元运算中操作数类型不兼容
3. **除零错误** - 除法或取模运算中除数为零
4. **未定义常量** - 引用了未定义的常量
5. **溢出错误** - 整数运算溢出

### 错误报告机制

```cpp
void ReportError(const std::string& message);
```

所有错误通过标准错误输出报告，并设置 `hasErrors` 标志。

## 设计特点

### 1. 分层求值策略

采用分层求值确保正确性：
- 首先检查表达式的常量性
- 然后进行递归求值
- 最后验证结果的有效性

### 2. 类型安全的常量值

使用继承体系确保类型安全：
- 基类提供统一接口
- 派生类实现具体类型
- 运行时类型检查防止类型错误

### 3. 上下文感知求值

根据上下文决定是否进行求值：
- 常量定义中的表达式必须求值
- 函数体中的表达式选择性求值
- 避免不必要的求值开销

### 4. 错误恢复机制

提供适当的错误恢复：
- 单个常量求值失败不影响其他常量
- 详细的错误信息帮助定位问题
- 错误标志阻止后续阶段继续

### 5. 可扩展性

设计支持未来扩展：
- 新的常量类型可以轻松添加
- 新的运算符可以通过扩展求值方法支持
- 模块化设计便于维护

## 语法检查详细分析

### 检查问题分类

ConstantEvaluator 组件在常量求值阶段进行与常量表达式相关的语法检查，主要检查以下类型的问题：

#### 1. 常量表达式错误
- **非常量表达式**：在需要常量的地方使用了非常量表达式
- **常量求值失败**：表达式无法在编译时求值
- **循环依赖**：常量定义之间存在循环依赖

#### 2. 数值计算错误
- **除零错误**：常量表达式中出现除零操作
- **数值溢出**：常量计算结果超出类型范围
- **无效运算**：不支持的操作组合

#### 3. 类型转换错误
- **常量类型不匹配**：常量值与期望类型不兼容
- **无效类型转换**：不支持的类型转换操作
- **精度丢失**：类型转换导致精度丢失

#### 4. 数组常量错误
- **数组大小不是常量**：数组大小使用了非常量表达式
- **数组元素类型不一致**：数组常量中元素类型不统一
- **多维数组错误**：多维数组常量的维度错误

#### 5. 字符串常量错误
- **无效字符**：字符串中包含无效字符
- **字符串长度超限**：字符串常量长度超过限制
- **编码错误**：字符串编码问题

### 具体检查实现

#### 1. 常量表达式检查

**检查内容**：
- 表达式是否为编译时常量
- 常量表达式的有效性
- 常量值的类型正确性

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> EvaluateConstantExpression(Expression& expr) {
    // 检查 1：表达式是否为常量
    if (!IsCompileTimeConstant(expr)) {
        ReportError("Expression is not a compile-time constant");
        return nullptr;
    }
    
    // 检查 2：循环依赖检测
    if (IsEvaluating(&expr)) {
        ReportError("Circular dependency in constant expression");
        return nullptr;
    }
    
    // 标记正在求值
    SetEvaluating(&expr, true);
    
    // 执行求值
    auto result = ActuallyEvaluate(expr);
    
    // 清除标记
    SetEvaluating(&expr, false);
    
    return result;
}
```

**检查的错误类型**：
- `Expression is not a compile-time constant`
- `Circular dependency in constant expression`
- `Constant expression evaluation failed`

#### 2. 数值运算检查

**检查内容**：
- 算术运算的有效性
- 除零错误检测
- 数值溢出检查

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> EvaluateBinaryOperation(BinaryExpression& expr) {
    auto leftValue = EvaluateConstantExpression(*expr.left);
    auto rightValue = EvaluateConstantExpression(*expr.right);
    
    if (!leftValue || !rightValue) {
        return nullptr;
    }
    
    // 检查 1：除零错误
    if (expr.op == TokenType::DIVIDE) {
        if (auto rightInt = dynamic_cast<IntConstant*>(rightValue.get())) {
            if (rightInt->GetValue() == 0) {
                ReportError("Division by zero in constant expression");
                return nullptr;
            }
        }
    }
    
    // 检查 2：运算类型兼容性
    if (!AreCompatibleForOperation(leftValue, rightValue, expr.op)) {
        ReportError("Incompatible types for binary operation");
        return nullptr;
    }
    
    // 执行运算并检查溢出
    return PerformCheckedOperation(leftValue, rightValue, expr.op);
}
```

**检查的错误类型**：
- `Division by zero in constant expression`
- `Integer overflow in constant expression`
- `Incompatible types for binary operation`

#### 3. 数组常量检查

**检查内容**：
- 数组大小的常量性
- 数组元素类型一致性
- 数组初始化的正确性

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> EvaluateArrayConstant(ArrayExpression& expr) {
    // 检查 1：数组大小是否为常量
    if (expr.size) {
        auto sizeValue = EvaluateConstantExpression(*expr.size);
        if (!sizeValue) {
            ReportError("Array size must be a compile-time constant");
            return nullptr;
        }
        
        if (auto sizeInt = dynamic_cast<IntConstant*>(sizeValue.get())) {
            if (sizeInt->GetValue() < 0) {
                ReportError("Array size cannot be negative");
                return nullptr;
            }
        }
    }
    
    // 检查 2：元素类型一致性
    std::shared_ptr<Type> elementType = nullptr;
    for (const auto& element : expr.elements) {
        auto elementValue = EvaluateConstantExpression(*element);
        if (!elementValue) {
            ReportError("Array element is not a compile-time constant");
            return nullptr;
        }
        
        auto elementType = elementValue->GetType();
        if (!firstElementType) {
            firstElementType = elementType;
        } else if (!AreTypesEqual(firstElementType, elementType)) {
            ReportError("Array elements must have same type");
            return nullptr;
        }
    }
    
    return CreateArrayConstant(expr.elements);
}
```

**检查的错误类型**：
- `Array size must be a compile-time constant`
- `Array size cannot be negative`
- `Array elements must have same type`

#### 4. 字符串常量检查

**检查内容**：
- 字符串字面量的有效性
- 转义序列的正确性
- 字符串长度限制

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> EvaluateStringConstant(const std::string& literal) {
    // 检查 1：字符串长度限制
    if (literal.length() > MAX_STRING_LENGTH) {
        ReportError("String literal too long (max " + std::to_string(MAX_STRING_LENGTH) + " characters)");
        return nullptr;
    }
    
    // 检查 2：转义序列有效性
    std::string processed;
    for (size_t i = 0; i < literal.length(); ++i) {
        if (literal[i] == '\\') {
            if (i + 1 >= literal.length()) {
                ReportError("Incomplete escape sequence at end of string");
                return nullptr;
            }
            
            char escapeChar = literal[i + 1];
            if (!IsValidEscapeChar(escapeChar)) {
                ReportError("Invalid escape sequence: \\" + std::string(1, escapeChar));
                return nullptr;
            }
            
            processed += ProcessEscapeSequence(escapeChar);
            i++; // 跳过转义字符
        } else {
            processed += literal[i];
        }
    }
    
    return std::make_shared<StringConstant>(processed);
}
```

**检查的错误类型**：
- `String literal too long (max 65535 characters)`
- `Incomplete escape sequence at end of string`
- `Invalid escape sequence: \x`

#### 5. 类型转换检查

**检查内容**：
- 常量类型转换的有效性
- 数值范围检查
- 精度丢失检测

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> CheckTypeConversion(std::shared_ptr<ConstantValue> value, 
                                                   Type* targetType) {
    auto sourceType = value->GetType();
    
    // 检查 1：转换是否允许
    if (!IsConversionAllowed(sourceType, targetType)) {
        ReportError("Cannot convert from '" + sourceType->ToString() + 
                   "' to '" + targetType->ToString() + "' in constant context");
        return nullptr;
    }
    
    // 检查 2：数值范围
    if (auto intValue = dynamic_cast<IntConstant*>(value.get())) {
        if (IsIntegerType(targetType)) {
            if (!IsValueInRange(intValue->GetValue(), targetType)) {
                ReportError("Constant value out of range for target type");
                return nullptr;
            }
        }
    }
    
    // 执行转换
    return PerformConversion(value, targetType);
}
```

**检查的错误类型**：
- `Cannot convert from 'string' to 'i32' in constant context`
- `Constant value out of range for target type`
- `Precision loss in constant conversion`

#### 6. 函数调用常量检查

**检查内容**：
- 常量函数的有效性
- 参数的常量性
- 内置常量函数的支持

**实现方式**：
```cpp
std::shared_ptr<ConstantValue> EvaluateConstantFunctionCall(CallExpression& expr) {
    // 检查 1：是否为常量函数
    if (!IsConstantFunction(expr.functionName)) {
        ReportError("Function '" + expr.functionName + "' is not a constant function");
        return nullptr;
    }
    
    // 检查 2：参数常量性
    for (const auto& arg : expr.arguments) {
        if (!IsCompileTimeConstant(*arg)) {
            ReportError("Argument to constant function must be constant");
            return nullptr;
        }
    }
    
    // 检查 3：内置常量函数
    if (IsBuiltinConstantFunction(expr.functionName)) {
        return EvaluateBuiltinConstantFunction(expr);
    }
    
    return EvaluateUserDefinedConstantFunction(expr);
}
```

**检查的错误类型**：
- `Function 'foo' is not a constant function`
- `Argument to constant function must be constant`
- `Constant function evaluation failed`

#### 7. 循环依赖检查

**检查内容**：
- 常量定义的依赖关系
- 循环依赖检测
- 依赖拓扑排序

**实现方式**：
```cpp
bool CheckCircularDependency(const std::string& constantName) {
    // 使用 DFS 检测循环依赖
    std::set<std::string> visiting;
    std::set<std::string> visited;
    
    return HasCircularDependency(constantName, visiting, visited);
}

bool HasCircularDependency(const std::string& name, 
                          std::set<std::string>& visiting,
                          std::set<std::string>& visited) {
    if (visiting.count(name)) {
        // 发现循环依赖
        ReportError("Circular dependency detected involving constant: " + name);
        return true;
    }
    
    if (visited.count(name)) {
        return false; // 已经检查过，无循环
    }
    
    visiting.insert(name);
    
    // 检查所有依赖
    auto dependencies = GetConstantDependencies(name);
    for (const auto& dep : dependencies) {
        if (HasCircularDependency(dep, visiting, visited)) {
            return true;
        }
    }
    
    visiting.erase(name);
    visited.insert(name);
    return false;
}
```

**检查的错误类型**：
- `Circular dependency detected involving constant: A`
- `Constant definition depends on itself`
- `Complex circular dependency in constant definitions`

### 错误恢复策略

#### 1. 部分求值恢复
- 遇到错误时返回默认常量值
- 继续处理其他常量表达式
- 保持常量表的完整性

#### 2. 类型默认值
- 数值类型返回 0
- 布尔类型返回 false
- 字符串类型返回空字符串

#### 3. 错误上下文保持
- 记录求值路径
- 提供依赖关系信息
- 给出修复建议

### 检查性能优化

#### 1. 求值结果缓存
- 常量表达式结果缓存
- 依赖关系缓存
- 类型检查结果缓存

#### 2. 增量求值
- 只重新求值变化的常量
- 依赖图增量更新
- 智能失效策略

#### 3. 内存管理
- 常量值对象池
- 垃圾回收优化
- 内存使用监控

通过这些详细的语法检查机制，ConstantEvaluator 确保了 Rx 语言程序中常量表达式的正确性和编译时求值的可靠性，为后续的语义分析阶段提供了准确的常量信息基础。
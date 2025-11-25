# ControlFlowAnalyzer（控制流分析）组件分析

## 概述

ControlFlowAnalyzer 是语义分析的第三阶段，负责分析程序的控制流特性，包括发散性分析、循环结构分析、返回语句分析等。这个组件为类型检查阶段提供控制流信息，特别是 never 类型的推断。

## 整体工作逻辑

### 主要职责

1. **控制流分析** - 分析程序的控制流路径和特性
2. **发散性检测** - 识别永远不会返回的表达式
3. **循环结构分析** - 分析 loop、while 等循环的控制流
4. **类型推断** - 推断基于控制流的表达式类型
5. **有效性检查** - 检查 break、continue 的使用有效性

### 处理流程

```
开始 → 遍历AST → 分析控制流 → 检测发散性 → 推断类型 → 存储 → 完成
```

## 核心接口分析

### 控制流类型定义

```cpp
enum class ControlFlow {
    Continues,    // 正常继续
    Breaks,       // 包含break
    ContinuesLoop,// 包含continue  
    Returns,      // 包含return
    Diverges      // 发散（never类型）
};
```

**功能**：定义不同的控制流状态，用于描述表达式的控制流行为

### Never 类型定义

```cpp
class NeverType : public SemanticType {
public:
    std::string tostring() const override { return "!"; }
};
```

**功能**：表示 never 类型（发散类型），用于标记永远不会正常返回的表达式

### 主要分析器类

```cpp
class ControlFlowAnalyzer : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;                              // 作用域树
    std::shared_ptr<ConstantEvaluator> constantEvaluator;                // 常量求值器
    std::stack<ASTNode*> nodeStack;                                   // AST节点栈
    std::stack<int> loopDepthStack;                                    // 循环深度栈
    std::stack<ControlFlow> controlFlowStack;                            // 控制流栈

    bool hasErrors = false;                                             // 错误标志
    int currentLoopDepth = 0;                                         // 当前循环深度
    bool inConstContext = false;                                        // 常量上下文标志
    
    std::unordered_map<ASTNode*, ControlFlow> nodeControlFlow;          // 节点控制流映射
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypes; // 节点类型映射
    std::unordered_map<ASTNode*, bool> alwaysDiverges;                   // 节点发散性映射
};
```

### 核心接口方法

#### 1. 初始化和状态查询接口

```cpp
ControlFlowAnalyzer(std::shared_ptr<ScopeTree> scopeTree, 
                   std::shared_ptr<ConstantEvaluator> constantEvaluator);
bool AnalyzeControlFlow();
bool HasAnalysisErrors() const;
ControlFlow GetControlFlow(ASTNode* node) const;
std::shared_ptr<SemanticType> GetNodeType(ASTNode* node) const;
bool AlwaysDivergesAt(ASTNode* node) const;
```

**功能**：初始化分析器并提供状态查询接口

**实现细节**：
- 构造函数接收前面阶段的作用域树和常量求值器
- `AnalyzeControlFlow()` 启动控制流分析过程
- 查询方法提供分析结果的访问

#### 2. AST访问接口

##### 函数访问
```cpp
void visit(Function& node) override;
```

**功能**：分析函数的控制流特性

**实现细节**：
- 检查函数体的控制流
- 分析返回类型与函数体发散性的关系
- 处理函数总是发散的情况

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(Function& node) {
    PushNode(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
        // 检查函数返回类型
        if (node.functionreturntype) {
            auto returnType = node.functionreturntype->type;
            auto bodyType = GetNodeType(node.blockexpression.get());
            
            // 如果函数体发散，则不需要返回语句
            if (bodyType && bodyType->tostring() != "!" && 
                GetControlFlow(node.blockexpression.get()) == ControlFlow::Diverges) {
                // 需要检查是否有return语句或发散表达式
            }
        }
    }
    
    PopNode();
}
```

##### 块表达式访问
```cpp
void visit(BlockExpression& node) override;
```

**功能**：分析块表达式的控制流

**实现细节**：
- 进入已存在的作用域
- 遍历块内所有语句
- 分析尾表达式
- 推断块的控制流和类型

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(BlockExpression& node) {
    PushNode(node);
    // 使用符号收集阶段已经创建的作用域，不创建新作用域
    scopeTree->EnterExistingScope(&node);
    for (const auto& stmt : node.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    
    // 分析尾表达式（如果有）
    if (node.expressionwithoutblock) {
        node.expressionwithoutblock->accept(*this);
    }
    
    // 分析块的控制流
    ControlFlow flow = AnalyzeBlockControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断块的类型
    std::shared_ptr<SemanticType> type = InferBlockType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    // 退出作用域
    scopeTree->ExitScope();
    PopNode();
}
```

##### 无限循环访问
```cpp
void visit(InfiniteLoopExpression& node) override;
```

**功能**：分析无限循环（loop）的控制流

**实现细节**：
- 进入循环上下文
- 分析循环体
- 判断循环是否发散
- 处理 break 表达式的影响

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(InfiniteLoopExpression& node) {
    PushNode(node);
    EnterLoop();
    
    // 分析循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    // 无限循环本身发散（除非有break）
    ControlFlow bodyFlow = GetControlFlow(node.blockexpression.get());
    if (bodyFlow != ControlFlow::Breaks) {
        // 循环体没有break，整个循环发散
        nodeControlFlow[&node] = ControlFlow::Diverges;
        nodeTypes[&node] = std::make_shared<NeverType>();
        alwaysDiverges[&node] = true;
    } else {
        // 循环体有break，类型由break表达式决定
        nodeControlFlow[&node] = ControlFlow::Continues;
        // 这里需要统一所有break的类型（简化处理）
        nodeTypes[&node] = std::make_shared<SimpleType>("()");
        alwaysDiverges[&node] = false;
    }
    
    ExitLoop();
    PopNode();
}
```

##### 谓词循环访问
```cpp
void visit(PredicateLoopExpression& node) override;
```

**功能**：分析谓词循环（while）的控制流

**实现细节**：
- 分析循环条件
- 分析循环体
- while 循环总是正常继续（除非内部有发散语句）

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(PredicateLoopExpression& node) {
    PushNode(node);
    EnterLoop();
    
    // 分析条件
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    // 分析循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    // 谓词循环：条件为false时退出
    // 控制流分析更复杂，这里简化处理
    nodeControlFlow[&node] = ControlFlow::Continues;
    nodeTypes[&node] = std::make_shared<SimpleType>("()");
    alwaysDiverges[&node] = false;
    
    ExitLoop();
    PopNode();
}
```

##### If 表达式访问
```cpp
void visit(IfExpression& node) override;
```

**功能**：分析 if 表达式的控制流

**实现细节**：
- 分析条件表达式
- 分析 if 分支
- 分析 else 分支（如果有）
- 推断整个 if 表达式的类型和发散性

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(IfExpression& node) {
    PushNode(node);
    
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    node.ifblockexpression->accept(*this);
    if (node.elseexpression) {
        node.elseexpression->accept(*this);
    }
    
    ControlFlow flow = AnalyzeIfControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断类型
    std::shared_ptr<SemanticType> type = InferIfType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    PopNode();
}
```

##### 控制语句访问

###### Break 表达式
```cpp
void visit(BreakExpression& node) override;
```

**功能**：分析 break 表达式

**实现细节**：
- 检查 break 是否在循环内
- 标记为发散表达式
- 处理 break 带值的情况

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(BreakExpression& node) {
    PushNode(node);
    
    // 检查break是否在循环内
    CheckBreakContinueValidity(node, Token::kbreak);
    
    // break表达式发散
    nodeControlFlow[&node] = ControlFlow::Breaks;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}
```

###### Continue 表达式
```cpp
void visit(ContinueExpression& node) override;
```

**功能**：分析 continue 表达式

**实现细节**：
- 检查 continue 是否在循环内
- 标记为发散表达式

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(ContinueExpression& node) {
    PushNode(node);
    
    // 检查continue是否在循环内
    CheckBreakContinueValidity(node, Token::kcontinue);
    
    // continue表达式发散
    nodeControlFlow[&node] = ControlFlow::ContinuesLoop;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    PopNode();
}
```

###### Return 表达式
```cpp
void visit(ReturnExpression& node) override;
```

**功能**：分析 return 表达式

**实现细节**：
- 标记为发散表达式
- 分析返回值的类型

**代码示例**：
```cpp
void ControlFlowAnalyzer::visit(ReturnExpression& node) {
    PushNode(node);
    
    // return表达式发散
    nodeControlFlow[&node] = ControlFlow::Returns;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    // 分析返回值
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}
```

#### 3. 控制流分析方法

##### 块控制流分析
```cpp
ControlFlow AnalyzeBlockControlFlow(BlockExpression& block);
```

**功能**：分析块表达式的控制流特性

**实现细节**：
- 遍历块内所有语句
- 检查是否有发散语句
- 分析尾表达式的影响

**代码示例**：
```cpp
ControlFlow ControlFlowAnalyzer::AnalyzeBlockControlFlow(BlockExpression& block) {
    ControlFlow resultFlow = ControlFlow::Continues;
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            resultFlow = GetControlFlow(stmt.get());
            // 如果遇到发散语句，可以提前退出
            if (resultFlow == ControlFlow::Diverges ||
                resultFlow == ControlFlow::Returns ||
                resultFlow == ControlFlow::Breaks ||
                resultFlow == ControlFlow::ContinuesLoop) {
                break;
            }
        }
    }
    
    // 如果有尾表达式，分析它
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        resultFlow = GetControlFlow(block.expressionwithoutblock.get());
    }
    
    return resultFlow;
}
```

##### If 控制流分析
```cpp
ControlFlow AnalyzeIfControlFlow(IfExpression& ifExpr);
```

**功能**：分析 if 表达式的控制流

**实现细节**：
- 分析 if 分支的控制流
- 分析 else 分支的控制流（如果有）
- 判断整个 if 表达式的发散性

**代码示例**：
```cpp
ControlFlow ControlFlowAnalyzer::AnalyzeIfControlFlow(IfExpression& ifExpr) {
    auto ifFlow = GetControlFlow(ifExpr.ifblockexpression.get());
    if (!ifExpr.elseexpression) {
        // 没有else分支，if表达式不影响控制流
        return ControlFlow::Continues;
    }
    
    auto elseFlow = GetControlFlow(ifExpr.elseexpression.get());
    
    // 如果两个分支都发散，则整个if表达式发散
    if ((ifFlow == ControlFlow::Diverges || ifFlow == ControlFlow::Returns || 
         ifFlow == ControlFlow::Breaks || ifFlow == ControlFlow::ContinuesLoop) &&
        (elseFlow == ControlFlow::Diverges || elseFlow == ControlFlow::Returns ||
         elseFlow == ControlFlow::Breaks || elseFlow == ControlFlow::ContinuesLoop)) {
        return ControlFlow::Diverges;
    }
    
    return ControlFlow::Continues;
}
```

#### 4. 类型推断方法

##### 块类型推断
```cpp
std::shared_ptr<SemanticType> InferBlockType(BlockExpression& block);
```

**功能**：推断块表达式的类型

**实现细节**：
- 检查块内是否有发散语句
- 分析尾表达式的类型
- 根据 Rx 语言规则确定块的类型

**代码示例**：
```cpp
std::shared_ptr<SemanticType> ControlFlowAnalyzer::InferBlockType(BlockExpression& block) {
    std::shared_ptr<SemanticType> resultType = std::make_shared<SimpleType>("()");
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            // 如果语句发散，则块发散
            if (AlwaysDivergesAt(stmt.get())) {
                return std::make_shared<NeverType>();
            }
        }
    }
    
    // 如果有尾表达式，块的类型由尾表达式决定
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        // 如果尾表达式发散，则块发散
        if (AlwaysDivergesAt(block.expressionwithoutblock.get())) {
            return std::make_shared<NeverType>();
        }
        // 否则，块的类型是尾表达式的类型
        return GetNodeType(block.expressionwithoutblock.get());
    }
    
    // 如果没有尾表达式，返回unit类型
    return resultType;
}
```

##### If 类型推断
```cpp
std::shared_ptr<SemanticType> InferIfType(IfExpression& ifExpr);
```

**功能**：推断 if 表达式的类型

**实现细节**：
- 分析两个分支的类型
- 处理 never 类型的特殊规则
- 检查类型兼容性

**代码示例**：
```cpp
std::shared_ptr<SemanticType> ControlFlowAnalyzer::InferIfType(IfExpression& ifExpr) {
    auto ifType = GetNodeType(ifExpr.ifblockexpression.get());
    if (!ifExpr.elseexpression) {
        // 没有else分支，返回unit类型
        return std::make_shared<SimpleType>("()");
    }
    
    auto elseType = GetNodeType(ifExpr.elseexpression.get());
    // 如果两个分支都发散，则类型为!
    if (ifType && ifType->tostring() == "!" && elseType && elseType->tostring() == "!") {
        return std::make_shared<NeverType>();
    }
    // 如果一个分支发散（!类型），另一个分支不发散，则if表达式类型为非发散分支的类型
    if (ifType && ifType->tostring() == "!" && elseType && elseType->tostring() != "!") {
        return elseType;
    }
    if (elseType && elseType->tostring() == "!" && ifType && ifType->tostring() != "!") {
        return ifType;
    }
    
    // 如果两个分支都不发散，检查类型兼容性
    if (ifType && elseType && ifType->tostring() != "!" && elseType->tostring() != "!") {
        // 检查类型是否兼容
        if (ifType->tostring() == elseType->tostring()) {
            return ifType;
        } else {
            // 类型不兼容，这里应该报告错误，但为了保持控制流分析的连续性，
            // 我们返回一个错误类型或默认类型
            return std::make_shared<SimpleType>("type_error");
        }
    }
    
    // 默认情况，返回if分支类型
    return ifType;
}
```

#### 5. 循环上下文管理

```cpp
void EnterLoop();
void ExitLoop();
bool InLoop() const;
```

**功能**：管理循环上下文，用于检查 break、continue 的有效性

**实现细节**：
- `EnterLoop()` 增加循环深度
- `ExitLoop()` 减少循环深度
- `InLoop()` 检查当前是否在循环内

**代码示例**：
```cpp
void ControlFlowAnalyzer::EnterLoop() {
    currentLoopDepth++;
    loopDepthStack.push(currentLoopDepth);
}

void ControlFlowAnalyzer::ExitLoop() {
    if (!loopDepthStack.empty()) {
        loopDepthStack.pop();
    }
    currentLoopDepth = loopDepthStack.empty() ? 0 : loopDepthStack.top();
}

bool ControlFlowAnalyzer::InLoop() const {
    return currentLoopDepth > 0;
}
```

#### 6. 有效性检查

```cpp
void CheckBreakContinueValidity(ASTNode& node, Token tokenType);
```

**功能**：检查 break、continue 语句的使用有效性

**实现细节**：
- 检查是否在循环内使用
- 报告无效的 break、continue 使用

**代码示例**：
```cpp
void ControlFlowAnalyzer::CheckBreakContinueValidity(ASTNode& node, Token tokenType) {
    std::string keyword = tokenType == Token::kbreak ? "break" : "continue";
    if (!InLoop()) {
        ReportError(keyword + " expression outside of loop");
    }
}
```

## 与其他组件的交互

### 输入依赖

1. **ScopeTree** - 来自符号收集阶段的作用域树
2. **ConstantEvaluator** - 常量求值器，用于获取常量值
3. **AST** - 抽象语法树，作为分析的输入

### 输出接口

```cpp
ControlFlow GetControlFlow(ASTNode* node) const;
std::shared_ptr<SemanticType> GetNodeType(ASTNode* node) const;
bool AlwaysDivergesAt(ASTNode* node) const;
bool HasAnalysisErrors() const;
```

### 为后续阶段提供的信息

1. **控制流信息** - 每个节点的控制流特性
2. **类型信息** - 基于控制流的类型推断结果
3. **发散性信息** - 标记发散的表达式
4. **有效性检查结果** - break、continue 的使用有效性

### 使用约定

- 后续阶段可以查询节点的控制流特性
- 类型检查阶段可以使用推断的类型信息
- 控制流信息不可修改，只能查询

## 错误处理

### 错误类型

1. **无效控制语句** - break、continue 在循环外使用
2. **发散性分析错误** - 控制流分析过程中的内部错误
3. **类型推断错误** - 基于控制流的类型推断失败

### 错误报告机制

```cpp
void ReportError(const std::string& message);
```

所有错误通过标准错误输出报告，并设置 `hasErrors` 标志。

## 设计特点

### 1. 分层分析策略

采用分层分析确保准确性：
- 首先分析基本控制流
- 然后推断类型信息
- 最后检查有效性

### 2. 状态驱动的分析

使用状态驱动的方式进行分析：
- 循环深度栈跟踪循环嵌套
- 控制流栈跟踪复杂控制流
- 节点映射存储分析结果

### 3. Never 类型支持

完整支持 never 类型系统：
- 识别发散表达式
- 正确处理 never 类型的类型规则
- 支持 never 类型的类型推断

### 4. Rx 语言特性支持

针对 Rx 语言的特殊设计：
- 支持 loop 表达式的 break 值
- 支持 if 表达式的类型推断规则
- 支持块表达式的尾表达式类型

### 5. 可扩展的架构

设计支持未来扩展：
- 新的控制流结构可以轻松添加
- 新的类型推断规则可以集成
- 模块化设计便于维护

## 语法检查详细分析

### 检查问题分类

ControlFlowAnalyzer 组件在控制流分析阶段进行与程序控制流相关的语法检查，主要检查以下类型的问题：

#### 1. 控制语句错误
- **break 位置错误**：在非循环结构中使用 break 语句
- **continue 位置错误**：在非循环结构中使用 continue 语句
- **return 位置错误**：在不允许返回的位置使用 return 语句

#### 2. 控制流完整性错误
- **函数缺少返回**：有返回类型的函数缺少 return 语句
- **不可达代码**：永远不会执行的代码段
- **发散表达式误用**：在不期望发散的地方使用发散表达式

#### 3. 循环结构错误
- **无限循环检测**：没有退出条件的循环
- **循环变量未使用**：循环控制变量未被使用
- **循环体发散**：循环体总是发散导致无法正常退出

#### 4. 条件表达式错误
- **条件表达式类型错误**：if 条件不是布尔类型
- **分支类型不匹配**：if 表达式的分支类型不兼容
- **条件常量警告**：条件表达式总是为真或假

#### 5. Never 类型使用错误
- **Never 类型误用**：在不合适的上下文中使用 Never 类型
- **发散函数调用**：调用发散函数后的代码不可达
- **Never 类型转换**：Never 类型与其他类型的不当转换

### 具体检查实现

#### 1. 控制语句位置检查

**检查内容**：
- break 语句必须在循环内部
- continue 语句必须在循环内部
- return 语句必须在函数内部

**实现方式**：
```cpp
void CheckControlStatement(ControlStatement& stmt) {
    switch (stmt.type) {
        case ControlStatementType::Break:
            if (!IsInsideLoop(&stmt)) {
                ReportError("Break can only be used inside loops");
            }
            break;
            
        case ControlStatementType::Continue:
            if (!IsInsideLoop(&stmt)) {
                ReportError("Continue can only be used inside loops");
            }
            break;
            
        case ControlStatementType::Return:
            if (!IsInsideFunction(&stmt)) {
                ReportError("Return can only be used inside functions");
            }
            CheckReturnType(stmt);
            break;
    }
}
```

**检查的错误类型**：
- `Break can only be used inside loops`
- `Continue can only be used inside loops`
- `Return can only be used inside functions`

#### 2. 函数返回完整性检查

**检查内容**：
- 有返回类型的函数必须有明确的返回路径
- 所有代码路径都必须有适当的返回语句
- 返回类型必须与函数声明一致

**实现方式**：
```cpp
void CheckFunctionReturnCompleteness(Function& function) {
    if (!function.returnType || IsNeverType(function.returnType)) {
        return; // 无返回类型或 Never 类型不需要检查
    }
    
    auto returnFlow = AnalyzeFunctionBodyControlFlow(function.body);
    
    // 检查 1：所有路径都有返回
    if (returnFlow != ControlFlow::Returns && returnFlow != ControlFlow::Diverges) {
        ReportError("Function '" + function.name + 
                   "' has return type but not all code paths return a value");
    }
    
    // 检查 2：返回类型一致性
    CheckReturnTypeConsistency(function);
}

void CheckReturnTypeConsistency(Function& function) {
    std::vector<std::shared_ptr<Type>> returnTypes;
    CollectReturnTypes(function.body, returnTypes);
    
    if (returnTypes.empty()) return;
    
    auto firstType = returnTypes[0];
    for (size_t i = 1; i < returnTypes.size(); ++i) {
        if (!AreTypesCompatible(firstType, returnTypes[i])) {
            ReportError("Inconsistent return types in function '" + function.name + 
                       "': expected '" + firstType->ToString() + 
                       "', found '" + returnTypes[i]->ToString() + "'");
        }
    }
}
```

**检查的错误类型**：
- `Function 'foo' has return type but not all code paths return a value`
- `Inconsistent return types: expected 'i32', found 'bool'`
- `Missing return statement in function`

#### 3. 循环结构检查

**检查内容**：
- 循环是否有退出条件
- 循环变量的使用情况
- 循环体的控制流特性

**实现方式**：
```cpp
void CheckLoopStructure(LoopExpression& loop) {
    // 检查 1：无限循环检测
    if (IsInfiniteLoop(loop)) {
        ReportWarning("Infinite loop detected - loop has no break condition");
    }
    
    // 检查 2：循环体控制流
    auto bodyFlow = AnalyzeBlockControlFlow(*loop.body);
    
    if (bodyFlow == ControlFlow::Diverges) {
        ReportError("Loop body always diverges - loop will never execute normally");
    }
    
    // 检查 3：循环变量使用
    CheckLoopVariableUsage(loop);
    
    // 检查 4：break 值类型
    CheckLoopBreakTypes(loop);
}

void CheckLoopVariableUsage(LoopExpression& loop) {
    auto loopVariables = ExtractLoopVariables(loop);
    
    for (const auto& var : loopVariables) {
        if (!IsVariableUsedInLoop(var, loop)) {
            ReportWarning("Loop variable '" + var + "' is never used");
        }
    }
}
```

**检查的错误类型**：
- `Infinite loop detected - loop has no break condition`
- `Loop body always diverges - loop will never execute normally`
- `Loop variable 'i' is never used`

#### 4. 条件表达式检查

**检查内容**：
- if 条件必须是布尔类型
- 分支表达式的类型兼容性
- 条件表达式的常量性

**实现方式**：
```cpp
void CheckIfExpression(IfExpression& ifExpr) {
    // 检查 1：条件类型
    auto conditionType = InferExpressionType(*ifExpr.condition);
    if (!IsBoolType(conditionType)) {
        ReportError("If condition must be bool type, found '" + conditionType->ToString() + "'");
    }
    
    // 检查 2：条件常量
    if (IsCompileTimeConstant(*ifExpr.condition)) {
        auto constValue = EvaluateConstantExpression(*ifExpr.condition);
        if (auto boolConst = dynamic_cast<BoolConstant*>(constValue.get())) {
            if (boolConst->GetValue()) {
                ReportWarning("If condition is always true");
            } else {
                ReportWarning("If condition is always false");
            }
        }
    }
    
    // 检查 3：分支类型兼容性
    if (ifExpr.elseBranch) {
        auto thenType = InferExpressionType(*ifExpr.thenBranch);
        auto elseType = InferExpressionType(*ifExpr.elseBranch);
        
        if (!AreTypesCompatible(thenType, elseType)) {
            ReportError("If branches have incompatible types: '" + 
                       thenType->ToString() + "' vs '" + elseType->ToString() + "'");
        }
    }
}
```

**检查的错误类型**：
- `If condition must be bool type, found 'i32'`
- `If condition is always true`
- `If branches have incompatible types: 'i32' vs 'bool'`

#### 5. Never 类型检查

**检查内容**：
- Never 类型的正确使用
- 发散表达式的上下文适宜性
- Never 类型转换的有效性

**实现方式**：
```cpp
void CheckNeverTypeUsage(Expression& expr) {
    auto exprType = InferExpressionType(expr);
    
    if (!IsNeverType(exprType)) {
        return;
    }
    
    // 检查 1：Never 类型使用的上下文
    auto context = GetExpressionContext(&expr);
    if (!IsValidNeverContext(context)) {
        ReportError("Never type expression not allowed in this context");
    }
    
    // 检查 2：发散后的代码可达性
    CheckCodeAfterDivergence(&expr);
}

void CheckCodeAfterDivergence(Expression* divergentExpr) {
    auto nextStatements = GetNextStatements(divergentExpr);
    
    for (const auto& stmt : nextStatements) {
        if (IsReachable(stmt)) {
            ReportWarning("Code after divergent expression is unreachable");
            break;
        }
    }
}

bool IsValidNeverContext(ExpressionContext context) {
    switch (context) {
        case ExpressionContext::FunctionReturn:
        case ExpressionContext::LoopBreak:
        case ExpressionContext::MatchArm:
            return true;
        case ExpressionContext::Assignment:
        case ExpressionContext::BinaryOperation:
        case ExpressionContext::FunctionArgument:
            return false;
        default:
            return false;
    }
}
```

**检查的错误类型**：
- `Never type expression not allowed in this context`
- `Code after divergent expression is unreachable`
- `Cannot use divergent function as value`

#### 6. 不可达代码检查

**检查内容**：
- 识别永远不会执行的代码
- 检查死代码的原因
- 提供代码优化建议

**实现方式**：
```cpp
void CheckUnreachableCode(BlockExpression& block) {
    bool hasUnreachable = false;
    
    for (size_t i = 0; i < block.statements.size(); ++i) {
        auto stmt = block.statements[i].get();
        
        if (hasUnreachable) {
            ReportWarning("Unreachable code detected at statement " + std::to_string(i + 1));
            continue;
        }
        
        auto stmtFlow = AnalyzeStatementControlFlow(*stmt);
        
        if (stmtFlow == ControlFlow::Returns || 
            stmtFlow == ControlFlow::Diverges || 
            stmtFlow == ControlFlow::Breaks) {
            hasUnreachable = true;
        }
    }
}

void CheckUnreachablePattern(MatchExpression& matchExpr) {
    for (size_t i = 0; i < matchExpr.arms.size(); ++i) {
        auto arm = matchExpr.arms[i].get();
        
        if (IsArmUnreachable(*arm, i)) {
            ReportWarning("Match arm " + std::to_string(i + 1) + " is unreachable");
        }
    }
}
```

**检查的错误类型**：
- `Unreachable code detected at statement 5`
- `Match arm 3 is unreachable`
- `Code after return statement is unreachable`

#### 7. 控制流一致性检查

**检查内容**：
- 控制流分析的完整性
- 控制流状态的一致性
- 控制流信息的正确传播

**实现方式**：
```cpp
void CheckControlFlowConsistency(Function& function) {
    // 检查 1：控制流状态一致性
    auto entryFlow = AnalyzeEntryControlFlow(function);
    auto exitFlow = AnalyzeExitControlFlow(function);
    
    if (!IsFlowConsistent(entryFlow, exitFlow)) {
        ReportError("Inconsistent control flow analysis in function '" + function.name + "'");
    }
    
    // 检查 2：控制流信息传播
    CheckControlFlowPropagation(function);
    
    // 检查 3：循环嵌套一致性
    CheckLoopNestingConsistency(function);
}

void CheckLoopNestingConsistency(Function& function) {
    auto loopInfo = ExtractLoopNestingInfo(function);
    
    for (const auto& loop : loopInfo) {
        if (loop.depth > MAX_LOOP_NESTING_DEPTH) {
            ReportWarning("Deep loop nesting detected (depth " + 
                         std::to_string(loop.depth) + ")");
        }
        
        CheckLoopStructureConsistency(loop);
    }
}
```

**检查的错误类型**：
- `Inconsistent control flow analysis in function 'foo'`
- `Deep loop nesting detected (depth 10)`
- `Loop nesting structure is inconsistent`

### 错误恢复策略

#### 1. 控制流状态恢复
- 遇到错误时使用默认控制流状态
- 继续分析后续代码
- 保持控制流图的完整性

#### 2. 类型推断恢复
- 控制流相关类型推断失败时使用默认类型
- 保持类型信息的连续性
- 避免级联错误

#### 3. 分析结果修正
- 检测到不一致时修正分析结果
- 使用启发式方法修复控制流图
- 提供警告而不是错误

### 检查性能优化

#### 1. 控制流分析缓存
- 基本块控制流信息缓存
- 函数级控制流摘要缓存
- 循环分析结果缓存

#### 2. 增量分析
- 只重新分析变化的代码段
- 增量更新控制流图
- 智能失效策略

#### 3. 早期退出优化
- 检测到严重错误时提前退出
- 跳过无关的检查项
- 优化分析顺序

通过这些详细的语法检查机制，ControlFlowAnalyzer 确保了 Rx 语言程序控制流的正确性和合理性，为后续的类型检查和代码生成阶段提供了可靠的控制流信息基础。
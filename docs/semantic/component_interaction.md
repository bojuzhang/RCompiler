# Rx 语言编译器 Semantic 组件交互接口分析

## 概述

Rx 语言编译器的语义分析阶段由四个主要组件组成，这些组件之间通过明确定义的接口进行交互，形成了一个完整的信息流和处理流水线。本文档详细分析各组件之间的交互接口、数据流转和协作机制。

## 组件架构概览

### 组件层次结构

```
CompleteSemanticAnalyzer
├── SymbolCollector
├── ConstantEvaluator
├── ControlFlowAnalyzer
└── TypeChecker
```

### 数据流向图

```
AST
  ↓
SymbolCollector → ScopeTree
  ↓
ConstantEvaluator → ScopeTree + ConstantValues
  ↓
ControlFlowAnalyzer → ScopeTree + ConstantValues + ControlFlowInfo
  ↓
TypeChecker → ScopeTree + ConstantValues + ControlFlowInfo + TypeMap
  ↓
Code Generation
```

## 核心交互接口

### 1. CompleteSemanticAnalyzer 协调器

#### 接口定义
```cpp
class CompleteSemanticAnalyzer {
private:
    std::shared_ptr<Crate> ast;                          // 输入：抽象语法树
    std::shared_ptr<ScopeTree> scopeTree;                  // 中间数据：作用域树
    std::shared_ptr<ConstantEvaluator> constantEvaluator;   // 中间组件：常量求值器
    std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer; // 中间组件：控制流分析器
    std::shared_ptr<TypeChecker> typeChecker;            // 中间组件：类型检查器
    
    bool hasErrors = false;                               // 状态：整体错误标志

public:
    CompleteSemanticAnalyzer(std::shared_ptr<Crate> ast);
    bool Analyze();                                      // 主入口：执行完整语义分析
    bool HasAnalysisErrors() const;                       // 状态查询：是否有错误
};
```

#### 协调机制
```cpp
bool CompleteSemanticAnalyzer::Analyze() {
    hasErrors = false;
    
    // 顺序执行四个阶段，每个阶段都可能设置 hasErrors
    if (!RunSymbolCollection()) {
        hasErrors = true;
        return false;
    }
    
    if (!RunConstantEvaluation()) {
        hasErrors = true;
        return false;
    }
    
    if (!RunControlFlowAnalysis()) {
        hasErrors = true;
        return false;
    }
    
    if (!RunTypeChecking()) {
        hasErrors = true;
        return false;
    }
    
    return !hasErrors;
}
```

### 2. SymbolCollection 输出接口

#### 主要输出
```cpp
class SymbolCollector : public ASTVisitor {
public:
    std::shared_ptr<ScopeTree> getScopeTree() { return root; }
    bool HasErrors() const { return hasError; }
};
```

#### 交互方式
- **输出**：`ScopeTree` - 完整的符号信息和作用域结构
- **使用方**：`ConstantEvaluator`, `ControlFlowAnalyzer`, `TypeChecker`
- **访问方式**：通过 `getScopeTree()` 方法获取

#### 关键数据结构
```cpp
// ScopeTree 提供的主要接口
class ScopeTree {
public:
    std::shared_ptr<Scope> GetCurrentScope();
    std::shared_ptr<Scope> GetRootScope();
    bool InsertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol, bool allowOverride = false);
    std::shared_ptr<Symbol> LookupSymbol(const std::string& name);
    void EnterScope(Scope::ScopeType type, ASTNode* node);
    void EnterExistingScope(ASTNode* node);
    void ExitScope();
};
```

### 3. ConstantEvaluator 交互接口

#### 输入依赖
```cpp
class ConstantEvaluator : public ASTVisitor {
public:
    ConstantEvaluator(std::shared_ptr<ScopeTree> scopeTree);  // 依赖：作用域树
    std::shared_ptr<ConstantValue> GetConstantValue(const std::string& name); // 输出：常量值查询
    bool HasEvaluationErrors() const;                           // 状态：求值错误
};
```

#### 与其他组件的交互
- **输入**：从 `SymbolCollector` 获取的 `ScopeTree`
- **输出**：常量值映射，供 `TypeChecker` 使用
- **使用方式**：`TypeChecker` 通过 `GetConstantValue()` 查询常量值

#### 关键数据结构
```cpp
// 常量值类型层次
class ConstantValue {
public:
    virtual ~ConstantValue() = default;
    virtual std::string toString() const = 0;
};

class IntConstant : public ConstantValue {
private:
    int64_t value;
public:
    IntConstant(int64_t value) : value(value) {}
    int64_t getValue() const { return value; }
    std::string toString() const override { return std::to_string(value); }
};

// 其他常量类型：BoolConstant, CharConstant, StringConstant, ArrayConstant
```

### 4. ControlFlowAnalyzer 交互接口

#### 输入依赖
```cpp
class ControlFlowAnalyzer : public ASTVisitor {
public:
    ControlFlowAnalyzer(std::shared_ptr<ScopeTree> scopeTree, 
                       std::shared_ptr<ConstantEvaluator> constantEvaluator);
    
    // 输出接口
    ControlFlow GetControlFlow(ASTNode* node) const;
    std::shared_ptr<SemanticType> GetNodeType(ASTNode* node) const;
    bool AlwaysDivergesAt(ASTNode* node) const;
    bool HasAnalysisErrors() const;
};
```

#### 与其他组件的交互
- **输入**：`ScopeTree` 和 `ConstantEvaluator`
- **输出**：控制流信息和类型信息，供 `TypeChecker` 使用
- **使用方式**：`TypeChecker` 通过查询方法获取控制流和类型信息

#### 关键数据结构
```cpp
// 控制流类型
enum class ControlFlow {
    Continues,    // 正常继续
    Breaks,       // 包含break
    ContinuesLoop,// 包含continue  
    Returns,      // 包含return
    Diverges      // 发散（never类型）
};

// Never 类型
class NeverType : public SemanticType {
public:
    std::string tostring() const override { return "!"; }
};
```

### 5. TypeChecker 交互接口

#### 输入依赖
```cpp
class TypeChecker : public ASTVisitor {
public:
    TypeChecker(std::shared_ptr<ScopeTree> scopeTree, 
               std::shared_ptr<ConstantEvaluator> constantEvaluator = nullptr);
    
    // 输出接口
    bool HasTypeErrors() const;
    
    // 内部数据访问（供代码生成使用）
    const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& GetNodeTypeMap() const;
};
```

#### 与其他组件的交互
- **输入**：`ScopeTree`, `ConstantEvaluator`, 以及从 `ControlFlowAnalyzer` 获取的信息
- **输出**：完整的类型信息，供代码生成阶段使用
- **使用方式**：代码生成阶段通过 `GetNodeTypeMap()` 获取类型信息

## 详细交互流程

### 1. 符号收集 → 常量求值

#### 数据传递
```cpp
// 在 CompleteSemanticAnalyzer 中
bool CompleteSemanticAnalyzer::RunConstantEvaluation() {
    constantEvaluator = std::make_shared<ConstantEvaluator>(scopeTree);
    ast->accept(*constantEvaluator);
    return !constantEvaluator->HasEvaluationErrors();
}
```

#### 交互内容
- **ScopeTree 传递**：符号收集建立的完整作用域树
- **符号查找**：常量求值器通过作用域树查找符号信息
- **类型信息**：利用符号收集阶段的类型定义

#### 具体使用示例
```cpp
// ConstantEvaluator 中使用 ScopeTree
std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluatePathExpression(PathExpression& expr) {
    std::string constName = expr.simplepath->simplepathsegements[0]->identifier;
    
    // 尝试从常量值存储中查找
    auto it = constantValues.find(constName);
    if (it != constantValues.end()) {
        return it->second;
    }
    
    // 从作用域树中查找符号
    if (scopeTree) {
        auto symbol = scopeTree->LookupSymbol(constName);
        if (symbol && symbol->kind == SymbolKind::Constant) {
            ReportError("Constant '" + constName + "' found but not evaluated yet");
            return nullptr;
        }
    }
    
    return nullptr;
}
```

### 2. 常量求值 → 控制流分析

#### 数据传递
```cpp
bool CompleteSemanticAnalyzer::RunControlFlowAnalysis() {
    controlFlowAnalyzer = std::make_shared<ControlFlowAnalyzer>(scopeTree, constantEvaluator);
    ast->accept(*controlFlowAnalyzer);
    return !controlFlowAnalyzer->HasAnalysisErrors();
}
```

#### 交互内容
- **双重输入**：同时接收 `ScopeTree` 和 `ConstantEvaluator`
- **常量查询**：控制流分析器可以查询编译时常量值
- **符号信息**：利用作用域树进行符号查找

#### 具体使用示例
```cpp
// ControlFlowAnalyzer 中使用 ConstantEvaluator
void ControlFlowAnalyzer::visit(CallExpression& node) {
    // 检查是否是 exit 函数调用
    if (auto pathExpr = dynamic_cast<PathExpression*>(node.expression.get())) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string functionName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            if (functionName == "exit") {
                // exit 函数发散
                nodeControlFlow[&node] = ControlFlow::Diverges;
                nodeTypes[&node] = std::make_shared<NeverType>();
                alwaysDiverges[&node] = true;
                return;
            }
        }
    }
}
```

### 3. 控制流分析 → 类型检查

#### 数据传递
```cpp
bool CompleteSemanticAnalyzer::RunTypeChecking() {
    typeChecker = std::make_shared<TypeChecker>(scopeTree, constantEvaluator);
    ast->accept(*typeChecker);
    return !typeChecker->HasTypeErrors();
}
```

#### 交互内容
- **主要输入**：`ScopeTree` 和 `ConstantEvaluator`
- **间接输入**：控制流分析的信息通过类型检查器的内部查询获取
- **信息整合**：类型检查器整合所有前面阶段的信息

#### 具体使用示例
```cpp
// TypeChecker 中使用 ConstantEvaluator
std::shared_ptr<SemanticType> TypeChecker::InferExpressionType(Expression& expr) {
    if (auto pathExpr = dynamic_cast<PathExpression*>(&expr)) {
        std::string varName = pathExpr->simplepath->simplepathsegements[0]->identifier;
        
        auto symbol = FindSymbol(varName);
        if (symbol && symbol->type) {
            return symbol->type;
        } else {
            // 如果是常量，从常量求值器获取类型
            if (constantEvaluator) {
                auto constValue = constantEvaluator->GetConstantValue(varName);
                if (constValue) {
                    if (dynamic_cast<IntConstant*>(constValue.get())) {
                        return std::make_shared<SimpleType>("usize");
                    } else if (dynamic_cast<BoolConstant*>(constValue.get())) {
                        return std::make_shared<SimpleType>("bool");
                    }
                }
            }
        }
    }
    
    return nullptr;
}
```

## 接口设计原则

### 1. 单向数据流

**原则**：数据从早期阶段流向后期阶段，避免循环依赖

**实现**：
- SymbolCollection → ConstantEvaluator
- ConstantEvaluator → ControlFlowAnalyzer  
- ControlFlowAnalyzer → TypeChecker

### 2. 信息积累

**目标**：后续阶段可以访问前面阶段的所有信息

**实现**：
- 每个阶段都将结果存储在共享数据结构中
- 后续阶段通过查询接口访问前面阶段的结果
- 避免信息丢失和重复计算

### 3. 接口稳定性

**要求**：接口设计保持稳定，支持内部实现的变更

**实现**：
- 使用抽象接口定义组件间的交互
- 具体实现细节封装在组件内部
- 通过标准化的方法名和参数进行交互

### 4. 错误传播

**机制**：每个阶段的错误都能被协调器检测和处理

**实现**：
- 每个组件提供错误状态查询接口
- 协调器检查每个阶段的错误状态
- 任何阶段的错误都会影响整体分析结果

## 关键交互场景

### 1. 函数调用类型检查

#### 涉及组件
- **SymbolCollection**：提供函数符号信息
- **ConstantEvaluator**：提供常量值信息
- **TypeChecker**：进行类型检查和推断

#### 交互流程
```cpp
// 1. SymbolCollection 收集函数符号
void SymbolCollector::CollectFunctionSymbol(Function& node) {
    auto funcSymbol = std::make_shared<FunctionSymbol>(funcName, params, returnType, false);
    root->InsertSymbol(funcName, funcSymbol);
}

// 2. TypeChecker 检查函数调用
void TypeChecker::visit(CallExpression& node) {
    auto functionSymbol = FindFunction(functionName);
    if (functionSymbol) {
        // 检查参数数量
        if (functionSymbol->parameters.size() != args.size()) {
            ReportError("Parameter count mismatch");
        }
        
        // 检查参数类型
        for (size_t i = 0; i < args.size(); ++i) {
            auto paramType = functionSymbol->parameters[i]->type;
            auto argType = InferExpressionType(*args[i]);
            if (!AreTypesCompatible(paramType, argType)) {
                ReportError("Parameter type mismatch");
            }
        }
    }
}
```

### 2. 常量表达式求值

#### 涉及组件
- **SymbolCollection**：提供常量符号信息
- **ConstantEvaluator**：进行编译时求值
- **TypeChecker**：使用求值结果

#### 交互流程
```cpp
// 1. SymbolCollection 收集常量符号
void SymbolCollector::CollectConstantSymbol(ConstantItem& node) {
    auto constSymbol = std::make_shared<ConstantSymbol>(constName, type);
    root->InsertSymbol(constName, constSymbol);
}

// 2. ConstantEvaluator 求值常量表达式
void ConstantEvaluator::visit(ConstantItem& node) {
    auto value = EvaluateExpression(*node.expression);
    if (value) {
        constantValues[constName] = value;
    }
}

// 3. TypeChecker 使用常量值
std::shared_ptr<SemanticType> TypeChecker::InferExpressionType(Expression& expr) {
    if (auto pathExpr = dynamic_cast<PathExpression*>(&expr)) {
        auto constValue = constantEvaluator->GetConstantValue(varName);
        if (constValue) {
            // 根据常量值推断类型
            if (dynamic_cast<IntConstant*>(constValue.get())) {
                return std::make_shared<SimpleType>("usize");
            }
        }
    }
}
```

### 3. 控制流相关类型推断

#### 涉及组件
- **ControlFlowAnalyzer**：分析控制流特性
- **TypeChecker**：使用控制流信息进行类型推断

#### 交互流程
```cpp
// 1. ControlFlowAnalyzer 分析 loop 表达式
void ControlFlowAnalyzer::visit(InfiniteLoopExpression& node) {
    ControlFlow bodyFlow = GetControlFlow(node.blockexpression.get());
    if (bodyFlow != ControlFlow::Breaks) {
        // 循环体没有break，整个循环发散
        nodeControlFlow[&node] = ControlFlow::Diverges;
        nodeTypes[&node] = std::make_shared<NeverType>();
    }
}

// 2. TypeChecker 使用控制流信息
std::shared_ptr<SemanticType> TypeChecker::InferExpressionType(Expression& expr) {
    if (auto loopExpr = dynamic_cast<InfiniteLoopExpression*>(&expr)) {
        // 可以从控制流分析器获取类型信息
        // 或者重新进行分析
        return InferInfiniteLoopExpressionType(*loopExpr);
    }
}
```

## 性能优化考虑

### 1. 避免重复遍历

**策略**：每个阶段在一次 AST 遍历中完成所有工作

**实现**：
- 使用 Visitor 模式进行高效遍历
- 在遍历过程中同时进行多种分析
- 避免多次遍历同一 AST 结构

### 2. 缓存查询结果

**目的**：减少重复的符号查找和类型推断

**实现**：
- TypeChecker 中的类型缓存机制
- 常量求值结果的缓存
- 控制流信息的缓存

### 3. 延迟计算

**策略**：只在需要时进行昂贵的计算

**实现**：
- 类型推断的延迟执行
- 控制流分析的按需进行
- 常量求值的上下文感知

## 扩展性设计

### 1. 组件独立性

**目标**：每个组件可以独立开发和测试

**实现**：
- 明确的接口定义
- 最小化组件间的直接依赖
- 标准化的数据交换格式

### 2. 新组件集成

**考虑**：支持未来添加新的语义分析组件

**实现**：
- 插件化的组件架构
- 标准化的组件接口
- 灵活的数据流配置

### 3. 接口版本兼容

**要求**：接口变更保持向后兼容

**实现**：
- 接口版本管理
- 适配器模式支持旧接口
- 渐进式的接口升级

## 总结

Rx 语言编译器的语义分析组件通过精心设计的交互接口，实现了高效的信息流转和处理协作。每个组件都有明确的职责和标准化的接口，通过单向的数据流避免了循环依赖，通过信息积累确保了后续阶段能够访问完整的语义信息。

这种设计既保证了系统的模块化和可维护性，又通过优化的交互机制确保了良好的性能表现，为 Rx 语言编译器的语义分析提供了坚实的技术基础。
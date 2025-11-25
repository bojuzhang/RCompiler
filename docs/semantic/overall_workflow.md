# Rx 语言编译器 Semantic 分析整体工作逻辑

## 概述

Rx 语言编译器的语义分析阶段是一个四阶段的流水线处理过程，每个阶段都有明确的职责和输入输出关系。整个语义分析过程从抽象语法树（AST）开始，经过符号收集、常量求值、控制流分析和类型检查，最终为代码生成阶段准备完整的语义信息。

## 整体架构

### 处理流水线

```
AST → SymbolCollection → ConstantEvaluator → ControlFlowAnalyzer → TypeChecker → 代码生成
```

### 核心数据结构

1. **AST（抽象语法树）** - 语义分析的输入，由语法分析器生成
2. **ScopeTree（作用域树）** - 符号收集阶段建立的层次化作用域结构
3. **ConstantValue（常量值）** - 常量求值阶段存储的编译时常量
4. **ControlFlow（控制流信息）** - 控制流分析阶段生成的控制流和类型信息
5. **NodeTypeMap（节点类型映射）** - 类型检查阶段建立的节点到类型的映射关系

## 详细工作流程

### 第一阶段：SymbolCollection（符号收集）

#### 启动时机
- 语义分析的开始阶段
- 接收完整的 AST 作为输入

#### 主要任务
1. **内置符号注册** - 注册内置类型和函数
2. **符号收集** - 遍历 AST，收集所有用户定义的符号
3. **作用域建立** - 建立层次化的作用域树结构
4. **类型信息收集** - 收集类型定义和函数签名

#### 处理策略
- **两遍扫描**：第一遍收集类型和函数符号，第二遍处理 impl 块
- **层次化作用域**：建立全局作用域、函数作用域、块作用域、impl 作用域
- **符号分类管理**：不同类型的符号分别处理和管理

#### 输出结果
```cpp
std::shared_ptr<ScopeTree> scopeTree;  // 完整的作用域树结构
bool hasErrors;                      // 符号收集错误标志
```

#### 关键代码示例
```cpp
// CompleteSemanticAnalyzer 中的调用
bool CompleteSemanticAnalyzer::RunSymbolCollection() {
    std::cerr << "Step 1: Symbol Collection" << std::endl;
    
    SymbolCollector collector;
    collector.BeginCollection();
    ast->accept(collector);
    
    scopeTree = collector.getScopeTree();
    
    if (collector.HasErrors()) {
        std::cerr << "Symbol collection failed with errors" << std::endl;
        return false;
    }
    
    return true;
}
```

### 第二阶段：ConstantEvaluator（常量求值）

#### 启动时机
- 符号收集阶段完成后
- 接收符号收集阶段建立的作用域树

#### 主要任务
1. **常量表达式识别** - 识别可以在编译时求值的表达式
2. **编译时计算** - 对常量表达式进行求值
3. **常量值存储** - 将求值结果存储为常量值对象
4. **错误检测** - 检测常量表达式中的错误

#### 处理策略
- **上下文感知**：根据上下文决定是否进行求值
- **类型安全**：使用继承体系确保类型安全的常量值
- **递归求值**：支持复杂的常量表达式求值

#### 输出结果
```cpp
std::unordered_map<std::string, std::shared_ptr<ConstantValue>> constantValues; // 常量值存储
bool hasEvaluationErrors;                                                 // 求值错误标志
```

#### 关键代码示例
```cpp
// CompleteSemanticAnalyzer 中的调用
bool CompleteSemanticAnalyzer::RunConstantEvaluation() {
    std::cerr << "Step 2: Constant Evaluation" << std::endl;
    
    constantEvaluator = std::make_shared<ConstantEvaluator>(scopeTree);
    ast->accept(*constantEvaluator);
    
    return !constantEvaluator->HasEvaluationErrors();
}
```

### 第三阶段：ControlFlowAnalyzer（控制流分析）

#### 启动时机
- 常量求值阶段完成后
- 接收符号收集阶段的作用域树和常量求值阶段的常量值

#### 主要任务
1. **控制流分析** - 分析程序的控制流路径和特性
2. **发散性检测** - 识别永远不会返回的表达式
3. **循环结构分析** - 分析 loop、while 等循环的控制流
4. **类型推断** - 推断基于控制流的表达式类型
5. **有效性检查** - 检查 break、continue 的使用有效性

#### 处理策略
- **状态驱动**：使用循环深度栈和控制流栈跟踪复杂控制流
- **Never 类型支持**：完整支持 never 类型系统
- **Rx 语言特性**：针对 Rx 语言的特殊设计

#### 输出结果
```cpp
std::unordered_map<ASTNode*, ControlFlow> nodeControlFlow;          // 节点控制流映射
std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypes; // 节点类型映射
std::unordered_map<ASTNode*, bool> alwaysDiverges;                   // 节点发散性映射
```

#### 关键代码示例
```cpp
// CompleteSemanticAnalyzer 中的调用
bool CompleteSemanticAnalyzer::RunControlFlowAnalysis() {
    std::cerr << "Step 3: Control Flow Analysis" << std::endl;
    
    controlFlowAnalyzer = std::make_shared<ControlFlowAnalyzer>(scopeTree, constantEvaluator);
    ast->accept(*controlFlowAnalyzer);
    
    return !controlFlowAnalyzer->HasAnalysisErrors();
}
```

### 第四阶段：TypeChecker（类型检查）

#### 启动时机
- 控制流分析阶段完成后
- 接收前面阶段建立的所有语义信息

#### 主要任务
1. **类型检查** - 验证表达式和语句的类型正确性
2. **类型推断** - 推断表达式的类型
3. **类型兼容性检查** - 检查类型之间的兼容性
4. **可变性检查** - 验证变量赋值和修改的可变性规则
5. **函数调用检查** - 验证函数调用的参数类型和数量
6. **方法调用检查** - 验证方法调用和 self 参数
7. **数组类型检查** - 验证数组类型和大小匹配

#### 处理策略
- **分层检查**：首先进行基础类型推断，然后检查兼容性，最后验证可变性
- **缓存机制**：使用类型缓存避免重复推断
- **引用类型支持**：完整支持引用类型系统
- **隐式转换**：实现 Rx 语言的隐式类型转换规则

#### 输出结果
```cpp
std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap; // 节点类型映射
bool hasTypeErrors;                                                    // 类型错误标志
```

#### 关键代码示例
```cpp
// CompleteSemanticAnalyzer 中的调用
bool CompleteSemanticAnalyzer::RunTypeChecking() {
    std::cerr << "Step 4: Type Checking" << std::endl;
    
    typeChecker = std::make_shared<TypeChecker>(scopeTree, constantEvaluator);
    ast->accept(*typeChecker);
    
    return !typeChecker->HasTypeErrors();
}
```

## 阶段间的数据流转

### 数据依赖关系

```
SymbolCollection → ScopeTree
     ↓
ConstantEvaluator → ScopeTree + ConstantValues
     ↓
ControlFlowAnalyzer → ScopeTree + ConstantValues + ControlFlowInfo
     ↓
TypeChecker → ScopeTree + ConstantValues + ControlFlowInfo + TypeMap
```

### 信息传递机制

1. **ScopeTree 传递** - 符号收集阶段建立的作用域树被所有后续阶段使用
2. **ConstantValues 传递** - 常量求值结果被控制流分析和类型检查使用
3. **ControlFlowInfo 传递** - 控制流信息被类型检查使用
4. **TypeMap 传递** - 类型检查结果被代码生成阶段使用

### 错误传播机制

每个阶段都有独立的错误检测和报告机制：

```cpp
// 每个阶段的错误检查
if (collector.HasErrors()) {
    hasErrors = true;
    return false;
}

if (constantEvaluator->HasEvaluationErrors()) {
    hasErrors = true;
    return false;
}

if (controlFlowAnalyzer->HasAnalysisErrors()) {
    hasErrors = true;
    return false;
}

if (typeChecker->HasTypeErrors()) {
    hasErrors = true;
    return false;
}
```

## 关键设计决策

### 1. 阶段性处理

**原因**：避免循环依赖，确保信息收集的完整性

**实现**：
- 符号收集阶段不依赖其他语义信息
- 常量求值阶段依赖符号收集的结果
- 控制流分析阶段依赖前面两个阶段的结果
- 类型检查阶段利用前面所有阶段的信息

### 2. 信息积累策略

**原则**：后续阶段可以利用前面阶段收集的所有信息

**实现**：
- 每个阶段都将结果存储在共享的数据结构中
- 后续阶段可以查询前面阶段的结果
- 避免重复计算和分析

### 3. 错误早期检测

**目标**：尽可能早地发现错误，避免无效的后续处理

**实现**：
- 每个阶段都有独立的错误检测
- 如果某个阶段失败，后续阶段可能被跳过
- 详细的错误报告帮助定位问题

### 4. 类型系统完整性

**要求**：类型检查阶段必须能够访问完整的类型信息

**实现**：
- 符号收集阶段收集所有类型定义
- 常量求值阶段提供常量类型信息
- 控制流分析阶段提供基于控制流的类型推断
- 类型检查阶段整合所有信息进行最终验证

## 性能考虑

### 1. 单次遍历优化

**策略**：每个阶段尽量在一次 AST 遍历中完成所有工作

**实现**：
- 使用 Visitor 模式进行遍历
- 在遍历过程中同时进行多种分析
- 避免多次遍历同一 AST

### 2. 缓存机制

**目的**：避免重复计算，提高性能

**实现**：
- 类型检查阶段使用类型缓存
- 常量求值阶段缓存常量值
- 控制流分析阶段缓存控制流信息

### 3. 增量处理

**考虑**：支持可能的增量编译需求

**实现**：
- 每个阶段的独立性支持增量处理
- 数据结构设计支持部分更新
- 错误恢复机制支持继续处理

## Rx 语言特性支持

### 1. 面向对象特性

**SymbolCollection 阶段**：
- 支持 impl 块和 Self 类型别名
- 收集方法信息和关联函数
- 处理 self 参数的不同形式

**TypeChecker 阶段**：
- 方法调用的类型检查
- self 参数的类型推断
- 可变性检查

### 2. 类型系统特性

**常量求值阶段**：
- 编译时常量表达式求值
- 数组常量的支持
- 类型安全的常量值

**类型检查阶段**：
- 隐式类型转换规则
- 引用类型的完整支持
- 数组类型的类型检查

### 3. 控制流特性

**控制流分析阶段**：
- loop 表达式的 break 值支持
- if 表达式的类型推断规则
- never 类型的完整支持

**类型检查阶段**：
- 基于 never 类型的类型检查
- 发散表达式的处理
- 控制流相关的类型推断

## 错误处理策略

### 1. 分层错误处理

**原则**：每个阶段负责检测自己能够发现的错误

**实现**：
- 符号收集阶段：符号重定义、类型不存在等错误
- 常量求值阶段：常量表达式错误、除零错误等
- 控制流分析阶段：break/continue 位置错误等
- 类型检查阶段：类型不匹配、可变性错误等

### 2. 错误恢复机制

**目标**：单个错误不影响其他错误的检测

**实现**：
- 每个阶段设置错误标志但不立即终止
- 继续处理其他部分以发现更多错误
- 收集所有错误后统一报告

### 3. 错误信息质量

**要求**：提供详细、准确的错误信息

**实现**：
- 包含错误位置信息
- 提供错误的具体描述
- 给出可能的修复建议

## 总结

Rx 语言编译器的语义分析阶段通过四个明确的阶段，完整地处理了从符号收集到类型检查的所有语义分析任务。每个阶段都有明确的职责和输入输出关系，通过信息积累和错误早期检测的策略，确保了语义分析的完整性和准确性。

这种设计既保证了各阶段的独立性，又通过数据流转机制实现了信息的有效传递，为后续的代码生成阶段提供了完整的语义信息基础。
# IRBuilder 组件设计文档

## 概述

IRBuilder 是 Rx 语言编译器 IR 生成模块的核心组件，负责生成符合 LLVM 15 语法的 IR 文本。IRBuilder 完全不依赖 LLVM C++ API，而是通过自定义的指令生成系统直接输出 LLVM IR 文本到 stdout。

## 设计目标

1. **完全独立**：不依赖任何 LLVM C++ API 或 IRBuilder
2. **直接输出**：生成 LLVM IR 文本并直接输出到 stdout
3. **类型安全**：确保生成的 IR 符合 LLVM 类型系统
4. **寄存器管理**：自动管理寄存器分配和命名
5. **基本块管理**：支持复杂控制流的基本块管理

## 核心架构

### 寄存器管理系统

IRBuilder 维护一个复杂的寄存器命名和作用域管理系统：

- **临时寄存器**：`_1`, `_2`, `_3`, ...
- **作用域感知的命名寄存器**：`var1_1`, `var1_2`, ...（处理变量遮蔽）
- **类型区分寄存器**：`var1_ptr`, `var1_val`, `var1_addr`（区分指针和值）
- **寄存器类型跟踪**：每个寄存器都有对应的 LLVM 类型信息和作用域信息

### 基本块管理系统

IRBuilder 支持层次化和唯一性的基本块管理：

- **全局唯一基本块**：`bb1`, `bb2`, `bb3`, ...（确保全局唯一性）
- **语义化基本块**：`if.then_1`, `if.else_1`, `loop.head_1`, `loop.body_1`（带语义前缀）
- **嵌套控制流基本块**：`if_1.then_1`, `if_1.else_1`, `if_2.then_1`（嵌套编号）
- **当前基本块上下文**：跟踪当前正在生成指令的基本块
- **基本块跳转**：支持条件和无条件跳转

### 指令生成系统

IRBuilder 提供完整的 LLVM IR 指令生成接口：

- **类型指令**：alloca, store, load, getelementptr
- **算术指令**：add, sub, mul, div, rem
- **比较指令**：icmp
- **逻辑指令**：and, or, xor
- **控制流指令**：br, call, ret
- **位运算指令**：shl, shr
- **内存指令**：memcpy, memset

## 接口设计

### 寄存器管理接口

```cpp
class IRBuilder {
public:
    // 构造函数：接收 ScopeTree 引用
    IRBuilder(std::shared_ptr<ScopeTree> scopeTree);
    
    // 生成新的临时寄存器（基于当前 Scope）
    std::string newRegister();
    
    // 为指定变量生成寄存器（基于当前 Scope）
    std::string newRegister(const std::string& variableName);
    
    // 为指定变量生成类型区分的寄存器
    std::string newRegister(const std::string& variableName, const std::string& suffix);
    
    // 手动同步 Scope（通常由外部调用）
    void syncWithScopeTree();
    
    // 获取当前 Scope 中的变量寄存器
    std::string getVariableRegister(const std::string& variableName);
    
    // 获取指定 Scope 中的变量寄存器
    std::string getVariableRegister(const std::string& variableName, std::shared_ptr<Scope> scope);
    
    // 通过符号获取寄存器
    std::string getSymbolRegister(std::shared_ptr<Symbol> symbol);
    
    // 获取寄存器类型
    std::string getRegisterType(const std::string& reg);
    
    // 设置寄存器类型
    void setRegisterType(const std::string& reg, const std::string& type);
    
    // 获取寄存器所属的 Scope
    std::shared_ptr<Scope> getRegisterScope(const std::string& reg);
    
    // 检查变量是否在当前 Scope 中
    bool isVariableInCurrentScope(const std::string& variableName);
    
    // 获取当前 Scope
    std::shared_ptr<Scope> getCurrentScope();
    
    // 清理 Scope 相关的寄存器（在 Scope 退出时调用）
    void cleanupScopeRegisters(std::shared_ptr<Scope> scope);
};
```

### 基本块管理接口

```cpp
class IRBuilder {
public:
    // 创建新的基本块（自动确保唯一性）
    std::string newBasicBlock(const std::string& prefix);
    
    // 创建带计数器的基本块（用于循环等）
    std::string newBasicBlock(const std::string& prefix, int counter);
    
    // 创建嵌套基本块（带父基本块信息）
    std::string newNestedBasicBlock(const std::string& prefix, const std::string& parentPrefix);
    
    // 设置当前基本块
    void setCurrentBasicBlock(const std::string& label);
    
    // 获取当前基本块
    std::string getCurrentBasicBlock();
    
    // 完成当前基本块（添加跳转指令）
    void finishCurrentBasicBlock();
    
    // 进入新的控制流上下文
    void enterControlFlowContext(const std::string& contextType);
    
    // 退出当前控制流上下文
    void exitControlFlowContext();
    
    // 获取当前控制流上下文
    std::string getCurrentControlFlowContext();
    
    // 检查基本块名称是否已存在
    bool isBasicBlockExists(const std::string& label);
};
```

### 指令生成接口

```cpp
class IRBuilder {
public:
    // 基础指令生成
    void emitInstruction(const std::string& instruction);
    void emitComment(const std::string& comment);
    
    // 类型相关指令
    void emitAlloca(const std::string& reg, const std::string& type, int align = 4);
    void emitStore(const std::string& value, const std::string& ptr, const std::string& type);
    void emitLoad(const std::string& result, const std::string& ptr, const std::string& type);
    void emitGetElementPtr(const std::string& result, const std::string& ptr, 
                           const std::vector<std::pair<std::string, std::string>>& indices);
    
    // 算术运算指令
    void emitAdd(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitSub(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitMul(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitDiv(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitRem(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    
    // 比较运算指令
    void emitIcmp(const std::string& result, const std::string& cond,
                 const std::string& left, const std::string& right, const std::string& type);
    
    // 逻辑运算指令
    void emitAnd(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitOr(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitXor(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    
    // 位运算指令
    void emitShl(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitShr(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    
    // 控制流指令
    void emitBr(const std::string& target);
    void emitCondBr(const std::string& condition, const std::string& thenTarget, const std::string& elseTarget);
    void emitRet(const std::string& value = "");
    void emitCall(const std::string& result, const std::string& funcName, 
                 const std::vector<std::string>& args, const std::string& returnType = "");
    
    // 内存操作指令
    void emitMemcpy(const std::string& dest, const std::string& src, const std::string& size);
    void emitMemset(const std::string& dest, const std::string& value, const std::string& size);
};
```

## 实现策略

### 输出管理

IRBuilder 直接输出到 stdout，使用以下策略：

1. **立即输出**：每条指令生成后立即输出到 stdout
2. **格式化输出**：确保生成的 IR 符合 LLVM 语法规范
3. **缩进管理**：根据基本块层次自动管理缩进
4. **注释支持**：支持生成调试注释

### 类型系统映射

IRBuilder 维护 Rx 类型到 LLVM 类型的映射：

```cpp
class TypeMapper {
public:
    static std::string mapRxTypeToLLVM(const std::string& rxType);
    static std::string getPointerElementType(const std::string& ptrType);
    static std::string getArrayElementType(const std::string& arrayType);
    static bool isPointerType(const std::string& type);
    static bool isIntegerType(const std::string& type);
    static int getTypeSize(const std::string& type);
};
```

### 寄存器生命周期管理

IRBuilder 实现复杂的作用域感知的寄存器生命周期管理：

#### 作用域层次结构
```cpp
struct ScopeInfo {
    int scopeLevel;                    // 作用域层级
    std::map<std::string, std::string> variableToRegister;  // 变量名到寄存器的映射
    std::set<std::string> registers;   // 当前作用域的寄存器集合
};

class IRBuilder {
private:
    std::vector<ScopeInfo> scopeStack;  // 作用域栈
    std::map<std::string, int> variableCounters;  // 变量名计数器（处理遮蔽）
    int currentScopeLevel;              // 当前作用域层级
};
```

#### 变量遮蔽处理
1. **变量计数器**：每个变量名维护一个计数器，处理同名变量
2. **作用域栈**：嵌套作用域的变量管理
3. **命名策略**：`变量名_作用域层级_计数器`，如 `x_1_1`, `x_2_1`
4. **查找策略**：优先查找当前作用域，然后向外查找

#### 类型区分处理
1. **指针寄存器**：`变量名_ptr`（存储指针）
2. **值寄存器**：`变量名_val`（存储值）
3. **地址寄存器**：`变量名_addr`（存储地址）
4. **临时寄存器**：`变量名_tmp`（临时计算）

#### 寄存器分配策略
1. **自动分配**：自动分配唯一名称
2. **类型跟踪**：记录每个寄存器的类型和作用域
3. **作用域管理**：支持嵌套作用域的寄存器管理
4. **寄存器复用**：在安全的情况下复用寄存器
5. **生命周期跟踪**：跟踪寄存器的创建和销毁

## 错误处理

### 指令验证

IRBuilder 在生成指令前进行验证：

1. **类型检查**：确保操作数类型匹配
2. **寄存器存在性**：确保引用的寄存器已定义
3. **基本块完整性**：确保基本块有正确的终止符
4. **函数签名匹配**：确保函数调用参数匹配

### 错误报告

IRBuilder 提供详细的错误信息：

1. **错误位置**：指出具体的指令位置
2. **错误类型**：说明错误的具体类型
3. **修复建议**：提供可能的修复方案
4. **继续处理**：在非致命错误时继续处理

## 性能优化

### 输出优化

1. **缓冲输出**：使用输出缓冲减少系统调用
2. **批量输出**：将相关指令批量输出
3. **字符串优化**：使用字符串池减少内存分配

### 寄存器优化

1. **寄存器复用**：复用不再使用的寄存器
2. **命名优化**：为频繁使用的寄存器提供有意义的名称
3. **类型缓存**：缓存类型映射结果

## 使用示例

### 基本使用

```cpp
// 获取语义分析阶段的 ScopeTree
auto scopeTree = semanticAnalyzer->getScopeTree();
IRBuilder builder(scopeTree);

// 创建基本块
std::string entry = builder.newBasicBlock("entry");
builder.setCurrentBasicBlock(entry);

// 同步到当前 Scope（通常由外部调用）
builder.syncWithScopeTree();

// 分配变量（基于当前 Scope 和符号信息）
auto currentScope = builder.getCurrentScope();
auto xSymbol = scopeTree->LookupSymbol("x");
if (xSymbol) {
    std::string xPtr = builder.newRegister("x", "ptr");  // x_ptr_2_1 (depth=2)
    builder.emitAlloca(xPtr, "i32", 4);
    
    // 存储值
    builder.emitStore("42", xPtr, "i32");
    
    // 加载值
    std::string xVal = builder.newRegister("x", "val");  // x_val_2_1
    builder.emitLoad(xVal, xPtr, "i32");
    
    // 返回
    builder.emitRet(xVal);
}
```

### 与 ScopeTree 同步的变量遮蔽处理

```cpp
// 假设在语义分析阶段已经建立了如下作用域结构：
// Global (depth=0): 变量 x
//   Function (depth=1): 参数 x (遮蔽全局 x)
//     Block (depth=2): 变量 x (遮蔽函数参数 x)

auto scopeTree = semanticAnalyzer->getScopeTree();
IRBuilder builder(scopeTree);

// 在 Block 作用域中（depth=2）
scopeTree->GoToNode(blockNode);
builder.syncWithScopeTree();

// 获取当前作用域的 x（Block 中的 x）
std::string blockX = builder.getVariableRegister("x");  // 返回 x_val_2_1

// 手动获取外层作用域的 x
auto functionScope = scopeTree->GetCurrentScope()->GetParent();
std::string funcX = builder.getVariableRegister("x", functionScope);  // 返回 x_val_1_1

auto globalScope = scopeTree->GetRootScope();
std::string globalX = builder.getVariableRegister("x", globalScope);  // 返回 x_val_0_1
```

### 基于符号的寄存器管理

```cpp
auto scopeTree = semanticAnalyzer->getScopeTree();
IRBuilder builder(scopeTree);

// 通过符号查找获取寄存器
auto symbol = scopeTree->LookupSymbol("myVar");
if (symbol) {
    std::string varReg = builder.getSymbolRegister(symbol);
    // 使用寄存器...
}

// 为新符号创建寄存器
auto newSymbol = std::make_shared<Symbol>("newVar", SymbolKind::Variable, type, false, node);
scopeTree->InsertSymbol("newVar", newSymbol);

std::string newReg = builder.newRegister("newVar");  // 基于新符号创建寄存器
```

### 类型区分处理（基于 Scope）

```cpp
auto scopeTree = semanticAnalyzer->getScopeTree();
IRBuilder builder(scopeTree);

builder.syncWithScopeTree();
auto currentScope = builder.getCurrentScope();

// 分配指针寄存器
std::string ptrReg = builder.newRegister("data", "ptr");  // data_ptr_2_1
builder.emitAlloca(ptrReg, "i32", 4);

// 分配值寄存器
std::string valReg = builder.newRegister("data", "val");  // data_val_2_1
builder.emitStore("42", ptrReg, "i32");

// 分配地址寄存器（用于 getelementptr）
std::string addrReg = builder.newRegister("data", "addr");  // data_addr_2_1
std::vector<std::pair<std::string, std::string>> indices = {{"0", "i32"}};
builder.emitGetElementPtr(addrReg, ptrReg, indices);

// 临时计算寄存器
std::string tmpReg = builder.newRegister("data", "tmp");  // data_tmp_2_1
builder.emitAdd(tmpReg, valReg, "10", "i32");

// 当作用域退出时自动清理
scopeTree->ExitScope();  // 这会触发 builder.cleanupScopeRegisters()
```

### 函数调用

```cpp
IRBuilder builder;

// 调用内置函数
std::string result = builder.newRegister();
std::vector<std::string> args = {"%str_ptr"};
builder.emitCall(result, "print", args, "void");

// 调用用户函数
std::string addResult = builder.newRegister();
std::vector<std::string> addArgs = {"%a", "%b"};
builder.emitCall(addResult, "add", addArgs, "i32");
```

### 控制流

```cpp
IRBuilder builder;

// 进入 if 控制流上下文
builder.enterControlFlowContext("if");

// 条件跳转
std::string condition = builder.newRegister();
builder.emitIcmp(condition, "ne", "%x", "%y", "i32");

// 创建嵌套基本块（自动处理命名冲突）
std::string thenBlock = builder.newNestedBasicBlock("then", "if");
std::string elseBlock = builder.newNestedBasicBlock("else", "if");
std::string endBlock = builder.newNestedBasicBlock("end", "if");

builder.emitCondBr(condition, thenBlock, elseBlock);

// then 基本块
builder.setCurrentBasicBlock(thenBlock);
builder.enterScope();  // then 块的作用域
// ... then 逻辑 ...
builder.exitScope();
builder.emitBr(endBlock);

// else 基本块
builder.setCurrentBasicBlock(elseBlock);
builder.enterScope();  // else 块的作用域
// ... else 逻辑 ...
builder.exitScope();
builder.emitBr(endBlock);

// end 基本块
builder.setCurrentBasicBlock(endBlock);
// ... 合并逻辑 ...

// 退出 if 控制流上下文
builder.exitControlFlowContext();
```

### 循环处理

```cpp
IRBuilder builder;

// 进入 loop 控制流上下文
builder.enterControlFlowContext("loop");

// 创建循环相关基本块
std::string headBlock = builder.newNestedBasicBlock("head", "loop");
std::string bodyBlock = builder.newNestedBasicBlock("body", "loop");
std::string endBlock = builder.newNestedBasicBlock("end", "loop");

// 循环头
builder.setCurrentBasicBlock(headBlock);
// 循环条件检查...
builder.emitCondBr(condition, bodyBlock, endBlock);

// 循环体
builder.setCurrentBasicBlock(bodyBlock);
builder.enterScope();  // 循环体作用域
// ... 循环体逻辑 ...
builder.exitScope();
builder.emitBr(headBlock);  // 跳回循环头

// 循环结束
builder.setCurrentBasicBlock(endBlock);

// 退出 loop 控制流上下文
builder.exitControlFlowContext();
```

## 与其他组件的集成

### 与 TypeMapper 的集成

IRBuilder 依赖 TypeMapper 进行类型转换：

```cpp
std::string rxType = "i32";
std::string llvmType = TypeMapper::mapRxTypeToLLVM(rxType);
builder.emitAlloca(reg, llvmType);
```

### 与 FunctionGenerator 的集成

FunctionGenerator 使用 IRBuilder 生成函数体：

```cpp
class FunctionGenerator {
private:
    IRBuilder& builder;
    
public:
    void generateFunctionBody(const Function* func) {
        // 使用 builder 生成函数体
        std::string entry = builder.newBasicBlock("entry");
        builder.setCurrentBasicBlock(entry);
        // ... 生成函数逻辑 ...
    }
};
```

### 与 ExpressionGenerator 的集成

ExpressionGenerator 使用 IRBuilder 生成表达式：

```cpp
class ExpressionGenerator {
private:
    IRBuilder& builder;
    
public:
    std::string generateExpression(const Expression* expr) {
        // 使用 builder 生成表达式代码
        std::string result = builder.newRegister();
        // ... 生成表达式逻辑 ...
        return result;
    }
};
```

## 测试策略

### 单元测试

1. **寄存器管理测试**：验证寄存器分配和类型跟踪
2. **基本块管理测试**：验证基本块创建和跳转
3. **指令生成测试**：验证各种指令的正确生成
4. **错误处理测试**：验证错误检测和报告

### 集成测试

1. **函数生成测试**：验证完整函数的生成
2. **控制流测试**：验证复杂控制流的生成
3. **类型系统测试**：验证类型映射的正确性
4. **输出验证测试**：验证输出 IR 的语法正确性

## 总结

IRBuilder 是 IR 生成模块的核心组件，提供了完整的 LLVM IR 文本生成功能。通过自定义的指令生成系统，IRBuilder 能够在不依赖 LLVM C++ API 的情况下生成符合 LLVM 15 规范的 IR 文本。

IRBuilder 的设计充分考虑了类型安全、寄存器管理、基本块管理和错误处理等关键方面，为其他 IR 生成组件提供了坚实的基础。通过与 TypeMapper、FunctionGenerator 和 ExpressionGenerator 等组件的紧密集成，IRBuilder 能够支持完整的 Rx 语言到 LLVM IR 的转换。
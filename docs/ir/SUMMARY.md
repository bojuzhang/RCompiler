# IR 生成模块设计总结

## 概述

本文档总结了重新设计的 Rx 语言编译器 IR 生成模块的完整设计方案。与原设计不同，新的 IR 生成模块不使用 LLVM C++ API，而是通过自定义的 IRBuilder 直接生成 LLVM IR 文本，并直接输出到 stdout 与 builtin.c 联合编译运行。

## 核心架构变更

### 主要设计变更

1. **无 LLVM 依赖**：完全移除对 LLVM C++ API 的依赖
2. **自定义 IRBuilder**：实现自己的 IR 指令生成系统
3. **直接文本输出**：生成 LLVM IR 文本并输出到 stdout
4. **内置函数声明**：只声明内置函数，依赖 builtin.c 实现
5. **最后阶段设计**：作为编译器最后阶段，无需考虑后续优化

### 主要组件

1. **IRGenerator（主控制器）**
   - 协调整个 IR 生成过程
   - 管理 IR 文本输出流
   - 提供统一的错误处理机制

2. **IRBuilder（自定义 IR 构建器）**
   - 自定义的 IR 指令生成系统
   - 管理寄存器分配（_1, _2, _3...）
   - 管理基本块生成和标签
   - 生成符合 LLVM 语法的文本指令

3. **TypeMapper（类型映射器）**
   - 将 Rx 类型系统映射到 LLVM 类型系统
   - 处理类型转换和类型检查
   - 维护类型缓存以提高性能

4. **ExpressionCodegen（表达式代码生成器）**
   - 为各种表达式生成 LLVM IR 文本
   - 管理表达式值的计算和寄存器分配
   - 处理复杂的表达式语义

5. **StatementCodegen（语句代码生成器）**
   - 为控制流语句生成 LLVM IR 文本
   - 管理基本块和跳转指令
   - 维护控制流上下文

6. **FunctionCodegen（函数代码生成器）**
   - 处理函数定义和调用
   - 管理函数参数和返回值
   - 生成函数签名和基本块结构

7. **BuiltinDeclarator（内置函数声明器）**
   - 生成内置函数的 LLVM IR 声明
   - 管理外部函数接口和类型信息
   - 提供内置函数查找和验证

## 工作流程

### 1. 初始化阶段
- 创建 IR 文本输出流
- 设置目标三元组（riscv32-unknown-unknown-elf）
- 声明所有内置函数

### 2. 顶层处理阶段
- 遍历 AST 顶层节点
- 处理函数定义和全局声明
- 建立全局符号表

### 3. 函数生成阶段
- 创建函数定义和签名
- 创建入口基本块
- 处理函数体语句
- 生成返回语句

### 4. 表达式生成阶段
- 确定表达式类型
- 生成表达式值的计算 IR
- 管理寄存器分配
- 处理类型转换

### 5. 语句生成阶段
- 生成控制流结构
- 处理变量声明和赋值
- 管理基本块和跳转
- 维护控制流上下文

### 6. 输出阶段
- 将生成的 IR 文本输出到 stdout
- 与 builtin.c 联合编译运行

## 类型系统映射

### 基本类型映射
```
Rx 类型    → LLVM 类型
i32        → i32
i64        → i64
u32        → i32
u64        → i64
bool       → i1
char       → i8
str        → i8*
unit       → void
! (never)  → 无返回类型函数
```

### 复合类型映射
```
[T; N]     → [N x T]
&T          → T*
&mut T      → T*
Struct {..} → %Struct 名称
fn(A)->B    → 函数类型
```

### 类型转换规则
- 隐式转换：i32 → i64, u32 → u64
- 显式转换：需要类型转换指令
- 引用转换：自动解引用和重新引用

## 自定义 IRBuilder 设计

### 核心功能
```cpp
class IRBuilder {
public:
    // 寄存器管理
    std::string newRegister();        // _1, _2, _3...
    std::string newNamedRegister(const std::string& prefix);
    
    // 基本块管理
    std::string newBasicBlock(const std::string& prefix);
    void setCurrentBasicBlock(const std::string& bb);
    
    // 指令生成
    void emitInstruction(const std::string& instruction);
    void emitLabel(const std::string& label);
    void emitComment(const std::string& comment);
    
    // 类型指令
    void emitAlloca(const std::string& reg, const std::string& type, int align = 4);
    void emitStore(const std::string& value, const std::string& ptr, const std::string& type, int align = 4);
    void emitLoad(const std::string& reg, const std::string& ptr, const std::string& type, int align = 4);
    
    // 算术指令
    void emitAdd(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitSub(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    void emitMul(const std::string& result, const std::string& left, const std::string& right, const std::string& type);
    
    // 比较指令
    void emitIcmp(const std::string& result, const std::string& cond, const std::string& left, const std::string& right, const std::string& type);
    
    // 控制流指令
    void emitBr(const std::string& condition, const std::string& thenLabel, const std::string& elseLabel);
    void emitBr(const std::string& label);
    void emitRet(const std::string& value = "", const std::string& type = "void");
    
    // 函数调用
    void emitCall(const std::string& result, const std::string& funcName, const std::vector<std::string>& args, const std::string& returnType);
    
    // 内存操作
    void emitGetElementPtr(const std::string& result, const std::string& ptr, const std::vector<std::string>& indices, const std::string& type);
};
```

## 表达式生成策略

### 1. 字面量表达式
```cpp
// 整数字面量
let x: i32 = 42;
// LLVM IR: %_1 = alloca i32, align 4
//          store i32 42, i32* %_1, align 4
```

### 2. 二元表达式
```cpp
// 算术运算
let result: i32 = a + b;
// LLVM IR: %_2 = load i32, i32* %a_ptr, align 4
//          %_3 = load i32, i32* %b_ptr, align 4
//          %_4 = add i32 %_2, %_3

// 比较运算
if (a < b) { ... }
// LLVM IR: %_5 = icmp slt i32 %_2, %_3
//          br i1 %_5, label %bb_then, label %bb_else
```

### 3. 函数调用表达式
```cpp
// 内置函数调用
printlnInt(42);
// LLVM IR: call void @printlnInt(i32 42)

// 用户函数调用
let result: i32 = add(a, b);
// LLVM IR: %_6 = call i32 @add(i32 %_2, i32 %_3)
```

### 4. 数组和结构体访问
```cpp
// 数组访问
let value: i32 = arr[index];
// LLVM IR: %_7 = load i32*, i32** %arr_ptr, align 4
//          %_8 = load i32, i32* %index_ptr, align 4
//          %_9 = getelementptr [10 x i32], [10 x i32]* %_7, i64 0, i32 %_8
//          %_10 = load i32, i32* %_9, align 4

// 结构体字段访问
let field: i32 = obj.field;
// LLVM IR: %_11 = getelementptr %Struct, %Struct* %obj_ptr, i32 0, i32 field_index
//          %_12 = load i32, i32* %_11, align 4
```

## 语句生成策略

### 1. 变量声明语句
```cpp
// 变量声明
let x: i32 = 42;
// LLVM IR: %_13 = alloca i32, align 4
//          store i32 42, i32* %_13, align 4
```

### 2. 条件语句
```cpp
// if 语句
if (condition) {
    then_block
} else {
    else_block
}
// LLVM IR: %_14 = icmp ...
//          br i1 %_14, label %bb_then, label %bb_else
// bb_then:                                       ; preds = %bb_entry
//          ; then_block
//          br label %bb_end
// bb_else:                                       ; preds = %bb_entry
//          ; else_block  
//          br label %bb_end
// bb_end:                                        ; preds = %bb_then, %bb_else
//          ; 合并点
```

### 3. 循环语句
```cpp
// while 循环
while (condition) {
    body
}
// LLVM IR: br label %bb_cond
// bb_cond:                                       ; preds = %bb_body, %bb_entry
//          %_15 = icmp ...
//          br i1 %_15, label %bb_body, label %bb_end
// bb_body:                                       ; preds = %bb_cond
//          ; body
//          br label %bb_cond
// bb_end:                                        ; preds = %bb_cond
//          ; 循环结束
```

## 内置函数声明策略

### 1. 函数声明格式
```cpp
// 输出函数声明
void declareBuiltinFunctions() {
    // 输出函数
    emitInstruction("declare dso_local void @print(ptr)");
    emitInstruction("declare dso_local void @println(ptr)");
    emitInstruction("declare dso_local void @printInt(i32)");
    emitInstruction("declare dso_local void @printlnInt(i32)");
    
    // 输入函数
    emitInstruction("declare dso_local ptr @getString()");
    emitInstruction("declare dso_local i32 @getInt()");
    
    // 内存管理函数
    emitInstruction("declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)");
    emitInstruction("declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)");
    
    // 特殊函数
    emitInstruction("declare dso_local void @exit(i32)");
}
```

### 2. 函数调用生成
```cpp
// 生成函数调用
std::string generateBuiltinCall(const CallExpression* call) {
    std::string funcName = call->getFunctionName();
    
    if (funcName == "printlnInt") {
        std::string argReg = generateExpression(call->getArgument(0));
        builder.emitCall("", "printlnInt", {argReg}, "void");
        return ""; // void 返回
    }
    
    // 其他内置函数...
}
```

## 内存管理策略

### 1. 栈分配
```cpp
// 局部变量分配
void allocateVariable(const std::string& name, const std::string& type) {
    std::string reg = builder.newRegister();
    builder.emitAlloca(reg, type, 4);
    variableMap[name] = reg;
}
// LLVM IR: %_16 = alloca i32, align 4
```

### 2. 堆分配
```cpp
// 调用 malloc
std::string allocateHeapMemory(const std::string& size) {
    std::string resultReg = builder.newRegister();
    builder.emitCall(resultReg, "malloc", {size}, "ptr");
    return resultReg;
}
// LLVM IR: %_17 = call ptr @malloc(i32 %_18)
```

### 3. 内存操作
```cpp
// 调用 memset
void generateMemset(const std::string& dest, int value, const std::string& size) {
    std::string resultReg = builder.newRegister();
    builder.emitCall(resultReg, "builtin_memset", {dest, std::to_string(value), size}, "ptr");
}
// LLVM IR: %_19 = call ptr @builtin_memset(ptr %_20, i8 0, i32 64)
```

## 输出格式

### 1. 目标三元组
```llvm
target triple = "riscv32-unknown-unknown-elf"
```

### 2. 内置函数声明
```llvm
declare dso_local void @print(ptr)
declare dso_local void @println(ptr)
declare dso_local void @printInt(i32)
declare dso_local void @printlnInt(i32)
declare dso_local ptr @getString()
declare dso_local i32 @getInt()
declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
```

### 3. 函数定义格式
```llvm
define i32 @main() {
start:
  %_1 = alloca [4 x i8], align 4
  %_2 = alloca [68 x i8], align 4
  call void @new(ptr sret([68 x i8]) align 4 %_2)
  store i32 0, ptr %_1, align 4
  br label %bb2

bb2:                                              ; preds = %start
  %_3 = load i32, ptr %_1, align 4
  %_4 = icmp slt i32 %_3, 10
  br i1 %_4, label %bb3, label %bb6

bb3:                                              ; preds = %bb2
  %_5 = call i32 @getInt() align 4
  call void @push(ptr align 4 %_2, i32 signext %_5)
  %_6 = load i32, ptr %_1, align 4
  %_7 = add i32 %_6, 1
  store i32 %_7, ptr %_1, align 4
  br label %bb2

bb6:                                              ; preds = %bb2, %bb9
  %_8 = call zeroext i1 @empty(ptr align 4 %_2)
  br i1 %_8, label %bb8, label %bb9

bb9:                                              ; preds = %bb6
  %_9 = call i32 @pop(ptr align 4 %_2)
  call void @printlnInt(i32 signext %_9)
  br label %bb6

bb8:                                              ; preds = %bb6
  ret i32 0
}
```

## 错误处理机制

### 1. 编译时错误
- 类型不匹配
- 未定义的符号
- 语法错误（语义分析阶段捕获）

### 2. 错误报告
- 详细的错误信息
- 源代码位置
- 修复建议

### 3. 错误恢复
- 尝试继续生成 IR
- 插入默认值或跳过错误代码
- 确保生成的 IR 语法正确

## 性能优化考虑

### 1. 代码质量
- 生成简洁高效的 IR
- 避免不必要的临时变量
- 合理使用寄存器

### 2. 输出效率
- 使用字符串流缓冲输出
- 减少频繁的 I/O 操作
- 优化字符串拼接

### 3. 内存使用
- 及时释放临时对象
- 避免内存泄漏
- 优化数据结构

## 测试策略

### 1. 语法测试
- 验证生成的 IR 文本语法正确
- 使用 LLVM 工具验证 IR 有效性
- 确保符合 LLVM 语法规范

### 2. 语义测试
- 验证生成的 IR 语义正确
- 使用测试用例验证执行结果
- 确保与预期行为一致

### 3. 集成测试
- 测试与 builtin.c 的联合编译
- 验证完整的编译流程
- 确保输出结果正确

### 4. 回归测试
- 确保新功能不破坏现有功能
- 使用自动化测试框架
- 定期运行完整测试套件

## 与现有代码的集成

### 1. 主程序集成
```cpp
int main(int argc, char* argv[]) {
    // ... 词法分析、语法分析、语义分析
    
    // IR 生成
    IRGenerator irGenerator(symbolTable, typeChecker);
    bool success = irGenerator.generateIR(ast);
    
    if (success) {
        std::cout << irGenerator.getIROutput();
        return 0;
    } else {
        std::cerr << "IR generation failed" << std::endl;
        return 1;
    }
}
```

### 2. 编译流程
```bash
# 编译 Rx 源代码到 LLVM IR
rxcompiler source.rx > output.ll

# 与 builtin.c 联合编译
clang builtin.c output.ll -o program

# 运行程序
./program
```

## 实现优先级

### 第一阶段（核心功能）
1. 实现基本的 IRBuilder 类
2. 实现基本类型映射
3. 实现简单表达式生成
4. 实现基本语句生成

### 第二阶段（完整功能）
1. 实现复合类型支持
2. 实现复杂表达式生成
3. 实现控制流语句
4. 实现内置函数声明

### 第三阶段（优化和完善）
1. 实现错误处理完善
2. 实现性能优化
3. 实现测试覆盖
4. 实现文档完善

## 总结

重新设计的 IR 生成模块具有以下核心特点：

### 设计优势
1. **独立性**：完全自包含，不依赖 LLVM C++ API
2. **可控性**：完全控制 IR 生成过程和输出格式
3. **简洁性**：直接文本输出，无需复杂的 API 调用
4. **可维护性**：清晰的模块分离和接口定义
5. **可扩展性**：为未来功能扩展预留接口

### 技术特点
1. **自定义 IRBuilder**：完整的 IR 指令生成系统
2. **寄存器管理**：自动分配和命名寄存器
3. **基本块管理**：自动生成和管理基本块
4. **类型安全**：保持类型系统的正确性
5. **错误处理**：统一的错误报告机制

### 实用价值
1. **简化依赖**：减少对外部库的依赖
2. **提高性能**：直接文本生成，减少开销
3. **便于调试**：生成的 IR 可读性强
4. **易于集成**：与现有编译器架构良好集成

这种设计确保了 IR 生成模块的独立性、可控性和正确性，为 Rx 语言编译器的成功实现提供了坚实的基础，同时满足了项目的特殊要求。

## 相关文档

### 核心文档
- [`README.md`](README.md) - IR 生成模块概述和设计变更
- [`overall_workflow.md`](overall_workflow.md) - 整体工作流程和自定义 IRBuilder
- [`component_interaction.md`](component_interaction.md) - 组件交互接口和自定义实现

### 组件详细文档
- [`irgenerator/README.md`](irgenerator/README.md) - IRGenerator 主控制器设计文档
- [`irbuilder/README.md`](irbuilder/README.md) - IRBuilder 自定义 IR 构建器设计文档
- [`typemapper/README.md`](typemapper/README.md) - TypeMapper 类型映射器设计文档
- [`expressiongenerator/README.md`](expressiongenerator/README.md) - ExpressionGenerator 表达式代码生成器设计文档
- [`statementgenerator/README.md`](statementgenerator/README.md) - StatementGenerator 语句代码生成器设计文档
- [`functioncodegen/README.md`](functioncodegen/README.md) - FunctionCodegen 函数代码生成器设计文档
- [`builtindeclarator/README.md`](builtindeclarator/README.md) - BuiltinDeclarator 内置函数声明器设计文档
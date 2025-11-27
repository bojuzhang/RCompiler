# IR 生成模块设计文档

## 概述

IR（Intermediate Representation）生成模块是 Rx 语言编译器的最后阶段，负责将经过语义分析的 AST（抽象语法树）转换为 LLVM-15 兼容的 LLVM IR 文本，并直接输出到 stdout。本模块位于编译器流水线的语义分析阶段之后，是连接高级语言抽象和底层代码生成的关键桥梁。

## 重要设计变更

根据项目要求，IR 生成模块有以下重要设计变更：

1. **无 LLVM 依赖**：不使用 LLVM C++ API 或 IRBuilder，完全自定义 IR 生成系统
2. **直接文本输出**：生成 LLVM IR 文本并直接输出到 stdout，与 builtin.c 联合编译
3. **内置函数声明**：只声明内置函数，不生成其实现，依赖 builtin.c 提供实现
4. **最后阶段**：IR 生成是编译器的最后阶段，无需考虑后续优化或代码生成

## 文档结构

本文档集包含以下部分：

- [`overall_workflow.md`](overall_workflow.md) - IR 生成的整体工作流程和架构设计
- [`component_interaction.md`](component_interaction.md) - IR 生成器各组件之间的交互接口设计
- [`SUMMARY.md`](SUMMARY.md) - IR 生成模块设计总结

## 设计原则

IR 生成模块遵循以下设计原则：

1. **独立性**：不依赖 LLVM C++ API，完全自包含的 IR 生成系统
2. **文本输出**：直接生成符合 LLVM 语法规范的文本输出
3. **模块化设计**：将 IR 生成分解为多个独立但协作的组件
4. **类型安全**：确保类型系统正确映射到 LLVM 类型系统
5. **错误处理**：提供完善的错误检测和报告机制
6. **性能考虑**：生成高效的 LLVM IR 文本

## 技术要求

- **目标平台**：LLVM-15 兼容的 IR 文本
- **输出格式**：LLVM IR 文本，直接输出到 stdout
- **依赖限制**：不使用 LLVM C++ API，完全自定义实现
- **内置函数**：仅声明，与 builtin.c 联合编译运行
- **优化级别**：不包含优化，专注于正确的 IR 生成

## 核心功能

IR 生成模块负责以下核心功能：

1. **类型映射**：将 Rx 语言类型系统映射到 LLVM 类型系统
2. **表达式生成**：为各种表达式生成对应的 LLVM IR 文本
3. **语句生成**：为控制流和声明语句生成 LLVM IR 文本
4. **函数生成**：处理函数定义、调用和参数传递
5. **内置函数声明**：生成对内置函数的声明
6. **内存管理**：处理栈和堆内存分配的 IR 生成
7. **寄存器管理**：自动分配和管理 LLVM IR 寄存器
8. **基本块管理**：自动生成和管理基本块标签
9. **错误处理**：检测和报告 IR 生成过程中的错误

## 主要组件

### 1. IRGenerator（主控制器）
- 协调整个 IR 生成过程
- 管理 IR 文本输出流
- 处理顶层声明和全局符号
- 提供统一的错误处理机制

### 2. IRBuilder（自定义 IR 构建器）
- 自定义的 IR 指令生成系统
- 管理寄存器分配和命名
- 管理基本块生成和标签
- 生成符合 LLVM 语法的文本指令

### 3. TypeMapper（类型映射器）
- 将 Rx 类型系统映射到 LLVM 类型系统
- 处理类型转换和类型检查
- 维护类型缓存以提高性能

### 4. ExpressionCodegen（表达式代码生成器）
- 为各种表达式生成 LLVM IR 文本
- 管理表达式值的计算和寄存器分配
- 处理复杂的表达式语义

### 5. StatementCodegen（语句代码生成器）
- 为控制流语句生成 LLVM IR 文本
- 管理基本块和跳转指令
- 维护控制流上下文

### 6. FunctionCodegen（函数代码生成器）
- 处理函数定义和调用
- 管理函数参数和返回值
- 生成函数签名和基本块结构

### 7. BuiltinDeclarator（内置函数声明器）
- 生成内置函数的 LLVM IR 声明
- 管理外部函数接口和类型信息
- 提供内置函数查找和验证

## 与其他模块的交互

IR 生成模块与编译器其他模块的交互关系：

- **输入**：接收来自语义分析阶段的 AST 和符号信息
- **输出**：生成 LLVM IR 文本，直接输出到 stdout
- **依赖**：依赖词法分析、语法分析和语义分析的结果
- **集成**：与 builtin.c 联合编译运行

## 内置函数支持

IR 生成器支持以下内置函数的声明：

### 输出函数
```llvm
declare dso_local void @print(ptr)
declare dso_local void @println(ptr)
declare dso_local void @printInt(i32)
declare dso_local void @printlnInt(i32)
```

### 输入函数
```llvm
declare dso_local ptr @getString()
declare dso_local i32 @getInt()
```

### 内存管理函数
```llvm
declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
```

### 特殊函数
```llvm
declare dso_local void @exit(i32)
```

## 输出格式示例

### 目标三元组
```llvm
target triple = "riscv32-unknown-unknown-elf"
```

### 函数定义
```llvm
define i32 @main() {
start:
  %_1 = alloca [4 x i8], align 4
  %_2 = alloca [68 x i8], align 4
  call void @new(ptr sret([68 x i8]) align 4 %_2)
  store i32 0, ptr %_1, align 4
  br label %bb2

bb2:
  ; ... 函数体
}
```

### 表达式生成
```llvm
; 加法表达式
%_3 = add i32 %_1, %_2

; 比较表达式
%_4 = icmp slt i32 %_3, 10

; 函数调用
call void @printlnInt(i32 %_3)
```

## 开发指南

在开发 IR 生成模块时，请参考以下指南：

1. 首先阅读 [`overall_workflow.md`](overall_workflow.md) 了解整体架构
2. 参考 [`component_interaction.md`](component_interaction.md) 了解组件接口规范
3. 确保不使用任何 LLVM C++ API
4. 确保生成的 IR 文本符合 LLVM 语法规范
5. 参考测试用例了解预期的 IR 输出格式
6. 使用内置函数的声明而不是实现

## 测试策略

IR 生成模块的测试策略包括：

1. **语法测试**：验证生成的 IR 文本语法正确
2. **语义测试**：验证生成的 IR 语义正确
3. **集成测试**：测试与 builtin.c 的联合编译
4. **回归测试**：使用现有测试用例确保兼容性
5. **性能测试**：验证生成 IR 的质量和效率

## 版本历史

- v2.0: 重新设计，移除 LLVM 依赖，直接文本输出
- v1.0: 初始设计文档，使用 LLVM C++ API

## 注意事项

1. **不使用 LLVM API**：严禁使用任何 LLVM C++ API 或 IRBuilder
2. **文本输出**：所有输出必须是纯文本格式的 LLVM IR
3. **内置函数**：只生成声明，不生成实现
4. **寄存器命名**：使用 _1, _2, _3... 的命名约定
5. **基本块命名**：使用有意义的名称和数字后缀
6. **错误处理**：确保在错误情况下仍能生成语法正确的 IR

## 未来扩展

虽然 IR 生成是最后阶段，但仍考虑以下扩展可能：

1. **调试信息**：生成调试信息和源代码映射
2. **注释生成**：在 IR 中添加有用的注释
3. **优化提示**：为手动优化提供提示信息
4. **错误恢复**：更好的错误恢复机制
5. **性能分析**：生成性能分析相关的 IR 注释
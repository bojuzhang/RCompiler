# IR 生成整体工作流程

## 概述

IR 生成阶段是 Rx 语言编译器的最后阶段，负责将经过语义分析的 AST 转换为 LLVM-15 兼容的 LLVM IR 文本，并直接输出到 stdout。本文档描述了 IR 生成的整体工作流程、主要阶段和关键决策点。

## 编译器流水线中的位置

```
源代码 → 词法分析 → 语法分析 → 语义分析 → IR 生成 → stdout → LLVM 编译
```

IR 生成阶段是编译器的最后阶段，直接生成 LLVM IR 文本输出，与 builtin.c 联合编译运行。

## 整体架构

### 主要组件

IR 生成器由以下主要组件组成：

1. **IRGenerator（主控制器）**
   - 协调整个 IR 生成过程
   - 管理 IR 文本输出
   - 处理顶层声明

2. **IRBuilder（IR 构建器）**
   - 自定义的 IR 指令生成系统
   - 管理寄存器分配和基本块
   - 生成 LLVM IR 文本指令

3. **TypeMapper（类型映射器）**
   - 将 Rx 类型系统映射到 LLVM 类型系统
   - 处理类型转换和类型检查

4. **ExpressionGenerator（表达式代码生成器）**
   - 为各种表达式生成 LLVM IR 文本
   - 管理表达式值的计算和寄存器分配
   - 完全处理 BlockExpression，包括其中的语句和尾表达式值

5. **StatementGenerator（语句代码生成器）**
   - 为真正的语句生成 LLVM IR 文本
   - 管理基本块和跳转指令
   - 只处理 Let 语句、Item 语句和 ExpressionStatement
   - 不再处理 BlockExpression

6. **FunctionCodegen（函数代码生成器）**
   - 处理函数定义和调用
   - 管理函数参数和返回值

7. **BuiltinDeclarator（内置函数声明器）**
   - 生成内置函数的声明
   - 管理外部函数接口

### 数据流

```
AST → 符号表 → 类型信息 → IRGenerator → ExpressionGenerator/StatementGenerator → IRBuilder → LLVM IR 文本 → stdout
                ↓
        控制流信息 → 基本块构建 → 控制流图 → IR 文本输出
```

### 循环依赖解决

由于 AST 节点设计中存在循环依赖关系：
- `Statement` 使用组合模式，包含 `Expression`（控制流语句）
- `BlockExpression` 包含 `Statement` 向量和可选的 `Expression`
- 这导致 `StatementGenerator` 和 `ExpressionGenerator` 之间的循环依赖

**解决策略**：
1. **前向声明**：两个组件互相前向声明
2. **接口分离**：将循环依赖的操作分离到独立接口
3. **延迟初始化**：通过构造函数后初始化解决循环引用
4. **职责明确**：
   - `StatementGenerator`：只处理真正的语句（Let、Item、ExpressionStatement）
   - `ExpressionGenerator`：处理所有表达式，包括 BlockExpression 和控制流表达式
   - `BlockExpression`：完全由 `ExpressionGenerator` 处理，包括语句生成和值计算
   - `BlockExpression` 的值：尾表达式的值（如果有），否则为单元类型

## 详细工作流程

### 阶段 1：初始化准备

1. **创建 IR 输出流**
   - 创建字符串输出流用于收集生成的IR代码
   - 初始化IRGenerator实例，传入输出流引用
   - 设置输出缓冲区和格式化参数

2. **设置目标平台**
   - 通过IRBuilder输出目标三元组（riscv32-unknown-unknown-elf）
   - 配置数据布局（如果需要）
   - 设置模块级别的编译参数

3. **声明内置函数**
   - 调用BuiltinDeclarator生成所有内置函数声明
   - 输出字符串I/O函数声明（print, println）
   - 输出整数I/O函数声明（printInt, printlnInt）
   - 输出输入函数声明（getString, getInt）
   - 输出内存管理函数声明（builtin_memset, builtin_memcpy）
   - 输出系统调用函数声明（exit等）

### 阶段 2：顶层处理

1. **遍历 AST 顶层节点**
   - 识别和处理函数定义节点
   - 处理全局常量声明
   - 处理结构体类型定义
   - 处理模块级别的导入声明

2. **建立全局符号表**
   - 从符号表中提取全局符号信息
   - 创建全局符号到LLVM符号的映射
   - 预处理符号依赖关系

### 阶段 3：函数生成

对于每个函数：

1. **创建函数定义**
   - 通过TypeMapper获取函数类型的LLVM表示
   - 通过IRBuilder生成函数定义开始标记
   - 处理函数名、返回类型和参数列表
   - 输出格式：define <return_type> @<function_name>(<params>) {

2. **创建入口基本块**
   - 为函数创建入口基本块
   - 设置当前活动基本块
   - 生成基本块标签
   - 输出格式：<label>:

3. **处理函数体**
   - 为函数参数分配栈空间（如果需要）
   - 生成函数体语句的IR代码
   - 处理控制流和返回语句
   - 管理局部变量的生命周期

4. **结束函数定义**
   - 生成函数定义结束标记
   - 清理函数相关的临时数据
   - 验证函数结构的完整性
   - 输出格式：}

### 阶段 4：表达式和语句生成

#### 表达式生成

对于每个表达式：

1. **确定表达式类型**
   - 从语义分析阶段的类型信息中获取表达式类型
   - 通过TypeMapper将Rx类型映射为LLVM类型
   - 处理类型转换和类型检查

2. **生成表达式 IR**
   - 字面量表达式：生成对应的常量值
   - 变量表达式：从符号表加载变量值
   - 二元操作表达式：生成算术或逻辑运算指令
   - 函数调用表达式：生成函数调用指令
   - 控制流表达式：生成基本块和跳转指令

3. **寄存器管理**
   - 为表达式结果分配临时寄存器
   - 通过IRBuilder管理寄存器生命周期
   - 处理复杂表达式的寄存器依赖关系
   - 示例流程：生成左右操作数→分配结果寄存器→生成运算指令

#### 语句生成

对于每个语句：

1. **语句类型识别**
   - Let语句：处理变量声明和初始化
   - ExpressionStatement：处理表达式语句（有分号）
   - Item语句：处理函数定义、结构体定义等

2. **循环依赖处理**
   - ExpressionGenerator完全负责BlockExpression的处理
   - 创建新的作用域和基本块上下文
   - 通过StatementGenerator处理块内语句
   - 处理尾表达式并返回结果值
   - 管理作用域的进入和退出
   - 错误处理：检测StatementGenerator未设置的情况

3. **寄存器和作用域管理**
   - StatementGenerator处理Let语句的变量分配
   - 为变量分配栈空间并生成alloca指令
   - 通过ExpressionGenerator生成初始化表达式
   - 将变量注册到符号表和IRBuilder中
   - 职责划分：StatementGenerator不处理BlockExpression

### 阶段 5：语句生成

对于每个语句：

1. **控制流语句**
   - 生成条件表达式的IR代码
   - 创建then、else和end基本块
   - 生成条件分支指令
   - 处理分支间的跳转和合并
   - 输出格式：br i1 %<condition>, label %<then_label>, label %<else_label>

2. **声明语句**
   - 获取变量名称和类型信息
   - 通过TypeMapper获取LLVM类型表示
   - 分配变量寄存器并生成alloca指令
   - 设置变量的对齐要求
   - 输出格式：%<reg> = alloca <type>, align <alignment>

3. **赋值语句**
   - 生成赋值右侧表达式的IR代码
   - 获取目标变量的指针
   - 生成存储指令将值存入变量
   - 处理类型转换和对齐要求
   - 输出格式：store <type> %<value>, <ptr_type> %<ptr>, align <alignment>

### 阶段 6：内存管理

1. **栈分配**
   - 为局部变量分配栈空间
   - 通过IRBuilder生成alloca指令
   - 设置适当的对齐要求
   - 输出格式：%<reg> = alloca <type>, align <alignment>

2. **堆分配**
   - 生成堆内存分配表达式
   - 调用malloc等内存分配函数
   - 处理分配失败的情况
   - 输出格式：%<reg> = call ptr @malloc(i32 %<size>)

3. **内存操作**
   - 生成内存初始化指令
   - 调用memset、memcpy等内存操作函数
   - 处理内存对齐和边界检查
   - 输出格式：%<reg> = call ptr @builtin_memset(ptr %<dest>, i8 <value>, i32 %<size>)

### 阶段 7：内置函数处理

1. **函数声明**
   - 通过BuiltinDeclarator生成所有内置函数声明
   - 声明字符串I/O函数（print, println）
   - 声明整数I/O函数（printInt, printlnInt）
   - 声明输入函数（getString, getInt）
   - 声明内存管理函数（builtin_memset, builtin_memcpy）
   - 设置函数属性和调用约定

2. **函数调用**
   - 生成内置函数的调用指令
   - 处理参数传递和返回值
   - 特殊处理可变参数函数
   - 输出格式：call <return_type> @<function_name>(<args>)

## 自定义 IR 构建器设计

### IRBuilder 核心功能

IRBuilder提供完整的LLVM IR文本生成功能，核心组件包括：

- **初始化接口**：
  - 构造函数接收输出流引用
  - 初始化寄存器和基本块计数器
  - 设置输出格式和缩进规则

- **寄存器管理**：
  - newRegister() - 生成唯一寄存器名称
  - newNamedRegister() - 生成带前缀的命名寄存器
  - 维护递增计数器确保名称唯一性

- **基本块管理**：
  - newBasicBlock() - 生成基本块标签
  - 维护基本块计数器和命名约定
  - 支持控制流相关的命名模式

- **指令生成**：
  - emitInstruction() - 输出通用指令
  - emitLabel() - 输出基本块标签
  - 确保正确的LLVM语法和缩进

- **类型指令**：
  - emitAlloca() - 生成栈分配指令
  - emitStore() - 生成存储指令
  - emitLoad() - 生成加载指令
  - 处理类型和对齐信息

- **算术指令**：
  - emitAdd(), emitSub(), emitMul() - 基本算术运算
  - emitDiv(), emitRem() - 除法和取余运算
  - 处理类型化的算术指令

- **比较指令**：
  - emitIcmp() - 生成整数比较指令
  - 支持各种比较条件（eq, ne, slt, sgt等）
  - 生成布尔值结果

- **控制流指令**：
  - emitBr() - 生成条件和无条件跳转
  - emitRet() - 生成返回指令
  - 处理基本块间的控制流转移

- **函数调用**：
  - emitCall() - 生成函数调用指令
  - 处理参数列表和返回值
  - 支持可变参数函数

- **内部状态管理**：
  - 维护输出流引用
  - 管理寄存器和基本块计数器
  - 跟踪当前基本块和作用域

## 关键设计决策

### 1. 直接文本输出

- 不使用 LLVM C++ API
- 直接生成 LLVM IR 文本
- 输出到 stdout，与 builtin.c 联合编译

### 2. 自定义寄存器管理

- 自动分配寄存器名称（_1, _2, _3...）
- 跟踪寄存器类型和生命周期
- 支持寄存器重用

### 3. 基本块管理

- 自动生成基本块标签
- 维护当前基本块上下文
- 支持嵌套控制流

### 4. 类型安全

- 严格遵循 Rx 语言的类型系统
- 在编译时进行类型检查
- 生成类型正确的 LLVM IR

## 错误处理策略

### 1. 编译时错误

- 类型不匹配
- 未定义的符号
- 语法错误（应该在语义分析阶段捕获）

### 2. 错误报告

- 提供详细的错误信息
- 包含源代码位置
- 建议修复方案

### 3. 错误恢复

- 尝试继续生成 IR
- 插入默认值或跳过错误代码
- 确保生成的 IR 语法正确

## 输出格式

### 1. 目标三元组
```
target triple = "riscv32-unknown-unknown-elf"
```

### 2. 内置函数声明
```
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
```
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

## 性能考虑

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

### 1. 单元测试

- 测试各个组件的独立功能
- 验证类型映射的正确性
- 测试表达式和语句的 IR 生成

### 2. 集成测试

- 测试完整的 IR 生成流程
- 使用标准测试用例
- 验证生成的 IR 可正确编译

### 3. 回归测试

- 确保新功能不破坏现有功能
- 使用自动化测试框架
- 定期运行完整测试套件

## 与现有代码的集成

### 1. 语义分析接口

IRGenerator提供与语义分析阶段的集成接口：

- **构造函数**：接收符号表和类型检查器引用
- **主要生成方法**：generateIR() - 处理AST顶层节点列表
- **输出获取方法**：getIROutput() - 返回生成的IR文本
- **内部组件**：包含IRBuilder、TypeMapper等子组件实例
- **错误处理**：提供错误状态检查和错误信息获取方法

### 2. 主程序集成

主程序中的IR生成集成流程：

- **初始化阶段**：创建IRGenerator实例，传入符号表和类型检查器
- **生成阶段**：调用generateIR()方法处理AST
- **输出阶段**：成功时输出IR到stdout，失败时输出错误信息
- **返回值处理**：根据生成结果返回适当的退出码
- **管道支持**：支持与后续LLVM工具链的管道操作

## 总结

重新设计的 IR 生成模块具有以下特点：

1. **直接文本输出**：不依赖 LLVM C++ API，直接生成 LLVM IR 文本
2. **自定义构建器**：实现自己的 IRBuilder 系统来管理寄存器和基本块
3. **内置函数声明**：只声明内置函数，与 builtin.c 联合编译
4. **stdout 输出**：直接输出到 stdout，便于管道操作
5. **模块化设计**：保持清晰的组件分离和接口定义
6. **职责明确**：BlockExpression 完全由 ExpressionGenerator 处理，StatementGenerator 只处理真正的语句
7. **值语义正确**：BlockExpression 有明确的值（尾表达式的值或单元类型）

这种设计确保了 IR 生成器的独立性、可控性和与现有编译器架构的良好集成，同时正确处理了 BlockExpression 的特殊语义。
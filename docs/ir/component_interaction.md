# IR 生成器组件交互接口分析

## 概述

本文档详细描述了重新设计的 IR 生成器各组件之间的交互接口设计。与之前的设计不同，新的 IR 生成器不使用 LLVM C++ API，而是通过自定义的 IRBuilder 直接生成 LLVM IR 文本输出到 stdout。

## 组件架构概览

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   IRGenerator   │    │  TypeMapper     │    │ IRBuilder       │
│   (主控制器)    │◄──►│  (类型映射器)    │◄──►│ (IR构建器)      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ExpressionCodegen │    │StatementCodegen │    │FunctionCodegen  │
│ (表达式生成器)   │    │ (语句生成器)     │    │ (函数生成器)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 ▼
                     ┌─────────────────┐
                     │BuiltinDeclarator│
                     │(内置函数声明器) │
                     └─────────────────┘
```

## 核心接口定义

### 1. IRGenerator 主控制器接口

IRGenerator作为主控制器，提供以下核心接口：

- **构造函数**：接收符号表和类型检查器引用，初始化所有子组件
- **主要生成接口**：`generateIR()` - 协调整个IR生成过程，处理顶层AST节点
- **输出获取接口**：`getIROutput()` - 获取生成的IR文本输出
- **组件访问接口**：提供对所有子组件的访问方法，包括TypeMapper、ExpressionCodegen等
- **IRBuilder访问接口**：提供对IRBuilder的直接访问
- **错误处理接口**：`reportError()` - 统一错误报告，`hasErrors()` - 错误状态检查
- **符号表访问接口**：提供符号信息查询功能

**内部实现策略**：
- 初始化阶段设置目标平台和输出格式
- 遍历AST顶层节点，分发任务给相应子组件
- 管理子组件间的依赖关系和交互
- 收集和报告生成过程中的错误信息

### 2. IRBuilder 自定义 IR 构建器接口

IRBuilder提供完整的LLVM IR文本生成功能，核心接口包括：

- **寄存器管理接口**：
  - `newRegister()` - 生成临时寄存器（_1, _2, _3...）
  - `newNamedRegister()` - 生成带前缀的命名寄存器
  - 实现策略：维护递增计数器，确保寄存器名称唯一性

- **基本块管理接口**：
  - `newBasicBlock()` - 生成基本块标签
  - `setCurrentBasicBlock()` - 设置当前活动基本块
  - `getCurrentBasicBlock()` - 获取当前基本块
  - 实现策略：维护基本块栈和计数器，支持嵌套控制流

- **指令生成接口**：
  - `emitInstruction()` - 输出通用指令
  - `emitLabel()` - 输出基本块标签
  - `emitComment()` - 输出注释
  - 实现策略：直接输出到流，确保LLVM语法正确性

- **类型指令接口**：
  - `emitAlloca()` - 生成栈分配指令
  - `emitStore()` - 生成存储指令
  - `emitLoad()` - 生成加载指令
  - `emitTargetTriple()` - 输出目标三元组
  - 实现策略：根据类型和对齐要求生成格式化指令

- **算术指令接口**：
  - `emitAdd()`, `emitSub()`, `emitMul()`, `emitDiv()`, `emitRem()` - 基本算术运算
  - 实现策略：生成类型化的算术指令，确保操作数类型匹配

- **比较指令接口**：
  - `emitIcmp()` - 生成整数比较指令
  - 实现策略：支持各种比较条件（eq, ne, slt, sgt等）

- **位运算指令接口**：
  - `emitAnd()`, `emitOr()`, `emitXor()`, `emitShl()` - 位运算操作
  - 实现策略：生成类型化的位运算指令

- **控制流指令接口**：
  - `emitBr()` - 生成条件和无条件跳转
  - `emitRet()` - 生成返回指令
  - 实现策略：处理基本块间的控制流转移

- **函数相关接口**：
  - `emitFunctionDecl()` - 生成函数声明
  - `emitFunctionDef()` - 生成函数定义开始
  - `emitFunctionEnd()` - 生成函数定义结束
  - `emitCall()` - 生成函数调用
  - 实现策略：管理函数签名、参数列表和调用约定

- **内存操作接口**：
  - `emitGetElementPtr()` - 生成地址计算指令
  - `emitBitcast()` - 生成类型转换指令
  - `emitExtractValue()` - 生成值提取指令
  - `emitInsertValue()` - 生成值插入指令
  - 实现策略：处理复杂的内存布局和类型转换

### 3. TypeMapper 类型映射器接口

TypeMapper提供完整的类型系统映射功能，核心接口包括：

- **基本类型映射接口**：
  - `mapType()` - 将Rx类型映射到LLVM类型
  - `mapBasicType()` - 处理基础类型（i32, i64, bool等）
  - `mapArrayType()` - 处理数组类型映射
  - `mapStructType()` - 处理结构体类型映射
  - `mapReferenceType()` - 处理引用类型映射
  - `mapFunctionType()` - 处理函数类型映射
  - 实现策略：维护类型缓存，避免重复映射计算

- **类型大小和对齐接口**：
  - `getTypeSize()` - 获取LLVM类型的字节大小
  - `getTypeAlignment()` - 获取LLVM类型的对齐要求
  - 实现策略：基于32位机器架构优化，提供标准类型大小表

- **类型转换接口**：
  - `generateTypeConversion()` - 生成类型转换的IR代码
  - 实现策略：通过IRBuilder生成适当的转换指令

- **类型检查接口**：
  - `isCompatible()` - 检查两个类型是否兼容
  - `canImplicitlyConvert()` - 检查是否可以隐式转换
  - 实现策略：基于LLVM类型系统规则进行兼容性判断

- **类型属性接口**：
  - `isIntegerType()`, `isPointerType()`, `isArrayType()`, `isStructType()` - 类型分类判断
  - 实现策略：基于字符串模式匹配进行类型识别

- **类型解析接口**：
  - `getElementType()` - 获取复合类型的元素类型
  - `getPointedType()` - 获取指针指向的类型
  - 实现策略：解析LLVM类型字符串，提取类型信息

- **内部实现策略**：
  - 维护类型映射缓存，提高性能
  - 支持嵌套类型的递归映射
  - 处理特殊SemanticType到LLVM类型的转换

### 4. ExpressionCodegen 表达式生成器接口

ExpressionCodegen负责处理所有表达式类型的IR生成，核心接口包括：

- **主要生成接口**：
  - `generateExpression()` - 表达式生成的统一入口点
  - 实现策略：根据表达式类型分发到具体的生成方法

- **具体表达式类型处理接口**：
  - `generateLiteral()` - 处理字面量表达式（整数、字符串、布尔值等）
  - `generatePath()` - 处理路径表达式（变量访问、常量访问）
  - `generateBinary()` - 处理二元运算表达式（算术、比较、逻辑）
  - `generateUnary()` - 处理一元运算表达式（取负、逻辑非、解引用等）
  - `generateCall()` - 处理函数调用表达式
  - `generateIndex()` - 处理数组索引表达式
  - `generateField()` - 处理字段访问表达式
  - `generateBlock()` - 处理块表达式（包含语句和尾表达式）
  - `generateIf()` - 处理if表达式（条件分支）
  - `generateLoop()` - 处理循环表达式
  - `generateBreak()`, `generateContinue()`, `generateReturn()` - 处理控制流表达式
  - 实现策略：每种表达式类型有专门的生成逻辑，处理复杂的语义

- **辅助方法接口**：
  - `generateArithmeticOperation()` - 生成算术运算IR
  - `generateComparisonOperation()` - 生成比较运算IR
  - `generateLogicalOperation()` - 生成逻辑运算IR
  - 实现策略：通过IRBuilder生成相应的LLVM指令

- **值管理接口**：
  - `storeExpressionResult()` - 存储表达式结果
  - `loadExpressionValue()` - 加载表达式值
  - 实现策略：管理表达式的临时存储和加载

- **内部实现策略**：
  - 维护表达式缓存，避免重复计算
  - 处理复杂的嵌套表达式
  - 与StatementGenerator协作处理BlockExpression中的语句
  - 通过IRBuilder管理寄存器和基本块

### 5. StatementCodegen 语句生成器接口

StatementCodegen负责处理语句类型的IR生成，核心接口包括：

- **主要生成接口**：
  - `generateStatement()` - 语句生成的统一入口点
  - 实现策略：根据语句类型分发到具体的生成方法

- **具体语句类型处理接口**：
  - `generateLet()` - 处理变量声明语句（类型推断、初始化）
  - `generateExpression()` - 处理表达式语句（函数调用、赋值等）
  - `generateIf()`, `generateWhile()`, `generateLoop()` - 处理控制流语句
  - `generateBreak()`, `generateContinue()`, `generateReturn()` - 处理跳转语句
  - 实现策略：每种语句类型有专门的生成逻辑，处理作用域和控制流

- **基本块管理接口**：
  - `createBasicBlock()` - 创建新的基本块
  - `setCurrentBasicBlock()` - 设置当前活动基本块
  - `getCurrentBasicBlock()` - 获取当前基本块
  - 实现策略：通过IRBuilder管理基本块生命周期

- **控制流上下文接口**：
  - `enterLoop()`, `exitLoop()` - 管理循环上下文
  - `enterFunction()`, `exitFunction()` - 管理函数上下文
  - 实现策略：维护上下文栈，处理嵌套控制流

- **变量管理接口**：
  - `allocateVariable()` - 分配变量存储空间
  - `storeVariable()` - 存储变量值
  - `loadVariable()` - 加载变量值
  - 实现策略：与IRBuilder协作管理变量寄存器

- **内部实现策略**：
  - 维护控制流上下文栈，支持嵌套结构
  - 管理变量符号表，处理作用域
  - 与ExpressionGenerator协作处理表达式语句
  - 注意：BlockExpression由ExpressionGenerator完全处理

### 6. FunctionCodegen 函数生成器接口

FunctionCodegen负责处理函数定义、声明和调用的IR生成，核心接口包括：

- **主要生成接口**：
  - `generateFunction()` - 生成完整函数定义
  - `generateFunctionDecl()` - 生成函数声明
  - `generateFunctionBody()` - 生成函数体
  - 实现策略：通过IRBuilder生成函数定义和函数体的完整结构

- **参数处理接口**：
  - `generateParameters()` - 生成函数参数列表
  - `generateArgumentLoad()` - 生成参数加载指令
  - 实现策略：为每个参数分配寄存器并生成加载指令

- **返回值处理接口**：
  - `generateReturnStatement()` - 生成返回语句
  - `generateReturnValue()` - 生成返回值表达式
  - 实现策略：根据函数返回类型生成适当的返回指令

- **函数调用接口**：
  - `generateFunctionCall()` - 生成普通函数调用
  - `generateBuiltinCall()` - 生成内置函数调用
  - 实现策略：生成调用指令并处理参数传递

- **内联函数处理接口**：
  - `canInline()` - 判断函数是否可以内联
  - `generateInlineCall()` - 生成内联函数调用
  - 实现策略：分析函数大小和复杂度决定是否内联

- **函数签名生成接口**：
  - `generateFunctionSignature()` - 生成函数签名
  - `generateParameterList()` - 生成参数类型列表
  - 实现策略：通过TypeMapper获取类型信息并格式化

- **内部实现策略**：
  - 维护函数缓存，避免重复生成
  - 生成函数序言和结语代码
  - 处理参数寄存器分配和加载
  - 与TypeMapper协作处理函数类型

### 7. BuiltinDeclarator 内置函数声明器接口

BuiltinDeclarator负责管理内置函数的声明，核心接口包括：

- **初始化接口**：
  - `declareBuiltinFunctions()` - 声明所有内置函数
  - 实现策略：在模块初始化时调用，生成所有必要的函数声明

- **具体内置函数声明接口**：
  - `declarePrint()`, `declarePrintln()` - 声明字符串输出函数
  - `declarePrintInt()`, `declarePrintlnInt()` - 声明整数输出函数
  - `declareGetString()`, `declareGetInt()` - 声明输入函数
  - `declareMalloc()`, `declareMemcpy()`, `declareMemset()` - 声明内存管理函数
  - `declareExit()` - 声明系统退出函数
  - 实现策略：通过IRBuilder生成符合LLVM语法的函数声明

- **内置函数查找接口**：
  - `isBuiltinFunction()` - 检查函数是否为内置函数
  - `getBuiltinFunctionType()` - 获取内置函数的完整类型
  - `getBuiltinParameterTypes()` - 获取内置函数的参数类型列表
  - 实现策略：维护内置函数信息表，提供快速查询

- **特殊函数声明接口**：
  - `declareStructFunctions()` - 声明结构体相关函数
  - 实现策略：根据结构体定义动态生成构造函数、析构函数等

- **内部实现策略**：
  - 维护内置函数信息映射表
  - 支持动态添加新的内置函数
  - 处理函数属性和调用约定
  - 通过IRBuilder生成标准格式的函数声明

## 组件间交互协议

### 1. 数据传递协议

#### 类型信息传递
TypeMapper向其他组件提供类型映射服务：
- ExpressionCodegen调用TypeMapper获取表达式类型的LLVM表示
- FunctionCodegen调用TypeMapper获取函数类型的LLVM表示
- 传递方式：通过方法调用返回类型字符串，确保类型信息在组件间正确流转

#### 值传递协议
组件间的值传递遵循以下模式：
- ExpressionCodegen生成表达式结果并返回寄存器标识符给StatementCodegen
- FunctionCodegen调用ExpressionCodegen生成返回值表达式
- 传递方式：通过字符串形式的寄存器标识符进行值传递

#### 寄存器管理协议
IRBuilder为所有组件提供统一的寄存器管理：
- 各组件通过IRBuilder接口请求新寄存器
- IRBuilder负责寄存器命名和生命周期管理
- 使用方式：组件请求寄存器后，通过IRBuilder生成相应的存储和加载指令

### 2. 控制流协议

#### 基本块管理
StatementCodegen负责控制流的基本块管理：
- 为if语句创建then、else和end基本块
- 通过IRBuilder生成条件分支和跳转指令
- 管理基本块的标签和跳转目标
- 实现策略：维护基本块栈，确保控制流正确性

#### 循环控制流
StatementCodegen提供循环上下文管理：
- 维护循环栈，存储continue和break目标
- 处理嵌套循环的上下文管理
- 为break和continue语句生成正确的跳转指令
- 错误处理：检测循环外的跳转语句并报告错误

#### BlockExpression 处理的职责划分
BlockExpression由ExpressionCodegen完全处理：
- ExpressionCodegen负责创建和管理块作用域
- 通过StatementCodegen处理块内语句
- 处理尾表达式并返回结果
- 作用域管理：确保变量生命周期的正确性

### 3. 错误处理协议

#### 错误传播
组件间的错误传播遵循统一协议：
- 所有子组件通过IRGenerator接口报告错误
- IRGenerator负责收集和格式化错误信息
- 错误信息包含源码位置和详细描述
- 传播方式：通过方法调用向上传播错误信息

#### 错误恢复
各组件实现适当的错误恢复策略：
- ExpressionCodegen在表达式生成失败时返回空寄存器
- StatementCodegen在语句生成失败时跳过当前语句
- FunctionCodegen在函数生成失败时生成空函数体
- 恢复原则：确保IR生成过程能够继续进行，收集所有错误

## 初始化和清理协议

### 1. 组件初始化顺序
IRGenerator构造函数按依赖顺序初始化各组件：
- 首先初始化输出流和IRBuilder
- 然后按依赖顺序初始化TypeMapper、ExpressionCodegen、StatementCodegen、FunctionCodegen
- 最后初始化BuiltinDeclarator
- 初始化策略：确保被依赖组件先于依赖组件初始化

### 2. 模块初始化
模块初始化过程包括：
- 通过IRBuilder输出目标三元组（riscv32-unknown-unknown-elf）
- 设置模块级别的输出格式
- 准备接收后续的函数和全局变量定义
- 初始化时机：在所有组件初始化完成后立即执行

### 3. 模块完成
模块完成阶段执行以下操作：
- 检查函数栈是否为空，确保所有函数正确关闭
- 验证模块结构的完整性
- 输出必要的模块结束信息
- 清理策略：LLVM IR不需要特殊的结束标记

## 输出管理协议

### 1. 文本输出格式
IRBuilder确保输出符合LLVM IR格式要求：
- 指令输出使用两个空格缩进
- 标签输出后跟冒号
- 每行输出以换行符结束
- 格式策略：严格遵循LLVM IR语法规范

### 2. 寄存器命名约定
IRBuilder采用统一的寄存器命名策略：
- 临时寄存器使用下划线加数字（_1, _2, _3...）
- 命名寄存器使用前缀加数字（var1, var2...）
- 命名原则：确保寄存器名称的唯一性和可读性
- 计数器管理：维护全局递增计数器

### 3. 基本块命名约定
IRBuilder提供一致的基本块命名规则：
- 基本块使用前缀加数字（bb1, bb2, if.then1...）
- 支持控制流相关的命名模式（if.then, if.else, loop.body等）
- 命名策略：反映基本块在控制流中的作用
- 计数器管理：维护独立的基本块计数器

## 性能优化接口

### 1. 缓存管理
TypeMapper实现类型映射缓存机制：
- 使用哈希表存储Rx类型到LLVM类型的映射
- 缓存策略：避免重复计算相同类型的映射
- 缓存管理：提供清理缓存的方法
- 性能优化：显著提高类型转换的效率

### 2. 表达式缓存
ExpressionCodegen实现表达式结果缓存：
- 使用哈希表存储表达式到寄存器的映射
- 缓存策略：避免重复生成相同表达式的IR
- 缓存管理：自动管理缓存生命周期
- 性能优化：减少重复计算，提高IR生成速度

### 3. 函数缓存
FunctionCodegen实现函数定义缓存：
- 缓存函数签名和类型信息
- 避免重复生成相同的函数声明
- 支持函数内联优化决策

### 4. 字符串优化
所有组件实现字符串操作优化：
- 使用字符串引用减少拷贝
- 预分配字符串缓冲区
- 批量输出减少I/O操作

## 总结

重新设计的组件交互接口具有以下特点：

1. **无 LLVM 依赖**：完全自定义的 IRBuilder 系统
2. **文本输出**：直接生成 LLVM IR 文本
3. **寄存器管理**：自动分配和命名寄存器
4. **基本块管理**：自动生成和管理基本块
5. **类型安全**：保持类型系统的正确性
6. **错误处理**：统一的错误报告机制
7. **性能优化**：缓存机制和高效的字符串操作

这种设计确保了 IR 生成器的独立性、可控性和与现有编译器架构的良好集成，同时满足了不使用 LLVM C++ API 的要求。
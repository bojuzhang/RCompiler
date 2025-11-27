# FunctionCodegen 函数代码生成器设计文档

## 概述

FunctionCodegen 组件负责将 Rx 语言的函数定义和调用转换为 LLVM IR 文本。它是 IR 生成阶段的核心组件之一，处理函数签名生成、参数传递、返回值处理、函数调用生成等所有与函数相关的代码生成任务。

## 设计目标

1. **函数定义生成**：生成函数签名、参数和函数体的 IR
2. **函数体处理**：处理函数体语句和表达式
3. **返回值处理**：处理基本返回值和尾表达式返回
4. **内置函数支持**：支持内置函数调用
5. **组件协作**：与其他 IR 生成组件协作

## 核心架构

### 函数处理分类

FunctionCodegen 处理以下函数相关任务：

1. **函数定义生成**
   - 函数签名生成
   - 参数处理和分配
   - 函数体生成
   - 返回值处理

2. **内置函数调用生成**
   - 内置函数调用
   - 与 ExpressionGenerator 协作处理普通函数调用

3. **参数传递处理**
   - 值传递
   - 引用传递
   - 结构体参数传递
   - 可变参数处理

4. **返回值处理**
   - 基本类型返回
   - 结构体返回
   - 引用返回
   - 多值返回（通过结构体）
   - 尾表达式返回

### 组件接口设计

**构造函数**
- 接收 IRBuilder、TypeMapper、ScopeTree 作为核心依赖
- 初始化函数上下文栈和内置函数缓存
- 设置组件的基本状态

**依赖组件设置**
- `setExpressionGenerator()`: 设置表达式生成器引用，用于处理返回值表达式
- `setStatementGenerator()`: 设置语句生成器引用，用于生成函数体语句

**主要生成接口**
- `generateFunction()`: 完整的函数生成入口，包括签名、参数、函数体和返回值处理
- `generateFunctionDeclaration()`: 仅生成函数声明，用于外部函数声明
- `generateFunctionBody()`: 仅生成函数体，用于函数定义的主体部分

**内置函数调用生成**
- `generateBuiltinCall()`: 生成内置函数调用的 IR 代码
- 支持各种内置函数：print、println、printInt、printlnInt、getString、getInt、malloc、memset、memcpy
- 提供与 ExpressionGenerator 的协作接口

**参数处理接口**
- `generateParameters()`: 生成函数参数的处理代码，包括栈分配和寄存器映射
- `generateArgumentLoad()`: 生成参数加载代码，处理值类型和引用类型
- `generateParameterAlloca()`: 为参数分配栈空间并存储参数值

**返回值处理接口**
- `generateReturnStatement()`: 生成 return 语句的 IR 代码
- `generateReturnValue()`: 生成返回值表达式的代码
- `generateReturnPhi()`: 生成返回值的 PHI 节点，处理多返回点
- `generateTailExpressionReturn()`: 处理尾表达式返回的情况
- `generateDefaultReturn()`: 生成默认返回值，用于无显式返回的情况

**函数签名生成接口**
- `generateFunctionSignature()`: 生成函数签名字符串
- `generateParameterList()`: 生成参数列表，包含名称和类型对
- `generateFunctionType()`: 生成函数类型字符串

**工具方法接口**
- `getFunctionName()`: 获取函数名称，处理名称修饰
- `getMangledName()`: 获取修饰后的函数名
- `isBuiltinFunction()`: 检查是否为内置函数
- `getBuiltinFunctionType()`: 获取内置函数的类型信息
- `generateArgument()`: 生成函数调用的参数处理代码
- `generateStructArgument()`: 处理结构体参数的特殊传递方式

**内部数据结构**
- **FunctionContext**: 函数上下文结构，包含函数名、返回类型、返回块、返回值寄存器等
- **functionStack**: 函数上下文栈，支持嵌套函数调用
- **currentFunction**: 当前函数上下文指针
- **builtinFunctionTypes**: 内置函数类型缓存，提高查找效率

**内部辅助方法**
- `enterFunction()`/`exitFunction()`: 进入/退出函数上下文
- `generatePrologue()`/`generateEpilogue()`: 生成函数序言和尾声
- `handleReturnValue()`: 处理返回值的通用逻辑
- `setupParameterScope()`: 设置参数作用域和符号表

## 实现策略

### 函数定义生成

函数定义生成采用以下策略：

1. **函数上下文管理**：
   - 进入函数时创建新的函数上下文
   - 设置函数名、返回类型等基本信息
   - 维护函数上下文栈以支持嵌套函数

2. **函数签名生成**：
   - 获取函数名和返回类型信息
   - 处理参数类型列表的生成
   - 生成符合 LLVM 语法的函数签名字符串
   - 缓存函数类型信息供后续使用

3. **参数列表生成**：
   - 为每个参数生成名称和类型对
   - 处理参数的命名规则和唯一性
   - 维护参数列表供函数定义使用

4. **函数结构生成**：
   - 创建入口基本块并设置标签
   - 生成函数序言（栈帧设置等）
   - 处理参数分配和作用域设置
   - 生成函数体和尾声
   - 结束函数定义并清理上下文

### 参数处理

参数处理采用以下策略：

1. **参数分配策略**：
   - 根据参数类型决定是否需要栈分配
   - 为需要栈空间的参数分配 alloca
   - 存储参数值到分配的栈空间
   - 记录参数寄存器映射关系

2. **参数类型处理**：
   - 区分值类型和引用类型的参数
   - 处理不同类型的参数传递方式
   - 确保参数类型与函数签名匹配

3. **参数作用域设置**：
   - 为函数创建新的作用域
   - 注册所有参数到符号表
   - 设置 IRBuilder 的变量寄存器映射
   - 确保参数在函数体内的正确访问

4. **参数加载优化**：
   - 对值类型参数进行必要的加载
   - 避免不必要的内存访问
   - 优化参数传递的性能

5. **参数验证**：
   - 验证参数数量和类型的正确性
   - 处理参数不匹配的错误情况
   - 提供详细的错误信息

### 函数体生成

函数体生成采用以下策略：

1. **空函数体处理**：
   - 检查函数体是否存在
   - 对于空函数体生成默认返回值
   - 确保函数总是有返回语句

2. **返回基本块设置**：
   - 创建统一的返回基本块
   - 分配返回值的 PHI 节点寄存器
   - 初始化返回状态标志

3. **函数体语句生成**：
   - 委托给 StatementGenerator 生成函数体语句
   - 处理函数体中的所有语句和表达式
   - 维护正确的控制流和作用域

4. **尾表达式返回处理**：
   - 检查函数体是否包含显式返回语句
   - 对于无显式返回的情况，尝试返回尾表达式的值
   - 处理块表达式的尾表达式提取

5. **返回值合并**：
   - 生成返回基本块的标签
   - 为多返回点生成 PHI 节点
   - 处理不同返回路径的值合并

6. **默认返回值生成**：
   - 根据返回类型生成适当的默认值
   - 处理基本类型、指针类型和结构体类型
   - 确保返回值的类型正确性

### 内置函数处理

内置函数处理采用以下策略：

1. **函数识别与分发**：
   - 从调用表达式中提取函数名
   - 根据函数名分发到相应的生成方法
   - 支持 print、println、printInt、printlnInt、getString、getInt、malloc、memset、memcpy 等内置函数

2. **参数处理策略**：
   - 通过 ExpressionGenerator 生成参数表达式
   - 获取参数的类型信息并进行必要的类型转换
   - 处理大结构体参数的特殊传递方式

3. **结构体参数优化**：
   - 对大型结构体使用引用传递
   - 分配临时空间并复制结构体内容
   - 返回指针而非值，提高传递效率

4. **类型安全保证**：
   - 验证参数类型与函数签名的匹配
   - 生成必要的类型转换代码
   - 确保内置函数调用的类型正确性

5. **错误处理**：
   - 对不支持的内置函数返回错误值
   - 提供详细的错误信息
   - 保持编译过程的稳定性

## 子组件通信

### 与 ExpressionGenerator 的协作

FunctionCodegen 与 ExpressionGenerator 通过以下方式进行协作：

1. **函数调用处理**：
   - ExpressionGenerator 负责处理 CallExpression
   - 对于内置函数，ExpressionGenerator 调用 FunctionCodegen::generateBuiltinCall
   - 对于普通函数调用，ExpressionGenerator 直接生成调用指令

2. **参数生成**：
   - ExpressionGenerator 调用 FunctionCodegen::generateArgument 处理参数
   - FunctionCodegen 提供参数类型转换和结构体参数处理

3. **返回值处理**：
   - FunctionCodegen 调用 ExpressionGenerator 生成返回值表达式
   - ExpressionGenerator 提供表达式类型信息

### 通信接口

### 通信接口设计

1. **双向引用设置**：
   - ExpressionGenerator 通过 setFunctionCodegen 设置 FunctionCodegen 引用
   - FunctionCodegen 通过 setExpressionGenerator 设置 ExpressionGenerator 引用
   - 建立组件间的双向通信机制

2. **函数调用分发**：
   - ExpressionGenerator 在 generateCallExpression 中检查函数类型
   - 对内置函数调用委托给 FunctionCodegen 处理
   - 对普通函数调用直接生成 call 指令

3. **参数处理协作**：
   - FunctionCodegen 提供参数处理的专业方法
   - 处理类型转换、结构体参数传递等复杂情况
   - 确保参数传递的正确性和效率

4. **返回值处理协作**：
   - FunctionCodegen 调用 ExpressionGenerator 生成返回值表达式
   - ExpressionGenerator 提供表达式类型信息用于类型转换
   - 协作处理复杂的返回值情况

### 返回值处理

返回值处理采用以下策略：

1. **返回语句处理**：
   - 检查当前函数上下文的有效性
   - 标记函数中有返回语句
   - 区分有返回值和无返回值的情况

2. **返回值类型转换**：
   - 获取返回值表达式的类型信息
   - 进行必要的类型转换以匹配函数返回类型
   - 确保返回值的类型正确性

3. **控制流处理**：
   - 生成跳转到返回基本块的指令
   - 在返回基本块中添加 PHI 节点的输入
   - 标记后续代码为不可达

4. **PHI 节点管理**：
   - 收集所有返回路径的值和基本块
   - 生成 PHI 节点合并多个返回值
   - 处理多返回点的复杂情况

## 函数上下文管理

### 上下文栈管理

函数上下文管理采用以下策略：

1. **上下文结构**：
   - FunctionContext 包含函数名、返回类型、返回块等信息
   - 维护参数列表和参数寄存器映射
   - 跟踪返回状态和返回值

2. **栈管理**：
   - enterFunction 创建新的函数上下文并压入栈
   - exitFunction 弹出当前函数上下文
   - currentFunction 始终指向栈顶元素

3. **状态跟踪**：
   - isInFunction 检查是否在函数上下文中
   - getCurrentFunction 获取当前函数上下文
   - 维护函数嵌套的正确性

### 返回值管理

返回值管理采用以下策略：

1. **返回值记录**：
   - 记录函数中有返回语句
   - 添加返回值到 PHI 节点输入列表
   - 维护返回值和来源基本块的对应关系

2. **多返回点处理**：
   - hasMultipleReturns 检查是否有多个返回点
   - 为多返回点生成 PHI 节点
   - 合并不同返回路径的值

3. **返回值类型统一**：
   - 确保所有返回值的类型一致
   - 进行必要的类型转换
   - 维护类型系统的完整性

## 测试策略

### 单元测试

单元测试采用以下策略：

1. **函数定义测试**：
   - 测试简单函数的 IR 生成
   - 验证函数签名、参数和返回值的正确性
   - 检查生成的 IR 符合 LLVM 语法

2. **内置函数测试**：
   - 测试各种内置函数的调用生成
   - 验证参数传递和返回值处理
   - 确保内置函数调用的正确性

3. **尾表达式返回测试**：
   - 测试尾表达式返回的生成
   - 验证尾表达式值的正确提取
   - 检查返回值的类型转换

### 集成测试

集成测试采用以下策略：

1. **复杂函数测试**：
   - 测试递归函数的 IR 生成
   - 验证复杂控制流的正确处理
   - 检查函数调用的嵌套处理

2. **IR 正确性验证**：
   - 使用 LLVM 验证器检查生成的 IR
   - 确保生成的 IR 可以被正确解析
   - 验证 IR 的语义正确性

3. **性能测试**：
   - 测试大型函数的生成性能
   - 验证内存使用的合理性
   - 检查生成效率的优化

## 使用示例

### 基本使用

1. **组件初始化**：
   - 创建 IRBuilder、TypeMapper、ScopeTree 等依赖组件
   - 初始化 FunctionCodegen 实例
   - 设置组件的基本状态

2. **依赖组件设置**：
   - 设置 ExpressionGenerator 和 StatementGenerator 引用
   - 建立组件间的双向通信
   - 确保组件协作的正确性

3. **函数生成**：
   - 生成函数定义的完整 IR
   - 处理函数签名、参数和函数体
   - 验证生成结果的正确性

### 组件协作示例

1. **函数调用处理**：
   - ExpressionGenerator 中的函数调用处理逻辑
   - 内置函数和普通函数调用的区分
   - 与 FunctionCodegen 的协作机制

2. **内置函数调用**：
   - 内置函数调用的识别和处理
   - 参数传递和返回值处理
   - 生成正确的 LLVM IR 代码

## 总结

FunctionCodegen 组件是 IR 生成阶段的核心组件，提供了函数生成功能：

1. **函数定义生成**：生成函数签名、参数和函数体的 IR
2. **函数体处理**：处理函数体语句和表达式
3. **返回值处理**：处理基本返回值和尾表达式返回
4. **内置函数支持**：支持内置函数调用
5. **组件协作**：与其他 IR 生成组件协作

通过 FunctionCodegen，IR 生成器可以将 Rx 语言函数正确地转换为 LLVM IR 代码，并与 ExpressionGenerator、StatementGenerator、IRBuilder、TypeMapper 等组件形成完整的代码生成流水线。
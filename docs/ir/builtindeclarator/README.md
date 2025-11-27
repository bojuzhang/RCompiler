# BuiltinDeclarator 内置函数声明器设计文档

## 概述

BuiltinDeclarator 组件负责生成 Rx 语言内置函数的 LLVM IR 声明。它是 IR 生成阶段的重要组件，确保所有内置函数在生成的 IR 中都有正确的声明，以便与 builtin.c 联合编译运行。该组件不生成内置函数的实现，只生成声明，实现由 builtin.c 提供。

## 设计目标

1. **完整的内置函数支持**：支持所有 Rx 语言内置函数的声明生成
2. **类型安全**：确保内置函数声明与实际实现类型匹配
3. **扩展性**：便于添加新的内置函数
4. **错误检测**：检测内置函数使用错误
5. **性能优化**：避免重复声明和类型查询

## 核心架构

### 内置函数分类

BuiltinDeclarator 处理以下类别的内置函数：

1. **输出函数**
   - `print(ptr)` - 打印字符串
   - `println(ptr)` - 打印字符串并换行
   - `printInt(i32)` - 打印整数
   - `printlnInt(i32)` - 打印整数并换行

2. **输入函数**
   - `getString()` - 获取字符串输入
   - `getInt()` - 获取整数输入

3. **内存管理函数**
   - `builtin_memset(ptr nocapture writeonly, i8, i32)` - 内存填充
   - `builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)` - 内存复制
   - `malloc(i32)` - 内存分配

4. **系统函数**
   - `exit(i32)` - 程序退出

5. **特殊函数**
   - 结构体相关函数（根据结构体定义动态生成）
   - 类型转换函数

### 组件接口设计

BuiltinDeclarator 组件提供以下核心接口：

**构造函数**
- 接收 IRBuilder 共享指针作为依赖，用于生成 IR 声明

**主要声明接口**
- `declareBuiltinFunctions()`: 批量声明所有标准内置函数
- `declareBuiltinFunction(name)`: 声明指定名称的单个内置函数

**具体函数声明方法**
- 针对每类内置函数提供专门的声明方法：
  - 输出函数：`declarePrint()`, `declarePrintln()`, `declarePrintInt()`, `declarePrintlnInt()`
  - 输入函数：`declareGetString()`, `declareGetInt()`
  - 内存管理函数：`declareMalloc()`, `declareMemset()`, `declareMemcpy()`
  - 系统函数：`declareExit()`

**结构体相关函数声明**
- `declareStructFunctions()`: 为指定结构体声明所有相关函数（构造函数、析构函数、比较函数）
- `declareStructConstructor()`: 声明结构体构造函数
- `declareStructDestructor()`: 声明结构体析构函数

**内置函数查询接口**
- `isBuiltinFunction()`: 检查指定函数是否为内置函数
- `getBuiltinFunctionType()`: 获取内置函数的完整 LLVM 类型签名
- `getBuiltinParameterTypes()`: 获取内置函数的参数类型列表
- `getBuiltinReturnType()`: 获取内置函数的返回类型

**类型检查接口**
- `isValidBuiltinCall()`: 验证内置函数调用的参数类型是否匹配
- `getBuiltinCallError()`: 获取类型不匹配时的详细错误信息

**自定义内置函数注册**
- `registerCustomBuiltin()`: 允许注册自定义内置函数，包括函数名、返回类型、参数类型和参数属性

**声明状态管理**
- `isFunctionDeclared()`: 检查函数是否已声明
- `markFunctionDeclared()`: 标记函数为已声明状态
- `clearDeclarationCache()`: 清理声明缓存

**内部数据结构**
- `BuiltinFunctionInfo`: 存储内置函数的完整信息，包括名称、返回类型、参数类型、参数属性、是否可变参数、是否已声明等
- 使用哈希表维护内置函数信息集合和已声明函数集合

**辅助方法**
- `initializeBuiltinFunctions()`: 初始化标准内置函数注册
- `registerStandardBuiltins()`: 注册所有标准内置函数
- `emitFunctionDeclaration()`: 通过 IRBuilder 输出函数声明
- `buildParameterList()`: 构建参数列表字符串
- `buildFunctionSignature()`: 构建完整的函数签名
- `validateParameterTypes()`: 验证参数类型匹配
- `isValidBuiltinName()`: 验证内置函数名称的有效性

## 实现策略

### 内置函数注册

BuiltinDeclarator 在构造时通过以下策略初始化内置函数注册：

1. **初始化流程**：构造函数接收 IRBuilder 依赖，立即调用初始化方法建立标准内置函数库

2. **标准内置函数注册**：按功能类别批量注册内置函数：
   - **输出函数**：注册 print 和 println（字符串输出）、printInt 和 printlnInt（整数输出），设置适当的参数属性如 nocapture readonly 和 signext
   - **输入函数**：注册 getString（返回字符串指针）和 getInt（返回带符号扩展的整数）
   - **内存管理函数**：注册 malloc（内存分配）、builtin_memset（内存填充）、builtin_memcpy（内存复制），设置正确的内存属性
   - **系统函数**：注册 exit（程序退出），标记为 noreturn 属性

3. **自定义内置函数注册机制**：通过 registerCustomBuiltin 方法动态添加新函数，包括：
   - 函数名称和返回类型
   - 参数类型列表和参数属性列表
   - 可变参数标志
   - 初始声明状态为未声明

4. **信息存储结构**：每个内置函数的信息存储在 BuiltinFunctionInfo 结构中，包含完整的类型和属性信息，便于后续声明生成和类型检查

### 主要声明流程

BuiltinDeclarator 采用以下策略生成内置函数声明：

1. **批量声明流程**：
   - 遍历所有注册的内置函数，检查声明状态
   - 对未声明的函数调用 emitFunctionDeclaration 生成声明
   - 更新函数声明状态并记录到已声明集合

2. **单个函数声明流程**：
   - 查找指定函数的信息，验证函数存在性
   - 检查声明状态，避免重复声明
   - 生成函数签名并通过 IRBuilder 输出
   - 处理未知函数的错误情况

3. **函数签名生成**：
   - 构建 LLVM 标准声明格式："declare dso_local 返回类型 @函数名(参数列表)"
   - 处理参数属性和类型的正确组合
   - 对无参数函数使用 void 类型
   - 确保参数间正确分隔

4. **声明输出机制**：
   - 通过 IRBuilder 的 emitInstruction 方法输出声明
   - 保持与整体 IR 生成的输出一致性

### 具体函数声明实现

BuiltinDeclarator 为每类内置函数提供专门的声明方法，这些方法采用统一的实现策略：

1. **简单委托模式**：每个具体函数声明方法都委托给 declareBuiltinFunction，传入对应的内置函数名称

2. **函数名称映射**：
   - 输出函数：print, println, printInt, printlnInt
   - 输入函数：getString, getInt
   - 内存管理函数：malloc, builtin_memset, builtin_memcpy
   - 系统函数：exit

3. **一致性保证**：所有具体声明方法都通过相同的声明流程，确保声明格式和状态管理的一致性

### 结构体相关函数声明

BuiltinDeclarator 为结构体提供动态函数声明功能，采用以下策略：

1. **结构体函数声明流程**：
   - 根据结构体名称和字段类型声明构造函数
   - 分析字段类型判断是否需要析构函数
   - 声明结构体比较函数

2. **构造函数声明**：
   - 生成标准命名："struct_结构体名_new"
   - 返回类型为结构体类型："%结构体名"
   - 参数类型与字段类型一一对应
   - 动态注册并立即声明

3. **析构函数声明**：
   - 生成标准命名："struct_结构体名_drop"
   - 接收结构体指针参数："%结构体名*"
   - 返回 void 类型
   - 仅在需要资源管理时声明

4. **比较函数声明**：
   - 生成标准命名："struct_结构体名_eq"
   - 接收两个结构体指针参数
   - 返回布尔值 i1
   - 用于结构体相等性比较

5. **析构函数需求判断**：
   - 检查字段类型是否包含指针或需要资源管理的类型
   - 递归分析复合类型的管理需求
   - 优化不必要的析构函数声明

### 内置函数查询

BuiltinDeclarator 提供完整的内置函数信息查询功能：

1. **函数存在性检查**：
   - 通过哈希表查找快速判断函数是否为内置函数
   - O(1) 时间复杂度的查询性能

2. **函数类型信息获取**：
   - 获取完整的 LLVM 函数签名，包括返回类型、参数类型和属性
   - 动态构建标准格式的函数声明字符串

3. **参数类型查询**：
   - 返回函数的参数类型列表
   - 用于类型检查和参数验证

4. **返回类型查询**：
   - 获取函数的返回类型
   - 支持表达式类型推导和验证

5. **错误处理**：
   - 对未知函数返回空值或默认值
   - 保持查询接口的健壮性

### 类型检查

BuiltinDeclarator 提供完整的内置函数调用类型检查机制：

1. **调用有效性验证**：
   - 检查函数是否为已注册的内置函数
   - 验证参数数量匹配（考虑可变参数函数）
   - 逐个验证参数类型兼容性

2. **参数类型验证**：
   - 逐个比较实际参数类型与期望参数类型
   - 使用类型兼容性规则进行宽松匹配
   - 支持隐式类型转换

3. **错误信息生成**：
   - 提供详细的错误描述，包括：
     - 未知函数错误
     - 参数数量不匹配错误
     - 参数类型不匹配错误（指明具体位置和类型）
   - 帮助开发者快速定位问题

4. **类型兼容性规则**：
   - **完全匹配**：相同类型直接兼容
   - **指针兼容**：所有指针类型在 LLVM 中互相兼容
   - **整数兼容**：所有整数类型支持隐式转换
   - **扩展性**：可添加更多兼容性规则

5. **类型识别辅助**：
   - 指针类型识别：检查 "*" 或 "ptr" 后缀
   - 整数类型识别：支持各种位宽的整数类型

### 声明状态管理

BuiltinDeclarator 采用双重状态管理机制跟踪函数声明状态：

1. **已声明函数集合**：
   - 使用哈希表快速查找已声明函数
   - 避免重复声明同一函数
   - 支持批量状态查询

2. **函数信息状态标记**：
   - 每个函数信息中包含 isDeclared 标志
   - 支持细粒度的状态管理
   - 便于调试和状态检查

3. **状态同步机制**：
   - 标记函数已声明时同步更新两个状态
   - 确保状态一致性
   - 处理异常情况下的状态恢复

4. **缓存清理功能**：
   - 清理所有声明状态，重新开始
   - 重置所有函数的声明标志
   - 支持重新生成 IR 的场景

## 错误处理

BuiltinDeclarator 提供完善的错误检测和报告机制：

1. **错误分类管理**：
   - 区分错误和警告消息
   - 维护错误状态标志
   - 支持错误历史记录

2. **错误报告方法**：
   - `reportError()`: 记录严重错误，设置错误标志
   - `reportWarning()`: 记录警告信息，不影响整体状态
   - 所有错误信息同时输出到 stderr 和内部存储

3. **专门错误类型**：
   - 重复声明警告：检测重复的函数声明
   - 无效函数名错误：验证函数名称合法性
   - 类型不匹配错误：详细的类型差异信息

4. **错误状态查询**：
   - `hasDeclarationErrors()`: 检查是否有错误发生
   - `getErrorMessages()`: 获取所有错误消息
   - `getWarningMessages()`: 获取所有警告消息

5. **错误清理**：
   - `clearErrors()`: 重置所有错误状态
   - 支持重新开始错误收集

## 错误恢复

BuiltinDeclarator 实现智能错误恢复机制：

1. **异常捕获机制**：
   - 在函数声明过程中捕获所有异常
   - 记录详细的错误信息和堆栈信息
   - 防止单个函数错误影响整体生成

2. **默认声明生成**：
   - 对未知函数生成通用声明："declare dso_local void @函数名(...)"
   - 使用可变参数格式确保兼容性
   - 允许编译继续进行

3. **恢复策略**：
   - 生成默认声明后记录警告信息
   - 保持声明状态的一致性
   - 为后续处理提供基础保障

4. **容错设计**：
   - 即使在错误情况下也尽力生成可用的 IR
   - 优先保证编译过程的完整性
   - 提供详细的错误信息帮助调试

## 性能优化

BuiltinDeclarator 提供多种性能优化策略：

1. **声明缓存机制**：
   - 缓存已生成的函数声明字符串
   - 避免重复计算相同函数的签名
   - 支持缓存开关控制

2. **缓存管理接口**：
   - `enableCache()`: 启用或禁用缓存机制
   - `getCachedDeclaration()`: 获取缓存的声明
   - `cacheDeclaration()`: 存储声明到缓存
   - `clearCache()`: 清理所有缓存

3. **内存优化**：
   - 使用哈希表实现 O(1) 查找性能
   - 按需缓存，仅缓存实际使用的声明
   - 支持缓存清理和内存回收

4. **批量声明优化**：
   - 按功能分组批量声明同类函数
   - 减少 IRBuilder 调用次数
   - 提高整体生成效率

### 批量声明优化

BuiltinDeclarator 采用分组批量声明策略提升性能：

1. **功能分组策略**：
   - **输出函数组**：print, println, printInt, printlnInt
   - **输入函数组**：getString, getInt
   - **内存管理函数组**：malloc, builtin_memset, builtin_memcpy
   - **系统函数组**：exit

2. **批量声明流程**：
   - 按功能组遍历函数列表
   - 检查每个函数的声明状态
   - 仅声明未声明的函数

3. **优化效果**：
   - 减少状态检查次数
   - 提高缓存命中率
   - 降低函数调用开销

4. **扩展性设计**：
   - 支持动态添加新的函数组
   - 便于维护和管理函数分类
   - 可根据使用频率优化声明顺序

## 测试策略

BuiltinDeclarator 采用全面的测试策略确保组件正确性：

### 单元测试

1. **基本声明测试**：
   - 验证所有标准内置函数正确声明
   - 检查函数类型和参数信息的准确性
   - 确保声明状态管理正确

2. **类型检查测试**：
   - 测试正确函数调用的类型验证
   - 测试错误函数调用的拒绝机制
   - 验证错误信息的准确性和有用性

3. **结构体函数测试**：
   - 验证结构体相关函数的动态生成
   - 检查构造函数和比较函数的类型正确性
   - 测试析构函数的需求判断逻辑

### 集成测试

1. **与 IRBuilder 集成测试**：
   - 验证生成的 IR 声明格式正确
   - 测试与 IRBuilder 的协作流程
   - 确保输出符合 LLVM 语法规范

2. **完整编译流程测试**：
   - 在完整编译环境中测试组件
   - 验证与 builtin.c 的联合编译兼容性
   - 测试实际运行时的正确性

3. **性能测试**：
   - 测试大量函数声明的性能
   - 验证缓存机制的效果
   - 测试内存使用效率

### 错误处理测试

1. **错误恢复测试**：
   - 测试各种错误情况的恢复能力
   - 验证默认声明的生成
   - 确保错误不会中断整体流程

2. **边界条件测试**：
   - 测试空函数名、无效类型等边界情况
   - 验证异常输入的处理
   - 确保组件的健壮性

### 集成测试

```cpp
// 与 IRBuilder 集成测试
TEST(BuiltinDeclaratorIntegrationTest, IRBuilderIntegration) {
    auto irBuilder = std::make_shared<IRBuilder>();
    BuiltinDeclarator declarator(irBuilder);
    
    // 声明内置函数
    declarator.declareBuiltinFunctions();
    
    // 验证 IR 输出
    std::string ir = irBuilder->getOutput();
    EXPECT_TRUE(ir.find("declare dso_local void @print(ptr)") != std::string::npos);
    EXPECT_TRUE(ir.find("declare dso_local void @printlnInt(i32)") != std::string::npos);
    EXPECT_TRUE(ir.find("declare dso_local ptr @malloc(i32)") != std::string::npos);
}

// 完整编译流程测试
TEST(BuiltinDeclaratorIntegrationTest, FullCompilation) {
    // 创建完整的 IR 生成环境
    auto irBuilder = std::make_shared<IRBuilder>();
    auto typeMapper = std::make_shared<TypeMapper>();
    auto scopeTree = std::make_shared<ScopeTree>();
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 声明内置函数
    declarator.declareBuiltinFunctions();
    
    // 生成使用内置函数的代码
    // ... 生成调用 printInt 的代码 ...
    
    // 验证生成的 IR 可以被 LLVM 解析
    std::string ir = irBuilder->getOutput();
    EXPECT_TRUE(verifyLLVMIR(ir));
}
```

## 使用示例

### 基本使用

BuiltinDeclarator 的基本使用流程：

1. **组件初始化**：
   - 创建 IRBuilder 实例
   - 构造 BuiltinDeclarator 并传入 IRBuilder 依赖

2. **批量声明**：
   - 调用 declareBuiltinFunctions() 声明所有标准内置函数
   - 自动生成符合 LLVM 语法的函数声明

3. **函数查询**：
   - 使用 isBuiltinFunction() 检查函数是否为内置函数
   - 获取函数的返回类型和参数类型信息
   - 用于后续的类型检查和代码生成

### 高级使用

BuiltinDeclarator 的高级功能使用：

1. **结构体函数声明**：
   - 为自定义结构体动态生成构造函数、比较函数等
   - 根据字段类型自动判断是否需要析构函数
   - 生成标准命名的结构体相关函数

2. **自定义函数注册**：
   - 通过 registerCustomBuiltin() 添加项目特定的内置函数
   - 支持自定义返回类型、参数类型和属性
   - 立即声明新注册的函数

3. **类型检查集成**：
   - 在代码生成前验证函数调用的类型正确性
   - 获取详细的类型不匹配错误信息
   - 集成到编译器的类型检查流程

4. **性能优化**：
   - 启用声明缓存减少重复计算
   - 使用批量声明优化提升效率
   - 根据项目需求调整优化策略

### 错误处理

BuiltinDeclarator 的错误处理机制：

1. **错误状态监控**：
   - 检查 hasDeclarationErrors() 判断是否有错误发生
   - 获取详细的错误和警告信息列表
   - 用于编译器前端显示错误信息

2. **错误恢复**：
   - 即使在错误情况下也能生成可用的 IR
   - 为未知函数生成默认声明
   - 保证编译过程的连续性

3. **调试支持**：
   - 详细的错误信息帮助定位问题
   - 警告信息提示潜在问题
   - 支持错误状态的清理和重置

## 总结

BuiltinDeclarator 组件是 IR 生成阶段的重要组件，提供了完整的内置函数声明功能：

1. **完整的内置函数支持**：支持所有 Rx 语言内置函数的声明生成
2. **类型安全**：确保内置函数声明与实际实现类型匹配
3. **扩展性**：便于添加新的内置函数和自定义函数
4. **错误检测**：完善的内置函数使用错误检测
5. **性能优化**：声明缓存和批量声明优化
6. **结构体支持**：动态生成结构体相关函数声明
7. **易于集成**：与 IRBuilder 和其他组件无缝集成

通过 BuiltinDeclarator，IR 生成器可以确保所有内置函数在生成的 LLVM IR 中都有正确的声明，为与 builtin.c 的联合编译提供必要的基础设施。
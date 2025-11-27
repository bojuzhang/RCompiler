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

```cpp
class BuiltinDeclarator {
public:
    // 构造函数
    BuiltinDeclarator(std::shared_ptr<IRBuilder> irBuilder);
    
    // 主要声明接口
    void declareBuiltinFunctions();
    void declareBuiltinFunction(const std::string& name);
    
    // 具体函数声明
    void declarePrint();
    void declarePrintln();
    void declarePrintInt();
    void declarePrintlnInt();
    void declareGetString();
    void declareGetInt();
    void declareMalloc();
    void declareMemset();
    void declareMemcpy();
    void declareExit();
    
    // 结构体相关函数声明
    void declareStructFunctions(const std::string& structName, 
                               const std::vector<std::string>& fieldTypes);
    void declareStructConstructor(const std::string& structName,
                                  const std::vector<std::string>& paramTypes);
    void declareStructDestructor(const std::string& structName);
    
    // 内置函数查询
    bool isBuiltinFunction(const std::string& name) const;
    std::string getBuiltinFunctionType(const std::string& name) const;
    std::vector<std::string> getBuiltinParameterTypes(const std::string& name) const;
    std::string getBuiltinReturnType(const std::string& name) const;
    
    // 类型检查
    bool isValidBuiltinCall(const std::string& name, 
                           const std::vector<std::string>& argTypes) const;
    std::string getBuiltinCallError(const std::string& name,
                                   const std::vector<std::string>& argTypes) const;
    
    // 自定义内置函数注册
    void registerCustomBuiltin(const std::string& name,
                              const std::string& returnType,
                              const std::vector<std::string>& paramTypes,
                              const std::vector<std::string>& paramAttrs = {});
    
    // 声明状态管理
    bool isFunctionDeclared(const std::string& name) const;
    void markFunctionDeclared(const std::string& name);
    void clearDeclarationCache();

private:
    std::shared_ptr<IRBuilder> irBuilder;
    
    // 内置函数信息
    struct BuiltinFunctionInfo {
        std::string name;
        std::string returnType;
        std::vector<std::string> paramTypes;
        std::vector<std::string> paramAttrs;
        bool isVariadic;
        bool isDeclared;
    };
    
    std::unordered_map<std::string, BuiltinFunctionInfo> builtinFunctions;
    std::unordered_set<std::string> declaredFunctions;
    
    // 初始化方法
    void initializeBuiltinFunctions();
    void registerStandardBuiltins();
    
    // 声明辅助方法
    void emitFunctionDeclaration(const BuiltinFunctionInfo& info);
    std::string buildParameterList(const std::vector<std::string>& paramTypes,
                                   const std::vector<std::string>& paramAttrs) const;
    std::string buildFunctionSignature(const std::string& name,
                                      const std::string& returnType,
                                      const std::vector<std::string>& paramTypes,
                                      const std::vector<std::string>& paramAttrs) const;
    
    // 类型验证方法
    bool validateParameterTypes(const std::string& name,
                              const std::vector<std::string>& argTypes) const;
    bool isValidBuiltinName(const std::string& name) const;
    
    // 错误处理
    void reportError(const std::string& message);
    void reportWarning(const std::string& message);
};
```

## 实现策略

### 内置函数注册

```cpp
BuiltinDeclarator::BuiltinDeclarator(std::shared_ptr<IRBuilder> irBuilder)
    : irBuilder(irBuilder) {
    initializeBuiltinFunctions();
}

void BuiltinDeclarator::initializeBuiltinFunctions() {
    registerStandardBuiltins();
}

void BuiltinDeclarator::registerStandardBuiltins() {
    // 输出函数
    registerCustomBuiltin("print", "void", {"ptr"}, {"nocapture readonly"});
    registerCustomBuiltin("println", "void", {"ptr"}, {"nocapture readonly"});
    registerCustomBuiltin("printInt", "void", {"i32"}, {"signext"});
    registerCustomBuiltin("printlnInt", "void", {"i32"}, {"signext"});
    
    // 输入函数
    registerCustomBuiltin("getString", "ptr", {});
    registerCustomBuiltin("getInt", "i32", {"signext"});
    
    // 内存管理函数
    registerCustomBuiltin("malloc", "ptr", {"i32"});
    registerCustomBuiltin("builtin_memset", "ptr", 
                         {"ptr nocapture writeonly", "i8", "i32"});
    registerCustomBuiltin("builtin_memcpy", "ptr", 
                         {"ptr nocapture writeonly", "ptr nocapture readonly", "i32"});
    
    // 系统函数
    registerCustomBuiltin("exit", "void", {"i32"}, {"noreturn"});
}

void BuiltinDeclarator::registerCustomBuiltin(const std::string& name,
                                             const std::string& returnType,
                                             const std::vector<std::string>& paramTypes,
                                             const std::vector<std::string>& paramAttrs) {
    BuiltinFunctionInfo info;
    info.name = name;
    info.returnType = returnType;
    info.paramTypes = paramTypes;
    info.paramAttrs = paramAttrs;
    info.isVariadic = false;
    info.isDeclared = false;
    
    builtinFunctions[name] = info;
}
```

### 主要声明流程

```cpp
void BuiltinDeclarator::declareBuiltinFunctions() {
    for (auto& [name, info] : builtinFunctions) {
        if (!info.isDeclared) {
            emitFunctionDeclaration(info);
            info.isDeclared = true;
            declaredFunctions.insert(name);
        }
    }
}

void BuiltinDeclarator::declareBuiltinFunction(const std::string& name) {
    auto it = builtinFunctions.find(name);
    if (it != builtinFunctions.end()) {
        if (!it->second.isDeclared) {
            emitFunctionDeclaration(it->second);
            it->second.isDeclared = true;
            declaredFunctions.insert(name);
        }
    } else {
        reportError("Unknown builtin function: " + name);
    }
}

void BuiltinDeclarator::emitFunctionDeclaration(const BuiltinFunctionInfo& info) {
    std::string signature = buildFunctionSignature(info.name, info.returnType,
                                                 info.paramTypes, info.paramAttrs);
    
    // 使用 IRBuilder 输出声明
    irBuilder->emitInstruction(signature);
}

std::string BuiltinDeclarator::buildFunctionSignature(const std::string& name,
                                                     const std::string& returnType,
                                                     const std::vector<std::string>& paramTypes,
                                                     const std::vector<std::string>& paramAttrs) const {
    std::string signature = "declare dso_local " + returnType + " @" + name + "(";
    
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) signature += ", ";
        
        // 添加参数属性
        if (i < paramAttrs.size() && !paramAttrs[i].empty()) {
            signature += paramAttrs[i] + " ";
        }
        
        signature += paramTypes[i];
    }
    
    if (paramTypes.empty()) {
        signature += "void";
    }
    
    signature += ")";
    
    return signature;
}
```

### 具体函数声明实现

```cpp
void BuiltinDeclarator::declarePrint() {
    declareBuiltinFunction("print");
}

void BuiltinDeclarator::declarePrintln() {
    declareBuiltinFunction("println");
}

void BuiltinDeclarator::declarePrintInt() {
    declareBuiltinFunction("printInt");
}

void BuiltinDeclarator::declarePrintlnInt() {
    declareBuiltinFunction("printlnInt");
}

void BuiltinDeclarator::declareGetString() {
    declareBuiltinFunction("getString");
}

void BuiltinDeclarator::declareGetInt() {
    declareBuiltinFunction("getInt");
}

void BuiltinDeclarator::declareMalloc() {
    declareBuiltinFunction("malloc");
}

void BuiltinDeclarator::declareMemset() {
    declareBuiltinFunction("builtin_memset");
}

void BuiltinDeclarator::declareMemcpy() {
    declareBuiltinFunction("builtin_memcpy");
}

void BuiltinDeclarator::declareExit() {
    declareBuiltinFunction("exit");
}
```

### 结构体相关函数声明

```cpp
void BuiltinDeclarator::declareStructFunctions(const std::string& structName,
                                               const std::vector<std::string>& fieldTypes) {
    // 声明结构体构造函数
    declareStructConstructor(structName, fieldTypes);
    
    // 声明结构体析构函数（如果需要）
    if (needsDestructor(fieldTypes)) {
        declareStructDestructor(structName);
    }
    
    // 声明结构体比较函数
    declareStructComparison(structName, fieldTypes);
}

void BuiltinDeclarator::declareStructConstructor(const std::string& structName,
                                                const std::vector<std::string>& paramTypes) {
    std::string constructorName = "struct_" + structName + "_new";
    std::string returnType = "%" + structName;
    
    registerCustomBuiltin(constructorName, returnType, paramTypes);
    declareBuiltinFunction(constructorName);
}

void BuiltinDeclarator::declareStructDestructor(const std::string& structName) {
    std::string destructorName = "struct_" + structName + "_drop";
    std::string paramType = "%" + structName + "*";
    
    registerCustomBuiltin(destructorName, "void", {paramType});
    declareBuiltinFunction(destructorName);
}

void BuiltinDeclarator::declareStructComparison(const std::string& structName,
                                               const std::vector<std::string>& fieldTypes) {
    std::string eqName = "struct_" + structName + "_eq";
    std::string paramType = "%" + structName + "*";
    
    registerCustomBuiltin(eqName, "i1", {paramType, paramType});
    declareBuiltinFunction(eqName);
}

bool BuiltinDeclarator::needsDestructor(const std::vector<std::string>& fieldTypes) const {
    // 检查字段类型是否需要析构
    for (const auto& fieldType : fieldTypes) {
        if (isManagedType(fieldType)) {
            return true;
        }
    }
    return false;
}

bool BuiltinDeclarator::isManagedType(const std::string& type) const {
    // 检查是否为需要管理的类型（如包含指针的类型）
    return type.find("*") != std::string::npos || 
           type.find("ptr") != std::string::npos;
}
```

### 内置函数查询

```cpp
bool BuiltinDeclarator::isBuiltinFunction(const std::string& name) const {
    return builtinFunctions.find(name) != builtinFunctions.end();
}

std::string BuiltinDeclarator::getBuiltinFunctionType(const std::string& name) const {
    auto it = builtinFunctions.find(name);
    if (it != builtinFunctions.end()) {
        return buildFunctionSignature(it->second.name, it->second.returnType,
                                    it->second.paramTypes, it->second.paramAttrs);
    }
    return "";
}

std::vector<std::string> BuiltinDeclarator::getBuiltinParameterTypes(const std::string& name) const {
    auto it = builtinFunctions.find(name);
    if (it != builtinFunctions.end()) {
        return it->second.paramTypes;
    }
    return {};
}

std::string BuiltinDeclarator::getBuiltinReturnType(const std::string& name) const {
    auto it = builtinFunctions.find(name);
    if (it != builtinFunctions.end()) {
        return it->second.returnType;
    }
    return "";
}
```

### 类型检查

```cpp
bool BuiltinDeclarator::isValidBuiltinCall(const std::string& name,
                                           const std::vector<std::string>& argTypes) const {
    auto it = builtinFunctions.find(name);
    if (it == builtinFunctions.end()) {
        return false;
    }
    
    const auto& info = it->second;
    
    // 检查参数数量
    if (!info.isVariadic && argTypes.size() != info.paramTypes.size()) {
        return false;
    }
    
    // 检查参数类型
    return validateParameterTypes(name, argTypes);
}

bool BuiltinDeclarator::validateParameterTypes(const std::string& name,
                                              const std::vector<std::string>& argTypes) const {
    auto it = builtinFunctions.find(name);
    if (it == builtinFunctions.end()) {
        return false;
    }
    
    const auto& info = it->second;
    
    for (size_t i = 0; i < argTypes.size() && i < info.paramTypes.size(); ++i) {
        if (!areTypesCompatible(argTypes[i], info.paramTypes[i])) {
            return false;
        }
    }
    
    return true;
}

std::string BuiltinDeclarator::getBuiltinCallError(const std::string& name,
                                                    const std::vector<std::string>& argTypes) const {
    auto it = builtinFunctions.find(name);
    if (it == builtinFunctions.end()) {
        return "Unknown builtin function: " + name;
    }
    
    const auto& info = it->second;
    
    // 检查参数数量
    if (!info.isVariadic && argTypes.size() != info.paramTypes.size()) {
        return "Expected " + std::to_string(info.paramTypes.size()) + 
               " arguments, got " + std::to_string(argTypes.size());
    }
    
    // 检查参数类型
    for (size_t i = 0; i < argTypes.size() && i < info.paramTypes.size(); ++i) {
        if (!areTypesCompatible(argTypes[i], info.paramTypes[i])) {
            return "Argument " + std::to_string(i + 1) + 
                   " type mismatch: expected " + info.paramTypes[i] + 
                   ", got " + argTypes[i];
        }
    }
    
    return "";
}

bool BuiltinDeclarator::areTypesCompatible(const std::string& actual, 
                                          const std::string& expected) const {
    // 简单的类型兼容性检查
    if (actual == expected) {
        return true;
    }
    
    // 指针类型兼容性
    if (isPointerType(actual) && isPointerType(expected)) {
        return true; // 所有指针类型在 LLVM 中都是兼容的
    }
    
    // 整数类型兼容性
    if (isIntegerType(actual) && isIntegerType(expected)) {
        return true; // LLVM 支持整数类型的隐式转换
    }
    
    return false;
}

bool BuiltinDeclarator::isPointerType(const std::string& type) const {
    return type.find("*") != std::string::npos || type == "ptr";
}

bool BuiltinDeclarator::isIntegerType(const std::string& type) const {
    return type == "i1" || type == "i8" || type == "i16" || 
           type == "i32" || type == "i64" || type == "u32" || type == "u64";
}
```

### 声明状态管理

```cpp
bool BuiltinDeclarator::isFunctionDeclared(const std::string& name) const {
    return declaredFunctions.find(name) != declaredFunctions.end();
}

void BuiltinDeclarator::markFunctionDeclared(const std::string& name) {
    declaredFunctions.insert(name);
    
    auto it = builtinFunctions.find(name);
    if (it != builtinFunctions.end()) {
        it->second.isDeclared = true;
    }
}

void BuiltinDeclarator::clearDeclarationCache() {
    declaredFunctions.clear();
    
    for (auto& [name, info] : builtinFunctions) {
        info.isDeclared = false;
    }
}
```

## 错误处理

### 错误检测

```cpp
class BuiltinDeclarator {
private:
    bool hasErrors;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;
    
public:
    void reportError(const std::string& message) {
        hasErrors = true;
        errorMessages.push_back("Builtin Declaration Error: " + message);
        std::cerr << errorMessages.back() << std::endl;
    }
    
    void reportWarning(const std::string& message) {
        warningMessages.push_back("Builtin Declaration Warning: " + message);
        std::cerr << warningMessages.back() << std::endl;
    }
    
    void reportDuplicateDeclaration(const std::string& name) {
        reportWarning("Duplicate builtin function declaration: " + name);
    }
    
    void reportInvalidBuiltinName(const std::string& name) {
        reportError("Invalid builtin function name: " + name);
    }
    
    void reportTypeMismatch(const std::string& name, 
                           const std::string& expectedType,
                           const std::string& actualType) {
        reportError("Type mismatch for builtin " + name + 
                   ": expected " + expectedType + ", got " + actualType);
    }
    
    bool hasDeclarationErrors() const {
        return hasErrors;
    }
    
    const std::vector<std::string>& getErrorMessages() const {
        return errorMessages;
    }
    
    const std::vector<std::string>& getWarningMessages() const {
        return warningMessages;
    }
    
    void clearErrors() {
        hasErrors = false;
        errorMessages.clear();
        warningMessages.clear();
    }
};
```

### 错误恢复

```cpp
void BuiltinDeclarator::declareBuiltinFunctionWithErrorRecovery(const std::string& name) {
    try {
        declareBuiltinFunction(name);
    } catch (const std::exception& e) {
        reportError("Failed to declare builtin function " + name + ": " + std::string(e.what()));
        
        // 生成默认声明
        generateDefaultDeclaration(name);
    }
}

void BuiltinDeclarator::generateDefaultDeclaration(const std::string& name) {
    // 为未知内置函数生成默认声明
    std::string defaultDecl = "declare dso_local void @" + name + "(...)";
    irBuilder->emitInstruction(defaultDecl);
    
    reportWarning("Generated default declaration for unknown builtin: " + name);
}
```

## 性能优化

### 声明缓存

```cpp
class BuiltinDeclarator {
private:
    // 声明缓存
    std::unordered_map<std::string, std::string> declarationCache;
    bool cacheEnabled;
    
public:
    void enableCache(bool enabled = true) {
        cacheEnabled = enabled;
    }
    
    std::string getCachedDeclaration(const std::string& name) const {
        if (!cacheEnabled) {
            return "";
        }
        
        auto it = declarationCache.find(name);
        return (it != declarationCache.end()) ? it->second : "";
    }
    
    void cacheDeclaration(const std::string& name, const std::string& declaration) {
        if (cacheEnabled) {
            declarationCache[name] = declaration;
        }
    }
    
    void clearCache() {
        declarationCache.clear();
    }
};
```

### 批量声明优化

```cpp
void BuiltinDeclarator::declareBuiltinFunctionsOptimized() {
    // 按类型分组声明
    std::vector<std::string> outputFunctions = {"print", "println", "printInt", "printlnInt"};
    std::vector<std::string> inputFunctions = {"getString", "getInt"};
    std::vector<std::string> memoryFunctions = {"malloc", "builtin_memset", "builtin_memcpy"};
    std::vector<std::string> systemFunctions = {"exit"};
    
    // 批量声明
    declareFunctionGroup(outputFunctions);
    declareFunctionGroup(inputFunctions);
    declareFunctionGroup(memoryFunctions);
    declareFunctionGroup(systemFunctions);
}

void BuiltinDeclarator::declareFunctionGroup(const std::vector<std::string>& functions) {
    for (const auto& funcName : functions) {
        if (!isFunctionDeclared(funcName)) {
            declareBuiltinFunction(funcName);
        }
    }
}
```

## 测试策略

### 单元测试

```cpp
// 基本声明测试
TEST(BuiltinDeclaratorTest, BasicDeclaration) {
    auto irBuilder = std::make_shared<IRBuilder>();
    BuiltinDeclarator declarator(irBuilder);
    
    declarator.declareBuiltinFunctions();
    
    // 验证标准内置函数已声明
    EXPECT_TRUE(declarator.isFunctionDeclared("print"));
    EXPECT_TRUE(declarator.isFunctionDeclared("printlnInt"));
    EXPECT_TRUE(declarator.isFunctionDeclared("malloc"));
    
    // 验证函数类型
    EXPECT_EQ(declarator.getBuiltinReturnType("printInt"), "void");
    auto paramTypes = declarator.getBuiltinParameterTypes("printInt");
    ASSERT_EQ(paramTypes.size(), 1);
    EXPECT_EQ(paramTypes[0], "i32");
}

// 类型检查测试
TEST(BuiltinDeclaratorTest, TypeChecking) {
    auto irBuilder = std::make_shared<IRBuilder>();
    BuiltinDeclarator declarator(irBuilder);
    
    // 正确的调用
    EXPECT_TRUE(declarator.isValidBuiltinCall("printInt", {"i32"}));
    EXPECT_TRUE(declarator.isValidBuiltinCall("getString", {}));
    
    // 错误的调用
    EXPECT_FALSE(declarator.isValidBuiltinCall("printInt", {"ptr"}));
    EXPECT_FALSE(declarator.isValidBuiltinCall("getString", {"i32"}));
    
    // 错误信息
    std::string error = declarator.getBuiltinCallError("printInt", {"ptr"});
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(error.find("type mismatch") != std::string::npos);
}

// 结构体函数测试
TEST(BuiltinDeclaratorTest, StructFunctions) {
    auto irBuilder = std::make_shared<IRBuilder>();
    BuiltinDeclarator declarator(irBuilder);
    
    std::vector<std::string> fieldTypes = {"i32", "ptr"};
    declarator.declareStructFunctions("Point", fieldTypes);
    
    // 验证结构体函数已声明
    EXPECT_TRUE(declarator.isFunctionDeclared("struct_Point_new"));
    EXPECT_TRUE(declarator.isFunctionDeclared("struct_Point_eq"));
    
    // 验证函数类型
    EXPECT_EQ(declarator.getBuiltinReturnType("struct_Point_new"), "%Point");
    auto params = declarator.getBuiltinParameterTypes("struct_Point_new");
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "i32");
    EXPECT_EQ(params[1], "ptr");
}
```

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

```cpp
// 创建 BuiltinDeclarator
auto irBuilder = std::make_shared<IRBuilder>();
BuiltinDeclarator builtinDeclarator(irBuilder);

// 声明所有内置函数
builtinDeclarator.declareBuiltinFunctions();
// 输出：
// declare dso_local void @print(ptr)
// declare dso_local void @println(ptr)
// declare dso_local void @printInt(i32)
// declare dso_local void @printlnInt(i32)
// declare dso_local ptr @getString()
// declare dso_local i32 @getInt()
// declare dso_local ptr @malloc(i32)
// declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
// declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
// declare dso_local void @exit(i32)

// 检查函数是否为内置函数
if (builtinDeclarator.isBuiltinFunction("printInt")) {
    std::string returnType = builtinDeclarator.getBuiltinReturnType("printInt");
    auto paramTypes = builtinDeclarator.getBuiltinParameterTypes("printInt");
    // returnType = "void", paramTypes = {"i32"}
}
```

### 高级使用

```cpp
// 声明结构体相关函数
std::vector<std::string> fieldTypes = {"i32", "i32"};
builtinDeclarator.declareStructFunctions("Point", fieldTypes);
// 输出：
// declare dso_local %Point @struct_Point_new(i32, i32)
// declare dso_local i1 @struct_Point_eq(%Point*, %Point*)

// 注册自定义内置函数
builtinDeclarator.registerCustomBuiltin("custom_func", "i32", {"ptr", "i32"});
builtinDeclarator.declareBuiltinFunction("custom_func");
// 输出：
// declare dso_local i32 @custom_func(ptr, i32)

// 类型检查
std::vector<std::string> args = {"i32"};
if (builtinDeclarator.isValidBuiltinCall("printInt", args)) {
    // 调用有效
} else {
    std::string error = builtinDeclarator.getBuiltinCallError("printInt", args);
    std::cerr << error << std::endl;
}

// 启用缓存优化
builtinDeclarator.enableCache(true);
builtinDeclarator.declareBuiltinFunctions(); // 使用缓存
```

### 错误处理

```cpp
// 检查声明错误
if (builtinDeclarator.hasDeclarationErrors()) {
    for (const auto& error : builtinDeclarator.getErrorMessages()) {
        std::cerr << error << std::endl;
    }
}

// 获取警告信息
for (const auto& warning : builtinDeclarator.getWarningMessages()) {
    std::cout << warning << std::endl;
}

// 清理错误状态
builtinDeclarator.clearErrors();
```

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
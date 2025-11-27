# IRGenerator 主控制器设计文档

## 概述

IRGenerator 是 Rx 语言编译器 IR 生成模块的主控制器，负责协调整个 IR 生成过程。它作为编译器的最后阶段，接收来自语义分析阶段的 AST 和符号信息，通过协调各个子组件将 Rx 语言代码转换为 LLVM-15 兼容的 IR 文本，并直接输出到 stdout 与 builtin.c 联合编译运行。

## 设计目标

1. **统一协调**：协调整个 IR 生成过程，管理所有子组件的交互
2. **直接输出**：生成 LLVM IR 文本并直接输出到 stdout
3. **错误处理**：提供统一的错误检测、报告和恢复机制
4. **模块管理**：管理各个子组件的生命周期和依赖关系
5. **性能优化**：确保高效的 IR 生成和输出

## 核心架构

### 主要职责

IRGenerator 作为主控制器，承担以下核心职责：

1. **模块初始化**
   - 创建和初始化所有子组件
   - 设置目标平台和输出格式
   - 声明内置函数
   - 建立组件间的依赖关系

2. **AST 遍历**
   - 遍历顶层 AST 节点
   - 分发任务给相应的子组件
   - 管理遍历顺序和上下文

3. **组件协调**
   - 解决组件间的循环依赖
   - 管理共享资源（如 IRBuilder、TypeMapper）
   - 协调控制流和作用域管理

4. **输出管理**
   - 管理 IR 文本输出流
   - 确保输出格式正确
   - 处理输出缓冲和刷新

5. **错误处理**
   - 统一的错误收集和报告
   - 错误恢复策略
   - 生成过程的完整性检查

### 组件依赖关系

```
┌─────────────────┐
│   IRGenerator   │ (主控制器)
│                 │
│ ┌─────────────┐ │
│ │ IRBuilder   │ │ (IR 构建器)
│ └─────────────┘ │
│                 │
│ ┌─────────────┐ │
│ │ TypeMapper  │ │ (类型映射器)
│ └─────────────┘ │
│                 │
│ ┌─────────────┐ │    ┌─────────────┐
│ │Expression   │ │◄──►│ Statement   │ │
│ │Generator    │ │    │ Generator   │ │
│ └─────────────┘ │    └─────────────┘ │
│                 │
│ ┌─────────────┐ │
│ │Function     │ │ (函数代码生成器)
│ │Codegen      │ │
│ └─────────────┘ │
│                 │
│ ┌─────────────┐ │
│ │Builtin      │ │ (内置函数声明器)
│ │Declarator   │ │
│ └─────────────┘ │
└─────────────────┘
```

## 接口设计

### 主接口

```cpp
class IRGenerator {
public:
    // 构造函数
    IRGenerator(const SymbolTable& symbolTable, 
               const TypeChecker& typeChecker,
               std::ostream& outputStream = std::cout);
    
    // 主要生成接口
    bool generateIR(const std::vector<std::unique_ptr<Item>>& items);
    
    // 输出获取
    std::string getIROutput() const;
    void flushOutput();
    
    // 错误处理
    bool hasErrors() const;
    const std::vector<std::string>& getErrors() const;
    void clearErrors();
    
    // 组件访问接口
    IRBuilder& getBuilder() { return *builder; }
    TypeMapper& getTypeMapper() { return *typeMapper; }
    ExpressionGenerator& getExpressionGenerator() { return *expressionGenerator; }
    StatementGenerator& getStatementGenerator() { return *statementGenerator; }
    FunctionCodegen& getFunctionCodegen() { return *functionCodegen; }
    BuiltinDeclarator& getBuiltinDeclarator() { return *builtinDeclarator; }
    
    // 符号表访问
    const SymbolInfo* getSymbol(const std::string& name) const;
    const Type* getSymbolType(const std::string& name) const;
    const NodeTypeMap& getNodeTypeMap() const;

private:
    // 输入数据
    const SymbolTable& symbolTable;
    const TypeChecker& typeChecker;
    
    // 输出管理
    std::ostringstream outputBuffer;
    std::ostream& outputStream;
    
    // 核心组件
    std::unique_ptr<IRBuilder> builder;
    std::unique_ptr<TypeMapper> typeMapper;
    std::unique_ptr<ExpressionGenerator> expressionGenerator;
    std::unique_ptr<StatementGenerator> statementGenerator;
    std::unique_ptr<FunctionCodegen> functionCodegen;
    std::unique_ptr<BuiltinDeclarator> builtinDeclarator;
    
    // 错误管理
    std::vector<std::string> errors;
    bool hasGenerationErrors;
    
    // 初始化方法
    void initializeComponents();
    void setupComponentDependencies();
    void initializeModule();
    void declareBuiltinFunctions();
    
    // 生成方法
    void generateTopLevelItems(const std::vector<std::unique_ptr<Item>>& items);
    void generateFunction(const Function* function);
    void generateStruct(const StructStruct* structDef);
    void generateConstant(const ConstantItem* constant);
    void generateImpl(const InherentImpl* impl);
    
    // 工具方法
    void reportError(const std::string& message, const ASTNode* node = nullptr);
    void reportWarning(const std::string& message, const ASTNode* node = nullptr);
    bool validateGeneration() const;
    void finalizeModule();
};
```

## 实现策略

### 初始化流程

```cpp
IRGenerator::IRGenerator(const SymbolTable& symbolTable, 
                         const TypeChecker& typeChecker,
                         std::ostream& outputStream)
    : symbolTable(symbolTable), typeChecker(typeChecker), 
      outputStream(outputStream), hasGenerationErrors(false) {
    
    // 1. 初始化核心组件
    initializeComponents();
    
    // 2. 设置组件间依赖关系
    setupComponentDependencies();
    
    // 3. 初始化 LLVM 模块
    initializeModule();
    
    // 4. 声明内置函数
    declareBuiltinFunctions();
}

void IRGenerator::initializeComponents() {
    // 获取作用域树
    auto scopeTree = symbolTable.getScopeTree();
    
    // 创建 IRBuilder（核心组件，其他组件依赖它）
    builder = std::make_unique<IRBuilder>(scopeTree);
    
    // 创建 TypeMapper（依赖 IRBuilder）
    typeMapper = std::make_unique<TypeMapper>(scopeTree);
    
    // 获取节点类型映射
    const auto& nodeTypeMap = typeChecker.getNodeTypeMap();
    
    // 创建表达式和语句生成器（存在循环依赖）
    expressionGenerator = std::make_unique<ExpressionGenerator>(
        builder.get(), typeMapper.get(), scopeTree, nodeTypeMap);
    statementGenerator = std::make_unique<StatementGenerator>(
        builder.get(), typeMapper.get(), scopeTree, nodeTypeMap);
    
    // 创建函数和内置函数生成器
    functionCodegen = std::make_unique<FunctionCodegen>(
        builder.get(), typeMapper.get(), scopeTree);
    builtinDeclarator = std::make_unique<BuiltinDeclarator>(builder.get());
}

void IRGenerator::setupComponentDependencies() {
    // 解决 ExpressionGenerator 和 StatementGenerator 的循环依赖
    expressionGenerator->setStatementGenerator(statementGenerator.get());
    statementGenerator->setExpressionGenerator(expressionGenerator.get());
    
    // 设置其他组件的依赖关系
    functionCodegen->setExpressionGenerator(expressionGenerator.get());
    functionCodegen->setStatementGenerator(statementGenerator.get());
}

void IRGenerator::initializeModule() {
    // 输出目标三元组
    builder->emitTargetTriple("riscv32-unknown-unknown-elf");
    
    // 输出空行分隔
    outputBuffer << "\n";
    
    // 同步 IRBuilder 到全局作用域
    builder->syncWithScopeTree();
}

void IRGenerator::declareBuiltinFunctions() {
    builtinDeclarator->declareBuiltinFunctions();
}
```

### 主要生成流程

```cpp
bool IRGenerator::generateIR(const std::vector<std::unique_ptr<Item>>& items) {
    try {
        // 清理之前的错误
        clearErrors();
        
        // 生成顶层项
        generateTopLevelItems(items);
        
        // 完成模块生成
        finalizeModule();
        
        // 验证生成结果
        if (!validateGeneration()) {
            return false;
        }
        
        // 输出到目标流
        outputStream << outputBuffer.str();
        outputStream.flush();
        
        return !hasGenerationErrors;
        
    } catch (const std::exception& e) {
        reportError("IR generation failed: " + std::string(e.what()));
        return false;
    }
}

void IRGenerator::generateTopLevelItems(const std::vector<std::unique_ptr<Item>>& items) {
    for (const auto& item : items) {
        if (!item) continue;
        
        try {
            if (auto function = dynamic_cast<const Function*>(item->item.get())) {
                generateFunction(function);
            } else if (auto structDef = dynamic_cast<const StructStruct*>(item->item.get())) {
                generateStruct(structDef);
            } else if (auto constant = dynamic_cast<const ConstantItem*>(item->item.get())) {
                generateConstant(constant);
            } else if (auto impl = dynamic_cast<const InherentImpl*>(item->item.get())) {
                generateImpl(impl);
            } else {
                reportWarning("Unsupported top-level item type");
            }
        } catch (const std::exception& e) {
            reportError("Failed to generate top-level item: " + std::string(e.what()), item->item.get());
        }
    }
}
```

### 函数生成

```cpp
void IRGenerator::generateFunction(const Function* function) {
    if (!function) {
        reportError("Null function pointer");
        return;
    }
    
    // 获取函数信息
    std::string functionName = function->getFunctionName();
    auto functionSymbol = symbolTable.LookupSymbol(functionName);
    
    if (!functionSymbol) {
        reportError("Function symbol not found: " + functionName);
        return;
    }
    
    // 使用 FunctionCodegen 生成函数
    try {
        functionCodegen->generateFunction(function);
        
        // 同步作用域（函数生成可能改变作用域）
        builder->syncWithScopeTree();
        
    } catch (const std::exception& e) {
        reportError("Failed to generate function " + functionName + ": " + std::string(e.what()), function);
    }
}
```

### 结构体生成

```cpp
void IRGenerator::generateStruct(const StructStruct* structDef) {
    if (!structDef) {
        reportError("Null struct definition pointer");
        return;
    }
    
    std::string structName = structDef->getName();
    
    // 生成结构体类型定义
    try {
        // 通过 TypeMapper 生成结构体类型
        std::string llvmType = typeMapper->mapStructType(structName);
        
        // 输出结构体类型定义（如果需要）
        if (shouldEmitStructDefinition(structDef)) {
            emitStructDefinition(structDef, llvmType);
        }
        
    } catch (const std::exception& e) {
        reportError("Failed to generate struct " + structName + ": " + std::string(e.what()), structDef);
    }
}

void IRGenerator::emitStructDefinition(const StructStruct* structDef, const std::string& llvmType) {
    // 输出结构体类型定义
    outputBuffer << llvmType << " = type { ";
    
    const auto& fields = structDef->getFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) outputBuffer << ", ";
        
        auto fieldType = typeMapper->mapSemanticTypeToLLVM(fields[i]->type);
        outputBuffer << fieldType;
    }
    
    outputBuffer << " }\n";
}
```

### 常量生成

```cpp
void IRGenerator::generateConstant(const ConstantItem* constant) {
    if (!constant) {
        reportError("Null constant pointer");
        return;
    }
    
    std::string constantName = constant->getName();
    
    try {
        // 生成常量表达式
        if (constant->getExpression()) {
            std::string valueReg = expressionGenerator->generateExpression(constant->getExpression());
            
            // 输出全局常量定义
            std::string constantType = typeMapper->mapSemanticTypeToLLVM(constant->getType());
            outputBuffer << "@" << constantName << " = constant " << constantType << " " << valueReg << "\n";
        }
        
    } catch (const std::exception& e) {
        reportError("Failed to generate constant " + constantName + ": " + std::string(e.what()), constant);
    }
}
```

### Impl 生成

```cpp
void IRGenerator::generateImpl(const InherentImpl* impl) {
    if (!impl) {
        reportError("Null impl pointer");
        return;
    }
    
    try {
        // 生成 impl 块中的方法
        for (const auto& item : impl->getItems()) {
            if (auto function = dynamic_cast<const Function*>(item.get())) {
                generateFunction(function);
            }
        }
        
    } catch (const std::exception& e) {
        reportError("Failed to generate impl block: " + std::string(e.what()), impl);
    }
}
```

## 错误处理

### 错误收集和报告

```cpp
void IRGenerator::reportError(const std::string& message, const ASTNode* node) {
    hasGenerationErrors = true;
    
    std::string fullMessage = "IR Generation Error: " + message;
    
    if (node) {
        fullMessage += " at line " + std::to_string(node->getLine());
        if (node->getColumn() > 0) {
            fullMessage += ", column " + std::to_string(node->getColumn());
        }
    }
    
    errors.push_back(fullMessage);
    std::cerr << fullMessage << std::endl;
}

void IRGenerator::reportWarning(const std::string& message, const ASTNode* node) {
    std::string fullMessage = "IR Generation Warning: " + message;
    
    if (node) {
        fullMessage += " at line " + std::to_string(node->getLine());
    }
    
    std::cerr << fullMessage << std::endl;
}
```

### 错误恢复策略

```cpp
bool IRGenerator::generateWithErrorRecovery(const std::vector<std::unique_ptr<Item>>& items) {
    clearErrors();
    
    for (const auto& item : items) {
        if (!item) continue;
        
        try {
            // 尝试生成项
            generateTopLevelItems({item});
            
        } catch (const std::exception& e) {
            // 记录错误但继续处理其他项
            reportError("Failed to generate item: " + std::string(e.what()), item->item.get());
            
            // 生成错误恢复代码
            generateErrorRecoveryCode(item);
        }
    }
    
    return !hasGenerationErrors;
}

void IRGenerator::generateErrorRecoveryCode(const std::unique_ptr<Item>& item) {
    // 根据项类型生成恢复代码
    if (auto function = dynamic_cast<const Function*>(item->item.get())) {
        // 生成空函数
        outputBuffer << "define void @" << function->getFunctionName() << "() {\n";
        outputBuffer << "  ret void\n";
        outputBuffer << "}\n\n";
    }
    // 其他类型的恢复策略...
}
```

### 生成验证

```cpp
bool IRGenerator::validateGeneration() const {
    // 检查是否有未完成的函数
    if (builder->hasUnclosedFunctions()) {
        reportError("Unclosed functions detected");
        return false;
    }
    
    // 检查是否有未完成的基本块
    if (builder->hasUnclosedBasicBlocks()) {
        reportError("Unclosed basic blocks detected");
        return false;
    }
    
    // 检查输出格式
    std::string output = outputBuffer.str();
    if (output.empty()) {
        reportError("Empty IR output");
        return false;
    }
    
    // 可以添加更多验证规则...
    
    return true;
}
```

## 输出管理

### 输出缓冲管理

```cpp
void IRGenerator::flushOutput() {
    if (!outputBuffer.str().empty()) {
        outputStream << outputBuffer.str();
        outputStream.flush();
        outputBuffer.str("");
        outputBuffer.clear();
    }
}

std::string IRGenerator::getIROutput() const {
    return outputBuffer.str();
}

void IRGenerator::finalizeModule() {
    // 完成所有未完成的基本块
    builder->finalizeAllBasicBlocks();
    
    // 添加模块结束注释
    outputBuffer << "\n; End of generated IR\n";
}
```

### 输出格式控制

```cpp
void IRGenerator::setOutputFormat(OutputFormat format) {
    switch (format) {
        case OutputFormat::LLVM_IR:
            // 标准 LLVM IR 格式
            builder->setCommentPrefix(";");
            break;
        case OutputFormat::DEBUG:
            // 调试格式，包含更多注释
            builder->setCommentPrefix(";");
            builder->enableDebugComments(true);
            break;
        case OutputFormat::COMPACT:
            // 紧凑格式，最小化注释
            builder->setCommentPrefix("");
            builder->enableDebugComments(false);
            break;
    }
}
```

## 性能优化

### 内存管理优化

```cpp
class IRGenerator {
private:
    // 内存池管理
    std::vector<std::unique_ptr<uint8_t[]>> memoryPools;
    size_t currentPoolSize;
    static constexpr size_t POOL_SIZE = 1024 * 1024; // 1MB pools
    
public:
    void* allocateInPool(size_t size) {
        if (currentPoolSize + size > POOL_SIZE) {
            // 分配新的内存池
            memoryPools.push_back(std::make_unique<uint8_t[]>(POOL_SIZE));
            currentPoolSize = 0;
        }
        
        void* ptr = memoryPools.back().get() + currentPoolSize;
        currentPoolSize += size;
        return ptr;
    }
};
```

### 输出优化

```cpp
void IRGenerator::optimizeOutput() {
    // 批量输出优化
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
    
    if (outputBuffer.str().size() > BUFFER_SIZE) {
        // 大量输出时直接刷新到流
        flushOutput();
    }
    
    // 字符串拼接优化
    outputBuffer.rdbuf()->pubsetbuf(nullptr, 0); // 无缓冲模式
}
```

## 测试策略

### 单元测试

```cpp
// 基本生成测试
TEST(IRGeneratorTest, BasicGeneration) {
    MockSymbolTable symbolTable;
    MockTypeChecker typeChecker;
    std::ostringstream output;
    
    IRGenerator generator(symbolTable, typeChecker, output);
    
    // 创建简单的 AST
    std::vector<std::unique_ptr<Item>> items;
    items.push_back(createSimpleFunction());
    
    bool success = generator.generateIR(items);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(generator.hasErrors());
    std::string ir = output.str();
    EXPECT_TRUE(ir.find("define i32 @main()") != std::string::npos);
}

// 错误处理测试
TEST(IRGeneratorTest, ErrorHandling) {
    MockSymbolTable symbolTable;
    MockTypeChecker typeChecker;
    std::ostringstream output;
    
    IRGenerator generator(symbolTable, typeChecker, output);
    
    // 创建有错误的 AST
    std::vector<std::unique_ptr<Item>> items;
    items.push_back(createErroneousFunction());
    
    bool success = generator.generateIR(items);
    
    EXPECT_FALSE(success);
    EXPECT_TRUE(generator.hasErrors());
    EXPECT_GT(generator.getErrors().size(), 0);
}
```

### 集成测试

```cpp
// 完整编译流程测试
TEST(IRGeneratorIntegrationTest, FullCompilation) {
    // 设置完整的语义分析结果
    auto symbolTable = performSemanticAnalysis("fn main() -> i32 { return 42; }");
    auto typeChecker = performTypeChecking(symbolTable);
    
    std::ostringstream output;
    IRGenerator generator(*symbolTable, *typeChecker, output);
    
    auto ast = parseSource("fn main() -> i32 { return 42; }");
    bool success = generator.generateIR(ast);
    
    EXPECT_TRUE(success);
    
    // 验证生成的 IR 可以被 LLVM 解析
    std::string ir = output.str();
    EXPECT_TRUE(verifyLLVMIR(ir));
}
```

## 使用示例

### 基本使用

```cpp
int main(int argc, char* argv[]) {
    // 执行词法分析、语法分析、语义分析...
    auto symbolTable = performSemanticAnalysis(sourceCode);
    auto typeChecker = performTypeChecking(*symbolTable);
    auto ast = parseSource(sourceCode);
    
    // 创建 IR 生成器
    IRGenerator irGenerator(*symbolTable, *typeChecker);
    
    // 生成 IR
    bool success = irGenerator.generateIR(ast);
    
    if (success) {
        // 输出到 stdout（默认）
        std::cout << irGenerator.getIROutput();
        return 0;
    } else {
        // 输出错误信息
        for (const auto& error : irGenerator.getErrors()) {
            std::cerr << error << std::endl;
        }
        return 1;
    }
}
```

### 高级使用

```cpp
// 自定义输出流
std::ofstream outputFile("output.ll");
IRGenerator irGenerator(*symbolTable, *typeChecker, outputFile);

// 设置调试格式
irGenerator.setOutputFormat(OutputFormat::DEBUG);

// 生成 IR
bool success = irGenerator.generateIR(ast);

// 获取组件访问
if (success) {
    auto& builder = irGenerator.getBuilder();
    auto& exprGen = irGenerator.getExpressionGenerator();
    
    // 可以进行额外的 IR 操作...
}

// 手动刷新输出
irGenerator.flushOutput();
```

## 总结

IRGenerator 作为 IR 生成模块的主控制器，具有以下核心特点：

1. **统一协调**：有效管理所有子组件的交互和依赖关系
2. **错误处理**：提供完善的错误检测、报告和恢复机制
3. **输出管理**：高效的 IR 文本生成和输出管理
4. **模块化设计**：清晰的组件分离和接口定义
5. **性能优化**：内存管理和输出优化
6. **易于扩展**：为未来功能扩展预留接口

通过 IRGenerator，编译器可以将完整的 Rx 语言程序正确地转换为高效的 LLVM IR 代码，确保与 builtin.c 的联合编译和正确执行。
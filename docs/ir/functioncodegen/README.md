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

```cpp
// 前向声明
class ExpressionGenerator;
class StatementGenerator;

class FunctionCodegen {
public:
    FunctionCodegen(std::shared_ptr<IRBuilder> irBuilder,
                   std::shared_ptr<TypeMapper> typeMapper,
                   std::shared_ptr<ScopeTree> scopeTree);
    
    // 设置依赖组件
    void setExpressionGenerator(std::shared_ptr<ExpressionGenerator> exprGen);
    void setStatementGenerator(std::shared_ptr<StatementGenerator> stmtGen);
    
    // 主要生成接口
    void generateFunction(const Function* function);
    void generateFunctionDeclaration(const Function* function);
    void generateFunctionBody(const Function* function);
    
    // 内置函数调用生成（供 ExpressionGenerator 调用）
    std::string generateBuiltinCall(const CallExpression* call);
    
    // 参数处理
    void generateParameters(const Function* function);
    std::string generateArgumentLoad(const Parameter* param, const std::string& arg);
    void generateParameterAlloca(const Parameter* param, const std::string& arg);
    
    // 返回值处理
    void generateReturnStatement(const ReturnExpression* returnExpr);
    std::string generateReturnValue(const Expression* returnExpr);
    void generateReturnPhi();
    void generateTailExpressionReturn(const Statement* body);
    void generateDefaultReturn();
    
    // 函数签名生成
    std::string generateFunctionSignature(const Function* function);
    std::vector<std::pair<std::string, std::string>> generateParameterList(const Function* function);
    std::string generateFunctionType(const Function* function);
    
    // 工具方法（供 ExpressionGenerator 调用）
    std::string getFunctionName(const Function* function);
    std::string getMangledName(const Function* function);
    bool isBuiltinFunction(const std::string& name);
    std::string getBuiltinFunctionType(const std::string& name);
    std::string generateArgument(const Expression* arg, const Type* paramType);
    std::string generateStructArgument(const std::string& arg, const std::string& structType);

private:
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ExpressionGenerator> expressionGenerator;
    std::shared_ptr<StatementGenerator> statementGenerator;
    
    // 函数上下文管理
    struct FunctionContext {
        std::string functionName;
        std::string returnType;
        std::string returnBlock;
        std::string returnPhiReg;
        bool hasReturn;
        std::vector<std::string> parameters;
        std::unordered_map<std::string, std::string> parameterRegs;
    };
    
    std::vector<FunctionContext> functionStack;
    FunctionContext* currentFunction;
    
    // 内置函数类型缓存
    std::unordered_map<std::string, std::string> builtinFunctionTypes;
    
    // 辅助方法
    void enterFunction(const Function* function);
    void exitFunction();
    void generatePrologue(const Function* function);
    void generateEpilogue(const Function* function);
    void handleReturnValue(const std::string& value, const std::string& type);
    std::string generateParameterType(const Parameter* param);
    std::string generateParameterName(const Parameter* param, int index);
    void setupParameterScope(const Function* function);
};
```

## 实现策略

### 函数定义生成

```cpp
void FunctionCodegen::generateFunction(const Function* function) {
    // 进入函数上下文
    enterFunction(function);
    
    // 生成函数签名
    std::string signature = generateFunctionSignature(function);
    irBuilder->emitFunctionDef(signature, generateParameterList(function));
    
    // 创建入口基本块
    std::string entryBB = irBuilder->newBasicBlock("entry");
    irBuilder->emitLabel(entryBB);
    
    // 生成函数序言
    generatePrologue(function);
    
    // 处理参数
    generateParameters(function);
    
    // 设置参数作用域
    setupParameterScope(function);
    
    // 生成函数体
    generateFunctionBody(function);
    
    // 生成函数尾声
    generateEpilogue(function);
    
    // 结束函数定义
    irBuilder->emitFunctionEnd();
    
    // 退出函数上下文
    exitFunction();
}

void FunctionCodegen::generateFunctionSignature(const Function* function) {
    std::string functionName = getFunctionName(function);
    std::string returnType = typeMapper->mapTypeToLLVM(function->getReturnType());
    
    // 获取参数类型列表
    std::vector<std::string> paramTypes;
    for (const auto& param : function->getParameters()) {
        std::string paramType = generateParameterType(param.get());
        paramTypes.push_back(paramType);
    }
    
    // 生成函数类型字符串
    std::string functionType = returnType + " (";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) functionType += ", ";
        functionType += paramTypes[i];
    }
    functionType += ")";
    
    // 缓存函数类型
    currentFunction->returnType = returnType;
    currentFunction->functionName = functionName;
}

std::vector<std::pair<std::string, std::string>> FunctionCodegen::generateParameterList(const Function* function) {
    std::vector<std::pair<std::string, std::string>> paramList;
    
    for (size_t i = 0; i < function->getParameters().size(); ++i) {
        const auto& param = function->getParameters()[i];
        std::string paramName = generateParameterName(param.get(), i);
        std::string paramType = generateParameterType(param.get());
        
        paramList.push_back({paramName, paramType});
        currentFunction->parameters.push_back(paramName);
    }
    
    return paramList;
}
```

### 参数处理

```cpp
void FunctionCodegen::generateParameters(const Function* function) {
    const auto& parameters = function->getParameters();
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        const auto& param = parameters[i];
        std::string paramName = generateParameterName(param.get(), i);
        std::string paramType = generateParameterType(param.get());
        
        // 为参数分配栈空间（如果需要）
        if (shouldAllocateParameter(param.get())) {
            std::string allocaReg = irBuilder->newRegister(paramName, "ptr");
            irBuilder->emitAlloca(allocaReg, paramType);
            
            // 存储参数值到栈空间
            irBuilder->emitStore(paramName, allocaReg, paramType);
            
            // 记录参数寄存器映射
            currentFunction->parameterRegs[paramName] = allocaReg;
        } else {
            // 直接使用参数寄存器
            currentFunction->parameterRegs[paramName] = "%" + paramName;
        }
    }
}

void FunctionCodegen::generateParameterAlloca(const Parameter* param, const std::string& arg) {
    std::string paramName = param->getName();
    std::string paramType = typeMapper->mapTypeToLLVM(param->getType());
    
    // 分配参数栈空间
    std::string allocaReg = irBuilder->newRegister(paramName, "ptr");
    irBuilder->emitAlloca(allocaReg, paramType);
    
    // 存储参数值
    irBuilder->emitStore(arg, allocaReg, paramType);
    
    // 注册到作用域
    auto currentScope = scopeTree->GetCurrentScope();
    auto symbol = std::make_shared<Symbol>(paramName, SymbolKind::Parameter,
                                          param->getType(), false, nullptr);
    scopeTree->InsertSymbol(paramName, symbol);
    irBuilder->setVariableRegister(paramName, allocaReg);
}

std::string FunctionCodegen::generateArgumentLoad(const Parameter* param, const std::string& arg) {
    std::string paramType = typeMapper->mapTypeToLLVM(param->getType());
    
    // 如果参数是值类型且需要复制，则加载
    if (isByValueParameter(param)) {
        std::string loadReg = irBuilder->newRegister();
        irBuilder->emitLoad(loadReg, arg, paramType);
        return loadReg;
    }
    
    return arg;
}

void FunctionCodegen::setupParameterScope(const Function* function) {
    // 为函数创建新的作用域
    scopeTree->EnterScope();
    irBuilder->enterScope();
    
    // 注册所有参数到符号表
    for (const auto& param : function->getParameters()) {
        std::string paramName = param->getName();
        auto it = currentFunction->parameterRegs.find(paramName);
        
        if (it != currentFunction->parameterRegs.end()) {
            auto symbol = std::make_shared<Symbol>(paramName, SymbolKind::Parameter,
                                                  param->getType(), false, nullptr);
            scopeTree->InsertSymbol(paramName, symbol);
            irBuilder->setVariableRegister(paramName, it->second);
        }
    }
}
```

### 函数体生成

```cpp
void FunctionCodegen::generateFunctionBody(const Function* function) {
    if (!function->getBody()) {
        // 空函数体，生成默认返回
        generateDefaultReturn();
        return;
    }
    
    // 创建返回基本块
    currentFunction->returnBlock = irBuilder->newBasicBlock("return");
    currentFunction->returnPhiReg = irBuilder->newRegister("return", "phi");
    currentFunction->hasReturn = false;
    
    // 生成函数体语句
    if (statementGenerator) {
        statementGenerator->generateStatement(function->getBody());
    }
    
    // 如果函数体没有返回语句，尝试返回尾表达式的值
    if (!currentFunction->hasReturn) {
        generateTailExpressionReturn(function->getBody());
    }
    
    // 生成返回基本块
    irBuilder->emitLabel(currentFunction->returnBlock);
    
    // 生成返回 PHI 节点（如果有多个返回点）
    if (hasMultipleReturns()) {
        generateReturnPhi();
    }
    
    // 生成最终返回指令
    if (currentFunction->returnType == "void") {
        irBuilder->emitRet("", "void");
    } else {
        irBuilder->emitRet(currentFunction->returnPhiReg, currentFunction->returnType);
    }
}

void FunctionCodegen::generateTailExpressionReturn(const Statement* body) {
    // 检查函数体是否是块表达式
    auto blockExpr = dynamic_cast<const BlockExpression*>(body);
    if (!blockExpr || blockExpr->getStatements().empty()) {
        generateDefaultReturn();
        return;
    }
    
    // 获取最后一个语句
    const auto& statements = blockExpr->getStatements();
    const auto& lastStmt = statements.back();
    
    // 检查最后一个语句是否是表达式
    auto exprStmt = dynamic_cast<const ExpressionStatement*>(lastStmt);
    if (!exprStmt) {
        generateDefaultReturn();
        return;
    }
    
    // 生成尾表达式的值
    std::string tailValue = expressionGenerator->generateExpression(exprStmt->getExpression());
    std::string tailType = expressionGenerator->getExpressionType(exprStmt->getExpression());
    
    // 类型转换（如果需要）
    if (tailType != currentFunction->returnType) {
        tailValue = generateTypeConversion(tailValue, tailType, currentFunction->returnType);
    }
    
    // 跳转到返回基本块
    irBuilder->emitBr(currentFunction->returnBlock);
    
    // 在返回基本块中添加 PHI 节点的输入
    addReturnPhiInput(tailValue);
}

void FunctionCodegen::generateDefaultReturn() {
    if (currentFunction->returnType == "void") {
        irBuilder->emitRet("", "void");
    } else {
        // 返回类型的默认值
        std::string defaultValue = generateDefaultValue(currentFunction->returnType);
        irBuilder->emitRet(defaultValue, currentFunction->returnType);
    }
}

std::string FunctionCodegen::generateDefaultValue(const std::string& type) {
    if (type == "i32" || type == "i64") {
        return "0";
    } else if (type == "i1") {
        return "false";
    } else if (type == "float" || type == "double") {
        return "0.0";
    } else if (type.find("*") != std::string::npos) {
        return "null";
    } else {
        // 对于结构体等复杂类型，分配未初始化的内存
        std::string reg = irBuilder->newRegister();
        irBuilder->emitAlloca(reg, type);
        return reg;
    }
}
```

### 内置函数处理

```cpp
std::string FunctionCodegen::generateBuiltinCall(const CallExpression* call) {
    std::string functionName = getFunctionNameFromCall(call);
    
    if (functionName == "print" || functionName == "println") {
        return generatePrintCall(call, functionName == "println");
    } else if (functionName == "printInt" || functionName == "printlnInt") {
        return generatePrintIntCall(call, functionName == "printlnInt");
    } else if (functionName == "getString") {
        return generateGetStringCall(call);
    } else if (functionName == "getInt") {
        return generateGetIntCall(call);
    } else if (functionName == "malloc") {
        return generateMallocCall(call);
    } else if (functionName == "memset" || functionName == "memcpy") {
        return generateMemoryCall(call, functionName);
    } else {
        return "null";
    }
}

std::string FunctionCodegen::generateArgument(const Expression* arg,
                                            const Type* paramType) {
    if (!expressionGenerator) {
        return "null";
    }
    
    // 生成参数表达式
    std::string argReg = expressionGenerator->generateExpression(arg);
    std::string argType = expressionGenerator->getExpressionType(arg);
    std::string paramTypeLLVM = typeMapper->mapTypeToLLVM(paramType);
    
    // 类型转换（如果需要）
    if (argType != paramTypeLLVM) {
        argReg = generateTypeConversion(argReg, argType, paramTypeLLVM);
    }
    
    // 处理大结构体参数（通过引用传递）
    if (isLargeStructType(paramTypeLLVM)) {
        return generateStructArgument(argReg, paramTypeLLVM);
    }
    
    return argReg;
}

std::string FunctionCodegen::generateStructArgument(const std::string& arg,
                                                  const std::string& structType) {
    // 为结构体参数分配临时空间
    std::string tempReg = irBuilder->newRegister("temp", "ptr");
    irBuilder->emitAlloca(tempReg, structType);
    
    // 复制结构体到临时空间
    irBuilder->emitMemcpy(tempReg, arg, getTypeSize(structType));
    
    // 返回指针
    return tempReg;
}
```

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

```cpp
// ExpressionGenerator 中的函数调用处理
class ExpressionGenerator {
private:
    std::shared_ptr<FunctionCodegen> functionCodegen;
    
public:
    void setFunctionCodegen(std::shared_ptr<FunctionCodegen> funcGen) {
        functionCodegen = funcGen;
    }
    
    std::string generateCallExpression(const CallExpression* call) {
        std::string functionName = getFunctionName(call);
        
        // 检查是否为内置函数
        if (functionCodegen && functionCodegen->isBuiltinFunction(functionName)) {
            return functionCodegen->generateBuiltinCall(call);
        }
        
        // 处理普通函数调用
        return generateRegularFunctionCall(call);
    }
    
    std::string generateRegularFunctionCall(const CallExpression* call) {
        // 查找函数符号
        auto functionSymbol = scopeTree->LookupSymbol(functionName);
        if (!functionSymbol || functionSymbol->kind != SymbolKind::Function) {
            return "null";
        }
        
        // 生成参数表达式
        std::vector<std::string> argRegs;
        auto functionType = std::dynamic_pointer_cast<FunctionType>(functionSymbol->type);
        
        if (functionType && functionType->parameters.size() == call->getArguments().size()) {
            for (size_t i = 0; i < call->getArguments().size(); ++i) {
                // 调用 FunctionCodegen 处理参数
                std::string argReg = functionCodegen->generateArgument(
                    call->getArguments()[i],
                    functionType->parameters[i]
                );
                argRegs.push_back(argReg);
            }
        }
        
        // 生成函数调用
        std::string resultReg = irBuilder->newRegister();
        std::string returnType = typeMapper->mapTypeToLLVM(functionType->returnType);
        
        irBuilder->emitCall(resultReg, functionName, argRegs, returnType);
        
        return resultReg;
    }
};
```


### 返回值处理

```cpp
void FunctionCodegen::generateReturnStatement(const ReturnExpression* returnExpr) {
    if (!currentFunction) {
        return;
    }
    
    currentFunction->hasReturn = true;
    
    if (returnExpr->getExpression()) {
        // 有返回值的 return
        std::string valueReg = generateReturnValue(returnExpr->getExpression());
        std::string valueType = expressionGenerator->getExpressionType(returnExpr->getExpression());
        
        // 类型转换（如果需要）
        if (valueType != currentFunction->returnType) {
            valueReg = generateTypeConversion(valueReg, valueType, currentFunction->returnType);
        }
        
        // 跳转到返回基本块
        irBuilder->emitBr(currentFunction->returnBlock);
        
        // 在返回基本块中添加 PHI 节点的输入
        addReturnPhiInput(valueReg);
        
    } else {
        // 无返回值的 return
        // 跳转到返回基本块
        irBuilder->emitBr(currentFunction->returnBlock);
    }
    
    // 标记代码为不可达
    handleUnreachableCode();
}

std::string FunctionCodegen::generateReturnValue(const Expression* returnExpr) {
    if (!expressionGenerator) {
        return "null";
    }
    
    return expressionGenerator->generateExpression(returnExpr);
}

void FunctionCodegen::generateReturnPhi() {
    if (currentFunction->returnValues.empty()) {
        return;
    }
    
    // 生成 PHI 节点
    std::vector<std::string> values;
    std::vector<std::string> blocks;
    
    for (const auto& returnValue : currentFunction->returnValues) {
        values.push_back(returnValue.first);
        blocks.push_back(returnValue.second);
    }
    
    irBuilder->emitPhi(currentFunction->returnPhiReg, currentFunction->returnType, 
                      values, blocks);
}

void FunctionCodegen::addReturnPhiInput(const std::string& value) {
    std::string currentBB = irBuilder->getCurrentBasicBlock();
    currentFunction->returnValues.push_back({value, currentBB});
}
```


## 函数上下文管理

### 上下文栈管理

```cpp
void FunctionCodegen::enterFunction(const Function* function) {
    FunctionContext context;
    context.functionName = getFunctionName(function);
    context.returnType = typeMapper->mapTypeToLLVM(function->getReturnType());
    context.hasReturn = false;
    
    functionStack.push_back(context);
    currentFunction = &functionStack.back();
}

void FunctionCodegen::exitFunction() {
    if (!functionStack.empty()) {
        functionStack.pop_back();
    }
    
    currentFunction = functionStack.empty() ? nullptr : &functionStack.back();
}

bool FunctionCodegen::isInFunction() const {
    return currentFunction != nullptr;
}

const FunctionContext* FunctionCodegen::getCurrentFunction() const {
    return currentFunction;
}
```

### 返回值管理

```cpp
void FunctionCodegen::handleReturnValue(const std::string& value, const std::string& type) {
    if (!currentFunction) {
        return;
    }
    
    // 记录返回值
    currentFunction->hasReturn = true;
    addReturnPhiInput(value);
    
    // 跳转到返回基本块
    irBuilder->emitBr(currentFunction->returnBlock);
}

bool FunctionCodegen::hasMultipleReturns() const {
    return currentFunction && currentFunction->returnValues.size() > 1;
}
```



## 测试策略

### 单元测试

```cpp
// 函数定义测试
TEST(FunctionCodegenTest, FunctionDefinition) {
    setupGenerator();
    
    auto function = createSimpleFunction("test", "i32", {"a", "b"}, {"i32", "i32"});
    generator->generateFunction(function);
    
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("define i32 @test(i32 %a, i32 %b)") != std::string::npos);
    EXPECT_TRUE(ir.find("ret i32") != std::string::npos);
}

// 内置函数测试
TEST(FunctionCodegenTest, BuiltinFunction) {
    setupGenerator();
    
    auto call = createFunctionCall("printlnInt", {createLiteral("42")});
    std::string result = generator->generateBuiltinCall(call);
    
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("call void @printlnInt(i32 42)") != std::string::npos);
}

// 尾表达式返回测试
TEST(FunctionCodegenTest, TailExpressionReturn) {
    setupGenerator();
    
    auto function = createFunctionWithTailExpression("add", "i32", {"a", "b"}, {"i32", "i32"});
    generator->generateFunction(function);
    
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("ret i32") != std::string::npos);
}
```

### 集成测试

```cpp
// 复杂函数测试
TEST(FunctionCodegenIntegrationTest, ComplexFunction) {
    setupGenerator();
    
    // 测试递归函数
    auto recursiveFunction = createRecursiveFunction("factorial", "i32", {"n"}, {"i32"});
    generator->generateFunction(recursiveFunction);
    
    // 验证生成的 IR 正确性
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(verifyFunctionIR(ir));
}
```

## 使用示例

### 基本使用

```cpp
// 创建 FunctionCodegen
auto irBuilder = std::make_shared<IRBuilder>(scopeTree);
auto typeMapper = std::make_shared<TypeMapper>(scopeTree);
auto scopeTree = semanticAnalyzer->getScopeTree();

FunctionCodegen funcGen(irBuilder, typeMapper, scopeTree);

// 设置依赖组件
funcGen.setExpressionGenerator(expressionGenerator);
funcGen.setStatementGenerator(statementGenerator);

// 设置组件间的双向通信
expressionGenerator->setFunctionCodegen(funcGen);

// 生成函数定义
auto function = createFunction("add", "i32", {"a", "b"}, {"i32", "i32"});
funcGen.generateFunction(function);
// 输出：
// define i32 @add(i32 %a, i32 %b) {
// entry:
//   %a_ptr = alloca i32
//   store i32 %a, i32* %a_ptr
//   %b_ptr = alloca i32
//   store i32 %b, i32* %b_ptr
//   ; ... 函数体 ...
// }
```

### 组件协作示例

```cpp
// 在 ExpressionGenerator 中处理函数调用
class ExpressionGenerator {
private:
    std::shared_ptr<FunctionCodegen> functionCodegen;
    
public:
    std::string generateExpression(const Expression* expr) {
        if (auto callExpr = dynamic_cast<const CallExpression*>(expr)) {
            return generateCallExpression(callExpr);
        }
        // ... 其他表达式处理
    }
    
    std::string generateCallExpression(const CallExpression* call) {
        std::string functionName = getFunctionName(call);
        
        // 检查是否为内置函数
        if (functionCodegen && functionCodegen->isBuiltinFunction(functionName)) {
            return functionCodegen->generateBuiltinCall(call);
        }
        
        // 处理普通函数调用
        return generateRegularFunctionCall(call);
    }
};

// 生成内置函数调用
auto printCall = createFunctionCall("printlnInt", {createLiteral("42")});
std::string printResult = expressionGenerator->generateExpression(printCall);
// 输出：call void @printlnInt(i32 42)
```

## 总结

FunctionCodegen 组件是 IR 生成阶段的核心组件，提供了函数生成功能：

1. **函数定义生成**：生成函数签名、参数和函数体的 IR
2. **函数体处理**：处理函数体语句和表达式
3. **返回值处理**：处理基本返回值和尾表达式返回
4. **内置函数支持**：支持内置函数调用
5. **组件协作**：与其他 IR 生成组件协作

通过 FunctionCodegen，IR 生成器可以将 Rx 语言函数正确地转换为 LLVM IR 代码，并与 ExpressionGenerator、StatementGenerator、IRBuilder、TypeMapper 等组件形成完整的代码生成流水线。
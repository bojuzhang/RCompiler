# FunctionCodegen 函数代码生成器设计文档

## 概述

FunctionCodegen 组件负责将 Rx 语言的函数定义和调用转换为 LLVM IR 文本。它是 IR 生成阶段的核心组件之一，处理函数签名生成、参数传递、返回值处理、函数调用生成等所有与函数相关的代码生成任务。

## 设计目标

1. **完整的函数支持**：支持所有 Rx 语言函数特性的 IR 生成
2. **调用约定处理**：正确处理函数调用约定和参数传递
3. **返回值管理**：处理各种返回值类型和返回语句
4. **内联优化**：支持函数内联优化
5. **错误处理**：提供完善的函数相关错误检测和恢复机制

## 核心架构

### 函数处理分类

FunctionCodegen 处理以下函数相关任务：

1. **函数定义生成**
   - 函数签名生成
   - 参数处理和分配
   - 函数体生成
   - 返回值处理

2. **函数调用生成**
   - 普通函数调用
   - 方法调用（通过 ExpressionGenerator）
   - 内置函数调用
   - 递归函数调用

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
    
    // 函数调用生成
    std::string generateFunctionCall(const CallExpression* call);
    std::string generateBuiltinCall(const CallExpression* call);
    std::string generateMethodCall(const MethodCallExpression* call);
    
    // 参数处理
    void generateParameters(const Function* function);
    std::string generateArgumentLoad(const Parameter* param, const std::string& arg);
    void generateParameterAlloca(const Parameter* param, const std::string& arg);
    
    // 返回值处理
    void generateReturnStatement(const ReturnExpression* returnExpr);
    std::string generateReturnValue(const Expression* returnExpr);
    void generateReturnPhi();
    
    // 内联函数处理
    bool canInline(const Function* function);
    std::string generateInlineCall(const CallExpression* call);
    void generateInlineFunction(const Function* function);
    
    // 函数签名生成
    std::string generateFunctionSignature(const Function* function);
    std::vector<std::pair<std::string, std::string>> generateParameterList(const Function* function);
    std::string generateFunctionType(const Function* function);
    
    // 工具方法
    std::string getFunctionName(const Function* function);
    std::string getMangledName(const Function* function);
    bool isBuiltinFunction(const std::string& name);
    std::string getBuiltinFunctionType(const std::string& name);

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
    
    // 内联函数缓存
    std::unordered_map<const Function*, bool> inlineCache;
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
    if (!function) {
        reportError("Null function pointer");
        return;
    }
    
    try {
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
        
    } catch (const std::exception& e) {
        reportError("Failed to generate function: " + std::string(e.what()), function);
    }
}

void FunctionCodegen::generateFunctionSignature(const Function* function) {
    std::string functionName = getFunctionName(function);
    std::string returnType = typeMapper->mapSemanticTypeToLLVM(function->getReturnType());
    
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
    std::string paramType = typeMapper->mapSemanticTypeToLLVM(param->getType());
    
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
    std::string paramType = typeMapper->mapSemanticTypeToLLVM(param->getType());
    
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
    } else {
        reportError("StatementGenerator not available for function body generation");
    }
    
    // 如果函数体没有返回语句，生成默认返回
    if (!currentFunction->hasReturn) {
        generateDefaultReturn();
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

### 函数调用生成

```cpp
std::string FunctionCodegen::generateFunctionCall(const CallExpression* call) {
    if (!call) {
        reportError("Null function call pointer");
        return "null";
    }
    
    // 获取函数名
    std::string functionName = getFunctionNameFromCall(call);
    
    // 检查是否为内置函数
    if (isBuiltinFunction(functionName)) {
        return generateBuiltinCall(call);
    }
    
    // 查找函数符号
    auto functionSymbol = scopeTree->LookupSymbol(functionName);
    if (!functionSymbol || functionSymbol->kind != SymbolKind::Function) {
        reportError("Undefined function: " + functionName);
        return "null";
    }
    
    // 检查是否可以内联
    auto function = getFunctionFromSymbol(functionSymbol);
    if (function && canInline(function)) {
        return generateInlineCall(call);
    }
    
    // 生成参数表达式
    std::vector<std::string> argRegs;
    auto functionType = std::dynamic_pointer_cast<FunctionType>(functionSymbol->type);
    
    if (functionType && functionType->parameters.size() == call->getArguments().size()) {
        for (size_t i = 0; i < call->getArguments().size(); ++i) {
            std::string argReg = generateArgument(call->getArguments()[i], 
                                               functionType->parameters[i]);
            argRegs.push_back(argReg);
        }
    } else {
        // 参数数量不匹配，生成错误恢复
        for (const auto& arg : call->getArguments()) {
            std::string argReg = expressionGenerator->generateExpression(arg);
            argRegs.push_back(argReg);
        }
    }
    
    // 生成函数调用
    std::string resultReg = irBuilder->newRegister();
    std::string returnType = typeMapper->mapSemanticTypeToLLVM(functionType->returnType);
    
    irBuilder->emitCall(resultReg, functionName, argRegs, returnType);
    
    return resultReg;
}

std::string FunctionCodegen::generateArgument(const Expression* arg, 
                                            std::shared_ptr<SemanticType> paramType) {
    if (!expressionGenerator) {
        reportError("ExpressionGenerator not available for argument generation");
        return "null";
    }
    
    // 生成参数表达式
    std::string argReg = expressionGenerator->generateExpression(arg);
    std::string argType = expressionGenerator->getExpressionType(arg);
    std::string paramTypeLLVM = typeMapper->mapSemanticTypeToLLVM(paramType);
    
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
        reportError("Unknown builtin function: " + functionName);
        return "null";
    }
}

std::string FunctionCodegen::generatePrintIntCall(const CallExpression* call, bool newline) {
    if (call->getArguments().size() != 1) {
        reportError("printInt/printlnInt expects exactly 1 argument");
        return "null";
    }
    
    // 生成参数表达式
    std::string argReg = expressionGenerator->generateExpression(call->getArguments()[0]);
    std::string argType = expressionGenerator->getExpressionType(call->getArguments()[0]);
    
    // 确保参数是整数类型
    if (!isIntegerType(argType)) {
        argReg = generateTypeConversion(argReg, argType, "i32");
    }
    
    // 生成内置函数调用
    std::string funcName = newline ? "printlnInt" : "printInt";
    irBuilder->emitCall("", funcName, {argReg}, "void");
    
    // print 函数返回 unit 类型
    return generateUnitValue();
}

std::string FunctionCodegen::generateGetStringCall(const CallExpression* call) {
    if (call->getArguments().size() != 0) {
        reportError("getString expects no arguments");
        return "null";
    }
    
    // 生成内置函数调用
    std::string resultReg = irBuilder->newRegister();
    irBuilder->emitCall(resultReg, "getString", {}, "ptr");
    
    return resultReg;
}

std::string FunctionCodegen::generateMallocCall(const CallExpression* call) {
    if (call->getArguments().size() != 1) {
        reportError("malloc expects exactly 1 argument");
        return "null";
    }
    
    // 生成大小参数
    std::string sizeReg = expressionGenerator->generateExpression(call->getArguments()[0]);
    std::string sizeType = expressionGenerator->getExpressionType(call->getArguments()[0]);
    
    // 确保大小是整数类型
    if (!isIntegerType(sizeType)) {
        sizeReg = generateTypeConversion(sizeReg, sizeType, "i32");
    }
    
    // 生成 malloc 调用
    std::string resultReg = irBuilder->newRegister();
    irBuilder->emitCall(resultReg, "malloc", {sizeReg}, "ptr");
    
    return resultReg;
}
```

### 返回值处理

```cpp
void FunctionCodegen::generateReturnStatement(const ReturnExpression* returnExpr) {
    if (!currentFunction) {
        reportError("Return statement outside of function");
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
        if (currentFunction->returnType != "void") {
            reportError("Empty return in non-void function");
        }
        
        // 跳转到返回基本块
        irBuilder->emitBr(currentFunction->returnBlock);
    }
    
    // 标记代码为不可达
    handleUnreachableCode();
}

std::string FunctionCodegen::generateReturnValue(const Expression* returnExpr) {
    if (!expressionGenerator) {
        reportError("ExpressionGenerator not available for return value generation");
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

### 内联函数处理

```cpp
bool FunctionCodegen::canInline(const Function* function) {
    if (!function) {
        return false;
    }
    
    // 检查缓存
    auto it = inlineCache.find(function);
    if (it != inlineCache.end()) {
        return it->second;
    }
    
    bool shouldInline = false;
    
    // 内联决策策略
    if (function->isInline()) {
        // 显式标记为内联
        shouldInline = true;
    } else if (function->getBody() && isSmallFunction(function)) {
        // 小函数自动内联
        shouldInline = true;
    } else if (function->isTemplate()) {
        // 模板函数通常需要内联
        shouldInline = true;
    }
    
    // 缓存结果
    inlineCache[function] = shouldInline;
    return shouldInline;
}

std::string FunctionCodegen::generateInlineCall(const CallExpression* call) {
    // 获取要内联的函数
    auto function = getFunctionFromCall(call);
    if (!function) {
        return "null";
    }
    
    // 创建内联上下文
    InlineContext context;
    context.originalCall = call;
    context.function = function;
    
    // 生成内联函数体
    return generateInlineFunctionBody(context);
}

std::string FunctionCodegen::generateInlineFunctionBody(const InlineContext& context) {
    // 保存当前状态
    auto savedScope = scopeTree->GetCurrentScope();
    auto savedFunction = currentFunction;
    
    try {
        // 创建新的作用域用于内联
        scopeTree->EnterScope();
        
        // 处理参数绑定
        std::unordered_map<std::string, std::string> paramBindings;
        for (size_t i = 0; i < context.function->getParameters().size(); ++i) {
            const auto& param = context.function->getParameters()[i];
            const auto& arg = context.originalCall->getArguments()[i];
            
            std::string argReg = expressionGenerator->generateExpression(arg);
            std::string paramName = param->getName();
            
            // 创建参数绑定
            paramBindings[paramName] = argReg;
            
            // 注册到内联作用域
            auto symbol = std::make_shared<Symbol>(paramName, SymbolKind::Parameter,
                                                  param->getType(), false, nullptr);
            scopeTree->InsertSymbol(paramName, symbol);
            
            if (isByValueParameter(param.get())) {
                // 为值参数分配栈空间
                std::string allocaReg = irBuilder->newRegister(paramName, "ptr");
                std::string paramType = typeMapper->mapSemanticTypeToLLVM(param->getType());
                irBuilder->emitAlloca(allocaReg, paramType);
                irBuilder->emitStore(argReg, allocaReg, paramType);
                irBuilder->setVariableRegister(paramName, allocaReg);
            } else {
                // 引用参数直接使用
                irBuilder->setVariableRegister(paramName, argReg);
            }
        }
        
        // 生成内联函数体
        std::string resultReg = "null";
        if (context.function->getBody()) {
            // 对于有返回值的函数，需要特殊处理
            if (context.function->getReturnType()->getKind() != TypeKind::Unit) {
                resultReg = generateInlineFunctionWithReturn(context.function->getBody());
            } else {
                statementGenerator->generateStatement(context.function->getBody());
                resultReg = generateUnitValue();
            }
        }
        
        // 恢复作用域
        scopeTree->ExitScope();
        
        return resultReg;
        
    } catch (const std::exception& e) {
        // 恢复状态
        scopeTree->ExitScope();
        currentFunction = savedFunction;
        
        reportError("Inline function generation failed: " + std::string(e.what()));
        return "null";
    }
}
```

## 函数上下文管理

### 上下文栈管理

```cpp
void FunctionCodegen::enterFunction(const Function* function) {
    FunctionContext context;
    context.functionName = getFunctionName(function);
    context.returnType = typeMapper->mapSemanticTypeToLLVM(function->getReturnType());
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
        reportError("Return value outside of function");
        return;
    }
    
    // 类型检查
    if (type != currentFunction->returnType) {
        reportError("Return type mismatch: expected " + currentFunction->returnType + 
                   ", got " + type);
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

## 错误处理

### 函数错误检测

```cpp
class FunctionCodegen {
private:
    bool hasErrors;
    std::vector<std::string> errorMessages;
    
public:
    void reportError(const std::string& message, const ASTNode* node = nullptr) {
        hasErrors = true;
        
        std::string fullMessage = "Function Generation Error: " + message;
        if (node) {
            fullMessage += " at line " + std::to_string(node->getLine());
        }
        
        errorMessages.push_back(fullMessage);
        std::cerr << fullMessage << std::endl;
    }
    
    void reportParameterError(const std::string& functionName, 
                            const std::string& paramName,
                            const std::string& error) {
        reportError("Parameter error in function " + functionName + 
                   ", parameter " + paramName + ": " + error);
    }
    
    void reportReturnError(const std::string& functionName, 
                         const std::string& expectedType,
                         const std::string& actualType) {
        reportError("Return type mismatch in function " + functionName + 
                   ": expected " + expectedType + ", got " + actualType);
    }
    
    bool hasGenerationErrors() const {
        return hasErrors;
    }
    
    const std::vector<std::string>& getErrorMessages() const {
        return errorMessages;
    }
};
```

### 错误恢复策略

```cpp
std::string FunctionCodegen::generateFunctionCallWithErrorRecovery(const CallExpression* call) {
    try {
        return generateFunctionCall(call);
    } catch (const GenerationError& error) {
        reportError(error.what());
        
        // 生成错误恢复调用
        return generateErrorRecoveryCall(call);
    }
}

std::string FunctionCodegen::generateErrorRecoveryCall(const CallExpression* call) {
    // 返回默认值
    std::string functionName = getFunctionNameFromCall(call);
    auto functionSymbol = scopeTree->LookupSymbol(functionName);
    
    if (functionSymbol && functionSymbol->kind == SymbolKind::Function) {
        auto functionType = std::dynamic_pointer_cast<FunctionType>(functionSymbol->type);
        if (functionType) {
            std::string returnType = typeMapper->mapSemanticTypeToLLVM(functionType->returnType);
            return generateDefaultValue(returnType);
        }
    }
    
    return "null";
}
```

## 性能优化

### 调用优化

```cpp
class FunctionCodegen {
private:
    // 函数调用缓存
    std::unordered_map<std::string, std::string> callCache;
    
    // 内联决策缓存
    std::unordered_map<const Function*, bool> inlineCache;
    
public:
    std::string generateOptimizedCall(const CallExpression* call) {
        std::string callKey = getCallKey(call);
        
        // 检查缓存
        auto it = callCache.find(callKey);
        if (it != callCache.end()) {
            return it->second;
        }
        
        // 生成调用
        std::string result = generateFunctionCall(call);
        
        // 缓存结果
        callCache[callKey] = result;
        
        return result;
    }
    
private:
    std::string getCallKey(const CallExpression* call) {
        // 生成调用键用于缓存
        std::string key = getFunctionNameFromCall(call);
        for (const auto& arg : call->getArguments()) {
            key += "|" + expressionGenerator->getExpressionType(arg);
        }
        return key;
    }
};
```

### 内联优化

```cpp
void FunctionCodegen::optimizeInlineDecisions() {
    // 分析函数调用频率
    analyzeCallFrequency();
    
    // 更新内联决策
    updateInlineCache();
    
    // 清理不再需要的缓存
    cleanupInlineCache();
}

void FunctionCodegen::analyzeCallFrequency() {
    // 统计函数调用频率
    std::unordered_map<std::string, int> callFrequency;
    
    // 遍历所有函数，统计调用次数
    for (const auto& function : getAllFunctions()) {
        if (function->getBody()) {
            countFunctionCalls(function->getBody(), callFrequency);
        }
    }
    
    // 基于调用频率更新内联决策
    for (const auto& [functionName, frequency] : callFrequency) {
        if (frequency > INLINE_THRESHOLD) {
            // 高频函数优先内联
            auto function = findFunction(functionName);
            if (function) {
                inlineCache[function] = true;
            }
        }
    }
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

// 函数调用测试
TEST(FunctionCodegenTest, FunctionCall) {
    setupGenerator();
    
    auto call = createFunctionCall("add", {createLiteral("10"), createLiteral("20")});
    std::string result = generator->generateFunctionCall(call);
    
    EXPECT_TRUE(result.find("_") == 0); // 应该是寄存器名
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("call i32 @add(i32 10, i32 20)") != std::string::npos);
}

// 内置函数测试
TEST(FunctionCodegenTest, BuiltinFunction) {
    setupGenerator();
    
    auto call = createFunctionCall("printlnInt", {createLiteral("42")});
    std::string result = generator->generateBuiltinCall(call);
    
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("call void @printlnInt(i32 42)") != std::string::npos);
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
    
    // 测试函数调用
    auto call = createFunctionCall("factorial", {createLiteral("5")});
    std::string result = generator->generateFunctionCall(call);
    
    // 验证生成的 IR 正确性
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(verifyFunctionIR(ir));
}

// 内联函数测试
TEST(FunctionCodegenIntegrationTest, InlineFunction) {
    setupGenerator();
    
    auto inlineFunction = createInlineFunction("square", "i32", {"x"}, {"i32"});
    generator->generateFunction(inlineFunction);
    
    auto call = createFunctionCall("square", {createLiteral("5")});
    std::string result = generator->generateFunctionCall(call);
    
    // 验证内联正确性
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(verifyInlineCorrectness(ir));
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

### 高级使用

```cpp
// 生成内联函数
auto inlineFunction = createInlineFunction("max", "i32", {"x", "y"}, {"i32", "i32"});
funcGen.generateFunction(inlineFunction);

// 生成函数调用（自动内联）
auto call = createFunctionCall("max", {createVariable("a"), createVariable("b")});
std::string result = funcGen.generateFunctionCall(call);

// 生成内置函数调用
auto printCall = createFunctionCall("printlnInt", {createLiteral("42")});
std::string printResult = funcGen.generateBuiltinCall(printCall);
// 输出：call void @printlnInt(i32 42)
```

## 总结

FunctionCodegen 组件是 IR 生成阶段的核心组件，提供了完整的函数生成功能：

1. **完整的函数支持**：支持所有 Rx 语言函数特性的 IR 生成
2. **调用约定处理**：正确处理函数调用约定和参数传递
3. **内联优化**：智能的内联决策和生成
4. **返回值管理**：处理各种返回值类型和返回语句
5. **错误处理**：完善的函数相关错误检测和恢复机制
6. **性能优化**：调用缓存、内联优化等性能提升
7. **易于扩展**：清晰的接口设计，便于添加新的函数特性

通过 FunctionCodegen，IR 生成器可以将复杂的 Rx 语言函数正确地转换为高效的 LLVM IR 代码，并与 ExpressionGenerator、StatementGenerator、IRBuilder、TypeMapper 等组件形成完整的代码生成流水线。
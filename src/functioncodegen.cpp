#include "functioncodegen.hpp"
#include <stdexcept>
#include <sstream>
#include <vector>

// 包含 StatementGenerator 和 ExpressionGenerator 头文件以解决前向声明问题
#include "statementgenerator.hpp"
#include "expressiongenerator.hpp"

// ==================== 构造函数和基本初始化方法 ====================

FunctionCodegen::FunctionCodegen(std::shared_ptr<IRBuilder> irBuilder,
                                 std::shared_ptr<TypeMapper> typeMapper,
                                 std::shared_ptr<ScopeTree> scopeTree,
                                 const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& nodeTypeMap)
    : irBuilder(irBuilder)
    , typeMapper(typeMapper)
    , scopeTree(scopeTree)
    , expressionGenerator(nullptr)
    , statementGenerator(nullptr)
    , nodeTypeMap(nodeTypeMap)
    , currentFunction(nullptr)
    , hasErrors(false)
{
    // 验证依赖组件
    if (!irBuilder) {
        reportError("IRBuilder cannot be null");
    }
    if (!typeMapper) {
        reportError("TypeMapper cannot be null");
    }
    if (!scopeTree) {
        reportError("ScopeTree cannot be null");
    }
    
    // 初始化内置函数类型缓存
    initializeBuiltinFunctionTypes();
}

// ==================== 依赖组件设置接口 ====================

void FunctionCodegen::setExpressionGenerator(ExpressionGenerator* exprGen) {
    expressionGenerator = exprGen;
}

void FunctionCodegen::setStatementGenerator(StatementGenerator* stmtGen) {
    statementGenerator = stmtGen;
}

ExpressionGenerator* FunctionCodegen::getExpressionGenerator() const {
    return expressionGenerator;
}

StatementGenerator* FunctionCodegen::getStatementGenerator() const {
    return statementGenerator;
}

// ==================== 主要生成接口 ====================

bool FunctionCodegen::generateFunction(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return false;
    }
    
    try {
        // 验证函数签名
        if (!validateFunctionSignature(function)) {
            return false;
        }
        
        // 获取函数信息
        std::string functionName = getFunctionName(function);
        std::string returnType = getFunctionReturnLLVMType(function);
        
        // 进入函数上下文
        enterFunction(functionName, returnType);
        
        // 生成函数签名
        std::string signature = generateFunctionSignature(function);
        std::vector<std::string> parameters = generateParameters(function);
        
        // 生成函数定义开始
        irBuilder->emitFunctionDef(functionName, returnType, parameters);
        
        // 生成函数序言
        generatePrologue(function);
        
        // 设置参数作用域
        setupParameterScope(function);
        
        // 生成函数体
        bool success = generateFunctionBody(function);
        
        // 生成函数尾声
        generateEpilogue(function);
        
        // 结束函数定义
        irBuilder->emitFunctionEnd();
        
        // 退出函数上下文
        exitFunction();
        
        return success;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunction: " + std::string(e.what()));
        exitFunction(); // 确保清理上下文
        return false;
    }
}

bool FunctionCodegen::generateFunctionDeclaration(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return false;
    }
    
    try {
        // 验证函数签名
        if (!validateFunctionSignature(function)) {
            return false;
        }
        
        // 获取函数信息
        std::string functionName = getFunctionName(function);
        std::string returnType = getFunctionReturnLLVMType(function);
        std::vector<std::string> parameters = generateParameters(function);
        
        // 生成函数声明
        irBuilder->emitFunctionDecl(functionName, returnType, parameters);
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionDeclaration: " + std::string(e.what()));
        return false;
    }
}

bool FunctionCodegen::generateFunctionBody(std::shared_ptr<Function> function) {
    if (!function || !function->blockexpression) {
        reportError("Function or block expression is null");
        return false;
    }
    
    try {
        // 检查函数体是否为空
        if (!validateStatementGenerator()) {
            reportError("StatementGenerator not set for function body generation");
            return false;
        }
        
        // 生成函数体语句
        std::vector<std::shared_ptr<Statement>> statements = function->blockexpression->statements;
        for (const auto& stmt : statements) {
            if (!statementGenerator->generateStatement(stmt)) {
                reportError("Failed to generate statement in function body");
                return false;
            }
        }
        
        // 处理尾表达式（如果存在）
        if (function->blockexpression->expressionwithoutblock) {
            std::string tailValue = generateTailExpressionReturn(function->blockexpression->expressionwithoutblock);
            if (!tailValue.empty()) {
                // 生成返回指令
                irBuilder->emitRet(tailValue);
            }
        } else {
            // 没有尾表达式，生成默认返回值
            std::string defaultValue = generateDefaultReturn();
            if (!defaultValue.empty()) {
                // 生成返回指令，直接使用常量值
                std::string returnType = getCurrentFunctionReturnType();
                std::string instruction = "ret " + returnType + " " + defaultValue;
                irBuilder->emitInstruction(instruction);
            } else {
                irBuilder->emitRetVoid();
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionBody: " + std::string(e.what()));
        return false;
    }
}

// ==================== 内置函数调用生成 ====================

std::string FunctionCodegen::generateBuiltinCall(const std::string& functionName,
                                                 const std::vector<std::string>& args) {
    try {
        // 获取内置函数类型
        std::string functionType = getBuiltinFunctionType(functionName);
        if (functionType.empty()) {
            reportError("Unknown builtin function: " + functionName);
            return "";
        }
        
        // 解析返回类型
        std::string returnType = "void"; // 默认返回类型
        if (functionName == "getInt") {
            returnType = "i32";
        } else if (functionName == "getString") {
            returnType = "i8*";
        } else if (functionName == "builtin_memset" || functionName == "builtin_memcpy") {
            returnType = "i8*";
        }
        
        // 对于 printInt，它是一个 void 函数，所以 emitCall 会返回空字符串
        // 但应该生成调用指令。让我们手动生成调用指令
        if (returnType == "void") {
            // 手动生成 void 函数调用
            std::string instruction = "call void @" + functionName + "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) instruction += ", ";
                instruction += "i32 " + args[i]; // 假设所有参数都是 i32
            }
            instruction += ")";
            irBuilder->emitInstruction(instruction);
            return ""; // void 函数调用不返回寄存器
        } else {
            // 对于有返回值的函数，使用 IRBuilder 的 emitCall 方法
            std::string callResult = irBuilder->emitCall(functionName, args, returnType);
            return callResult;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBuiltinCall: " + std::string(e.what()));
        return "";
    }
}

// ==================== 参数处理接口 ====================

std::vector<std::string> FunctionCodegen::generateParameters(std::shared_ptr<Function> function) {
    std::vector<std::string> parameters;
    
    if (!function || !function->functionparameters) {
        return parameters;
    }
    
    try {
        const auto& functionParams = function->functionparameters->functionparams;
        
        for (size_t i = 0; i < functionParams.size(); ++i) {
            std::string paramType = getParameterLLVMType(functionParams[i]);
            std::string paramName = "param_" + std::to_string(i);
            std::string paramDecl = paramType + " %" + paramName;
            parameters.push_back(paramDecl);
        }
        
        return parameters;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateParameters: " + std::string(e.what()));
        return parameters;
    }
}

std::string FunctionCodegen::generateArgumentLoad(std::shared_ptr<FunctionParam> param, int index) {
    if (!param) {
        reportError("FunctionParam is null");
        return "";
    }
    
    try {
        // 获取参数类型
        std::string paramType = getParameterLLVMType(param);
        if (paramType.empty()) {
            reportError("Cannot determine parameter type");
            return "";
        }
        
        // 为参数分配栈空间
        std::string allocaReg = generateParameterAlloca(param, index);
        if (allocaReg.empty()) {
            reportError("Failed to allocate parameter stack space");
            return "";
        }
        
        // 存储参数值到栈空间
        // 注意：这里假设参数已经在寄存器中，实际实现需要根据调用约定处理
        std::string paramName = "param_" + std::to_string(index);
        std::string paramReg = irBuilder->newRegister(paramName);
        
        // 生成存储指令 - 参数已经在调用时传递，这里只需要分配空间
        // 实际的参数存储会在函数调用时由调用方处理
        
        // 记录参数寄存器映射
        if (currentFunction) {
            currentFunction->parameterRegisters[paramReg] = allocaReg;
        }
        
        return paramType + " %" + paramName;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateArgumentLoad: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateParameterAlloca(std::shared_ptr<FunctionParam> param, int index) {
    if (!param) {
        reportError("FunctionParam is null");
        return "";
    }
    
    try {
        // 获取参数类型
        std::string paramType = getParameterLLVMType(param);
        if (paramType.empty()) {
            reportError("Cannot determine parameter type for alloca");
            return "";
        }
        
        // 分配栈空间
        std::string allocaReg = irBuilder->emitAlloca(paramType);
        
        // 为参数设置特定的寄存器名
        std::string paramName = "param_" + std::to_string(index);
        std::string namedReg = irBuilder->newRegister(paramName, "_ptr");
        
        // 记录参数信息
        if (currentFunction) {
            currentFunction->parameters.push_back(paramName);
        }
        
        return allocaReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateParameterAlloca: " + std::string(e.what()));
        return "";
    }
}

// ==================== 返回值处理接口 ====================

bool FunctionCodegen::generateReturnStatement(std::shared_ptr<ReturnExpression> returnExpr) {
    if (!returnExpr) {
        reportError("ReturnExpression is null");
        return false;
    }
    
    try {
        // 检查是否在函数上下文中
        if (!isInFunction()) {
            reportError("Return statement outside of function");
            return false;
        }
        
        // 标记函数中有返回语句
        if (currentFunction) {
            currentFunction->hasReturnStatement = true;
        }
        
        // 生成返回值
        std::string returnValue;
        if (returnExpr->expression) {
            returnValue = generateReturnValue(returnExpr->expression);
            if (returnValue.empty()) {
                reportError("Failed to generate return value");
                return false;
            }
        } else {
            // 无返回值
            irBuilder->emitRetVoid();
            return true;
        }
        
        // 生成返回指令
        irBuilder->emitRet(returnValue);
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateReturnStatement: " + std::string(e.what()));
        return false;
    }
}

std::string FunctionCodegen::generateReturnValue(std::shared_ptr<Expression> expression) {
    if (!expression) {
        reportError("Expression is null for return value generation");
        return "";
    }
    
    try {
        // 检查是否在函数上下文中
        if (!isInFunction()) {
            reportError("Return value generation outside of function");
            return "";
        }
        
        // 验证 ExpressionGenerator
        // if (!validateExpressionGenerator()) {
        //     reportError("ExpressionGenerator not set for return value generation");
        //     return "";
        // }
        
        // 生成表达式值
        if (!validateExpressionGenerator()) {
            reportError("ExpressionGenerator not set for return value generation");
            return "";
        }
        
        std::string valueReg = expressionGenerator->generateExpression(expression);
        if (valueReg.empty()) {
            reportError("Failed to generate return value expression");
            return "";
        }
        
        // 获取表达式类型
        std::string valueType = getNodeLLVMType(expression);
        std::string expectedType = getCurrentFunctionReturnType();
        
        // 进行类型转换（如果需要）
        if (needsTypeConversion(valueType, expectedType)) {
            std::string convertedReg = generateTypeConversion(valueReg, valueType, expectedType);
            if (!convertedReg.empty()) {
                valueReg = convertedReg;
                valueType = expectedType;
            } else {
                reportError("Failed to convert return value type from " + valueType + " to " + expectedType);
                return "";
            }
        }
        
        // 处理返回值
        return handleReturnValue(valueReg, valueType);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateReturnValue: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateReturnPhi() {
    if (!isInFunction()) {
        reportError("PHI generation outside of function");
        return "";
    }
    
    try {
        // 检查是否有多个返回点
        if (!currentFunction || !currentFunction->hasMultipleReturns) {
            return ""; // 不需要 PHI 节点
        }
        
        // 创建返回基本块
        std::string returnBlock = createReturnBlock();
        
        // 生成 PHI 节点
        std::string phiReg = irBuilder->newRegister("return_phi");
        std::string returnType = getCurrentFunctionReturnType();
        
        // 构建 PHI 节点指令
        std::string phiInstruction = phiReg + " = phi " + returnType;
        for (const auto& input : currentFunction->returnInputs) {
            phiInstruction += " [ " + input.first + ", %" + input.second + " ]";
        }
        
        irBuilder->emitInstruction(phiInstruction);
        irBuilder->setRegisterType(phiReg, returnType);
        
        return phiReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateReturnPhi: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateTailExpressionReturn(std::shared_ptr<Expression> expression) {
    if (!expression) {
        reportError("Expression is null for tail expression return");
        return "";
    }
    
    try {
        // 生成尾表达式值
        std::string valueReg = generateReturnValue(expression);
        if (valueReg.empty()) {
            reportError("Failed to generate tail expression value");
            return "";
        }
        
        return valueReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTailExpressionReturn: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateDefaultReturn() {
    try {
        if (!isInFunction()) {
            reportError("Default return generation outside of function");
            return "";
        }
        
        std::string returnType = getCurrentFunctionReturnType();
        
        // 根据返回类型生成默认值
        if (returnType == "void") {
            return ""; // void 类型不需要返回值
        } else if (returnType == "i1") {
            // 布尔类型默认为 false (0)
            return "0";
        } else if (typeMapper->isIntegerType(returnType)) {
            // 整数类型默认为 0
            return "0";
        } else if (typeMapper->isPointerType(returnType)) {
            // 指针类型默认为 null (0)
            return "0";
        } else {
            reportError("Unsupported return type for default return: " + returnType);
            return "";
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateDefaultReturn: " + std::string(e.what()));
        return "";
    }
}

// ==================== 函数签名生成接口 ====================

std::string FunctionCodegen::generateFunctionSignature(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return "";
    }
    
    try {
        std::string functionName = getFunctionName(function);
        std::string returnType = getFunctionReturnLLVMType(function);
        std::string parameterList = generateParameterList(function);
        
        return returnType + " @" + functionName + "(" + parameterList + ")";
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionSignature: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateParameterList(std::shared_ptr<Function> function) {
    if (!function || !function->functionparameters) {
        return "";
    }
    
    try {
        std::ostringstream paramList;
        const auto& functionParams = function->functionparameters->functionparams;
        
        for (size_t i = 0; i < functionParams.size(); ++i) {
            if (i > 0) {
                paramList << ", ";
            }
            
            std::string paramType = getParameterLLVMType(functionParams[i]);
            std::string paramName = "param_" + std::to_string(i);
            
            paramList << paramType << " %" << paramName;
        }
        
        return paramList.str();
    }
    catch (const std::exception& e) {
        reportError("Exception in generateParameterList: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::generateFunctionType(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return "";
    }
    
    try {
        std::string returnType = getFunctionReturnLLVMType(function);
        std::string parameterList = generateParameterList(function);
        
        return returnType + " (" + parameterList + ")*";
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionType: " + std::string(e.what()));
        return "";
    }
}

// ==================== 工具方法接口 ====================

std::string FunctionCodegen::getFunctionName(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return "";
    }
    
    return getMangledName(function->identifier_name);
}

std::string FunctionCodegen::getMangledName(const std::string& baseName) {
    // 简化的名称修饰，实际实现可能需要更复杂的逻辑
    return baseName;
}

bool FunctionCodegen::isBuiltinFunction(const std::string& functionName) {
    return builtinFunctionTypes.find(functionName) != builtinFunctionTypes.end();
}

std::string FunctionCodegen::getBuiltinFunctionType(const std::string& functionName) {
    auto it = builtinFunctionTypes.find(functionName);
    if (it != builtinFunctionTypes.end()) {
        return it->second;
    }
    return "";
}

std::vector<std::string> FunctionCodegen::generateCallArguments(std::shared_ptr<CallParams> callParams) {
    std::vector<std::string> args;
    
    if (!callParams) {
        return args;
    }
    
    try {
        // 验证 ExpressionGenerator
        // if (!validateExpressionGenerator()) {
        //     reportError("ExpressionGenerator not set for call argument generation");
        //     return args;
        // }
        
        if (!validateExpressionGenerator()) {
            reportError("ExpressionGenerator not set for call argument generation");
            return args;
        }
        
        for (const auto& expr : callParams->expressions) {
            std::string argReg = expressionGenerator->generateExpression(expr);
            if (!argReg.empty()) {
                args.push_back(argReg);
            }
        }
        
        return args;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateCallArguments: " + std::string(e.what()));
        return args;
    }
}

std::string FunctionCodegen::generateStructArgument(const std::string& structReg, const std::string& structType) {
    try {
        // 对于大型结构体，使用引用传递优化
        if (typeMapper->isStructType(structType)) {
            int structSize = typeMapper->getTypeSize(structType);
            // 如果结构体大小超过某个阈值，使用引用传递
            if (structSize > 16) {
                return optimizeLargeStructParameter(structReg, structType);
            }
        }
        
        return structReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateStructArgument: " + std::string(e.what()));
        return structReg;
    }
}

// ==================== 函数上下文管理接口 ====================

void FunctionCodegen::enterFunction(const std::string& functionName, const std::string& returnType) {
    functionStack.emplace(functionName, returnType);
    currentFunction = &functionStack.top();
}

void FunctionCodegen::exitFunction() {
    if (!functionStack.empty()) {
        functionStack.pop();
    }
    
    if (functionStack.empty()) {
        currentFunction = nullptr;
    } else {
        currentFunction = &functionStack.top();
    }
}

bool FunctionCodegen::isInFunction() const {
    return currentFunction != nullptr;
}

FunctionCodegen::FunctionContext* FunctionCodegen::getCurrentFunction() {
    return currentFunction;
}

std::string FunctionCodegen::getCurrentFunctionName() const {
    if (currentFunction) {
        return currentFunction->functionName;
    }
    return "";
}

std::string FunctionCodegen::getCurrentFunctionReturnType() const {
    if (currentFunction) {
        return currentFunction->returnType;
    }
    return "";
}

// ==================== 错误处理接口 ====================

bool FunctionCodegen::hasError() const {
    return hasErrors;
}

std::vector<std::string> FunctionCodegen::getErrorMessages() const {
    return errorMessages;
}

void FunctionCodegen::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
}

void FunctionCodegen::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

// ==================== 私有辅助方法 ====================

bool FunctionCodegen::validateExpressionGenerator() {
    if (!expressionGenerator) {
        reportError("ExpressionGenerator not set");
        return false;
    }
    return true;
}

bool FunctionCodegen::validateStatementGenerator() {
    if (!statementGenerator) {
        reportError("StatementGenerator not set");
        return false;
    }
    return true;
}

void FunctionCodegen::generatePrologue(std::shared_ptr<Function> function) {
    try {
        irBuilder->emitComment("Function prologue for " + function->identifier_name);
        
        // emitFunctionDef 已经创建了入口基本块，不需要重复创建
        
        // 创建返回基本块
        if (currentFunction) {
            currentFunction->returnBlock = createReturnBlock();
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generatePrologue: " + std::string(e.what()));
    }
}

void FunctionCodegen::generateEpilogue(std::shared_ptr<Function> function) {
    try {
        irBuilder->emitComment("Function epilogue for " + function->identifier_name);
        
        // 完成返回值处理
        finalizeFunctionReturn();
    }
    catch (const std::exception& e) {
        reportError("Exception in generateEpilogue: " + std::string(e.what()));
    }
}

std::string FunctionCodegen::handleReturnValue(const std::string& valueReg, const std::string& valueType) {
    try {
        if (!currentFunction) {
            return valueReg;
        }
        
        // 标记有返回语句
        currentFunction->hasReturnStatement = true;
        
        // 添加返回值到 PHI 节点
        std::string currentBlock = irBuilder->getCurrentBasicBlock();
        addReturnValueToPhi(valueReg, currentBlock);
        
        return valueReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in handleReturnValue: " + std::string(e.what()));
        return valueReg;
    }
}

void FunctionCodegen::setupParameterScope(std::shared_ptr<Function> function) {
    try {
        irBuilder->emitComment("Setting up parameter scope");
        
        // 为每个参数分配栈空间
        if (function->functionparameters) {
            for (size_t i = 0; i < function->functionparameters->functionparams.size(); ++i) {
                auto param = function->functionparameters->functionparams[i];
                if (param) {
                    std::string paramType = getParameterLLVMType(param);
                    std::string paramName = "param_" + std::to_string(i);
                    
                    // 为参数分配栈空间
                    std::string paramReg = irBuilder->emitAlloca(paramType);
                    
                    // 将参数寄存器映射到变量名
                    irBuilder->newRegister(paramName);
                    
                    // 存储参数信息到当前函数上下文
                    if (currentFunction) {
                        currentFunction->parameters.push_back(paramName);
                        currentFunction->parameterRegisters[paramName] = paramReg;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in setupParameterScope: " + std::string(e.what()));
    }
}

void FunctionCodegen::initializeBuiltinFunctionTypes() {
    // 初始化内置函数类型缓存
    builtinFunctionTypes["print"] = "void (i8*)";
    builtinFunctionTypes["println"] = "void (i8*)";
    builtinFunctionTypes["printInt"] = "void (i32)";
    builtinFunctionTypes["printlnInt"] = "void (i32)";
    builtinFunctionTypes["getString"] = "i8* ()";
    builtinFunctionTypes["getInt"] = "i32 ()";
    builtinFunctionTypes["builtin_memset"] = "i8* (i8*, i32)";
    builtinFunctionTypes["builtin_memcpy"] = "i8* (i8*, i32)";
    builtinFunctionTypes["exit"] = "void (i32)";
}

std::string FunctionCodegen::getParameterLLVMType(std::shared_ptr<FunctionParam> param) {
    if (!param || !param->type) {
        reportError("FunctionParam or its type is null");
        return "";
    }
    
    try {
        // 将 Type 节点转换为字符串表示
        // TODO: 这里需要更复杂的类型转换逻辑
        // 暂时使用默认类型
        return "i32";
    }
    catch (const std::exception& e) {
        reportError("Exception in getParameterLLVMType: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::getFunctionReturnLLVMType(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return "void";
    }
    
    try {
        if (function->functionreturntype && function->functionreturntype->type) {
            // 将 Type 节点转换为 LLVM 类型
            // 检查是否是 TypePath
            auto typePath = std::dynamic_pointer_cast<TypePath>(function->functionreturntype->type);
            if (typePath && typePath->simplepathsegement) {
                auto simpleSegment = typePath->simplepathsegement;
                if (simpleSegment) {
                    std::string typeName = simpleSegment->identifier;
                    // 根据类型名称返回对应的 LLVM 类型
                    if (typeName == "void") {
                        return "void";
                    } else if (typeName == "i32") {
                        return "i32";
                    } else if (typeName == "i64") {
                        return "i32";  // 32位机器上i64映射为i32
                    } else if (typeName == "bool") {
                        return "i1";
                    } else if (typeName == "str") {
                        return "i8*";
                    } else {
                        // 默认为 i32
                        return "i32";
                    }
                }
            }
            // 如果无法解析类型，默认为 i32
            return "i32";
        }
        return "void";
    }
    catch (const std::exception& e) {
        reportError("Exception in getFunctionReturnLLVMType: " + std::string(e.what()));
        return "void";
    }
}

bool FunctionCodegen::functionNeedsReturnValue(std::shared_ptr<Function> function) {
    if (!function) {
        return false;
    }
    
    return function->functionreturntype && function->functionreturntype->type;
}

std::string FunctionCodegen::generateFunctionCall(const std::string& functionName,
                                                   const std::vector<std::string>& args,
                                                   const std::string& returnType) {
    try {
        return irBuilder->emitCall(functionName, args, returnType);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionCall: " + std::string(e.what()));
        return "";
    }
}

std::string FunctionCodegen::optimizeLargeStructParameter(const std::string& paramReg, const std::string& paramType) {
    try {
        // 对于大型结构体，分配临时空间并复制内容
        std::string tempReg = irBuilder->emitAlloca(paramType);
        
        // 复制结构体内容
        int structSize = typeMapper->getTypeSize(paramType);
        std::string sizeReg = irBuilder->newRegister();
        std::string instruction = sizeReg + " = add i32 0, " + std::to_string(structSize);
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(sizeReg, "i32");
        
        irBuilder->emitMemcpy(tempReg, paramReg, sizeReg);
        
        return tempReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in optimizeLargeStructParameter: " + std::string(e.what()));
        return paramReg;
    }
}

std::string FunctionCodegen::generateTypeConversion(const std::string& valueReg,
                                                     const std::string& fromType,
                                                     const std::string& toType) {
    try {
        return irBuilder->emitBitcast(valueReg, fromType, toType);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTypeConversion: " + std::string(e.what()));
        return "";
    }
}

bool FunctionCodegen::needsTypeConversion(const std::string& fromType, const std::string& toType) {
    return fromType != toType;
}

std::string FunctionCodegen::getNodeLLVMType(std::shared_ptr<ASTNode> node) {
    if (!node) {
        return "i32"; // 默认类型
    }
    
    auto it = nodeTypeMap.find(node.get());
    if (it != nodeTypeMap.end()) {
        return typeMapper->mapSemanticTypeToLLVM(it->second);
    }
    
    return "i32"; // 默认类型
}

std::string FunctionCodegen::createReturnBlock() {
    try {
        std::string returnBlock = irBuilder->newBasicBlock("return");
        return returnBlock;
    }
    catch (const std::exception& e) {
        reportError("Exception in createReturnBlock: " + std::string(e.what()));
        return "";
    }
}

void FunctionCodegen::addReturnValueToPhi(const std::string& valueReg, const std::string& sourceBlock) {
    try {
        if (currentFunction) {
            currentFunction->returnInputs.emplace_back(valueReg, sourceBlock);
            
            // 如果有多个返回点，标记需要 PHI 节点
            if (currentFunction->returnInputs.size() > 1) {
                currentFunction->hasMultipleReturns = true;
            }
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in addReturnValueToPhi: " + std::string(e.what()));
    }
}

void FunctionCodegen::finalizeFunctionReturn() {
    try {
        if (currentFunction && currentFunction->hasMultipleReturns) {
            // 生成 PHI 节点
            generateReturnPhi();
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in finalizeFunctionReturn: " + std::string(e.what()));
    }
}

bool FunctionCodegen::validateFunctionSignature(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return false;
    }
    
    if (function->identifier_name.empty()) {
        reportError("Function name is empty");
        return false;
    }
    
    // TODO: 添加更多验证逻辑
    return true;
}

std::string FunctionCodegen::handleParameterDefaultValue(std::shared_ptr<FunctionParam> param, int index) {
    // TODO: 实现参数默认值处理
    return "";
}
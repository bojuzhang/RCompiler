#include "functioncodegen.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>

// 包含 StatementGenerator 和 ExpressionGenerator 头文件以解决前向声明问题
#include "statementgenerator.hpp"
#include "expressiongenerator.hpp"

// 包含语义类型头文件
#include "symbol.hpp"
#include "typewrapper.hpp"

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

void FunctionCodegen::setBuiltinDeclarator(BuiltinDeclarator* builtinDecl) {
    builtinDeclarator = builtinDecl;
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
        
        // 预处理：收集所有内部函数
        std::vector<std::shared_ptr<Function>> nestedFunctions = preprocessNestedFunctions(function);

        scopeTree->EnterExistingScope(function.get(), 0);
        scopeTree->EnterExistingScope(function->blockexpression.get(), 0);
        
        // 先生成所有内部函数（在全局定义域）
        if (!generateNestedFunctions(nestedFunctions)) {
            reportError("Failed to generate nested functions");
            return false;
        }

        scopeTree->ExitScope();
        
        // 获取函数信息
        std::string functionName = getFunctionName(function);
        std::string returnType = getFunctionReturnLLVMType(function);
        
        // 如果返回类型是结构体，先输出结构体定义
        if (returnType.find("%struct_") == 0 && returnType.back() == '*') {
            std::string structName = returnType.substr(0, returnType.length() - 1); // 去掉 *
            irBuilder->emitComment("Forward declaration for struct: " + structName);
            // 这里不需要额外的声明，因为结构体定义会在后面生成
        }
        
        // 进入函数上下文
        enterFunction(functionName, returnType);
        
        // 生成函数签名
        std::string signature = generateFunctionSignature(function);
        std::vector<std::string> parameters = generateParameters(function);
        
        // 对于用户定义函数，返回类型应该是void
        std::string actualReturnType = returnType;
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        if (isUserDefined) {
            actualReturnType = "void";
        }
        
        // 生成函数定义开始
        irBuilder->emitFunctionDef(functionName, actualReturnType, parameters);
        
        // 生成函数序言
        generatePrologue(function);
        
        // 生成函数体（setupParameterScope 在 generateFunctionBody 内部调用）
        bool success = generateFunctionBody(function);
        
        // 生成函数尾声
        generateEpilogue(function);
        
        // 结束函数定义
        irBuilder->emitFunctionEnd();
        
        // 退出函数上下文
        exitFunction();

        scopeTree->ExitScope();
        
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
        // 首先设置参数作用域（确保参数指针在其他变量之前生成）
        setupParameterScope(function);
        
        // 检查函数体是否为空
        if (!validateStatementGenerator()) {
            reportError("StatementGenerator not set for function body generation");
            return false;
        }

        scopeTree->EnterExistingScope(function->blockexpression.get(), 0);
        
        // 生成函数体语句
        std::vector<std::shared_ptr<Statement>> statements = function->blockexpression->statements;
        for (const auto& stmt : statements) {
            if (!statementGenerator->generateStatement(stmt)) {
                reportError("Failed to generate statement in function body");
                return false;
            }
        }
        
        // 检查是否为用户定义函数（需要返回槽机制）
        bool needsReturnSlot = currentFunctionNeedsReturnSlot();
        
        // 处理尾表达式（如果存在）
        irBuilder->emitComment("Checking for tail expression");
        if (function->blockexpression->expressionwithoutblock) {
            irBuilder->emitComment("Found tail expression, processing...");
            std::string tailValue = generateTailExpressionReturn(function->blockexpression->expressionwithoutblock);
            irBuilder->emitComment("Tail expression generated value: " + tailValue);
            if (!tailValue.empty()) {
                if (needsReturnSlot) {
                    irBuilder->emitComment("User-defined function: storing to return slot");
                    // 用户定义函数：将值存储到返回槽
                    std::string returnType = getCurrentFunctionReturnType();
                    if (!storeToReturnSlot(tailValue, returnType)) {
                        reportError("Failed to store tail expression value to return slot");
                        return false;
                    }
                    // 返回void
                    irBuilder->emitRetVoid();
                    return true; // 避免继续执行
                } else {
                    irBuilder->emitComment("Builtin function: normal return");
                    // 内置函数：正常返回值
                    irBuilder->emitRet(tailValue);
                    return true; // 避免继续执行
                }
            } else {
                // 如果尾表达式生成失败，生成默认返回值
                std::string defaultValue = generateDefaultReturn();
                if (!defaultValue.empty()) {
                    std::string returnType = getCurrentFunctionReturnType();
                    if (needsReturnSlot) {
                        // 用户定义函数：存储默认值到返回槽
                        if (!storeToReturnSlot(defaultValue, returnType)) {
                            reportError("Failed to store default return value to return slot");
                            return false;
                        }
                        irBuilder->emitRetVoid();
                    } else {
                        // 内置函数：正常返回
                        std::string instruction = "ret " + returnType + " " + defaultValue;
                        irBuilder->emitInstruction(instruction);
                    }
                } else {
                    irBuilder->emitRetVoid();
                }
                return true; // 避免继续执行
            }
        } else {
            // 没有尾表达式，生成默认返回值
            std::string defaultValue = generateDefaultReturn();
            if (!defaultValue.empty()) {
                std::string returnType = getCurrentFunctionReturnType();
                if (needsReturnSlot) {
                    // 用户定义函数：存储默认值到返回槽
                    if (!storeToReturnSlot(defaultValue, returnType)) {
                        reportError("Failed to store default return value to return slot");
                        return false;
                    }
                    irBuilder->emitRetVoid();
                } else {
                    // 内置函数：正常返回
                    std::string instruction = "ret " + returnType + " " + defaultValue;
                    irBuilder->emitInstruction(instruction);
                }
            } else {
                // 对于用户定义函数，即使没有默认值，也需要确保返回void
                if (needsReturnSlot) {
                    irBuilder->emitRetVoid();
                } else {
                    irBuilder->emitRetVoid();
                }
            }
            return true; // 避免继续执行
        }

        scopeTree->ExitScope();
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionBody: " + std::string(e.what()));
        return false;
    }
}

// ==================== 内部函数预处理 ====================

std::vector<std::shared_ptr<Function>> FunctionCodegen::preprocessNestedFunctions(std::shared_ptr<Function> function) {
    std::vector<std::shared_ptr<Function>> nestedFunctions;
    
    if (!function || !function->blockexpression) {
        return nestedFunctions;
    }
    
    try {
        // 递归遍历函数体中的所有语句，查找内部函数定义
        for (const auto& stmt : function->blockexpression->statements) {
            collectNestedFunctions(stmt, nestedFunctions);
        }
        
        return nestedFunctions;
    }
    catch (const std::exception& e) {
        reportError("Exception in preprocessNestedFunctions: " + std::string(e.what()));
        return nestedFunctions;
    }
}

void FunctionCodegen::collectNestedFunctions(std::shared_ptr<Statement> statement,
                                         std::vector<std::shared_ptr<Function>>& nestedFunctions) {
    if (!statement || !statement->astnode) {
        return;
    }
    
    try {
        // 检查是否为项语句
        if (auto item = std::dynamic_pointer_cast<Item>(statement->astnode)) {
            // 检查是否为函数定义
            if (auto function = std::dynamic_pointer_cast<Function>(item->item)) {
                nestedFunctions.push_back(function);
                
                // 递归处理这个内部函数中的嵌套函数
                auto innerNested = preprocessNestedFunctions(function);
                nestedFunctions.insert(nestedFunctions.end(), innerNested.begin(), innerNested.end());
            }
        }
        // 注意：我们不需要递归处理表达式语句中的函数定义，
        // 因为在 Rx 语言中，函数定义只能作为语句出现
    }
    catch (const std::exception& e) {
        reportError("Exception in collectNestedFunctions: " + std::string(e.what()));
    }
}

bool FunctionCodegen::generateNestedFunctions(const std::vector<std::shared_ptr<Function>>& nestedFunctions) {
    bool success = true;
    
    for (const auto& nestedFunc : nestedFunctions) {
        if (!nestedFunc) {
            success = false;
            continue;
        }
        
        try {
            irBuilder->emitComment("Generating nested function: " + nestedFunc->identifier_name);
            
            // 验证函数签名
            if (!validateFunctionSignature(nestedFunc)) {
                success = false;
                continue;
            }
            
            // 获取函数信息
            std::string functionName = getFunctionName(nestedFunc);
            std::string returnType = getFunctionReturnLLVMType(nestedFunc);
            std::vector<std::string> parameters = generateParameters(nestedFunc);
            
            // 对于用户定义函数，返回类型应该是void
            std::string actualReturnType = returnType;
            bool isUserDefined = isUserDefinedFunction(nestedFunc->identifier_name);
            
            // 关键修复：检查函数返回类型是否为unit（void）
            bool isUnitType = (actualReturnType == "void");
            
            if (isUserDefined) {
                actualReturnType = "void";
            }
            
            // 生成函数定义开始（在全局定义域）
            irBuilder->emitFunctionDef(functionName, actualReturnType, parameters);
            
            // 临时进入函数上下文以生成函数体
            // 关键修复：正确设置isUserDefined标志
            enterFunction(functionName, returnType);
            if (currentFunction && isUserDefined) {
                currentFunction->isUserDefined = true;
                currentFunction->returnSlotPtr = "%return_slot";
            }
            
            // 生成函数序言
            generatePrologue(nestedFunc);
            
            // 生成函数体
            bool bodySuccess = generateFunctionBody(nestedFunc);
            
            // 生成函数尾声
            generateEpilogue(nestedFunc);
            
            // 结束函数定义
            irBuilder->emitFunctionEnd();
            
            // 退出函数上下文
            exitFunction();
            
            if (!bodySuccess) {
                success = false;
            }
        }
        catch (const std::exception& e) {
            reportError("Exception in generateNestedFunctions: " + std::string(e.what()));
            success = false;
        }
    }
    
    return success;
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
    
    if (!function) {
        return parameters;
    }
    
    try {
        // 检查是否为用户定义函数，如果是则添加返回槽参数（即使没有其他参数）
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        
        // 关键修复：检查函数返回类型是否为unit（void）
        // unit类型函数不需要return_slot参数
        std::string returnType = getFunctionReturnLLVMType(function);
        bool isUnitType = (returnType == "void");
        
        if (isUserDefined && !isUnitType) {
            std::string returnSlotParam = generateReturnSlotParameter(returnType);
            parameters.push_back(returnSlotParam);
            
            // 在函数上下文中标记需要返回槽
            if (currentFunction) {
                currentFunction->isUserDefined = true;
                currentFunction->returnSlotPtr = "%return_slot";
            }
        }
        
        // 如果没有函数参数，直接返回（只包含返回槽参数，如果有的话）
        if (!function->functionparameters) {
            return parameters;
        }
        
        const auto& functionParams = function->functionparameters->functionparams;
        size_t paramIndex = (isUserDefined && !isUnitType) ? 1 : 0; // 如果有返回槽，参数索引从1开始
        
        // 首先处理 self 参数（如果存在）
        if (function->functionparameters->hasSelfParam) {
            // self 参数统一传递结构体类型指针
            // 需要从函数名中提取 impl 的目标类型
            std::string structType = "%struct_*"; // 默认类型
            std::string functionName = function->identifier_name;
            size_t underscorePos = functionName.find('_');
            if (underscorePos != std::string::npos) {
                std::string structName = functionName.substr(0, underscorePos);
                structType = "%struct_" + structName;
            }
            
            // 添加 self 参数到参数列表
            std::string paramDecl = structType + "* %self";
            parameters.push_back(paramDecl);
            paramIndex++;
        }
        
        // 处理剩余的普通参数
        // 如果有 self 参数，需要跳过 functionparams 中的第一个参数（如果它是 self 的重复）
        size_t startIndex = (function->functionparameters->hasSelfParam && functionParams.size() > 0) ? 1 : 0;
        
        for (size_t i = startIndex; i < functionParams.size(); ++i) {
            std::string paramType = getParameterLLVMType(functionParams[i], i);
            std::string paramName = "param_" + std::to_string(paramIndex);
            std::string paramDecl = paramType + " %" + paramName;
            parameters.push_back(paramDecl);
            paramIndex++;
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
        std::string paramType = getParameterLLVMType(param, static_cast<size_t>(index));
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
        std::string paramType = getParameterLLVMType(param, static_cast<size_t>(index));
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
        
        // 检查是否为用户定义函数（需要返回槽机制）
        bool needsReturnSlot = currentFunctionNeedsReturnSlot();
        
        // 生成返回值
        if (returnExpr->expression) {
            // 验证 ExpressionGenerator
            if (!validateExpressionGenerator()) {
                reportError("ExpressionGenerator not set for return value generation");
                return false;
            }
            
            // 生成表达式值
            std::string returnValue = expressionGenerator->generateExpression(returnExpr->expression);
            if (returnValue.empty()) {
                reportError("Failed to generate return value expression");
                return false;
            }
            
            // 获取表达式类型
            std::string valueType = getNodeLLVMType(returnExpr->expression);
            std::string expectedType = getCurrentFunctionReturnType();

            // 聚合类型实际值为指针类型
            if (irBuilder->isAggregateType(valueType)) {
                valueType += "*";
            }
            
            // 进行类型转换（如果需要）
            if (needsTypeConversion(valueType, expectedType)) {
                std::string convertedReg = generateTypeConversion(returnValue, valueType, expectedType);
                if (!convertedReg.empty()) {
                    returnValue = convertedReg;
                    valueType = expectedType;
                } else {
                    reportError("Failed to convert return value type from " + valueType + " to " + expectedType);
                    return false;
                }
            }
            
            if (needsReturnSlot) {
                // 用户定义函数：将值存储到返回槽
                std::string returnType = getCurrentFunctionReturnType();
                if (!storeToReturnSlot(returnValue, returnType)) {
                    reportError("Failed to store return value to return slot");
                    return false;
                }
                // 返回void
                irBuilder->emitRetVoid();
                return true; // 避免继续执行
            } else {
                // 内置函数：正常返回值
                irBuilder->emitRet(returnValue);
                return true; // 避免继续执行
            }
        } else {
            // 无返回值
            irBuilder->emitRetVoid();
        }
        
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
        
        // 直接返回值寄存器，让调用者处理存储
        return valueReg;
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
        // 验证 ExpressionGenerator
        if (!validateExpressionGenerator()) {
            reportError("ExpressionGenerator not set for tail expression return");
            return "";
        }
        
        // 直接使用 ExpressionGenerator 生成表达式值
        std::string valueReg = expressionGenerator->generateExpression(expression);
        if (valueReg.empty()) {
            reportError("Failed to generate tail expression value");
            return "";
        }
        
        // 获取表达式类型
        std::string valueType = getNodeLLVMType(expression);
        std::string expectedType = getCurrentFunctionReturnType();
        
        // 进行类型转换（如果需要）
        // 对于聚合类型，不需要进行类型转换，直接使用指针
        if (irBuilder->isAggregateType(valueType) || irBuilder->isAggregateType(expectedType)) {
            // 聚合类型：不需要转换，直接使用指针
        } else if (needsTypeConversion(valueType, expectedType)) {
            std::string convertedReg = generateTypeConversion(valueReg, valueType, expectedType);
            if (!convertedReg.empty()) {
                valueReg = convertedReg;
                valueType = expectedType;
            } else {
                reportError("Failed to convert tail expression type from " + valueType + " to " + expectedType);
                return "";
            }
        }
        
        // 对于用户定义函数，这里不直接返回，而是返回值寄存器
        // 调用者负责将值存储到返回槽中
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
        
        // 检查是否为用户定义函数，如果是则返回void
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        
        // 关键修复：检查函数返回类型是否为unit（void）
        std::string actualReturnType = getFunctionReturnLLVMType(function);
        bool isUnitType = (actualReturnType == "void");
        
        if (isUserDefined) {
            returnType = "void";
        }
        
        // 对于用户定义函数，需要包含返回槽参数（除非是unit类型）
        std::string parameterList;
        if (isUserDefined && !isUnitType) {
            // 获取实际的返回类型
            std::string actualReturnType = getFunctionReturnLLVMType(function);
            // 关键修复：返回槽类型应该是 T*，而不是 T**
            // 如果 actualReturnType 已经是 T*，那么返回槽就是 T* %return_slot
            // 如果 actualReturnType 是 T，那么返回槽是 T* %return_slot
            if (actualReturnType.find("*") != std::string::npos) {
                parameterList = actualReturnType + " %return_slot";
            } else {
                parameterList = actualReturnType + "* %return_slot";
            }
            
            // 添加其他参数 - 修复：不要调用generateParameterList，直接在这里处理
            // 避免重复添加返回槽参数
            if (function->functionparameters) {
                const auto& functionParams = function->functionparameters->functionparams;
                size_t paramIndex = 1; // 从1开始，因为0是返回槽
                
                // 首先处理 self 参数（如果存在）
                if (function->functionparameters->hasSelfParam) {
                    // self 参数统一传递结构体类型指针
                    std::string structType = "%struct_*"; // 默认类型
                    std::string functionName = function->identifier_name;
                    size_t underscorePos = functionName.find('_');
                    if (underscorePos != std::string::npos) {
                        std::string structName = functionName.substr(0, underscorePos);
                        structType = "%struct_" + structName;
                    }
                    
                    parameterList += ", " + structType + "* %self";
                    paramIndex++;
                }
                
                // 处理剩余的普通参数
                size_t startIndex = (function->functionparameters->hasSelfParam && functionParams.size() > 0) ? 1 : 0;
                
                for (size_t i = startIndex; i < functionParams.size(); ++i) {
                    std::string paramType = getParameterLLVMType(functionParams[i], i);
                    std::string paramName = "param_" + std::to_string(paramIndex);
                    
                    parameterList += ", " + paramType + " %" + paramName;
                    paramIndex++;
                }
            }
        } else {
            parameterList = generateParameterList(function);
        }
        
        std::string signature = returnType + " @" + functionName + "(" + parameterList + ")";
        irBuilder->emitComment("Generated function signature: " + signature);
        return signature;
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
        
        // 检查是否为用户定义函数
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        
        // 关键修复：检查函数返回类型是否为unit（void）
        std::string returnType = getFunctionReturnLLVMType(function);
        bool isUnitType = (returnType == "void");
        
        // 对于用户定义函数，总是添加返回槽参数作为第一个参数（除非是unit类型）
        if (isUserDefined && !isUnitType) {
            // 关键修复：返回槽类型应该是 T*，而不是 T**
            // 如果 returnType 已经是 T*，那么返回槽就是 T* %return_slot
            // 如果 returnType 是 T，那么返回槽是 T* %return_slot
            if (returnType.find("*") != std::string::npos) {
                paramList << returnType << " %return_slot";
            } else {
                paramList << returnType << "* %return_slot";
            }
        }
        
        // 首先处理 self 参数（如果存在）
        size_t paramIndex = 0;
        
        // 检查是否有 self 参数
        if (function->functionparameters->hasSelfParam) {
            // self 参数统一传递结构体类型指针
            // 需要从函数名中提取 impl 的目标类型
            std::string structType = "%struct_*"; // 默认类型
            std::string functionName = function->identifier_name;
            size_t underscorePos = functionName.find('_');
            if (underscorePos != std::string::npos) {
                std::string structName = functionName.substr(0, underscorePos);
                structType = "%struct_" + structName;
            }
            
            // 添加 self 参数到参数列表
            if (paramIndex > 0 || isUserDefined) {
                paramList << ", ";
            }
            paramList << structType << "* %self";
            paramIndex++;
        }
        
        // 处理剩余的普通参数
        // 对于用户定义函数，参数从 param_1 开始（param_0 是返回槽）
        // 对于内置函数，参数从 param_0 开始
        size_t startParamIndex = isUserDefined ? 1 : 0;
        
        for (size_t i = 0; i < functionParams.size(); ++i) {
            if (paramIndex > 0 || isUserDefined) {
                paramList << ", ";
            }
            
            std::string paramType = getParameterLLVMType(functionParams[i], i);
            std::string paramName = "param_" + std::to_string(paramIndex + startParamIndex);
            
            paramList << paramType << " %" << paramName;
            paramIndex++;
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
        
        // 检查是否为用户定义函数，如果是则返回void
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        
        // 关键修复：检查函数返回类型是否为unit（void）
        std::string actualReturnType = getFunctionReturnLLVMType(function);
        bool isUnitType = (actualReturnType == "void");
        
        if (isUserDefined) {
            returnType = "void";
        }
        
        // 对于用户定义函数，需要包含返回槽参数（除非是unit类型）
        std::string parameterList;
        if (isUserDefined && !isUnitType) {
            // 获取实际的返回类型
            std::string actualReturnType = getFunctionReturnLLVMType(function);
            // 关键修复：返回槽类型应该是 T*，而不是 T**
            // 如果 actualReturnType 已经是 T*，那么返回槽就是 T* %return_slot
            // 如果 actualReturnType 是 T，那么返回槽是 T* %return_slot
            if (actualReturnType.find("*") != std::string::npos) {
                parameterList = actualReturnType + " %return_slot";
            } else {
                parameterList = actualReturnType + "* %return_slot";
            }
            
            // 添加其他参数
            std::string otherParams = generateParameterList(function);
            if (!otherParams.empty()) {
                parameterList += ", " + otherParams;
            }
        } else {
            parameterList = generateParameterList(function);
        }
        
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
        
        if (!function->functionparameters) {
            return;
        }
        
        size_t paramIndex = 0;
        
        // 检查是否为用户定义函数，处理返回槽参数
        bool isUserDefined = isUserDefinedFunction(function->identifier_name);
        
        // 关键修复：检查函数返回类型是否为unit（void）
        // unit类型函数不需要return_slot参数
        std::string returnType = getFunctionReturnLLVMType(function);
        bool isUnitType = (returnType == "void");
        
        if (isUserDefined && !isUnitType) {
            irBuilder->emitComment("Setting up return slot parameter for user-defined function");
            
            // 返回槽参数是第一个参数，名为 %return_slot
            std::string returnSlotParam = "%return_slot";
            
            // 关键修复：将返回槽参数注册到IRBuilder的寄存器系统中
            std::string returnType = getFunctionReturnLLVMType(function);
            std::string returnSlotType;
            
            // 根据sret约定，返回槽类型应该始终是 T*
            if (returnType.find("*") != std::string::npos) {
                // 返回类型已经是指针（聚合类型），返回槽类型是相同的指针类型
                returnSlotType = returnType;
            } else {
                // 返回类型是基本类型，返回槽类型是指向该类型的指针
                returnSlotType = returnType + "*";
            }
            
            // 注册返回槽参数到IRBuilder
            irBuilder->registerVariableToCurrentScope("return_slot", returnSlotParam, returnSlotType);
            
            // 将返回槽参数添加到符号表中
            auto currentScope = scopeTree->GetCurrentScope();
            if (currentScope) {
                // 创建返回槽参数符号
                auto returnSlotSymbol = std::make_shared<Symbol>("return_slot", SymbolKind::Variable);
                returnSlotSymbol->ismutable = true; // 返回槽可变
                
                // 设置返回槽的类型为指针类型，指向实际的返回类型
                // 创建一个简单的指针类型，使用字符串表示
                auto pointerType = std::make_shared<SimpleType>(returnSlotType);
                returnSlotSymbol->type = pointerType;
                
                // 添加到当前作用域
                currentScope->Insert("return_slot", returnSlotSymbol);
                
                // 存储参数信息到当前函数上下文
                if (currentFunction) {
                    currentFunction->parameters.push_back("return_slot");
                    currentFunction->parameterRegisters["return_slot"] = returnSlotParam;
                }
            }
            
            paramIndex++;
        }
        
        // 首先处理 self 参数（如果存在）
        if (function->functionparameters->hasSelfParam) {
            irBuilder->emitComment("Setting up self parameter");
            
            // self 参数统一传递结构体类型指针
            // 需要从函数名中提取 impl 的目标类型
            std::string structType = "%struct_*"; // 默认类型
            std::string functionName = function->identifier_name;
            size_t underscorePos = functionName.find('_');
            if (underscorePos != std::string::npos) {
                std::string structName = functionName.substr(0, underscorePos);
                structType = "%struct_" + structName;
            }
            
            // 创建 self 参数的指针变量
            std::string paramPtrReg = irBuilder->newRegister("self", "_ptr");
            
            // 根据用户要求，使用 bitcast 原类型到原类型实现显式的指针赋值
            std::string incomingParamName = "%self"; // self 参数在签名中直接命名为 %self
            
            // 生成 bitcast 指令：%self_ptr = bitcast %struct_Node* %self to %struct_Node*
            std::string bitcastInstruction = paramPtrReg + " = bitcast " + structType + "* " + incomingParamName + " to " + structType + "*";
            irBuilder->emitInstruction(bitcastInstruction);
            irBuilder->setRegisterType(paramPtrReg, structType + "*");
            
            // 将 self 参数添加到符号表中作为变量
            auto currentScope = scopeTree->GetCurrentScope();
            if (currentScope) {
                // 创建 self 参数符号
                auto paramSymbol = std::make_shared<Symbol>("self", SymbolKind::Variable);
                paramSymbol->ismutable = true; // self 参数默认可变
                
                // 设置 self 参数的类型为结构体指针类型
                // 这里需要根据实际的 impl 类型来设置
                if (underscorePos != std::string::npos) {
                    std::string structName = functionName.substr(0, underscorePos);
                    // 创建对应的结构体类型
                    paramSymbol->type = std::make_shared<SimpleType>(structName);
                }
                
                // 添加到当前作用域
                currentScope->Insert("self", paramSymbol);
            }
            
            // 存储参数信息到当前函数上下文
            if (currentFunction) {
                currentFunction->parameters.push_back("self");
                currentFunction->parameterRegisters["self"] = paramPtrReg;
            }
            
            paramIndex++;
        }
        
        // 处理剩余的普通参数
        // 计算起始索引，考虑返回槽参数和self参数
        size_t startIndex = 0;
        if (isUserDefined && !isUnitType) {
            // 用户定义函数（非unit类型）：跳过返回槽参数
            startIndex = 0;
        }
        if (function->functionparameters->hasSelfParam && function->functionparameters->functionparams.size() > 0) {
            // 如果有self参数，且functionparams中有重复的self，需要跳过
            startIndex = 1;
        }
        
        for (size_t i = startIndex; i < function->functionparameters->functionparams.size(); ++i) {
            auto param = function->functionparameters->functionparams[i];
            if (param) {
                std::string paramType = getParameterLLVMType(param, i);
                
                // 获取参数的实际名称
                std::string actualParamName = getParameterName(param);
                if (actualParamName.empty()) {
                    actualParamName = "param_" + std::to_string(paramIndex);
                }
                
                // 对于普通变量参数，使用参数声明的实际类型
                // 从 nodeTypeMap 获取参数的语义类型，然后映射到 LLVM 类型
                std::string actualParamType = "i32"; // 默认类型
                
                auto it = nodeTypeMap.find(param.get());
                if (it != nodeTypeMap.end() && it->second) {
                    actualParamType = typeMapper->mapSemanticTypeToLLVM(it->second);
                } else if (param->type) {
                    actualParamType = typeMapper->mapASTTypeToLLVM(param->type);
                }
                
                // 将传入的参数值存储到分配的栈空间中
                // 传入的参数在函数签名中的命名与 generateParameters 保持一致
                std::string incomingParamName;
                if (isUserDefined && !isUnitType) {
                    // 用户定义函数（非unit类型）：参数从 %param_1 开始（%param_0 是返回槽）
                    incomingParamName = "%param_" + std::to_string(paramIndex);
                } else if (function->functionparameters->hasSelfParam) {
                    // 如果有self参数，普通参数从 %param_1 开始编号
                    incomingParamName = "%param_" + std::to_string(paramIndex);
                } else {
                    // 如果没有self参数，普通参数从 %param_0 开始编号
                    incomingParamName = "%param_" + std::to_string(i);
                }
                
                // 创建符合普通变量命名规则的指针变量名
                std::string paramPtrReg = irBuilder->newRegister(actualParamName, "_ptr");
                
                // 检查是否为指针类型参数
                if (typeMapper->isPointerType(paramType) && irBuilder->isAggregateType(typeMapper->getPointedType(paramType))) {
                    // 指针类型参数：不再新 alloca 一个变量出来，直接通过 bitcast 实现指针间的 SSA 赋值
                    irBuilder->emitComment("Direct bitcast for pointer parameter");
                    
                    // 根据用户要求，使用 bitcast 原类型到原类型实现显式的指针赋值
                    // 生成 bitcast 指令：%param_ptr = bitcast T* %param_X to T*
                    std::string bitcastInstruction = paramPtrReg + " = bitcast " + paramType + " " + incomingParamName + " to " + paramType;
                    irBuilder->emitInstruction(bitcastInstruction);
                    irBuilder->setRegisterType(paramPtrReg, paramType);
                    
                    // 手动注册这个变量到 IRBuilder 的寄存器映射中，确保 getVariableRegister 能找到它
                    irBuilder->registerVariableToCurrentScope(actualParamName, paramPtrReg, paramType);
                } else {
                    // 非指针类型参数：使用实际的参数类型进行 alloca
                    std::string allocaInstruction = paramPtrReg + " = alloca " + actualParamType + ", align 4";
                    irBuilder->emitInstruction(allocaInstruction);
                    irBuilder->setRegisterType(paramPtrReg, actualParamType + "*");
                }
                
                // 对于指针类型参数，跳过后续的存储处理，因为已经通过 bitcast 处理了
                if (typeMapper->isPointerType(paramType) && irBuilder->isAggregateType(typeMapper->getPointedType(paramType))) {
                    // 指针类型参数已经通过 bitcast 处理，不需要额外的存储操作
                } else if (typeMapper->isStructType(actualParamType) || typeMapper->isArrayType(actualParamType)) {
                    // 聚合类型参数：传入的是指针，需要使用 memcpy 复制内容
                    irBuilder->emitComment("Copying aggregate parameter using memcpy");
                    
                    // 计算类型大小
                    int typeSize = typeMapper->getTypeSize(actualParamType);
                    std::string sizeReg = irBuilder->newRegister();
                    std::string sizeInstruction = sizeReg + " = add i32 0, " + std::to_string(typeSize);
                    irBuilder->emitInstruction(sizeInstruction);
                    irBuilder->setRegisterType(sizeReg, "i32");
                    
                    // 调用 builtin_memcpy 复制数据
                    irBuilder->emitMemcpy(paramPtrReg, incomingParamName, sizeReg);
                } else {
                    // 基本类型参数：直接存储，确保类型匹配
                    // 手动生成存储指令，确保类型正确
                    std::string storeInstruction = "store " + actualParamType + " " + incomingParamName + ", " + actualParamType + "* " + paramPtrReg;
                    irBuilder->emitInstruction(storeInstruction);
                }
                
                // 将参数添加到符号表中作为变量
                auto currentScope = scopeTree->GetCurrentScope();
                if (currentScope) {
                    // 创建参数符号
                    auto paramSymbol = std::make_shared<Symbol>(actualParamName, SymbolKind::Variable);
                    
                    // 从参数类型获取语义类型
                    if (param->type) {
                        // 从 nodeTypeMap 获取参数的语义类型
                        auto it = nodeTypeMap.find(param.get());
                        if (it != nodeTypeMap.end()) {
                            paramSymbol->type = it->second;
                        } else {
                            // 回退到字符串解析
                            if (auto typePath = std::dynamic_pointer_cast<TypePath>(param->type)) {
                                if (typePath->simplepathsegement) {
                                    std::string typeName = typePath->simplepathsegement->identifier;
                                    // 创建对应的语义类型
                                    if (typeName == "i32") {
                                        paramSymbol->type = std::make_shared<SignedIntType>();
                                    } else if (typeName == "bool") {
                                        paramSymbol->type = std::make_shared<SimpleType>("bool");
                                    } else {
                                        paramSymbol->type = std::make_shared<SignedIntType>(); // 默认类型
                                    }
                                }
                            }
                        }
                    }
                    
                    paramSymbol->ismutable = true; // 参数默认可变
                    
                    // 添加到当前作用域
                    currentScope->Insert(actualParamName, paramSymbol);
                }
                
                // 存储参数信息到当前函数上下文
                if (currentFunction) {
                    currentFunction->parameters.push_back(actualParamName);
                    currentFunction->parameterRegisters[actualParamName] = paramPtrReg;
                }
                
                paramIndex++;
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

std::string FunctionCodegen::getParameterLLVMType(std::shared_ptr<FunctionParam> param, size_t paramIndex) {
    if (!param) {
        reportError("FunctionParam is null");
        return "";
    }
    
    try {
        // 优先从 nodeTypeMap 获取参数类型
        auto it = nodeTypeMap.find(param.get());
        if (it != nodeTypeMap.end() && it->second) {
            std::string llvmType = typeMapper->mapSemanticTypeToLLVM(it->second);
            
            // 检查是否为聚合类型，如果是则改为指针类型
            if (typeMapper->isStructType(llvmType) || typeMapper->isArrayType(llvmType)) {
                return llvmType + "*";
            }
            
            return llvmType;
        }
        
        // 如果 nodeTypeMap 中没有，则从 AST 类型节点获取
        if (!param->type) {
            reportError("FunctionParam type is null");
            return "i32";
        }
        
        // 检查是否为 Self 类型
        if (auto typePath = std::dynamic_pointer_cast<TypePath>(param->type)) {
            if (typePath->simplepathsegement && typePath->simplepathsegement->isSelf) {
                // Self 类型参数需要从当前函数的上下文中获取实际的类型
                // 这里需要获取当前函数所属的 impl 类型
                if (currentFunction) {
                    std::string functionName = currentFunction->functionName;
                    size_t underscorePos = functionName.find('_');
                    if (underscorePos != std::string::npos) {
                        std::string structName = functionName.substr(0, underscorePos);
                        return "%struct_" + structName + "*";
                    }
                }
                return "%struct_*"; // 默认类型
            }
        }
        
        // 使用 TypeMapper 的新方法直接从 AST Type 节点映射到 LLVM 类型
        std::string llvmType = typeMapper->mapASTTypeToLLVM(param->type);
        
        // 检查是否为聚合类型，如果是则改为指针类型
        if ((typeMapper->isStructType(llvmType) || typeMapper->isArrayType(llvmType)) && (!llvmType.ends_with('*'))) {
            return llvmType + "*";
        }
        
        return llvmType;
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
            // 首先检查是否为 Self 类型
            if (auto typePath = std::dynamic_pointer_cast<TypePath>(function->functionreturntype->type)) {
                if (typePath->simplepathsegement) {
                    // 检查是否为 Self 类型
                    if (typePath->simplepathsegement->isSelf) {
                        // 从函数名中提取 impl 的目标类型
                        std::string functionName = function->identifier_name;
                        size_t underscorePos = functionName.find('_');
                        if (underscorePos != std::string::npos) {
                            std::string structName = functionName.substr(0, underscorePos);
                            // 对于结构体类型，函数应该返回指针类型
                            return "%struct_" + structName + "*";
                        }
                        return "%struct_*"; // 默认类型
                    }
                }
            }
            
            // 使用 TypeMapper 的新方法直接从 AST Type 节点映射到 LLVM 类型
            std::string llvmType = typeMapper->mapASTTypeToLLVM(function->functionreturntype->type);
            
            // 对于结构体类型，函数应该返回指针类型
            if (typeMapper->isStructType(llvmType)) {
                return llvmType + "*";
            }
            
            return llvmType;
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

std::string FunctionCodegen::getParameterName(std::shared_ptr<FunctionParam> param) {
    if (!param || !param->patternnotopalt) {
        return "";
    }
    
    try {
        // 尝试将 pattern 转换为 IdentifierPattern
        if (auto identifierPattern = std::dynamic_pointer_cast<IdentifierPattern>(param->patternnotopalt)) {
            return identifierPattern->identifier;
        }
        
        // 如果不是 IdentifierPattern，返回空字符串
        return "";
    }
    catch (const std::exception& e) {
        reportError("Exception in getParameterName: " + std::string(e.what()));
        return "";
    }
}

// ==================== 新增的返回槽机制支持方法 ====================

bool FunctionCodegen::isUserDefinedFunction(const std::string& functionName) {
    // 检查是否为内置函数
    if (isBuiltinFunction(functionName)) {
        return false;
    }
    
    // main函数是特殊的，不应该使用sret机制
    if (functionName == "main") {
        return false;
    }
    
    // 所有其他非内置函数都是用户定义函数，包括 associated function
    // 例如：Node_new, Node_sum 等都需要 sret 机制
    return true;
}

std::string FunctionCodegen::generateReturnSlotParameter(const std::string& returnType) {
    // 生成返回槽参数：指向返回类型的指针
    // 根据sret约定，返回槽类型应该是 T*，其中T是实际的返回类型
    // 关键修复：返回槽类型应该始终是 T*，而不是 T**
    
    if (returnType.find("*") != std::string::npos) {
        // 返回类型已经是指针（聚合类型），返回槽应该是相同的指针类型 T*
        // 例如：如果返回类型是 %struct_Node*，返回槽应该是 %struct_Node* %return_slot
        return returnType + " %return_slot";
    } else {
        // 返回类型不是指针（基本类型），返回槽应该是指向返回类型的指针 T*
        // 例如：如果返回类型是 i32，返回槽应该是 i32* %return_slot
        return returnType + "* %return_slot";
    }
}

bool FunctionCodegen::storeToReturnSlot(const std::string& valueReg, const std::string& returnType) {
    try {
        if (!currentFunction || currentFunction->returnSlotPtr.empty()) {
            reportError("No return slot available for user-defined function");
            return false;
        }
        
        // 将值存储到返回槽中
        std::string returnSlotPtr = currentFunction->returnSlotPtr;
        irBuilder->emitComment("Storing value " + valueReg + " to return slot " + returnSlotPtr);
        
        // 生成存储指令
        // 对于返回槽，我们需要手动构建存储指令，因为返回槽可能不在寄存器映射中
        std::string valueRegType = irBuilder->getRegisterType(valueReg);
        
        // 关键修复：确定正确的存储类型
        // 对于聚合类型返回值，我们需要存储结构体的值到返回槽中
        // 返回槽的类型应该始终是 T*，其中T是基础类型
        
        std::string storeType = valueRegType;
        std::string returnSlotType;
        
        if (storeType.empty()) {
            storeType = returnType; // 使用返回类型作为默认
        }
        
        // 根据sret约定，返回槽类型应该始终是 T*
        if (returnType.find("*") != std::string::npos) {
            // 返回类型已经是指针（聚合类型），返回槽类型是相同的指针类型
            returnSlotType = returnType;
            
            // 关键修复：对于聚合类型，如果valueReg是指针，我们需要使用memcpy复制内容
            // 而不是存储指针本身
            if (storeType == returnType) {
                // valueReg的类型与返回槽类型相同，都是T*
                // 这意味着我们需要复制指针指向的内容，而不是指针本身
                irBuilder->emitComment("Copying aggregate return value using memcpy");
                
                // 计算类型大小
                std::string baseType = returnType.substr(0, returnType.length() - 1); // 去掉 *
                int typeSize = typeMapper->getTypeSize(baseType);
                std::string sizeReg = irBuilder->newRegister();
                std::string sizeInstruction = sizeReg + " = add i32 0, " + std::to_string(typeSize);
                irBuilder->emitInstruction(sizeInstruction);
                irBuilder->setRegisterType(sizeReg, "i32");
                
                // 关键修复：确保返回槽已注册到IRBuilder中
                // 如果返回槽未注册，先注册它
                if (irBuilder->getRegisterType(returnSlotPtr).empty()) {
                    std::string returnSlotType = returnType;
                    irBuilder->registerVariableToCurrentScope("return_slot", returnSlotPtr, returnSlotType);
                }
                
                // 使用memcpy复制结构体内容
                irBuilder->emitMemcpy(returnSlotPtr, valueReg, sizeReg);
                return true;
            }
        } else {
            // 返回类型是基本类型，返回槽类型是指向该类型的指针
            returnSlotType = returnType + "*";
        }
        
        // 手动构建存储指令
        std::string instruction = "store " + storeType + " " + valueReg + ", " + returnSlotType + " " + returnSlotPtr;
        irBuilder->emitInstruction(instruction);
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in storeToReturnSlot: " + std::string(e.what()));
        return false;
    }
}

std::string FunctionCodegen::getCurrentFunctionReturnSlot() const {
    if (currentFunction && currentFunction->isUserDefined) {
        return currentFunction->returnSlotPtr;
    }
    return "";
}

bool FunctionCodegen::currentFunctionNeedsReturnSlot() const {
    return currentFunction && currentFunction->isUserDefined;
}

// ==================== 新增的函数调用支持方法 ====================

std::string FunctionCodegen::generateUserDefinedFunctionCall(const std::string& functionName,
                                                             const std::vector<std::string>& args,
                                                             std::shared_ptr<Function> calleeFunction) {
    try {
        // 获取函数的返回类型
        std::string returnType = "i32"; // 默认返回类型
        if (calleeFunction) {
            returnType = getFunctionReturnLLVMType(calleeFunction);
        }
        
        // 分配返回槽空间 - 关键修复：返回槽类型应该始终是 T*
        // 无论返回类型是否已经是指针，我们都分配基础类型的指针空间
        std::string returnSlotPtr;
        if (returnType.find("*") != std::string::npos) {
            // 返回类型已经是指针（聚合类型），去掉*后分配基础类型的指针空间
            std::string baseType = returnType.substr(0, returnType.length() - 1);
            returnSlotPtr = irBuilder->emitAlloca(baseType);
        } else {
            // 返回类型是基本类型，分配该类型的指针空间
            returnSlotPtr = irBuilder->emitAlloca(returnType);
        }
        
        // 构建参数列表：第一个参数是返回槽指针
        std::vector<std::string> callArgs;
        callArgs.push_back(returnSlotPtr);
        
        // 添加用户提供的参数
        callArgs.insert(callArgs.end(), args.begin(), args.end());
        
        // 生成函数调用（返回类型为void）
        irBuilder->emitCall(functionName, callArgs, "void");
        
        // 对于聚合类型，直接返回返回槽指针（T*）
        // 对于非聚合类型，从返回槽加载返回值
        std::string resultReg;
        if (irBuilder->isAggregateType(returnType)) {
            // 聚合类型：返回指针 T*
            resultReg = returnSlotPtr;
        } else {
            // 非聚合类型：加载值
            resultReg = irBuilder->emitLoad(returnSlotPtr, returnType);
        }
        
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateUserDefinedFunctionCall: " + std::string(e.what()));
        return "";
    }
}
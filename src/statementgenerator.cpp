#include "statementgenerator.hpp"
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>

// 前向声明 ExpressionGenerator 类（将在实际实现中包含对应的头文件）
// #include "expressiongenerator.hpp"

// 构造函数
StatementGenerator::StatementGenerator(std::shared_ptr<IRBuilder> irBuilder,
                                     std::shared_ptr<TypeMapper> typeMapper,
                                     std::shared_ptr<ScopeTree> scopeTree)
    : irBuilder(irBuilder)
    , typeMapper(typeMapper)
    , scopeTree(scopeTree)
    , expressionGenerator(nullptr)
    , hasErrors(false)
    , isUnreachable(false)
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
}

// ==================== ExpressionGenerator 集成接口 ====================

void StatementGenerator::setExpressionGenerator(ExpressionGenerator* exprGen) {
    expressionGenerator = exprGen;
}

ExpressionGenerator* StatementGenerator::getExpressionGenerator() const {
    return expressionGenerator;
}

// ==================== 主要生成接口 ====================

bool StatementGenerator::generateStatement(std::shared_ptr<Statement> statement) {
    if (!statement) {
        reportError("Statement is null");
        return false;
    }
    
    // 检查不可达代码
    if (isUnreachable) {
        handleUnreachableCode(statement);
        return true; // 不可达代码不是错误，只是跳过
    }
    
    try {
        // 根据 Statement 的实际类型进行分发
        if (auto letStatement = std::dynamic_pointer_cast<LetStatement>(statement->astnode)) {
            return generateLetStatement(letStatement);
        }
        else if (auto exprStatement = std::dynamic_pointer_cast<ExpressionStatement>(statement->astnode)) {
            return generateExpressionStatement(exprStatement);
        }
        else if (auto item = std::dynamic_pointer_cast<Item>(statement->astnode)) {
            return generateItemStatement(item);
        }
        else {
            reportError("Unsupported statement type");
            return false;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateStatement: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateStatements(const std::vector<std::shared_ptr<Statement>>& statements) {
    bool success = true;
    
    for (const auto& statement : statements) {
        if (!generateStatement(statement)) {
            success = false;
            // 继续处理其他语句，收集所有错误
        }
    }
    
    return success;
}

// ==================== 专用生成方法 ====================

bool StatementGenerator::generateLetStatement(std::shared_ptr<LetStatement> letStatement) {
    if (!letStatement) {
        reportError("LetStatement is null");
        return false;
    }
    
    try {
        // 1. 从模式中提取变量名
        std::string variableName = extractVariableName(letStatement->patternnotopalt);
        if (variableName.empty()) {
            reportError("Cannot extract variable name from pattern");
            return false;
        }
        
        // 2. 确定变量类型
        std::string llvmType;
        if (letStatement->type) {
            // 使用显式类型
            // TODO: 这里需要从 Type 节点获取类型信息
            // 暂时使用默认类型
            llvmType = "i32";
        } else {
            // 从初始化表达式推断类型
            if (letStatement->expression) {
                // TODO: 这里需要调用 ExpressionGenerator 获取表达式类型
                // 暂时使用默认类型
                llvmType = "i32";
            } else {
                // 使用默认类型
                llvmType = "i32";
            }
        }
        
        // 3. 为变量分配栈空间
        std::string variableReg = allocateVariable(variableName, llvmType);
        if (variableReg.empty()) {
            reportError("Failed to allocate variable: " + variableName);
            return false;
        }
        
        // 4. 处理初始化
        if (letStatement->expression) {
            if (!validateExpressionGenerator()) {
                reportError("ExpressionGenerator not set for let statement initialization");
                return false;
            }
            
            // TODO: 调用 ExpressionGenerator 生成初始化表达式
            // std::string initReg = expressionGenerator->generateExpression(letStatement->expression);
            // 暂时使用空字符串
            std::string initReg = "";
            
            if (!initReg.empty()) {
                // 存储初始值到变量
                storeVariable(variableName, initReg, llvmType);
            }
        }
        
        // 5. 注册变量到符号表
        registerVariable(variableName, llvmType, variableReg);
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateLetStatement: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateExpressionStatement(std::shared_ptr<ExpressionStatement> exprStatement) {
    if (!exprStatement) {
        reportError("ExpressionStatement is null");
        return false;
    }
    
    if (!validateExpressionGenerator()) {
        reportError("ExpressionGenerator not set for expression statement");
        return false;
    }
    
    try {
        // 检查表达式是否为控制流表达式
        // TODO: 这里需要检查表达式类型，如果是控制流表达式则委托给 ExpressionGenerator
        // 暂时统一委托给 ExpressionGenerator
        
        // TODO: 调用 ExpressionGenerator 生成表达式
        // std::string resultReg = expressionGenerator->generateExpression(exprStatement->astnode);
        // 暂时使用空字符串
        std::string resultReg = "";
        
        // 对于有分号的表达式语句，丢弃结果
        if (exprStatement->hassemi && !resultReg.empty()) {
            // 添加注释说明结果被丢弃
            irBuilder->emitComment("Result of expression statement discarded");
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateExpressionStatement: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateItemStatement(std::shared_ptr<Item> item) {
    if (!item) {
        reportError("Item is null");
        return false;
    }
    
    try {
        // 根据项的具体类型进行分发
        if (auto function = std::dynamic_pointer_cast<Function>(item->item)) {
            return generateFunctionItem(function);
        }
        else if (auto structDef = std::dynamic_pointer_cast<StructStruct>(item->item)) {
            return generateStructItem(structDef);
        }
        else if (auto constant = std::dynamic_pointer_cast<ConstantItem>(item->item)) {
            return generateConstantItem(constant);
        }
        else if (auto impl = std::dynamic_pointer_cast<InherentImpl>(item->item)) {
            return generateImplItem(impl);
        }
        else {
            reportError("Unsupported item type");
            return false;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateItemStatement: " + std::string(e.what()));
        return false;
    }
}

// ==================== 控制流上下文管理 ====================

void StatementGenerator::enterControlFlowContext(const std::string& header, const std::string& body, const std::string& exit) {
    controlFlowStack.emplace(header, body, exit);
}

void StatementGenerator::exitControlFlowContext() {
    if (!controlFlowStack.empty()) {
        controlFlowStack.pop();
    }
}

StatementGenerator::ControlFlowContext StatementGenerator::getCurrentControlFlowContext() {
    if (controlFlowStack.empty()) {
        throw std::runtime_error("No control flow context available");
    }
    return controlFlowStack.top();
}

bool StatementGenerator::isInLoopContext() const {
    return !controlFlowStack.empty();
}

void StatementGenerator::addBreakTarget(const std::string& target) {
    if (!controlFlowStack.empty()) {
        controlFlowStack.top().breakTargets.push_back(target);
    }
}

void StatementGenerator::addContinueTarget(const std::string& target) {
    if (!controlFlowStack.empty()) {
        controlFlowStack.top().continueTargets.push_back(target);
    }
}

std::string StatementGenerator::getCurrentBreakTarget() {
    if (controlFlowStack.empty()) {
        throw std::runtime_error("No loop context for break statement");
    }
    const auto& targets = controlFlowStack.top().breakTargets;
    if (targets.empty()) {
        throw std::runtime_error("No break target available");
    }
    return targets.back();
}

std::string StatementGenerator::getCurrentContinueTarget() {
    if (controlFlowStack.empty()) {
        throw std::runtime_error("No loop context for continue statement");
    }
    const auto& targets = controlFlowStack.top().continueTargets;
    if (targets.empty()) {
        throw std::runtime_error("No continue target available");
    }
    return targets.back();
}

// ==================== 变量管理接口 ====================

std::string StatementGenerator::allocateVariable(const std::string& variableName, const std::string& type) {
    try {
        // 使用 IRBuilder 分配栈空间
        std::string reg = irBuilder->emitAlloca(type);
        
        // 为变量设置特定的寄存器名
        std::string variableReg = irBuilder->newRegister(variableName, "_ptr");
        
        // 如果 IRBuilder 返回的寄存器名不同，需要重新分配
        if (reg != variableReg) {
            // TODO: 这里可能需要更复杂的寄存器重命名逻辑
            // 暂时使用 IRBuilder 返回的寄存器
        }
        
        return variableReg;
    }
    catch (const std::exception& e) {
        reportError("Failed to allocate variable " + variableName + ": " + std::string(e.what()));
        return "";
    }
}

void StatementGenerator::storeVariable(const std::string& variableName, const std::string& valueReg, const std::string& type) {
    try {
        std::string variableReg = irBuilder->getVariableRegister(variableName);
        if (variableReg.empty()) {
            reportError("Variable not found: " + variableName);
            return;
        }
        
        irBuilder->emitStore(valueReg, variableReg);
    }
    catch (const std::exception& e) {
        reportError("Failed to store variable " + variableName + ": " + std::string(e.what()));
    }
}

std::string StatementGenerator::loadVariable(const std::string& variableName) {
    try {
        std::string variableReg = irBuilder->getVariableRegister(variableName);
        if (variableReg.empty()) {
            reportError("Variable not found: " + variableName);
            return "";
        }
        
        std::string type = getVariableLLVMType(variableName);
        return irBuilder->emitLoad(variableReg, type);
    }
    catch (const std::exception& e) {
        reportError("Failed to load variable " + variableName + ": " + std::string(e.what()));
        return "";
    }
}

// ==================== 工具方法 ====================

std::string StatementGenerator::getStatementType(std::shared_ptr<Statement> statement) {
    if (!statement || !statement->astnode) {
        return "unknown";
    }
    
    if (std::dynamic_pointer_cast<LetStatement>(statement->astnode)) {
        return "let";
    }
    else if (std::dynamic_pointer_cast<ExpressionStatement>(statement->astnode)) {
        return "expression";
    }
    else if (std::dynamic_pointer_cast<Item>(statement->astnode)) {
        return "item";
    }
    
    return "unknown";
}

bool StatementGenerator::isStatementTerminator(std::shared_ptr<Statement> statement) {
    // TODO: 实现语句终止符检查逻辑
    // 需要检查语句是否包含 return、break、continue 等终止符
    return false;
}

void StatementGenerator::handleUnreachableCode(std::shared_ptr<Statement> statement) {
    // 添加注释说明代码不可达
    std::string stmtType = getStatementType(statement);
    irBuilder->emitComment("Unreachable " + stmtType + " statement");
}

std::string StatementGenerator::extractVariableName(std::shared_ptr<Pattern> pattern) {
    if (!pattern) {
        return "";
    }
    
    // 尝试转换为标识符模式
    if (auto identPattern = std::dynamic_pointer_cast<IdentifierPattern>(pattern)) {
        return identPattern->identifier;
    }
    
    // TODO: 处理其他模式类型
    return "";
}

bool StatementGenerator::isVariableInCurrentScope(const std::string& variableName) {
    return irBuilder->isVariableInCurrentScope(variableName);
}

// ==================== 错误处理接口 ====================

bool StatementGenerator::hasError() const {
    return hasErrors;
}

std::vector<std::string> StatementGenerator::getErrorMessages() const {
    return errorMessages;
}

void StatementGenerator::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
}

void StatementGenerator::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

// ==================== 私有辅助方法 ====================

bool StatementGenerator::generateFunctionItem(std::shared_ptr<Function> function) {
    // TODO: 实现函数定义生成
    // 这需要与 FunctionCodegen 组件协作
    irBuilder->emitComment("Function definition: " + function->identifier_name);
    return true;
}

bool StatementGenerator::generateStructItem(std::shared_ptr<StructStruct> structDef) {
    // TODO: 实现结构体定义生成
    irBuilder->emitComment("Struct definition: " + structDef->identifier);
    return true;
}

bool StatementGenerator::generateConstantItem(std::shared_ptr<ConstantItem> constant) {
    // TODO: 实现常量定义生成
    irBuilder->emitComment("Constant definition: " + constant->identifier);
    return true;
}

bool StatementGenerator::generateImplItem(std::shared_ptr<InherentImpl> impl) {
    // TODO: 实现 impl 块生成
    irBuilder->emitComment("Impl block");
    return true;
}

std::string StatementGenerator::createUnreachableBlock() {
    std::string blockName = irBuilder->newBasicBlock("unreachable");
    irBuilder->emitLabel(blockName);
    irBuilder->emitInstruction("unreachable");
    return blockName;
}

void StatementGenerator::setUnreachable() {
    isUnreachable = true;
}

void StatementGenerator::resetReachable() {
    isUnreachable = false;
}

bool StatementGenerator::checkUnreachable() const {
    return isUnreachable;
}

bool StatementGenerator::validateExpressionGenerator() {
    if (!expressionGenerator) {
        reportError("ExpressionGenerator not set");
        return false;
    }
    return true;
}

std::string StatementGenerator::getVariableLLVMType(const std::string& variableName) {
    // TODO: 从符号表获取变量的类型信息
    // 暂时返回默认类型
    return "i32";
}

void StatementGenerator::registerVariable(const std::string& variableName, const std::string& type, const std::string& registerName) {
    // TODO: 创建符号并注册到符号表
    // 这需要与 ScopeTree 协作
    
    // 暂时只添加注释
    irBuilder->emitComment("Register variable: " + variableName + " of type " + type);
}
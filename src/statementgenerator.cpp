#include "statementgenerator.hpp"
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>

// 包含 ExpressionGenerator 和 FunctionCodegen 头文件以解决前向声明问题
#include "expressiongenerator.hpp"
#include "functioncodegen.hpp"

// 构造函数
StatementGenerator::StatementGenerator(std::shared_ptr<IRBuilder> irBuilder,
                                     std::shared_ptr<TypeMapper> typeMapper,
                                     std::shared_ptr<ScopeTree> scopeTree)
    : irBuilder(irBuilder)
    , typeMapper(typeMapper)
    , scopeTree(scopeTree)
    , expressionGenerator(nullptr)
    , functionCodegen(nullptr)
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

void StatementGenerator::setFunctionCodegen(FunctionCodegen* funcGen) {
    functionCodegen = funcGen;
}

FunctionCodegen* StatementGenerator::getFunctionCodegen() const {
    return functionCodegen;
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
            // 使用显式类型 - 从 Type 节点获取类型信息
            llvmType = typeToStringHelper(letStatement->type);
            if (llvmType.empty()) {
                llvmType = "i32"; // 默认类型
            }
        } else {
            // 从初始化表达式推断类型
            if (letStatement->expression) {
                // 调用 ExpressionGenerator 获取表达式类型
                if (validateExpressionGenerator()) {
                    // 从语义分析阶段获取类型信息
                    llvmType = expressionGenerator->getNodeTypeLLVM(letStatement->expression);
                    if (llvmType.empty()) {
                        llvmType = "i32"; // 默认类型
                    }
                } else {
                    llvmType = "i32";
                }
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
            
            // 调用 ExpressionGenerator 生成初始化表达式
            std::string initReg = expressionGenerator->generateExpression(letStatement->expression);
            
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
        // 控制流表达式（if、loop、while、break、continue、return）都应该委托给 ExpressionGenerator
        // 因为它们都是 Expression 的子类
        
        // 调用 ExpressionGenerator 生成表达式
        std::string resultReg = expressionGenerator->generateExpression(exprStatement->astnode);
        
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
        // 为变量设置特定的寄存器名
        std::string variableReg = irBuilder->newRegister(variableName, "_ptr");
        
        // 对于引用类型，需要使用 TypeMapper 来正确映射
        // 例如：&i32 应该映射为 i32*，而不是 &i32
        std::string actualType = type;
        if (type.find('&') == 0) {
            // 这是一个引用类型，使用 TypeMapper 映射
            actualType = typeMapper->mapRxTypeToLLVM(type);
        }
        
        // 手动设置寄存器类型
        irBuilder->setRegisterType(variableReg, actualType + "*");
        
        // 生成 alloca 指令，但使用变量寄存器作为结果
        std::string alignStr = "";
        int autoAlign = typeMapper->getTypeAlignment(actualType); // 使用 TypeMapper 获取对齐
        if (autoAlign > 0) {
            alignStr = ", align " + std::to_string(autoAlign);
        }
        
        std::string instruction = variableReg + " = alloca " + actualType + alignStr;
        irBuilder->emitInstruction(instruction);
        
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
        
        // 检查是否为聚合类型（数组或结构体）
        if (irBuilder->isAggregateType(type)) {
            // 聚合类型：使用 builtin_memcpy 进行内存拷贝
            irBuilder->emitAggregateCopy(variableReg, valueReg, type);
        } else {
            // 非聚合类型：直接存储
            irBuilder->emitStore(valueReg, variableReg);
        }
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
    if (!statement || !statement->astnode) {
        return false;
    }
    
    // 检查表达式语句是否包含终止符
    if (auto exprStmt = std::dynamic_pointer_cast<ExpressionStatement>(statement->astnode)) {
        if (!exprStmt->astnode) {
            return false;
        }
        
        // 检查是否为控制流表达式
        return (std::dynamic_pointer_cast<ReturnExpression>(exprStmt->astnode) != nullptr) ||
               (std::dynamic_pointer_cast<BreakExpression>(exprStmt->astnode) != nullptr) ||
               (std::dynamic_pointer_cast<ContinueExpression>(exprStmt->astnode) != nullptr);
    }
    
    // 检查项语句中的函数定义
    if (auto item = std::dynamic_pointer_cast<Item>(statement->astnode)) {
        if (auto function = std::dynamic_pointer_cast<Function>(item->item)) {
            // 函数定义本身不是终止符，但函数体可能包含
            // 这里简化处理，返回 false
            return false;
        }
    }
    
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
    
    // 处理其他模式类型
    if (auto wildcardPattern = std::dynamic_pointer_cast<WildcardPattern>(pattern)) {
        return "_"; // 通配符模式
    }
    
    if (auto refPattern = std::dynamic_pointer_cast<ReferencePattern>(pattern)) {
        // 引用模式，递归处理内部模式
        return extractVariableName(refPattern->pattern);
    }
    
    if (auto pathPattern = std::dynamic_pointer_cast<PathPattern>(pattern)) {
        // 路径模式，从路径中提取变量名
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(pathPattern->pathexpression)) {
            if (pathExpr->simplepath) {
                auto segments = pathExpr->simplepath->simplepathsegements;
                if (!segments.empty()) {
                    return segments.back()->identifier;
                }
            }
        }
    }
    
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
    if (!function) {
        reportError("Function is null");
        return false;
    }
    
    try {
        irBuilder->emitComment("Found nested function: " + function->identifier_name);
        
        // 注意：内部函数现在由 FunctionCodegen 的预处理机制处理
        // 这里我们只需要记录注释，实际的函数生成已经在预处理阶段完成
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunctionItem: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateStructItem(std::shared_ptr<StructStruct> structDef) {
    if (!structDef) {
        reportError("Struct definition is null");
        return false;
    }
    
    try {
        irBuilder->emitComment("Struct definition: " + structDef->identifier);
        
        // 生成结构体类型定义
        std::string structName = structDef->identifier;
        std::string structType = "%struct_" + structName;
        
        // 收集字段信息
        std::vector<std::string> fieldTypes;
        if (structDef->structfields) {
            for (const auto& field : structDef->structfields->structfields) {
                if (field && field->type) {
                    std::string fieldType = typeToStringHelper(field->type);
                    if (!fieldType.empty()) {
                        fieldTypes.push_back(fieldType);
                    } else {
                        fieldTypes.push_back("i32"); // 默认类型
                    }
                }
            }
        }
        
        // 生成结构体类型定义
        std::string structDefStr = structType + " = type {";
        for (size_t i = 0; i < fieldTypes.size(); ++i) {
            if (i > 0) structDefStr += ", ";
            structDefStr += fieldTypes[i];
        }
        structDefStr += "}";
        
        // 这里应该将结构体定义添加到模块头部
        // 暂时只生成注释
        irBuilder->emitComment("Struct type: " + structDefStr);
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateStructItem: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateConstantItem(std::shared_ptr<ConstantItem> constant) {
    if (!constant) {
        reportError("Constant definition is null");
        return false;
    }
    
    try {
        irBuilder->emitComment("Constant definition: " + constant->identifier);
        
        // 确定常量类型
        std::string constType;
        if (constant->type) {
            constType = typeToStringHelper(constant->type);
        } else {
            // 从表达式推断类型
            if (constant->expression && validateExpressionGenerator()) {
                // 从表达式推断类型
                constType = expressionGenerator->getNodeTypeLLVM(constant->expression);
                if (constType.empty()) {
                    constType = "i32";
                }
            } else {
                constType = "i32";
            }
        }
        
        // 生成常量值
        if (constant->expression && validateExpressionGenerator()) {
            std::string constValue = expressionGenerator->generateExpression(constant->expression);
            if (!constValue.empty()) {
                // 生成全局常量定义
                std::string globalName = "@" + constant->identifier;
                std::string globalDecl = globalName + " = constant " + constType + " " + constValue;
                
                // 这里应该将全局常量添加到模块头部
                // 暂时只生成注释
                irBuilder->emitComment("Global constant: " + globalDecl);
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateConstantItem: " + std::string(e.what()));
        return false;
    }
}

bool StatementGenerator::generateImplItem(std::shared_ptr<InherentImpl> impl) {
    if (!impl) {
        reportError("Impl block is null");
        return false;
    }
    
    try {
        irBuilder->emitComment("Impl block");
        
        // 获取实现的目标类型
        std::string targetType;
        if (impl->type) {
            targetType = typeToStringHelper(impl->type);
        }
        
        irBuilder->emitComment("Impl for type: " + targetType);
        
        // 处理关联项（方法、常量等）
        for (const auto& item : impl->associateditems) {
            if (item && item->consttantitem_or_function) {
                if (auto function = std::dynamic_pointer_cast<Function>(item->consttantitem_or_function)) {
                    irBuilder->emitComment("Method: " + function->identifier_name);
                    // 这里应该生成方法定义
                } else if (auto constant = std::dynamic_pointer_cast<ConstantItem>(item->consttantitem_or_function)) {
                    irBuilder->emitComment("Associated constant: " + constant->identifier);
                    // 这里应该生成关联常量定义
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateImplItem: " + std::string(e.what()));
        return false;
    }
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
    // 从符号表获取变量的类型信息
    if (scopeTree) {
        auto currentScope = scopeTree->GetCurrentScope();
        if (currentScope) {
            auto symbol = currentScope->Lookup(variableName);
            if (symbol && symbol->type) {
                return typeMapper->mapSemanticTypeToLLVM(symbol->type);
            }
        }
    }
    
    return "i32"; // 默认类型
}

// ==================== 私有辅助方法实现 ====================

std::string StatementGenerator::typeToStringHelper(std::shared_ptr<Type> type) {
    if (!type) {
        return "i32"; // 默认类型
    }
    
    // 根据 Type 的具体子类型进行转换
    if (auto typePath = std::dynamic_pointer_cast<TypePath>(type)) {
        if (typePath->simplepathsegement) {
            std::string typeName = typePath->simplepathsegement->identifier;
            // 使用 TypeMapper 进行正确的类型映射
            return typeMapper->mapRxTypeToLLVM(typeName);
        }
    } else if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(type)) {
        // 递归获取元素类型
        std::string elementType = typeToStringHelper(arrayType->type);
        
        // 尝试从大小表达式获取实际大小
        std::string sizeStr = "0"; // 默认大小
        if (arrayType->expression) {
            if (auto literalExpr = std::dynamic_pointer_cast<LiteralExpression>(arrayType->expression)) {
                if (literalExpr->tokentype == Token::kINTEGER_LITERAL) {
                    sizeStr = literalExpr->literal;
                }
            } else {
                // 对于复杂表达式，尝试使用 TypeChecker 的 EvaluateArraySize 方法
                // 这里我们需要访问 TypeChecker 的实例，但当前架构下可能不可用
                // 作为替代方案，我们尝试从 nodeTypeMap 获取已求值的大小信息
                // 这个信息应该在类型检查阶段就已经计算好了
                
                // 查找 ArrayTypeWrapper 对象来获取已求值的大小
                // 首先尝试从语义类型中获取大小信息
                // 这里我们需要一个方法来获取 ArrayTypeWrapper，但当前架构下不可用
                // 作为临时解决方案，我们实现一个简单的二元表达式求值
                
                if (auto binaryExpr = std::dynamic_pointer_cast<BinaryExpression>(arrayType->expression)) {
                    // 处理简单的二元运算，如 6 - 2
                    if (auto leftLiteral = std::dynamic_pointer_cast<LiteralExpression>(binaryExpr->leftexpression)) {
                        if (auto rightLiteral = std::dynamic_pointer_cast<LiteralExpression>(binaryExpr->rightexpression)) {
                            if (leftLiteral->tokentype == Token::kINTEGER_LITERAL &&
                                rightLiteral->tokentype == Token::kINTEGER_LITERAL) {
                                try {
                                    int64_t left = std::stoll(leftLiteral->literal);
                                    int64_t right = std::stoll(rightLiteral->literal);
                                    int64_t result = 0;
                                    
                                    switch (binaryExpr->binarytype) {
                                        case Token::kPlus: result = left + right; break;
                                        case Token::kMinus: result = left - right; break;
                                        case Token::kStar: result = left * right; break;
                                        case Token::kSlash:
                                            if (right != 0) result = left / right;
                                            break;
                                        default: break;
                                    }
                                    
                                    sizeStr = std::to_string(result);
                                } catch (const std::exception&) {
                                    sizeStr = "0";
                                }
                            }
                        }
                    } else if (auto literalExpr = std::dynamic_pointer_cast<LiteralExpression>(arrayType->expression)) {
                        if (literalExpr->tokentype == Token::kINTEGER_LITERAL) {
                            sizeStr = literalExpr->literal;
                        }
                    }
                }
            }
        }
        
        // 生成正确的 LLVM 数组类型格式：[N x T]
        return "[" + sizeStr + " x " + elementType + "]";
    } else if (auto refType = std::dynamic_pointer_cast<ReferenceType>(type)) {
        std::string targetType = typeToStringHelper(refType->type);
        // 对于引用类型，使用 TypeMapper 来正确映射
        std::string refTypeStr = (refType->ismut ? "&mut " : "&") + targetType;
        return typeMapper->mapRxTypeToLLVM(refTypeStr);
    } else if (auto unitType = std::dynamic_pointer_cast<UnitType>(type)) {
        return "()";
    }
    
    return "i32"; // 默认类型
}

void StatementGenerator::registerVariable(const std::string& variableName, const std::string& type, const std::string& registerName) {
    // 创建符号并注册到符号表
    if (scopeTree) {
        auto currentScope = scopeTree->GetCurrentScope();
        if (currentScope) {
            // 创建变量符号
            auto symbol = std::make_shared<Symbol>(variableName, SymbolKind::Variable);
            
            // 创建语义类型
            auto semanticType = std::make_shared<SimpleType>(type);
            symbol->type = semanticType;
            symbol->ismutable = true; // let 绑定的变量默认可变
            
            // 注册到符号表
            currentScope->Insert(variableName, symbol);
            
            irBuilder->emitComment("Register variable: " + variableName + " of type " + type + " at " + registerName);
        } else {
            reportError("No current scope available for variable registration: " + variableName);
        }
    } else {
        reportError("ScopeTree not available for variable registration: " + variableName);
    }
}
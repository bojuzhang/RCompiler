#include "expressiongenerator.hpp"
#include <stdexcept>
#include <cctype>

// 包含 StatementGenerator 头文件以解决前向声明问题
#include "statementgenerator.hpp"

// ==================== 构造函数和基本初始化方法 ====================

ExpressionGenerator::ExpressionGenerator(std::shared_ptr<IRBuilder> irBuilder,
                                         std::shared_ptr<TypeMapper> typeMapper,
                                         std::shared_ptr<ScopeTree> scopeTree,
                                         const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& nodeTypeMap)
    : irBuilder(irBuilder)
    , typeMapper(typeMapper)
    , scopeTree(scopeTree)
    , statementGenerator(nullptr)
    , nodeTypeMap(nodeTypeMap)
    , hasErrors(false)
    , stringCounter(0)
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

// ==================== StatementGenerator 集成接口 ====================

void ExpressionGenerator::setStatementGenerator(StatementGenerator* stmtGen) {
    statementGenerator = stmtGen;
}

StatementGenerator* ExpressionGenerator::getStatementGenerator() const {
    return statementGenerator;
}

// ==================== 主要生成接口 ====================

std::string ExpressionGenerator::generateExpression(std::shared_ptr<Expression> expression) {
    if (!expression) {
        reportError("Expression is null");
        return "";
    }
    
    try {
        // 根据表达式类型进行分发
        if (auto literalExpr = std::dynamic_pointer_cast<LiteralExpression>(expression)) {
            return generateLiteralExpression(literalExpr);
        }
        else if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(expression)) {
            return generatePathExpression(pathExpr);
        }
        else if (auto arrayExpr = std::dynamic_pointer_cast<ArrayExpression>(expression)) {
            return generateArrayExpression(arrayExpr);
        }
        else if (auto indexExpr = std::dynamic_pointer_cast<IndexExpression>(expression)) {
            return generateIndexExpression(indexExpr);
        }
        else if (auto tupleExpr = std::dynamic_pointer_cast<TupleExpression>(expression)) {
            return generateTupleExpression(tupleExpr);
        }
        else if (auto structExpr = std::dynamic_pointer_cast<StructExpression>(expression)) {
            return generateStructExpression(structExpr);
        }
        else if (auto callExpr = std::dynamic_pointer_cast<CallExpression>(expression)) {
            return generateCallExpression(callExpr);
        }
        else if (auto methodCallExpr = std::dynamic_pointer_cast<MethodCallExpression>(expression)) {
            return generateMethodCallExpression(methodCallExpr);
        }
        else if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(expression)) {
            return generateFieldExpression(fieldExpr);
        }
        else if (auto unaryExpr = std::dynamic_pointer_cast<UnaryExpression>(expression)) {
            return generateUnaryExpression(unaryExpr);
        }
        else if (auto binaryExpr = std::dynamic_pointer_cast<BinaryExpression>(expression)) {
            return generateBinaryExpression(binaryExpr);
        }
        else if (auto assignExpr = std::dynamic_pointer_cast<AssignmentExpression>(expression)) {
            return generateAssignmentExpression(assignExpr);
        }
        else if (auto compoundAssignExpr = std::dynamic_pointer_cast<CompoundAssignmentExpression>(expression)) {
            return generateCompoundAssignmentExpression(compoundAssignExpr);
        }
        else if (auto typeCastExpr = std::dynamic_pointer_cast<TypeCastExpression>(expression)) {
            return generateTypeCastExpression(typeCastExpr);
        }
        else if (auto blockExpr = std::dynamic_pointer_cast<BlockExpression>(expression)) {
            return generateBlockExpression(blockExpr);
        }
        else if (auto ifExpr = std::dynamic_pointer_cast<IfExpression>(expression)) {
            return generateIfExpression(ifExpr);
        }
        else if (auto loopExpr = std::dynamic_pointer_cast<InfiniteLoopExpression>(expression)) {
            return generateLoopExpression(loopExpr);
        }
        else if (auto whileExpr = std::dynamic_pointer_cast<PredicateLoopExpression>(expression)) {
            return generateWhileExpression(whileExpr);
        }
        else if (auto breakExpr = std::dynamic_pointer_cast<BreakExpression>(expression)) {
            return generateBreakExpression(breakExpr);
        }
        else if (auto continueExpr = std::dynamic_pointer_cast<ContinueExpression>(expression)) {
            return generateContinueExpression(continueExpr);
        }
        else if (auto returnExpr = std::dynamic_pointer_cast<ReturnExpression>(expression)) {
            return generateReturnExpression(returnExpr);
        }
        else {
            reportError("Unsupported expression type");
            return "";
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 字面量表达式生成方法 ====================

std::string ExpressionGenerator::generateLiteralExpression(std::shared_ptr<LiteralExpression> literalExpr) {
    if (!literalExpr) {
        reportError("LiteralExpression is null");
        return "";
    }
    
    try {
        std::string resultReg = irBuilder->newRegister();
        std::string llvmType = getExpressionType(literalExpr);
        
        // 根据字面量类型生成相应的 IR
        switch (literalExpr->tokentype) {
            case Token::kINTEGER_LITERAL: {
                // 整数字面量
                std::string instruction = resultReg + " = add " + llvmType + " 0, " + literalExpr->literal;
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, llvmType);
                break;
            }
            case Token::kSTRING_LITERAL: {
                // 字符串字面量 - 创建全局常量
                std::string globalName = generateStringConstant(literalExpr->literal);
                std::string ptrReg = irBuilder->emitGetElementPtr(globalName, {"0", "0"}, "i8*");
                irBuilder->setRegisterType(ptrReg, "i8*");
                return ptrReg;
            }
            case Token::ktrue: {
                // 布尔值 true
                std::string instruction = resultReg + " = add i1 0, 1";
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, "i1");
                break;
            }
            case Token::kfalse: {
                // 布尔值 false
                std::string instruction = resultReg + " = add i1 0, 0";
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, "i1");
                break;
            }
            case Token::kCHAR_LITERAL: {
                // 字符字面量
                std::string charValue = literalExpr->literal;
                if (charValue.length() >= 2 && charValue.front() == '\'' && charValue.back() == '\'') {
                    // 提取字符值
                    charValue = charValue.substr(1, charValue.length() - 2);
                }
                std::string instruction = resultReg + " = add i8 0, " + std::to_string(static_cast<int>(charValue[0]));
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, "i8");
                break;
            }
            default: {
                reportError("Unsupported literal type: " + std::to_string(static_cast<int>(literalExpr->tokentype)));
                return "";
            }
        }
        
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateLiteralExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 路径表达式生成方法 ====================

std::string ExpressionGenerator::generatePathExpression(std::shared_ptr<PathExpression> pathExpr) {
    if (!pathExpr || !pathExpr->simplepath) {
        reportError("PathExpression or SimplePath is null");
        return "";
    }
    
    try {
        // 获取路径的最后一个段作为变量名
        if (pathExpr->simplepath->simplepathsegements.empty()) {
            reportError("Empty path in PathExpression");
            return "";
        }
        
        auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
        std::string variableName = lastSegment->identifier;
        
        // 从符号表查找变量
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            reportError("No current scope available");
            return "";
        }
        
        auto symbol = currentScope->Lookup(variableName);
        if (!symbol) {
            reportError("Undefined variable: " + variableName);
            return "";
        }
        
        // 根据符号类型处理
        if (symbol->kind == SymbolKind::Variable || symbol->kind == SymbolKind::Constant) {
            // 变量或常量 - 加载值
            std::string variableReg = irBuilder->getVariableRegister(variableName);
            if (variableReg.empty()) {
                reportError("Variable register not found: " + variableName);
                return "";
            }
            
            std::string llvmType = typeMapper->mapSemanticTypeToLLVM(symbol->type);
            std::string valueReg = irBuilder->emitLoad(variableReg, llvmType);
            return valueReg;
        }
        else if (symbol->kind == SymbolKind::Function) {
            // 函数 - 返回函数指针
            // 在 LLVM 中，函数名本身就是指针
            return "@" + variableName;
        }
        else {
            reportError("Unsupported symbol type for path expression: " + std::to_string(static_cast<int>(symbol->kind)));
            return "";
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generatePathExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 数组表达式生成方法 ====================

std::string ExpressionGenerator::generateArrayExpression(std::shared_ptr<ArrayExpression> arrayExpr) {
    if (!arrayExpr || !arrayExpr->arrayelements) {
        reportError("ArrayExpression or ArrayElements is null");
        return "";
    }
    
    try {
        const auto& elements = arrayExpr->arrayelements->expressions;
        if (elements.empty()) {
            reportError("Empty array expression");
            return "";
        }
        
        // 确定元素类型（从第一个元素推断）
        std::string elementType = getExpressionType(elements[0]);
        
        // 分配数组空间
        std::string arrayType = "[" + std::to_string(elements.size()) + " x " + elementType + "]";
        std::string arrayReg = irBuilder->emitAlloca(arrayType);
        
        // 初始化数组元素
        for (size_t i = 0; i < elements.size(); ++i) {
            // 生成元素表达式
            std::string elementReg = generateExpression(elements[i]);
            if (elementReg.empty()) {
                reportError("Failed to generate array element at index " + std::to_string(i));
                continue;
            }
            
            // 计算元素地址
            std::vector<std::string> indices = {"0", std::to_string(i)};
            std::string elementPtrReg = irBuilder->emitGetElementPtr(arrayReg, indices, elementType + "*");
            
            // 存储元素值
            irBuilder->emitStore(elementReg, elementPtrReg);
        }
        
        return arrayReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateArrayExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 索引表达式生成方法 ====================

std::string ExpressionGenerator::generateIndexExpression(std::shared_ptr<IndexExpression> indexExpr) {
    if (!indexExpr || !indexExpr->expressionout || !indexExpr->expressionin) {
        reportError("IndexExpression or its components are null");
        return "";
    }
    
    try {
        // 生成基础表达式（数组或指针）
        std::string baseReg = generateExpression(indexExpr->expressionout);
        if (baseReg.empty()) {
            reportError("Failed to generate base expression for indexing");
            return "";
        }
        
        // 生成索引表达式
        std::string indexReg = generateExpression(indexExpr->expressionin);
        if (indexReg.empty()) {
            reportError("Failed to generate index expression");
            return "";
        }
        
        // 获取基础表达式的类型
        std::string baseType = getExpressionType(indexExpr->expressionout);
        
        // 确定元素类型
        std::string elementType;
        if (typeMapper->isPointerType(baseType)) {
            // 指针索引
            elementType = typeMapper->getPointedType(baseType);
        } else if (typeMapper->isArrayType(baseType)) {
            // 数组索引
            elementType = typeMapper->getArrayElementType(baseType);
        } else {
            reportError("Cannot index into non-pointer, non-array type: " + baseType);
            return "";
        }
        
        // 生成元素地址
        std::string elementPtrReg;
        if (typeMapper->isPointerType(baseType)) {
            // 指针索引：直接使用指针和索引
            std::vector<std::string> indices = {indexReg};
            elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
        } else {
            // 数组索引：需要添加数组起始偏移
            std::vector<std::string> indices = {"0", indexReg};
            elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
        }
        
        // 加载元素值
        std::string valueReg = irBuilder->emitLoad(elementPtrReg, elementType);
        return valueReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateIndexExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 元组表达式生成方法 ====================

std::string ExpressionGenerator::generateTupleExpression(std::shared_ptr<TupleExpression> tupleExpr) {
    if (!tupleExpr || !tupleExpr->tupleelements) {
        reportError("TupleExpression or TupleElements is null");
        return "";
    }
    
    try {
        const auto& elements = tupleExpr->tupleelements->expressions;
        
        // 对于元组，我们简化处理：返回第一个元素的值
        // 在实际实现中，应该创建一个结构体来表示元组
        if (elements.empty()) {
            // 空元组返回 unit 类型
            return generateUnitValue();
        }
        
        // 生成第一个元素作为元组的代表值
        return generateExpression(elements[0]);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTupleExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 结构体表达式生成方法 ====================

std::string ExpressionGenerator::generateStructExpression(std::shared_ptr<StructExpression> structExpr) {
    if (!structExpr || !structExpr->pathexpression || !structExpr->structinfo) {
        reportError("StructExpression or its components are null");
        return "";
    }
    
    try {
        // 获取结构体名称
        std::string structName = structExpr->pathexpression->simplepath->simplepathsegements.back()->identifier;
        
        // 分配结构体空间
        std::string structType = "%struct_" + structName;
        std::string structReg = irBuilder->emitAlloca(structType);
        
        // 处理结构体字段初始化
        if (auto structFields = std::dynamic_pointer_cast<StructExprFields>(structExpr->structinfo)) {
            for (const auto& field : structFields->structexprfields) {
                if (!field || !field->expression) {
                    continue;
                }
                
                // 生成字段值表达式
                std::string fieldReg = generateExpression(field->expression);
                if (fieldReg.empty()) {
                    reportError("Failed to generate field expression: " + field->identifier);
                    continue;
                }
                
                // 计算字段地址
                std::string fieldPtrReg = generateFieldAccessAddress(structReg, field->identifier, structType);
                if (fieldPtrReg.empty()) {
                    reportError("Failed to calculate field address: " + field->identifier);
                    continue;
                }
                
                // 存储字段值
                irBuilder->emitStore(fieldReg, fieldPtrReg);
            }
        }
        
        return structReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateStructExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 函数调用表达式生成方法 ====================

std::string ExpressionGenerator::generateCallExpression(std::shared_ptr<CallExpression> callExpr) {
    if (!callExpr || !callExpr->expression || !callExpr->callparams) {
        reportError("CallExpression or its components are null");
        return "";
    }
    
    try {
        // 获取函数名
        std::string functionName;
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(callExpr->expression)) {
            functionName = pathExpr->simplepath->simplepathsegements.back()->identifier;
        } else {
            reportError("Unsupported function expression type");
            return "";
        }
        
        // 检查是否为内置函数
        if (isBuiltinFunction(functionName)) {
            std::vector<std::string> args = generateCallArguments(callExpr->callparams);
            return generateBuiltinCall(functionName, args);
        }
        
        // 生成参数表达式
        std::vector<std::string> args = generateCallArguments(callExpr->callparams);
        
        // 获取函数返回类型
        std::string returnType = getFunctionReturnType(functionName);
        if (returnType.empty()) {
            returnType = "i32"; // 默认返回类型
        }
        
        // 生成函数调用
        std::string resultReg = irBuilder->emitCall(functionName, args, returnType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateCallExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 方法调用表达式生成方法 ====================

std::string ExpressionGenerator::generateMethodCallExpression(std::shared_ptr<MethodCallExpression> methodCallExpr) {
    if (!methodCallExpr || !methodCallExpr->receiver || !methodCallExpr->callparams) {
        reportError("MethodCallExpression or its components are null");
        return "";
    }
    
    try {
        // 生成接收者表达式
        std::string receiverReg = generateExpression(methodCallExpr->receiver);
        if (receiverReg.empty()) {
            reportError("Failed to generate receiver expression");
            return "";
        }
        
        // 构建方法名（简化处理：receiver_type_method_name）
        std::string receiverType = getExpressionType(methodCallExpr->receiver);
        std::string methodName = receiverType + "_" + methodCallExpr->method_name;
        
        // 生成参数表达式（包括接收者作为第一个参数）
        std::vector<std::string> args = generateCallArguments(methodCallExpr->callparams);
        args.insert(args.begin(), receiverReg); // 接收者作为第一个参数
        
        // 获取方法返回类型
        std::string returnType = "i32"; // 默认返回类型
        
        // 生成方法调用
        std::string resultReg = irBuilder->emitCall(methodName, args, returnType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateMethodCallExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 字段访问表达式生成方法 ====================

std::string ExpressionGenerator::generateFieldExpression(std::shared_ptr<FieldExpression> fieldExpr) {
    if (!fieldExpr || !fieldExpr->expression) {
        reportError("FieldExpression or its expression is null");
        return "";
    }
    
    try {
        // 生成接收者表达式
        std::string receiverReg = generateExpression(fieldExpr->expression);
        if (receiverReg.empty()) {
            reportError("Failed to generate receiver expression for field access");
            return "";
        }
        
        // 获取接收者类型
        std::string receiverType = getExpressionType(fieldExpr->expression);
        
        // 计算字段地址
        std::string fieldPtrReg = generateFieldAccessAddress(receiverReg, fieldExpr->identifier, receiverType);
        if (fieldPtrReg.empty()) {
            reportError("Failed to calculate field address: " + fieldExpr->identifier);
            return "";
        }
        
        // 获取字段类型
        std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
        if (fieldType.empty()) {
            fieldType = "i32"; // 默认类型
        }
        
        // 加载字段值
        std::string valueReg = irBuilder->emitLoad(fieldPtrReg, fieldType);
        return valueReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFieldExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 一元表达式生成方法 ====================

std::string ExpressionGenerator::generateUnaryExpression(std::shared_ptr<UnaryExpression> unaryExpr) {
    if (!unaryExpr || !unaryExpr->expression) {
        reportError("UnaryExpression or its expression is null");
        return "";
    }
    
    try {
        // 生成操作数表达式
        std::string operandReg = generateExpression(unaryExpr->expression);
        if (operandReg.empty()) {
            reportError("Failed to generate unary operand");
            return "";
        }
        
        std::string operandType = getExpressionType(unaryExpr->expression);
        std::string resultReg = irBuilder->newRegister();
        
        // 根据一元运算符类型生成相应的 IR
        switch (unaryExpr->unarytype) {
            case Token::kMinus: {
                // 取负
                std::string zeroReg = irBuilder->newRegister();
                std::string instruction = zeroReg + " = add " + operandType + " 0, 0";
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(zeroReg, operandType);
                
                instruction = resultReg + " = sub " + operandType + " " + zeroReg + ", " + operandReg;
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, operandType);
                break;
            }
            case Token::kNot: {
                // 逻辑非
                std::string instruction = resultReg + " = xor " + operandType + " " + operandReg + ", 1";
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, operandType);
                break;
            }
            case Token::kStar: { // 解引用操作符 *
                // 解引用
                if (!typeMapper->isPointerType(operandType)) {
                    reportError("Cannot dereference non-pointer type: " + operandType);
                    return "";
                }
                std::string elementType = typeMapper->getPointedType(operandType);
                resultReg = irBuilder->emitLoad(operandReg, elementType);
                break;
            }
            case Token::kAnd: { // 取引用操作符 &
                // 取引用
                if (typeMapper->isPointerType(operandType)) {
                    // 已经是指针，直接返回
                    return operandReg;
                } else {
                    // 需要创建临时变量并返回其地址
                    std::string tempReg = irBuilder->emitAlloca(operandType);
                    irBuilder->emitStore(operandReg, tempReg);
                    return tempReg;
                }
            }
            default: {
                reportError("Unsupported unary operator: " + std::to_string(static_cast<int>(unaryExpr->unarytype)));
                return "";
            }
        }
        
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateUnaryExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 二元表达式生成方法 ====================

std::string ExpressionGenerator::generateBinaryExpression(std::shared_ptr<BinaryExpression> binaryExpr) {
    if (!binaryExpr || !binaryExpr->leftexpression || !binaryExpr->rightexpression) {
        reportError("BinaryExpression or its components are null");
        return "";
    }
    
    try {
        // 生成左右操作数
        std::string leftReg = generateExpression(binaryExpr->leftexpression);
        std::string rightReg = generateExpression(binaryExpr->rightexpression);
        
        if (leftReg.empty() || rightReg.empty()) {
            reportError("Failed to generate binary expression operands");
            return "";
        }
        
        // 获取表达式类型
        std::string resultType = getExpressionType(binaryExpr);
        
        // 根据运算符类型生成相应的 IR
        switch (binaryExpr->binarytype) {
            case Token::kPlus:
            case Token::kMinus:
            case Token::kStar:
            case Token::kSlash:
            case Token::kPercent:
                return generateArithmeticOperation(leftReg, rightReg, binaryExpr->binarytype, resultType);
                
            case Token::kEqEq:
            case Token::kNe:
            case Token::kLt:
            case Token::kLe:
            case Token::kGt:
            case Token::kGe:
                return generateComparisonOperation(leftReg, rightReg, binaryExpr->binarytype, resultType);
                
            case Token::kAndAnd:
            case Token::kOrOr:
                return generateLogicalOperation(leftReg, rightReg, binaryExpr->binarytype, resultType);
                
            case Token::kCaret:
            case Token::kShl:
            case Token::kShr: {
                std::string resultReg = irBuilder->newRegister();
                std::string instruction;
                
                switch (binaryExpr->binarytype) {
                    case Token::kAnd:
                        instruction = resultReg + " = and " + resultType + " " + leftReg + ", " + rightReg;
                        break;
                    case Token::kOr:
                        instruction = resultReg + " = or " + resultType + " " + leftReg + ", " + rightReg;
                        break;
                    case Token::kCaret:
                        instruction = resultReg + " = xor " + resultType + " " + leftReg + ", " + rightReg;
                        break;
                    case Token::kShl:
                        instruction = resultReg + " = shl " + resultType + " " + leftReg + ", " + rightReg;
                        break;
                    case Token::kShr:
                        instruction = resultReg + " = ashr " + resultType + " " + leftReg + ", " + rightReg;
                        break;
                    default:
                        reportError("Unsupported bitwise operator");
                        return "";
                }
                
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, resultType);
                return resultReg;
            }
                
            default:
                reportError("Unsupported binary operator: " + std::to_string(static_cast<int>(binaryExpr->binarytype)));
                return "";
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBinaryExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 赋值表达式生成方法 ====================

std::string ExpressionGenerator::generateAssignmentExpression(std::shared_ptr<AssignmentExpression> assignExpr) {
    if (!assignExpr || !assignExpr->leftexpression || !assignExpr->rightexpression) {
        reportError("AssignmentExpression or its components are null");
        return "";
    }
    
    try {
        // 分析左值表达式
        auto [leftPtrReg, leftType] = analyzeLValue(assignExpr->leftexpression);
        if (leftPtrReg.empty()) {
            reportError("Invalid left side of assignment");
            return "";
        }
        
        // 生成右值表达式
        std::string rightReg = generateExpression(assignExpr->rightexpression);
        if (rightReg.empty()) {
            reportError("Failed to generate right side of assignment");
            return "";
        }
        
        // 检查类型兼容性并进行必要的转换
        std::string rightType = getExpressionType(assignExpr->rightexpression);
        if (!typeMapper->areTypesCompatible(leftType, rightType)) {
            // 尝试类型转换
            std::string convertedReg = generateTypeConversion(rightReg, rightType, leftType);
            if (!convertedReg.empty()) {
                rightReg = convertedReg;
            } else {
                reportError("Type mismatch in assignment: " + leftType + " = " + rightType);
                return "";
            }
        }
        
        // 存储值到左值
        irBuilder->emitStore(rightReg, leftPtrReg);
        
        // 赋值表达式返回右值
        return rightReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateAssignmentExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 复合赋值表达式生成方法 ====================

std::string ExpressionGenerator::generateCompoundAssignmentExpression(std::shared_ptr<CompoundAssignmentExpression> compoundAssignExpr) {
    if (!compoundAssignExpr || !compoundAssignExpr->leftexpression || !compoundAssignExpr->rightexpression) {
        reportError("CompoundAssignmentExpression or its components are null");
        return "";
    }
    
    try {
        // 分析左值表达式
        auto [leftPtrReg, leftType] = analyzeLValue(compoundAssignExpr->leftexpression);
        if (leftPtrReg.empty()) {
            reportError("Invalid left side of compound assignment");
            return "";
        }
        
        // 加载当前值
        std::string currentReg = irBuilder->emitLoad(leftPtrReg, leftType);
        
        // 生成右值表达式
        std::string rightReg = generateExpression(compoundAssignExpr->rightexpression);
        if (rightReg.empty()) {
            reportError("Failed to generate right side of compound assignment");
            return "";
        }
        
        // 执行相应的二元运算
        std::string resultReg;
        switch (compoundAssignExpr->type) {
            case Token::kPlusEq:
            case Token::kMinusEq:
            case Token::kStarEq:
            case Token::kSlashEq:
            case Token::kPercentEq: {
                // 算术复合赋值
                Token opType;
                switch (compoundAssignExpr->type) {
                    case Token::kPlusEq: opType = Token::kPlus; break;
                    case Token::kMinusEq: opType = Token::kMinus; break;
                    case Token::kStarEq: opType = Token::kStar; break;
                    case Token::kSlashEq: opType = Token::kSlash; break;
                    case Token::kPercentEq: opType = Token::kPercent; break;
                    default: opType = Token::kPlus; break;
                }
                resultReg = generateArithmeticOperation(currentReg, rightReg, opType, leftType);
                break;
            }
            case Token::kAndEq:
            case Token::kOrEq:
            case Token::kCaretEq:
            case Token::kShlEq:
            case Token::kShrEq: {
                // 位运算复合赋值
                Token opType;
                switch (compoundAssignExpr->type) {
                    case Token::kAndEq: opType = Token::kAnd; break;
                    case Token::kOrEq: opType = Token::kOr; break;
                    case Token::kCaretEq: opType = Token::kCaret; break;
                    case Token::kShlEq: opType = Token::kShl; break;
                    case Token::kShrEq: opType = Token::kShr; break;
                    default: opType = Token::kAnd; break;
                }
                
                std::string instruction;
                std::string tempReg = irBuilder->newRegister();
                switch (opType) {
                    case Token::kAnd:
                        instruction = tempReg + " = and " + leftType + " " + currentReg + ", " + rightReg;
                        break;
                    case Token::kOr:
                        instruction = tempReg + " = or " + leftType + " " + currentReg + ", " + rightReg;
                        break;
                    case Token::kCaret:
                        instruction = tempReg + " = xor " + leftType + " " + currentReg + ", " + rightReg;
                        break;
                    case Token::kShl:
                        instruction = tempReg + " = shl " + leftType + " " + currentReg + ", " + rightReg;
                        break;
                    case Token::kShr:
                        instruction = tempReg + " = ashr " + leftType + " " + currentReg + ", " + rightReg;
                        break;
                    default:
                        reportError("Unsupported bitwise operator in compound assignment");
                        return "";
                }
                
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(tempReg, leftType);
                resultReg = tempReg;
                break;
            }
            default:
                reportError("Unsupported compound assignment operator: " + std::to_string(static_cast<int>(compoundAssignExpr->type)));
                return "";
        }
        
        // 存储结果回左值
        irBuilder->emitStore(resultReg, leftPtrReg);
        
        // 复合赋值表达式返回结果值
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateCompoundAssignmentExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 类型转换表达式生成方法 ====================

std::string ExpressionGenerator::generateTypeCastExpression(std::shared_ptr<TypeCastExpression> typeCastExpr) {
    if (!typeCastExpr || !typeCastExpr->expression || !typeCastExpr->typenobounds) {
        reportError("TypeCastExpression or its components are null");
        return "";
    }
    
    try {
        // 生成源表达式
        std::string sourceReg = generateExpression(typeCastExpr->expression);
        if (sourceReg.empty()) {
            reportError("Failed to generate source expression for type cast");
            return "";
        }
        
        // 获取源类型和目标类型
        std::string sourceType = getExpressionType(typeCastExpr->expression);
        // 将 Type 转换为字符串表示，然后映射到 LLVM 类型
        std::string targetTypeStr = typeToStringHelper(typeCastExpr->typenobounds);
        std::string targetType = typeMapper->mapRxTypeToLLVM(targetTypeStr);
        
        // 生成类型转换
        return generateTypeConversion(sourceReg, sourceType, targetType);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTypeCastExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 错误处理接口 ====================

bool ExpressionGenerator::hasError() const {
    return hasErrors;
}

std::vector<std::string> ExpressionGenerator::getErrorMessages() const {
    return errorMessages;
}

void ExpressionGenerator::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
}

void ExpressionGenerator::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

// ==================== 私有辅助方法 ====================

bool ExpressionGenerator::validateStatementGenerator() {
    if (!statementGenerator) {
        reportError("StatementGenerator not set");
        return false;
    }
    return true;
}

std::string ExpressionGenerator::getExpressionType(std::shared_ptr<Expression> expression) {
    if (!expression) {
        return "i32"; // 默认类型
    }
    
    auto it = nodeTypeMap.find(expression.get());
    if (it != nodeTypeMap.end()) {
        return typeMapper->mapSemanticTypeToLLVM(it->second);
    }
    
    return "i32"; // 默认类型
}

bool ExpressionGenerator::isLValue(std::shared_ptr<Expression> expression) {
    if (!expression) {
        return false;
    }
    
    // 简化实现：路径表达式、字段访问、索引表达式可以是左值
    return (std::dynamic_pointer_cast<PathExpression>(expression) != nullptr) ||
           (std::dynamic_pointer_cast<FieldExpression>(expression) != nullptr) ||
           (std::dynamic_pointer_cast<IndexExpression>(expression) != nullptr);
}

std::pair<std::string, std::string> ExpressionGenerator::analyzeLValue(std::shared_ptr<Expression> expression) {
    if (!expression) {
        return {"", ""};
    }
    
    try {
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(expression)) {
            // 变量访问
            std::string variableName = pathExpr->simplepath->simplepathsegements.back()->identifier;
            std::string varReg = irBuilder->getVariableRegister(variableName);
            std::string varType = getExpressionType(expression);
            return {varReg, varType};
        }
        else if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(expression)) {
            // 字段访问
            std::string receiverReg = generateExpression(fieldExpr->expression);
            std::string receiverType = getExpressionType(fieldExpr->expression);
            std::string fieldPtrReg = generateFieldAccessAddress(receiverReg, fieldExpr->identifier, receiverType);
            std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
            if (fieldType.empty()) {
                fieldType = "i32"; // 默认类型
            }
            return {fieldPtrReg, fieldType};
        }
        else if (auto indexExpr = std::dynamic_pointer_cast<IndexExpression>(expression)) {
            // 索引访问
            std::string baseReg = generateExpression(indexExpr->expressionout);
            std::string baseType = getExpressionType(indexExpr->expressionout);
            std::string indexReg = generateExpression(indexExpr->expressionin);
            
            std::string elementType;
            if (typeMapper->isPointerType(baseType)) {
                elementType = typeMapper->getPointedType(baseType);
                std::vector<std::string> indices = {indexReg};
                std::string elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
                return {elementPtrReg, elementType};
            } else if (typeMapper->isArrayType(baseType)) {
                elementType = typeMapper->getArrayElementType(baseType);
                std::vector<std::string> indices = {"0", indexReg};
                std::string elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
                return {elementPtrReg, elementType};
            }
        }
        
        reportError("Expression is not a valid LValue");
        return {"", ""};
    }
    catch (const std::exception& e) {
        reportError("Exception in analyzeLValue: " + std::string(e.what()));
        return {"", ""};
    }
}

std::string ExpressionGenerator::generateStringConstant(const std::string& stringValue) {
    // 检查是否已经存在相同的字符串常量
    auto it = stringConstants.find(stringValue);
    if (it != stringConstants.end()) {
        return it->second;
    }
    
    // 生成新的字符串常量名称
    std::string constantName = ".str" + std::to_string(++stringCounter);
    
    // 转义字符串
    std::string escapedValue = escapeString(stringValue);
    
    // 生成全局常量声明
    std::string globalDecl = constantName + " = private unnamed_addr constant [" + 
                           std::to_string(escapedValue.length() + 1) + " x i8] c\"" + 
                           escapedValue + "\\00\"";
    
    // 记录全局常量信息，稍后由 IRGenerator 添加到模块头部
    // 暂时只记录常量名称
    stringConstants[stringValue] = constantName;
    
    return constantName;
}

std::string ExpressionGenerator::escapeString(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\"': result += "\\\""; break;
            case '\n': result += "\\0A"; break;
            case '\r': result += "\\0D"; break;
            case '\t': result += "\\09"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string ExpressionGenerator::generateFieldAccessAddress(const std::string& basePtr, 
                                                            const std::string& fieldName, 
                                                            const std::string& structType) {
    try {
        // 获取字段索引
        int fieldIndex = getStructFieldIndex(structType, fieldName);
        if (fieldIndex < 0) {
            reportError("Field not found: " + fieldName + " in " + structType);
            return "";
        }
        
        // 生成 getelementptr 指令
        std::vector<std::string> indices = {"0", std::to_string(fieldIndex)};
        std::string fieldPtrReg = irBuilder->emitGetElementPtr(basePtr, indices, structType);
        
        return fieldPtrReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFieldAccessAddress: " + std::string(e.what()));
        return "";
    }
}

int ExpressionGenerator::getStructFieldIndex(const std::string& structName, const std::string& fieldName) {
    try {
        // 从符号表查找结构体定义
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            return -1;
        }
        
        auto symbol = currentScope->Lookup(structName);
        if (!symbol || symbol->kind != SymbolKind::Struct) {
            return -1;
        }
        
        auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(symbol);
        if (!structSymbol) {
            return -1;
        }
        
        // 查找字段索引
        for (size_t i = 0; i < structSymbol->fields.size(); ++i) {
            if (structSymbol->fields[i]->name == fieldName) {
                return static_cast<int>(i);
            }
        }
        
        return -1; // 未找到字段
    }
    catch (const std::exception& e) {
        reportError("Exception in getStructFieldIndex: " + std::string(e.what()));
        return -1;
    }
}

std::vector<std::string> ExpressionGenerator::generateCallArguments(std::shared_ptr<CallParams> callParams) {
    std::vector<std::string> args;
    
    if (!callParams) {
        return args;
    }
    
    for (const auto& expr : callParams->expressions) {
        std::string argReg = generateExpression(expr);
        if (!argReg.empty()) {
            args.push_back(argReg);
        }
    }
    
    return args;
}

bool ExpressionGenerator::isBuiltinFunction(const std::string& functionName) {
    // 检查是否为内置函数
    return functionName == "print" || functionName == "println" ||
           functionName == "printInt" || functionName == "printlnInt" ||
           functionName == "getString" || functionName == "getInt" ||
           functionName == "builtin_memset" || functionName == "builtin_memcpy" ||
           functionName == "exit";
}

std::string ExpressionGenerator::generateBuiltinCall(const std::string& functionName, 
                                                      const std::vector<std::string>& args) {
    try {
        // 根据内置函数名称生成相应的调用
        std::string returnType = "void"; // 大多数内置函数返回 void
        
        if (functionName == "getInt") {
            returnType = "i32";
        } else if (functionName == "getString") {
            returnType = "i8*";
        } else if (functionName == "builtin_memset" || functionName == "builtin_memcpy") {
            returnType = "i8*";
        }
        
        std::string resultReg = irBuilder->emitCall(functionName, args, returnType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBuiltinCall: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateTypeConversion(const std::string& valueReg, 
                                                       const std::string& fromType, 
                                                       const std::string& toType) {
    if (fromType == toType) {
        return valueReg; // 无需转换
    }
    
    try {
        std::string resultReg = irBuilder->newRegister();
        std::string instruction;
        
        // 整数类型转换
        if (typeMapper->isIntegerType(fromType) && typeMapper->isIntegerType(toType)) {
            if (fromType == "i1" && (toType == "i32" || toType == "i64")) {
                // bool 到整数扩展
                instruction = resultReg + " = zext " + fromType + " " + valueReg + " to " + toType;
            } else if ((fromType == "i32" || fromType == "i64") && toType == "i1") {
                // 整数到 bool 截断
                instruction = resultReg + " = trunc " + fromType + " " + valueReg + " to " + toType;
            } else if (fromType == "i32" && toType == "i64") {
                // i32 到 i64 扩展
                instruction = resultReg + " = sext " + fromType + " " + valueReg + " to " + toType;
            } else if (fromType == "i64" && toType == "i32") {
                // i64 到 i32 截断
                instruction = resultReg + " = trunc " + fromType + " " + valueReg + " to " + toType;
            } else {
                reportError("Unsupported integer type conversion: " + fromType + " to " + toType);
                return "";
            }
        } else {
            reportError("Unsupported type conversion: " + fromType + " to " + toType);
            return "";
        }
        
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(resultReg, toType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTypeConversion: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateArithmeticOperation(const std::string& left, const std::string& right, 
                                                           Token opType, const std::string& resultType) {
    try {
        std::string resultReg = irBuilder->newRegister();
        std::string instruction;
        
        switch (opType) {
            case Token::kPlus:
                instruction = resultReg + " = add " + resultType + " " + left + ", " + right;
                break;
            case Token::kMinus:
                instruction = resultReg + " = sub " + resultType + " " + left + ", " + right;
                break;
            case Token::kStar:
                instruction = resultReg + " = mul " + resultType + " " + left + ", " + right;
                break;
            case Token::kSlash:
                instruction = resultReg + " = sdiv " + resultType + " " + left + ", " + right;
                break;
            case Token::kPercent:
                instruction = resultReg + " = srem " + resultType + " " + left + ", " + right;
                break;
            default:
                reportError("Unsupported arithmetic operator");
                return "";
        }
        
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(resultReg, resultType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateArithmeticOperation: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateComparisonOperation(const std::string& left, const std::string& right, 
                                                          Token opType, const std::string& resultType) {
    try {
        std::string resultReg = irBuilder->newRegister();
        std::string condition = getComparisonCondition(opType);
        std::string instruction = resultReg + " = icmp " + condition + " " + resultType + " " + left + ", " + right;
        
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(resultReg, "i1");
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateComparisonOperation: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateLogicalOperation(const std::string& left, const std::string& right, 
                                                         Token opType, const std::string& resultType) {
    try {
        std::string resultReg = irBuilder->newRegister();
        std::string instruction;
        
        switch (opType) {
            case Token::kAnd:
                instruction = resultReg + " = and " + resultType + " " + left + ", " + right;
                break;
            case Token::kOr:
                instruction = resultReg + " = or " + resultType + " " + left + ", " + right;
                break;
            default:
                reportError("Unsupported logical operator");
                return "";
        }
        
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(resultReg, resultType);
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateLogicalOperation: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::getComparisonCondition(Token token) {
    switch (token) {
        case Token::kEqEq: return "eq";
        case Token::kNe: return "ne";
        case Token::kLt: return "slt";
        case Token::kLe: return "sle";
        case Token::kGt: return "sgt";
        case Token::kGe: return "sge";
        default: return "eq"; // 默认
    }
}

std::string ExpressionGenerator::generateUnitValue() {
    try {
        // 生成 unit 类型的默认值
        std::string resultReg = irBuilder->newRegister();
        std::string instruction = resultReg + " = add i32 0, 0"; // 使用 i32 0 作为 unit 值
        irBuilder->emitInstruction(instruction);
        irBuilder->setRegisterType(resultReg, "i32");
        return resultReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateUnitValue: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::typeToStringHelper(std::shared_ptr<Type> type) {
    if (!type) {
        return "i32"; // 默认类型
    }
    
    // 根据 Type 的具体子类型进行转换
    if (auto typePath = std::dynamic_pointer_cast<TypePath>(type)) {
        if (typePath->simplepathsegement) {
            return typePath->simplepathsegement->identifier;
        }
    } else if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(type)) {
        std::string elementType = typeToStringHelper(arrayType->type);
        std::string sizeStr = "10"; // 默认大小，实际应该从表达式求值
        return "[" + elementType + "; " + sizeStr + "]";
    } else if (auto refType = std::dynamic_pointer_cast<ReferenceType>(type)) {
        std::string targetType = typeToStringHelper(refType->type);
        return (refType->ismut ? "&mut " : "&") + targetType;
    } else if (auto unitType = std::dynamic_pointer_cast<UnitType>(type)) {
        return "()";
    }
    
    return "i32"; // 默认类型
}

// ==================== 块表达式和控制流表达式实现 ====================

std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> blockExpr) {
    if (!blockExpr) {
        reportError("BlockExpression is null");
        return "";
    }
    
    try {
        // 创建新的作用域
        scopeTree->EnterScope(Scope::ScopeType::Block, blockExpr.get());
        
        // 生成块中的所有语句
        if (!generateBlockStatements(blockExpr->statements)) {
            reportError("Failed to generate block statements");
            return "";
        }
        
        // 如果有尾表达式，生成它
        if (blockExpr->expressionwithoutblock) {
            std::string result = generateExpression(blockExpr->expressionwithoutblock);
            // 退出作用域
            scopeTree->ExitScope();
            return result;
        } else {
            // 没有尾表达式，返回 unit 值
            std::string result = generateUnitValue();
            // 退出作用域
            scopeTree->ExitScope();
            return result;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBlockExpression: " + std::string(e.what()));
        return "";
    }
}

bool ExpressionGenerator::generateBlockStatements(const std::vector<std::shared_ptr<Statement>>& statements) {
    if (!validateStatementGenerator()) {
        return false;
    }
    
    try {
        for (const auto& stmt : statements) {
            if (!stmt || !stmt->astnode) {
                reportError("Null statement in block");
                continue;
            }
            
            // 委托给 StatementGenerator 处理
            if (!statementGenerator->generateStatement(stmt)) {
                reportError("Failed to generate statement in block");
                return false;
            }
        }
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBlockStatements: " + std::string(e.what()));
        return false;
    }
}

std::string ExpressionGenerator::generateIfExpression(std::shared_ptr<IfExpression> ifExpr) {
    if (!ifExpr || !ifExpr->conditions || !ifExpr->ifblockexpression) {
        reportError("IfExpression or its components are null");
        return "";
    }
    
    try {
        // 生成条件表达式
        std::string conditionReg = generateExpression(ifExpr->conditions->expression);
        if (conditionReg.empty()) {
            reportError("Failed to generate if condition");
            return "";
        }
        
        // 创建基本块
        std::string thenBB = irBuilder->newBasicBlock("if.then");
        std::string elseBB = irBuilder->newBasicBlock("if.else");
        std::string endBB = irBuilder->newBasicBlock("if.end");
        
        // 生成条件跳转
        irBuilder->emitCondBr(conditionReg, thenBB, elseBB);
        
        // 生成 then 分支
        irBuilder->emitLabel(thenBB);
        std::string thenResult = generateExpression(ifExpr->ifblockexpression);
        irBuilder->emitBr(endBB);
        
        // 生成 else 分支（如果存在）
        std::string elseResult;
        if (ifExpr->elseexpression) {
            irBuilder->emitLabel(elseBB);
            elseResult = generateExpression(ifExpr->elseexpression);
            irBuilder->emitBr(endBB);
        } else {
            irBuilder->emitLabel(elseBB);
            elseResult = generateUnitValue();
            irBuilder->emitBr(endBB);
        }
        
        // 生成合并点
        irBuilder->emitLabel(endBB);
        
        // 简化处理：返回 then 分支的结果
        // 实际实现中应该使用 phi 节点
        return thenResult;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateIfExpression: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> loopExpr) {
    if (!loopExpr || !loopExpr->blockexpression) {
        reportError("LoopExpression or its block is null");
        return "";
    }
    
    try {
        // 创建循环基本块
        std::string startBB = irBuilder->newBasicBlock("loop.start");
        std::string bodyBB = irBuilder->newBasicBlock("loop.body");
        std::string endBB = irBuilder->newBasicBlock("loop.end");
        
        // 进入循环上下文
        enterLoopContext(startBB, endBB, bodyBB);
        
        // 跳转到循环开始
        irBuilder->emitBr(startBB);
        
        // 循环开始标签
        irBuilder->emitLabel(startBB);
        irBuilder->emitBr(bodyBB);
        
        // 循环体
        irBuilder->emitLabel(bodyBB);
        std::string bodyResult = generateExpression(loopExpr->blockexpression);
        irBuilder->emitBr(startBB);
        
        // 循环结束标签
        irBuilder->emitLabel(endBB);
        
        // 退出循环上下文
        exitLoopContext();
        
        // 循环表达式返回 unit 类型
        return generateUnitValue();
    }
    catch (const std::exception& e) {
        reportError("Exception in generateLoopExpression: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateWhileExpression(std::shared_ptr<PredicateLoopExpression> whileExpr) {
    if (!whileExpr || !whileExpr->conditions || !whileExpr->blockexpression) {
        reportError("WhileExpression or its components are null");
        return "";
    }
    
    try {
        // 创建循环基本块
        std::string condBB = irBuilder->newBasicBlock("while.cond");
        std::string bodyBB = irBuilder->newBasicBlock("while.body");
        std::string endBB = irBuilder->newBasicBlock("while.end");
        
        // 进入循环上下文
        enterLoopContext(condBB, endBB, bodyBB);
        
        // 跳转到条件检查
        irBuilder->emitBr(condBB);
        
        // 条件检查
        irBuilder->emitLabel(condBB);
        std::string conditionReg = generateExpression(whileExpr->conditions->expression);
        if (conditionReg.empty()) {
            reportError("Failed to generate while condition");
            exitLoopContext();
            return "";
        }
        irBuilder->emitCondBr(conditionReg, bodyBB, endBB);
        
        // 循环体
        irBuilder->emitLabel(bodyBB);
        std::string bodyResult = generateExpression(whileExpr->blockexpression);
        irBuilder->emitBr(condBB);
        
        // 循环结束
        irBuilder->emitLabel(endBB);
        
        // 退出循环上下文
        exitLoopContext();
        
        // while 表达式返回 unit 类型
        return generateUnitValue();
    }
    catch (const std::exception& e) {
        reportError("Exception in generateWhileExpression: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateBreakExpression(std::shared_ptr<BreakExpression> breakExpr) {
    if (!isInLoopContext()) {
        reportError("break statement outside of loop");
        return "";
    }
    
    try {
        // 获取当前循环上下文
        LoopContext loopCtx = getCurrentLoopContext();
        
        // 如果有 break 值表达式，生成它
        if (breakExpr->expression) {
            std::string breakValue = generateExpression(breakExpr->expression);
            setBreakValue(breakValue, getExpressionType(breakExpr->expression));
        } else {
            setBreakValue(generateUnitValue(), "i32");
        }
        
        // 跳转到循环结束
        irBuilder->emitBr(loopCtx.loopEndBB);
        
        // break 表达式不会返回（控制流转移）
        return "";
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBreakExpression: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateContinueExpression(std::shared_ptr<ContinueExpression> continueExpr) {
    if (!isInLoopContext()) {
        reportError("continue statement outside of loop");
        return "";
    }
    
    try {
        // 获取当前循环上下文
        LoopContext loopCtx = getCurrentLoopContext();
        
        // 跳转到循环开始或条件检查
        irBuilder->emitBr(loopCtx.loopStartBB);
        
        // continue 表达式不会返回（控制流转移）
        return "";
    }
    catch (const std::exception& e) {
        reportError("Exception in generateContinueExpression: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::generateReturnExpression(std::shared_ptr<ReturnExpression> returnExpr) {
    try {
        // 如果有返回值表达式，生成它
        if (returnExpr->expression) {
            std::string returnValue = generateExpression(returnExpr->expression);
            if (returnValue.empty()) {
                reportError("Failed to generate return value");
                return "";
            }
            
            // 生成返回指令
            irBuilder->emitRet(returnValue);
        } else {
            // 无返回值
            irBuilder->emitRetVoid();
        }
        
        // return 表达式不会返回（控制流转移）
        return "";
    }
    catch (const std::exception& e) {
        reportError("Exception in generateReturnExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 循环上下文管理实现 ====================

void ExpressionGenerator::enterLoopContext(const std::string& startBB, const std::string& endBB, const std::string& bodyBB) {
    loopContextStack.push(LoopContext(startBB, endBB, bodyBB));
}

void ExpressionGenerator::exitLoopContext() {
    if (!loopContextStack.empty()) {
        loopContextStack.pop();
    }
}

ExpressionGenerator::LoopContext ExpressionGenerator::getCurrentLoopContext() {
    if (loopContextStack.empty()) {
        reportError("No loop context available");
        throw std::runtime_error("No loop context available");
    }
    return loopContextStack.top();
}

bool ExpressionGenerator::isInLoopContext() const {
    return !loopContextStack.empty();
}

void ExpressionGenerator::setBreakValue(const std::string& valueReg, const std::string& valueType) {
    if (!loopContextStack.empty()) {
        loopContextStack.top().hasBreak = true;
        loopContextStack.top().breakValueReg = valueReg;
        loopContextStack.top().breakValueType = valueType;
    }
}

// ==================== 其他辅助方法实现 ====================

std::string ExpressionGenerator::storeExpressionResult(std::shared_ptr<Expression> expression, const std::string& valueReg) {
    if (!expression || valueReg.empty()) {
        reportError("Invalid parameters for storeExpressionResult");
        return "";
    }
    
    try {
        // 分析左值表达式
        auto [ptrReg, type] = analyzeLValue(expression);
        if (ptrReg.empty()) {
            reportError("Cannot store to non-lvalue expression");
            return "";
        }
        
        // 存储值
        irBuilder->emitStore(valueReg, ptrReg);
        return ptrReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in storeExpressionResult: " + std::string(e.what()));
        return "";
    }
}

// ==================== 新增的辅助方法实现 ====================

std::string ExpressionGenerator::getFunctionReturnType(const std::string& functionName) {
    try {
        // 从符号表查找函数定义
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            return "";
        }
        
        auto symbol = currentScope->Lookup(functionName);
        if (!symbol || symbol->kind != SymbolKind::Function) {
            return "";
        }
        
        auto functionSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
        if (!functionSymbol || !functionSymbol->returntype) {
            return "";
        }
        
        // 将语义类型映射为 LLVM 类型
        return typeMapper->mapSemanticTypeToLLVM(functionSymbol->returntype);
    }
    catch (const std::exception& e) {
        reportError("Exception in getFunctionReturnType: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::getStructFieldType(const std::string& structName, const std::string& fieldName) {
    try {
        // 从符号表查找结构体定义
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            return "";
        }
        
        auto symbol = currentScope->Lookup(structName);
        if (!symbol || symbol->kind != SymbolKind::Struct) {
            return "";
        }
        
        auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(symbol);
        if (!structSymbol) {
            return "";
        }
        
        // 查找字段类型
        for (const auto& field : structSymbol->fields) {
            if (field->name == fieldName && field->type) {
                return typeMapper->mapSemanticTypeToLLVM(field->type);
            }
        }
        
        return ""; // 未找到字段
    }
    catch (const std::exception& e) {
        reportError("Exception in getStructFieldType: " + std::string(e.what()));
        return "";
    }
}

std::string ExpressionGenerator::loadExpressionValue(std::shared_ptr<Expression> expression) {
    if (!expression) {
        reportError("Expression is null in loadExpressionValue");
        return "";
    }
    
    try {
        // 分析左值表达式
        auto [ptrReg, type] = analyzeLValue(expression);
        if (ptrReg.empty()) {
            // 不是左值，直接生成表达式
            return generateExpression(expression);
        }
        
        // 从指针加载值
        return irBuilder->emitLoad(ptrReg, type);
    }
    catch (const std::exception& e) {
        reportError("Exception in loadExpressionValue: " + std::string(e.what()));
        return "";
    }
}


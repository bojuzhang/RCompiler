#include "expressiongenerator.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <string>

// 包含 StatementGenerator 头文件以解决前向声明问题
#include "astnodes.hpp"
#include "lexer.hpp"
#include "statementgenerator.hpp"
#include "symbol.hpp"
#include "typecheck.hpp"
#include "typemapper.hpp"

// ==================== 构造函数和基本初始化方法 ====================

ExpressionGenerator::ExpressionGenerator(std::shared_ptr<IRBuilder> irBuilder,
                                         std::shared_ptr<TypeMapper> typeMapper,
                                         std::shared_ptr<ScopeTree> scopeTree,
                                         const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& nodeTypeMap,
                                         std::shared_ptr<TypeChecker> typeChecker)
    : irBuilder(irBuilder)
    , typeMapper(typeMapper)
    , scopeTree(scopeTree)
    , statementGenerator(nullptr)
    , nodeTypeMap(nodeTypeMap)
    , typeChecker(typeChecker)
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
        else if (auto borrowExpr = std::dynamic_pointer_cast<BorrowExpression>(expression)) {
            return generateBorrowExpression(borrowExpr);
        }
        else if (auto derefExpr = std::dynamic_pointer_cast<DereferenceExpression>(expression)) {
            return generateDereferenceExpression(derefExpr);
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
        else if (auto groupedExpr = std::dynamic_pointer_cast<GroupedExpression>(expression)) {
            return generateGroupedExpression(groupedExpr);
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
                auto literal = literalExpr->literal;
                if (literal.length() >= 5) {
                    if (literal.substr(literal.length() - 5) == "usize") {
                        literal = literal.substr(0, literal.length() - 5);
                    }
                    if (literal.substr(literal.length() - 5) == "isize") {
                        literal = literal.substr(0, literal.length() - 5);
                    }
                }
                
                if (literal.length() >= 3) {
                    std::string suffix = literal.substr(literal.length() - 3);
                    if (suffix == "u32") {
                        literal = literal.substr(0, literal.length() - 3);
                    }
                    if (suffix == "i32") {
                        literal = literal.substr(0, literal.length() - 3);
                    }
                }

                if (literal.size() > 2) {
                    if (literal.substr(0, 2) == "0x") {
                        literal = std::to_string(std::stoll(literal.substr(2, literal.size() - 2), nullptr, 16));
                    } else if (literal.substr(0, 2) == "0o") {
                        literal = std::to_string(std::stoll(literal.substr(2, literal.size() - 2), nullptr, 8));
                    } else if (literal.substr(0, 2) == "0b") {
                        literal = std::to_string(std::stoll(literal.substr(2, literal.size() - 2), nullptr, 2));
                    }
                }
                
                std::string instruction = resultReg + " = add " + llvmType + " 0, " + literal;
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
        std::string variableName;
        
        // 特殊处理 self 和 Self
        if (lastSegment->isself) {
            variableName = "self";
        } else if (lastSegment->isSelf) {
            variableName = "Self";
        } else {
            variableName = lastSegment->identifier;
        }
        
        // 从符号表查找变量
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            reportError("No current scope available");
            return "";
        }
        
        auto symbol = currentScope->Lookup(variableName);
        if (!symbol) {
            // 特殊处理self：如果找不到self，尝试从参数寄存器中获取
            if (variableName == "self") {
                // 尝试获取self_ptr寄存器
                std::string selfPtrReg = irBuilder->getVariableRegister("self_ptr");
                if (!selfPtrReg.empty()) {
                    // 找到self_ptr，返回它
                    std::string llvmType = "%struct_*"; // 默认结构体指针类型
                    return selfPtrReg;
                }
                
                // 如果还是找不到，尝试直接使用参数寄存器
                // 在impl方法中，self通常是第一个参数
                return "%param_0";
            }
            
            reportError("Undefined variable: " + variableName);
            return "";
        }
        
        // 根据符号类型处理
        if (symbol->kind == SymbolKind::Variable || symbol->kind == SymbolKind::Constant) {
            if (symbol->kind == SymbolKind::Constant) {
                return typeChecker->constantEvaluator->GetConstantValue(variableName)->toString();
            }
            // 变量或常量
            std::string variableReg = irBuilder->getVariableRegister(variableName);
            if (variableReg.empty()) {
                reportError("Variable register not found: " + variableName);
                return "";
            }
            
            std::string llvmType = typeMapper->getPointedType(irBuilder->getRegisterType(variableReg));

            if (llvmType.empty()) {
                llvmType = typeMapper->mapSemanticTypeToLLVM(symbol->type);
            }
            
            // 检查是否为引用类型（&T 或 &mut T）
            if (auto refType = std::dynamic_pointer_cast<ReferenceType>(symbol->type)) {
                // 对于引用类型，返回指针而不是加载值
                return variableReg;
            }
            // 检查是否为聚合类型
            else if (irBuilder->isAggregateType(llvmType)) {
                // 聚合类型：返回指针而不是加载值
                // 这样在 let 语句初始化和赋值表达式中可以直接使用 memcpy
                return variableReg;
            } else {
                // 非聚合类型：加载值
                std::string valueReg = irBuilder->emitLoad(variableReg, llvmType);
                return valueReg;
            }
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

        if (arrayExpr.get()->arrayelements->istwo) {
            size_t arraySize = typeChecker->EvaluateArraySize(*elements[1].get());
            // 分配数组空间
            std::string arrayType = "[" + std::to_string(arraySize) + " x " + elementType + "]";
            std::string arrayReg = irBuilder->emitAlloca(arrayType);
            
            // 初始化数组元素
            for (size_t i = 0; i < arraySize; ++i) {
                // 生成元素表达式
                std::string elementReg = generateExpression(elements[0]);
                if (elementReg.empty()) {
                    reportError("Failed to generate array element at index " + std::to_string(i));
                    continue;
                }
                
                // 计算元素地址
                std::vector<std::string> indices = {"0", std::to_string(i)};
                std::string elementPtrReg = irBuilder->emitGetElementPtr(arrayReg, indices, arrayType);
                
                // 存储元素值
                // 检查元素类型是否为聚合类型，如果是则使用 memcpy
                if (irBuilder->isAggregateType(elementType)) {
                    irBuilder->emitAggregateCopy(elementPtrReg, elementReg, elementType);
                } else {
                    irBuilder->emitStore(elementReg, elementPtrReg);
                }
            }
            
            // 对于数组表达式，我们需要返回数组的值而不是指针
            // 但是由于数组是聚合类型，我们需要在赋值时使用 memcpy
            // 所以这里返回指针，让调用者处理
            return arrayReg;
        } else {
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
                std::string elementPtrReg = irBuilder->emitGetElementPtr(arrayReg, indices, arrayType);
                
                // 存储元素值
                // 检查元素类型是否为聚合类型，如果是则使用 memcpy
                if (irBuilder->isAggregateType(elementType)) {
                    irBuilder->emitAggregateCopy(elementPtrReg, elementReg, elementType);
                } else {
                    irBuilder->emitStore(elementReg, elementPtrReg);
                }
            }
            
            // 对于数组表达式，我们需要返回数组的值而不是指针
            // 但是由于数组是聚合类型，我们需要在赋值时使用 memcpy
            // 所以这里返回指针，让调用者处理
            return arrayReg;
        }
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
        // 生成索引表达式
        std::string indexReg = generateExpression(indexExpr->expressionin);
        if (indexReg.empty()) {
            reportError("Failed to generate index expression");
            return "";
        }
        
        // 获取基础表达式的类型
        std::string baseType = getExpressionType(indexExpr->expressionout);
        
        // 确定元素类型
        std::string elementType = baseType;
        // 解指针
        while (typeMapper->isPointerType(elementType)) {
            // 指针索引
            elementType = typeMapper->getPointedType(elementType);
        }

        if (typeMapper->isArrayType(elementType)) {
            // 数组索引
            elementType = typeMapper->getArrayElementType(elementType);
        } else {
            reportError("Cannot index into non-pointer, non-array type: " + baseType);
            return "";
        }
        
        // 生成元素地址
        std::string elementPtrReg;
        
        // 检查基础表达式是否是路径表达式（变量访问）
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(indexExpr->expressionout)) {
            // 对于数组变量，直接从变量对应的指针读取元素
            auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
            std::string variableName;
            
            // 特殊处理 self 和 Self
            if (lastSegment->isself) {
                variableName = "self";
            } else if (lastSegment->isSelf) {
                variableName = "Self";
            } else {
                variableName = lastSegment->identifier;
            }
            
            std::string varReg = irBuilder->getVariableRegister(variableName);
            if (varReg.empty()) {
                reportError("Variable register not found: " + variableName);
                return "";
            }
            
            // 使用变量指针进行索引
            std::vector<std::string> indices = {"0", indexReg};
            // 对于数组索引，resultType 应该是数组类型，而不是元素指针类型
            // 从 baseType 中提取数组大小和元素类型
            std::string arraySize = typeMapper->getArraySize(baseType);
            // 去除可能的空格
            arraySize.erase(0, arraySize.find_first_not_of(" \t\n\r"));
            arraySize.erase(arraySize.find_last_not_of(" \t\n\r") + 1);
            std::string arrayType = "[" + arraySize + " x " + elementType + "]";
            elementPtrReg = irBuilder->emitGetElementPtr(varReg, indices, arrayType);
        } else if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(indexExpr->expressionout)) {
            // 处理结构体字段访问中的数组索引，如 self.data[self.top]
            // 获取接收者类型
            std::string receiverType = getExpressionType(fieldExpr->expression);
            
            // 对于字段访问，我们需要接收者的指针
            std::string receiverPtrReg;
            
            // 检查接收者是否为路径表达式（变量访问）
            if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(fieldExpr->expression)) {
                // 对于变量访问，获取变量的指针寄存器
                auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                std::string variableName;
                
                // 特殊处理 self 和 Self
                if (lastSegment->isself) {
                    variableName = "self";
                } else if (lastSegment->isSelf) {
                    variableName = "Self";
                } else {
                    variableName = lastSegment->identifier;
                }
                
                receiverPtrReg = irBuilder->getVariableRegister(variableName);
                // receiverType = irBuilder->getRegisterType(receiverPtrReg);
            } else {
                // 对于其他类型的表达式，生成表达式然后获取其地址
                std::string receiverReg = generateExpression(fieldExpr->expression);
                if (receiverReg.empty()) {
                    reportError("Failed to generate receiver expression for field access");
                    return "";
                }
                
                // 如果接收者不是指针，需要分配内存并存储
                std::string receiverRegType = irBuilder->getRegisterType(receiverReg);
                if (!irBuilder->isPointerType(receiverRegType)) {
                    receiverPtrReg = irBuilder->emitAlloca(receiverType);
                    irBuilder->emitStore(receiverReg, receiverPtrReg);
                } else {
                    receiverPtrReg = receiverReg;
                }
            }
            
            // 计算字段地址（这里是数组字段的地址）
            std::string fieldPtrReg = generateFieldAccessAddress(receiverPtrReg, fieldExpr->identifier, receiverType);
            if (fieldPtrReg.empty()) {
                reportError("Failed to calculate field address: " + fieldExpr->identifier);
                return "";
            }
            
            // 现在fieldPtrReg是指向数组字段的指针，可以直接进行索引
            // 对于指向数组的指针 [N x T]*，我们需要两个索引：0（数组起始）和元素索引
            std::vector<std::string> indices = {"0", indexReg};
            
            // 关键修复：直接从字段类型获取数组类型，而不依赖fieldPtrReg的类型
            std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
            std::string arrayType = fieldType;
            
            // 确保arrayType是数组类型
            if (!typeMapper->isArrayType(arrayType)) {
                reportError("Field is not an array type: " + fieldExpr->identifier + " -> " + arrayType);
                return "";
            }
            
            elementPtrReg = irBuilder->emitGetElementPtr(fieldPtrReg, indices, arrayType);
        } else {
            // 生成基础表达式（数组或指针）
            std::string baseReg = generateExpression(indexExpr->expressionout);
            if (baseReg.empty()) {
                reportError("Failed to generate base expression for indexing");
                return "";
            }
            
            if (typeMapper->isPointerType(baseType)) {
                // 指针索引：直接使用指针和索引
                std::vector<std::string> indices = {indexReg};
                elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
            } else {
                // 数组索引：需要添加数组起始偏移
                std::vector<std::string> indices = {"0", indexReg};
                elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
            }
        }
        
        // 加载元素值
        if (irBuilder->isAggregateType(elementType)) {
            return elementPtrReg;
        }
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
        
        // 确保寄存器类型正确设置为指针类型
        irBuilder->setRegisterType(structReg, structType + "*");
        
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
                // 检查字段类型，如果是聚合类型则使用 memcpy
                std::string fieldType = getStructFieldType(structType, field->identifier);
                if (!fieldType.empty() && irBuilder->isAggregateType(fieldType)) {
                    irBuilder->emitAggregateCopy(fieldPtrReg, fieldReg, fieldType);
                } else {
                    irBuilder->emitStore(fieldReg, fieldPtrReg);
                }
            }
        }
        
        // 对于结构体表达式，返回结构体的指针
        // 这样在 sret 机制中可以直接存储到返回槽
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
        // 获取函数名和路径信息
        std::string functionName;
        std::string structName; // 用于关联函数
        bool isAssociatedFunction = false;
        
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(callExpr->expression)) {
            // 检查是否为关联函数调用（如 Type::function）
            if (pathExpr->simplepath->simplepathsegements.size() >= 2) {
                // 这是关联函数调用，如 Stack::new()
                auto typeSegment = pathExpr->simplepath->simplepathsegements[0];
                auto functionSegment = pathExpr->simplepath->simplepathsegements.back();
                
                if (!typeSegment->isself && !typeSegment->isSelf) {
                    structName = typeSegment->identifier;
                    functionName = structName + "_" + functionSegment->identifier;
                    isAssociatedFunction = true;
                }
            }
            
            // 如果不是关联函数，处理普通函数调用
            if (!isAssociatedFunction) {
                auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                
                // 特殊处理 self 和 Self
                if (lastSegment->isself) {
                    functionName = "self";
                } else if (lastSegment->isSelf) {
                    functionName = "Self";
                } else {
                    functionName = lastSegment->identifier;
                }
            }
        } else {
            reportError("Unsupported function expression type");
            return "";
        }
        
        // 检查是否为内置函数
        if (isBuiltinFunction(functionName)) {
            std::vector<std::string> args = generateCallArguments(callExpr->callparams);
            return generateBuiltinCall(functionName, args);
        }
        
        // 用户定义函数：使用返回槽机制
        std::vector<std::string> args = generateCallArguments(callExpr->callparams);
        
        // 获取函数返回类型
        std::string returnType = getFunctionReturnType(functionName);
        if (returnType.empty()) {
            // 如果找不到函数信息，尝试从类型映射中获取
            auto it = nodeTypeMap.find(callExpr.get());
            if (it != nodeTypeMap.end() && it->second) {
                returnType = typeMapper->mapSemanticTypeToLLVM(it->second);
                // 对于结构体类型，函数应该返回指针类型
                if (typeMapper->isStructType(returnType)) {
                    returnType = returnType + "*";
                }
            } else {
                returnType = "i32"; // 默认返回类型
            }
        }
        
        // 特殊处理：如果这是关联函数调用且返回类型是 Self，确保返回指针类型
        if (isAssociatedFunction && returnType != "i32" && returnType != "void" &&
            returnType != "i8*" && returnType != "i1" && returnType.find("*") == std::string::npos) {
            // 如果返回类型是结构体类型但没有指针，添加指针
            if (returnType.find("%struct_") == 0) {
                returnType = returnType + "*";
            }
        }
        
        // 对于聚合类型返回值，确保返回指针类型而不是值类型
        if (returnType != "i32" && returnType != "void" && returnType != "i8*" && returnType != "i1" &&
            (returnType.find("%struct_") == 0 || returnType.find("[") == 0)) {
            if (returnType.find("*") == std::string::npos) {
                returnType = returnType + "*";
            }
        }
        
        // 用户定义函数调用：使用返回槽机制
        return generateUserDefinedFunctionCall(functionName, args, callExpr);
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
        
        // 获取接收者类型
        std::string receiverType = getExpressionType(methodCallExpr->receiver);
        
        // 构建方法名（简化处理：receiver_type_method_name）
        std::string methodName;
        if (typeMapper->isStructType(receiverType)) {
            // 对于结构体类型，去掉 %struct_ 前缀
            std::string structName = receiverType;
            if (structName.find("%struct_") == 0) {
                structName = structName.substr(8); // 去掉 "%struct_" 前缀
            }
            while (structName.ends_with('*')) {
                structName.pop_back();
            }
            methodName = structName + "_" + methodCallExpr->method_name;
        } else {
            methodName = receiverType + "_" + methodCallExpr->method_name;
        }
        
        // 生成参数表达式（包括接收者作为第一个参数）
        std::vector<std::string> args = generateCallArguments(methodCallExpr->callparams);
        
        // 检查接收者类型，如果是聚合类型需要特殊处理
        if (irBuilder->isAggregateType(receiverType)) {
            // 对于方法调用，接收者需要作为 self 参数传递
            // 获取接收者的地址（指针）
            std::string receiverPtrReg;
            
            // 检查接收者是否为左值（变量、字段访问等）
            if (isLValue(methodCallExpr->receiver)) {
                auto [lvaluePtr, lvalueType] = analyzeLValue(methodCallExpr->receiver);
                if (!lvaluePtr.empty()) {
                    receiverPtrReg = lvaluePtr;
                } else {
                    reportError("Failed to get receiver address for method call");
                    return "";
                }
            } else {
                // 对于右值，需要分配临时空间并存储
                receiverPtrReg = irBuilder->emitAlloca(receiverType);
                // 对于聚合类型，使用 builtin_memcpy 而不是 store
                if (irBuilder->isAggregateType(receiverType)) {
                    irBuilder->emitAggregateCopy(receiverPtrReg, receiverReg, receiverType);
                } else {
                    irBuilder->emitStore(receiverReg, receiverPtrReg);
                }
            }
            
            // 将接收者指针作为第一个参数传递（self 参数）
            args.insert(args.begin(), receiverPtrReg);
        } else {
            // 非聚合类型，直接传递值
            args.insert(args.begin(), receiverReg);
        }
        
        // 检查是否为用户定义函数（需要 sret 机制）
        bool isUserDefined = !isBuiltinFunction(methodName);
        
        if (isUserDefined) {
            // 获取方法返回类型
            std::string returnType = getFunctionReturnType(methodName);
            if (returnType.empty()) {
                // 如果找不到函数信息，尝试从类型映射中获取
                auto it = nodeTypeMap.find(methodCallExpr.get());
                if (it != nodeTypeMap.end() && it->second) {
                    returnType = typeMapper->mapSemanticTypeToLLVM(it->second);
                    // // 对于结构体类型，函数应该返回指针类型
                    // if (typeMapper->isStructType(returnType)) {
                    //     returnType = returnType + "*";
                    // }
                } else {
                    returnType = "i32"; // 默认返回类型
                }
            }
            
            // 如果返回类型是void，直接调用
            if (returnType == "void") {
                irBuilder->emitCall(methodName, args, "void");
                return ""; // void函数调用不返回值
            }
            
            // 分配返回槽空间
            std::string returnSlotPtr = irBuilder->emitAlloca(returnType);
            irBuilder->setRegisterType(returnSlotPtr, returnType + "*");
            
            // 构建参数列表：返回槽指针 + 原始参数
            std::vector<std::string> finalArgs;
            finalArgs.push_back(returnSlotPtr); // 第一个参数是返回槽指针
            finalArgs.insert(finalArgs.end(), args.begin(), args.end());
            
            // 调用用户定义函数（返回void）
            irBuilder->emitCall(methodName, finalArgs, "void");
            
            // 从返回槽加载结果
            // 如果是聚合类型则不 load
            if (typeMapper->isPointerType(irBuilder->getRegisterType(returnSlotPtr)) &&
               irBuilder->isAggregateType(typeMapper->getPointedType(irBuilder->getRegisterType(returnSlotPtr)))) {
                return returnSlotPtr;
            }
            std::string resultReg = irBuilder->emitLoad(returnSlotPtr, returnType);
            irBuilder->setRegisterType(resultReg, returnType);
            
            return resultReg;
        } else {
            // 内置函数调用
            // 获取方法返回类型
            std::string returnType = "i32"; // 默认返回类型
            
            // 生成方法调用
            std::string resultReg = irBuilder->emitCall(methodName, args, returnType);
            return resultReg;
        }
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
        // 获取接收者类型
        std::string receiverType = getExpressionType(fieldExpr->expression);
        
        // 对于字段访问，我们需要接收者的指针
        std::string receiverPtrReg;
        
        // 检查接收者是否为路径表达式（变量访问）
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(fieldExpr->expression)) {
            // 对于变量访问，获取变量的指针寄存器
            auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
            std::string variableName;
            
            // 特殊处理 self 和 Self
            if (lastSegment->isself) {
                variableName = "self";
            } else if (lastSegment->isSelf) {
                variableName = "Self";
            } else {
                variableName = lastSegment->identifier;
            }
            
            receiverPtrReg = irBuilder->getVariableRegister(variableName);
            receiverType = irBuilder->getRegisterType(receiverPtrReg);
        } else {
            // 对于其他类型的表达式，生成表达式然后获取其地址
            std::string receiverReg = generateExpression(fieldExpr->expression);
            if (receiverReg.empty()) {
                reportError("Failed to generate receiver expression for field access");
                return "";
            }
            
            // 如果接收者不是指针，需要分配内存并存储
            std::string receiverRegType = irBuilder->getRegisterType(receiverReg);
            if (!irBuilder->isPointerType(receiverRegType)) {
                receiverPtrReg = irBuilder->emitAlloca(receiverType);
                irBuilder->emitStore(receiverReg, receiverPtrReg);
            } else {
                receiverPtrReg = receiverReg;
            }
        }

        // std::cerr << "Field: " << receiverType << " " << receiverPtrReg << " " << fieldExpr->identifier << "\n";
        
        // 确保我们有指针
        std::string receiverPtrRegType = irBuilder->getRegisterType(receiverPtrReg);
        if (!irBuilder->isPointerType(receiverPtrRegType)) {
            reportError("Receiver is not a pointer type: " + receiverPtrRegType);
            return "";
        }
        
        // 计算字段地址
        std::string fieldPtrReg = generateFieldAccessAddress(receiverPtrReg, fieldExpr->identifier, receiverType);
        // std::cerr << "fieldPtrReg: " << fieldPtrReg << "\n";
        if (fieldPtrReg.empty()) {
            reportError("Failed to calculate field address: " + fieldExpr->identifier);
            return "";
        }
        
        // 获取字段类型
        std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
        if (fieldType.empty()) {
            fieldType = "i32"; // 默认类型
        }
        
        // 检查字段类型是否为聚合类型
        if (irBuilder->isAggregateType(fieldType)) {
            // 对于聚合类型字段（如数组），返回指针而不是加载值
            // 这样在赋值和索引操作中可以直接使用
            
            // 关键修复：确保fieldPtrReg的类型正确设置为字段类型的指针
            // 但是generateFieldAccessAddress已经设置了正确的类型，这里不需要重复设置
            // irBuilder->setRegisterType(fieldPtrReg, fieldType + "*");
            
            return fieldPtrReg;
        } else {
            // 对于基本类型字段，加载值
            std::string valueReg = irBuilder->emitLoad(fieldPtrReg, fieldType);
            return valueReg;
        }
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
                std::string instruction = resultReg + " = xor " + operandType + " " + operandReg + ", -1";
                irBuilder->emitInstruction(instruction);
                irBuilder->setRegisterType(resultReg, operandType);
                break;
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

// ==================== 借用表达式生成方法 ====================

std::string ExpressionGenerator::generateBorrowExpression(std::shared_ptr<BorrowExpression> borrowExpr) {
    if (!borrowExpr || !borrowExpr->expression) {
        reportError("BorrowExpression or its expression is null");
        return "";
    }
    
    try {
        // 获取操作数的类型
        std::string operandType = getExpressionType(borrowExpr->expression);
        
        // 检查是否为聚合类型
        bool isAggregate = irBuilder->isAggregateType(operandType);
        
        if (isAggregate) {
            // 对于聚合类型，不应该再添加一层指针，直接返回操作数的地址
            if (isLValue(borrowExpr->expression)) {
                auto [lvaluePtr, lvalueType] = analyzeLValue(borrowExpr->expression);
                if (!lvaluePtr.empty()) {
                    // 对于聚合类型，直接返回指针，不需要额外的间接层
                    return lvaluePtr;
                } else {
                    reportError("Failed to get lvalue address for aggregate borrow expression");
                    return "";
                }
            } else {
                // 对于右值聚合类型，需要先存储到临时位置，然后返回地址
                std::string operandReg = generateExpression(borrowExpr->expression);
                if (operandReg.empty()) {
                    reportError("Failed to generate operand for aggregate borrow expression");
                    return "";
                }
                
                // 分配空间存储右值
                std::string tempStorage = irBuilder->emitAlloca(operandType);
                
                // 对于聚合类型，使用 builtin_memcpy 而不是 store
                irBuilder->emitAggregateCopy(tempStorage, operandReg, operandType);
                
                // 返回临时存储的地址
                return tempStorage;
            }
        } else {
            // 对于非聚合类型，使用原有的逻辑
            // 对于 &T 表达式，结果类型应该是 T*
            std::string pointerType = operandType + "*";
            
            // 创建临时变量来存储指针（分配 T** 空间）
            std::string tempPtrPtr = irBuilder->emitAlloca(pointerType);
            irBuilder->setRegisterType(tempPtrPtr, pointerType + "*");
            
            // 如果操作数是左值（变量、字段访问等），获取其地址
            if (isLValue(borrowExpr->expression)) {
                auto [lvaluePtr, lvalueType] = analyzeLValue(borrowExpr->expression);
                if (!lvaluePtr.empty()) {
                    // 直接存储左值的地址到 T** 中
                    irBuilder->emitStore(lvaluePtr, tempPtrPtr);
                } else {
                    reportError("Failed to get lvalue address for borrow expression");
                    return "";
                }
            } else {
                // 对于右值，需要先存储到临时位置，然后取地址
                std::string operandReg = generateExpression(borrowExpr->expression);
                if (operandReg.empty()) {
                    reportError("Failed to generate operand for borrow expression");
                    return "";
                }
                
                // 分配空间存储右值
                std::string tempStorage = irBuilder->emitAlloca(operandType);
                irBuilder->emitStore(operandReg, tempStorage);
                
                // 将右值的地址存储到 T** 中
                irBuilder->emitStore(tempStorage, tempPtrPtr);
            }
            
            // 加载指针值（从 T** 加载 T*）
            std::string resultReg = irBuilder->emitLoad(tempPtrPtr, pointerType);
            irBuilder->setRegisterType(resultReg, pointerType);
            
            return resultReg;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateBorrowExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 解引用表达式生成方法 ====================

std::string ExpressionGenerator::generateDereferenceExpression(std::shared_ptr<DereferenceExpression> derefExpr) {
    if (!derefExpr || !derefExpr->expression) {
        reportError("DereferenceExpression or its expression is null");
        return "";
    }
    
    try {
        // 生成指针表达式
        std::string pointerReg = generateExpression(derefExpr->expression);
        if (pointerReg.empty()) {
            reportError("Failed to generate pointer for dereference expression");
            return "";
        }
        
        // 获取指针类型
        std::string pointerType = irBuilder->getRegisterType(pointerReg);
        if (pointerType.empty()) {
            pointerType = getExpressionType(derefExpr->expression);
        }

        // 确保是指针类型
        if (!typeMapper->isPointerType(pointerType)) {
            reportError("Cannot dereference non-pointer type: " + pointerType);
            return "";
        }
        
        // 获取指向的类型
        std::string elementType = typeMapper->getPointedType(pointerType);
        
        // 检查是否为聚合类型
        bool isAggregate = irBuilder->isAggregateType(elementType);
        
        if (isAggregate) {
            // 对于聚合类型，不应该专门 load 一次，直接返回指针
            // 这样在后续的字段访问等操作中可以直接使用
            return pointerReg;
        } else {
            // 对于非聚合类型，需要加载值
            std::string resultReg = irBuilder->emitLoad(pointerReg, elementType);
            irBuilder->setRegisterType(resultReg, elementType);
            return resultReg;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateDereferenceExpression: " + std::string(e.what()));
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
        
        // 在 BinaryExpression 阶段判断是否应该使用无符号运算
        bool isUnsigned = shouldUseUnsignedOperation(binaryExpr);
        
        // 根据运算符类型生成相应的 IR
        switch (binaryExpr->binarytype) {
            case Token::kPlus:
            case Token::kMinus:
            case Token::kStar:
            case Token::kSlash:
            case Token::kPercent:
                return generateArithmeticOperation(leftReg, rightReg, binaryExpr->binarytype, resultType, isUnsigned);
                
            case Token::kEqEq:
            case Token::kNe:
            case Token::kLt:
            case Token::kLe:
            case Token::kGt:
            case Token::kGe:
                return generateComparisonOperation(leftReg, rightReg, binaryExpr->binarytype, resultType, isUnsigned);
                
            case Token::kAndAnd:
            case Token::kOrOr:
                return generateLogicalOperation(leftReg, rightReg, binaryExpr->binarytype, resultType);
                
            case Token::kAnd:
            case Token::kOr:
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
        
        // 检查右值类型
        std::string rightType = getExpressionType(assignExpr->rightexpression);
        std::string rightReg;
        
        // 对于聚合类型的特殊处理
        if (irBuilder->isAggregateType(rightType)) {
            // 如果右值是路径表达式（变量），直接获取其指针
            if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(assignExpr->rightexpression)) {
                auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                std::string variableName;
                
                // 特殊处理 self 和 Self
                if (lastSegment->isself) {
                    variableName = "self";
                } else if (lastSegment->isSelf) {
                    variableName = "Self";
                } else {
                    variableName = lastSegment->identifier;
                }
                
                rightReg = irBuilder->getVariableRegister(variableName);
            } else {
                // 其他情况，生成表达式
                rightReg = generateExpression(assignExpr->rightexpression);
            }
        } else {
            // 非聚合类型，正常生成表达式
            rightReg = generateExpression(assignExpr->rightexpression);
        }
        
        if (rightReg.empty()) {
            reportError("Failed to generate right side of assignment");
            return "";
        }
        
        // 检查类型兼容性并进行必要的转换
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
        
        // 检查是否为聚合类型，如果是则使用 builtin_memcpy
        if (irBuilder->isAggregateType(leftType)) {
            irBuilder->emitAggregateCopy(leftPtrReg, rightReg, leftType);
        } else {
            irBuilder->emitStore(rightReg, leftPtrReg);
        }
        
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
                
                // 对于复合赋值，我们需要判断是否应该使用无符号运算
                // 创建一个临时的二元表达式来重用 shouldUseUnsignedOperation 方法
                auto tempBinaryExpr = std::make_shared<BinaryExpression>(compoundAssignExpr->leftexpression, compoundAssignExpr->rightexpression, opType);
                bool isUnsigned = shouldUseUnsignedOperation(tempBinaryExpr);
                
                resultReg = generateArithmeticOperation(currentReg, rightReg, opType, leftType, isUnsigned);
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
        
        // 检查是否为聚合类型，如果是则使用 memcpy
        if (irBuilder->isAggregateType(leftType)) {
            irBuilder->emitAggregateCopy(leftPtrReg, resultReg, leftType);
        } else {
            irBuilder->emitStore(resultReg, leftPtrReg);
        }
        
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
        std::string sourceType = irBuilder->getRegisterType(sourceReg);
        if (sourceType.empty()) {
            sourceType = getExpressionType(typeCastExpr->expression);
        }
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
    
    // 简化实现：路径表达式、字段访问、索引表达式、解引用表达式可以是左值
    return (std::dynamic_pointer_cast<PathExpression>(expression) != nullptr) ||
           (std::dynamic_pointer_cast<FieldExpression>(expression) != nullptr) ||
           (std::dynamic_pointer_cast<IndexExpression>(expression) != nullptr) ||
           (std::dynamic_pointer_cast<DereferenceExpression>(expression) != nullptr);
}

std::pair<std::string, std::string> ExpressionGenerator::analyzeLValue(std::shared_ptr<Expression> expression) {
    if (!expression) {
        return {"", ""};
    }
    
    try {
        if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(expression)) {
            // 变量访问
            auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
            std::string variableName;
            
            // 特殊处理 self 和 Self
            if (lastSegment->isself) {
                variableName = "self";
            } else if (lastSegment->isSelf) {
                variableName = "Self";
            } else {
                variableName = lastSegment->identifier;
            }
            
            std::string varReg = irBuilder->getVariableRegister(variableName);
            std::string varType = getExpressionType(expression);
            return {varReg, varType};
        }
        else if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(expression)) {
            std::string receiverType = getExpressionType(fieldExpr->expression);
            std::string receiverReg;
            if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(fieldExpr->expression)) {
                // 对于变量访问，获取变量的指针寄存器
                auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                std::string variableName;
                
                // 特殊处理 self 和 Self
                if (lastSegment->isself) {
                    variableName = "self";
                } else if (lastSegment->isSelf) {
                    variableName = "Self";
                } else {
                    variableName = lastSegment->identifier;
                }
                
                receiverReg = irBuilder->getVariableRegister(variableName);
                receiverType = irBuilder->getRegisterType(receiverReg);
            } else {
                receiverReg = generateExpression(fieldExpr->expression);
            }
            
            std::string fieldPtrReg = generateFieldAccessAddress(receiverReg, fieldExpr->identifier, receiverType);
            std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
            if (fieldType.empty()) {
                fieldType = "i32"; // 默认类型
            }
            return {fieldPtrReg, fieldType};
        }
        else if (auto derefExpr = std::dynamic_pointer_cast<DereferenceExpression>(expression)) {
            // 解引用表达式作为左值：*ptr = value
            // 生成指针表达式
            std::string pointerReg = generateExpression(derefExpr->expression);
            if (pointerReg.empty()) {
                reportError("Failed to generate pointer for dereference lvalue");
                return {"", ""};
            }
            
            // 获取指针类型
            std::string pointerType = irBuilder->getRegisterType(pointerReg);

            if (pointerType.empty()) {
                pointerType = getExpressionType(derefExpr->expression);
            }
            
            // 确保是指针类型
            if (!typeMapper->isPointerType(pointerType)) {
                reportError("Cannot dereference non-pointer type: " + pointerType);
                return {"", ""};
            }
            
            // 获取指向的类型
            std::string elementType = typeMapper->getPointedType(pointerType);
            
            // 对于解引用表达式作为左值，我们返回指针本身和指向的类型
            // 这样在赋值时可以直接存储到该地址
            return {pointerReg, elementType};
        }
        else if (auto indexExpr = std::dynamic_pointer_cast<IndexExpression>(expression)) {
            // 索引访问 - 修复复杂字段访问作为左值的情况
            std::string baseType = getExpressionType(indexExpr->expressionout);
            std::string indexReg = generateExpression(indexExpr->expressionin);
            // std::cerr << "indexReg: " << indexReg << "\n";
            if (baseType.size() && baseType.back() == '*') {
                baseType.pop_back();
            }
            
            std::string elementType;
            std::string elementPtrReg;
            
            if (typeMapper->isPointerType(baseType)) {
                elementType = typeMapper->getPointedType(baseType);
                std::string baseReg = generateExpression(indexExpr->expressionout);
                std::vector<std::string> indices = {indexReg};
                elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
                return {elementPtrReg, elementType};
            } else if (typeMapper->isArrayType(baseType)) {
                elementType = typeMapper->getArrayElementType(baseType);
                
                // 检查基础表达式是否是字段访问表达式（如 self.data）
                if (auto fieldExpr = std::dynamic_pointer_cast<FieldExpression>(indexExpr->expressionout)) {
                    // 处理字段访问中的数组索引，如 self.data[self.top]
                    std::string receiverType = getExpressionType(fieldExpr->expression);
                    std::string receiverPtrReg;
                    
                    // 检查接收者是否为路径表达式（变量访问）
                    if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(fieldExpr->expression)) {
                        // 对于变量访问，获取变量的指针寄存器
                        auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                        std::string variableName;
                        
                        // 特殊处理 self 和 Self
                        if (lastSegment->isself) {
                            variableName = "self";
                        } else if (lastSegment->isSelf) {
                            variableName = "Self";
                        } else {
                            variableName = lastSegment->identifier;
                        }
                        
                        receiverPtrReg = irBuilder->getVariableRegister(variableName);
                        // receiverType = irBuilder->getRegisterType(receiverPtrReg);
                    } else {
                        // 对于其他类型的表达式，生成表达式然后获取其地址
                        std::string receiverReg = generateExpression(fieldExpr->expression);
                        if (receiverReg.empty()) {
                            reportError("Failed to generate receiver expression for field access");
                            return {"", ""};
                        }
                        
                        // 如果接收者不是指针，需要分配内存并存储
                        std::string receiverRegType = irBuilder->getRegisterType(receiverReg);
                        if (!irBuilder->isPointerType(receiverRegType)) {
                            receiverPtrReg = irBuilder->emitAlloca(receiverType);
                            irBuilder->emitStore(receiverReg, receiverPtrReg);
                        } else {
                            receiverPtrReg = receiverReg;
                        }
                    }
                    
                    // 计算字段地址（这里是数组字段的地址）
                    std::string fieldPtrReg = generateFieldAccessAddress(receiverPtrReg, fieldExpr->identifier, receiverType);
                    if (fieldPtrReg.empty()) {
                        reportError("Failed to calculate field address: " + fieldExpr->identifier);
                        return {"", ""};
                    }
                    
                    // 现在fieldPtrReg是指向数组字段的指针，可以直接进行索引
                    // 对于指向数组的指针 [N x T]*，我们需要两个索引：0（数组起始）和元素索引
                    std::vector<std::string> indices = {"0", indexReg};
                    
                    // 关键修复：始终使用字段类型作为数组类型，而不是依赖fieldPtrReg的类型
                    // 因为fieldPtrReg的类型可能不正确（特别是在指针参数的情况下）
                    std::string fieldType = getStructFieldType(receiverType, fieldExpr->identifier);
                    std::string arrayType = fieldType;
                    
                    // 确保arrayType是数组类型
                    if (!typeMapper->isArrayType(arrayType)) {
                        reportError("Field is not an array type: " + fieldExpr->identifier + " -> " + arrayType);
                        return {"", ""};
                    }
                    
                    elementPtrReg = irBuilder->emitGetElementPtr(fieldPtrReg, indices, arrayType);
                    
                    return {elementPtrReg, elementType};
                }
                // 检查基础表达式是否是路径表达式（变量访问）
                else if (auto pathExpr = std::dynamic_pointer_cast<PathExpression>(indexExpr->expressionout)) {
                    // 对于数组变量，直接从变量对应的指针读取元素
                    auto lastSegment = pathExpr->simplepath->simplepathsegements.back();
                    std::string variableName;
                    
                    // 特殊处理 self 和 Self
                    if (lastSegment->isself) {
                        variableName = "self";
                    } else if (lastSegment->isSelf) {
                        variableName = "Self";
                    } else {
                        variableName = lastSegment->identifier;
                    }
                    
                    std::string varReg = irBuilder->getVariableRegister(variableName);
                    if (varReg.empty()) {
                        reportError("Variable register not found: " + variableName);
                        return {"", ""};
                    }
                    
                    // 使用变量指针进行索引
                    std::vector<std::string> indices = {"0", indexReg};
                    // 对于数组索引，resultType 应该是数组类型，而不是元素指针类型
                    std::string arraySize = typeMapper->getArraySize(baseType);
                    // 去除可能的空格
                    arraySize.erase(0, arraySize.find_first_not_of(" \t\n\r"));
                    arraySize.erase(arraySize.find_last_not_of(" \t\n\r") + 1);
                    std::string arrayType = "[" + arraySize + " x " + elementType + "]";
                    elementPtrReg = irBuilder->emitGetElementPtr(varReg, indices, arrayType);
                } else {
                    // 生成基础表达式
                    std::string baseReg = generateExpression(indexExpr->expressionout);
                    std::vector<std::string> indices = {"0", indexReg};
                    elementPtrReg = irBuilder->emitGetElementPtr(baseReg, indices, elementType + "*");
                }
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
        
        // 确保basePtr是指针类型
        std::string basePtrType = irBuilder->getRegisterType(basePtr);
        if (!irBuilder->isPointerType(basePtrType)) {
            reportError("Base pointer is not a pointer type: " + basePtrType);
            return "";
        }
        
        // 生成 getelementptr 指令
        // 对于结构体字段访问，第一个索引是0（指向结构体本身），第二个索引是字段索引
        std::vector<std::string> indices = {"0", std::to_string(fieldIndex)};
        
        // 获取指向的元素类型（去掉*）
        std::string pointedType;
        if (basePtrType.back() == '*') {
            pointedType = basePtrType.substr(0, basePtrType.length() - 1);
        } else {
            pointedType = basePtrType; // 已经是正确的类型
        }
        
        // 关键修复：对于字段访问，getelementptr的第一个参数类型应该是结构体类型
        // 而不是字段类型，因为我们在访问结构体的字段
        std::string fieldPtrReg = irBuilder->emitGetElementPtr(basePtr, indices, pointedType);
        
        // 重要：设置正确的寄存器类型
        // fieldPtrReg 应该是指向字段类型的指针
        // 如果字段是数组类型，则 fieldPtrReg 的类型应该是 [N x T]*
        // 如果字段是基本类型，则 fieldPtrReg 的类型应该是 T*
        std::string fieldType = getStructFieldType(structType, fieldName);
        if (!fieldType.empty()) {
            irBuilder->setRegisterType(fieldPtrReg, fieldType + "*");
        }
        
        return fieldPtrReg;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFieldAccessAddress: " + std::string(e.what()));
        return "";
    }
}

int ExpressionGenerator::getStructFieldIndex(const std::string& structName, const std::string& fieldName) {
    try {
        // 从结构体类型名称中提取纯结构体名
        std::string pureStructName = structName;
        if (structName.find("%struct_") == 0) {
            pureStructName = structName.substr(8); // 去掉 "%struct_" 前缀
        }

        while (!pureStructName.empty() && pureStructName.ends_with("*")) {
            pureStructName.pop_back();
        }
        
        // 从符号表查找结构体定义
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            return -1;
        }
        
        auto symbol = currentScope->Lookup(pureStructName);
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
        if (argReg.empty()) {
            continue;
        }
        
        // 检查参数类型
        std::string argType = getExpressionType(expr);
        auto actualType = typeMapper->isPointerType(argType) ? typeMapper->getPointedType(argType) : argType;
        
        // 如果是聚合类型，需要特殊处理
        if (irBuilder->isAggregateType(actualType) && !typeMapper->isPointerType(argType)) {
            // 为聚合类型参数分配临时空间
            std::string tempReg = irBuilder->emitAlloca(argType);
            
            // 使用 memcpy 将参数值复制到临时空间
            irBuilder->emitAggregateCopy(tempReg, argReg, argType);
            
            // 传递指针而不是值
            args.push_back(tempReg);
        } else {
            // 非聚合类型，直接传递值
            args.push_back(argReg);
        }
    }
    
    return args;
}

// ==================== 新增的用户定义函数调用处理 ====================

std::string ExpressionGenerator::generateUserDefinedFunctionCall(const std::string& functionName,
                                                          const std::vector<std::string>& args,
                                                          std::shared_ptr<CallExpression> callExpr) {
    try {
        // 获取函数返回类型
        std::string returnType = getFunctionReturnType(functionName);
        if (returnType.empty()) {
            // 如果找不到函数信息，尝试从类型映射中获取
            auto it = nodeTypeMap.find(callExpr.get());
            if (it != nodeTypeMap.end() && it->second) {
                returnType = typeMapper->mapSemanticTypeToLLVM(it->second);
                // 对于结构体类型，函数应该返回指针类型
                if (typeMapper->isStructType(returnType)) {
                    returnType = returnType + "*";
                }
            } else {
                returnType = "i32"; // 默认返回类型
            }
        }
        
        // 如果返回类型是void，直接调用
        if (returnType == "void") {
            irBuilder->emitCall(functionName, args, "void");
            return ""; // void函数调用不返回值
        }
        
        // 分配返回槽空间 - 关键修复：返回槽类型应该始终是 T*
        // 根据sret约定，返回槽参数的类型应该是T*，其中T是函数返回的基础类型
        std::string returnSlotPtr;
        
        // 确定返回槽的基础类型
        std::string baseType = returnType;
        
        // 如果返回类型已经是指针类型（T*），我们需要提取T
        if (baseType.find("*") != std::string::npos) {
            baseType = baseType.substr(0, baseType.length() - 1); // 去掉 *
        }
        
        // 返回槽的类型应该是baseType*，即指向基础类型的指针
        returnSlotPtr = irBuilder->emitAlloca(baseType);
        irBuilder->setRegisterType(returnSlotPtr, baseType + "*");
        
        // 构建参数列表：返回槽指针 + 原始参数
        std::vector<std::string> finalArgs;
        finalArgs.push_back(returnSlotPtr); // 第一个参数是返回槽指针
        finalArgs.insert(finalArgs.end(), args.begin(), args.end());
        
        // 调用用户定义函数（返回void）
        irBuilder->emitCall(functionName, finalArgs, "void");
        
        // 处理函数调用后的返回值
        if (irBuilder->isAggregateType(baseType)) {
            // 对于聚合类型，函数已经将结果写入返回槽，返回槽指针就是结果
            // 返回槽指针的类型是 baseType*，这正是我们需要的
            return returnSlotPtr;
        } else if (returnType.find("*") != std::string::npos) {
            // 对于指针类型返回值（如 T*），从返回槽加载指针值
            std::string resultReg = irBuilder->emitLoad(returnSlotPtr, returnType);
            irBuilder->setRegisterType(resultReg, returnType);
            return resultReg;
        } else {
            // 对于基本类型返回值，从返回槽加载结果
            std::string resultReg = irBuilder->emitLoad(returnSlotPtr, returnType);
            irBuilder->setRegisterType(resultReg, returnType);
            return resultReg;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateUserDefinedFunctionCall: " + std::string(e.what()));
        return "";
    }
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
            if (fromType == "i1" && toType == "i32") {
                // bool 到整数扩展（32位机器）
                instruction = resultReg + " = zext " + fromType + " " + valueReg + " to " + toType;
            } else if (fromType == "i32" && toType == "i1") {
                // 整数到 bool 截断（32位机器）
                instruction = resultReg + " = trunc " + fromType + " " + valueReg + " to " + toType;
            } else if (fromType == "i32" && toType == "i32") {
                // i32 到 i32（无转换）
                return valueReg;
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
                                                           Token opType, const std::string& resultType, bool isUnsigned) {
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
                // 根据操作数类型选择有符号或无符号除法
                if (isUnsigned) {
                    instruction = resultReg + " = udiv " + resultType + " " + left + ", " + right;
                } else {
                    instruction = resultReg + " = sdiv " + resultType + " " + left + ", " + right;
                }
                break;
            case Token::kPercent:
                // 根据操作数类型选择有符号或无符号取模
                if (isUnsigned) {
                    instruction = resultReg + " = urem " + resultType + " " + left + ", " + right;
                } else {
                    instruction = resultReg + " = srem " + resultType + " " + left + ", " + right;
                }
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
                                                          Token opType, const std::string& resultType, bool isUnsigned) {
    try {
        std::string resultReg = irBuilder->newRegister();
        
        std::string condition = getComparisonCondition(opType, isUnsigned);
        
        // 对于比较操作，操作数类型应该是操作数的实际类型，而不是结果类型
        // 比较操作的结果总是 i1，但操作数类型应该是 i32（整数比较）
        std::string operandType = irBuilder->getRegisterType(left);
        std::string instruction = resultReg + " = icmp " + condition + " " + operandType + " " + left + ", " + right;
        
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
            case Token::kAndAnd:
                instruction = resultReg + " = and " + resultType + " " + left + ", " + right;
                break;
            case Token::kOrOr:
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

std::string ExpressionGenerator::getComparisonCondition(Token token, bool isUnsigned) {
    switch (token) {
        case Token::kEqEq: return "eq";
        case Token::kNe: return "ne";
        case Token::kLt: return isUnsigned ? "ult" : "slt";
        case Token::kLe: return isUnsigned ? "ule" : "sle";
        case Token::kGt: return isUnsigned ? "ugt" : "sgt";
        case Token::kGe: return isUnsigned ? "uge" : "sge";
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
            sizeStr = std::to_string(typeChecker->EvaluateArraySize(*(arrayType->expression.get())));
        }
        
        // 生成正确的 LLVM 数组类型格式：[N x T]
        return "[" + sizeStr + " x " + elementType + "]";
    } else if (auto refType = std::dynamic_pointer_cast<ReferenceType>(type)) {
        std::string targetType = typeToStringHelper(refType->type);
        return (refType->ismut ? "&mut " : "&") + targetType;
    } else if (auto unitType = std::dynamic_pointer_cast<UnitType>(type)) {
        return "()";
    }
    
    return "i32"; // 默认类型
}

// ==================== 分组表达式实现 ====================

std::string ExpressionGenerator::generateGroupedExpression(std::shared_ptr<GroupedExpression> groupedExpr) {
    if (!groupedExpr || !groupedExpr->expression) {
        reportError("GroupedExpression or its expression is null");
        return "";
    }
    
    try {
        // 分组表达式的值就是内部表达式的值
        // 直接生成内部表达式并返回其结果
        return generateExpression(groupedExpr->expression);
    }
    catch (const std::exception& e) {
        reportError("Exception in generateGroupedExpression: " + std::string(e.what()));
        return "";
    }
}

// ==================== 块表达式和控制流表达式实现 ====================

std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> blockExpr) {
    if (!blockExpr) {
        reportError("BlockExpression is null");
        return "";
    }
    
    try {
        // 创建新的作用域
        scopeTree->EnterExistingScope(blockExpr.get(), 0);
        
        // 生成块中的所有语句
        if (!generateBlockStatements(blockExpr->statements)) {
            reportError("Failed to generate block statements");
            scopeTree->ExitScope();
            return "";
        }
        
        // 如果有尾表达式，生成它
        if (blockExpr->expressionwithoutblock) {
            std::string result = generateExpression(blockExpr->expressionwithoutblock);
            // 退出作用域
            scopeTree->ExitScope();
            return result;
        } else if (blockExpr->statements.size() && dynamic_cast<ExpressionStatement*>(blockExpr->statements.back()->astnode.get())) {
            scopeTree->ExitScope();
            return statementGenerator->getStatementRegname(dynamic_cast<ExpressionStatement*>(blockExpr->statements.back()->astnode.get()));
        } else {
            // 没有尾表达式，需要根据块表达式的类型决定是否生成值
            // 退出作用域
            scopeTree->ExitScope();
            
            // 获取块表达式的类型
            auto blockTypeIt = nodeTypeMap.find(blockExpr.get());
            if (blockTypeIt != nodeTypeMap.end()) {
                std::shared_ptr<SemanticType> blockType = blockTypeIt->second;
                if (blockType) {
                    std::string typeStr = blockType->tostring();
                    // 只有当类型不是 unit 或 never 时才需要生成值
                    if (typeStr != "()" && typeStr != "!") {
                        return generateUnitValue();
                    }
                }
            }
            
            // 默认情况下，不需要生成值（返回空字符串）
            return "";
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
        
        // 获取 if 表达式的类型
        auto ifTypeIt = nodeTypeMap.find(ifExpr.get());
        std::shared_ptr<SemanticType> ifType = nullptr;
        if (ifTypeIt != nodeTypeMap.end()) {
            ifType = ifTypeIt->second;
        }
        
        // 检查 if 表达式是否需要生成值（非 unit 或 never 类型）
        bool needsValue = false;
        std::string ifTypeStr;
        if (ifType) {
            ifTypeStr = ifType->tostring();
            needsValue = (ifTypeStr != "()" && ifTypeStr != "!");
        }
        
        // 声明一个值来存储 if 表达式的结果（如果需要值）
        std::string resultPtr;
        std::string resultType;
        if (needsValue) {
            resultType = getExpressionType(ifExpr);
            // 分配内存来存储 if 表达式的结果
            resultPtr = irBuilder->emitAlloca(resultType);
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
        if (needsValue && !thenResult.empty()) {
            // 检查 then 结果是否为 unit 类型，如果是则跳过赋值
            auto thenTypeIt = nodeTypeMap.find(ifExpr->ifblockexpression.get());
            if (thenTypeIt != nodeTypeMap.end()) {
                std::string thenTypeStr = thenTypeIt->second->tostring();
                if (thenTypeStr != "()") {
                    // 检查结果类型是否为聚合类型，如果是则使用 builtin_memcpy
                    if (irBuilder->isAggregateType(resultType)) {
                        irBuilder->emitAggregateCopy(resultPtr, thenResult, resultType);
                    } else {
                        // 将 then 分支的结果存储到分配的内存中
                        irBuilder->emitStore(thenResult, resultPtr);
                    }
                }
            }
        }
        irBuilder->emitBr(endBB);
        
        // 生成 else 分支（如果存在）
        if (ifExpr->elseexpression) {
            irBuilder->emitLabel(elseBB);
            std::string elseResult = generateExpression(ifExpr->elseexpression);
            if (needsValue && !elseResult.empty()) {
                // 检查 else 结果是否为 unit 类型，如果是则跳过赋值
                auto elseTypeIt = nodeTypeMap.find(ifExpr->elseexpression.get());
                if (elseTypeIt != nodeTypeMap.end()) {
                    std::string elseTypeStr = elseTypeIt->second->tostring();
                    if (elseTypeStr != "()") {
                        // 检查结果类型是否为聚合类型，如果是则使用 builtin_memcpy
                        if (irBuilder->isAggregateType(resultType)) {
                            irBuilder->emitAggregateCopy(resultPtr, elseResult, resultType);
                        } else {
                            // 将 else 分支的结果存储到分配的内存中
                            irBuilder->emitStore(elseResult, resultPtr);
                        }
                    }
                }
            }
            irBuilder->emitBr(endBB);
        } else {
            irBuilder->emitLabel(elseBB);
            if (needsValue) {
                std::string elseResult = generateUnitValue();
                // 对于没有 else 分支的情况，如果需要值，则赋 unit 值
                // 但根据类型检查，这种情况应该已经在 typecheck 阶段报错了
                // 检查结果类型是否为聚合类型，如果是则使用 builtin_memcpy
                if (irBuilder->isAggregateType(resultType)) {
                    irBuilder->emitAggregateCopy(resultPtr, elseResult, resultType);
                } else {
                    irBuilder->emitStore(elseResult, resultPtr);
                }
            }
            irBuilder->emitBr(endBB);
        }
        
        // 生成合并点
        irBuilder->emitLabel(endBB);
        
        // 如果需要值，从内存中加载结果并返回
        if (needsValue) {
            if (irBuilder->isAggregateType(resultType)) {
                return resultPtr;
            }
            return irBuilder->emitLoad(resultPtr, resultType);
        } else {
            // 如果不需要值，返回空字符串
            return "";
        }
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
        // 获取 loop 表达式的类型
        auto loopTypeIt = nodeTypeMap.find(loopExpr.get());
        std::shared_ptr<SemanticType> loopType = nullptr;
        if (loopTypeIt != nodeTypeMap.end()) {
            loopType = loopTypeIt->second;
        }
        
        // 检查 loop 表达式是否需要生成值（非 unit 或 never 类型）
        bool needsValue = false;
        std::string loopTypeStr;
        std::string resultType;
        if (loopType) {
            loopTypeStr = loopType->tostring();
            needsValue = (loopTypeStr != "()" && loopTypeStr != "!");
        }
        
        // 声明一个值来存储 loop 表达式的结果（如果需要值）
        std::string resultPtr;
        if (needsValue) {
            resultType = getExpressionType(loopExpr);
            // 分配内存来存储 loop 表达式的结果
            resultPtr = irBuilder->emitAlloca(resultType);
        }
        
        // 创建循环基本块
        std::string startBB = irBuilder->newBasicBlock("loop.start");
        std::string bodyBB = irBuilder->newBasicBlock("loop.body");
        std::string endBB = irBuilder->newBasicBlock("loop.end");
        
        // 进入循环上下文
        enterLoopContext(startBB, endBB, bodyBB);
        
        // 设置循环上下文的结果存储信息
        if (!loopContextStack.empty()) {
            loopContextStack.top().resultPtr = resultPtr;
            loopContextStack.top().resultType = resultType;
        }
        
        // 初始化循环结果存储位置（如果需要值）
        if (needsValue && !resultPtr.empty()) {
            std::string initValue = generateUnitValue();
            irBuilder->emitStore(initValue, resultPtr);
        }
        
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
        
        // 如果需要值，从内存中加载结果并返回
        if (needsValue && !resultPtr.empty()) {
            return irBuilder->emitLoad(resultPtr, resultType);
        } else {
            // 如果不需要值，返回空字符串
            return "";
        }
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
            std::string breakType = getExpressionType(breakExpr->expression);
            
            // 如果循环需要值且有结果存储指针，存储 break 值
            if (!loopCtx.resultPtr.empty()) {
                // 检查结果类型是否为聚合类型，如果是则使用 builtin_memcpy
                if (irBuilder->isAggregateType(breakType)) {
                    irBuilder->emitAggregateCopy(loopCtx.resultPtr, breakValue, breakType);
                } else {
                    irBuilder->emitStore(breakValue, loopCtx.resultPtr);
                }
            }
            
            setBreakValue(breakValue, breakType);
        } else {
            // 如果没有 break 值表达式，但有结果存储指针，存储 unit 值
            if (!loopCtx.resultPtr.empty()) {
                std::string unitValue = generateUnitValue();
                // 检查结果类型是否为聚合类型，如果是则使用 builtin_memcpy
                if (loopCtx.resultType.empty() || !irBuilder->isAggregateType(loopCtx.resultType)) {
                    irBuilder->emitStore(unitValue, loopCtx.resultPtr);
                } else {
                    irBuilder->emitAggregateCopy(loopCtx.resultPtr, unitValue, loopCtx.resultType);
                }
                setBreakValue(unitValue, "i32");
            } else {
                setBreakValue(generateUnitValue(), "i32");
            }
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
        
        // 检查是否为 Self 类型
        std::string returnTypeStr = functionSymbol->returntype->tostring();
        if (returnTypeStr == "Self") {
            // 从函数名中提取 impl 的目标类型
            size_t underscorePos = functionName.find('_');
            if (underscorePos != std::string::npos) {
                std::string structName = functionName.substr(0, underscorePos);
                // 对于 Self 类型，函数应该返回结构体指针类型
                return "%struct_" + structName + "*";
            }
            return "%struct_*"; // 默认类型
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
        // 从结构体类型名称中提取纯结构体名
        std::string pureStructName = structName;
        if (structName.find("%struct_") == 0) {
            pureStructName = structName.substr(8); // 去掉 "%struct_" 前缀
        }
        
        // 关键修复：处理指针类型，去掉末尾的 *
        while (!pureStructName.empty() && pureStructName.back() == '*') {
            pureStructName.pop_back();
        }
        
        // 从符号表查找结构体定义
        auto currentScope = scopeTree->GetCurrentScope();
        if (!currentScope) {
            return "";
        }
        
        auto symbol = currentScope->Lookup(pureStructName);
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

std::string ExpressionGenerator::getNodeTypeLLVM(std::shared_ptr<ASTNode> node) {
    if (!node) {
        return "i32"; // 默认类型
    }
    
    auto it = nodeTypeMap.find(node.get());
    if (it != nodeTypeMap.end()) {
        return typeMapper->mapSemanticTypeToLLVM(it->second);
    }
    
    return "i32"; // 默认类型
}

bool ExpressionGenerator::shouldUseUnsignedOperation(std::shared_ptr<BinaryExpression> binaryExpr) {
    try {
        if (!binaryExpr || !binaryExpr->leftexpression || !binaryExpr->rightexpression) {
            return false;
        }
        
        // 从 nodeTypeMap 获取左右操作数的语义类型
        auto leftTypeIt = nodeTypeMap.find(binaryExpr->leftexpression.get());
        auto rightTypeIt = nodeTypeMap.find(binaryExpr->rightexpression.get());
        
        if (leftTypeIt == nodeTypeMap.end() || rightTypeIt == nodeTypeMap.end()) {
            return false; // 如果找不到类型信息，默认使用有符号运算
        }
        
        std::shared_ptr<SemanticType> leftSemanticType = leftTypeIt->second;
        std::shared_ptr<SemanticType> rightSemanticType = rightTypeIt->second;
        
        if (!leftSemanticType || !rightSemanticType) {
            return false;
        }
        
        // 使用 TypeMapper 检查是否为无符号整数类型
        bool leftIsUnsigned = typeMapper->isUnsignedIntegerType(leftSemanticType);
        bool rightIsUnsigned = typeMapper->isUnsignedIntegerType(rightSemanticType);
        
        // 如果任一操作数是无符号整数，则使用无符号运算
        return leftIsUnsigned || rightIsUnsigned;
    }
    catch (const std::exception& e) {
        reportError("Exception in shouldUseUnsignedOperation: " + std::string(e.what()));
        return false; // 出错时默认使用有符号运算
    }
}


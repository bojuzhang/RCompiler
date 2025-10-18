#include "typecheck.hpp"
#include "lexer.hpp"
#include "typewrapper.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <memory>
#include <utility>
#include <unordered_map>

TypeChecker::TypeChecker(std::shared_ptr<ScopeTree> scopeTree, std::shared_ptr<ConstantEvaluator> constantEvaluator)
    : scopeTree(scopeTree), constantEvaluator(constantEvaluator) {}

bool TypeChecker::checkTypes() {
    // 默认检查在 visit 中实现
    hasErrors = false;
    return !hasErrors;
}

bool TypeChecker::hasTypeErrors() const {
    return hasErrors;
}

void TypeChecker::visit(Crate& node) {
    pushNode(node);
        
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }

    for (const auto& [traitName, implementations] : traitImplementations) {
        auto requirementsIt = traitRequirements.find(traitName);
        if (requirementsIt != traitRequirements.end()) {
            const auto& requirements = requirementsIt->second;
            for (const auto& requirement : requirements) {
                if (implementations.find(requirement) == implementations.end()) {
                    reportMissingTraitImplementation(traitName, requirement);
                }
            }
        }
    }
    
    popNode();
}

void TypeChecker::visit(Item& node) {
    pushNode(node);
    if (node.item) {
        node.item->accept(*this);
    }
    popNode();
}


void TypeChecker::visit(Statement& node) {
    pushNode(node);
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    popNode();
}

void TypeChecker::visit(Function& node) {
    pushNode(node);
    
    // 检查函数签名
    // checkFunctionSignature(node);
    
    // 不进入函数作用域，保持在全局作用域中进行类型检查
    // 这样可以确保能够找到全局作用域中的函数符号
    if (node.functionparameters) {
        checkFunctionParameters(*node.functionparameters);
    }
    
    // 检查返回类型
    if (node.functionreturntype) {
        checkFunctionReturnType(*node.functionreturntype);
    }
    
    // 检查函数体
    checkFunctionBody(node);
    
    popNode();
}

void TypeChecker::visit(ConstantItem& node) {
    pushNode(node);
    
    auto type = checkType(*node.type);
    if (!type) {
        reportError("Invalid type in constant declaration");
        popNode();
        return;
    }
    
    // // 检查常量表达式类型兼容性
    // auto exprType = inferExpressionType(*node.expression);
    // if (exprType && !areTypesCompatible(type, exprType)) {
    //     reportError("Type mismatch in constant declaration");
    // }
    
    popNode();
}

void TypeChecker::visit(StructStruct& node) {
    pushNode(node);
    
    std::string previousStruct = currentStruct;
    enterStructContext(node.identifier);
    
    scopeTree->enterScope(Scope::ScopeType::Struct, &node);
    checkStructFields(node);
    scopeTree->exitScope();
    
    exitStructContext();
    currentStruct = previousStruct;
    
    popNode();
}

void TypeChecker::visit(Enumeration& node) {
    pushNode(node);
    
    auto enumName = node.identifier;
    if (!typeExists(enumName)) {
        reportUndefinedType(enumName, &node);
    }
    
    // 检查variants（如果有类型信息）
    if (node.enumvariants) {
        for (const auto& variant : node.enumvariants->enumvariants) {
            // 这里简化处理，实际需要根据enum variant的具体类型检查
        }
    }
    
    popNode();
}

void TypeChecker::visit(InherentImpl& node) {
    pushNode(node);
    
    std::string previousImpl = currentImpl;
    enterImplContext("impl_" + std::to_string(reinterpret_cast<uintptr_t>(&node)));
    
    scopeTree->enterScope(Scope::ScopeType::Impl, &node);
    
    // 检查impl目标类型
    auto targetType = getImplTargetType(node);
    if (!targetType) {
        reportError("Invalid target type in impl block");
    }
    
    // 检查是固有实现还是trait实现
    std::string traitName = getTraitNameFromImpl(node);
    if (!traitName.empty()) {
        checkTraitImpl(node);
    } else {
        checkInherentImpl(node);
    }
    
    // 检查关联项
    for (const auto& item : node.associateditems) {
        if (item) {
            checkAssociatedItem(*item);
        }
    }
    
    scopeTree->exitScope();
    
    exitImplContext();
    currentImpl = previousImpl;
    
    popNode();
}

void TypeChecker::checkStructFields(StructStruct& node) {
    if (!node.structfileds) return;
    
    for (const auto& field : node.structfileds->structfields) {
        checkStructFieldType(*field);
    }
}

void TypeChecker::checkStructFieldType(StructField& field) {
    auto fieldType = checkType(*field.type);
    if (!fieldType) {
        reportError("Invalid type in struct field: " + field.identifier);
    }
    
    auto fieldSymbol = std::make_shared<Symbol>(
        field.identifier,
        SymbolKind::Variable,
        fieldType,
        false,
        &field
    );
    
    scopeTree->insertSymbol(field.identifier, fieldSymbol);
}

void TypeChecker::checkInherentImpl(InherentImpl& node) {
    auto targetType = getImplTargetType(node);
    if (!targetType) {
        reportError("Cannot determine target type for impl block");
        return;
    }
    
    // 检查目标类型是否存在
    auto targetTypeName = targetType->tostring();
    if (!typeExists(targetTypeName)) {
        reportUndefinedType(targetTypeName, &node);
        return;
    }
    
    // 记录固有实现的方法
    for (const auto& item : node.associateditems) {
        if (auto function = dynamic_cast<Function*>(item.get())) {
            std::string methodName = function->identifier_name;
            structMethods[targetTypeName].insert(methodName);
        }
    }
}

void TypeChecker::checkTraitImpl(InherentImpl& node) {
    std::string traitName = getTraitNameFromImpl(node);
    if (traitName.empty()) {
        reportError("Invalid trait implementation");
        return;
    }
    
    if (!typeExists(traitName)) {
        reportUndefinedType(traitName, &node);
        return;
    }
    
    // 记录impl到trait的映射
    implToTraitMap[currentImpl] = traitName;
    
    // 收集trait的要求
    collectTraitRequirements(traitName);
    
    // 初始化这个trait的实现集合
    traitImplementations[traitName] = std::unordered_set<std::string>();
}

std::shared_ptr<SemanticType> TypeChecker::getImplTargetType(InherentImpl& node) {
    // 从impl节点中提取目标类型
    // 简化实现：返回一个简单类型
    return std::make_shared<SimpleType>("UnknownType");
}

std::string TypeChecker::getTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
    // 简化实现：返回空字符串表示固有实现
    return "";
}

void TypeChecker::checkTraitImplementation(InherentImpl& node, const std::string& traitName) {
    // 收集trait的要求
    collectTraitRequirements(traitName);
    // 检查实现是否满足trait要求
    checkTraitRequirementsSatisfied(traitName, currentImpl);
}

void TypeChecker::collectTraitRequirements(const std::string& traitName) {
    auto traitSymbol = findTrait(traitName);
    if (!traitSymbol) return;
    
    // 清空当前要求
    currentTraitRequirements.clear();
    // 收集trait的所有关联项要求
    for (const auto& item : traitSymbol->associatedItems) {
        currentTraitRequirements.insert(item->name);
    }
    // 缓存trait要求
    traitRequirements[traitName] = currentTraitRequirements;
}

void TypeChecker::checkTraitRequirementsSatisfied(const std::string& traitName, const std::string& implName) {
    const auto& requirements = traitRequirements[traitName];
    auto& implementations = traitImplementations[implToTraitMap[implName]];
    // 检查每个要求是否都有实现
    for (const auto& requirement : requirements) {
        if (implementations.find(requirement) == implementations.end()) {
            reportMissingTraitImplementation(traitName, requirement);
        }
    }
}


void TypeChecker::checkAssociatedItem(AssociatedItem& item) {
    if (!item.consttantitem_or_function) return;
    
    std::string itemName;
    if (auto function = dynamic_cast<Function*>(item.consttantitem_or_function.get())) {
        checkAssociatedFunction(*function);
        itemName = function->identifier_name;
    } else if (auto constant = dynamic_cast<ConstantItem*>(item.consttantitem_or_function.get())) {
        checkAssociatedConstant(*constant);
        itemName = constant->identifier;
    }
    
    // 记录实现项
    if (!currentImpl.empty() && !itemName.empty()) {
        // 查找这个impl对应的trait
        auto it = implToTraitMap.find(currentImpl);
        if (it != implToTraitMap.end()) {
            const std::string& traitName = it->second;
            traitImplementations[traitName].insert(itemName);
        }
    }
}

void TypeChecker::checkAssociatedFunction(Function& function) {
    // 检查关联函数签名
    checkFunctionSignature(function);
    
    scopeTree->enterScope(Scope::ScopeType::Function, &function);
    checkFunctionParameters(*function.functionparameters);
    checkFunctionReturnType(*function.functionreturntype);
    checkFunctionBody(function);
    
    scopeTree->exitScope();
}

void TypeChecker::checkAssociatedConstant(ConstantItem& constant) {
    // 检查关联常量类型
    auto type = checkType(*constant.type);
    if (!type) {
        reportError("Invalid type in associated constant");
    }
    
    // 检查常量表达式
    auto exprType = inferExpressionType(*constant.expression);
    if (exprType && !areTypesCompatible(type, exprType)) {
        reportError("Type mismatch in associated constant");
    }
}

void TypeChecker::checkFunctionSignature(Function& function) {
    // 检查函数名是否冲突（在相应作用域内）
    std::string funcName = function.identifier_name;
    auto existingSymbol = scopeTree->lookupSymbolInCurrentScope(funcName);
    if (existingSymbol && existingSymbol->kind == SymbolKind::Function) {
        reportError("Function '" + funcName + "' is already defined in this scope");
    }
}

void TypeChecker::checkFunctionParameters(FunctionParameters& params) {
    for (const auto& param : params.functionparams) {
        // 检查参数类型
        auto paramType = checkType(*param->type);
        if (!paramType) {
            reportError("Invalid type in function parameter");
        }
        // 检查参数模式
        checkPattern(*param->patternnotopalt, paramType);
    }
}

void TypeChecker::checkFunctionReturnType(FunctionReturnType& returnType) {
    if (returnType.type != nullptr) {
        auto type = checkType(*returnType.type);
        if (!type) {
            reportError("Invalid return type in function");
        }
    }
}

void TypeChecker::checkFunctionBody(Function& function) {
    if (function.blockexpression) {
        if (function.functionreturntype != nullptr && function.functionreturntype->type != nullptr) {
            pushExpectedType(checkType(*function.functionreturntype->type));
        } else {
            // 如果没有显式返回类型，设置默认的unit类型
            pushExpectedType(std::make_shared<SimpleType>("unit"));
        }
        
        function.blockexpression->accept(*this);
        popExpectedType();
    }
}

std::shared_ptr<SemanticType> TypeChecker::checkType(Type& typeNode) {
    if (auto typePath = dynamic_cast<TypePath*>(&typeNode)) {
        return checkType(*typePath);
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&typeNode)) {
        return checkType(*arrayType);
    } else if (auto refType = dynamic_cast<ReferenceType*>(&typeNode)) {
        return checkType(*refType);
    }

    return nullptr;
}

std::shared_ptr<SemanticType> TypeChecker::checkType(TypePath& typePath) {
    if (!typePath.simplepathsegement) {
        return nullptr;
    }
    std::string typeName = typePath.simplepathsegement->identifier;
    return resolveType(typeName);
}

std::shared_ptr<SemanticType> TypeChecker::checkType(ArrayType& arrayType) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&arrayType);
    if (nodeIt != nodeTypeMap.end()) {
        return nodeIt->second;
    }
    
    // 先设置一个占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("array_placeholder");
    nodeTypeMap[&arrayType] = placeholder;
    
    auto elementType = checkType(*arrayType.type);
    if (!elementType) {
        nodeTypeMap.erase(&arrayType);
        return nullptr;
    }
    
    // 对于数组大小表达式，我们只检查它是否为字面量，而不进行完整的类型推断
    // 这样可以避免循环依赖
    if (arrayType.expression) {
        // 检查是否为字面量表达式
        if (auto literalExpr = dynamic_cast<LiteralExpression*>(arrayType.expression.get())) {
            if (literalExpr->tokentype != Token::kINTEGER_LITERAL) {
                reportError("Array size must be an integer literal");
            }
        } else {
            // 需要更复杂的常量表达式求值
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, arrayType.expression.get());
    nodeTypeMap[&arrayType] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::checkType(ReferenceType& refType) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&refType);
    if (nodeIt != nodeTypeMap.end()) {
        return nodeIt->second; 
    }
    
    // 先设置一个占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("ref_placeholder");
    nodeTypeMap[&refType] = placeholder;
    
    // 检查引用的目标类型
    auto targetType = checkType(*refType.type);
    if (!targetType) {
        nodeTypeMap.erase(&refType);
        return nullptr;
    }
    
    // 创建引用类型
    auto result = std::make_shared<ReferenceTypeWrapper>(targetType, refType.ismut);
    nodeTypeMap[&refType] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::resolveType(const std::string& typeName) {
    auto type = std::make_shared<SimpleType>(typeName);
    return type;
}

bool TypeChecker::typeExists(const std::string& typeName) {
    auto symbol = findSymbol(typeName);
    return symbol && (symbol->kind == SymbolKind::Struct || 
                     symbol->kind == SymbolKind::Enum || 
                     symbol->kind == SymbolKind::BuiltinType ||
                     symbol->kind == SymbolKind::TypeAlias);
}

bool TypeChecker::isTypeVisible(const std::string& typeName) {
    // 简化实现：假设所有类型在当前作用域都可见
    return typeExists(typeName);
}

bool TypeChecker::areTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual) {
    if (!expected || !actual) return false;
    
    std::string expectedStr = expected->tostring();
    std::string actualStr = actual->tostring();
    
    // 简化类型兼容性检查
    if (expectedStr == actualStr) {
        return true;
    }
    // 推断类型与任何类型兼容
    if (actualStr == "_") {
        return true;
    }
    return false;
}

std::shared_ptr<SemanticType> TypeChecker::inferExpressionType(Expression& expr) {
    // 检查缓存
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        // 如果缓存结果是占位符，说明正在处理中，需要继续推断而不是返回占位符
        if (nodeIt->second->tostring() == "inferring") {
            nodeTypeMap.erase(&expr);
        } else {
            return nodeIt->second;
        }
    }
    
    // 防止无限递归，先设置一个占位符
    auto placeholder = std::make_shared<SimpleType>("inferring");
    nodeTypeMap[&expr] = placeholder;
    
    std::shared_ptr<SemanticType> type;
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        type = inferLiteralExpressionType(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        type = inferBinaryExpressionType(*binary);
    } else if (auto call = dynamic_cast<CallExpression*>(&expr)) {
        type = inferCallExpressionType(*call);
    } else if (auto arrayExpr = dynamic_cast<ArrayExpression*>(&expr)) {
        type = inferArrayExpressionType(*arrayExpr);
    } else if (auto pathExpr = dynamic_cast<PathExpression*>(&expr)) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string varName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            auto symbol = findSymbol(varName);
            if (symbol && symbol->type) {
                type = symbol->type;
            } else {
                // 如果找不到符号或符号没有类型信息，返回错误
                type = nullptr;
            }
        } else {
            type = nullptr;
        }
    }
    // else if (auto methodCall = dynamic_cast<MethodCallExpression*>(&expr)) {
    //     type = inferMethodCallExpressionType(*methodCall);
    // }
    
    // 更新缓存
    if (type) {
        nodeTypeMap[&expr] = type;
    } else {
        // 类型推断失败，移除占位符以允许重试
        nodeTypeMap.erase(&expr);
    }
    
    return type;
}

std::shared_ptr<SemanticType> TypeChecker::inferBinaryExpressionType(BinaryExpression& expr) {
    auto leftType = inferExpressionType(*expr.leftexpression);
    auto rightType = inferExpressionType(*expr.rightexpression);
    
    if (!leftType || !rightType) {
        return nullptr;
    }

    // std::cerr << "test binary " << leftType->tostring() << " " << rightType->tostring() << " " << to_string(expr.binarytype) << "\n";
    
    switch (expr.binarytype) {
        case Token::kPlus:
        case Token::kMinus:
        case Token::kStar:
        case Token::kSlash:
            return leftType;
            
        case Token::kShl:
        case Token::kShr:
        case Token::kAnd:
        case Token::kOr:
        case Token::kCaret:
            return leftType;
            
        case Token::kEqEq:
        case Token::kNe:
        case Token::kLt:
        case Token::kGt:
        case Token::kLe:
        case Token::kGe:
            return std::make_shared<SimpleType>("bool");
            
        case Token::kAndAnd:
        case Token::kOrOr:
            return std::make_shared<SimpleType>("bool");
            
        default:
            return nullptr;
    }
}

std::shared_ptr<SemanticType> TypeChecker::inferCallExpressionType(CallExpression& expr) {
    auto calleeType = inferExpressionType(*expr.expression);

    // 查找函数符号
    // 如果callee是路径表达式，尝试解析为函数调用
    if (auto pathExpr = dynamic_cast<PathExpression*>(expr.expression.get())) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string functionName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            
            // 直接从作用域中查找符号
            auto symbol = findSymbol(functionName);
            if (symbol && symbol->kind == SymbolKind::Function) {
                if (symbol->type) {
                    return symbol->type;
                }
            }
            
            // 查找函数符号
            auto functionSymbol = findFunction(functionName);
            if (functionSymbol) {
                if (functionSymbol->returntype) {
                    return functionSymbol->returntype;
                }
                // 如果returntype字段无效，尝试使用type字段
                if (functionSymbol->type) {
                    return functionSymbol->type;
                }
            }
        }
    }
    
    return calleeType;
}

std::shared_ptr<SemanticType> TypeChecker::inferMethodCallExpressionType(MethodCallExpression& expr) {
    // 方法调用：需要查找方法定义并返回其返回类型
    return std::make_shared<SimpleType>("unknown");
}

std::shared_ptr<SemanticType> TypeChecker::inferLiteralExpressionType(LiteralExpression& expr) {
    // 根据字面量类型推断类型
    switch (expr.tokentype) {
        case Token::kINTEGER_LITERAL: {
            const std::string& literal = expr.literal;
            
            if (literal.length() >= 5) {
                if (literal.substr(literal.length() - 5) == "usize") {
                    return std::make_shared<SimpleType>("usize");
                }
                if (literal.substr(literal.length() - 5) == "isize") {
                    return std::make_shared<SimpleType>("isize");
                }
            }
            if (literal.length() >= 3) {
                std::string suffix = literal.substr(literal.length() - 3);
                if (suffix == "u32") {
                    return std::make_shared<SimpleType>("u32");
                }
                if (suffix == "i32") {
                    return std::make_shared<SimpleType>("i32");
                }
                if (suffix == "u64") {
                    return std::make_shared<SimpleType>("u64");
                }
                if (suffix == "i64") {
                    return std::make_shared<SimpleType>("i64");
                }
            }
            
            // 如果没有后缀，默认为 i32
            return std::make_shared<SimpleType>("i32");
        }
        case Token::kCHAR_LITERAL:
            return std::make_shared<SimpleType>("char");
        case Token::kSTRING_LITERAL:
        case Token::kRAW_STRING_LITERAL:
            return std::make_shared<SimpleType>("str");
        case Token::ktrue:
        case Token::kfalse:
            return std::make_shared<SimpleType>("bool");
        default:
            return std::make_shared<SimpleType>("unknown");
    }
}

std::shared_ptr<SemanticType> TypeChecker::inferArrayExpressionType(ArrayExpression& expr) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        // 如果缓存结果是占位符，说明正在处理中，需要继续推断而不是返回占位符
        if (nodeIt->second->tostring() == "array_expr_placeholder" || nodeIt->second->tostring() == "inferring") {
            // 移除占位符，继续推断
            nodeTypeMap.erase(&expr);
        } else {
            // 返回已完成的推断结果
            return nodeIt->second;
        }
    }
    
    // 先设置占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("array_expr_placeholder");
    nodeTypeMap[&expr] = placeholder;
    
    if (!expr.arrayelements) {
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    std::shared_ptr<SemanticType> elementType = nullptr;
    
    // 检查是否是重复元素语法 [value; count]
    if (expr.arrayelements->istwo) {
        // 对于重复元素语法，只使用第一个表达式作为元素类型
        if (expr.arrayelements->expressions.empty()) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        const auto& element = expr.arrayelements->expressions[0];
        if (!element) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        std::shared_ptr<SemanticType> elemType;
        // 对于字面量，直接推断类型
        if (auto literal = dynamic_cast<LiteralExpression*>(element.get())) {
            elemType = inferLiteralExpressionType(*literal);
        } else {
            // 对于其他类型的表达式，递归推断
            elemType = inferExpressionType(*element);
        }
        
        if (!elemType) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        elementType = elemType;
    } else {
        // 对于逗号分隔语法，检查所有数组元素的类型
        for (const auto& element : expr.arrayelements->expressions) {
            if (element) {
                std::shared_ptr<SemanticType> elemType;
                // 对于字面量，直接推断类型
                if (auto literal = dynamic_cast<LiteralExpression*>(element.get())) {
                    elemType = inferLiteralExpressionType(*literal);
                } else {
                    // 对于其他类型的表达式，递归推断（但要避免对数组表达式递归）
                    if (auto innerArrayExpr = dynamic_cast<ArrayExpression*>(element.get())) {
                        elemType = inferArrayExpressionType(*innerArrayExpr);
                    } else {
                        elemType = inferExpressionType(*element);
                    }
                }
                
                if (!elemType) {
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
                if (!elementType) {
                    elementType = elemType;
                } else if (!areTypesCompatible(elementType, elemType)) {
                    reportError("Array elements must have the same type");
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
            }
        }
    }
    
    if (!elementType) {
        // 空数组，无法推断类型
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, nullptr);
    nodeTypeMap[&expr] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::inferConstantExpressionType(Expression& expr, std::shared_ptr<SemanticType> expectedType) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        // 如果是占位符，说明正在处理中，返回期望类型
        if (nodeIt->second->tostring() == "const_expr_placeholder") {
            return expectedType;
        }
        return nodeIt->second;
    }
    
    // 先设置占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("const_expr_placeholder");
    nodeTypeMap[&expr] = placeholder;
    
    std::shared_ptr<SemanticType> result = nullptr;
    
    if (auto literalExpr = dynamic_cast<LiteralExpression*>(&expr)) {
        result = inferLiteralExpressionType(*literalExpr);
    } else if (auto arrayExpr = dynamic_cast<ArrayExpression*>(&expr)) {
        result = inferArrayExpressionTypeWithExpected(*arrayExpr, expectedType);
    } else if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&expr)) {
        result = inferBinaryExpressionType(*binaryExpr);
    } else {
        result = expectedType;
    }
    
    // 只有当结果有效时才替换占位符
    if (result) {
        nodeTypeMap[&expr] = result;
    } else {
        nodeTypeMap.erase(&expr);
    }
    
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::inferArrayExpressionTypeWithExpected(ArrayExpression& expr, std::shared_ptr<SemanticType> expectedType) {
    if (!expectedType) {
        return inferArrayExpressionType(expr);
    }
    
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        // 如果是占位符，说明正在处理中，返回期望类型
        if (nodeIt->second->tostring() == "array_expr_placeholder") {
            return expectedType;
        }
        return nodeIt->second;
    }
    
    // 先设置占位符
    auto placeholder = std::make_shared<SimpleType>("array_expr_placeholder");
    nodeTypeMap[&expr] = placeholder;
    
    std::shared_ptr<SemanticType> expectedElementType = nullptr;
    if (auto arrayTypeWrapper = dynamic_cast<ArrayTypeWrapper*>(expectedType.get())) {
        expectedElementType = arrayTypeWrapper->getElementType();
    }
    
    if (!expr.arrayelements) {
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    std::shared_ptr<SemanticType> elementType = expectedElementType;
    
    // 检查是否是重复元素语法 [value; count]
    if (expr.arrayelements->istwo) {
        // 对于重复元素语法，只使用第一个表达式作为元素类型
        if (expr.arrayelements->expressions.empty()) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        const auto& element = expr.arrayelements->expressions[0];
        if (!element) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        
        std::shared_ptr<SemanticType> elemType;
        
        // 如果元素是数组表达式（用于多维数组），递归处理
        if (auto innerArrayExpr = dynamic_cast<ArrayExpression*>(element.get())) {
            if (expectedElementType) {
                elemType = inferArrayExpressionTypeWithExpected(*innerArrayExpr, expectedElementType);
            } else {
                elemType = inferArrayExpressionType(*innerArrayExpr);
            }
        } else {
            // 非数组元素，使用期望类型推断
            if (expectedElementType) {
                elemType = inferConstantExpressionType(*element, expectedElementType);
            } else {
                elemType = inferExpressionType(*element);
            }
        }
        
        if (!elemType) {
            nodeTypeMap.erase(&expr);
            return nullptr;
        }
        elementType = elemType;
    } else {
        // 对于逗号分隔语法，检查所有数组元素的类型
        for (const auto& element : expr.arrayelements->expressions) {
            if (element) {
                std::shared_ptr<SemanticType> elemType;
                // 如果元素是数组表达式（用于多维数组），递归处理
                if (auto innerArrayExpr = dynamic_cast<ArrayExpression*>(element.get())) {
                    if (expectedElementType) {
                        elemType = inferArrayExpressionTypeWithExpected(*innerArrayExpr, expectedElementType);
                    } else {
                        elemType = inferArrayExpressionType(*innerArrayExpr);
                    }
                } else {
                    // 非数组元素，使用期望类型推断
                    if (expectedElementType) {
                        elemType = inferConstantExpressionType(*element, expectedElementType);
                    } else {
                        elemType = inferExpressionType(*element);
                    }
                }
                
                if (!elemType) {
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
                if (!elementType) {
                    elementType = elemType;
                } else if (!areTypesCompatible(elementType, elemType)) {
                    reportError("Array elements must have the same type");
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
            }
        }
    }
    
    if (!elementType) {
        nodeTypeMap.erase(&expr);
        return nullptr;
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, nullptr);
    nodeTypeMap[&expr] = result; // 替换占位符
    return result;
}

// 错误报告
void TypeChecker::reportError(const std::string& message) {
    std::cerr << "Type Error: " << message << std::endl;
    hasErrors = true;
}

void TypeChecker::reportUndefinedType(const std::string& typeName, ASTNode* context) {
    std::cerr << "Undefined Type: '" << typeName << "'" << std::endl;
    hasErrors = true;
}

void TypeChecker::reportMissingTraitImplementation(const std::string& traitName, const std::string& missingItem) {
    std::cerr << "Missing trait implementation: trait '" << traitName 
              << "' requires '" << missingItem << "'" << std::endl;
    hasErrors = true;
}

std::shared_ptr<Symbol> TypeChecker::findSymbol(const std::string& name) {
    if (!scopeTree) {
        return nullptr;
    }
    auto  symbol= scopeTree->lookupSymbol(name);
    return symbol;
}

std::shared_ptr<FunctionSymbol> TypeChecker::findFunction(const std::string& name) {
    auto symbol = findSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Function) {
        return std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    }
    return nullptr;
}

std::shared_ptr<StructSymbol> TypeChecker::findStruct(const std::string& name) {
    auto symbol = findSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Struct) {
        return std::dynamic_pointer_cast<StructSymbol>(symbol);
    }
    return nullptr;
}

std::shared_ptr<TraitSymbol> TypeChecker::findTrait(const std::string& name) {
    auto symbol = findSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Trait) {
        return std::dynamic_pointer_cast<TraitSymbol>(symbol);
    }
    return nullptr;
}

// 上下文管理
void TypeChecker::enterStructContext(const std::string& structName) {
    currentStruct = structName;
}

void TypeChecker::exitStructContext() {
    currentStruct.clear();
}

void TypeChecker::enterTraitContext(const std::string& traitName) {
    currentTrait = traitName;
}

void TypeChecker::exitTraitContext() {
    currentTrait.clear();
    currentTraitRequirements.clear();
}

void TypeChecker::enterImplContext(const std::string& implName) {
    currentImpl = implName;
}

void TypeChecker::exitImplContext() {
    currentImpl.clear();
}

// 辅助方法
void TypeChecker::pushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void TypeChecker::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* TypeChecker::getCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void TypeChecker::pushExpectedType(std::shared_ptr<SemanticType> type) {
    expectedTypeStack.push(type);
}

void TypeChecker::popExpectedType() {
    if (!expectedTypeStack.empty()) {
        expectedTypeStack.pop();
    }
}

std::shared_ptr<SemanticType> TypeChecker::getExpectedType() {
    return expectedTypeStack.empty() ? nullptr : expectedTypeStack.top();
}

// 在TypeChecker类中添加模式检查方法
void TypeChecker::checkPattern(Pattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    if (auto identPattern = dynamic_cast<IdentifierPattern*>(&pattern)) {
        checkPattern(*identPattern, expectedType);
    } else if (auto refPattern = dynamic_cast<ReferencePattern*>(&pattern)) {
        checkPattern(*refPattern, expectedType);
    }
}

void TypeChecker::checkPattern(IdentifierPattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    std::string varName = pattern.identifier;
    auto varSymbol = std::make_shared<Symbol>(
        varName,
        SymbolKind::Variable,
        expectedType,
        pattern.hasmut,
        &pattern
    );
    
    if (!scopeTree->insertSymbol(varName, varSymbol)) {
        reportError("Variable '" + varName + "' is already defined in this scope");
    }
}


void TypeChecker::checkPattern(ReferencePattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    // 检查引用模式
    if (!expectedType || expectedType->tostring().find("&") != 0) {
        reportError("Reference pattern requires reference type");
        return;
    }
    
    // 检查内部模式
    auto innerType = std::make_shared<SimpleType>(expectedType->tostring().substr(1));
    checkPattern(*pattern.pattern, innerType);
}

// 可变性检查实现
void TypeChecker::checkAssignmentMutability(Expression& lhs) {
    if (auto pathExpr = dynamic_cast<PathExpression*>(&lhs)) {
        checkVariableMutability(*pathExpr);
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(&lhs)) {
        checkFieldMutability(*fieldExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(&lhs)) {
        checkIndexMutability(*indexExpr);
    }
}

void TypeChecker::checkVariableMutability(PathExpression& pathExpr) {
    if (!pathExpr.simplepath) return;
    
    std::string varName;
    if (!pathExpr.simplepath || pathExpr.simplepath->simplepathsegements.empty()) {
        return;
    }
    varName = pathExpr.simplepath->simplepathsegements[0]->identifier;
    
    if (varName.empty()) return;
    auto symbol = scopeTree->lookupSymbol(varName);
    if (!symbol) {
        reportError("Undefined variable: " + varName);
        return;
    }
    
    if (!symbol->ismutable) {
        reportMutabilityError(varName, "variable", &pathExpr);
    }
}

void TypeChecker::checkFieldMutability(FieldExpression& fieldExpr) {
    // 首先检查基础表达式的可变性
    checkAssignmentMutability(*fieldExpr.expression);
    
    // 对于结构体字段，我们需要检查结构体实例是否可变
    // 递归检查基础表达式，确保结构体实例本身是可变的
    if (auto pathExpr = dynamic_cast<PathExpression*>(fieldExpr.expression.get())) {
        checkVariableMutability(*pathExpr);
    } else if (auto nestedFieldExpr = dynamic_cast<FieldExpression*>(fieldExpr.expression.get())) {
        checkFieldMutability(*nestedFieldExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(fieldExpr.expression.get())) {
        checkIndexMutability(*indexExpr);
    }
}

void TypeChecker::checkIndexMutability(IndexExpression& indexExpr) {
    // 对于数组索引，我们需要检查数组本身是否可变
    // 直接检查基础表达式的可变性，确保数组变量是可变的
    
    if (auto pathExpr = dynamic_cast<PathExpression*>(indexExpr.expressionout.get())) {
        checkVariableMutability(*pathExpr);
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(indexExpr.expressionout.get())) {
        checkFieldMutability(*fieldExpr);
    } else if (auto nestedIndexExpr = dynamic_cast<IndexExpression*>(indexExpr.expressionout.get())) {
        // 递归处理嵌套的索引表达式，如 arr[1][2]
        checkIndexMutability(*nestedIndexExpr);
    } else {
        // 递归检查更复杂的表达式
        checkAssignmentMutability(*indexExpr.expressionout);
    }
}

void TypeChecker::reportMutabilityError(const std::string& name, const std::string& errorType, ASTNode* context) {
    std::cerr << "Mutability Error: Cannot modify " << errorType << " '" << name
              << "' as it is not declared as mutable" << std::endl;
    hasErrors = true;
}

void TypeChecker::visit(AssignmentExpression& node) {
    pushNode(node);
    
    // 检查左值的可变性
    if (node.leftexpression) {
        checkAssignmentMutability(*node.leftexpression);
    }
    
    popNode();
}

void TypeChecker::visit(CompoundAssignmentExpression& node) {
    pushNode(node);
    
    // 检查左值的可变性
    if (node.leftexpression) {
        checkAssignmentMutability(*node.leftexpression);
    }
    // 检查右值的类型
    if (node.rightexpression) {
        auto rightType = inferExpressionType(*node.rightexpression);
    }
    
    popNode();
}

void TypeChecker::visit(IndexExpression& node) {
    pushNode(node);
    
    if (node.expressionout) {
        node.expressionout->accept(*this);
    }
    if (node.expressionin) {
        node.expressionin->accept(*this);
        
        // 检查索引类型必须是整数
        auto indexType = inferExpressionType(*node.expressionin);
        if (indexType && indexType->tostring() != "i32" && indexType->tostring() != "usize") {
            reportError("Array index must be of integer type, found " + indexType->tostring());
        }
    }
    
    // 推断索引表达式的类型
    if (node.expressionout) {
        auto arrayType = inferExpressionType(*node.expressionout);
        if (arrayType) {
            // 提取元素类型
            if (auto arrayTypeWrapper = dynamic_cast<ArrayTypeWrapper*>(arrayType.get())) {
                auto elementType = arrayTypeWrapper->getElementType();
                nodeTypeMap[&node] = elementType;
            } else {
                reportError("Cannot index into non-array type: " + arrayType->tostring());
            }
        }
    }
    
    popNode();
}

void TypeChecker::visit(FieldExpression& node) {
    pushNode(node);
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void TypeChecker::visit(PathExpression& node) {
    pushNode(node);
    
    // 路径表达式通常不需要特殊处理，主要是符号查找
    // 实际的类型推断在使用时进行
    
    popNode();
}

void TypeChecker::visit(ExpressionStatement& node) {
    pushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    
    popNode();
}

void TypeChecker::visit(LetStatement& node) {
    pushNode(node);
    
    std::shared_ptr<SemanticType> declaredType = nullptr;
    if (node.type) {
        declaredType = checkType(*node.type);
        if (!declaredType) {
            reportError("Invalid type in let statement");
            popNode();
            return;
        }
    }
    
    // 检查初始化表达式
    if (node.expression) {
        if (declaredType) {
            // 对于有明确类型声明的let语句，进行完整的类型检查，包括数组长度验证
            // 推断初始化表达式的类型
            auto initType = inferExpressionType(*node.expression);
            
            if (!initType) {
                if (auto pattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
                    reportError("Unable to infer type for let statement initializer for variable: " + pattern->identifier);
                } else {
                    reportError("Unable to infer type for let statement initializer");
                }
                popNode();
                return;
            }
            // 检查类型兼容性
            if (!areTypesCompatible(declaredType, initType)) {
                reportError("Type mismatch in let statement: expected '" + declaredType->tostring() +
                           "', found '" + initType->tostring() + "'");
                popNode();
                return;
            }
            // 特殊处理数组类型：进行大小验证
            if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(declaredType.get())) {
                if (auto arrayExpr = dynamic_cast<ArrayExpression*>(node.expression.get())) {
                    checkArraySizeMatch(*arrayType, *arrayExpr);
                }
            }
        } else {
            // 没有声明类型，使用普通推断
            auto initType = inferExpressionType(*node.expression);
            if (!initType) {
                // 添加调试信息
                if (auto pattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
                    reportError("Unable to infer type for let statement for variable: " + pattern->identifier);
                } else {
                    reportError("Unable to infer type for let statement");
                }
                popNode();
                return;
            }
        }
    }
    
    // 检查模式并注册变量
    if (node.patternnotopalt) {
        std::shared_ptr<SemanticType> varType = declaredType ? declaredType : std::make_shared<SimpleType>("inferred");
        checkPattern(*node.patternnotopalt, varType);
    }
    popNode();
}

// 数组大小验证实现
void TypeChecker::checkArraySizeMatch(ArrayTypeWrapper& declaredType, ArrayExpression& arrayExpr) {
    // 获取声明的数组大小
    auto sizeExpr = declaredType.getSizeExpression();
    if (!sizeExpr) {
        return;
    }
    
    int64_t declaredSize = evaluateArraySize(*sizeExpr);
    if (declaredSize < 0) {
        reportError("Invalid array size expression");
        return;
    }
    
    // 获取初始化数组的实际大小
    if (!arrayExpr.arrayelements) {
        reportError("Array expression has no elements");
        return;
    }
    
    int64_t actualSize;
    // 检查是否是重复元素语法 [value; count]
    if (arrayExpr.arrayelements->istwo) {
        // 对于重复元素语法，第二个表达式是重复次数
        if (arrayExpr.arrayelements->expressions.size() < 2) {
            reportError("Invalid repeated array expression: missing count");
            return;
        }
        
        // 获取重复次数
        const auto& countExpr = arrayExpr.arrayelements->expressions[1];
        actualSize = evaluateArraySize(*countExpr);
        if (actualSize < 0) {
            reportError("Invalid array count expression");
            return;
        }
    } else {
        // 对于逗号分隔语法，实际大小就是表达式数量
        actualSize = arrayExpr.arrayelements->expressions.size();
    }
    
    // 比较大小
    if (declaredSize != actualSize) {
        reportError("Array size mismatch: declared size " + std::to_string(declaredSize) +
                   ", but initializer has " + std::to_string(actualSize) + " elements");
    }
}

int64_t TypeChecker::evaluateArraySize(Expression& sizeExpr) {
    if (auto literal = dynamic_cast<LiteralExpression*>(&sizeExpr)) {
        if (literal->tokentype == Token::kINTEGER_LITERAL) {
            try {
                std::string numStr = literal->literal;
                if (numStr.length() >= 3) {
                    std::string suffix = numStr.substr(numStr.length() - 3);
                    if (suffix == "u32" || suffix == "i32" || suffix == "u64" || suffix == "i64") {
                        numStr = numStr.substr(0, numStr.length() - 3);
                    }
                }
                if (numStr.length() >= 5) {
                    std::string suffix = numStr.substr(numStr.length() - 5);
                    if (suffix == "usize" || suffix == "isize") {
                        numStr = numStr.substr(0, numStr.length() - 5);
                    }
                }
                
                return std::stoll(numStr);
            } catch (const std::exception& e) {
                reportError("Invalid integer literal in array size: " + literal->literal);
                return -1;
            }
        } else {
            reportError("Array size must be an integer literal");
            return -1;
        }
    }
    
    if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&sizeExpr)) {
        if (binaryExpr->binarytype == Token::kPlus ||
            binaryExpr->binarytype == Token::kMinus ||
            binaryExpr->binarytype == Token::kStar ||
            binaryExpr->binarytype == Token::kSlash) {
            
            int64_t leftValue = evaluateArraySize(*binaryExpr->leftexpression);
            int64_t rightValue = evaluateArraySize(*binaryExpr->rightexpression);
            
            if (leftValue < 0 || rightValue < 0) {
                return -1; // 传播错误
            }
            
            switch (binaryExpr->binarytype) {
                case Token::kPlus:
                    return leftValue + rightValue;
                case Token::kMinus:
                    return leftValue - rightValue;
                case Token::kStar:
                    return leftValue * rightValue;
                case Token::kSlash:
                    if (rightValue == 0) {
                        reportError("Division by zero in array size expression");
                        return -1;
                    }
                    return leftValue / rightValue;
                default:
                    break;
            }
        }
    }
    
    if (auto pathExpr = dynamic_cast<PathExpression*>(&sizeExpr)) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string constName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            
            if (constantEvaluator) {
                auto constValue = constantEvaluator->getConstantValue(constName);
                if (constValue) {
                    // 尝试将常量值转换为整数
                    if (auto intConst = dynamic_cast<IntConstant*>(constValue.get())) {
                        return intConst->getValue();
                    } else {
                        reportError("Constant '" + constName + "' is not an integer constant");
                        return -1;
                    }
                }
            }
            
            // 查找常量符号（作为备用方案）
            auto symbol = findSymbol(constName);
            if (symbol && symbol->kind == SymbolKind::Constant) {
                reportError("Cannot evaluate constant '" + constName + "' at compile time");
                return -1;
            }
        }
    }
    
    reportError("Complex array size expressions not supported");
    return -1;
}

void TypeChecker::visit(BlockExpression& node) {
    pushNode(node);
    
    // 进入新的作用域
    scopeTree->enterScope(Scope::ScopeType::Block, &node);
    for (const auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    // 退出作用域
    scopeTree->exitScope();
    
    popNode();
}

void TypeChecker::visit(IfExpression& node) {
    pushNode(node);
    
    // 检查条件表达式
    if (node.conditions && node.conditions->expression) {
        node.conditions->expression->accept(*this);
        auto condType = inferExpressionType(*node.conditions->expression);
        // 条件表达式必须是布尔类型
        if (condType && condType->tostring() != "bool") {
            reportError("If condition must be of type bool, found " + condType->tostring());
        }
    }
    
    // 检查if分支
    if (node.ifblockexpression) {
        node.ifblockexpression->accept(*this);
    }
    
    // 检查else分支（如果有）
    if (node.elseexpression) {
        node.elseexpression->accept(*this);
        // 检查两个分支的类型兼容性
        auto ifType = inferExpressionType(*node.ifblockexpression);
        auto elseType = inferExpressionType(*node.elseexpression);
        if (ifType && elseType) {
            // 如果两个分支都不是!类型，检查类型兼容性
            if (ifType->tostring() != "!" && elseType->tostring() != "!") {
                if (!areTypesCompatible(ifType, elseType)) {
                    reportError("If expression branches have incompatible types: " +
                               ifType->tostring() + " vs " + elseType->tostring());
                }
            }
        }
    }
    
    popNode();
}




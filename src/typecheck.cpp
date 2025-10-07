#include "typecheck.hpp"
#include "typewrapper.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <memory>
#include <utility>

TypeChecker::TypeChecker(std::shared_ptr<ScopeTree> scopeTree) 
    : scopeTree(scopeTree) {}

bool TypeChecker::checkTypes() {
    hasErrors = false;
    return !hasErrors;
}

bool TypeChecker::hasTypeErrors() const {
    return hasErrors;
}

void TypeChecker::visit(Crate& node) {
    pushNode(node);
    
    // 检查crate中的所有item
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
    
    auto itemContent = std::move(node.item);
    if (itemContent) {
        itemContent->accept(*this);
    }
    
    popNode();
}

void TypeChecker::visit(Function& node) {
    pushNode(node);
    
    // 检查函数签名
    checkFunctionSignature(node);
    
    // 进入函数作用域
    scopeTree->enterScope(Scope::ScopeType::Function, &node);
    
    // 检查参数类型
    checkFunctionParameters(*node.functionparameters);
    
    // 检查返回类型
    checkFunctionReturnType(*node.functionreturntype);
    
    // 检查函数体
    checkFunctionBody(node);
    
    // 退出函数作用域
    scopeTree->exitScope();
    
    popNode();
}

void TypeChecker::visit(ConstantItem& node) {
    pushNode(node);
    
    // 检查常量类型
    auto type = checkType(*node.type);
    if (!type) {
        reportError("Invalid type in constant declaration");
    }
    
    // 检查常量表达式类型兼容性
    auto exprType = inferExpressionType(*node.expression);
    if (exprType && !areTypesCompatible(type, exprType)) {
        reportError("Type mismatch in constant declaration");
    }
    
    popNode();
}

void TypeChecker::visit(StructStruct& node) {
    pushNode(node);
    
    std::string previousStruct = currentStruct;
    enterStructContext(node.identifier);
    
    // 进入结构体作用域
    scopeTree->enterScope(Scope::ScopeType::Struct, &node);
    
    // 检查结构体字段类型
    checkStructFields(node);
    
    // 退出结构体作用域
    scopeTree->exitScope();
    
    exitStructContext();
    currentStruct = previousStruct;
    
    popNode();
}

void TypeChecker::visit(Enumeration& node) {
    pushNode(node);
    
    // 检查枚举类型
    auto enumName = node.identifier;
    if (!typeExists(enumName)) {
        reportUndefinedType(enumName, &node);
    }
    
    // 检查variants（如果有类型信息）
    auto variants = std::move(node.enumvariants);
    if (variants) {
        for (const auto& variant : variants->enumvariants) {
            // 检查variant类型（如果有）
            // 这里简化处理，实际需要根据enum variant的具体类型检查
        }
    }
    
    popNode();
}

void TypeChecker::visit(InherentImpl& node) {
    pushNode(node);
    
    std::string previousImpl = currentImpl;
    enterImplContext("impl_" + std::to_string(reinterpret_cast<uintptr_t>(&node)));
    
    // 进入impl作用域
    scopeTree->enterScope(Scope::ScopeType::Impl, &node);
    
    // 检查impl目标类型
    auto targetType = getImplTargetType(node);
    if (!targetType) {
        reportError("Invalid target type in impl block");
    }
    
    // 检查是固有实现还是trait实现
    std::string traitName = getTraitNameFromImpl(node);
    if (!traitName.empty()) {
        // Trait实现检查
        checkTraitImpl(node);
    } else {
        // 固有实现检查
        checkInherentImpl(node);
    }
    
    // 检查关联项
    auto items = std::move(node.associateditems);
    for (const auto& item : items) {
        if (item) {
            checkAssociatedItem(*item);
        }
    }
    
    // 退出impl作用域
    scopeTree->exitScope();
    
    exitImplContext();
    currentImpl = previousImpl;
    
    popNode();
}

// Struct字段检查
void TypeChecker::checkStructFields(StructStruct& node) {
    auto fields = std::move(node.structfileds);
    if (!fields) return;
    
    for (const auto& field : fields->structfields) {
        checkStructFieldType(*field);
    }
}

void TypeChecker::checkStructFieldType(StructField& field) {
    auto fieldType = checkType(*field.type);
    if (!fieldType) {
        reportError("Invalid type in struct field: " + field.identifier);
    }
    
    // 记录字段类型到符号表
    auto fieldSymbol = std::make_shared<Symbol>(
        field.identifier,
        SymbolKind::Variable,
        fieldType,
        false,
        &field
    );
    
    scopeTree->insertSymbol(field.identifier, fieldSymbol);
}

// Impl检查
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
    auto items = std::move(node.associateditems);
    for (const auto& item : items) {
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
    
    // 检查trait是否存在
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
    // 这里需要根据你的AST结构实现具体逻辑
    // 简化实现：返回一个简单类型
    return std::make_shared<SimpleType>("UnknownType");
}

std::string TypeChecker::getTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
    // 这里需要根据你的AST结构实现具体逻辑
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
    auto itemContent = std::move(item.consttantitem_or_function);
    if (!itemContent) return;
    
    std::string itemName;
    
    if (auto function = dynamic_cast<Function*>(itemContent.get())) {
        checkAssociatedFunction(*function);
        itemName = function->identifier_name;
    } else if (auto constant = dynamic_cast<ConstantItem*>(itemContent.get())) {
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
    
    // 进入函数作用域
    scopeTree->enterScope(Scope::ScopeType::Function, &function);
    
    // 检查参数和返回类型
    checkFunctionParameters(*function.functionparameters);
    checkFunctionReturnType(*function.functionreturntype);
    
    // 检查函数体
    checkFunctionBody(function);
    
    // 退出函数作用域
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

// 函数检查
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
    auto type = checkType(*returnType.type);
    if (!type) {
        reportError("Invalid return type in function");
    }
}

void TypeChecker::checkFunctionBody(Function& function) {
    auto body = std::move(function.blockexpression);
    if (body) {
        // 设置期望的返回类型
        pushExpectedType(checkType(*function.functionreturntype->type));
        body->accept(*this);
        popExpectedType();
    }
}

// 类型解析和检查
std::shared_ptr<SemanticType> TypeChecker::checkType(Type& typeNode) {
    if (auto typePath = dynamic_cast<TypePath*>(&typeNode)) {
        return checkType(*typePath);
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&typeNode)) {
        return checkType(*arrayType);
    } else if (auto sliceType = dynamic_cast<SliceType*>(&typeNode)) {
        return checkType(*sliceType);
    } else if (auto refType = dynamic_cast<ReferenceType*>(&typeNode)) {
        return checkType(*refType);
    } else if (auto inferredType = dynamic_cast<InferredType*>(&typeNode)) {
        return checkType(*inferredType);
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeChecker::checkType(TypePath& typePath) {
    auto segment = std::move(typePath.simplepathsegement);
    if (!segment) return nullptr;
    
    std::string typeName = segment->identifier;
    return resolveType(typeName);
}

std::shared_ptr<SemanticType> TypeChecker::checkType(ArrayType& arrayType) {
    auto elementType = checkType(*arrayType.type);
    if (!elementType) {
        return nullptr;
    }
    
    // 检查数组大小表达式
    auto sizeExpr = std::move(arrayType.expression);
    if (sizeExpr) {
        auto sizeType = inferExpressionType(*sizeExpr);
        // 应该是一个整数类型
        if (sizeType && sizeType->tostring() != "usize" && 
            sizeType->tostring() != "u32" && sizeType->tostring() != "i32") {
            reportError("Array size must be an integer");
        }
    }
    
    return std::make_shared<ArrayTypeWrapper>(elementType, sizeExpr.get());
}

std::shared_ptr<SemanticType> TypeChecker::checkType(SliceType& sliceType) {
    auto elementType = checkType(*sliceType.type);
    if (!elementType) {
        return nullptr;
    }
    
    return std::make_shared<SliceTypeWrapper>(elementType);
}

std::shared_ptr<SemanticType> TypeChecker::checkType(InferredType& inferredType) {
    // 推断类型，返回一个占位符
    return std::make_shared<SimpleType>("_");
}

std::shared_ptr<SemanticType> TypeChecker::resolveType(const std::string& typeName) {
    // 检查缓存
    auto cacheIt = typeCache.find(typeName);
    if (cacheIt != typeCache.end()) {
        return cacheIt->second;
    }
    
    // 查找类型符号
    auto symbol = findSymbol(typeName);
    if (!symbol || (symbol->kind != SymbolKind::Struct && 
                   symbol->kind != SymbolKind::Enum && 
                   symbol->kind != SymbolKind::BuiltinType &&
                   symbol->kind != SymbolKind::TypeAlias)) {
        reportUndefinedType(typeName, getCurrentNode());
        return nullptr;
    }
    
    // 创建类型并缓存
    auto type = std::make_shared<SimpleType>(typeName);
    typeCache[typeName] = type;
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
    // 实际实现需要检查模块可见性规则
    return typeExists(typeName);
}

bool TypeChecker::areTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual) {
    if (!expected || !actual) return false;
    
    // 简化类型兼容性检查
    // 实际实现需要处理类型转换、子类型等复杂情况
    return expected->tostring() == actual->tostring() || 
           actual->tostring() == "_";  // 推断类型与任何类型兼容
}

// 表达式类型推断
std::shared_ptr<SemanticType> TypeChecker::inferExpressionType(Expression& expr) {
    // 检查缓存
    auto nodeIt = nodeTypeMap.find(&expr);
    if (nodeIt != nodeTypeMap.end()) {
        return nodeIt->second;
    }
    
    std::shared_ptr<SemanticType> type;
    
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        type = inferExpressionType(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        type = inferBinaryExpressionType(*binary);
    } else if (auto call = dynamic_cast<CallExpression*>(&expr)) {
        type = inferCallExpressionType(*call);
    } 
    // else if (auto methodCall = dynamic_cast<MethodCallExpression*>(&expr)) {
    //     type = inferMethodCallExpressionType(*methodCall);
    // }
    // 其他表达式类型...
    
    // 缓存结果
    if (type) {
        nodeTypeMap[&expr] = type;
    }
    
    return type;
}

std::shared_ptr<SemanticType> TypeChecker::inferBinaryExpressionType(BinaryExpression& expr) {
    auto leftType = inferExpressionType(*expr.leftexpression);
    auto rightType = inferExpressionType(*expr.rightexpression);
    
    if (!leftType || !rightType) {
        return nullptr;
    }
    
    // 根据操作符推断类型
    switch (expr.binarytype) {
        case Token::kPlus:
        case Token::kMinus:
        case Token::kStar:
        case Token::kSlash:
            // 算术运算：返回操作数类型（需要类型提升）
            return leftType;
            
        case Token::kEqEq:
        case Token::kNe:
        case Token::kLt:
        case Token::kGt:
        case Token::kLe:
        case Token::kGe:
            // 比较运算：返回bool
            return std::make_shared<SimpleType>("bool");
            
        case Token::kAndAnd:
        case Token::kOrOr:
            // 逻辑运算：返回bool
            return std::make_shared<SimpleType>("bool");
            
        default:
            return nullptr;
    }
}

std::shared_ptr<SemanticType> TypeChecker::inferCallExpressionType(CallExpression& expr) {
    auto calleeType = inferExpressionType(*expr.expression);
    if (!calleeType) return nullptr;
    
    // 查找函数符号
    // 这里简化处理，实际需要解析callee的路径
    return std::make_shared<SimpleType>("unknown");
}

std::shared_ptr<SemanticType> TypeChecker::inferMethodCallExpressionType(MethodCallExpression& expr) {
    // 方法调用：需要查找方法定义并返回其返回类型
    return std::make_shared<SimpleType>("unknown");
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

// 符号查找
std::shared_ptr<Symbol> TypeChecker::findSymbol(const std::string& name) {
    return scopeTree->lookupSymbol(name);
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
    // 检查标识符模式类型兼容性
    std::string varName = pattern.identifier;
    
    // 在符号表中注册变量
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
#include "typecheck.hpp"
#include "lexer.hpp"
#include "typewrapper.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <memory>
#include <utility>
#include <unordered_map>
#include <algorithm>

TypeChecker::TypeChecker(std::shared_ptr<ScopeTree> scopeTree, std::shared_ptr<ConstantEvaluator> constantEvaluator)
    : scopeTree(scopeTree), constantEvaluator(constantEvaluator) {}

bool TypeChecker::CheckTypes() {
    // 默认检查在 visit 中实现
    hasErrors = false;
    return !hasErrors;
}

bool TypeChecker::HasTypeErrors() const {
    return hasErrors;
}

void TypeChecker::visit(Crate& node) {
    PushNode(node);
        
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
                    ReportMissingTraitImplementation(traitName, requirement);
                }
            }
        }
    }
    
    PopNode();
}

void TypeChecker::visit(Item& node) {
    PushNode(node);
    if (node.item) {
        node.item->accept(*this);
    }
    PopNode();
}


void TypeChecker::visit(Statement& node) {
    PushNode(node);
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    PopNode();
}

void TypeChecker::visit(Function& node) {
    PushNode(node);
    
    // 检查函数签名
    // checkFunctionSignature(node);
    
    // 进入函数作用域，这样可以在函数体内找到参数符号
    scopeTree->EnterScope(Scope::ScopeType::Function, &node);
    
    if (node.functionparameters) {
        CheckFunctionParameters(*node.functionparameters);
    }
    
    // 检查返回类型
    if (node.functionreturntype) {
        CheckFunctionReturnType(*node.functionreturntype);
    }
    
    // 检查函数体
    CheckFunctionBody(node);
    
    // 退出函数作用域
    scopeTree->ExitScope();
    
    PopNode();
}

void TypeChecker::visit(ConstantItem& node) {
    PushNode(node);
    
    auto type = CheckType(*node.type);
    if (!type) {
        ReportError("Invalid type in constant declaration");
        PopNode();
        return;
    }
    
    // 检查常量表达式类型兼容性
    if (node.expression) {
        // 对于常量项，使用期望类型来推断表达式类型，这样可以正确处理隐式类型转换
        std::shared_ptr<SemanticType> exprType = nullptr;
        
        // 如果是数组表达式，使用带期望类型的推断方法
        if (auto arrayExpr = dynamic_cast<ArrayExpression*>(node.expression.get())) {
            exprType = InferArrayExpressionTypeWithExpected(*arrayExpr, type);
        } else {
            exprType = InferConstantExpressionType(*node.expression, type);
        }
        
        // std::cerr << "TypeChecker: Constant " << node.identifier << " expected type: " << type->tostring()
        //           << ", actual type: " << (exprType ? exprType->tostring() : "null") << std::endl;
        if (exprType && !AreTypesCompatible(type, exprType)) {
            ReportError("Type mismatch in constant declaration: expected '" + type->tostring() +
                       "', found '" + exprType->tostring() + "'");
        }
    }
    
    PopNode();
}

void TypeChecker::visit(StructStruct& node) {
    PushNode(node);
    
    std::string previousStruct = currentStruct;
    EnterStructContext(node.identifier);
    
    scopeTree->EnterScope(Scope::ScopeType::Struct, &node);
    CheckStructFields(node);
    scopeTree->ExitScope();
    
    ExitStructContext();
    currentStruct = previousStruct;
    
    PopNode();
}

void TypeChecker::visit(Enumeration& node) {
    PushNode(node);
    
    auto enumName = node.identifier;
    if (!TypeExists(enumName)) {
        ReportUndefinedType(enumName, &node);
    }
    
    // 检查variants（如果有类型信息）
    if (node.enumvariants) {
        for (const auto& variant : node.enumvariants->enumvariants) {
            // 这里简化处理，实际需要根据enum variant的具体类型检查
        }
    }
    
    PopNode();
}

void TypeChecker::visit(InherentImpl& node) {
    PushNode(node);
    
    std::string previousImpl = currentImpl;
    EnterImplContext("impl_" + std::to_string(reinterpret_cast<uintptr_t>(&node)));
    
    scopeTree->EnterScope(Scope::ScopeType::Impl, &node);
    
    // 检查impl目标类型
    auto targetType = GetImplTargetType(node);
    if (!targetType) {
        ReportError("Invalid target type in impl block");
    }
    
    // 检查是固有实现还是trait实现
    std::string traitName = GetTraitNameFromImpl(node);
    if (!traitName.empty()) {
        CheckTraitImpl(node);
    } else {
        CheckInherentImpl(node);
    }
    
    // 检查关联项
    for (const auto& item : node.associateditems) {
        if (item) {
            CheckAssociatedItem(*item);
        }
    }
    
    scopeTree->ExitScope();
    
    ExitImplContext();
    currentImpl = previousImpl;
    
    PopNode();
}

void TypeChecker::CheckStructFields(StructStruct& node) {
    if (!node.structfileds) return;
    
    for (const auto& field : node.structfileds->structfields) {
        CheckStructFieldType(*field);
    }
}

void TypeChecker::CheckStructFieldType(StructField& field) {
    auto fieldType = CheckType(*field.type);
    if (!fieldType) {
        ReportError("Invalid type in struct field: " + field.identifier);
    }
    
    auto fieldSymbol = std::make_shared<Symbol>(
        field.identifier,
        SymbolKind::Variable,
        fieldType,
        false,
        &field
    );
    
    scopeTree->InsertSymbol(field.identifier, fieldSymbol);
}

void TypeChecker::CheckInherentImpl(InherentImpl& node) {
    auto targetType = GetImplTargetType(node);
    if (!targetType) {
        ReportError("Cannot determine target type for impl block");
        return;
    }
    
    // 检查目标类型是否存在
    auto targetTypeName = targetType->tostring();
    if (!TypeExists(targetTypeName)) {
        ReportUndefinedType(targetTypeName, &node);
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

void TypeChecker::CheckTraitImpl(InherentImpl& node) {
    std::string traitName = GetTraitNameFromImpl(node);
    if (traitName.empty()) {
        ReportError("Invalid trait implementation");
        return;
    }
    
    if (!TypeExists(traitName)) {
        ReportUndefinedType(traitName, &node);
        return;
    }
    
    // 记录impl到trait的映射
    implToTraitMap[currentImpl] = traitName;
    
    // 收集trait的要求
    CollectTraitRequirements(traitName);
    
    // 初始化这个trait的实现集合
    traitImplementations[traitName] = std::unordered_set<std::string>();
}

std::shared_ptr<SemanticType> TypeChecker::GetImplTargetType(InherentImpl& node) {
    // 从impl节点中提取目标类型
    if (node.type) {
        return CheckType(*node.type);
    }
    return nullptr;
}

std::string TypeChecker::GetTraitNameFromImpl(InherentImpl& node) {
    // 从impl节点中提取trait名称
    // 简化实现：返回空字符串表示固有实现
    return "";
}

void TypeChecker::CheckTraitImplementation(InherentImpl& node, const std::string& traitName) {
    // 收集trait的要求
    CollectTraitRequirements(traitName);
    // 检查实现是否满足trait要求
    CheckTraitRequirementsSatisfied(traitName, currentImpl);
}

void TypeChecker::CollectTraitRequirements(const std::string& traitName) {
    auto traitSymbol = FindTrait(traitName);
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

void TypeChecker::CheckTraitRequirementsSatisfied(const std::string& traitName, const std::string& implName) {
    const auto& requirements = traitRequirements[traitName];
    auto& implementations = traitImplementations[implToTraitMap[implName]];
    // 检查每个要求是否都有实现
    for (const auto& requirement : requirements) {
        if (implementations.find(requirement) == implementations.end()) {
            ReportMissingTraitImplementation(traitName, requirement);
        }
    }
}


void TypeChecker::CheckAssociatedItem(AssociatedItem& item) {
    if (!item.consttantitem_or_function) return;
    
    std::string itemName;
    if (auto function = dynamic_cast<Function*>(item.consttantitem_or_function.get())) {
        CheckAssociatedFunction(*function);
        itemName = function->identifier_name;
    } else if (auto constant = dynamic_cast<ConstantItem*>(item.consttantitem_or_function.get())) {
        CheckAssociatedConstant(*constant);
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

void TypeChecker::CheckAssociatedFunction(Function& function) {
    // 检查关联函数签名
    CheckFunctionSignature(function);
    
    scopeTree->EnterScope(Scope::ScopeType::Function, &function);
    CheckFunctionParameters(*function.functionparameters);
    // 修复：暂时跳过返回类型检查，避免段错误
    if (function.functionreturntype) {
        CheckFunctionReturnType(*function.functionreturntype);
    }
    CheckFunctionBody(function);
    
    scopeTree->ExitScope();
}

void TypeChecker::CheckAssociatedConstant(ConstantItem& constant) {
    // 检查关联常量类型
    auto type = CheckType(*constant.type);
    if (!type) {
        ReportError("Invalid type in associated constant");
    }
    
    // 检查常量表达式
    auto exprType = InferExpressionType(*constant.expression);
    if (exprType && !AreTypesCompatible(type, exprType)) {
        ReportError("Type mismatch in associated constant");
    }
}

void TypeChecker::CheckFunctionSignature(Function& function) {
    // 检查函数名是否冲突（在相应作用域内）
    std::string funcName = function.identifier_name;
    auto existingSymbol = scopeTree->LookupSymbolInCurrentScope(funcName);
    if (existingSymbol && existingSymbol->kind == SymbolKind::Function) {
        ReportError("Function '" + funcName + "' is already defined in this scope");
    }
}

void TypeChecker::CheckFunctionParameters(FunctionParameters& params) {
    for (const auto& param : params.functionparams) {
        // 检查参数类型
        auto paramType = CheckType(*param->type);
        if (!paramType) {
            ReportError("Invalid type in function parameter");
        }
        // 检查参数模式
        CheckPattern(*param->patternnotopalt, paramType);
    }
}

void TypeChecker::CheckFunctionReturnType(FunctionReturnType& returnType) {
    if (returnType.type) {
        auto type = CheckType(*returnType.type);
        if (!type) {
            ReportError("Invalid return type in function");
        }
    }
}

void TypeChecker::CheckFunctionBody(Function& function) {
    if (function.blockexpression) {
        if (function.functionreturntype != nullptr && function.functionreturntype->type != nullptr) {
            PushExpectedType(CheckType(*function.functionreturntype->type));
        } else {
            // 如果没有显式返回类型，设置默认的unit类型
            PushExpectedType(std::make_shared<SimpleType>("unit"));
        }
        
        function.blockexpression->accept(*this);
        PopExpectedType();
    }
}

std::shared_ptr<SemanticType> TypeChecker::CheckType(Type& typeNode) {
    if (auto typePath = dynamic_cast<TypePath*>(&typeNode)) {
        return CheckType(*typePath);
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&typeNode)) {
        return CheckType(*arrayType);
    } else if (auto refType = dynamic_cast<ReferenceType*>(&typeNode)) {
        return CheckType(*refType);
    }

    return nullptr;
}

std::shared_ptr<SemanticType> TypeChecker::CheckType(TypePath& typePath) {
    if (!typePath.simplepathsegement) {
        return nullptr;
    }
    std::string typeName = typePath.simplepathsegement->identifier;
    return ResolveType(typeName);
}

std::shared_ptr<SemanticType> TypeChecker::CheckType(ArrayType& arrayType) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&arrayType);
    if (nodeIt != nodeTypeMap.end()) {
        return nodeIt->second;
    }
    
    // 先设置一个占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("array_placeholder");
    nodeTypeMap[&arrayType] = placeholder;
    
    auto elementType = CheckType(*arrayType.type);
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
                ReportError("Array size must be an integer literal");
            }
        } else {
            // 需要更复杂的常量表达式求值
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, arrayType.expression.get());
    nodeTypeMap[&arrayType] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::CheckType(ReferenceType& refType) {
    // 检查循环依赖
    auto nodeIt = nodeTypeMap.find(&refType);
    if (nodeIt != nodeTypeMap.end()) {
        return nodeIt->second; 
    }
    
    // 先设置一个占位符防止循环
    auto placeholder = std::make_shared<SimpleType>("ref_placeholder");
    nodeTypeMap[&refType] = placeholder;
    
    // 检查引用的目标类型
    auto targetType = CheckType(*refType.type);
    if (!targetType) {
        nodeTypeMap.erase(&refType);
        return nullptr;
    }
    
    // 创建引用类型
    auto result = std::make_shared<ReferenceTypeWrapper>(targetType, refType.ismut);
    nodeTypeMap[&refType] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::ResolveType(const std::string& typeName) {
    // 修复：处理 Self 类型
    if (typeName == "Self") {
        // 在当前 impl 作用域中查找 Self 的定义
        auto selfSymbol = FindSymbol("Self");
        if (selfSymbol && selfSymbol->kind == SymbolKind::TypeAlias) {
            return selfSymbol->type;
        }
    }
    
    auto type = std::make_shared<SimpleType>(typeName);
    return type;
}

bool TypeChecker::TypeExists(const std::string& typeName) {
    auto symbol = FindSymbol(typeName);
    return symbol && (symbol->kind == SymbolKind::Struct || 
                     symbol->kind == SymbolKind::Enum || 
                     symbol->kind == SymbolKind::BuiltinType ||
                     symbol->kind == SymbolKind::TypeAlias);
}

bool TypeChecker::IsTypeVisible(const std::string& typeName) {
    // 简化实现：假设所有类型在当前作用域都可见
    return TypeExists(typeName);
}

bool TypeChecker::AreTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual) {
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
    
    // 实现隐式转换规则：
    // 1) Int 可以为 usize,isize,i32,u32
    // 2) SignedInt 可以为 i32,isize
    // 3) UnsignedInt 可以为 u32,usize
    // 4) 其余usize,isize,u32,i32 类型之间不应该有任何的隐式类型转化
    
    if (expectedStr == "Int") {
        return (actualStr == "usize" || actualStr == "isize" || actualStr == "i32" || actualStr == "u32");
    }
    
    if (actualStr == "Int") {
        return (expectedStr == "usize" || expectedStr == "isize" || expectedStr == "i32" || expectedStr == "u32");
    }
    
    if (expectedStr == "SignedInt") {
        return (actualStr == "i32" || actualStr == "isize");
    }
    
    if (actualStr == "SignedInt") {
        return (expectedStr == "i32" || expectedStr == "isize");
    }
    
    if (expectedStr == "UnsignedInt") {
        return (actualStr == "u32" || actualStr == "usize");
    }
    
    if (actualStr == "UnsignedInt") {
        return (expectedStr == "u32" || expectedStr == "usize");
    }
    
    // 特殊处理数组类型的兼容性检查
    auto expectedArray = dynamic_cast<ArrayTypeWrapper*>(expected.get());
    auto actualArray = dynamic_cast<ArrayTypeWrapper*>(actual.get());
    
    if (expectedArray && actualArray) {
        // 检查元素类型是否兼容
        if (!AreTypesCompatible(expectedArray->GetElementType(), actualArray->GetElementType())) {
            return false;
        }
        
        // 检查数组大小是否匹配
        auto expectedSizeExpr = expectedArray->GetSizeExpression();
        auto actualSizeExpr = actualArray->GetSizeExpression();
        
        if (expectedSizeExpr && actualSizeExpr) {
            int64_t expectedSize = EvaluateArraySize(*expectedSizeExpr);
            int64_t actualSize = EvaluateArraySize(*actualSizeExpr);
            
            if (expectedSize >= 0 && actualSize >= 0 && expectedSize != actualSize) {
                return false;
            }
        }
        
        return true;
    }
    
    return false;
}

bool TypeChecker::CanPerformBinaryOperation(std::shared_ptr<SemanticType> leftType, std::shared_ptr<SemanticType> rightType, Token op) {
    if (!leftType || !rightType) return false;
    
    // 对于算术运算符、比较运算符和位运算符，使用 AreTypesCompatible 检查双向兼容性
    if (op == Token::kPlus || op == Token::kMinus || op == Token::kStar || op == Token::kSlash ||
        op == Token::kEqEq || op == Token::kNe || op == Token::kLt || op == Token::kGt ||
        op == Token::kLe || op == Token::kGe ||
        op == Token::kAnd || op == Token::kOr || op == Token::kCaret ||
        op == Token::kShl || op == Token::kShr) {
        // 检查左操作数是否可以与右操作数运算，或者右操作数是否可以与左操作数运算
        return AreTypesCompatible(leftType, rightType) || AreTypesCompatible(rightType, leftType);
    }
    
    // 逻辑运算符要求布尔类型
    if (op == Token::kAndAnd || op == Token::kOrOr) {
        return leftType->tostring() == "bool" && rightType->tostring() == "bool";
    }
    
    return false;
}

std::shared_ptr<SemanticType> TypeChecker::GetBinaryOperationResultType(std::shared_ptr<SemanticType> leftType, std::shared_ptr<SemanticType> rightType, Token op) {
    if (!leftType || !rightType) return nullptr;
    
    std::string leftStr = leftType->tostring();
    std::string rightStr = rightType->tostring();
    
    // 对于比较运算符和逻辑运算符，结果是布尔类型
    if (op == Token::kEqEq || op == Token::kNe || op == Token::kLt || op == Token::kGt ||
        op == Token::kLe || op == Token::kGe || op == Token::kAndAnd || op == Token::kOrOr) {
        return std::make_shared<SimpleType>("bool");
    }
    
    // 对于算术运算符，返回限制更强的类型
    if (op == Token::kPlus || op == Token::kMinus || op == Token::kStar || op == Token::kSlash) {
        // 如果类型相同，返回该类型
        if (leftStr == rightStr) {
            return leftType;
        }
        
        // 定义类型强度顺序（限制更强的类型排在后面）
        // Int < SignedInt/UnsignedInt < 具体类型(i32, u32, isize, usize)
        
        // 如果其中一个是 Int，返回另一个类型（因为另一个更具体）
        if (leftStr == "Int") return rightType;
        if (rightStr == "Int") return leftType;
        
        // 如果其中一个是 SignedInt，返回另一个类型（如果另一个是具体类型）
        if (leftStr == "SignedInt") {
            if (rightStr == "i32" || rightStr == "isize") return rightType;
            return leftType; // 否则返回 SignedInt
        }
        if (rightStr == "SignedInt") {
            if (leftStr == "i32" || leftStr == "isize") return leftType;
            return rightType; // 否则返回 SignedInt
        }
        
        // 如果其中一个是 UnsignedInt，返回另一个类型（如果另一个是具体类型）
        if (leftStr == "UnsignedInt") {
            if (rightStr == "u32" || rightStr == "usize") return rightType;
            return leftType; // 否则返回 UnsignedInt
        }
        if (rightStr == "UnsignedInt") {
            if (leftStr == "u32" || leftStr == "usize") return leftType;
            return rightType; // 否则返回 UnsignedInt
        }
        
        // 如果都是具体类型但不相同，这种情况应该在前面的检查中被拒绝
        // 但为了安全起见，返回左操作数类型
        return leftType;
    }
    
    // 对于位运算符，返回左操作数类型
    if (op == Token::kAnd || op == Token::kOr || op == Token::kCaret || op == Token::kShl || op == Token::kShr) {
        return leftType;
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeChecker::InferExpressionType(Expression& expr) {
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
        type = InferLiteralExpressionType(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        type = InferBinaryExpressionType(*binary);
    } else if (auto call = dynamic_cast<CallExpression*>(&expr)) {
        type = InferCallExpressionType(*call);
    } else if (auto arrayExpr = dynamic_cast<ArrayExpression*>(&expr)) {
        type = InferArrayExpressionType(*arrayExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(&expr)) {
        type = InferIndexExpressionType(*indexExpr);
    } else if (auto pathExpr = dynamic_cast<PathExpression*>(&expr)) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string varName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            auto symbol = FindSymbol(varName);
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

std::shared_ptr<SemanticType> TypeChecker::InferBinaryExpressionType(BinaryExpression& expr) {
    auto leftType = InferExpressionType(*expr.leftexpression);
    auto rightType = InferExpressionType(*expr.rightexpression);
    
    if (!leftType || !rightType) {
        return nullptr;
    }

    // std::cerr << "test binary " << leftType->tostring() << " " << rightType->tostring() << " " << to_string(expr.binarytype) << "\n";
    
    // 检查是否可以进行二元运算
    if (!CanPerformBinaryOperation(leftType, rightType, expr.binarytype)) {
        ReportError("Cannot perform binary operation '" + to_string(expr.binarytype) +
                   "' between types '" + leftType->tostring() + "' and '" + rightType->tostring() + "'");
        return nullptr;
    }
    
    // 返回运算结果类型
    return GetBinaryOperationResultType(leftType, rightType, expr.binarytype);
}

std::shared_ptr<SemanticType> TypeChecker::InferCallExpressionType(CallExpression& expr) {
    auto calleeType = InferExpressionType(*expr.expression);

    // 修复：检查是否是方法调用
    if (auto fieldExpr = dynamic_cast<FieldExpression*>(expr.expression.get())) {
        return InferMethodCallType(expr, *fieldExpr);
    }
    
    // 查找函数符号
    // 如果callee是路径表达式，尝试解析为函数调用
    if (auto pathExpr = dynamic_cast<PathExpression*>(expr.expression.get())) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string functionName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            
            // 直接从作用域中查找符号
            auto symbol = FindSymbol(functionName);
            if (symbol && symbol->kind == SymbolKind::Function) {
                if (symbol->type) {
                    return symbol->type;
                }
            }
            
            // 查找函数符号
            auto functionSymbol = FindFunction(functionName);
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

std::shared_ptr<SemanticType> TypeChecker::InferMethodCallExpressionType(MethodCallExpression& expr) {
    // 方法调用：需要查找方法定义并返回其返回类型
    return std::make_shared<SimpleType>("unknown");
}

std::shared_ptr<SemanticType> TypeChecker::InferIndexExpressionType(IndexExpression& expr) {
    // 推断数组表达式的类型
    if (expr.expressionout) {
        auto arrayType = InferExpressionType(*expr.expressionout);
        if (arrayType) {
            // 如果是数组类型，返回元素类型
            if (auto arrayTypeWrapper = dynamic_cast<ArrayTypeWrapper*>(arrayType.get())) {
                return arrayTypeWrapper->GetElementType();
            }
            // 如果是引用类型，需要解引用
            else if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(arrayType.get())) {
                auto derefType = refType->getTargetType();
                if (auto innerArrayType = dynamic_cast<ArrayTypeWrapper*>(derefType.get())) {
                    return innerArrayType->GetElementType();
                }
                // 如果解引用后仍然是引用类型，继续解引用
                else if (auto innerRefType = dynamic_cast<ReferenceTypeWrapper*>(derefType.get())) {
                    auto innerDerefType = innerRefType->getTargetType();
                    if (auto innerArrayType2 = dynamic_cast<ArrayTypeWrapper*>(innerDerefType.get())) {
                        return innerArrayType2->GetElementType();
                    }
                }
            }
        }
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeChecker::InferLiteralExpressionType(LiteralExpression& expr) {
    // 根据字面量类型推断类型
    switch (expr.tokentype) {
        case Token::kINTEGER_LITERAL: {
            const std::string& literal = expr.literal;
            
            // 检查是否有显式类型后缀
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
            }
            
            // 没有显式后缀，需要根据数值大小进行推断
            std::string numStr = literal;
            
            // 移除可能的下划线
            numStr.erase(std::remove(numStr.begin(), numStr.end(), '_'), numStr.end());
            
            // 处理不同进制
            int64_t value = 0;
            if (numStr.length() >= 2 && numStr.substr(0, 2) == "0b") {
                // 二进制
                std::string binStr = numStr.substr(2);
                for (char c : binStr) {
                    if (c == '0' || c == '1') {
                        value = value * 2 + (c - '0');
                    }
                }
            } else if (numStr.length() >= 2 && numStr.substr(0, 2) == "0o") {
                // 八进制
                std::string octStr = numStr.substr(2);
                for (char c : octStr) {
                    if (c >= '0' && c <= '7') {
                        value = value * 8 + (c - '0');
                    }
                }
            } else if (numStr.length() >= 2 && numStr.substr(0, 2) == "0x") {
                // 十六进制
                std::string hexStr = numStr.substr(2);
                for (char c : hexStr) {
                    if (c >= '0' && c <= '9') {
                        value = value * 16 + (c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        value = value * 16 + (c - 'a' + 10);
                    } else if (c >= 'A' && c <= 'F') {
                        value = value * 16 + (c - 'A' + 10);
                    }
                }
            } else {
                // 十进制
                try {
                    value = std::stoll(numStr);
                } catch (const std::exception&) {
                    // 如果解析失败，默认为 Int
                    return std::make_shared<IntType>();
                }
            }
            
            // 根据数值范围推断类型
            // 负数且在 i32 范围内为 SignedInt
            if (value < 0 && value >= -2147483648LL) {
                return std::make_shared<SignedIntType>();
            }
            
            // 超出 32 位有符号整数范围为 UnsignedInt
            if (value > 2147483647LL) {
                return std::make_shared<UnsignedIntType>();
            }
            
            // 其余为 Int
            return std::make_shared<IntType>();
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

std::shared_ptr<SemanticType> TypeChecker::InferArrayExpressionType(ArrayExpression& expr) {
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
            elemType = InferLiteralExpressionType(*literal);
        } else {
            // 对于其他类型的表达式，递归推断
            elemType = InferExpressionType(*element);
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
                    elemType = InferLiteralExpressionType(*literal);
                } else {
                    // 对于其他类型的表达式，递归推断（但要避免对数组表达式递归）
                    if (auto innerArrayExpr = dynamic_cast<ArrayExpression*>(element.get())) {
                        elemType = InferArrayExpressionType(*innerArrayExpr);
                    } else {
                        elemType = InferExpressionType(*element);
                    }
                }
                
                if (!elemType) {
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
                if (!elementType) {
                    elementType = elemType;
                } else if (!AreTypesCompatible(elementType, elemType)) {
                    ReportError("Array elements must have the same type");
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
    
    // 创建大小表达式
    std::shared_ptr<Expression> sizeExpr = nullptr;
    if (expr.arrayelements) {
        if (expr.arrayelements->istwo) {
            // 对于重复元素语法，使用第二个表达式作为大小
            if (expr.arrayelements->expressions.size() >= 2) {
                sizeExpr = expr.arrayelements->expressions[1];
            }
        } else {
            // 对于逗号分隔语法，创建一个字面量表达式表示大小
            auto literal = std::make_shared<LiteralExpression>(
                std::to_string(expr.arrayelements->expressions.size()),
                Token::kINTEGER_LITERAL
            );
            sizeExpr = literal;
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, sizeExpr.get());
    nodeTypeMap[&expr] = result; // 替换占位符
    return result;
}

std::shared_ptr<SemanticType> TypeChecker::InferConstantExpressionType(Expression& expr, std::shared_ptr<SemanticType> expectedType) {
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
        result = InferLiteralExpressionType(*literalExpr);
        // 如果有期望类型，检查是否可以应用隐式类型转换
        if (expectedType && result && AreTypesCompatible(expectedType, result)) {
            // 如果类型兼容但不同，使用期望类型（应用隐式转换）
            if (result->tostring() != expectedType->tostring()) {
                result = expectedType;
            }
        }
    } else if (auto arrayExpr = dynamic_cast<ArrayExpression*>(&expr)) {
        result = InferArrayExpressionTypeWithExpected(*arrayExpr, expectedType);
    } else if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&expr)) {
        result = InferBinaryExpressionType(*binaryExpr);
        // 如果有期望类型，检查是否可以应用隐式类型转换
        if (expectedType && result && AreTypesCompatible(expectedType, result)) {
            // 如果类型兼容但不同，使用期望类型（应用隐式转换）
            if (result->tostring() != expectedType->tostring()) {
                result = expectedType;
            }
        }
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

std::shared_ptr<SemanticType> TypeChecker::InferArrayExpressionTypeWithExpected(ArrayExpression& expr, std::shared_ptr<SemanticType> expectedType) {
    if (!expectedType) {
        return InferArrayExpressionType(expr);
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
    Expression* expectedSizeExpr = nullptr;
    
    // 从期望类型中提取元素类型和大小表达式
    if (auto arrayTypeWrapper = dynamic_cast<ArrayTypeWrapper*>(expectedType.get())) {
        expectedElementType = arrayTypeWrapper->GetElementType();
        expectedSizeExpr = arrayTypeWrapper->GetSizeExpression();
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
                elemType = InferArrayExpressionTypeWithExpected(*innerArrayExpr, expectedElementType);
            } else {
                elemType = InferArrayExpressionType(*innerArrayExpr);
            }
        } else {
            // 非数组元素，使用期望类型推断
            if (expectedElementType) {
                elemType = InferConstantExpressionType(*element, expectedElementType);
            } else {
                elemType = InferExpressionType(*element);
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
                        elemType = InferArrayExpressionTypeWithExpected(*innerArrayExpr, expectedElementType);
                    } else {
                        elemType = InferArrayExpressionType(*innerArrayExpr);
                    }
                } else {
                    // 非数组元素，使用期望类型推断
                    if (expectedElementType) {
                        elemType = InferConstantExpressionType(*element, expectedElementType);
                    } else {
                        elemType = InferExpressionType(*element);
                    }
                }
                
                if (!elemType) {
                    nodeTypeMap.erase(&expr);
                    return nullptr;
                }
                if (!elementType) {
                    elementType = elemType;
                } else if (!AreTypesCompatible(elementType, elemType)) {
                    ReportError("Array elements must have the same type");
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
    
    // 对于多维数组，需要正确处理大小表达式
    std::shared_ptr<Expression> sizeExpr = nullptr;
    
    // 检查是否是内层数组（多维数组的情况）
    // 只有当元素类型是数组类型，并且当前数组表达式是某个数组的元素时，才是内层数组
    bool isInnerArray = false;
    if (expectedElementType) {
        if (auto innerArrayType = dynamic_cast<ArrayTypeWrapper*>(expectedElementType.get())) {
            // 检查当前数组表达式是否是某个数组的元素
            // 这需要检查调用栈或者上下文，这里简化处理
            // 如果期望类型是数组类型，并且当前数组表达式的元素类型与期望元素类型匹配，
            // 那么当前数组表达式可能是内层数组
            if (expr.arrayelements && !expr.arrayelements->expressions.empty()) {
                auto firstElement = expr.arrayelements->expressions[0];
                if (firstElement) {
                    auto firstElementType = InferExpressionType(*firstElement);
                    if (firstElementType && firstElementType->tostring() == innerArrayType->GetElementType()->tostring()) {
                        isInnerArray = true;
                    }
                }
            }
            
            if (isInnerArray) {
                // 这是多维数组的内层数组，使用内层数组自己的大小表达式
                auto innerSizeExpr = innerArrayType->GetSizeExpression();
                if (innerSizeExpr) {
                    sizeExpr = std::shared_ptr<Expression>(innerSizeExpr, [](Expression*){});
                }
            } else {
                // 这不是内层数组，使用期望类型中的大小表达式
                sizeExpr = std::shared_ptr<Expression>(expectedSizeExpr, [](Expression*){});
            }
        } else {
            // 这不是内层数组，使用期望类型中的大小表达式
            sizeExpr = std::shared_ptr<Expression>(expectedSizeExpr, [](Expression*){});
        }
    } else if (expectedSizeExpr) {
        // 没有期望元素类型，使用期望类型中的大小表达式
        sizeExpr = std::shared_ptr<Expression>(expectedSizeExpr, [](Expression*){});
    } else if (expr.arrayelements) {
        if (expr.arrayelements->istwo) {
            // 对于重复元素语法，使用第二个表达式作为大小
            if (expr.arrayelements->expressions.size() >= 2) {
                sizeExpr = expr.arrayelements->expressions[1];
            }
        } else {
            // 对于逗号分隔语法，创建一个字面量表达式表示大小
            auto literal = std::make_shared<LiteralExpression>(
                std::to_string(expr.arrayelements->expressions.size()),
                Token::kINTEGER_LITERAL
            );
            sizeExpr = literal;
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, sizeExpr.get());
    nodeTypeMap[&expr] = result; // 替换占位符
    return result;
}

// 错误报告
void TypeChecker::ReportError(const std::string& message) {
    std::cerr << "Type Error: " << message << std::endl;
    hasErrors = true;
}

void TypeChecker::ReportUndefinedType(const std::string& typeName, ASTNode* context) {
    std::cerr << "Undefined Type: '" << typeName << "'" << std::endl;
    hasErrors = true;
}

void TypeChecker::ReportMissingTraitImplementation(const std::string& traitName, const std::string& missingItem) {
    std::cerr << "Missing trait implementation: trait '" << traitName 
              << "' requires '" << missingItem << "'" << std::endl;
    hasErrors = true;
}

std::shared_ptr<Symbol> TypeChecker::FindSymbol(const std::string& name) {
    if (!scopeTree) {
        return nullptr;
    }
    auto  symbol= scopeTree->LookupSymbol(name);
    return symbol;
}

std::shared_ptr<FunctionSymbol> TypeChecker::FindFunction(const std::string& name) {
    auto symbol = FindSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Function) {
        return std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    }
    return nullptr;
}

std::shared_ptr<StructSymbol> TypeChecker::FindStruct(const std::string& name) {
    auto symbol = FindSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Struct) {
        return std::dynamic_pointer_cast<StructSymbol>(symbol);
    }
    return nullptr;
}

std::shared_ptr<TraitSymbol> TypeChecker::FindTrait(const std::string& name) {
    auto symbol = FindSymbol(name);
    if (symbol && symbol->kind == SymbolKind::Trait) {
        return std::dynamic_pointer_cast<TraitSymbol>(symbol);
    }
    return nullptr;
}

// 上下文管理
void TypeChecker::EnterStructContext(const std::string& structName) {
    currentStruct = structName;
}

void TypeChecker::ExitStructContext() {
    currentStruct.clear();
}

void TypeChecker::EnterTraitContext(const std::string& traitName) {
    currentTrait = traitName;
}

void TypeChecker::ExitTraitContext() {
    currentTrait.clear();
    currentTraitRequirements.clear();
}

void TypeChecker::EnterImplContext(const std::string& implName) {
    currentImpl = implName;
}

void TypeChecker::ExitImplContext() {
    currentImpl.clear();
}

// 辅助方法
void TypeChecker::PushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void TypeChecker::PopNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* TypeChecker::GetCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void TypeChecker::PushExpectedType(std::shared_ptr<SemanticType> type) {
    expectedTypeStack.push(type);
}

void TypeChecker::PopExpectedType() {
    if (!expectedTypeStack.empty()) {
        expectedTypeStack.pop();
    }
}

std::shared_ptr<SemanticType> TypeChecker::GetExpectedType() {
    return expectedTypeStack.empty() ? nullptr : expectedTypeStack.top();
}

// 在TypeChecker类中添加模式检查方法
void TypeChecker::CheckPattern(Pattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    if (auto identPattern = dynamic_cast<IdentifierPattern*>(&pattern)) {
        CheckPattern(*identPattern, expectedType);
    } else if (auto refPattern = dynamic_cast<ReferencePattern*>(&pattern)) {
        CheckPattern(*refPattern, expectedType);
    }
}

void TypeChecker::CheckPattern(IdentifierPattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    std::string varName = pattern.identifier;
    auto varSymbol = std::make_shared<Symbol>(
        varName,
        SymbolKind::Variable,
        expectedType,
        pattern.hasmut,
        &pattern
    );
    
    if (!scopeTree->InsertSymbol(varName, varSymbol)) {
        ReportError("Variable '" + varName + "' is already defined in this scope");
    }
}


void TypeChecker::CheckPattern(ReferencePattern& pattern, std::shared_ptr<SemanticType> expectedType) {
    // 修复：暂时跳过引用类型的检查，避免段错误
    if (!expectedType) {
        ReportError("Reference pattern requires reference type");
        return;
    }
    
    // 检查内部模式
    std::shared_ptr<SemanticType> innerType;
    if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(expectedType.get())) {
        innerType = refType->getTargetType();
    } else {
        // 从字符串中解析引用类型
        std::string typeStr = expectedType->tostring();
        if (typeStr.find("&") == 0) {
            innerType = std::make_shared<SimpleType>(typeStr.substr(1));
        } else {
            innerType = expectedType;
        }
    }
    
    if (innerType && pattern.pattern) {
        CheckPattern(*pattern.pattern, innerType);
    }
}

// 可变性检查实现
void TypeChecker::CheckAssignmentMutability(Expression& lhs) {
    if (auto pathExpr = dynamic_cast<PathExpression*>(&lhs)) {
        CheckVariableMutability(*pathExpr);
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(&lhs)) {
        CheckFieldMutability(*fieldExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(&lhs)) {
        CheckIndexMutability(*indexExpr);
    }
}

void TypeChecker::CheckVariableMutability(PathExpression& pathExpr) {
    if (!pathExpr.simplepath) return;
    
    std::string varName;
    if (!pathExpr.simplepath || pathExpr.simplepath->simplepathsegements.empty()) {
        return;
    }
    varName = pathExpr.simplepath->simplepathsegements[0]->identifier;
    
    if (varName.empty()) return;
    auto symbol = scopeTree->LookupSymbol(varName);
    if (!symbol) {
        ReportError("Undefined variable: " + varName);
        return;
    }
    
    // 特殊处理：如果变量是引用类型，检查引用本身是否可变
    bool isMutable = symbol->ismutable;
    if (symbol->type) {
        if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(symbol->type.get())) {
            // 对于引用类型，可变性由引用本身决定
            isMutable = refType->GetIsMutable();
        }
    }
    
    if (!isMutable) {
        ReportMutabilityError(varName, "variable", &pathExpr);
    }
}

void TypeChecker::CheckFieldMutability(FieldExpression& fieldExpr) {
    // 首先检查基础表达式的可变性
    CheckAssignmentMutability(*fieldExpr.expression);
    
    // 对于结构体字段，我们需要检查结构体实例是否可变
    // 递归检查基础表达式，确保结构体实例本身是可变的
    if (auto pathExpr = dynamic_cast<PathExpression*>(fieldExpr.expression.get())) {
        CheckVariableMutability(*pathExpr);
    } else if (auto nestedFieldExpr = dynamic_cast<FieldExpression*>(fieldExpr.expression.get())) {
        CheckFieldMutability(*nestedFieldExpr);
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(fieldExpr.expression.get())) {
        CheckIndexMutability(*indexExpr);
    }
}

void TypeChecker::CheckIndexMutability(IndexExpression& indexExpr) {
    // 对于数组索引，我们需要检查数组本身是否可变
    // 直接检查基础表达式的可变性，确保数组变量是可变的
    
    if (auto pathExpr = dynamic_cast<PathExpression*>(indexExpr.expressionout.get())) {
        CheckVariableMutability(*pathExpr);
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(indexExpr.expressionout.get())) {
        CheckFieldMutability(*fieldExpr);
    } else if (auto nestedIndexExpr = dynamic_cast<IndexExpression*>(indexExpr.expressionout.get())) {
        // 递归处理嵌套的索引表达式，如 arr[1][2]
        CheckIndexMutability(*nestedIndexExpr);
    } else {
        // 递归检查更复杂的表达式
        CheckAssignmentMutability(*indexExpr.expressionout);
    }
}

void TypeChecker::ReportMutabilityError(const std::string& name, const std::string& errorType, ASTNode* context) {
    std::cerr << "Mutability Error: Cannot modify " << errorType << " '" << name
              << "' as it is not declared as mutable" << std::endl;
    hasErrors = true;
}

void TypeChecker::visit(AssignmentExpression& node) {
    PushNode(node);
    
    // 检查左值的可变性
    if (node.leftexpression) {
        CheckAssignmentMutability(*node.leftexpression);
    }
    
    PopNode();
}

void TypeChecker::visit(CompoundAssignmentExpression& node) {
    PushNode(node);
    
    // 检查左值的可变性
    if (node.leftexpression) {
        CheckAssignmentMutability(*node.leftexpression);
    }
    // 检查右值的类型
    if (node.rightexpression) {
        auto rightType = InferExpressionType(*node.rightexpression);
    }
    
    PopNode();
}

void TypeChecker::visit(IndexExpression& node) {
    PushNode(node);
    
    if (node.expressionout) {
        node.expressionout->accept(*this);
    }
    if (node.expressionin) {
        node.expressionin->accept(*this);
        
        // 检查索引类型必须是整数
        auto indexType = InferExpressionType(*node.expressionin);
        if (indexType && indexType->tostring() != "i32" && indexType->tostring() != "usize") {
            ReportError("Array index must be of integer type, found " + indexType->tostring());
        }
    }
    
    // 推断索引表达式的类型
    if (node.expressionout) {
        auto arrayType = InferExpressionType(*node.expressionout);
        if (arrayType) {
            // 提取元素类型
            if (auto arrayTypeWrapper = dynamic_cast<ArrayTypeWrapper*>(arrayType.get())) {
                auto elementType = arrayTypeWrapper->GetElementType();
                nodeTypeMap[&node] = elementType;
            } else if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(arrayType.get())) {
                // 如果是引用类型，需要解引用
                auto derefType = refType->getTargetType();
                if (auto innerArrayType = dynamic_cast<ArrayTypeWrapper*>(derefType.get())) {
                    auto elementType = innerArrayType->GetElementType();
                    nodeTypeMap[&node] = elementType;
                } else {
                    ReportError("Cannot index into non-array type: " + arrayType->tostring());
                }
            } else {
                ReportError("Cannot index into non-array type: " + arrayType->tostring());
            }
        }
    }
    
    PopNode();
}

void TypeChecker::visit(FieldExpression& node) {
    PushNode(node);
    if (node.expression) {
        node.expression->accept(*this);
    }
    PopNode();
}

void TypeChecker::visit(PathExpression& node) {
    PushNode(node);
    
    // 路径表达式通常不需要特殊处理，主要是符号查找
    // 实际的类型推断在使用时进行
    
    PopNode();
}

void TypeChecker::visit(ExpressionStatement& node) {
    PushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    
    PopNode();
}

void TypeChecker::visit(LetStatement& node) {
    PushNode(node);
    
    std::shared_ptr<SemanticType> declaredType = nullptr;
    if (node.type) {
        declaredType = CheckType(*node.type);
        if (!declaredType) {
            ReportError("Invalid type in let statement");
            PopNode();
            return;
        }
    }
    
    // 检查初始化表达式
    if (node.expression) {
        if (declaredType) {
            // 对于有明确类型声明的let语句，进行完整的类型检查，包括数组长度验证
            // 推断初始化表达式的类型
            std::shared_ptr<SemanticType> initType = nullptr;
            
            // 如果是数组表达式，使用带期望类型的推断方法
            if (auto arrayExpr = dynamic_cast<ArrayExpression*>(node.expression.get())) {
                initType = InferArrayExpressionTypeWithExpected(*arrayExpr, declaredType);
            } else {
                initType = InferExpressionType(*node.expression);
            }
            
            // 如果推断失败，但有声明类型，使用声明类型
            if (!initType) {
                initType = declaredType;
                nodeTypeMap[node.expression.get()] = declaredType;
            } else if (auto ifExpr = dynamic_cast<IfExpression*>(node.expression.get())) {
                // 对于 if 表达式，如果推断失败但有声明类型，使用声明类型
                if (!initType || initType->tostring() == "type_error") {
                    nodeTypeMap[node.expression.get()] = declaredType;
                    initType = declaredType;
                }
            }
            
            // 检查类型兼容性
            if (!AreTypesCompatible(declaredType, initType)) {
                ReportError("Type mismatch in let statement: expected '" + declaredType->tostring() +
                           "', found '" + initType->tostring() + "'");
                PopNode();
                return;
            }
            // 特殊处理数组类型：进行大小验证
            if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(declaredType.get())) {
                CheckArraySizeMatchForAnyExpression(*arrayType, *node.expression);
            }
        } else {
            // 没有声明类型，使用普通推断
            auto initType = InferExpressionType(*node.expression);
            if (!initType) {
                // 添加调试信息
                if (auto pattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
                    ReportError("Unable to infer type for let statement for variable: " + pattern->identifier);
                } else {
                    ReportError("Unable to infer type for let statement");
                }
                PopNode();
                return;
            }
        }
    }
    
    // 检查模式并注册变量
    if (node.patternnotopalt) {
        std::shared_ptr<SemanticType> varType = declaredType ? declaredType : std::make_shared<SimpleType>("inferred");
        CheckPattern(*node.patternnotopalt, varType);
    }
    PopNode();
}

// 数组大小验证实现
void TypeChecker::CheckArraySizeMatch(ArrayTypeWrapper& declaredType, ArrayExpression& arrayExpr) {
    // 获取声明的数组大小
    auto sizeExpr = declaredType.GetSizeExpression();
    if (!sizeExpr) {
        return;
    }
    
    int64_t declaredSize = EvaluateArraySize(*sizeExpr);
    if (declaredSize < 0) {
        ReportError("Invalid array size expression");
        return;
    }
    
    // 获取初始化数组的实际大小
    if (!arrayExpr.arrayelements) {
        ReportError("Array expression has no elements");
        return;
    }
    
    int64_t actualSize;
    // 检查是否是重复元素语法 [value; count]
    if (arrayExpr.arrayelements->istwo) {
        // 对于重复元素语法，第二个表达式是重复次数
        if (arrayExpr.arrayelements->expressions.size() < 2) {
            ReportError("Invalid repeated array expression: missing count");
            return;
        }
        
        // 获取重复次数
        const auto& countExpr = arrayExpr.arrayelements->expressions[1];
        actualSize = EvaluateArraySize(*countExpr);
        if (actualSize < 0) {
            ReportError("Invalid array count expression");
            return;
        }
    } else {
        // 对于逗号分隔语法，实际大小就是表达式数量
        actualSize = arrayExpr.arrayelements->expressions.size();
    }
    
    // 比较大小
    if (declaredSize != actualSize) {
        ReportError("Array size mismatch: declared size " + std::to_string(declaredSize) +
                   ", but initializer has " + std::to_string(actualSize) + " elements");
        return;
    }
    
    // 对于多维数组，递归检查内部数组的大小
    auto elementType = declaredType.GetElementType();
    if (auto innerArrayType = dynamic_cast<ArrayTypeWrapper*>(elementType.get())) {
        // 这是一个多维数组，需要检查每个内部数组的大小
        for (const auto& element : arrayExpr.arrayelements->expressions) {
            if (auto innerArrayExpr = dynamic_cast<ArrayExpression*>(element.get())) {
                // 如果元素是数组表达式，递归检查
                CheckArraySizeMatch(*innerArrayType, *innerArrayExpr);
            } else {
                // 如果元素不是数组表达式，我们跳过递归检查
                // 这允许使用重复元素语法或其他表达式来初始化多维数组
                // 只要它们的类型与期望的内部数组类型兼容
                // 类型兼容性已经在之前的AreTypesCompatible检查中验证过了
            }
        }
    }
}

void TypeChecker::CheckArraySizeMatchForAnyExpression(ArrayTypeWrapper& declaredType, Expression& initExpr) {
    // 获取声明的数组大小
    auto sizeExpr = declaredType.GetSizeExpression();
    if (!sizeExpr) {
        return;
    }
    
    int64_t declaredSize = EvaluateArraySize(*sizeExpr);
    if (declaredSize < 0) {
        ReportError("Invalid array size expression in declaration");
        return;
    }
    
    // 获取初始化表达式的实际大小
    int64_t actualSize = -1;
    
    if (auto arrayExpr = dynamic_cast<ArrayExpression*>(&initExpr)) {
        // 如果是数组表达式，复用原有的 CheckArraySizeMatch 函数
        // 这样可以保留原有实现对于多维数组的递归检查
        CheckArraySizeMatch(declaredType, *arrayExpr);
        return;
    } else if (auto pathExpr = dynamic_cast<PathExpression*>(&initExpr)) {
        // 如果是路径表达式（变量引用），获取变量的类型
        std::string varName;
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            varName = pathExpr->simplepath->simplepathsegements[0]->identifier;
        }
        
        if (varName.empty()) {
            ReportError("Invalid variable name in array initialization");
            return;
        }
        
        auto symbol = FindSymbol(varName);
        if (!symbol) {
            ReportError("Undefined variable: " + varName);
            return;
        }
        
        if (!symbol->type) {
            ReportError("Variable '" + varName + "' has no type information");
            return;
        }
        
        // 检查变量类型是否为数组
        if (auto varArrayType = dynamic_cast<ArrayTypeWrapper*>(symbol->type.get())) {
            auto varSizeExpr = varArrayType->GetSizeExpression();
            if (varSizeExpr) {
                actualSize = EvaluateArraySize(*varSizeExpr);
                if (actualSize < 0) {
                    ReportError("Cannot evaluate size of array variable: " + varName);
                    return;
                }
            } else {
                ReportError("Array variable '" + varName + "' has no size information");
                return;
            }
        } else {
            ReportError("Variable '" + varName + "' is not an array type");
            return;
        }
    } else {
        // 对于其他类型的表达式，目前不支持数组大小检查
        // 但我们可以尝试推断表达式类型，如果是数组类型则检查大小
        auto exprType = InferExpressionType(initExpr);
        if (exprType) {
            auto exprArrayType = dynamic_cast<ArrayTypeWrapper*>(exprType.get());
            if (exprArrayType) {
                auto exprSizeExpr = exprArrayType->GetSizeExpression();
                if (exprSizeExpr) {
                    actualSize = EvaluateArraySize(*exprSizeExpr);
                    if (actualSize < 0) {
                        ReportError("Cannot evaluate size of array expression");
                        return;
                    }
                }
            } else {
                // 不是数组表达式，无法进行大小检查
                return;
            }
        }
    }
    
    // 比较大小
    if (actualSize >= 0 && declaredSize != actualSize) {
        ReportError("Array size mismatch: declared size " + std::to_string(declaredSize) +
                   ", but initializer has " + std::to_string(actualSize) + " elements");
        return;
    }
}

int64_t TypeChecker::EvaluateArraySize(Expression& sizeExpr) {
    if (auto literal = dynamic_cast<LiteralExpression*>(&sizeExpr)) {
        if (literal->tokentype == Token::kINTEGER_LITERAL) {
            try {
                std::string numStr = literal->literal;
                if (numStr.length() >= 3) {
                    std::string suffix = numStr.substr(numStr.length() - 3);
                    if (suffix == "u32" || suffix == "i32") {
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
                ReportError("Invalid integer literal in array size: " + literal->literal);
                return -1;
            }
        } else {
            ReportError("Array size must be an integer literal");
            return -1;
        }
    }
    
    if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&sizeExpr)) {
        if (binaryExpr->binarytype == Token::kPlus ||
            binaryExpr->binarytype == Token::kMinus ||
            binaryExpr->binarytype == Token::kStar ||
            binaryExpr->binarytype == Token::kSlash) {
            
            int64_t leftValue = EvaluateArraySize(*binaryExpr->leftexpression);
            int64_t rightValue = EvaluateArraySize(*binaryExpr->rightexpression);
            
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
                        ReportError("Division by zero in array size expression");
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
                auto constValue = constantEvaluator->GetConstantValue(constName);
                if (constValue) {
                    // 尝试将常量值转换为整数
                    if (auto intConst = dynamic_cast<IntConstant*>(constValue.get())) {
                        return intConst->getValue();
                    } else {
                        ReportError("Constant '" + constName + "' is not an integer constant");
                        return -1;
                    }
                }
            }
            
            // 查找常量符号（作为备用方案）
            auto symbol = FindSymbol(constName);
            if (symbol && symbol->kind == SymbolKind::Constant) {
                ReportError("Cannot evaluate constant '" + constName + "' at compile time");
                return -1;
            }
        }
    }
    
    ReportError("Complex array size expressions not supported");
    return -1;
}

void TypeChecker::visit(BlockExpression& node) {
    PushNode(node);
    
    // 进入新的作用域
    scopeTree->EnterScope(Scope::ScopeType::Block, &node);
    for (const auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    // 退出作用域
    scopeTree->ExitScope();
    
    // 推断块表达式的类型（基于最后一条语句）
    if (!node.statements.empty()) {
        auto lastStmt = node.statements.back();
        if (auto exprStmt = dynamic_cast<ExpressionStatement*>(lastStmt.get())) {
            if (exprStmt->astnode) {
                auto lastExprType = InferExpressionType(*exprStmt->astnode);
                if (lastExprType) {
                    nodeTypeMap[&node] = lastExprType;
                }
            }
        } else {
            // 如果最后一条语句不是表达式语句，块表达式类型为 unit
            nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
        }
    } else {
        // 空块表达式的类型为 unit
        nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
    }
    
    PopNode();
}

void TypeChecker::visit(IfExpression& node) {
    PushNode(node);
    
    // 检查条件表达式
    if (node.conditions && node.conditions->expression) {
        node.conditions->expression->accept(*this);
        auto condType = InferExpressionType(*node.conditions->expression);
        // 条件表达式必须是布尔类型
        if (condType && condType->tostring() != "bool") {
            ReportError("If condition must be of type bool, found " + condType->tostring());
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
        auto ifType = InferExpressionType(*node.ifblockexpression);
        auto elseType = InferExpressionType(*node.elseexpression);
        if (ifType && elseType) {
            // 如果两个分支都不是!类型，检查类型兼容性
            if (ifType->tostring() != "!" && elseType->tostring() != "!") {
                if (!AreTypesCompatible(ifType, elseType)) {
                    ReportError("If expression branches have incompatible types: " +
                               ifType->tostring() + " vs " + elseType->tostring());
                }
            }
        }
    }
    
    PopNode();
}


void TypeChecker::visit(ReturnExpression& node) {
    PushNode(node);
    
    // 检查返回表达式类型与函数返回类型是否兼容
    if (node.expression) {
        auto exprType = InferExpressionType(*node.expression);
        
        if (exprType) {
            // 查找当前函数的返回类型
            std::stack<ASTNode*> tempStack = nodeStack;
            std::shared_ptr<SemanticType> expectedReturnType = nullptr;
            
            while (!tempStack.empty()) {
                auto topNode = tempStack.top();
                tempStack.pop();
                if (auto funcNode = dynamic_cast<Function*>(topNode)) {
                    if (funcNode->functionreturntype && funcNode->functionreturntype->type) {
                        expectedReturnType = CheckType(*funcNode->functionreturntype->type);
                    }
                    break;
                }
            }
            
            // 如果找到了期望的返回类型，检查类型兼容性
            if (expectedReturnType) {
                // 如果函数返回类型不是 unit 类型，进行类型检查
                if (expectedReturnType->tostring() != "()") {
                    if (!AreTypesCompatible(expectedReturnType, exprType)) {
                        ReportError("Type mismatch in return expression: expected '" + expectedReturnType->tostring() +
                                   "', found '" + exprType->tostring() + "'");
                    }
                }
            }
        }
    }
    
    PopNode();
}

void TypeChecker::visit(InfiniteLoopExpression& node) {
    PushNode(node);
    
    // 访问循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    PopNode();
}

// 新增方法调用类型推断
std::shared_ptr<SemanticType> TypeChecker::InferMethodCallType(CallExpression& expr, FieldExpression& fieldExpr) {
    // 获取接收者类型
    auto receiverType = InferExpressionType(*fieldExpr.expression);
    if (!receiverType) {
        return nullptr;
    }
    
    std::string methodName = fieldExpr.identifier;
    std::string receiverTypeName = receiverType->tostring();
    
    // 检查是否是内置方法
    if (IsBuiltinMethodCall(receiverTypeName, methodName)) {
        return GetBuiltinMethodReturnType(receiverTypeName, methodName);
    }
    
    // 在结构体中查找方法
    if (auto structSymbol = FindStruct(receiverTypeName)) {
        for (const auto& method : structSymbol->methods) {
            if (method->name == methodName) {
                return method->returntype;
            }
        }
    }
    
    ReportError("Method '" + methodName + "' not found for type '" + receiverTypeName + "'");
    return nullptr;
}

// 检查是否是内置方法调用
bool TypeChecker::IsBuiltinMethodCall(const std::string& receiverType, const std::string& methodName) {
    // u32 和 usize 类型的 to_string 方法
    if ((receiverType == "u32" || receiverType == "usize") && methodName == "to_string") {
        return true;
    }
    
    // String 类型的 as_str 和 as_mut_str 方法
    if (receiverType == "String" && (methodName == "as_str" || methodName == "as_mut_str")) {
        return true;
    }
    
    // 数组类型的 len 方法
    if (receiverType.find("[") == 0 && receiverType.find("]") == receiverType.length() - 1 && methodName == "len") {
        return true;
    }
    
    return false;
}

// 获取内置方法的返回类型
std::shared_ptr<SemanticType> TypeChecker::GetBuiltinMethodReturnType(const std::string& receiverType, const std::string& methodName) {
    // u32 和 usize 类型的 to_string 方法返回 String
    if ((receiverType == "u32" || receiverType == "usize") && methodName == "to_string") {
        return std::make_shared<SimpleType>("String");
    }
    
    // String 类型的 as_str 和 as_mut_str 方法返回 &str 和 &mut str
    if (receiverType == "String") {
        if (methodName == "as_str") {
            return std::make_shared<ReferenceTypeWrapper>(std::make_shared<SimpleType>("str"), false);
        } else if (methodName == "as_mut_str") {
            return std::make_shared<ReferenceTypeWrapper>(std::make_shared<SimpleType>("str"), true);
        }
    }
    
    // 数组类型的 len 方法返回 usize
    if (receiverType.find("[") == 0 && receiverType.find("]") == receiverType.length() - 1 && methodName == "len") {
        return std::make_shared<SimpleType>("usize");
    }
    
    return nullptr;
}

void TypeChecker::visit(PredicateLoopExpression& node) {
    PushNode(node);
    
    // 访问条件表达式
    if (node.conditions && node.conditions->expression) {
        node.conditions->expression->accept(*this);
    }
    
    // 访问循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    PopNode();
}

void TypeChecker::visit(BinaryExpression& node) {
    PushNode(node);
    
    // 访问左表达式和右表达式
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    
    PopNode();
}

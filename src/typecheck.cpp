#include "typecheck.hpp"
#include "lexer.hpp"
#include "typewrapper.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <memory>
#include <utility>
#include <unordered_map>
#include <algorithm>
#include <typeinfo>

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
        // 检查是否是 Item（包含常量定义的情况）
        if (auto item = dynamic_cast<Item*>(node.astnode.get())) {
            // 如果是 Item，需要访问其内部的 item
            if (item->item) {
                item->item->accept(*this);
            }
        } else {
            // 其他类型的语句，直接访问
            node.astnode->accept(*this);
        }
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
        // 获取函数声明的返回类型
        std::shared_ptr<SemanticType> declaredReturnType = nullptr;
        if (function.functionreturntype != nullptr && function.functionreturntype->type != nullptr) {
            declaredReturnType = CheckType(*function.functionreturntype->type);
        } else {
            // 如果没有显式返回类型，设置默认的unit类型
            declaredReturnType = std::make_shared<SimpleType>("()");
        }
        
        PushExpectedType(declaredReturnType);
        
        // 访问函数体
        function.blockexpression->accept(*this);
        
        // 分析返回语句
        ReturnAnalysisResult returnAnalysis = AnalyzeReturnStatements(*function.blockexpression);
        
        if (returnAnalysis.hasCertainReturn) {
            // 如果有确定执行的返回语句，只需要检查返回语句的类型是否匹配
            if (returnAnalysis.certainReturnType && !AreTypesCompatible(declaredReturnType, returnAnalysis.certainReturnType)) {
                ReportError("Function '" + function.identifier_name + "' return type mismatch: expected '" +
                           declaredReturnType->tostring() + "', found '" + returnAnalysis.certainReturnType->tostring() + "'");
            }
        } else {
            // 如果没有确定执行的返回语句，需要像现有的一样对于函数的尾表达式类型进行匹配分析
            auto bodyTypeIt = nodeTypeMap.find(function.blockexpression.get());
            std::shared_ptr<SemanticType> bodyType = nullptr;
            if (bodyTypeIt != nodeTypeMap.end()) {
                bodyType = bodyTypeIt->second;
            }

            
            if (bodyType && declaredReturnType) {
                // 检查函数体类型与声明的返回类型是否匹配
                // 注意：如果函数体类型是 !（never），说明函数总是发散，不需要检查
                if (bodyType->tostring() != "!") {
                    // 如果声明的返回类型不是 unit，或者函数体类型不是 unit，进行类型检查
                    if (declaredReturnType->tostring() != "()" || bodyType->tostring() != "()") {
                        if (!AreTypesCompatible(declaredReturnType, bodyType)) {
                            ReportError("Function '" + function.identifier_name + "' return type mismatch: expected '" +
                                       declaredReturnType->tostring() + "', found '" + bodyType->tostring() + "'");
                        }
                    }
                }
            }
        }
        
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
    } else if (auto unitType = dynamic_cast<UnitType*>(&typeNode)) {
        // 特殊处理 UnitType，直接创建 SimpleType("()")
        return std::make_shared<SimpleType>("()");
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
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, arrayType.expression.get(), constantEvaluator.get());
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
    
    // 特殊处理 unit 类型 ()，确保它总是被认为是有效的
    if (typeName == "()") {
        return std::make_shared<SimpleType>("()");
    }
    
    auto type = std::make_shared<SimpleType>(typeName);
    return type;
}

bool TypeChecker::TypeExists(const std::string& typeName) {
    // 特殊处理 unit 类型 ()，它总是被认为是有效的
    if (typeName == "()") {
        return true;
    }
    
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
        // 检查元素类型是否兼容（支持双向隐式转换）
        // 不仅要检查 expected -> actual，还要检查 actual -> expected
        if (!AreTypesCompatible(expectedArray->GetElementType(), actualArray->GetElementType()) &&
            !AreTypesCompatible(actualArray->GetElementType(), expectedArray->GetElementType())) {
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
        op == Token::kPercent || 
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
    if (op == Token::kPlus || op == Token::kMinus || op == Token::kStar || op == Token::kSlash || op == Token::kPercent) {
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
                // 如果找不到符号或符号没有类型信息，检查是否是常量
                if (constantEvaluator) {
                    auto constValue = constantEvaluator->GetConstantValue(varName);
                    if (constValue) {
                        // 如果是常量，根据常量值推断类型
                        if (dynamic_cast<IntConstant*>(constValue.get())) {
                            type = std::make_shared<SimpleType>("usize");
                        } else if (dynamic_cast<BoolConstant*>(constValue.get())) {
                            type = std::make_shared<SimpleType>("bool");
                        } else if (dynamic_cast<StringConstant*>(constValue.get())) {
                            type = std::make_shared<SimpleType>("str");
                        } else if (dynamic_cast<CharConstant*>(constValue.get())) {
                            type = std::make_shared<SimpleType>("char");
                        } else {
                            type = std::make_shared<SimpleType>("unknown");
                        }
                    } else {
                        // 如果找不到符号或常量，直接报错
                        ReportError("Undefined variable: " + varName);
                        type = nullptr;
                    }
                } else {
                    // 如果找不到符号或符号没有类型信息，直接报错
                    ReportError("Undefined variable: " + varName);
                    type = nullptr;
                }
            }
        } else {
            type = nullptr;
        }
    } else if (auto groupedExpr = dynamic_cast<GroupedExpression*>(&expr)) {
        // GroupedExpression 的类型为其内部 expression 的类型
        if (groupedExpr->expression) {
            type = InferExpressionType(*groupedExpr->expression);
        } else {
            type = nullptr;
        }
    } else if (auto unaryExpr = dynamic_cast<UnaryExpression*>(&expr)) {
        // UnaryExpression 的类型为其内部 expression 的类型
        if (unaryExpr->expression) {
            type = InferExpressionType(*unaryExpr->expression);
        } else {
            type = nullptr;
        }
    } else if (auto ifExpr = dynamic_cast<IfExpression*>(&expr)) {
        // IfExpression 类型推断
        type = InferIfExpressionType(*ifExpr);
    } else if (auto loopExpr = dynamic_cast<InfiniteLoopExpression*>(&expr)) {
        // InfiniteLoopExpression (loop) 类型推断
        type = InferInfiniteLoopExpressionType(*loopExpr);
    } else if (auto whileExpr = dynamic_cast<PredicateLoopExpression*>(&expr)) {
        // PredicateLoopExpression (while) 类型推断
        type = InferPredicateLoopExpressionType(*whileExpr);
    } else if (auto blockExpr = dynamic_cast<BlockExpression*>(&expr)) {
        // BlockExpression 类型推断
        type = InferBlockExpressionType(*blockExpr);
    } else if (auto typeCastExpr = dynamic_cast<TypeCastExpression*>(&expr)) {
        // TypeCastExpression 类型推断
        type = InferTypeCastExpressionType(*typeCastExpr);
    } else if (auto structExpr = dynamic_cast<StructExpression*>(&expr)) {
        // StructExpression 类型推断
        // 根据任务描述，StructExpression 的类型为其对应的 struct 的类型，
        // 对应其存储的 pathexpression.simplepath.simplepathsegements[0] 存储的 identifier（即结构体名）
        if (structExpr->pathexpression &&
            structExpr->pathexpression->simplepath &&
            !structExpr->pathexpression->simplepath->simplepathsegements.empty()) {
            std::string structName = structExpr->pathexpression->simplepath->simplepathsegements[0]->identifier;
            type = std::make_shared<SimpleType>(structName);
        } else {
            type = nullptr;
        }
    } else {
        // 未知表达式类型
        type = nullptr;
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
        // 如果类型推断失败，错误已经在 InferExpressionType 中报告了
        return nullptr;
    }
    
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
            
            // 如果找不到函数符号，直接报错
            ReportError("Undefined function: " + functionName);
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
            return std::make_shared<ReferenceTypeWrapper>(std::make_shared<SimpleType>("str"), false);
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
                        // 对于内层数组，需要独立推断其类型，不受外层数组影响
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
                    // 创建元素类型的深拷贝，避免共享
                    if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(elemType.get())) {
                        // 如果是数组类型，创建新的数组类型包装器
                        auto newElementType = arrayType->GetElementType();
                        auto newSizeExpr = arrayType->GetSizeExpression();
                        elementType = std::make_shared<ArrayTypeWrapper>(newElementType, newSizeExpr, constantEvaluator.get());
                    } else {
                        elementType = elemType;
                    }
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
            // 注意：这里必须使用当前数组表达式的元素数量，而不是期望类型的大小
            auto literal = std::make_shared<LiteralExpression>(
                std::to_string(expr.arrayelements->expressions.size()),
                Token::kINTEGER_LITERAL
            );
            sizeExpr = literal;
        }
    }
    
    // 创建数组类型，确保大小表达式是独立的
    // 创建一个新的LiteralExpression来避免共享指针
    Expression* independentSizeExpr = nullptr;
    if (sizeExpr) {
        if (auto literal = dynamic_cast<LiteralExpression*>(sizeExpr.get())) {
            independentSizeExpr = new LiteralExpression(literal->literal, literal->tokentype);
        } else {
            independentSizeExpr = sizeExpr.get();
        }
    }
    
    auto result = std::make_shared<ArrayTypeWrapper>(elementType, independentSizeExpr);
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
                    // 对于内层数组，不要传递期望类型，让它独立推断
                    // 这样可以避免大小表达式被错误地覆盖
                    elemType = InferArrayExpressionType(*innerArrayExpr);
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
    auto symbol = scopeTree->LookupSymbol(name);
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
    
    // 检查右值的类型
    if (node.rightexpression) {
        auto rightType = InferExpressionType(*node.rightexpression);
        // 类型推断失败时，错误已经在 InferExpressionType 中报告了
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

void TypeChecker::visit(StructExpression& node) {
    PushNode(node);
    
    // 访问路径表达式
    if (node.pathexpression) {
        node.pathexpression->accept(*this);
    }
    
    // 访问结构体信息（字段或基础结构体）
    if (node.structinfo) {
        node.structinfo->accept(*this);
    }
    
    // 推断 StructExpression 的类型并设置到 nodeTypeMap
    auto structType = InferExpressionType(node);
    if (structType) {
        nodeTypeMap[&node] = structType;
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
    
    // 推断块表达式的类型（按照语义规范优先级）
    // 1. 首先检查是否有尾表达式（expressionwithoutblock）
    if (node.expressionwithoutblock) {
        // 在作用域内访问尾表达式并推断其类型
        node.expressionwithoutblock->accept(*this);
        auto tailExprType = InferExpressionType(*node.expressionwithoutblock);
        
        if (tailExprType) {
            nodeTypeMap[&node] = tailExprType;
        } else {
            // 如果尾表达式类型推断失败，默认为 unit
            nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
        }
    } else if (!node.statements.empty()) {
        // 2. 如果没有尾表达式，检查最后一条语句是否是不带分号的表达式语句
        auto lastStmt = node.statements.back();
        // Statement 使用组合模式，需要检查其 astnode 的类型
        if (auto exprStmt = dynamic_cast<ExpressionStatement*>(lastStmt->astnode.get())) {
            // 检查是否为不带分号的表达式语句（expressionstatement without semi）
            if (!exprStmt->hassemi && exprStmt->astnode) {
                // 不带分号的表达式语句，作为尾表达式处理
                auto lastExprType = InferExpressionType(*exprStmt->astnode);
                if (lastExprType) {
                    nodeTypeMap[&node] = lastExprType;
                } else {
                    // 如果表达式类型推断失败，默认为 unit
                    nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
                }
            } else {
                // 带分号的表达式语句或其他情况，块表达式类型为 unit
                nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
            }
        } else {
            // 如果最后一条语句不是表达式语句，块表达式类型为 unit
            nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
        }
    } else {
        // 3. 空块表达式的类型为 unit
        nodeTypeMap[&node] = std::make_shared<SimpleType>("()");
    }
    
    
    // 退出作用域
    scopeTree->ExitScope();
    
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
    }
    
    // 使用 InferIfExpressionType 函数来推断和设置类型（无论是否有 else 分支）
    auto ifExprType = InferIfExpressionType(node);
    if (ifExprType) {
        nodeTypeMap[&node] = ifExprType;
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
                    } else {
                        // 如果没有显式返回类型，默认为 unit
                        expectedReturnType = std::make_shared<SimpleType>("()");
                    }
                    break;
                }
            }
            
            // 如果找到了期望的返回类型，检查类型兼容性
            if (expectedReturnType) {
                // 对于所有返回类型（包括 unit），都进行类型检查
                // 但如果是 unit 类型且返回表达式也是 unit，则跳过检查
                if (expectedReturnType->tostring() != "()" || exprType->tostring() != "()") {
                    if (!AreTypesCompatible(expectedReturnType, exprType)) {
                        ReportError("Type mismatch in return expression: expected '" + expectedReturnType->tostring() +
                                   "', found '" + exprType->tostring() + "'");
                    }
                }
            }
        }
    } else {
        // 没有表达式的 return 语句，检查函数是否应该返回 unit
        std::stack<ASTNode*> tempStack = nodeStack;
        std::shared_ptr<SemanticType> expectedReturnType = nullptr;
        
        while (!tempStack.empty()) {
            auto topNode = tempStack.top();
            tempStack.pop();
            if (auto funcNode = dynamic_cast<Function*>(topNode)) {
                if (funcNode->functionreturntype && funcNode->functionreturntype->type) {
                    expectedReturnType = CheckType(*funcNode->functionreturntype->type);
                } else {
                    // 如果没有显式返回类型，默认为 unit
                    expectedReturnType = std::make_shared<SimpleType>("()");
                }
                break;
            }
        }
        
        // 如果函数期望返回非 unit 类型，但没有返回表达式，报错
        if (expectedReturnType && expectedReturnType->tostring() != "()") {
            ReportError("Return expression expected value of type '" + expectedReturnType->tostring() +
                       "', but found no expression");
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
        
        // 检查条件表达式类型必须是布尔类型
        auto condType = InferExpressionType(*node.conditions->expression);
        if (condType && condType->tostring() != "bool") {
            ReportError("While condition must be of type bool, found " + condType->tostring());
        }
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


// 返回语句分析实现
TypeChecker::ReturnAnalysisResult TypeChecker::AnalyzeReturnStatements(BlockExpression& blockExpr) {
    ReturnAnalysisResult result;
    
    // 由于没有 deadcode，只需要分析最后一个 statement 或尾表达式
    if (!blockExpr.statements.empty()) {
        // 检查最后一个语句
        const auto& lastStmt = blockExpr.statements.back();
        if (lastStmt) {
            AnalyzeReturnStatementsInStatement(*lastStmt, result);
        }
    }
    
    // 检查尾表达式是否是返回语句
    if (blockExpr.expressionwithoutblock) {
        AnalyzeReturnStatementsInExpression(*blockExpr.expressionwithoutblock, result);
    }
    
    return result;
}

void TypeChecker::AnalyzeReturnStatementsInStatement(Statement& stmt, ReturnAnalysisResult& result) {
    // Statement 使用组合模式，astnode 存储的是实际的语句类型
    // 我们只关心 expressionstatement，它的 astnode 可能为 returnexpression
    
    // 检查 stmt.astnode 是否是 ExpressionStatement
    if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt.astnode.get())) {
        if (exprStmt->astnode) {
            // 检查是否是 return 语句
            if (auto returnExpr = dynamic_cast<ReturnExpression*>(exprStmt->astnode.get())) {
                // 找到 return 语句
                if (returnExpr->expression) {
                    auto returnType = InferExpressionType(*returnExpr->expression);
                    result.hasCertainReturn = true;
                    result.certainReturnType = returnType;
                } else {
                    // 无表达式的 return 语句
                    result.hasCertainReturn = true;
                    result.certainReturnType = std::make_shared<SimpleType>("()");
                }
            } else {
                // 其他类型的表达式，递归分析
                AnalyzeReturnStatementsInExpression(*exprStmt->astnode, result);
            }
        }
    } else if (auto letStmt = dynamic_cast<LetStatement*>(stmt.astnode.get())) {
        // let 语句中的表达式可能包含 return
        if (letStmt->expression) {
            AnalyzeReturnStatementsInExpression(*letStmt->expression, result);
        }
    }
    // 注意：stmt 本身不会是 ExpressionStatement 或 LetStatement，因为 Statement 使用组合模式
}

void TypeChecker::AnalyzeReturnStatementsInExpression(Expression& expr, ReturnAnalysisResult& result) {
    if (auto returnExpr = dynamic_cast<ReturnExpression*>(&expr)) {
        // 直接在表达式中的 return 语句（如作为尾表达式）
        result.hasCertainReturn = true;
        if (returnExpr->expression) {
            result.certainReturnType = InferExpressionType(*returnExpr->expression);
        } else {
            result.certainReturnType = std::make_shared<SimpleType>("()");
        }
    } else if (auto ifExpr = dynamic_cast<IfExpression*>(&expr)) {
        // if 表达式中的 return 语句是不确定执行的
        if (ifExpr->ifblockexpression) {
            auto ifResult = AnalyzeReturnStatements(*ifExpr->ifblockexpression);
            result.hasUncertainReturn = result.hasUncertainReturn || ifResult.hasUncertainReturn || ifResult.hasCertainReturn;
        }
        if (ifExpr->elseexpression) {
            if (auto elseBlock = dynamic_cast<BlockExpression*>(ifExpr->elseexpression.get())) {
                auto elseResult = AnalyzeReturnStatements(*elseBlock);
                result.hasUncertainReturn = result.hasUncertainReturn || elseResult.hasUncertainReturn || elseResult.hasCertainReturn;
            } else {
                AnalyzeReturnStatementsInExpression(*ifExpr->elseexpression, result);
            }
        }
    } else if (auto loopExpr = dynamic_cast<InfiniteLoopExpression*>(&expr)) {
        // 无限循环中的 return 语句是不确定执行的（因为可能永远不会执行到）
        if (loopExpr->blockexpression) {
            auto loopResult = AnalyzeReturnStatements(*loopExpr->blockexpression);
            result.hasUncertainReturn = result.hasUncertainReturn || loopResult.hasUncertainReturn || loopResult.hasCertainReturn;
        }
    } else if (auto whileExpr = dynamic_cast<PredicateLoopExpression*>(&expr)) {
        // while 循环中的 return 语句是不确定执行的
        if (whileExpr->blockexpression) {
            auto whileResult = AnalyzeReturnStatements(*whileExpr->blockexpression);
            result.hasUncertainReturn = result.hasUncertainReturn || whileResult.hasUncertainReturn || whileResult.hasCertainReturn;
        }
    } else if (auto blockExpr = dynamic_cast<BlockExpression*>(&expr)) {
        // 嵌套的块表达式
        auto blockResult = AnalyzeReturnStatements(*blockExpr);
        if (blockResult.hasCertainReturn) {
            result.hasCertainReturn = true;
            result.certainReturnType = blockResult.certainReturnType;
        }
        result.hasUncertainReturn = result.hasUncertainReturn || blockResult.hasUncertainReturn;
    } else if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&expr)) {
        // 二元表达式的左右操作数可能包含 return 语句
        if (binaryExpr->leftexpression) {
            AnalyzeReturnStatementsInExpression(*binaryExpr->leftexpression, result);
        }
        if (binaryExpr->rightexpression) {
            AnalyzeReturnStatementsInExpression(*binaryExpr->rightexpression, result);
        }
    } else if (auto assignmentExpr = dynamic_cast<AssignmentExpression*>(&expr)) {
        // 赋值表达式的左右操作数可能包含 return 语句
        if (assignmentExpr->leftexpression) {
            AnalyzeReturnStatementsInExpression(*assignmentExpr->leftexpression, result);
        }
        if (assignmentExpr->rightexpression) {
            AnalyzeReturnStatementsInExpression(*assignmentExpr->rightexpression, result);
        }
    }
    // 其他类型的表达式不需要特殊处理
}

// 实现 IfExpression 类型推断
std::shared_ptr<SemanticType> TypeChecker::InferIfExpressionType(IfExpression& expr) {
    // 检查条件表达式类型
    if (expr.conditions && expr.conditions->expression) {
        auto condType = InferExpressionType(*expr.conditions->expression);
        if (condType && condType->tostring() != "bool") {
            ReportError("If condition must be of type bool, found " + condType->tostring());
        }
    }
    
    // 推断 if 分支类型
    std::shared_ptr<SemanticType> ifType = nullptr;
    if (expr.ifblockexpression) {
        ifType = InferExpressionType(*expr.ifblockexpression);
    }
    
    // 推断 else 分支类型
    std::shared_ptr<SemanticType> elseType = nullptr;
    if (expr.elseexpression) {
        elseType = InferExpressionType(*expr.elseexpression);
    }
    
    // 根据 Rx 语言规范进行类型推断
    if (!elseType) {
        // 没有 else 分支，类型为 unit
        return ifType ? ifType : std::make_shared<SimpleType>("()");
    }
    
    // 有 else 分支的情况
    if (!ifType || !elseType) {
        return nullptr;
    }
    
    // 检查是否有一个分支是 ! 类型（never type）
    bool ifIsNever = ifType->tostring() == "!";
    bool elseIsNever = elseType->tostring() == "!";
    
    if (ifIsNever && elseIsNever) {
        // 两个分支都是 ! 类型，结果为 !
        return std::make_shared<SimpleType>("!");
    } else if (ifIsNever) {
        // if 分支是 ! 类型，结果为 else 分支类型
        return elseType;
    } else if (elseIsNever) {
        // else 分支是 ! 类型，结果为 if 分支类型
        return ifType;
    } else {
        // 两个分支都不是 ! 类型，检查类型兼容性
        if (!AreTypesCompatible(ifType, elseType)) {
            ReportError("If expression branches have incompatible types: " +
                       ifType->tostring() + " vs " + elseType->tostring());
            return nullptr;
        }
        // 返回 if 分支类型（两个分支兼容）
        return ifType;
    }
}

// 实现 InfiniteLoopExpression (loop) 类型推断
std::shared_ptr<SemanticType> TypeChecker::InferInfiniteLoopExpressionType(InfiniteLoopExpression& expr) {
    // 根据 Rx 语言规范，loop 表达式的类型取决于其中的 break 表达式
    // 如果没有 break，类型为 !（never）
    // 如果有 break 表达式，类型为所有 break 表达式类型的统一
    
    if (expr.blockexpression) {
        // 推断循环体类型，但 loop 的类型由 break 决定
        InferExpressionType(*expr.blockexpression);
        
        // 分析循环体中的 break 表达式
        auto breakAnalysis = AnalyzeBreakExpressions(*expr.blockexpression);
        
        if (!breakAnalysis.hasBreak) {
            // 没有 break 表达式，类型为 !（never）
            return std::make_shared<SimpleType>("!");
        }
        
        if (breakAnalysis.breakTypes.empty()) {
            // 有 break 但没有类型信息，返回 unit
            return std::make_shared<SimpleType>("()");
        }
        
        // 检查所有 break 表达式的类型是否一致
        auto firstBreakType = breakAnalysis.breakTypes[0];
        for (size_t i = 1; i < breakAnalysis.breakTypes.size(); ++i) {
            auto currentBreakType = breakAnalysis.breakTypes[i];
            if (!AreTypesCompatible(firstBreakType, currentBreakType)) {
                ReportError("Break expressions in loop have incompatible types: " +
                           firstBreakType->tostring() + " vs " + currentBreakType->tostring());
                return nullptr;
            }
        }
        
        // 返回第一个 break 表达式的类型（所有 break 类型都兼容）
        return firstBreakType;
    }
    
    // 没有循环体，返回 ! 类型（死循环）
    return std::make_shared<SimpleType>("!");
}

// 实现 break 表达式分析
TypeChecker::BreakAnalysisResult TypeChecker::AnalyzeBreakExpressions(BlockExpression& blockExpr) {
    BreakAnalysisResult result;
    
    // 遍历块中的所有语句
    for (const auto& stmt : blockExpr.statements) {
        if (stmt) {
            AnalyzeBreakExpressionsInStatement(*stmt, result);
        }
    }
    
    // 检查尾表达式是否包含 break
    if (blockExpr.expressionwithoutblock) {
        AnalyzeBreakExpressionsInExpression(*blockExpr.expressionwithoutblock, result);
    }
    
    return result;
}

void TypeChecker::AnalyzeBreakExpressionsInStatement(Statement& stmt, BreakAnalysisResult& result) {
    // Statement 使用组合模式，astnode 存储的是实际的语句类型
    if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt.astnode.get())) {
        if (exprStmt->astnode) {
            AnalyzeBreakExpressionsInExpression(*exprStmt->astnode, result);
        }
    } else if (auto letStmt = dynamic_cast<LetStatement*>(stmt.astnode.get())) {
        // let 语句中的表达式可能包含 break
        if (letStmt->expression) {
            AnalyzeBreakExpressionsInExpression(*letStmt->expression, result);
        }
    }
}

void TypeChecker::AnalyzeBreakExpressionsInExpression(Expression& expr, BreakAnalysisResult& result) {
    if (auto breakExpr = dynamic_cast<BreakExpression*>(&expr)) {
        // 找到 break 表达式
        result.hasBreak = true;
        
        if (breakExpr->expression) {
            // break 有表达式，推断其类型
            auto breakType = InferExpressionType(*breakExpr->expression);
            if (breakType) {
                result.breakTypes.push_back(breakType);
            } else {
                // 如果类型推断失败，使用 unit 类型
                result.breakTypes.push_back(std::make_shared<SimpleType>("()"));
            }
        } else {
            // break 没有表达式，类型为 unit
            result.breakTypes.push_back(std::make_shared<SimpleType>("()"));
        }
    } else if (auto ifExpr = dynamic_cast<IfExpression*>(&expr)) {
        // if 表达式中的 break
        if (ifExpr->ifblockexpression) {
            auto ifResult = AnalyzeBreakExpressions(*ifExpr->ifblockexpression);
            result.hasBreak = result.hasBreak || ifResult.hasBreak;
            result.breakTypes.insert(result.breakTypes.end(),
                                  ifResult.breakTypes.begin(),
                                  ifResult.breakTypes.end());
        }
        if (ifExpr->elseexpression) {
            if (auto elseBlock = dynamic_cast<BlockExpression*>(ifExpr->elseexpression.get())) {
                auto elseResult = AnalyzeBreakExpressions(*elseBlock);
                result.hasBreak = result.hasBreak || elseResult.hasBreak;
                result.breakTypes.insert(result.breakTypes.end(),
                                      elseResult.breakTypes.begin(),
                                      elseResult.breakTypes.end());
            } else {
                AnalyzeBreakExpressionsInExpression(*ifExpr->elseexpression, result);
            }
        }
    } else if (auto loopExpr = dynamic_cast<InfiniteLoopExpression*>(&expr)) {
        // 嵌套的 loop 表达式中的 break（不影响外层 loop）
        if (loopExpr->blockexpression) {
            // 递归分析内层 loop，但不将其 break 类型添加到外层结果中
            AnalyzeBreakExpressions(*loopExpr->blockexpression);
        }
    } else if (auto whileExpr = dynamic_cast<PredicateLoopExpression*>(&expr)) {
        // while 循环中的 break（不影响外层 loop）
        if (whileExpr->blockexpression) {
            // 递归分析内层 while，但不将其 break 类型添加到外层结果中
            AnalyzeBreakExpressions(*whileExpr->blockexpression);
        }
    } else if (auto blockExpr = dynamic_cast<BlockExpression*>(&expr)) {
        // 嵌套的块表达式
        auto blockResult = AnalyzeBreakExpressions(*blockExpr);
        result.hasBreak = result.hasBreak || blockResult.hasBreak;
        result.breakTypes.insert(result.breakTypes.end(),
                              blockResult.breakTypes.begin(),
                              blockResult.breakTypes.end());
    } else if (auto binaryExpr = dynamic_cast<BinaryExpression*>(&expr)) {
        // 二元表达式的左右操作数可能包含 break
        if (binaryExpr->leftexpression) {
            AnalyzeBreakExpressionsInExpression(*binaryExpr->leftexpression, result);
        }
        if (binaryExpr->rightexpression) {
            AnalyzeBreakExpressionsInExpression(*binaryExpr->rightexpression, result);
        }
    } else if (auto assignmentExpr = dynamic_cast<AssignmentExpression*>(&expr)) {
        // 赋值表达式的左右操作数可能包含 break
        if (assignmentExpr->leftexpression) {
            AnalyzeBreakExpressionsInExpression(*assignmentExpr->leftexpression, result);
        }
        if (assignmentExpr->rightexpression) {
            AnalyzeBreakExpressionsInExpression(*assignmentExpr->rightexpression, result);
        }
    }
    // 其他类型的表达式不需要特殊处理
}

// 实现 PredicateLoopExpression (while) 类型推断
std::shared_ptr<SemanticType> TypeChecker::InferPredicateLoopExpressionType(PredicateLoopExpression& expr) {
    // 根据 Rx 语言规范，while 表达式的类型总是 unit
    
    // 检查条件表达式类型
    if (expr.conditions && expr.conditions->expression) {
        auto condType = InferExpressionType(*expr.conditions->expression);
        if (condType && condType->tostring() != "bool") {
            ReportError("While condition must be of type bool, found " + condType->tostring());
        }
    }
    
    // 推断循环体类型（虽然不影响 while 的类型）
    if (expr.blockexpression) {
        InferExpressionType(*expr.blockexpression);
    }
    
    // while 表达式的类型总是 unit
    return std::make_shared<SimpleType>("()");
}

// 实现 BlockExpression 类型推断
std::shared_ptr<SemanticType> TypeChecker::InferBlockExpressionType(BlockExpression& expr) {
    // 根据 Rx 语言规范，BlockExpression 的类型推断规则：
    // 1. 有尾表达式的情况，使用该表达式的类型
    // 2. 没有尾表达式的情况，如果所有控制流都会遇到 return/break/continue，使用 !
    // 3. 否则类型为 unit
    
    // 重要：在推断类型之前，先访问所有语句以确保变量被注册到符号表中
    for (const auto& stmt : expr.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    
    // 1. 首先检查是否有尾表达式（expressionwithoutblock）
    if (expr.expressionwithoutblock) {
        // 推断尾表达式的类型
        auto tailExprType = InferExpressionType(*expr.expressionwithoutblock);
        if (tailExprType) {
            return tailExprType;
        } else {
            // 如果尾表达式类型推断失败，默认为 unit
            return std::make_shared<SimpleType>("()");
        }
    } else if (!expr.statements.empty()) {
        // 2. 如果没有尾表达式，检查最后一条语句是否是不带分号的表达式语句
        auto lastStmt = expr.statements.back();
        // Statement 使用组合模式，需要检查其 astnode 的类型
        if (auto exprStmt = dynamic_cast<ExpressionStatement*>(lastStmt->astnode.get())) {
            // 检查是否为不带分号的表达式语句（expressionstatement without semi）
            if (!exprStmt->hassemi && exprStmt->astnode) {
                // 不带分号的表达式语句，作为尾表达式处理
                auto lastExprType = InferExpressionType(*exprStmt->astnode);
                if (lastExprType) {
                    return lastExprType;
                } else {
                    // 如果表达式类型推断失败，默认为 unit
                    return std::make_shared<SimpleType>("()");
                }
            } else {
                // 带分号的表达式语句或其他情况，块表达式类型为 unit
                return std::make_shared<SimpleType>("()");
            }
        } else {
            // 如果最后一条语句不是表达式语句，块表达式类型为 unit
            return std::make_shared<SimpleType>("()");
        }
    } else {
        // 3. 空块表达式的类型为 unit
        return std::make_shared<SimpleType>("()");
    }
}

// 实现 TypeCastExpression 类型推断
std::shared_ptr<SemanticType> TypeChecker::InferTypeCastExpressionType(TypeCastExpression& expr) {
    // TypeCastExpression 的类型为其转化后 as 对应的类型，对应其存储的 typenobounds
    if (expr.typenobounds) {
        return CheckType(*expr.typenobounds);
    }
    return nullptr;
}


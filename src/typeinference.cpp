#include "typeinference.hpp"
#include "astnodes.hpp"
#include "typewrapper.hpp"
#include <iostream>

// TypeEnvironment实现
TypeEnvironment::TypeEnvironment() {}

std::shared_ptr<SemanticType> TypeEnvironment::freshTypeVariable() {
    std::string varName = "T" + std::to_string(nextTypeVarId++);
    auto typeVar = std::make_shared<TypeVariable>(varName);
    typeVariables[varName] = typeVar;
    return typeVar;
}

std::shared_ptr<SemanticType> TypeEnvironment::getTypeVariable(const std::string& name) {
    auto it = typeVariables.find(name);
    return it != typeVariables.end() ? it->second : nullptr;
}

void TypeEnvironment::setTypeVariable(const std::string& name, std::shared_ptr<SemanticType> type) {
    typeVariables[name] = type;
}

void TypeEnvironment::addSubstitution(const std::string& typeVar, std::shared_ptr<SemanticType> type) {
    // 进行occurs检查
    if (occursCheck(typeVar, type)) {
        throw std::runtime_error("Occurs check failed: circular type definition");
    }
    typeSubstitutions[typeVar] = type;
}

std::shared_ptr<SemanticType> TypeEnvironment::applySubstitutions(std::shared_ptr<SemanticType> type) {
    if (auto typeVar = dynamic_cast<TypeVariable*>(type.get())) {
        auto it = typeSubstitutions.find(typeVar->getName());
        if (it != typeSubstitutions.end()) {
            return applySubstitutions(it->second);  // 递归应用替换
        }
        return type;
    } else if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        auto newElement = applySubstitutions(arrayType->getElementType());
        return std::make_shared<ArrayTypeWrapper>(newElement, arrayType->getSizeExpression());
    } else if (auto sliceType = dynamic_cast<SliceTypeWrapper*>(type.get())) {
        auto newElement = applySubstitutions(sliceType->getElementType());
        return std::make_shared<SliceTypeWrapper>(newElement);
    }
    
    return type;
}

void TypeEnvironment::unify(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2) {
    type1 = applySubstitutions(type1);
    type2 = applySubstitutions(type2);
    
    if (type1->tostring() == type2->tostring()) {
        return;  // 类型相同，无需处理
    }
    
    // 处理类型变量
    if (auto var1 = dynamic_cast<TypeVariable*>(type1.get())) {
        addSubstitution(var1->getName(), type2);
        return;
    }
    
    if (auto var2 = dynamic_cast<TypeVariable*>(type2.get())) {
        addSubstitution(var2->getName(), type1);
        return;
    }
    
    // // 处理引用类型
    // if (auto ref1 = dynamic_cast<ReferenceTypeWrapper*>(type1.get())) {
    //     if (auto ref2 = dynamic_cast<ReferenceTypeWrapper*>(type2.get())) {
    //         if (ref1->getIsMutable() == ref2->getIsMutable()) {
    //             unify(ref1->getReferencedType(), ref2->getReferencedType());
    //             return;
    //         }
    //     }
    // }
    
    // 处理数组类型
    if (auto array1 = dynamic_cast<ArrayTypeWrapper*>(type1.get())) {
        if (auto array2 = dynamic_cast<ArrayTypeWrapper*>(type2.get())) {
            unify(array1->getElementType(), array2->getElementType());
            // 数组大小在编译时检查，这里不处理
            return;
        }
    }
    
    // 处理切片类型
    if (auto slice1 = dynamic_cast<SliceTypeWrapper*>(type1.get())) {
        if (auto slice2 = dynamic_cast<SliceTypeWrapper*>(type2.get())) {
            unify(slice1->getElementType(), slice2->getElementType());
            return;
        }
    }
    
    throw std::runtime_error("SemanticType mismatch: " + type1->tostring() + " vs " + type2->tostring());
}

bool TypeEnvironment::occursCheck(const std::string& typeVar, std::shared_ptr<SemanticType> type) {
    if (auto var = dynamic_cast<TypeVariable*>(type.get())) {
        return var->getName() == typeVar;
    } 
    // else if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(type.get())) {
    //     return occursCheck(typeVar, refType->getReferencedType());
    // } 
    else if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        return occursCheck(typeVar, arrayType->getElementType());
    } else if (auto sliceType = dynamic_cast<SliceTypeWrapper*>(type.get())) {
        return occursCheck(typeVar, sliceType->getElementType());
    }
    
    return false;
}

// TypeInferenceChecker实现
TypeInferenceChecker::TypeInferenceChecker(std::shared_ptr<ScopeTree> scopeTree,
                                         std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer,
                                         std::shared_ptr<ConstantEvaluator> constantEvaluator)
    : scopeTree(scopeTree), controlFlowAnalyzer(controlFlowAnalyzer), 
      constantEvaluator(constantEvaluator) {
    typeEnv = std::make_shared<TypeEnvironment>();
}

bool TypeInferenceChecker::inferTypes() {
    hasErrors = false;
    return !hasErrors;
}

bool TypeInferenceChecker::hasInferenceErrors() const {
    return hasErrors;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::getInferredType(ASTNode* node) const {
    auto it = inferredTypes.find(node);
    return it != inferredTypes.end() ? it->second : nullptr;
}

void TypeInferenceChecker::visit(Crate& node) {
    pushNode(node);
    
    // 推断整个crate的类型
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    // 解决所有类型约束
    try {
        solveTypeConstraints();
    } catch (const std::exception& e) {
        reportError("SemanticType constraint solving failed: " + std::string(e.what()));
    }
    
    popNode();
}

void TypeInferenceChecker::visit(Function& node) {
    pushNode(node);
    
    // 设置函数返回类型上下文
    std::string previousReturnType = currentFunctionReturnType;
    auto returnTypeNode = std::move(node.functionreturntype->type);
    currentFunctionReturnType = returnTypeNode ? dynamic_cast<TypePath*>(returnTypeNode.get())->simplepathsegement->identifier : "()";
    
    enterFunctionContext(currentFunctionReturnType);
    
    // 进入函数作用域
    scopeTree->enterScope(Scope::ScopeType::Function, &node);
    
    // 处理函数参数类型
    auto params = std::move(node.functionparameters);
    if (params) {
        for (const auto& param : params->functionparams) {
            param->accept(*this);
        }
    }
    
    // 处理函数体类型
    auto body = std::move(node.blockexpression);
    if (body) {
        // 设置期望的返回类型
        pushExpectedType(std::make_shared<SimpleType>(currentFunctionReturnType));
        body->accept(*this);
        popExpectedType();
        
        // 检查函数体类型与返回类型是否兼容
        auto bodyType = getInferredType(body.get());
        if (bodyType && !areTypesCompatible(std::make_shared<SimpleType>(currentFunctionReturnType), bodyType)) {
            reportTypeError(currentFunctionReturnType, bodyType->tostring(), body.get());
        }
    }
    
    // 退出函数作用域
    scopeTree->exitScope();
    exitFunctionContext();
    currentFunctionReturnType = previousReturnType;
    
    popNode();
}

void TypeInferenceChecker::visit(LetStatement& node) {
    pushNode(node);
    
    // 获取声明的类型
    auto declaredType = std::move(node.type);
    std::shared_ptr<SemanticType> type;
    
    if (declaredType) {
        // 有显式类型注解
        declaredType->accept(*this);
        type = getInferredType(declaredType.get());
    } else {
        // 类型推断：创建新的类型变量
        type = typeEnv->freshTypeVariable();
    }
    
    // 检查模式
    auto pattern = std::move(node.patternnotopalt);
    if (pattern) {
        // 设置期望的类型用于模式检查
        pushExpectedType(type);
        pattern->accept(*this);
        popExpectedType();
    }
    
    // 检查初始化表达式
    auto initExpr = std::move(node.expression);
    if (initExpr) {
        // 设置期望的类型
        pushExpectedType(type);
        initExpr->accept(*this);
        popExpectedType();
        
        // 检查初始化表达式类型与声明类型是否兼容
        auto initType = getInferredType(initExpr.get());
        if (initType && type) {
            addTypeConstraint(initType, type);
        }
    }
    
    // 记录变量类型
    if (auto identPattern = dynamic_cast<IdentifierPattern*>(pattern.get())) {
        std::string varName = identPattern->identifier;
        setVariableType(varName, type);
    }
    
    popNode();
}

void TypeInferenceChecker::visit(ExpressionStatement& node) {
    pushNode(node);
    
    auto expr = std::move(node.astnode);
    if (expr) {
        expr->accept(*this);
        
        // 表达式语句的类型是unit，但表达式的类型用于检查
        inferredTypes[&node] = std::make_shared<SimpleType>("()");
    }
    
    popNode();
}

void TypeInferenceChecker::visit(PathExpression& node) {
    pushNode(node);
    
    auto path = std::move(node.simplepath);
    if (path) {
        std::shared_ptr<SemanticType> type = inferPathType(node);
        if (type) {
            inferredTypes[&node] = type;
        } else {
            reportError("Cannot infer type for path expression");
        }
    }
    
    popNode();
}

void TypeInferenceChecker::visit(CallExpression& node) {
    pushNode(node);
    
    auto type = inferCallType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(FieldExpression& node) {
    pushNode(node);
    
    auto type = inferFieldAccessType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(BinaryExpression& node) {
    pushNode(node);
    
    auto type = inferBinaryExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(IfExpression& node) {
    pushNode(node);
    
    auto type = inferIfExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(BlockExpression& node) {
    pushNode(node);
    
    auto type = inferBlockExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(ReturnExpression& node) {
    pushNode(node);
    
    // 检查返回表达式类型与函数返回类型是否兼容
    auto expr = std::move(node.expression);
    if (expr) {
        // 设置期望的返回类型
        pushExpectedType(std::make_shared<SimpleType>(currentFunctionReturnType));
        expr->accept(*this);
        popExpectedType();
        
        auto exprType = getInferredType(expr.get());
        if (exprType) {
            addTypeConstraint(exprType, std::make_shared<SimpleType>(currentFunctionReturnType));
        }
    } else {
        // 没有表达式的return，检查返回类型是否是unit
        if (currentFunctionReturnType != "()") {
            reportError("Empty return in function with non-unit return type");
        }
    }
    
    // return表达式本身的类型是never (!)
    inferredTypes[&node] = std::make_shared<NeverType>();
    
    popNode();
}

void TypeInferenceChecker::visit(AssignmentExpression& node) {
    pushNode(node);
    
    auto type = inferAssignmentType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    popNode();
}

void TypeInferenceChecker::visit(IdentifierPattern& node) {
    pushNode(node);
    
    // 获取期望的类型
    auto expectedType = getExpectedType();
    if (expectedType) {
        // 记录变量类型
        std::string varName = node.identifier;
        setVariableType(varName, expectedType);
        
        // 检查可变性
        if (node.hasmut) {
            // 可变绑定：类型系统需要支持可变性
            // 这里简化处理，实际需要更复杂的可变性跟踪
        }
    }
    
    popNode();
}

// 类型推断具体实现
std::shared_ptr<SemanticType> TypeInferenceChecker::inferPathType(PathExpression& expr) {
    auto path = std::move(expr.simplepath);
    if (!path) return nullptr;
    
    return resolvePathType(*path);
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferCallType(CallExpression& expr) {
    auto callee = std::move(expr.expression);
    auto params = std::move(expr.callparams);
    
    if (!callee) return nullptr;
    
    // 推断callee的类型
    callee->accept(*this);
    auto calleeType = getInferredType(callee.get());
    
    // 推断参数类型
    std::vector<std::shared_ptr<SemanticType>> argTypes;
    if (params) {
        argTypes = inferArgumentTypes(*params);
    }
    
    // 解析函数类型
    // 这里需要根据callee的类型信息来解析
    // 简化处理：假设callee是路径表达式
    if (auto pathExpr = dynamic_cast<PathExpression*>(callee.get())) {
        auto path = std::move(pathExpr->simplepath);
        if (path) {
            auto segments = std::move(path->simplepathsegements);
            if (!segments.empty()) {
                std::string functionName = segments.back()->identifier;
                return resolveFunctionType(functionName, argTypes);
            }
        }
    }
    
    // 如果无法解析，创建函数类型变量
    auto returnType = typeEnv->freshTypeVariable();
    return returnType;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferFieldAccessType(FieldExpression& expr) {
    auto baseExpr = std::move(expr.expression);
    std::string fieldName = expr.identifier;
    
    if (!baseExpr) return nullptr;
    
    // 推断基础表达式类型
    baseExpr->accept(*this);
    auto baseType = getInferredType(baseExpr.get());
    
    if (!baseType) return nullptr;
    
    // 解析字段类型
    // 这里需要根据baseType的结构体/枚举信息来查找字段
    std::string baseTypeName = baseType->tostring();
    
    // 查找结构体字段
    auto fieldType = getStructFieldType(baseTypeName, fieldName);
    if (fieldType) {
        return fieldType;
    }
    
    // 查找枚举variant
    auto variantType = getEnumVariantType(baseTypeName, fieldName);
    if (variantType) {
        return variantType;
    }
    
    reportUndefinedError(fieldName, "field", &expr);
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferBinaryExpressionType(BinaryExpression& expr) {
    auto left = std::move(expr.leftexpression);
    auto right = std::move(expr.rightexpression);
    Token op = expr.binarytype;
    
    if (!left || !right) return nullptr;
    
    // 推断左右操作数类型
    left->accept(*this);
    right->accept(*this);
    
    auto leftType = getInferredType(left.get());
    auto rightType = getInferredType(right.get());
    
    if (!leftType || !rightType) return nullptr;
    
    // 根据操作符推断类型
    switch (op) {
        case Token::kPlus:
        case Token::kMinus:
        case Token::kStar:
        case Token::kSlash:
        case Token::kPercent:
            // 算术运算：需要数值类型
            addTypeConstraint(leftType, rightType);  // 左右类型应该相同
            return leftType;  // 返回操作数类型
            
        case Token::kShl:
        case Token::kShr:
        case Token::kAnd:
        case Token::kOr:
        case Token::kCaret:
            // 位运算：需要整数类型
            addTypeConstraint(leftType, rightType);
            return leftType;
            
        case Token::kEqEq:
        case Token::kNe:
        case Token::kLt:
        case Token::kGt:
        case Token::kLe:
        case Token::kGe:
            // 比较运算：返回bool
            addTypeConstraint(leftType, rightType);  // 可比较的类型应该相同
            return std::make_shared<SimpleType>("bool");
            
        case Token::kAndAnd:
        case Token::kOrOr:
            // 逻辑运算：需要bool类型，返回bool
            addTypeConstraint(leftType, std::make_shared<SimpleType>("bool"));
            addTypeConstraint(rightType, std::make_shared<SimpleType>("bool"));
            return std::make_shared<SimpleType>("bool");
            
        default:
            reportError("Unsupported binary operator: " + to_string(op));
            return nullptr;
    }
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferIfExpressionType(IfExpression& expr) {
    auto condition = std::move(expr.conditions);
    auto ifBlock = std::move(expr.ifblockexpression);
    auto elseExpr = std::move(expr.elseexpression);
    
    if (!condition || !ifBlock) return nullptr;
    
    // 推断条件类型（应该是bool）
    condition->accept(*this);
    auto condType = getInferredType(condition->expression.get());
    if (condType) {
        addTypeConstraint(condType, std::make_shared<SimpleType>("bool"));
    }
    
    // 推断if分支类型
    ifBlock->accept(*this);
    auto ifType = getInferredType(ifBlock.get());
    
    if (!elseExpr) {
        // 没有else分支，返回unit类型
        return std::make_shared<SimpleType>("()");
    }
    
    // 推断else分支类型
    elseExpr->accept(*this);
    auto elseType = getInferredType(elseExpr.get());
    
    if (!ifType || !elseType) return nullptr;
    
    // 统一if和else分支的类型
    addTypeConstraint(ifType, elseType);
    return ifType;  // 返回统一的类型
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferBlockExpressionType(BlockExpression& block) {
    auto stmt = std::move(block.statement);
    if (!stmt) {
        return std::make_shared<SimpleType>("()");
    }
    
    // 推断块中最后一个语句/表达式的类型
    stmt->accept(*this);
    auto stmtType = getInferredType(stmt.get());
    
    // 如果块发散，则类型为never
    if (controlFlowAnalyzer->alwaysDivergesAt(&block)) {
        return std::make_shared<NeverType>();
    }
    
    return stmtType ? stmtType : std::make_shared<SimpleType>("()");
}

std::shared_ptr<SemanticType> TypeInferenceChecker::inferAssignmentType(AssignmentExpression& expr) {
    auto lhs = std::move(expr.leftexpression);
    auto rhs = std::move(expr.rightexpression);
    
    if (!lhs || !rhs) return nullptr;
    
    // 检查左侧是否可赋值
    checkAssignmentMutability(*lhs);
    
    // 推断右侧类型
    rhs->accept(*this);
    auto rhsType = getInferredType(rhs.get());
    
    if (!rhsType) return nullptr;
    
    // 对于赋值表达式，我们需要推断左侧的类型
    // 但由于左侧可能是模式，这里简化处理
    inAssignmentContext = true;
    lhs->accept(*this);
    inAssignmentContext = false;
    
    auto lhsType = getInferredType(lhs.get());
    
    if (lhsType && rhsType) {
        // 添加类型约束：右侧类型应该可以赋值给左侧
        addTypeConstraint(rhsType, lhsType);
    }
    
    // 赋值表达式的类型是unit
    return std::make_shared<SimpleType>("()");
}

// 路径解析
std::shared_ptr<SemanticType> TypeInferenceChecker::resolvePathType(SimplePath& path) {
    auto segments = std::move(path.simplepathsegements);
    if (segments.empty()) {
        return nullptr;
    }
    
    // 单段路径：变量、常量或类型
    if (segments.size() == 1) {
        std::string name = segments[0]->identifier;
        
        // 查找符号
        auto symbol = resolveSymbol(name);
        if (symbol) {
            return resolveTypeFromSymbol(symbol);
        }
        
        reportUndefinedError(name, "variable or type", &path);
        return nullptr;
    }
    
    // 多段路径：可能是模块路径、关联项等
    // 这里简化处理，只处理两段路径（如A::foo）
    if (segments.size() == 2) {
        std::string baseName = segments[0]->identifier;
        std::string itemName = segments[1]->identifier;
        
        // 查找基础类型
        auto baseSymbol = resolveSymbol(baseName);
        if (baseSymbol && baseSymbol->type) {
            return resolveAssociatedType(baseSymbol->type, itemName);
        }
    }
    
    reportError("Complex path expressions not fully supported");
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::resolveFunctionType(const std::string& functionName, 
                                                               const std::vector<std::shared_ptr<SemanticType>>& argTypes) {
    // 查找函数符号
    auto symbol = resolveSymbol(functionName);
    if (!symbol || symbol->kind != SymbolKind::Function) {
        reportUndefinedError(functionName, "function", getCurrentNode());
        return nullptr;
    }
    
    auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!funcSymbol) {
        return nullptr;
    }
    
    // 检查参数数量和类型
    if (funcSymbol->parameterTypes.size() != argTypes.size()) {
        reportError("Function " + functionName + " expects " + 
                   std::to_string(funcSymbol->parameterTypes.size()) + 
                   " arguments, but " + std::to_string(argTypes.size()) + " provided");
        return nullptr;
    }
    
    // 添加参数类型约束
    for (size_t i = 0; i < argTypes.size(); ++i) {
        if (i < funcSymbol->parameterTypes.size()) {
            addTypeConstraint(argTypes[i], funcSymbol->parameterTypes[i]);
        }
    }
    
    return funcSymbol->returntype;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::resolveMethodType(std::shared_ptr<SemanticType> receiverType,
                                                             const std::string& methodName,
                                                             const std::vector<std::shared_ptr<SemanticType>>& argTypes) {
    // 这里需要根据接收者类型查找方法
    // 简化处理：在符号表中查找方法
    
    std::string receiverTypeName = receiverType->tostring();
    
    // 构造完整的方法名（如Type::method）
    std::string fullMethodName = receiverTypeName + "::" + methodName;
    
    auto symbol = resolveSymbol(fullMethodName);
    if (!symbol || symbol->kind != SymbolKind::Function) {
        reportUndefinedError(methodName, "method", getCurrentNode());
        return nullptr;
    }
    
    auto methodSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!methodSymbol) {
        return nullptr;
    }
    
    // 检查参数（包括self参数）
    if (methodSymbol->parameterTypes.size() != argTypes.size() + 1) {
        reportError("Method " + methodName + " expects " + 
                   std::to_string(methodSymbol->parameterTypes.size()) + 
                   " arguments, but " + std::to_string(argTypes.size() + 1) + " provided");
        return nullptr;
    }
    
    // 第一个参数应该是self，检查接收者类型
    if (!methodSymbol->parameterTypes.empty()) {
        addTypeConstraint(receiverType, methodSymbol->parameterTypes[0]);
    }
    
    // 添加其他参数类型约束
    for (size_t i = 0; i < argTypes.size(); ++i) {
        if (i + 1 < methodSymbol->parameterTypes.size()) {
            addTypeConstraint(argTypes[i], methodSymbol->parameterTypes[i + 1]);
        }
    }
    
    return methodSymbol->returntype;
}

std::vector<std::shared_ptr<SemanticType>> TypeInferenceChecker::inferArgumentTypes(CallParams& params) {
    std::vector<std::shared_ptr<SemanticType>> argTypes;
    
    for (const auto& expr : std::move(params.expressions)) {
        expr->accept(*this);
        auto type = getInferredType(expr.get());
        if (type) {
            argTypes.push_back(type);
        }
    }
    
    return argTypes;
}

// 类型约束处理
void TypeInferenceChecker::addTypeConstraint(std::shared_ptr<SemanticType> actual, std::shared_ptr<SemanticType> expected) {
    try {
        typeEnv->unify(actual, expected);
    } catch (const std::exception& e) {
        reportError("SemanticType unification failed: " + std::string(e.what()));
    }
}

void TypeInferenceChecker::solveTypeConstraints() {
    // 类型约束已经在unify过程中解决
    // 这里可以添加额外的约束解决逻辑
}

bool TypeInferenceChecker::areTypesCompatible(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2) {
    try {
        typeEnv->unify(type1, type2);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// 符号解析
std::shared_ptr<Symbol> TypeInferenceChecker::resolveSymbol(const std::string& name) {
    return scopeTree->lookupSymbol(name);
}

std::shared_ptr<SemanticType> TypeInferenceChecker::getVariableType(const std::string& varName) {
    auto it = variableTypes.find(varName);
    if (it != variableTypes.end()) {
        return it->second;
    }
    
    // 在符号表中查找
    auto symbol = resolveSymbol(varName);
    if (symbol) {
        return resolveTypeFromSymbol(symbol);
    }
    
    return nullptr;
}

void TypeInferenceChecker::setVariableType(const std::string& varName, std::shared_ptr<SemanticType> type) {
    variableTypes[varName] = type;
}

// 可变性检查
void TypeInferenceChecker::checkMutability(const std::string& varName, ASTNode* usageContext) {
    auto symbol = resolveSymbol(varName);
    if (symbol && symbol->kind == SymbolKind::Variable) {
        if (!symbol->ismutable && inAssignmentContext) {
            reportError("Cannot assign to immutable variable '" + varName + "'");
        }
    }
}

void TypeInferenceChecker::checkAssignmentMutability(Expression& lhs) {
    // 检查左侧表达式是否可赋值
    if (auto pathExpr = dynamic_cast<PathExpression*>(&lhs)) {
        auto path = std::move(pathExpr->simplepath);
        if (path) {
            auto segments = std::move(path->simplepathsegements);
            if (segments.size() == 1) {
                std::string varName = segments[0]->identifier;
                checkMutability(varName, &lhs);
            }
        }
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(&lhs)) {
        // 字段赋值：需要检查字段的可变性
        // 这里简化处理
    }
    // 其他可赋值表达式...
}

// 错误处理
void TypeInferenceChecker::reportError(const std::string& message) {
    std::cerr << "SemanticType Inference Error: " << message << std::endl;
    hasErrors = true;
}

void TypeInferenceChecker::reportTypeError(const std::string& expected, const std::string& actual, ASTNode* context) {
    std::cerr << "SemanticType Error: expected " << expected << ", got " << actual << std::endl;
    hasErrors = true;
}

void TypeInferenceChecker::reportUndefinedError(const std::string& name, const std::string& kind, ASTNode* context) {
    std::cerr << "Undefined " << kind << ": '" << name << "'" << std::endl;
    hasErrors = true;
}

// 辅助方法实现
void TypeInferenceChecker::pushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void TypeInferenceChecker::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* TypeInferenceChecker::getCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void TypeInferenceChecker::pushExpectedType(std::shared_ptr<SemanticType> type) {
    expectedTypeStack.push(type);
}

void TypeInferenceChecker::popExpectedType() {
    if (!expectedTypeStack.empty()) {
        expectedTypeStack.pop();
    }
}

std::shared_ptr<SemanticType> TypeInferenceChecker::getExpectedType() {
    return expectedTypeStack.empty() ? nullptr : expectedTypeStack.top();
}

void TypeInferenceChecker::enterFunctionContext(const std::string& returnType) {
    currentFunctionReturnType = returnType;
}

void TypeInferenceChecker::exitFunctionContext() {
    currentFunctionReturnType.clear();
}

void TypeInferenceChecker::enterImplContext(std::shared_ptr<SemanticType> selfType) {
    currentSelfType = selfType;
}

void TypeInferenceChecker::exitImplContext() {
    currentSelfType = nullptr;
}

// 其他辅助方法实现...
std::shared_ptr<SemanticType> TypeInferenceChecker::resolveTypeFromSymbol(std::shared_ptr<Symbol> symbol) {
    if (!symbol) return nullptr;
    
    if (symbol->type) {
        return symbol->type;
    }
    
    // 根据符号种类返回默认类型
    switch (symbol->kind) {
        case SymbolKind::Variable:
            // 变量类型需要在声明时推断
            return typeEnv->freshTypeVariable();
            
        case SymbolKind::Function:
            if (auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol)) {
                return funcSymbol->returntype;
            }
            break;
            
        case SymbolKind::Struct:
            return std::make_shared<SimpleType>(symbol->name);
            
        case SymbolKind::Enum:
            return std::make_shared<SimpleType>(symbol->name);
            
        case SymbolKind::Constant:
            if (auto constSymbol = std::dynamic_pointer_cast<ConstantSymbol>(symbol)) {
                return constSymbol->type;
            }
            break;
            
        default:
            break;
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::getStructFieldType(const std::string& structName, 
                                                              const std::string& fieldName) {
    // 查找结构体符号
    auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(resolveSymbol(structName));
    if (!structSymbol) {
        return nullptr;
    }
    
    // 查找字段
    for (const auto& field : structSymbol->fields) {
        if (field->name == fieldName) {
            return field->type;
        }
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::resolveAssociatedType(std::shared_ptr<SemanticType> baseType, 
                                                                 const std::string& associatedItem) {
    // 这里需要根据基础类型查找关联项
    // 简化处理：在符号表中查找
    std::string fullName = baseType->tostring() + "::" + associatedItem;
    auto symbol = resolveSymbol(fullName);
    if (symbol) {
        return resolveTypeFromSymbol(symbol);
    }
    
    return nullptr;
}
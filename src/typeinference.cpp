#include "typeinference.hpp"
#include "astnodes.hpp"
#include "symbol.hpp"
#include "typewrapper.hpp"
#include <iostream>

// TypeEnvironment实现
TypeEnvironment::TypeEnvironment() {}

std::shared_ptr<SemanticType> TypeEnvironment::FreshTypeVariable() {
    std::string varName = "T" + std::to_string(nextTypeVarId++);
    auto typeVar = std::make_shared<TypeVariable>(varName);
    typeVariables[varName] = typeVar;
    return typeVar;
}

std::shared_ptr<SemanticType> TypeEnvironment::GetTypeVariable(const std::string& name) {
    auto it = typeVariables.find(name);
    return it != typeVariables.end() ? it->second : nullptr;
}

void TypeEnvironment::SetTypeVariable(const std::string& name, std::shared_ptr<SemanticType> type) {
    typeVariables[name] = type;
}

void TypeEnvironment::AddSubstitution(const std::string& typeVar, std::shared_ptr<SemanticType> type) {
    // 进行occurs检查
    if (occursCheck(typeVar, type)) {
        throw std::runtime_error("Occurs check failed: circular type definition");
    }
    typeSubstitutions[typeVar] = type;
}

std::shared_ptr<SemanticType> TypeEnvironment::ApplySubstitutions(std::shared_ptr<SemanticType> type) {
    if (auto typeVar = dynamic_cast<TypeVariable*>(type.get())) {
        auto it = typeSubstitutions.find(typeVar->GetName());
        if (it != typeSubstitutions.end()) {
            return ApplySubstitutions(it->second);
        }
        return type;
    } else if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        auto newElement = ApplySubstitutions(arrayType->GetElementType());
        return std::make_shared<ArrayTypeWrapper>(newElement, arrayType->GetSizeExpression());
    } 
    
    return type;
}

void TypeEnvironment::unify(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2) {
    type1 = ApplySubstitutions(type1);
    type2 = ApplySubstitutions(type2);
    
    if (type1->tostring() == type2->tostring()) {
        return;  // 类型相同，无需处理
    }
    
    // 处理类型变量
    if (auto var1 = dynamic_cast<TypeVariable*>(type1.get())) {
        AddSubstitution(var1->GetName(), type2);
        return;
    }
    if (auto var2 = dynamic_cast<TypeVariable*>(type2.get())) {
        AddSubstitution(var2->GetName(), type1);
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
            unify(array1->GetElementType(), array2->GetElementType());
            // 数组大小在编译时检查，这里不处理
            return;
        }
    }
    
    throw std::runtime_error("SemanticType mismatch: " + type1->tostring() + " vs " + type2->tostring());
}

bool TypeEnvironment::occursCheck(const std::string& typeVar, std::shared_ptr<SemanticType> type) {
    if (auto var = dynamic_cast<TypeVariable*>(type.get())) {
        return var->GetName() == typeVar;
    } 
    // else if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(type.get())) {
    //     return occursCheck(typeVar, refType->getReferencedType());
    // } 
    else if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        return occursCheck(typeVar, arrayType->GetElementType());
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

bool TypeInferenceChecker::InferTypes() {
    hasErrors = false;
    return !hasErrors;
}

bool TypeInferenceChecker::HasInferenceErrors() const {
    return hasErrors;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::GetInferredType(ASTNode* node) const {
    auto it = inferredTypes.find(node);
    return it != inferredTypes.end() ? it->second : nullptr;
}

void TypeInferenceChecker::visit(Crate& node) {
    PushNode(node);
    
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    // 解决所有类型约束
    try {
        SolveTypeConstraints();
    } catch (const std::exception& e) {
        ReportError("SemanticType constraint solving failed: " + std::string(e.what()));
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(Function& node) {
    PushNode(node);
    
    // 设置函数返回类型上下文
    std::string previousReturnType = currentFunctionReturnType;
    currentFunctionReturnType = node.functionreturntype->type ? dynamic_cast<TypePath*>(node.functionreturntype->type.get())->simplepathsegement->identifier : "()";
    
    EnterFunctionContext(currentFunctionReturnType);
    // 进入函数作用域
    scopeTree->EnterScope(Scope::ScopeType::Function, &node);
    
    if (node.functionparameters) {
        for (const auto& param : node.functionparameters->functionparams) {
            param->accept(*this);
        }
    }
    
    if (node.blockexpression) {
        PushExpectedType(std::make_shared<SimpleType>(currentFunctionReturnType));
        node.blockexpression->accept(*this);
        PopExpectedType();
        
        // 检查函数体类型与返回类型是否兼容
        auto bodyType = GetInferredType(node.blockexpression.get());
        if (bodyType && !AreTypesCompatible(std::make_shared<SimpleType>(currentFunctionReturnType), bodyType)) {
            ReportTypeError(currentFunctionReturnType, bodyType->tostring(), node.blockexpression.get());
        }
    }
    
    scopeTree->ExitScope();
    ExitFunctionContext();
    currentFunctionReturnType = previousReturnType;
    
    PopNode();
}

void TypeInferenceChecker::visit(LetStatement& node) {
    PushNode(node);
    
    std::shared_ptr<SemanticType> type;
    if (node.type) {
        // 有显式类型注解
        node.type->accept(*this);
        type = GetInferredType(node.type.get());
    } else {
        // 类型推断：创建新的类型变量
        type = typeEnv->FreshTypeVariable();
    }
    
    if (node.patternnotopalt) {
        // 设置期望的类型用于模式检查
        PushExpectedType(type);
        node.patternnotopalt->accept(*this);
        PopExpectedType();
    }
    
    if (node.expression) {
        // 设置期望的类型
        PushExpectedType(type);
        node.expression->accept(*this);
        PopExpectedType();
        
        // 检查初始化表达式类型与声明类型是否兼容
        auto initType = GetInferredType(node.expression.get());
        if (initType && type) {
            AddTypeConstraint(initType, type);
        }
    }
    
    // 记录变量类型
    if (auto identPattern = dynamic_cast<IdentifierPattern*>(node.patternnotopalt.get())) {
        std::string varName = identPattern->identifier;
        SetVariableType(varName, type);
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(ExpressionStatement& node) {
    PushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
        // 表达式语句的类型是unit，但表达式的类型用于检查
        inferredTypes[&node] = std::make_shared<SimpleType>("()");
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(PathExpression& node) {
    PushNode(node);
    
    if (node.simplepath) {
        std::shared_ptr<SemanticType> type = InferPathType(node);
        if (type) {
            inferredTypes[&node] = type;
        } else {
            ReportError("Cannot infer type for path expression");
        }
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(CallExpression& node) {
    PushNode(node);
    
    auto type = InferCallType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(FieldExpression& node) {
    PushNode(node);
    
    auto type = InferFieldAccessType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(BinaryExpression& node) {
    PushNode(node);
    
    auto type = InferBinaryExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(IfExpression& node) {
    PushNode(node);
    
    auto type = InferIfExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(BlockExpression& node) {
    PushNode(node);
    
    scopeTree->EnterScope(Scope::ScopeType::Block, &node);
    for (const auto& stmt : node.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    
    // 处理尾表达式（如果有）
    if (node.expressionwithoutblock) {
        node.expressionwithoutblock->accept(*this);
    }
    
    // 推断块的类型
    auto type = InferBlockExpressionType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    scopeTree->ExitScope();
    
    PopNode();
}

void TypeInferenceChecker::visit(ReturnExpression& node) {
    PushNode(node);
    
    // 检查返回表达式类型与函数返回类型是否兼容
    if (node.expression) {
        // 设置期望的返回类型
        PushExpectedType(std::make_shared<SimpleType>(currentFunctionReturnType));
        node.expression->accept(*this);
        PopExpectedType();
        
        auto exprType = GetInferredType(node.expression.get());
        if (exprType) {
            AddTypeConstraint(exprType, std::make_shared<SimpleType>(currentFunctionReturnType));
        }
    } else {
        // 没有表达式的return，检查返回类型是否是unit
        if (currentFunctionReturnType != "()") {
            ReportError("Empty return in function with non-unit return type");
        }
    }
    
    // return表达式本身的类型是never (!)
    inferredTypes[&node] = std::make_shared<NeverType>();
    
    PopNode();
}

void TypeInferenceChecker::visit(AssignmentExpression& node) {
    PushNode(node);
    
    auto type = InferAssignmentType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(IdentifierPattern& node) {
    PushNode(node);
    
    // 获取期望的类型
    auto expectedType = GetExpectedType();
    if (expectedType) {
        // 记录变量类型
        std::string varName = node.identifier;
        SetVariableType(varName, expectedType);
        
        // 检查可变性
        if (node.hasmut) {
            // 可变绑定：类型系统需要支持可变性
            // 这里简化处理，实际需要更复杂的可变性跟踪
        }
    }
    
    PopNode();
}

void TypeInferenceChecker::visit(LiteralExpression& node) {
    PushNode(node);
    
    auto type = InferLiteralType(node);
    if (type) {
        inferredTypes[&node] = type;
    }
    
    PopNode();
}

// 类型推断具体实现
std::shared_ptr<SemanticType> TypeInferenceChecker::InferPathType(PathExpression& expr) {
    auto path = std::move(expr.simplepath);
    if (!path) return nullptr;
    
    return ResolvePathType(*path);
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferCallType(CallExpression& expr) {
    auto callee = expr.expression;
    auto params = expr.callparams;
    
    if (!callee) return nullptr;
    
    // 推断callee的类型
    callee->accept(*this);
    auto calleeType = GetInferredType(callee.get());
    
    // 推断参数类型
    std::vector<std::shared_ptr<SemanticType>> argTypes;
    if (params) {
        argTypes = InferArgumentTypes(*params);
    }
    
    // 解析函数类型
    // 这里需要根据callee的类型信息来解析
    // 简化处理：假设callee是路径表达式
    if (auto pathExpr = dynamic_cast<PathExpression*>(callee.get())) {
        if (pathExpr->simplepath) {
            auto segments = pathExpr->simplepath->simplepathsegements;
            if (!segments.empty()) {
                std::string functionName = segments.back()->identifier;
                return ResolveFunctionType(functionName, argTypes);
            }
        }
    }
    
    // 如果无法解析，创建函数类型变量
    auto returnType = typeEnv->FreshTypeVariable();
    return returnType;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferFieldAccessType(FieldExpression& expr) {
    auto baseExpr = expr.expression;
    
    if (!baseExpr) return nullptr;
    
    baseExpr->accept(*this);
    auto baseType = GetInferredType(baseExpr.get());
    if (!baseType) return nullptr;
    
    // 解析字段类型
    // 这里需要根据baseType的结构体/枚举信息来查找字段
    std::string baseTypeName = baseType->tostring();
    
    // 查找结构体字段
    auto fieldType = GetStructFieldType(baseTypeName, expr.identifier);
    if (fieldType) {
        return fieldType;
    }
    
    // 查找枚举variant
    auto variantType = GetEnumVariantType(baseTypeName, expr.identifier);
    if (variantType) {
        return variantType;
    }
    
    ReportUndefinedError(expr.identifier, "field", &expr);
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferBinaryExpressionType(BinaryExpression& expr) {
    auto left = expr.leftexpression;
    auto right = expr.rightexpression;
    Token op = expr.binarytype;
    
    if (!left || !right) return nullptr;
    
    left->accept(*this);
    right->accept(*this);
    auto leftType = GetInferredType(left.get());
    auto rightType = GetInferredType(right.get());
    if (!leftType || !rightType) return nullptr;
    
    // 根据操作符推断类型
    switch (op) {
        case Token::kPlus:
        case Token::kMinus:
        case Token::kStar:
        case Token::kSlash:
        case Token::kPercent:
            AddTypeConstraint(leftType, rightType);
            return leftType; 

        case Token::kShl:
        case Token::kShr:
        case Token::kAnd:
        case Token::kOr:
        case Token::kCaret:
            AddTypeConstraint(leftType, rightType);
            return leftType;
            
        case Token::kEqEq:
        case Token::kNe:
        case Token::kLt:
        case Token::kGt:
        case Token::kLe:
        case Token::kGe:
            AddTypeConstraint(leftType, rightType);
            return std::make_shared<SimpleType>("bool");
            
        case Token::kAndAnd:
        case Token::kOrOr:
            AddTypeConstraint(leftType, std::make_shared<SimpleType>("bool"));
            AddTypeConstraint(rightType, std::make_shared<SimpleType>("bool"));
            return std::make_shared<SimpleType>("bool");
            
        default:
            ReportError("Unsupported binary operator: " + to_string(op));
            return nullptr;
    }
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferIfExpressionType(IfExpression& expr) {
    auto condition = expr.conditions;
    auto ifBlock = expr.ifblockexpression;
    auto elseExpr = expr.elseexpression;
    
    if (!condition || !ifBlock) return nullptr;
    
    condition->accept(*this);
    auto condType = GetInferredType(condition->expression.get());
    
    if (condType) {
        AddTypeConstraint(condType, std::make_shared<SimpleType>("bool"));
    }
    
    ifBlock->accept(*this);
    auto ifType = GetInferredType(ifBlock.get());
    
    if (!elseExpr) {
        return std::make_shared<SimpleType>("()");
    }
    
    // 推断else分支类型
    elseExpr->accept(*this);
    auto elseType = GetInferredType(elseExpr.get());
    
    if (!ifType || !elseType) return nullptr;
    
    // 检查两个分支的类型兼容性
    if (ifType->tostring() != "!" && elseType->tostring() != "!") {
        try {
            AddTypeConstraint(ifType, elseType);
        } catch (const std::exception& e) {
            ReportError("If expression branches have incompatible types: " +
                       ifType->tostring() + " vs " + elseType->tostring());
            return std::make_shared<SimpleType>("type_error");
        }
    }
    
    return ifType;  // 返回统一的类型
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferBlockExpressionType(BlockExpression& block) {
    std::shared_ptr<SemanticType> lastType = std::make_shared<SimpleType>("()");
    
    // 推断所有语句的类型
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            // 对于表达式语句，获取其类型
            if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt.get())) {
                auto exprType = GetInferredType(exprStmt);
                if (exprType) {
                    lastType = exprType;
                }
            }
        }
    }
    
    // 如果有尾表达式，块的类型由尾表达式决定
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        auto tailType = GetInferredType(block.expressionwithoutblock.get());
        if (tailType) {
            lastType = tailType;
        }
    }
    
    // 如果块发散，则类型为never
    if (controlFlowAnalyzer->AlwaysDivergesAt(&block)) {
        return std::make_shared<NeverType>();
    }
    
    return lastType;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::InferAssignmentType(AssignmentExpression& expr) {
    auto lhs = expr.leftexpression;
    auto rhs = expr.rightexpression;
    
    if (!lhs || !rhs) return nullptr;
    
    // 检查左侧是否可赋值
    CheckAssignmentMutability(*lhs);
    
    // 推断右侧类型
    rhs->accept(*this);
    auto rhsType = GetInferredType(rhs.get());
    if (!rhsType) return nullptr;
    
    // 对于赋值表达式，我们需要推断左侧的类型
    // 但由于左侧可能是模式，这里简化处理
    inAssignmentContext = true;
    lhs->accept(*this);
    inAssignmentContext = false;
    
    auto lhsType = GetInferredType(lhs.get());
    
    if (lhsType && rhsType) {
        // 添加类型约束：右侧类型应该可以赋值给左侧
        AddTypeConstraint(rhsType, lhsType);
    }
    
    // 赋值表达式的类型是unit
    return std::make_shared<SimpleType>("()");
}

std::shared_ptr<SemanticType> TypeInferenceChecker::ResolvePathType(SimplePath& path) {
    auto segments = path.simplepathsegements;
    if (segments.empty()) {
        return nullptr;
    }
    
    // 单段路径：变量、常量或类型
    if (segments.size() == 1) {
        std::string name = segments[0]->identifier;
        // 查找符号
        auto symbol = ResolveSymbol(name);
        if (symbol) {
            return ResolveTypeFromSymbol(symbol);
        }
        
        ReportUndefinedError(name, "variable or type", &path);
        return nullptr;
    }
    
    // 多段路径：可能是模块路径、关联项等
    // 这里简化处理，只处理两段路径（如A::foo）
    if (segments.size() == 2) {
        std::string baseName = segments[0]->identifier;
        std::string itemName = segments[1]->identifier;
        auto baseSymbol = ResolveSymbol(baseName);
        if (baseSymbol && baseSymbol->type) {
            return ResolveAssociatedType(baseSymbol->type, itemName);
        }
    }
    
    ReportError("Complex path expressions not fully supported");
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::ResolveFunctionType(const std::string& functionName,
                                                               const std::vector<std::shared_ptr<SemanticType>>& argTypes) {
    // 查找函数符号
    auto symbol = ResolveSymbol(functionName);
    if (!symbol || symbol->kind != SymbolKind::Function) {
        ReportUndefinedError(functionName, "function", GetCurrentNode());
        return nullptr;
    }
    auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!funcSymbol) {
        return nullptr;
    }
    
    // 检查参数数量和类型
    if (funcSymbol->parameterTypes.size() != argTypes.size()) {
        ReportError("Function " + functionName + " expects " +
                   std::to_string(funcSymbol->parameterTypes.size()) + 
                   " arguments, but " + std::to_string(argTypes.size()) + " provided");
        return nullptr;
    }
    
    // 添加参数类型约束
    for (size_t i = 0; i < argTypes.size(); ++i) {
        if (i < funcSymbol->parameterTypes.size()) {
            AddTypeConstraint(argTypes[i], funcSymbol->parameterTypes[i]);
        }
    }
    
    return funcSymbol->returntype;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::ResolveMethodType(std::shared_ptr<SemanticType> receiverType,
                                                             const std::string& methodName,
                                                             const std::vector<std::shared_ptr<SemanticType>>& argTypes) {
    // 这里需要根据接收者类型查找方法
    // 简化处理：在符号表中查找方法
    
    std::string receiverTypeName = receiverType->tostring();
    
    // 构造完整的方法名（如Type::method）
    std::string fullMethodName = receiverTypeName + "::" + methodName;
    
    auto symbol = ResolveSymbol(fullMethodName);
    if (!symbol || symbol->kind != SymbolKind::Function) {
        ReportUndefinedError(methodName, "method", GetCurrentNode());
        return nullptr;
    }
    auto methodSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);
    if (!methodSymbol) {
        return nullptr;
    }
    
    // 检查参数（包括self参数）
    if (methodSymbol->parameterTypes.size() != argTypes.size() + 1) {
        ReportError("Method " + methodName + " expects " +
                   std::to_string(methodSymbol->parameterTypes.size()) + 
                   " arguments, but " + std::to_string(argTypes.size() + 1) + " provided");
        return nullptr;
    }
    
    // 第一个参数应该是self，检查接收者类型
    if (!methodSymbol->parameterTypes.empty()) {
        AddTypeConstraint(receiverType, methodSymbol->parameterTypes[0]);
    }
    
    // 添加其他参数类型约束
    for (size_t i = 0; i < argTypes.size(); ++i) {
        if (i + 1 < methodSymbol->parameterTypes.size()) {
            AddTypeConstraint(argTypes[i], methodSymbol->parameterTypes[i + 1]);
        }
    }
    
    return methodSymbol->returntype;
}

std::vector<std::shared_ptr<SemanticType>> TypeInferenceChecker::InferArgumentTypes(CallParams& params) {
    std::vector<std::shared_ptr<SemanticType>> argTypes;
    
    for (const auto& expr : std::move(params.expressions)) {
        expr->accept(*this);
        auto type = GetInferredType(expr.get());
        if (type) {
            argTypes.push_back(type);
        }
    }
    
    return argTypes;
}

// 类型约束处理
void TypeInferenceChecker::AddTypeConstraint(std::shared_ptr<SemanticType> actual, std::shared_ptr<SemanticType> expected) {
    try {
        typeEnv->unify(actual, expected);
    } catch (const std::exception& e) {
        ReportError("SemanticType unification failed: " + std::string(e.what()));
    }
}

void TypeInferenceChecker::SolveTypeConstraints() {
    // 类型约束已经在unify过程中解决
}

bool TypeInferenceChecker::AreTypesCompatible(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2) {
    try {
        typeEnv->unify(type1, type2);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// 符号解析
std::shared_ptr<Symbol> TypeInferenceChecker::ResolveSymbol(const std::string& name) {
    return scopeTree->LookupSymbol(name);
}

std::shared_ptr<SemanticType> TypeInferenceChecker::GetVariableType(const std::string& varName) {
    auto it = variableTypes.find(varName);
    if (it != variableTypes.end()) {
        return it->second;
    }
    
    // 在符号表中查找
    auto symbol = ResolveSymbol(varName);
    if (symbol) {
        return ResolveTypeFromSymbol(symbol);
    }
    
    return nullptr;
}

void TypeInferenceChecker::SetVariableType(const std::string& varName, std::shared_ptr<SemanticType> type) {
    variableTypes[varName] = type;
}

// 可变性检查
void TypeInferenceChecker::CheckMutability(const std::string& varName, ASTNode* usageContext) {
    auto symbol = ResolveSymbol(varName);
    if (symbol && symbol->kind == SymbolKind::Variable) {
        if (!symbol->ismutable && inAssignmentContext) {
            ReportError("Cannot assign to immutable variable '" + varName + "'");
        }
    }
}

void TypeInferenceChecker::CheckAssignmentMutability(Expression& lhs) {
    // 检查左侧表达式是否可赋值
    if (auto pathExpr = dynamic_cast<PathExpression*>(&lhs)) {
        if (pathExpr->simplepath) {
            auto segments = pathExpr->simplepath->simplepathsegements;
            if (segments.size() == 1) {
                std::string varName = segments[0]->identifier;
            CheckMutability(varName, &lhs);
            }
        }
    } else if (auto indexExpr = dynamic_cast<IndexExpression*>(&lhs)) {
        // 对于索引表达式，需要检查基础表达式的可变性
        if (auto pathExpr = dynamic_cast<PathExpression*>(indexExpr->expressionout.get())) {
            if (pathExpr->simplepath) {
                auto segments = pathExpr->simplepath->simplepathsegements;
                if (segments.size() == 1) {
                    std::string varName = segments[0]->identifier;
                    CheckMutability(varName, &lhs);
                }
            }
        }
    } else if (auto fieldExpr = dynamic_cast<FieldExpression*>(&lhs)) {
        // 字段赋值：需要检查字段的可变性
        // 这里简化处理
    }
}

// 错误处理
void TypeInferenceChecker::ReportError(const std::string& message) {
    std::cerr << "SemanticType Inference Error: " << message << std::endl;
    hasErrors = true;
}

void TypeInferenceChecker::ReportTypeError(const std::string& expected, const std::string& actual, ASTNode* context) {
    std::cerr << "SemanticType Error: expected " << expected << ", got " << actual << std::endl;
    hasErrors = true;
}

void TypeInferenceChecker::ReportUndefinedError(const std::string& name, const std::string& kind, ASTNode* context) {
    std::cerr << "Undefined " << kind << ": '" << name << "'" << std::endl;
    hasErrors = true;
}

// 辅助方法实现
void TypeInferenceChecker::PushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void TypeInferenceChecker::PopNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* TypeInferenceChecker::GetCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void TypeInferenceChecker::PushExpectedType(std::shared_ptr<SemanticType> type) {
    expectedTypeStack.push(type);
}

void TypeInferenceChecker::PopExpectedType() {
    if (!expectedTypeStack.empty()) {
        expectedTypeStack.pop();
    }
}

std::shared_ptr<SemanticType> TypeInferenceChecker::GetExpectedType() {
    return expectedTypeStack.empty() ? nullptr : expectedTypeStack.top();
}

void TypeInferenceChecker::EnterFunctionContext(const std::string& returnType) {
    currentFunctionReturnType = returnType;
}

void TypeInferenceChecker::ExitFunctionContext() {
    currentFunctionReturnType.clear();
}

void TypeInferenceChecker::EnterImplContext(std::shared_ptr<SemanticType> selfType) {
    currentSelfType = selfType;
}

void TypeInferenceChecker::ExitImplContext() {
    currentSelfType = nullptr;
}

// 其他辅助方法实现...
std::shared_ptr<SemanticType> TypeInferenceChecker::ResolveTypeFromSymbol(std::shared_ptr<Symbol> symbol) {
    if (!symbol) return nullptr;
    if (symbol->type) {
        return symbol->type;
    }
    
    // 根据符号种类返回默认类型
    switch (symbol->kind) {
        case SymbolKind::Variable:
            return typeEnv->FreshTypeVariable();
            
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

std::shared_ptr<SemanticType> TypeInferenceChecker::GetStructFieldType(const std::string& structName,
                                                           const std::string& fieldName) {
    auto structSymbol = std::dynamic_pointer_cast<StructSymbol>(ResolveSymbol(structName));
    if (!structSymbol) {
        return nullptr;
    }
    
    for (const auto& field : structSymbol->fields) {
        if (field->name == fieldName) {
            return field->type;
        }
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::GetEnumVariantType(const std::string& enumName,
                                                           const std::string& variantName) {
    auto enumSymbol = std::dynamic_pointer_cast<EnumSymbol>(ResolveSymbol(enumName));
    if (!enumSymbol) {
        return nullptr;
    }
    
    for (const auto& variant : enumSymbol->variants) {
        if (variant->name == variantName) {
            return variant->type;
        }
    }
    
    return nullptr;
}

std::shared_ptr<SemanticType> TypeInferenceChecker::ResolveAssociatedType(std::shared_ptr<SemanticType> baseType,
                                                                  const std::string& associatedItem) {
    // 这里需要根据基础类型查找关联项
    // 简化处理：在符号表中查找
    std::string fullName = baseType->tostring() + "::" + associatedItem;
    auto symbol = ResolveSymbol(fullName);
    if (symbol) {
        return ResolveTypeFromSymbol(symbol);
    }
    
    return nullptr;
}

// 字面量类型推断实现
std::shared_ptr<SemanticType> TypeInferenceChecker::InferLiteralType(LiteralExpression& expr) {
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
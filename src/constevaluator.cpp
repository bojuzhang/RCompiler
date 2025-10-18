#include "constevaluator.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <sstream>
#include <utility>

ConstantEvaluator::ConstantEvaluator(std::shared_ptr<ScopeTree> scopeTree) 
    : scopeTree(scopeTree) {}

bool ConstantEvaluator::EvaluateConstants() {
    hasErrors = false;
    return !hasErrors;
}

bool ConstantEvaluator::HasEvaluationErrors() const {
    return hasErrors;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::GetConstantValue(const std::string& name) {
    auto it = constantValues.find(name);
    return it != constantValues.end() ? it->second : nullptr;
}

void ConstantEvaluator::visit(Crate& node) {
    PushNode(node);
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    PopNode();
}

void ConstantEvaluator::visit(Item& node) {
    PushNode(node);
    if (node.item) {
        node.item->accept(*this);
    }
    PopNode();
}

void ConstantEvaluator::visit(ConstantItem& node) {
    PushNode(node);
    
    std::string constName = node.identifier;
    
    bool previousConstContext = inConstContext;
    inConstContext = true;
    
    // 求值常量表达式
    auto value = EvaluateExpression(*node.expression);
    if (value) {
        constantValues[constName] = value;
    } else {
        ReportError("Cannot evaluate constant '" + constName + "' at compile time");
    }
    
    inConstContext = previousConstContext;
    PopNode();
}

void ConstantEvaluator::visit(Function& node) {
    // 函数定义中可能包含常量表达式（如默认参数），但这里简化处理
    PushNode(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    PopNode();
}

void ConstantEvaluator::visit(Statement& node) {
    PushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    
    PopNode();
}

void ConstantEvaluator::visit(LetStatement& node) {
    PushNode(node);
    
    if (inConstContext) {
        auto initValue = EvaluateExpression(*node.expression);
        if (initValue) {
            // 记录常量初始值（用于后续分析）
            std::cerr << "Let statement initializer evaluated to: " << initValue->toString() << std::endl;
        }
    }
    
    PopNode();
}

void ConstantEvaluator::visit(ExpressionStatement& node) {
    PushNode(node);
    
    if (inConstContext) {
        auto value = EvaluateExpression(*node.astnode);
        if (value) {
            std::cerr << "Expression statement evaluated to: " << value->toString() << std::endl;
        }
    }
    
    PopNode();
}

void ConstantEvaluator::visit(BlockExpression& node) {
    PushNode(node);
    
    if (inConstContext) {
        for (const auto& stmt : node.statements) {
            if (stmt) {
                stmt->accept(*this);
            }
        }
        
        // 处理尾表达式（如果有）
        if (node.expressionwithoutblock) {
            node.expressionwithoutblock->accept(*this);
        }
    } else {
        // 即使不在常量上下文中，也要处理函数内部的常量声明
        EvaluateBlockExpression(node);
    }
    
    PopNode();
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateExpression(Expression& expr) {
    // 检查是否已经是编译时常量
    if (!IsCompileTimeConstant(expr)) {
        return nullptr;
    }
    
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        return EvaluateLiteral(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        return EvaluateBinaryExpression(*binary);
    } else if (auto unary = dynamic_cast<UnaryExpression*>(&expr)) {
        return EvaluateUnaryExpression(*unary);
    } else if (auto array = dynamic_cast<ArrayExpression*>(&expr)) {
        return EvaluateArrayExpression(*array);
    } else if (auto path = dynamic_cast<PathExpression*>(&expr)) {
        return EvaluatePathExpression(*path);
    } else if (auto block = dynamic_cast<BlockExpression*>(&expr)) {
        return EvaluateBlockExpression(*block);
    } else if (auto ifExpr = dynamic_cast<IfExpression*>(&expr)) {
        return EvaluateIfExpression(*ifExpr);
    }
    
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateLiteral(LiteralExpression& expr) {
    Token tokenType = expr.tokentype;
    const std::string& valueStr = expr.literal;
    
    switch (tokenType) {
        case Token::kINTEGER_LITERAL:
            try {
                int64_t value = std::stoll(valueStr);
                return std::make_shared<IntConstant>(value);
            } catch (const std::exception& e) {
                ReportError("Invalid integer literal: " + valueStr);
                return nullptr;
            }
            
        case Token::kCHAR_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '\'' && valueStr.back() == '\'') {
                // 简化处理字符字面量
                if (valueStr.length() == 3) { // 'x'
                    return std::make_shared<CharConstant>(valueStr[1]);
                }
            }
            ReportError("Invalid char literal: " + valueStr);
            return nullptr;
            
        case Token::kSTRING_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '"' && valueStr.back() == '"') {
                std::string content = valueStr.substr(1, valueStr.length() - 2);
                return std::make_shared<StringConstant>(content);
            }
            ReportError("Invalid string literal: " + valueStr);
            return nullptr;
            
        case Token::ktrue:
            return std::make_shared<BoolConstant>(true);
            
        case Token::kfalse:
            return std::make_shared<BoolConstant>(false);
            
        default:
            ReportError("Unsupported literal type for constant evaluation");
            return nullptr;
    }
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateBinaryExpression(BinaryExpression& expr) {
    auto leftValue = EvaluateExpression(*expr.leftexpression);
    auto rightValue = EvaluateExpression(*expr.rightexpression);
    
    if (!leftValue || !rightValue) {
        return nullptr;
    }
    
    // 类型检查：左右操作数类型必须兼容
    if (typeid(*leftValue) != typeid(*rightValue)) {
        ReportError("Type mismatch in binary expression");
        return nullptr;
    }
    
    Token op = expr.binarytype;
    
    if (auto leftInt = dynamic_cast<IntConstant*>(leftValue.get())) {
        auto rightInt = dynamic_cast<IntConstant*>(rightValue.get());
        int64_t left = leftInt->getValue();
        int64_t right = rightInt->getValue();
        
        switch (op) {
            case Token::kPlus:
                return std::make_shared<IntConstant>(left + right);
            case Token::kMinus:
                return std::make_shared<IntConstant>(left - right);
            case Token::kStar:
                return std::make_shared<IntConstant>(left * right);
            case Token::kSlash:
                if (right == 0) {
                    ReportError("Division by zero in constant expression");
                    return nullptr;
                }
                return std::make_shared<IntConstant>(left / right);
            case Token::kPercent:
                if (right == 0) {
                    ReportError("Modulo by zero in constant expression");
                    return nullptr;
                }
                return std::make_shared<IntConstant>(left % right);
            case Token::kShl:
                return std::make_shared<IntConstant>(left << right);
            case Token::kShr:
                return std::make_shared<IntConstant>(left >> right);
            case Token::kAnd:
                return std::make_shared<IntConstant>(left & right);
            case Token::kOr:
                return std::make_shared<IntConstant>(left | right);
            case Token::kCaret:
                return std::make_shared<IntConstant>(left ^ right);
            case Token::kEqEq:
                return std::make_shared<BoolConstant>(left == right);
            case Token::kNe:
                return std::make_shared<BoolConstant>(left != right);
            case Token::kLt:
                return std::make_shared<BoolConstant>(left < right);
            case Token::kGt:
                return std::make_shared<BoolConstant>(left > right);
            case Token::kLe:
                return std::make_shared<BoolConstant>(left <= right);
            case Token::kGe:
                return std::make_shared<BoolConstant>(left >= right);
            default:
                ReportError("Unsupported binary operator for integers: " + to_string(op));
                return nullptr;
        }
    } else if (auto leftBool = dynamic_cast<BoolConstant*>(leftValue.get())) {
        auto rightBool = dynamic_cast<BoolConstant*>(rightValue.get());
        bool left = leftBool->getValue();
        bool right = rightBool->getValue();
        
        switch (op) {
            case Token::kAndAnd:
                return std::make_shared<BoolConstant>(left && right);
            case Token::kOrOr:
                return std::make_shared<BoolConstant>(left || right);
            case Token::kEqEq:
                return std::make_shared<BoolConstant>(left == right);
            case Token::kNe:
                return std::make_shared<BoolConstant>(left != right);
            default:
                ReportError("Unsupported binary operator for booleans: " + to_string(op));
                return nullptr;
        }
    }
    
    ReportError("Unsupported types for binary operation");
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateUnaryExpression(UnaryExpression& expr) {
    auto operandValue = EvaluateExpression(*expr.expression);
    if (!operandValue) {
        return nullptr;
    }
    
    Token op = expr.unarytype;
    
    if (auto intVal = dynamic_cast<IntConstant*>(operandValue.get())) {
        int64_t value = intVal->getValue();
        switch (op) {
            case Token::kMinus:
                return std::make_shared<IntConstant>(-value);
            case Token::kNot:
                return std::make_shared<IntConstant>(~value);
            default:
                ReportError("Unsupported unary operator for integers: " + to_string(op));
                return nullptr;
        }
    } else if (auto boolVal = dynamic_cast<BoolConstant*>(operandValue.get())) {
        bool value = boolVal->getValue();
        switch (op) {
            case Token::kNot:
                return std::make_shared<BoolConstant>(!value);
            default:
                ReportError("Unsupported unary operator for booleans: " + to_string(op));
                return nullptr;
        }
    }
    
    ReportError("Unsupported type for unary operation");
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateArrayExpression(ArrayExpression& expr) {
    if (!expr.arrayelements) {
        return nullptr;
    }
    
    std::vector<std::shared_ptr<ConstantValue>> evaluatedElements;
    for (const auto& element : expr.arrayelements->expressions) {
        auto elementValue = EvaluateExpression(*element);
        if (!elementValue) {
            return nullptr;
        }
        evaluatedElements.push_back(elementValue);
    }
    
    return std::make_shared<ArrayConstant>(std::move(evaluatedElements));
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluatePathExpression(PathExpression& expr) {
    if (!expr.simplepath || expr.simplepath->simplepathsegements.empty()) {
        return nullptr;
    }
    
    // 简化处理：只处理单段路径（常量名）
    std::string constName = expr.simplepath->simplepathsegements[0]->identifier;
    
    auto it = constantValues.find(constName);
    if (it != constantValues.end()) {
        return it->second;
    }
    
    // 尝试从作用域树中查找符号
    if (scopeTree) {
        auto symbol = scopeTree->LookupSymbol(constName);
        if (symbol && symbol->kind == SymbolKind::Constant) {
            // 如果是常量符号但还没有求值，尝试求值
            ReportError("Constant '" + constName + "' found but not evaluated yet");
            return nullptr;
        }
    }
    
    ReportError("Undefined constant in constant expression: " + constName);
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateBlockExpression(BlockExpression& expr) {
    // 块表达式在常量上下文中：遍历所有语句
    std::shared_ptr<ConstantValue> lastValue = nullptr;
    
    for (const auto& stmt : expr.statements) {
        if (stmt) {
            stmt->accept(*this);
            if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt.get())) {
                if (exprStmt->astnode) {
                    lastValue = EvaluateExpression(*exprStmt->astnode);
                }
            }
        }
    }
    
    // 如果有尾表达式，块的值由尾表达式决定
    if (expr.expressionwithoutblock) {
        lastValue = EvaluateExpression(*expr.expressionwithoutblock);
    }
    
    return lastValue;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::EvaluateIfExpression(IfExpression& expr) {
    auto conditionValue = EvaluateExpression(*expr.conditions->expression);
    if (!conditionValue) {
        return nullptr;
    }
    
    // 条件必须是布尔值
    auto boolCondition = dynamic_cast<BoolConstant*>(conditionValue.get());
    if (!boolCondition) {
        ReportError("If condition must be boolean in constant expression");
        return nullptr;
    }
    
    if (boolCondition->getValue()) {
        return EvaluateExpression(*expr.ifblockexpression);
    } else {
        if (expr.elseexpression) {
            return EvaluateExpression(*expr.elseexpression);
        } else {
            // 没有else分支，返回unit类型（这里用nullptr表示）
            return nullptr;
        }
    }
}

bool ConstantEvaluator::IsCompileTimeConstant(Expression& expr) {
    // 检查表达式是否可以在编译时求值
    if (dynamic_cast<LiteralExpression*>(&expr)) {
        return true;
    } else if (auto path = dynamic_cast<PathExpression*>(&expr)) {
        // 路径表达式必须是常量
        if (path->simplepath) {
            auto segments = path->simplepath->simplepathsegements;
            if (segments.size() == 1) {
                std::string name = segments[0]->identifier;
                return constantValues.find(name) != constantValues.end();
            }
        }
        return false;
    } else if (dynamic_cast<BinaryExpression*>(&expr)) {
        auto binary = dynamic_cast<BinaryExpression*>(&expr);
        return IsCompileTimeConstant(*binary->leftexpression) &&
               IsCompileTimeConstant(*binary->rightexpression);
    } else if (dynamic_cast<UnaryExpression*>(&expr)) {
        auto unary = dynamic_cast<UnaryExpression*>(&expr);
        return IsCompileTimeConstant(*unary->expression);
    } else if (dynamic_cast<ArrayExpression*>(&expr)) {
        auto array = dynamic_cast<ArrayExpression*>(&expr);
        if (array && array->arrayelements) {
            for (const auto& element : array->arrayelements->expressions) {
                if (!IsCompileTimeConstant(*element)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    
    return false;
}

void ConstantEvaluator::ReportError(const std::string& message) {
    std::cerr << "Constant Evaluation Error: " << message << std::endl;
    hasErrors = true;
}

void ConstantEvaluator::PushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void ConstantEvaluator::PopNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* ConstantEvaluator::GetCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}
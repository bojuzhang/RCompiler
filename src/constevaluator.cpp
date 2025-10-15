#include "constevaluator.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <sstream>
#include <utility>

ConstantEvaluator::ConstantEvaluator(std::shared_ptr<ScopeTree> scopeTree) 
    : scopeTree(scopeTree) {}

bool ConstantEvaluator::evaluateConstants() {
    hasErrors = false;
    return !hasErrors;
}

bool ConstantEvaluator::hasEvaluationErrors() const {
    return hasErrors;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::getConstantValue(const std::string& name) {
    auto it = constantValues.find(name);
    return it != constantValues.end() ? it->second : nullptr;
}

void ConstantEvaluator::visit(Crate& node) {
    pushNode(node);
    
    // 遍历所有item，收集常量
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    popNode();
}

void ConstantEvaluator::visit(Item& node) {
    pushNode(node);
    
    if (node.item) {
        node.item->accept(*this);
    }
    
    popNode();
}

void ConstantEvaluator::visit(ConstantItem& node) {
    pushNode(node);
    
    std::string constName = node.identifier;
    
    // 设置常量求值上下文
    bool previousConstContext = inConstContext;
    inConstContext = true;
    
    // 求值常量表达式
    auto value = evaluateExpression(*node.expression);
    if (value) {
        constantValues[constName] = value;
        std::cerr << "Constant '" << constName << "' = " << value->toString() << std::endl;
    } else {
        reportError("Cannot evaluate constant '" + constName + "' at compile time");
    }
    
    inConstContext = previousConstContext;
    popNode();
}

void ConstantEvaluator::visit(Function& node) {
    // 函数定义中可能包含常量表达式（如默认参数），但这里简化处理
    pushNode(node);
    
    // 不处理函数体中的常量，只处理函数签名中的常量表达式
    if (node.functionparameters) {
        for (const auto& param : node.functionparameters->functionparams) {
            // 检查参数默认值（如果有）
        }
    }
    
    popNode();
}

void ConstantEvaluator::visit(Statement& node) {
    pushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    
    popNode();
}

void ConstantEvaluator::visit(LetStatement& node) {
    pushNode(node);
    
    // 在常量上下文中，可以求值let语句的初始值
    if (inConstContext) {
        auto initValue = evaluateExpression(*node.expression);
        if (initValue) {
            // 记录常量初始值（用于后续分析）
            std::cerr << "Let statement initializer evaluated to: " << initValue->toString() << std::endl;
        }
    }
    
    popNode();
}

void ConstantEvaluator::visit(ExpressionStatement& node) {
    pushNode(node);
    
    // 在常量上下文中，可以求值表达式语句
    if (inConstContext) {
        auto value = evaluateExpression(*node.astnode);
        if (value) {
            std::cerr << "Expression statement evaluated to: " << value->toString() << std::endl;
        }
    }
    
    popNode();
}


// 表达式求值
std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateExpression(Expression& expr) {
    // 检查是否已经是编译时常量
    if (!isCompileTimeConstant(expr)) {
        return nullptr;
    }
    
    // 根据表达式类型分派到具体的求值方法
    if (auto literal = dynamic_cast<LiteralExpression*>(&expr)) {
        return evaluateLiteral(*literal);
    } else if (auto binary = dynamic_cast<BinaryExpression*>(&expr)) {
        return evaluateBinaryExpression(*binary);
    } else if (auto unary = dynamic_cast<UnaryExpression*>(&expr)) {
        return evaluateUnaryExpression(*unary);
    } else if (auto array = dynamic_cast<ArrayExpression*>(&expr)) {
        return evaluateArrayExpression(*array);
    } else if (auto path = dynamic_cast<PathExpression*>(&expr)) {
        return evaluatePathExpression(*path);
    } else if (auto block = dynamic_cast<BlockExpression*>(&expr)) {
        return evaluateBlockExpression(*block);
    } else if (auto ifExpr = dynamic_cast<IfExpression*>(&expr)) {
        return evaluateIfExpression(*ifExpr);
    }
    
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateLiteral(LiteralExpression& expr) {
    Token tokenType = expr.tokentype;
    const std::string& valueStr = expr.literal;
    
    switch (tokenType) {
        case Token::kINTEGER_LITERAL:
            try {
                int64_t value = std::stoll(valueStr);
                return std::make_shared<IntConstant>(value);
            } catch (const std::exception& e) {
                reportError("Invalid integer literal: " + valueStr);
                return nullptr;
            }
            
        case Token::kCHAR_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '\'' && valueStr.back() == '\'') {
                // 简化处理字符字面量
                if (valueStr.length() == 3) { // 'x'
                    return std::make_shared<CharConstant>(valueStr[1]);
                }
            }
            reportError("Invalid char literal: " + valueStr);
            return nullptr;
            
        case Token::kSTRING_LITERAL:
            if (!valueStr.empty() && valueStr[0] == '"' && valueStr.back() == '"') {
                // 去除引号
                std::string content = valueStr.substr(1, valueStr.length() - 2);
                return std::make_shared<StringConstant>(content);
            }
            reportError("Invalid string literal: " + valueStr);
            return nullptr;
            
        case Token::ktrue:
            return std::make_shared<BoolConstant>(true);
            
        case Token::kfalse:
            return std::make_shared<BoolConstant>(false);
            
        default:
            reportError("Unsupported literal type for constant evaluation");
            return nullptr;
    }
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateBinaryExpression(BinaryExpression& expr) {
    auto leftValue = evaluateExpression(*expr.leftexpression);
    auto rightValue = evaluateExpression(*expr.rightexpression);
    
    if (!leftValue || !rightValue) {
        return nullptr;
    }
    
    // 类型检查：左右操作数类型必须兼容
    if (typeid(*leftValue) != typeid(*rightValue)) {
        reportError("Type mismatch in binary expression");
        return nullptr;
    }
    
    Token op = expr.binarytype;
    
    // 整数运算
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
                    reportError("Division by zero in constant expression");
                    return nullptr;
                }
                return std::make_shared<IntConstant>(left / right);
            case Token::kPercent:
                if (right == 0) {
                    reportError("Modulo by zero in constant expression");
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
                reportError("Unsupported binary operator for integers: " + to_string(op));
                return nullptr;
        }
    }
    // 布尔运算
    else if (auto leftBool = dynamic_cast<BoolConstant*>(leftValue.get())) {
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
                reportError("Unsupported binary operator for booleans: " + to_string(op));
                return nullptr;
        }
    }
    
    reportError("Unsupported types for binary operation");
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateUnaryExpression(UnaryExpression& expr) {
    auto operandValue = evaluateExpression(*expr.expression);
    if (!operandValue) {
        return nullptr;
    }
    
    Token op = expr.unarytype;
    
    // 整数一元运算
    if (auto intVal = dynamic_cast<IntConstant*>(operandValue.get())) {
        int64_t value = intVal->getValue();
        
        switch (op) {
            case Token::kMinus:
                return std::make_shared<IntConstant>(-value);
            case Token::kNot:
                return std::make_shared<IntConstant>(~value);
            default:
                reportError("Unsupported unary operator for integers: " + to_string(op));
                return nullptr;
        }
    }
    // 布尔一元运算
    else if (auto boolVal = dynamic_cast<BoolConstant*>(operandValue.get())) {
        bool value = boolVal->getValue();
        
        switch (op) {
            case Token::kNot:
                return std::make_shared<BoolConstant>(!value);
            default:
                reportError("Unsupported unary operator for booleans: " + to_string(op));
                return nullptr;
        }
    }
    
    reportError("Unsupported type for unary operation");
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateArrayExpression(ArrayExpression& expr) {
    if (!expr.arrayelements) {
        return nullptr;
    }
    
    std::vector<std::shared_ptr<ConstantValue>> evaluatedElements;
    
    for (const auto& element : expr.arrayelements->expressions) {
        auto elementValue = evaluateExpression(*element);
        if (!elementValue) {
            return nullptr;
        }
        evaluatedElements.push_back(elementValue);
    }
    
    return std::make_shared<ArrayConstant>(std::move(evaluatedElements));
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluatePathExpression(PathExpression& expr) {
    if (!expr.simplepath) {
        return nullptr;
    }
    
    // // 简化处理：只处理单段路径（常量名）
    // auto segments = path;
    // if (segments.size() != 1) {
    //     reportError("Complex path expressions not supported in constant evaluation");
    //     return nullptr;
    // }
    
    // std::string constName = segments[0]->getIdentifier();
    
    // // 查找常量值
    // auto it = constantValues.find(constName);
    // if (it != constantValues.end()) {
    //     return it->second;
    // }
    
    // reportError("Undefined constant in constant expression: " + constName);
    return nullptr;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateBlockExpression(BlockExpression& expr) {
    // 块表达式在常量上下文中：遍历所有语句
    std::shared_ptr<ConstantValue> lastValue = nullptr;
    
    // 处理所有语句
    for (const auto& stmt : expr.statements) {
        if (stmt) {
            stmt->accept(*this);
            // 对于表达式语句，尝试获取其值
            if (auto exprStmt = dynamic_cast<ExpressionStatement*>(stmt.get())) {
                if (exprStmt->astnode) {
                    lastValue = evaluateExpression(*exprStmt->astnode);
                }
            }
        }
    }
    
    // 如果有尾表达式，块的值由尾表达式决定
    if (expr.expressionwithoutblock) {
        lastValue = evaluateExpression(*expr.expressionwithoutblock);
    }
    
    return lastValue;
}

std::shared_ptr<ConstantValue> ConstantEvaluator::evaluateIfExpression(IfExpression& expr) {
    // 求值条件
    auto conditionValue = evaluateExpression(*expr.conditions->expression);
    if (!conditionValue) {
        return nullptr;
    }
    
    // 条件必须是布尔值
    auto boolCondition = dynamic_cast<BoolConstant*>(conditionValue.get());
    if (!boolCondition) {
        reportError("If condition must be boolean in constant expression");
        return nullptr;
    }
    
    if (boolCondition->getValue()) {
        // 求值if分支
        return evaluateExpression(*expr.ifblockexpression);
    } else {
        // 求值else分支（如果有）
        if (expr.elseexpression) {
            return evaluateExpression(*expr.elseexpression);
        } else {
            // 没有else分支，返回unit类型（这里用nullptr表示）
            return nullptr;
        }
    }
}

// 工具方法
bool ConstantEvaluator::isCompileTimeConstant(Expression& expr) {
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
        // 二元表达式：递归检查操作数
        auto binary = dynamic_cast<BinaryExpression*>(&expr);
        return isCompileTimeConstant(*binary->leftexpression) && 
               isCompileTimeConstant(*binary->rightexpression);
    } else if (dynamic_cast<UnaryExpression*>(&expr)) {
        // 一元表达式：递归检查操作数
        auto unary = dynamic_cast<UnaryExpression*>(&expr);
        return isCompileTimeConstant(*unary->expression);
    } else if (dynamic_cast<ArrayExpression*>(&expr)) {
        // 数组表达式：检查所有元素
        auto array = dynamic_cast<ArrayExpression*>(&expr);
        if (array && array->arrayelements) {
            for (const auto& element : array->arrayelements->expressions) {
                if (!isCompileTimeConstant(*element)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    
    return false;
}

void ConstantEvaluator::reportError(const std::string& message) {
    std::cerr << "Constant Evaluation Error: " << message << std::endl;
    hasErrors = true;
}

void ConstantEvaluator::pushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void ConstantEvaluator::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* ConstantEvaluator::getCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}
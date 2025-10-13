#include "controlflow.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <utility>

ControlFlowAnalyzer::ControlFlowAnalyzer(std::shared_ptr<ScopeTree> scopeTree,
                                       std::shared_ptr<ConstantEvaluator> constantEvaluator)
    : scopeTree(scopeTree), constantEvaluator(constantEvaluator) {}

bool ControlFlowAnalyzer::analyzeControlFlow() {
    hasErrors = false;
    return !hasErrors;
}

bool ControlFlowAnalyzer::hasAnalysisErrors() const {
    return hasErrors;
}

ControlFlow ControlFlowAnalyzer::getControlFlow(ASTNode* node) const {
    auto it = nodeControlFlow.find(node);
    return it != nodeControlFlow.end() ? it->second : ControlFlow::Continues;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::getNodeType(ASTNode* node) const {
    auto it = nodeTypes.find(node);
    return it != nodeTypes.end() ? it->second : nullptr;
}

bool ControlFlowAnalyzer::alwaysDivergesAt(ASTNode* node) const {
    auto it = alwaysDiverges.find(node);
    return it != alwaysDiverges.end() ? it->second : false;
}

void ControlFlowAnalyzer::visit(Crate& node) {
    pushNode(node);
    
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    popNode();
}

void ControlFlowAnalyzer::visit(Function& node) {
    pushNode(node);
    
    // 分析函数体控制流
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
        
        // 检查函数返回类型
        auto returnType = std::move(node.functionreturntype->type);
        auto bodyType = getNodeType(body.get());
        
        // 如果函数体发散，则不需要返回语句
        if (bodyType && bodyType->tostring() != "!" && getControlFlow(body.get()) == ControlFlow::Diverges) {
            // 需要检查是否有return语句或发散表达式
        }
    }
    
    popNode();
}

void ControlFlowAnalyzer::visit(ConstantItem& node) {
    pushNode(node);
    
    // 设置常量上下文
    bool previousConstContext = inConstContext;
    inConstContext = true;
    
    // 分析常量表达式
    auto expr = std::move(node.expression);
    if (expr) {
        expr->accept(*this);
        
        // 检查是否可以在编译时求值
        if (!constantEvaluator->getConstantValue(node.identifier)) {
            reportError("Constant '" + node.identifier + "' cannot be evaluated at compile time");
        }
    }
    
    inConstContext = previousConstContext;
    popNode();
}

void ControlFlowAnalyzer::visit(BlockExpression& node) {
    pushNode(node);
    
    // 进入块作用域
    scopeTree->enterScope(Scope::ScopeType::Block, &node);
    
    // 分析所有语句
    for (const auto& stmt : node.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    
    // 分析尾表达式（如果有）
    if (node.expressionwithoutblock) {
        node.expressionwithoutblock->accept(*this);
    }
    
    // 分析块的控制流
    ControlFlow flow = analyzeBlockControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断块的类型
    std::shared_ptr<SemanticType> type = inferBlockType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    // 退出块作用域
    scopeTree->exitScope();
    
    popNode();
}

void ControlFlowAnalyzer::visit(InfiniteLoopExpression& node) {
    pushNode(node);
    
    enterLoop();
    
    // 分析循环体
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
    }
    
    // 无限循环本身发散（除非有break）
    ControlFlow bodyFlow = getControlFlow(body.get());
    if (bodyFlow != ControlFlow::Breaks) {
        // 循环体没有break，整个循环发散
        nodeControlFlow[&node] = ControlFlow::Diverges;
        nodeTypes[&node] = std::make_shared<NeverType>();
        alwaysDiverges[&node] = true;
    } else {
        // 循环体有break，类型由break表达式决定
        nodeControlFlow[&node] = ControlFlow::Continues;
        // 这里需要统一所有break的类型（简化处理）
        nodeTypes[&node] = std::make_shared<SimpleType>("()");
        alwaysDiverges[&node] = false;
    }
    
    exitLoop();
    popNode();
}

void ControlFlowAnalyzer::visit(PredicateLoopExpression& node) {
    pushNode(node);
    
    enterLoop();
    
    // 分析条件
    auto conditions = std::move(node.conditions);
    if (conditions) {
        conditions->accept(*this);
    }
    
    // 分析循环体
    auto body = std::move(node.blockexpression);
    if (body) {
        body->accept(*this);
    }
    
    // 谓词循环：条件为false时退出
    // 控制流分析更复杂，这里简化处理
    nodeControlFlow[&node] = ControlFlow::Continues;
    nodeTypes[&node] = std::make_shared<SimpleType>("()");
    alwaysDiverges[&node] = false;
    
    exitLoop();
    popNode();
}

void ControlFlowAnalyzer::visit(IfExpression& node) {
    pushNode(node);
    
    // 分析条件
    auto conditions = std::move(node.conditions);
    if (conditions) {
        conditions->accept(*this);
    }
    
    // 分析if分支
    auto ifBlock = std::move(node.ifblockexpression);
    ifBlock->accept(*this);
    
    // 分析else分支（如果有）
    auto elseExpr = std::move(node.elseexpression);
    if (elseExpr) {
        elseExpr->accept(*this);
    }
    
    // 分析控制流
    ControlFlow flow = analyzeIfControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断类型
    std::shared_ptr<SemanticType> type = inferIfType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    popNode();
}

void ControlFlowAnalyzer::visit(BreakExpression& node) {
    pushNode(node);
    
    // 检查break是否在循环内
    checkBreakContinueValidity(node, Token::kbreak);
    
    // break表达式发散
    nodeControlFlow[&node] = ControlFlow::Breaks;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    // 分析break值（如果有）
    auto expr = std::move(node.expression);
    if (expr) {
        expr->accept(*this);
    }
    
    popNode();
}

void ControlFlowAnalyzer::visit(ContinueExpression& node) {
    pushNode(node);
    
    // 检查continue是否在循环内
    checkBreakContinueValidity(node, Token::kcontinue);
    
    // continue表达式发散
    nodeControlFlow[&node] = ControlFlow::ContinuesLoop;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    popNode();
}

void ControlFlowAnalyzer::visit(ReturnExpression& node) {
    pushNode(node);
    
    // return表达式发散
    nodeControlFlow[&node] = ControlFlow::Returns;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    // 分析返回值
    auto expr = std::move(node.expression);
    if (expr) {
        expr->accept(*this);
    }
    
    popNode();
}

void ControlFlowAnalyzer::visit(LetStatement& node) {
    pushNode(node);
    
    // 分析初始值表达式
    auto expr = std::move(node.expression);
    if (expr) {
        expr->accept(*this);
    }
    
    // let语句不影响控制流
    nodeControlFlow[&node] = ControlFlow::Continues;
    
    popNode();
}

void ControlFlowAnalyzer::visit(ExpressionStatement& node) {
    pushNode(node);
    
    // 分析表达式
    auto expr = std::move(node.astnode);
    if (expr) {
        expr->accept(*this);
        
        // 表达式语句的控制流由表达式决定
        nodeControlFlow[&node] = getControlFlow(expr.get());
        nodeTypes[&node] = getNodeType(expr.get());
        alwaysDiverges[&node] = alwaysDivergesAt(expr.get());
    }
    
    popNode();
}

// 控制流分析方法
ControlFlow ControlFlowAnalyzer::analyzeBlockControlFlow(BlockExpression& block) {
    ControlFlow resultFlow = ControlFlow::Continues;
    
    // 分析所有语句
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            resultFlow = getControlFlow(stmt.get());
            // 如果遇到发散语句，可以提前退出
            if (resultFlow == ControlFlow::Diverges ||
                resultFlow == ControlFlow::Returns ||
                resultFlow == ControlFlow::Breaks ||
                resultFlow == ControlFlow::ContinuesLoop) {
                break;
            }
        }
    }
    
    // 如果有尾表达式，分析它
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        resultFlow = getControlFlow(block.expressionwithoutblock.get());
    }
    
    return resultFlow;
}

ControlFlow ControlFlowAnalyzer::analyzeIfControlFlow(IfExpression& ifExpr) {
    auto ifFlow = getControlFlow(ifExpr.ifblockexpression.get());
    auto elseExpr = std::move(ifExpr.elseexpression);
    
    if (!elseExpr) {
        // 没有else分支，if表达式不影响控制流
        return ControlFlow::Continues;
    }
    
    auto elseFlow = getControlFlow(elseExpr.get());
    
    // 如果两个分支都发散，则整个if表达式发散
    if ((ifFlow == ControlFlow::Diverges || ifFlow == ControlFlow::Returns || 
         ifFlow == ControlFlow::Breaks || ifFlow == ControlFlow::ContinuesLoop) &&
        (elseFlow == ControlFlow::Diverges || elseFlow == ControlFlow::Returns ||
         elseFlow == ControlFlow::Breaks || elseFlow == ControlFlow::ContinuesLoop)) {
        return ControlFlow::Diverges;
    }
    
    return ControlFlow::Continues;
}

ControlFlow ControlFlowAnalyzer::analyzeLoopControlFlow(InfiniteLoopExpression& loop) {
    auto body = std::move(loop.blockexpression);
    if (!body) {
        return ControlFlow::Continues;
    }
    
    auto bodyFlow = getControlFlow(body.get());
    
    // 如果循环体没有break，则循环发散
    if (bodyFlow != ControlFlow::Breaks) {
        return ControlFlow::Diverges;
    }
    
    return ControlFlow::Continues;
}

// 类型推断方法
std::shared_ptr<SemanticType> ControlFlowAnalyzer::inferBlockType(BlockExpression& block) {
    std::shared_ptr<SemanticType> resultType = std::make_shared<SimpleType>("()");
    
    // 分析所有语句
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            // 如果语句发散，则块发散
            if (alwaysDivergesAt(stmt.get())) {
                return std::make_shared<NeverType>();
            }
        }
    }
    
    // 如果有尾表达式，块的类型由尾表达式决定
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        // 如果尾表达式发散，则块发散
        if (alwaysDivergesAt(block.expressionwithoutblock.get())) {
            return std::make_shared<NeverType>();
        }
        // 否则，块的类型是尾表达式的类型
        return getNodeType(block.expressionwithoutblock.get());
    }
    
    // 如果没有尾表达式，返回unit类型
    return resultType;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::inferIfType(IfExpression& ifExpr) {
    auto ifType = getNodeType(ifExpr.ifblockexpression.get());
    auto elseExpr = std::move(ifExpr.elseexpression);
    
    if (!elseExpr) {
        // 没有else分支，返回unit类型
        return std::make_shared<SimpleType>("()");
    }
    
    auto elseType = getNodeType(elseExpr.get());
    
    // 如果两个分支都发散，则类型为!
    if (ifType && ifType->tostring() == "!" && elseType && elseType->tostring() == "!") {
        return std::make_shared<NeverType>();
    }
    
    // 否则，需要统一两个分支的类型（简化处理，返回if分支类型）
    return ifType;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::inferLoopType(InfiniteLoopExpression& loop) {
    auto body = std::move(loop.blockexpression);
    if (!body) {
        return std::make_shared<SimpleType>("()");
    }
    
    // 如果循环发散（没有break），则类型为!
    if (getControlFlow(&loop) == ControlFlow::Diverges) {
        return std::make_shared<NeverType>();
    }
    
    // 否则，类型由break表达式决定（需要统一所有break的类型）
    return std::make_shared<SimpleType>("()");
}

// 工具方法
bool ControlFlowAnalyzer::isAlwaysDiverging(Expression& expr) {
    return alwaysDivergesAt(&expr);
}

void ControlFlowAnalyzer::checkBreakContinueValidity(ASTNode& node, Token tokenType) {
    if (!inLoop()) {
        std::string keyword = tokenType == Token::kbreak ? "break" : "continue";
        reportError(keyword + " expression outside of loop");
    }
}

void ControlFlowAnalyzer::reportError(const std::string& message) {
    std::cerr << "Control Flow Analysis Error: " << message << std::endl;
    hasErrors = true;
}

void ControlFlowAnalyzer::pushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void ControlFlowAnalyzer::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* ControlFlowAnalyzer::getCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void ControlFlowAnalyzer::enterLoop() {
    currentLoopDepth++;
    loopDepthStack.push(currentLoopDepth);
}

void ControlFlowAnalyzer::exitLoop() {
    if (!loopDepthStack.empty()) {
        loopDepthStack.pop();
    }
    currentLoopDepth = loopDepthStack.empty() ? 0 : loopDepthStack.top();
}

bool ControlFlowAnalyzer::inLoop() const {
    return currentLoopDepth > 0;
}

void ControlFlowAnalyzer::pushControlFlow(ControlFlow flow) {
    controlFlowStack.push(flow);
}

ControlFlow ControlFlowAnalyzer::popControlFlow() {
    if (controlFlowStack.empty()) {
        return ControlFlow::Continues;
    }
    ControlFlow flow = controlFlowStack.top();
    controlFlowStack.pop();
    return flow;
}

ControlFlow ControlFlowAnalyzer::getCurrentControlFlow() {
    return controlFlowStack.empty() ? ControlFlow::Continues : controlFlowStack.top();
}
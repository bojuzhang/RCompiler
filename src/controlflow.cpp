#include "controlflow.hpp"
#include "astnodes.hpp"
#include <iostream>
#include <utility>

ControlFlowAnalyzer::ControlFlowAnalyzer(std::shared_ptr<ScopeTree> scopeTree,
                                       std::shared_ptr<ConstantEvaluator> constantEvaluator)
    : scopeTree(scopeTree), constantEvaluator(constantEvaluator) {}

bool ControlFlowAnalyzer::AnalyzeControlFlow() {
    hasErrors = false;
    return !hasErrors;
}

bool ControlFlowAnalyzer::HasAnalysisErrors() const {
    return hasErrors;
}

ControlFlow ControlFlowAnalyzer::GetControlFlow(ASTNode* node) const {
    auto it = nodeControlFlow.find(node);
    return it != nodeControlFlow.end() ? it->second : ControlFlow::Continues;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::GetNodeType(ASTNode* node) const {
    auto it = nodeTypes.find(node);
    return it != nodeTypes.end() ? it->second : nullptr;
}

bool ControlFlowAnalyzer::AlwaysDivergesAt(ASTNode* node) const {
    auto it = alwaysDiverges.find(node);
    return it != alwaysDiverges.end() ? it->second : false;
}

void ControlFlowAnalyzer::visit(Crate& node) {
    PushNode(node);
    
    for (const auto& item : node.items) {
        if (item) {
            item->accept(*this);
        }
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(Item& node) {
    if (node.item) {
        node.item->accept(*this);
    }
}

void ControlFlowAnalyzer::visit(Statement& node) {
    if (node.astnode) {
        node.astnode->accept(*this);
    }
}

void ControlFlowAnalyzer::visit(Function& node) {
    PushNode(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
        // 检查函数返回类型
        if (node.functionreturntype) {
            auto returnType = node.functionreturntype->type;
            auto bodyType = GetNodeType(node.blockexpression.get());
            
            // 如果函数体发散，则不需要返回语句
            if (bodyType && bodyType->tostring() != "!" && GetControlFlow(node.blockexpression.get()) == ControlFlow::Diverges) {
                // 需要检查是否有return语句或发散表达式
            }
        }
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(ConstantItem& node) {
    PushNode(node);
    // 设置常量上下文
    bool previousConstContext = inConstContext;
    inConstContext = true;
    
    if (node.expression) {
        node.expression->accept(*this);
        // 检查是否可以在编译时求值
        if (!constantEvaluator->GetConstantValue(node.identifier)) {
            ReportError("Constant '" + node.identifier + "' cannot be evaluated at compile time");
        }
    }
    
    inConstContext = previousConstContext;
    PopNode();
}

void ControlFlowAnalyzer::visit(BlockExpression& node) {
    PushNode(node);
    // 进入块作用域
    scopeTree->EnterScope(Scope::ScopeType::Block, &node);
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
    ControlFlow flow = AnalyzeBlockControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断块的类型
    std::shared_ptr<SemanticType> type = InferBlockType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    // 退出块作用域
    scopeTree->ExitScope();
    PopNode();
}

void ControlFlowAnalyzer::visit(InfiniteLoopExpression& node) {
    PushNode(node);
    EnterLoop();
    
    // 分析循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    // 无限循环本身发散（除非有break）
    ControlFlow bodyFlow = GetControlFlow(node.blockexpression.get());
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
    
    ExitLoop();
    PopNode();
}

void ControlFlowAnalyzer::visit(PredicateLoopExpression& node) {
    PushNode(node);
    EnterLoop();
    
    // 分析条件
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    // 分析循环体
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    // 谓词循环：条件为false时退出
    // 控制流分析更复杂，这里简化处理
    nodeControlFlow[&node] = ControlFlow::Continues;
    nodeTypes[&node] = std::make_shared<SimpleType>("()");
    alwaysDiverges[&node] = false;
    
    ExitLoop();
    PopNode();
}

void ControlFlowAnalyzer::visit(IfExpression& node) {
    PushNode(node);
    
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    
    node.ifblockexpression->accept(*this);
    if (node.elseexpression) {
        node.elseexpression->accept(*this);
    }
    
    ControlFlow flow = AnalyzeIfControlFlow(node);
    nodeControlFlow[&node] = flow;
    
    // 推断类型
    std::shared_ptr<SemanticType> type = InferIfType(node);
    if (type) {
        nodeTypes[&node] = type;
        alwaysDiverges[&node] = (type->tostring() == "!");
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(BreakExpression& node) {
    PushNode(node);
    
    // 检查break是否在循环内
    CheckBreakContinueValidity(node, Token::kbreak);
    
    // break表达式发散
    nodeControlFlow[&node] = ControlFlow::Breaks;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(ContinueExpression& node) {
    PushNode(node);
    
    // 检查continue是否在循环内
    CheckBreakContinueValidity(node, Token::kcontinue);
    
    // continue表达式发散
    nodeControlFlow[&node] = ControlFlow::ContinuesLoop;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    PopNode();
}

void ControlFlowAnalyzer::visit(ReturnExpression& node) {
    PushNode(node);
    
    // return表达式发散
    nodeControlFlow[&node] = ControlFlow::Returns;
    nodeTypes[&node] = std::make_shared<NeverType>();
    alwaysDiverges[&node] = true;
    
    // 分析返回值
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(LetStatement& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    // let语句不影响控制流
    nodeControlFlow[&node] = ControlFlow::Continues;
    
    PopNode();
}

void ControlFlowAnalyzer::visit(ExpressionStatement& node) {
    PushNode(node);
    
    if (node.astnode) {
        node.astnode->accept(*this);
        // 表达式语句的控制流由表达式决定
        nodeControlFlow[&node] = GetControlFlow(node.astnode.get());
        nodeTypes[&node] = GetNodeType(node.astnode.get());
        alwaysDiverges[&node] = AlwaysDivergesAt(node.astnode.get());
    }
    
    PopNode();
}

void ControlFlowAnalyzer::visit(CallExpression& node) {
    PushNode(node);
    
    if (node.expression) {
        node.expression->accept(*this);
    }
    
    // 检查是否是 exit 函数调用
    if (auto pathExpr = dynamic_cast<PathExpression*>(node.expression.get())) {
        if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
            std::string functionName = pathExpr->simplepath->simplepathsegements[0]->identifier;
            if (functionName == "exit") {
                // exit 函数发散
                nodeControlFlow[&node] = ControlFlow::Diverges;
                nodeTypes[&node] = std::make_shared<NeverType>();
                alwaysDiverges[&node] = true;
                PopNode();
                return;
            }
        }
    }
    
    PopNode();
}

// 控制流分析方法
ControlFlow ControlFlowAnalyzer::AnalyzeBlockControlFlow(BlockExpression& block) {
    ControlFlow resultFlow = ControlFlow::Continues;
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            resultFlow = GetControlFlow(stmt.get());
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
        resultFlow = GetControlFlow(block.expressionwithoutblock.get());
    }
    
    return resultFlow;
}

ControlFlow ControlFlowAnalyzer::AnalyzeIfControlFlow(IfExpression& ifExpr) {
    auto ifFlow = GetControlFlow(ifExpr.ifblockexpression.get());
    if (!ifExpr.elseexpression) {
        // 没有else分支，if表达式不影响控制流
        return ControlFlow::Continues;
    }
    
    auto elseFlow = GetControlFlow(ifExpr.elseexpression.get());
    
    // 如果两个分支都发散，则整个if表达式发散
    if ((ifFlow == ControlFlow::Diverges || ifFlow == ControlFlow::Returns || 
         ifFlow == ControlFlow::Breaks || ifFlow == ControlFlow::ContinuesLoop) &&
        (elseFlow == ControlFlow::Diverges || elseFlow == ControlFlow::Returns ||
         elseFlow == ControlFlow::Breaks || elseFlow == ControlFlow::ContinuesLoop)) {
        return ControlFlow::Diverges;
    }
    
    return ControlFlow::Continues;
}

ControlFlow ControlFlowAnalyzer::AnalyzeLoopControlFlow(InfiniteLoopExpression& loop) {
    if (!loop.blockexpression) {
        return ControlFlow::Continues;
    }
    auto bodyFlow = GetControlFlow(loop.blockexpression.get());
    
    // 如果循环体没有break，则循环发散
    if (bodyFlow != ControlFlow::Breaks) {
        return ControlFlow::Diverges;
    }
    
    return ControlFlow::Continues;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::InferBlockType(BlockExpression& block) {
    std::shared_ptr<SemanticType> resultType = std::make_shared<SimpleType>("()");
    for (const auto& stmt : block.statements) {
        if (stmt) {
            stmt->accept(*this);
            // 如果语句发散，则块发散
            if (AlwaysDivergesAt(stmt.get())) {
                return std::make_shared<NeverType>();
            }
        }
    }
    
    // 如果有尾表达式，块的类型由尾表达式决定
    if (block.expressionwithoutblock) {
        block.expressionwithoutblock->accept(*this);
        // 如果尾表达式发散，则块发散
        if (AlwaysDivergesAt(block.expressionwithoutblock.get())) {
            return std::make_shared<NeverType>();
        }
        // 否则，块的类型是尾表达式的类型
        return GetNodeType(block.expressionwithoutblock.get());
    }
    
    // 如果没有尾表达式，返回unit类型
    return resultType;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::InferIfType(IfExpression& ifExpr) {
    auto ifType = GetNodeType(ifExpr.ifblockexpression.get());
    if (!ifExpr.elseexpression) {
        // 没有else分支，返回unit类型
        return std::make_shared<SimpleType>("()");
    }
    
    auto elseType = GetNodeType(ifExpr.elseexpression.get());
    // 如果两个分支都发散，则类型为!
    if (ifType && ifType->tostring() == "!" && elseType && elseType->tostring() == "!") {
        return std::make_shared<NeverType>();
    }
    // 如果一个分支发散（!类型），另一个分支不发散，则if表达式类型为非发散分支的类型
    if (ifType && ifType->tostring() == "!" && elseType && elseType->tostring() != "!") {
        return elseType;
    }
    if (elseType && elseType->tostring() == "!" && ifType && ifType->tostring() != "!") {
        return ifType;
    }
    
    // 如果两个分支都不发散，检查类型兼容性
    if (ifType && elseType && ifType->tostring() != "!" && elseType->tostring() != "!") {
        // 检查类型是否兼容
        if (ifType->tostring() == elseType->tostring()) {
            return ifType;
        } else {
            // 类型不兼容，这里应该报告错误，但为了保持控制流分析的连续性，
            // 我们返回一个错误类型或默认类型
            // 在实际实现中，应该通过错误处理机制报告类型不匹配
            return std::make_shared<SimpleType>("type_error");
        }
    }
    
    // 默认情况，返回if分支类型
    return ifType;
}

std::shared_ptr<SemanticType> ControlFlowAnalyzer::InferLoopType(InfiniteLoopExpression& loop) {
    if (!loop.blockexpression) {
        return std::make_shared<SimpleType>("()");
    }
    // 如果循环发散（没有break），则类型为!
    if (GetControlFlow(&loop) == ControlFlow::Diverges) {
        return std::make_shared<NeverType>();
    }
    
    // 否则，类型由break表达式决定（需要统一所有break的类型）
    return std::make_shared<SimpleType>("()");
}

// 工具方法
bool ControlFlowAnalyzer::IsAlwaysDiverging(Expression& expr) {
    return AlwaysDivergesAt(&expr);
}

void ControlFlowAnalyzer::CheckBreakContinueValidity(ASTNode& node, Token tokenType) {
    std::string keyword = tokenType == Token::kbreak ? "break" : "continue";
    if (!InLoop()) {
        ReportError(keyword + " expression outside of loop");
    }
}

void ControlFlowAnalyzer::ReportError(const std::string& message) {
    std::cerr << "Control Flow Analysis Error: " << message << std::endl;
    hasErrors = true;
}

void ControlFlowAnalyzer::PushNode(ASTNode& node) {
    nodeStack.push(&node);
}

void ControlFlowAnalyzer::PopNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop();
    }
}

ASTNode* ControlFlowAnalyzer::GetCurrentNode() {
    return nodeStack.empty() ? nullptr : nodeStack.top();
}

void ControlFlowAnalyzer::EnterLoop() {
    currentLoopDepth++;
    loopDepthStack.push(currentLoopDepth);
}

void ControlFlowAnalyzer::ExitLoop() {
    if (!loopDepthStack.empty()) {
        loopDepthStack.pop();
    }
    currentLoopDepth = loopDepthStack.empty() ? 0 : loopDepthStack.top();
}

bool ControlFlowAnalyzer::InLoop() const {
    return currentLoopDepth > 0;
}

void ControlFlowAnalyzer::PushControlFlow(ControlFlow flow) {
    controlFlowStack.push(flow);
}

ControlFlow ControlFlowAnalyzer::PopControlFlow() {
    if (controlFlowStack.empty()) {
        return ControlFlow::Continues;
    }
    ControlFlow flow = controlFlowStack.top();
    controlFlowStack.pop();
    return flow;
}

ControlFlow ControlFlowAnalyzer::GetCurrentControlFlow() {
    return controlFlowStack.empty() ? ControlFlow::Continues : controlFlowStack.top();
}
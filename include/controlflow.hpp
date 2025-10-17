// control_flow_analyzer.hpp
#pragma once

#include "visitor.hpp"
#include "scope.hpp"
#include "constevaluator.hpp"
#include <memory>
#include <stack>
#include <unordered_map>

// 控制流结果
enum class ControlFlow {
    Continues,    // 正常继续
    Breaks,       // 包含break
    ContinuesLoop,// 包含continue  
    Returns,      // 包含return
    Diverges      // 发散（never类型）
};

// 类型信息（扩展）
class NeverType : public SemanticType {
public:
    std::string tostring() const override { return "!"; }
};

// 控制流分析器
class ControlFlowAnalyzer : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ConstantEvaluator> constantEvaluator;
    std::stack<ASTNode*> nodeStack;
    std::stack<int> loopDepthStack;
    std::stack<ControlFlow> controlFlowStack;
    
    // 分析状态
    bool hasErrors = false;
    int currentLoopDepth = 0;
    bool inConstContext = false;
    
    // 结果存储
    std::unordered_map<ASTNode*, ControlFlow> nodeControlFlow;
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypes;
    std::unordered_map<ASTNode*, bool> alwaysDiverges;

public:
    ControlFlowAnalyzer(std::shared_ptr<ScopeTree> scopeTree, 
                       std::shared_ptr<ConstantEvaluator> constantEvaluator);
    
    bool analyzeControlFlow();
    bool hasAnalysisErrors() const;
    ControlFlow getControlFlow(ASTNode* node) const;
    std::shared_ptr<SemanticType> getNodeType(ASTNode* node) const;
    bool alwaysDivergesAt(ASTNode* node) const;
    
    // Visitor接口实现
    void visit(Crate& node) override;
    void visit(Item& node) override {}
    void visit(Function& node) override;
    void visit(ConstantItem& node) override;
    void visit(StructStruct& node) override {}
    void visit(Enumeration& node) override {}
    void visit(InherentImpl& node) override {}
    
    // 语句节点
    void visit(Statement& node) override {}
    void visit(LetStatement& node) override;
    void visit(ExpressionStatement& node) override;
    
    // 表达式节点
    void visit(Expression& node) override {}
    void visit(LiteralExpression& node) override {}
    void visit(PathExpression& node) override {}
    void visit(BinaryExpression& node) override {}
    void visit(UnaryExpression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override {}
    void visit(InfiniteLoopExpression& node) override;
    void visit(PredicateLoopExpression& node) override;
    void visit(IfExpression& node) override;
    void visit(BreakExpression& node) override;
    void visit(ContinueExpression& node) override;
    void visit(ReturnExpression& node) override;
    void visit(CallExpression& node) override {}
    void visit(ArrayExpression& node) override {}
    
    // 其他表达式节点（简化处理）
    void visit(GroupedExpression& node) override {}
    void visit(IndexExpression& node) override {}
    void visit(TupleExpression& node) override {}
    void visit(StructExpression& node) override {}
    void visit(MethodCallExpression& node) override {}
    void visit(FieldExpression& node) override {}
    void visit(UnderscoreExpression& node) override {}
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override {}
    void visit(CompoundAssignmentExpression& node) override {}
    
    // 模式、类型、路径节点
    void visit(Pattern& node) override {}
    void visit(LiteralPattern& node) override {}
    void visit(IdentifierPattern& node) override {}
    void visit(WildcardPattern& node) override {}
    void visit(PathPattern& node) override {}
    void visit(Type& node) override {}
    void visit(TypePath& node) override {}
    void visit(ArrayType& node) override {}
    void visit(ReferenceType& node) override {}
    void visit(SimplePath& node) override {}
    void visit(SimplePathSegment& node) override {}

    void visit(FunctionParameters& node) override {}
    void visit(FunctionParam& node) override {}
    void visit(FunctionReturnType& node) override {}

    void visit(StructFields& node) override {}
    void visit(StructField& node) override {}

    void visit(EnumVariants& node) override {}
    void visit(EnumVariant& node) override {}

    void visit(AssociatedItem& node) override {}
    void visit(PathInExpression& node) override {}

private:
    void pushNode(ASTNode& node);
    void popNode();
    ASTNode* getCurrentNode();
    
    void enterLoop();
    void exitLoop();
    bool inLoop() const;
    
    void pushControlFlow(ControlFlow flow);
    ControlFlow popControlFlow();
    ControlFlow getCurrentControlFlow();
    
    // 控制流分析方法
    ControlFlow analyzeExpressionControlFlow(Expression& expr);
    ControlFlow analyzeBlockControlFlow(BlockExpression& block);
    ControlFlow analyzeIfControlFlow(IfExpression& ifExpr);
    ControlFlow analyzeLoopControlFlow(InfiniteLoopExpression& loop);
    ControlFlow analyzePredicateLoopControlFlow(PredicateLoopExpression& loop);
    
    // 类型推断方法
    std::shared_ptr<SemanticType> inferExpressionType(Expression& expr);
    std::shared_ptr<SemanticType> inferBlockType(BlockExpression& block);
    std::shared_ptr<SemanticType> inferIfType(IfExpression& ifExpr);
    std::shared_ptr<SemanticType> inferLoopType(InfiniteLoopExpression& loop);
    
    // 工具方法
    bool isAlwaysDiverging(Expression& expr);
    void reportError(const std::string& message);
    void checkBreakContinueValidity(ASTNode& node, Token tokenType);
};
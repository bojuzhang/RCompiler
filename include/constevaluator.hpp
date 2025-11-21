#pragma once

#include "visitor.hpp"
#include "scope.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

// 常量值表示
class ConstantValue {
public:
    virtual ~ConstantValue() = default;
    virtual std::string toString() const = 0;
};

class IntConstant : public ConstantValue {
private:
    int64_t value;
public:
    IntConstant(int64_t value) : value(value) {}
    int64_t getValue() const { return value; }
    std::string toString() const override { return std::to_string(value); }
};

class BoolConstant : public ConstantValue {
private:
    bool value;
public:
    BoolConstant(bool value) : value(value) {}
    bool getValue() const { return value; }
    std::string toString() const override { return value ? "true" : "false"; }
};

class CharConstant : public ConstantValue {
private:
    char value;
public:
    CharConstant(char value) : value(value) {}
    char getValue() const { return value; }
    std::string toString() const override { return std::string("'") + value + "'"; }
};

class StringConstant : public ConstantValue {
private:
    std::string value;
public:
    StringConstant(const std::string& value) : value(value) {}
    const std::string& getValue() const { return value; }
    std::string toString() const override { return "\"" + value + "\""; }
};

class ArrayConstant : public ConstantValue {
private:
    std::vector<std::shared_ptr<ConstantValue>> elements;
public:
    ArrayConstant(std::vector<std::shared_ptr<ConstantValue>> elements) 
        : elements(std::move(elements)) {}
    
    const std::vector<std::shared_ptr<ConstantValue>>& getElements() const { return elements; }
    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += elements[i]->toString();
        }
        result += "]";
        return result;
    }
};

// 常量求值器
class ConstantEvaluator : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::unordered_map<std::string, std::shared_ptr<ConstantValue>> constantValues;
    std::stack<std::shared_ptr<ConstantValue>> evaluationStack;
    std::stack<ASTNode*> nodeStack;
    
    bool hasErrors = false;
    bool inConstContext = false;

public:
    ConstantEvaluator(std::shared_ptr<ScopeTree> scopeTree);
    
    bool EvaluateConstants();
    bool HasEvaluationErrors() const;
    std::shared_ptr<ConstantValue> GetConstantValue(const std::string& name);
    
    void visit(Crate& node) override;
    void visit(Item& node) override;
    void visit(ConstantItem& node) override;
    void visit(Function& node) override;
    void visit(StructStruct& node) override {}
    void visit(Enumeration& node) override {}
    void visit(InherentImpl& node) override {}
    
    void visit(Expression& node) override {}
    void visit(LiteralExpression& node) override {}
    void visit(PathExpression& node) override {}
    void visit(BinaryExpression& node) override {}
    void visit(UnaryExpression& node) override {}
    void visit(ArrayExpression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(IfExpression& node) override {}
    
    void visit(GroupedExpression& node) override;
    void visit(IndexExpression& node) override {}
    void visit(TupleExpression& node) override {}
    void visit(StructExpression& node) override {}
    void visit(CallExpression& node) override {}
    void visit(MethodCallExpression& node) override {}
    void visit(FieldExpression& node) override {}
    void visit(ContinueExpression& node) override {}
    void visit(BreakExpression& node) override {}
    void visit(ReturnExpression& node) override {}
    void visit(UnderscoreExpression& node) override {}
    void visit(ConstBlockExpression& node) override {
        PushNode(node);
        
        // 设置常量上下文
        bool previousConstContext = inConstContext;
        inConstContext = true;
        
        // 处理常量块表达式
        auto block = std::move(node.blockexpression);
        if (block) {
            block->accept(*this);
        }
        
        inConstContext = previousConstContext;
        PopNode();
    }
    void visit(InfiniteLoopExpression& node) override {}
    void visit(PredicateLoopExpression& node) override {}
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override {}
    void visit(CompoundAssignmentExpression& node) override {}
    void visit(BorrowExpression& node) override {}
    void visit(DereferenceExpression& node) override {}
    
    void visit(Pattern& node) override {}
    void visit(LiteralPattern& node) override {}
    void visit(IdentifierPattern& node) override {}
    void visit(WildcardPattern& node) override {}
    void visit(PathPattern& node) override {}
    void visit(Type& node) override {}
    void visit(TypePath& node) override {}
    void visit(ArrayType& node) override {}
    void visit(ReferenceType& node) override {}
    void visit(UnitType& node) override {}
    void visit(SimplePath& node) override {}
    void visit(SimplePathSegment& node) override {}
    
    void visit(Statement& node) override;
    void visit(LetStatement& node) override;
    void visit(ExpressionStatement& node) override;

    void visit(FunctionParameters& node) override {}
    void visit(FunctionParam& node) override {}
    void visit(FunctionReturnType& node) override {}

    void visit(StructFields& node) override {}
    void visit(StructField& node) override {}

    void visit(EnumVariants& node) override {}
    void visit(EnumVariant& node) override {}

    void visit(AssociatedItem& node) override {}

private:
    void PushNode(ASTNode& node);
    void PopNode();
    ASTNode* GetCurrentNode();
    
    std::shared_ptr<ConstantValue> EvaluateExpression(Expression& expr);
    std::shared_ptr<ConstantValue> EvaluateLiteral(LiteralExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateBinaryExpression(BinaryExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateUnaryExpression(UnaryExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateArrayExpression(ArrayExpression& expr);
    std::shared_ptr<ConstantValue> EvaluatePathExpression(PathExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateBlockExpression(BlockExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateIfExpression(IfExpression& expr);
    std::shared_ptr<ConstantValue> EvaluateGroupedExpression(GroupedExpression& expr);
    
    bool IsCompileTimeConstant(Expression& expr);
    void ReportError(const std::string& message);
};
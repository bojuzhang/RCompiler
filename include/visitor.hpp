#pragma once

#include "visitorforward.hpp"

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual void visit(Crate& node) = 0;
    virtual void visit(Item& node) = 0;
    
    virtual void visit(Function& node) = 0;
    virtual void visit(ConstantItem& node) = 0;
    virtual void visit(StructStruct& node) = 0;
    virtual void visit(Enumeration& node) = 0;
    virtual void visit(InherentImpl& node) = 0;
    
    virtual void visit(Statement& node) = 0;
    virtual void visit(LetStatement& node) = 0;
    virtual void visit(ExpressionStatement& node) = 0;
    
    virtual void visit(Expression& node) = 0;
    virtual void visit(LiteralExpression& node) = 0;
    virtual void visit(PathExpression& node) = 0;
    virtual void visit(GroupedExpression& node) = 0;
    virtual void visit(ArrayExpression& node) = 0;
    virtual void visit(IndexExpression& node) = 0;
    virtual void visit(TupleExpression& node) = 0;
    virtual void visit(StructExpression& node) = 0;
    virtual void visit(CallExpression& node) = 0;
    virtual void visit(MethodCallExpression& node) = 0;
    virtual void visit(FieldExpression& node) = 0;
    virtual void visit(ContinueExpression& node) = 0;
    virtual void visit(BreakExpression& node) = 0;
    virtual void visit(ReturnExpression& node) = 0;
    virtual void visit(UnderscoreExpression& node) = 0;
    virtual void visit(BlockExpression& node) = 0;
    virtual void visit(ConstBlockExpression& node) = 0;
    virtual void visit(InfiniteLoopExpression& node) = 0;
    virtual void visit(PredicateLoopExpression& node) = 0;
    virtual void visit(IfExpression& node) = 0;
    virtual void visit(MatchExpression& node) = 0;
    virtual void visit(TypeCastExpression& node) = 0;
    virtual void visit(AssignmentExpression& node) = 0;
    virtual void visit(CompoundAssignmentExpression& node) = 0;
    virtual void visit(UnaryExpression& node) = 0;
    virtual void visit(BinaryExpression& node) = 0;
    virtual void visit(BorrowExpression& node) = 0;
    virtual void visit(DereferenceExpression& node) = 0;
    
    virtual void visit(Pattern& node) = 0;
    virtual void visit(LiteralPattern& node) = 0;
    virtual void visit(IdentifierPattern& node) = 0;
    virtual void visit(WildcardPattern& node) = 0;
    virtual void visit(PathPattern& node) = 0;
    
    virtual void visit(Type& node) = 0;
    virtual void visit(TypePath& node) = 0;
    virtual void visit(ArrayType& node) = 0;
    virtual void visit(ReferenceType& node) = 0;
    virtual void visit(UnitType& node) = 0;
    
    virtual void visit(SimplePath& node) = 0;
    virtual void visit(SimplePathSegment& node) = 0;

    virtual void visit(FunctionParameters& node) = 0;
    virtual void visit(FunctionParam& node) = 0;
    virtual void visit(FunctionReturnType& node) = 0;

    virtual void visit(StructFields& node) = 0;
    virtual void visit(StructField& node) = 0;

    virtual void visit(EnumVariants& node) = 0;
    virtual void visit(EnumVariant& node) = 0;

    virtual void visit(AssociatedItem& node) = 0;
    virtual void visit(PathInExpression& node) = 0;
};

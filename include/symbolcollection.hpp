// first pass
#pragma once

#include "visitor.hpp"
#include "scope.hpp"
#include <memory>
#include <stack>

class SymbolCollector : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> root;
    std::stack<ASTNode*> nodeStack;
    
    bool inFunction = false;
    bool inLoop = false;
    bool inTrait = false;
    bool inImpl = false;
    std::string currentFunctionName;
    
public:
    SymbolCollector();
    
    void beginCollection();
    void endCollection();
    std::shared_ptr<Scope> getScope() const;
    
    void visit(Crate& node) override;
    void visit(Item& node) override;
    
    void visit(Function& node) override;
    void visit(ConstantItem& node) override;
    void visit(StructStruct& node) override;
    void visit(Enumeration& node) override;
    void visit(InherentImpl& node) override;
    
    void visit(Statement& node) override {}
    void visit(LetStatement& node) override {}
    void visit(ExpressionStatement& node) override {}
    
    void visit(Expression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override {}
    void visit(InfiniteLoopExpression& node) override;
    void visit(PredicateLoopExpression& node) override;
    void visit(IfExpression& node) override {}
    void visit(LiteralExpression& node) override {}
    void visit(PathExpression& node) override {}
    void visit(GroupedExpression& node) override {}
    void visit(ArrayExpression& node) override {}
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
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override {}
    void visit(CompoundAssignmentExpression& node) override {}
    void visit(UnaryExpression& node) override {}
    void visit(BinaryExpression& node) override {}
    
    void visit(Pattern& node) override {}
    void visit(LiteralPattern& node) override {}
    void visit(IdentifierPattern& node) override {}
    void visit(WildcardPattern& node) override {}
    void visit(PathPattern& node) override {}
    
    void visit(Type& node) override {}
    void visit(TypePath& node) override {}
    void visit(ArrayType& node) override {}
    void visit(SliceType& node) override {}
    void visit(InferredType& node) override {}
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

    std::shared_ptr<ScopeTree> getScopeTree() {return root;}
    
private:
    void pushNode(ASTNode& node);
    void popNode();
    ASTNode* getCurrentNode();
    
    void collectFunctionSymbol(Function& node);
    void collectConstantSymbol(ConstantItem& node);
    void collectStructSymbol(StructStruct& node);
    void collectEnumSymbol(Enumeration& node);
    void collectImplSymbol(InherentImpl& node);
    void collectParameterSymbols(Function& node);
    void collectFieldSymbols(StructStruct& node);
    void collectVariantSymbols(Enumeration& node);
    
    std::shared_ptr<SemanticType> resolveTypeFromNode(Type& node);
    std::shared_ptr<SemanticType> createSimpleType(const std::string& name);

    std::shared_ptr<SemanticType> getImplTargetType(InherentImpl& node);
    std::string getTraitNameFromImpl(InherentImpl& node);

    void collectAssociatedItem(AssociatedItem& item, std::shared_ptr<ImplSymbol> implSymbol);
    void collectAssociatedFunction(Function& function, std::shared_ptr<ImplSymbol> implSymbol);
    void collectAssociatedConstant(ConstantItem& constant, std::shared_ptr<ImplSymbol> implSymbol);
};
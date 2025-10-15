#pragma once

#include "symbol.hpp"
#include "visitor.hpp"
#include "scope.hpp"
#include <memory>
#include <stack>
#include <unordered_map>
#include <unordered_set>

class TypeChecker : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::stack<ASTNode*> nodeStack;
    std::stack<std::shared_ptr<SemanticType>> expectedTypeStack;
    
    bool hasErrors = false;
    std::string currentStruct;
    std::string currentTrait;
    std::string currentImpl;
    std::unordered_set<std::string> currentTraitRequirements;
    
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeCache;
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    
    std::unordered_map<std::string, std::unordered_set<std::string>> structMethods;
    std::unordered_map<std::string, std::unordered_set<std::string>> traitRequirements;
    std::unordered_map<std::string, std::unordered_set<std::string>> traitImplementations;
    std::unordered_map<std::string, std::string> implToTraitMap;

public:
    TypeChecker(std::shared_ptr<ScopeTree> scopeTree);
    
    bool checkTypes();
    bool hasTypeErrors() const;
    
    void visit(Crate& node) override;
    void visit(Item& node) override;
    
    void visit(Function& node) override;
    void visit(ConstantItem& node) override;
    void visit(StructStruct& node) override;
    void visit(Enumeration& node) override;
    void visit(InherentImpl& node) override;
    
    void visit(Statement& node) override;
    void visit(LetStatement& node) override;
    void visit(ExpressionStatement& node) override;
    
    void visit(Expression& node) override {}
    void visit(LiteralExpression& node) override {}
    void visit(PathExpression& node) override;
    void visit(GroupedExpression& node) override {}
    void visit(ArrayExpression& node) override {}
    void visit(IndexExpression& node) override;
    void visit(TupleExpression& node) override {}
    void visit(StructExpression& node) override {}
    void visit(CallExpression& node) override {}
    void visit(MethodCallExpression& node) override {}
    void visit(FieldExpression& node) override;
    void visit(ContinueExpression& node) override {}
    void visit(BreakExpression& node) override {}
    void visit(ReturnExpression& node) override {}
    void visit(UnderscoreExpression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override {}
    void visit(InfiniteLoopExpression& node) override {}
    void visit(PredicateLoopExpression& node) override {}
    void visit(IfExpression& node) override {}
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override;
    void visit(CompoundAssignmentExpression& node) override;
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

private:
    void pushNode(ASTNode& node);
    void popNode();
    ASTNode* getCurrentNode();
    void pushExpectedType(std::shared_ptr<SemanticType> type);
    void popExpectedType();
    std::shared_ptr<SemanticType> getExpectedType();
    
    std::shared_ptr<SemanticType> checkType(Type& typeNode);
    std::shared_ptr<SemanticType> checkType(TypePath& typePath);
    std::shared_ptr<SemanticType> checkType(ArrayType& arrayType);
    std::shared_ptr<SemanticType> checkType(SliceType& sliceType);
    std::shared_ptr<SemanticType> checkType(InferredType& inferredType);
    std::shared_ptr<SemanticType> resolveType(const std::string& typeName);
    bool typeExists(const std::string& typeName);
    bool isTypeVisible(const std::string& typeName);
    bool areTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual);
    
    void checkStructFields(StructStruct& node);
    void checkStructFieldType(StructField& field);
    
    void checkInherentImpl(InherentImpl& node);
    void checkTraitImpl(InherentImpl& node);
    std::shared_ptr<SemanticType> getImplTargetType(InherentImpl& node);
    std::string getTraitNameFromImpl(InherentImpl& node);
    void checkTraitImplementation(InherentImpl& node, const std::string& traitName);
    void collectTraitRequirements(const std::string& traitName);
    void checkTraitRequirementsSatisfied(const std::string& traitName, const std::string& implName);
    
    void checkAssociatedItem(AssociatedItem& item);
    void checkAssociatedFunction(Function& function);
    void checkAssociatedConstant(ConstantItem& constant);
    
    void checkFunctionSignature(Function& function);
    void checkFunctionParameters(FunctionParameters& params);
    void checkFunctionReturnType(FunctionReturnType& returnType);
    void checkFunctionBody(Function& function);
    
    std::shared_ptr<SemanticType> inferExpressionType(Expression& expr);
    std::shared_ptr<SemanticType> inferBinaryExpressionType(BinaryExpression& expr);
    std::shared_ptr<SemanticType> inferCallExpressionType(CallExpression& expr);
    std::shared_ptr<SemanticType> inferMethodCallExpressionType(MethodCallExpression& expr);
    
    void reportError(const std::string& message);
    void reportUndefinedType(const std::string& typeName, ASTNode* context);
    void reportMissingTraitImplementation(const std::string& traitName, const std::string& missingItem);
    
    std::shared_ptr<Symbol> findSymbol(const std::string& name);
    std::shared_ptr<FunctionSymbol> findFunction(const std::string& name);
    std::shared_ptr<StructSymbol> findStruct(const std::string& name);
    std::shared_ptr<TraitSymbol> findTrait(const std::string& name);
    
    void enterStructContext(const std::string& structName);
    void exitStructContext();
    void enterTraitContext(const std::string& traitName);
    void exitTraitContext();
    void enterImplContext(const std::string& implName);
    void exitImplContext();

    void checkPattern(Pattern& pattern, std::shared_ptr<SemanticType> expectedType);
    void checkPattern(IdentifierPattern& pattern, std::shared_ptr<SemanticType> expectedType);
    void checkPattern(ReferencePattern& pattern, std::shared_ptr<SemanticType> expectedType);
    
    // 可变性检查方法
    void checkAssignmentMutability(Expression& lhs);
    void checkVariableMutability(PathExpression& pathExpr);
    void checkFieldMutability(FieldExpression& fieldExpr);
    void checkIndexMutability(IndexExpression& indexExpr);
    void reportMutabilityError(const std::string& name, const std::string& errorType, ASTNode* context);
};
#pragma once

#include "astnodes.hpp"
#include "symbol.hpp"
#include "visitor.hpp"
#include "scope.hpp"
#include "typewrapper.hpp"
#include "constevaluator.hpp"
#include <memory>
#include <stack>
#include <unordered_map>
#include <unordered_set>

class TypeChecker : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ConstantEvaluator> constantEvaluator;
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
    TypeChecker(std::shared_ptr<ScopeTree> scopeTree, std::shared_ptr<ConstantEvaluator> constantEvaluator = nullptr);
    
    bool CheckTypes();
    bool HasTypeErrors() const;
    
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
    void visit(ReturnExpression& node) override;
    void visit(UnderscoreExpression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override {}
    void visit(InfiniteLoopExpression& node) override;
    void visit(PredicateLoopExpression& node) override;
    void visit(IfExpression& node) override;
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override;
    void visit(CompoundAssignmentExpression& node) override;
    void visit(UnaryExpression& node) override {}
    void visit(BinaryExpression& node) override;
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
    void PushNode(ASTNode& node);
    void PopNode();
    ASTNode* GetCurrentNode();
    void PushExpectedType(std::shared_ptr<SemanticType> type);
    void PopExpectedType();
    std::shared_ptr<SemanticType> GetExpectedType();
    
    std::shared_ptr<SemanticType> CheckType(Type& typeNode);
    std::shared_ptr<SemanticType> CheckType(TypePath& typePath);
    std::shared_ptr<SemanticType> CheckType(ArrayType& arrayType);
    std::shared_ptr<SemanticType> CheckType(ReferenceType& refType);
    std::shared_ptr<SemanticType> ResolveType(const std::string& typeName);
    bool TypeExists(const std::string& typeName);
    bool IsTypeVisible(const std::string& typeName);
    bool AreTypesCompatible(std::shared_ptr<SemanticType> expected, std::shared_ptr<SemanticType> actual);
    
    void CheckStructFields(StructStruct& node);
    void CheckStructFieldType(StructField& field);
    
    void CheckInherentImpl(InherentImpl& node);
    void CheckTraitImpl(InherentImpl& node);
    std::shared_ptr<SemanticType> GetImplTargetType(InherentImpl& node);
    std::string GetTraitNameFromImpl(InherentImpl& node);
    void CheckTraitImplementation(InherentImpl& node, const std::string& traitName);
    void CollectTraitRequirements(const std::string& traitName);
    void CheckTraitRequirementsSatisfied(const std::string& traitName, const std::string& implName);
    
    void CheckAssociatedItem(AssociatedItem& item);
    void CheckAssociatedFunction(Function& function);
    void CheckAssociatedConstant(ConstantItem& constant);
    
    void CheckFunctionSignature(Function& function);
    void CheckFunctionParameters(FunctionParameters& params);
    void CheckFunctionReturnType(FunctionReturnType& returnType);
    void CheckFunctionBody(Function& function);
    
    std::shared_ptr<SemanticType> InferExpressionType(Expression& expr);
    std::shared_ptr<SemanticType> InferBinaryExpressionType(BinaryExpression& expr);
    std::shared_ptr<SemanticType> InferCallExpressionType(CallExpression& expr);
    std::shared_ptr<SemanticType> InferMethodCallExpressionType(MethodCallExpression& expr);
    std::shared_ptr<SemanticType> InferIndexExpressionType(IndexExpression& expr);
    std::shared_ptr<SemanticType> InferArrayExpressionType(ArrayExpression& expr);
    std::shared_ptr<SemanticType> InferConstantExpressionType(Expression& expr, std::shared_ptr<SemanticType> expectedType);
    std::shared_ptr<SemanticType> InferArrayExpressionTypeWithExpected(ArrayExpression& expr, std::shared_ptr<SemanticType> expectedType);
    std::shared_ptr<SemanticType> InferLiteralExpressionType(LiteralExpression& expr);
    
    void ReportError(const std::string& message);
    void ReportUndefinedType(const std::string& typeName, ASTNode* context);
    void ReportMissingTraitImplementation(const std::string& traitName, const std::string& missingItem);
    
    std::shared_ptr<Symbol> FindSymbol(const std::string& name);
    std::shared_ptr<FunctionSymbol> FindFunction(const std::string& name);
    std::shared_ptr<StructSymbol> FindStruct(const std::string& name);
    std::shared_ptr<TraitSymbol> FindTrait(const std::string& name);
    
    void EnterStructContext(const std::string& structName);
    void ExitStructContext();
    void EnterTraitContext(const std::string& traitName);
    void ExitTraitContext();
    void EnterImplContext(const std::string& implName);
    void ExitImplContext();

    void CheckPattern(Pattern& pattern, std::shared_ptr<SemanticType> expectedType);
    void CheckPattern(IdentifierPattern& pattern, std::shared_ptr<SemanticType> expectedType);
    void CheckPattern(ReferencePattern& pattern, std::shared_ptr<SemanticType> expectedType);
    
    // 可变性检查方法
    void CheckAssignmentMutability(Expression& lhs);
    void CheckVariableMutability(PathExpression& pathExpr);
    void CheckFieldMutability(FieldExpression& fieldExpr);
    void CheckIndexMutability(IndexExpression& indexExpr);
    void ReportMutabilityError(const std::string& name, const std::string& errorType, ASTNode* context);
    
    void CheckArraySizeMatch(ArrayTypeWrapper& declaredType, ArrayExpression& arrayExpr);
    void CheckArraySizeMatchForAnyExpression(ArrayTypeWrapper& declaredType, Expression& initExpr);
    int64_t EvaluateArraySize(Expression& sizeExpr);
    
    // 二元运算符类型检查辅助函数
    bool CanPerformBinaryOperation(std::shared_ptr<SemanticType> leftType, std::shared_ptr<SemanticType> rightType, Token op);
    std::shared_ptr<SemanticType> GetBinaryOperationResultType(std::shared_ptr<SemanticType> leftType, std::shared_ptr<SemanticType> rightType, Token op);
};
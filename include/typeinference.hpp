#pragma once

#include "visitor.hpp"
#include "scope.hpp"
#include "controlflow.hpp"
#include "constevaluator.hpp"
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>
#include <string>

class SimpleSemanticType : public SemanticType {
private:
    std::string typeName;
public:
    SimpleSemanticType(const std::string& name) : typeName(name) {}
    std::string tostring() const override { return typeName; }
};

class NeverSemanticType : public SemanticType {
public:
    std::string tostring() const override { return "!"; }
};

class TypeEnvironment {
private:
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeVariables;
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeSubstitutions;
    int nextTypeVarId = 0;

public:
    TypeEnvironment();
    
    // 类型变量管理
    std::shared_ptr<SemanticType> FreshTypeVariable();
    std::shared_ptr<SemanticType> GetTypeVariable(const std::string& name);
    void SetTypeVariable(const std::string& name, std::shared_ptr<SemanticType> type);
    
    // 类型替换
    void AddSubstitution(const std::string& typeVar, std::shared_ptr<SemanticType> type);
    std::shared_ptr<SemanticType> ApplySubstitutions(std::shared_ptr<SemanticType> type);
    void unify(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2);
    
    // 环境管理
    void EnterScope();
    void ExitScope();
    
private:
    bool occursCheck(const std::string& typeVar, std::shared_ptr<SemanticType> type);
};

class TypeInferenceChecker : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer;
    std::shared_ptr<ConstantEvaluator> constantEvaluator;
    std::shared_ptr<TypeEnvironment> typeEnv;
    
    std::stack<ASTNode*> nodeStack;
    std::stack<std::shared_ptr<SemanticType>> expectedTypeStack;
    std::stack<std::unordered_map<std::string, std::shared_ptr<SemanticType>>> localTypeVars;
    
    // 推断状态
    bool hasErrors = false;
    std::string currentFunctionReturnType;
    std::shared_ptr<SemanticType> currentSelfType;
    bool inAssignmentContext = false;
    
    // 结果存储
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> inferredTypes;
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> variableTypes;
    std::unordered_map<ASTNode*, std::vector<std::shared_ptr<SemanticType>>> typeConstraints;

public:
    TypeInferenceChecker(std::shared_ptr<ScopeTree> scopeTree,
                        std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer,
                        std::shared_ptr<ConstantEvaluator> constantEvaluator);
    
    bool InferTypes();
    bool HasInferenceErrors() const;
    std::shared_ptr<SemanticType> GetInferredType(ASTNode* node) const;
    
    void visit(Crate& node) override;
    void visit(Item& node) override {}
    void visit(Function& node) override;
    void visit(ConstantItem& node) override {}
    void visit(StructStruct& node) override {}
    void visit(Enumeration& node) override {}
    void visit(InherentImpl& node) override {}
    
    void visit(Statement& node) override {}
    void visit(LetStatement& node) override;
    void visit(ExpressionStatement& node) override;
    
    void visit(Expression& node) override {}
    void visit(LiteralExpression& node) override;
    void visit(PathExpression& node) override;
    void visit(GroupedExpression& node) override {}
    void visit(ArrayExpression& node) override {}
    void visit(IndexExpression& node) override {}
    void visit(TupleExpression& node) override {}
    void visit(StructExpression& node) override {}
    void visit(CallExpression& node) override;
    void visit(MethodCallExpression& node) override {}
    void visit(FieldExpression& node) override;
    void visit(ContinueExpression& node) override {}
    void visit(BreakExpression& node) override {}
    void visit(ReturnExpression& node) override;
    void visit(UnderscoreExpression& node) override {}
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override {}
    void visit(InfiniteLoopExpression& node) override {}
    void visit(PredicateLoopExpression& node) override {}
    void visit(IfExpression& node) override;
    void visit(MatchExpression& node) override {}
    void visit(TypeCastExpression& node) override {}
    void visit(AssignmentExpression& node) override;
    void visit(CompoundAssignmentExpression& node) override {}
    void visit(UnaryExpression& node) override {}
    void visit(BinaryExpression& node) override;
    void visit(BorrowExpression& node) override {}
    void visit(DereferenceExpression& node) override {}
    
    void visit(Pattern& node) override {}
    void visit(LiteralPattern& node) override {}
    void visit(IdentifierPattern& node) override;
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
    
    std::shared_ptr<SemanticType> InferExpressionType(Expression& expr);
    void CheckExpressionType(Expression& expr, std::shared_ptr<SemanticType> expectedType);
    
    std::shared_ptr<SemanticType> InferLiteralType(LiteralExpression& expr);
    std::shared_ptr<SemanticType> InferPathType(PathExpression& expr);
    std::shared_ptr<SemanticType> InferCallType(CallExpression& expr);
    std::shared_ptr<SemanticType> InferMethodCallType(MethodCallExpression& expr);
    std::shared_ptr<SemanticType> InferFieldAccessType(FieldExpression& expr);
    std::shared_ptr<SemanticType> InferBinaryExpressionType(BinaryExpression& expr);
    std::shared_ptr<SemanticType> InferUnaryExpressionType(UnaryExpression& expr);
    std::shared_ptr<SemanticType> InferIfExpressionType(IfExpression& expr);
    std::shared_ptr<SemanticType> InferBlockExpressionType(BlockExpression& expr);
    std::shared_ptr<SemanticType> InferArrayExpressionType(ArrayExpression& expr);
    std::shared_ptr<SemanticType> InferTupleExpressionType(TupleExpression& expr);
    std::shared_ptr<SemanticType> InferStructExpressionType(StructExpression& expr);
    std::shared_ptr<SemanticType> InferAssignmentType(AssignmentExpression& expr);
    
    std::shared_ptr<SemanticType> ResolveFunctionType(const std::string& functionName,
                                             const std::vector<std::shared_ptr<SemanticType>>& argTypes);
    std::shared_ptr<SemanticType> ResolveMethodType(std::shared_ptr<SemanticType> receiverType,
                                           const std::string& methodName,
                                           const std::vector<std::shared_ptr<SemanticType>>& argTypes);
    std::vector<std::shared_ptr<SemanticType>> InferArgumentTypes(CallParams& params);
    
    std::shared_ptr<SemanticType> ResolvePathType(SimplePath& path);
    std::shared_ptr<SemanticType> ResolveAssociatedType(std::shared_ptr<SemanticType> baseType,
                                               const std::string& associatedItem);
    
    void AddTypeConstraint(std::shared_ptr<SemanticType> actual, std::shared_ptr<SemanticType> expected);
    void SolveTypeConstraints();
    bool AreTypesCompatible(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2);
    bool IsSubtype(std::shared_ptr<SemanticType> subType, std::shared_ptr<SemanticType> superType);
    
    std::shared_ptr<SemanticType> GetVariableType(const std::string& varName);
    void SetVariableType(const std::string& varName, std::shared_ptr<SemanticType> type);
    std::shared_ptr<Symbol> ResolveSymbol(const std::string& name);
    std::shared_ptr<SemanticType> ResolveTypeFromSymbol(std::shared_ptr<Symbol> symbol);
    
    std::shared_ptr<SemanticType> GetStructFieldType(const std::string& structName,
                                             const std::string& fieldName);
    std::shared_ptr<SemanticType> GetEnumVariantType(const std::string& enumName,
                                             const std::string& variantName);
    
    void CheckMutability(const std::string& varName, ASTNode* usageContext);
    void CheckAssignmentMutability(Expression& lhs);
    
    void ReportError(const std::string& message);
    void ReportTypeError(const std::string& expected, const std::string& actual, ASTNode* context);
    void ReportUndefinedError(const std::string& name, const std::string& kind, ASTNode* context);
    
    void EnterFunctionContext(const std::string& returnType);
    void ExitFunctionContext();
    void EnterImplContext(std::shared_ptr<SemanticType> selfType);
    void ExitImplContext();
    
    std::shared_ptr<SemanticType> ConvertASTTypeToSemanticType(Type& astType);
};
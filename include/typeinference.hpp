// type_inference.hpp
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

// 类型环境（用于类型推断）
class TypeEnvironment {
private:
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeVariables;
    std::unordered_map<std::string, std::shared_ptr<SemanticType>> typeSubstitutions;
    int nextTypeVarId = 0;

public:
    TypeEnvironment();
    
    // 类型变量管理
    std::shared_ptr<SemanticType> freshTypeVariable();
    std::shared_ptr<SemanticType> getTypeVariable(const std::string& name);
    void setTypeVariable(const std::string& name, std::shared_ptr<SemanticType> type);
    
    // 类型替换
    void addSubstitution(const std::string& typeVar, std::shared_ptr<SemanticType> type);
    std::shared_ptr<SemanticType> applySubstitutions(std::shared_ptr<SemanticType> type);
    void unify(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2);
    
    // 环境管理
    void enterScope();
    void exitScope();
    
private:
    bool occursCheck(const std::string& typeVar, std::shared_ptr<SemanticType> type);
};

// 类型推断检查器
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
    
    bool inferTypes();
    bool hasInferenceErrors() const;
    std::shared_ptr<SemanticType> getInferredType(ASTNode* node) const;
    
    // Visitor接口实现
    void visit(Crate& node) override;
    void visit(Item& node) override {}
    void visit(Function& node) override;
    void visit(ConstantItem& node) override {}
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
    
    // 模式节点
    void visit(Pattern& node) override {}
    void visit(LiteralPattern& node) override {}
    void visit(IdentifierPattern& node) override;
    void visit(WildcardPattern& node) override {}
    void visit(PathPattern& node) override {}
    
    // 类型节点（AST中的Type，不是SemanticType）
    void visit(Type& node) override {}
    void visit(TypePath& node) override {}
    void visit(ArrayType& node) override {}
    void visit(ReferenceType& node) override {}
    void visit(UnitType& node) override {}
    
    // 路径节点
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
    // 辅助方法
    void pushNode(ASTNode& node);
    void popNode();
    ASTNode* getCurrentNode();
    void pushExpectedType(std::shared_ptr<SemanticType> type);
    void popExpectedType();
    std::shared_ptr<SemanticType> getExpectedType();
    
    // 类型推断核心方法
    std::shared_ptr<SemanticType> inferExpressionType(Expression& expr);
    void checkExpressionType(Expression& expr, std::shared_ptr<SemanticType> expectedType);
    
    // 具体表达式类型推断
    std::shared_ptr<SemanticType> inferLiteralType(LiteralExpression& expr);
    std::shared_ptr<SemanticType> inferPathType(PathExpression& expr);
    std::shared_ptr<SemanticType> inferCallType(CallExpression& expr);
    std::shared_ptr<SemanticType> inferMethodCallType(MethodCallExpression& expr);
    std::shared_ptr<SemanticType> inferFieldAccessType(FieldExpression& expr);
    std::shared_ptr<SemanticType> inferBinaryExpressionType(BinaryExpression& expr);
    std::shared_ptr<SemanticType> inferUnaryExpressionType(UnaryExpression& expr);
    std::shared_ptr<SemanticType> inferIfExpressionType(IfExpression& expr);
    std::shared_ptr<SemanticType> inferBlockExpressionType(BlockExpression& expr);
    std::shared_ptr<SemanticType> inferArrayExpressionType(ArrayExpression& expr);
    std::shared_ptr<SemanticType> inferTupleExpressionType(TupleExpression& expr);
    std::shared_ptr<SemanticType> inferStructExpressionType(StructExpression& expr);
    std::shared_ptr<SemanticType> inferAssignmentType(AssignmentExpression& expr);
    
    // 函数和方法处理
    std::shared_ptr<SemanticType> resolveFunctionType(const std::string& functionName, 
                                             const std::vector<std::shared_ptr<SemanticType>>& argTypes);
    std::shared_ptr<SemanticType> resolveMethodType(std::shared_ptr<SemanticType> receiverType,
                                           const std::string& methodName,
                                           const std::vector<std::shared_ptr<SemanticType>>& argTypes);
    std::vector<std::shared_ptr<SemanticType>> inferArgumentTypes(CallParams& params);
    
    // 路径解析
    std::shared_ptr<SemanticType> resolvePathType(SimplePath& path);
    std::shared_ptr<SemanticType> resolveAssociatedType(std::shared_ptr<SemanticType> baseType, 
                                               const std::string& associatedItem);
    
    // 类型检查和约束
    void addTypeConstraint(std::shared_ptr<SemanticType> actual, std::shared_ptr<SemanticType> expected);
    void solveTypeConstraints();
    bool areTypesCompatible(std::shared_ptr<SemanticType> type1, std::shared_ptr<SemanticType> type2);
    bool isSubtype(std::shared_ptr<SemanticType> subType, std::shared_ptr<SemanticType> superType);
    
    // 变量和符号处理
    std::shared_ptr<SemanticType> getVariableType(const std::string& varName);
    void setVariableType(const std::string& varName, std::shared_ptr<SemanticType> type);
    std::shared_ptr<Symbol> resolveSymbol(const std::string& name);
    std::shared_ptr<SemanticType> resolveTypeFromSymbol(std::shared_ptr<Symbol> symbol);
    
    // 结构体和枚举处理
    std::shared_ptr<SemanticType> getStructFieldType(const std::string& structName, 
                                            const std::string& fieldName);
    std::shared_ptr<SemanticType> getEnumVariantType(const std::string& enumName,
                                            const std::string& variantName);
    
    // 可变性检查
    void checkMutability(const std::string& varName, ASTNode* usageContext);
    void checkAssignmentMutability(Expression& lhs);
    
    // 错误处理
    void reportError(const std::string& message);
    void reportTypeError(const std::string& expected, const std::string& actual, ASTNode* context);
    void reportUndefinedError(const std::string& name, const std::string& kind, ASTNode* context);
    
    // 上下文管理
    void enterFunctionContext(const std::string& returnType);
    void exitFunctionContext();
    void enterImplContext(std::shared_ptr<SemanticType> selfType);
    void exitImplContext();
    
    // AST Type到SemanticType的转换
    std::shared_ptr<SemanticType> convertASTTypeToSemanticType(Type& astType);
};
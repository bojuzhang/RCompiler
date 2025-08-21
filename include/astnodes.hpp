#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "lexser.hpp"

class ASTNode;

class Crate;
class Item;
class Function;
class ConstantItem;
class Module;
class StructStruct;
class TupleStruct;
class Enumeration;
class InherentImpl;
// class FunctionQualifiers;
class FunctionParameters;
class FunctionParam;
class FunctionReturnType;
class StructFields;
class StructField;
class TupleFields;
class TupleField;
class EnumVariants;
class EnumVariant;
class EnumVariantTuple;
class EnumVariantStruct;
class EnumVariantDiscriminant;
class AssociatedItem;

// STATEMENT Syntax
class Statement;
class LetStatement;
class ExpressionStatement;

// EXPRESSION Syntax
class Expression;
// class ExpressionWithoutBlock;
// class ExpressionWithBlock;
class LiteralExpression;
class PathExpression;
class GroupedExpression;
class ArrayExpression;
class IndexExpression;
class TupleExpression;
class TupleIndexingExpression;
class StructExpression;
class CallExpression;
class MethodCallExpression;
class FieldExpression;
class ContinueExpression;
class BreakExpression;
class ReturnExpression;
class UnderscoreExpression;
class BlockExpression;
class ConstBlockExpression;
class InfiniteLoopExpression;
class PredicateLoopExpression;
class IfExpression;
class MatchExpression;
// class NegationExpression;
// class ArithmeticOrLogicalExpression;
// class ComparisonExpression;
// class LazyBooleanExpression;
class TypeCastExpression;
class AssignmentExpression;
class CompoundAssignmentExpression;
class ArrayElements;
class TupleElements;
class StructExprFields;
class StructExprField;
class StructBase;
class CallParams;
class Conditions;
class MatchArms;
class MatchArm;
class MatchArmGuard;
class UnaryExpression;
class BinaryExpression;

// PATTERN Syntax
class Pattern;
class PatternWithoutRange;
class PatternNoTopAlt;
class LiteralPattern;
class IdentifierPattern;
class WildcardPattern;
class RestPattern;
class StructPattern;
class TupleStructPattern;
class TuplePattern;
class GroupedPattern;
class SlicePattern;
class PathPattern;
class StructPatternElements;
class StructPatternFields;
class StructPatternField;
class TupleStructItems;
class TuplePatternItems;
class SlicePatternItems;

// TYPE Syntax
class Type;
// class TypeNoBounds;
// class ParenthesizedType;
class TypePath;
// class TupleType;
// class NeverType;
class ArrayType;
class SliceType;
class InferredType;
class ReferenceType;

// PATH Syntax
class SimplePath;
class SimplePathSegment;
class PathInExpression;


class ASTNode {
public:
    ASTNode();
    virtual ~ASTNode();
};

// ITEM Syntax
class Crate : public ASTNode {
private:
    std::vector<std::unique_ptr<Item>> items;
public:
    Crate(std::vector<std::unique_ptr<Item>>&& items);
};
class Item : public ASTNode {
private:
    std::unique_ptr<ASTNode> item;
public:
    Item(std::unique_ptr<ASTNode> item);
};
class Function : public ASTNode {
private:
    // std::unique_ptr<FunctionQualifiers> functionqualifiers;
    bool isconst;
    std::string identifier_name;
    std::unique_ptr<FunctionParameters> functionparameters;
    std::unique_ptr<FunctionReturnType> functionreturntype;
    std::unique_ptr<BlockExpression> blockexpression;
public:
    Function(bool isconst,
             std::string identifier_name,
             std::unique_ptr<FunctionParameters> functionparameters,
             std::unique_ptr<FunctionReturnType> functionreturntype,
             std::unique_ptr<BlockExpression> blockexpression);
};
class ConstantItem : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
public:
    ConstantItem(std::string identifier,
                 std::unique_ptr<Type> type,
                 std::unique_ptr<Expression> expression);
};
class Module : public ASTNode {
private:
    std::string identifier;
    std::vector<std::unique_ptr<Item>> items;
public:
    Module(std::string identifier,
           std::vector<std::unique_ptr<Item>>&& items);
};
class StructStruct : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<StructFields> structfileds;
    bool issemi;
public:
    StructStruct(std::string identifier,
                 std::unique_ptr<StructFields> structfileds,
                 bool issemi);
};
class TupleStruct : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<TupleFields> tuplefields;
public:
    TupleStruct(std::string identifier,
                std::unique_ptr<TupleFields> tuplefields);
};
class Enumeration : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<EnumVariants> enumvariants;
public:
    Enumeration(std::string identifier,
                std::unique_ptr<EnumVariants> enumvariants);
};
class InherentImpl : public ASTNode {
private:
    std::unique_ptr<Type> type;
    std::vector<std::unique_ptr<AssociatedItem>> associateditems;
public:
    InherentImpl(std::unique_ptr<Type> type,
                 std::vector<std::unique_ptr<AssociatedItem>>&& associateditems);
};
// class FunctionQualifiers : public ASTNode {
// private:
//     bool isconst;
// public:
//     explicit FunctionQualifiers(bool isconst);
// };
class FunctionParameters : public ASTNode {
private:
    std::vector<std::unique_ptr<FunctionParam>> functionparams;
public:
    explicit FunctionParameters(std::vector<std::unique_ptr<FunctionParam>>&& functionparams);
};
class FunctionParam : public ASTNode {
private:
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
    std::unique_ptr<Type> type;
public:
    FunctionParam(std::unique_ptr<PatternNoTopAlt> patternnotopalt,
                  std::unique_ptr<Type> type);
};
class FunctionReturnType : public ASTNode {
private:
    std::unique_ptr<Type> type;
public:
    explicit FunctionReturnType(std::unique_ptr<Type> type);
};
class StructFields : public ASTNode {
private:
    std::vector<std::unique_ptr<StructField>> structfields; 
public:
    explicit StructFields(std::vector<std::unique_ptr<StructField>>&& structfields);
};
class StructField : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<Type> type;
public:
    StructField(std::string identifier,
                std::unique_ptr<Type> type);
};
class TupleFields : public ASTNode {
private:
    std::vector<std::unique_ptr<TupleField>> tuplefields; 
public:
    explicit TupleFields(std::vector<std::unique_ptr<TupleField>>&& tuplefields);
};
class TupleField : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<Type> type;
public:
    TupleField(std::string identifier,
               std::unique_ptr<Type> type);
};
class EnumVariants : public ASTNode {
private:
    std::vector<std::unique_ptr<EnumVariant>> enumvariants; 
public:
    explicit EnumVariants(std::vector<std::unique_ptr<EnumVariant>>&& enumvariants);
};
class EnumVariant : public ASTNode {
private:
    std::string identifier;
public:
    EnumVariant(std::string identifier);
};
class EnumVariantTuple : public ASTNode {
private:
    std::unique_ptr<TupleFields> tuplefields;
public:
    explicit EnumVariantTuple(std::unique_ptr<TupleFields> tuplefields);
};
class EnumVariantStruct : public ASTNode {
private:
    std::unique_ptr<StructFields> structfields;
public:
    explicit EnumVariantStruct(std::unique_ptr<StructFields> structfields);
};
class EnumVariantDiscriminant : public ASTNode {
private:
    std::unique_ptr<Expression> expression;
public:
    explicit EnumVariantDiscriminant(std::unique_ptr<Expression> expression);
};
class AssociatedItem : public ASTNode {
private:
    std::unique_ptr<ASTNode> consttantitem_or_function;
public:
    explicit AssociatedItem(std::unique_ptr<ASTNode> consttantitem_or_function);
};

// STATEMENT Syntax
class Statement : public ASTNode{
private:
    std::unique_ptr<ASTNode> astnode; // item or letstatement or expressionstatement
public:
    explicit Statement(std::unique_ptr<ASTNode> astnode);
};
class LetStatement : public ASTNode {
private:
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
public:
    LetStatement(std::unique_ptr<PatternNoTopAlt> patternnotopalt,
                 std::unique_ptr<Type> type,
                 std::unique_ptr<Expression> expression);
};
class ExpressionStatement : public ASTNode {
private:
    std::unique_ptr<Expression> astnode;
    bool hassemi;
public:
    explicit ExpressionStatement(std::unique_ptr<Expression> astnode, bool hassemi);
};

// EXPRESSION Syntax
class Expression : public ASTNode {
public:
    Expression();
    virtual ~Expression();
};

class LiteralExpression : public Expression {
private:
    std::string literal;
    Token tokentype;
public:
    LiteralExpression(std::string literal, Token tokentype);
};
class PathExpression : public Expression {
private:
    std::unique_ptr<SimplePath> simplepath;
public:
    explicit PathExpression(std::unique_ptr<SimplePath> simplepath);
};
class GroupedExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
public:
    explicit GroupedExpression(std::unique_ptr<Expression> expression);
};
class ArrayExpression : public Expression {
private:
    std::unique_ptr<ArrayElements> arrayelements;
public:
    explicit ArrayExpression(std::unique_ptr<ArrayElements> arrayelements);
};
class IndexExpression : public Expression {
private:
    std::unique_ptr<Expression> expressionout;
    std::unique_ptr<Expression> expressionin;
public:
    IndexExpression(std::unique_ptr<Expression> expressionout,
                    std::unique_ptr<Expression> expressionin);
};
class TupleExpression : public Expression {
private:
    std::unique_ptr<TupleElements> tupleelements;
public:
    explicit TupleExpression(std::unique_ptr<TupleElements> tupleelements);
};
class TupleIndexingExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    int tupleindex;
public:
    TupleIndexingExpression(std::unique_ptr<Expression> expression, int tupleindex);
};
class StructExpression : public Expression {
private:
    std::unique_ptr<PathInExpression> pathinexpression;
    std::unique_ptr<ASTNode> structinfo; // exprfields or base
public:
    StructExpression(std::unique_ptr<PathInExpression> pathinexpression,
                     std::unique_ptr<ASTNode> structinfo);
};
class CallExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    std::unique_ptr<CallParams> callparams;
public:
    CallExpression(std::unique_ptr<Expression> expression,
                   std::unique_ptr<CallParams> callparams);
};
class MethodCallExpression : public Expression {
private:
    // is it deleted?
    std::unique_ptr<Expression> expression;
    // std::unique_ptr<Path>
    std::unique_ptr<CallParams> callparams;
};
class FieldExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    std::string identifier;
public:
    FieldExpression(std::unique_ptr<Expression> expression, std::string identifier);
};
class ContinueExpression : public Expression {
private:
    // nothing but continue
public:
    ContinueExpression();
};
class BreakExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
public:
    explicit BreakExpression(std::unique_ptr<Expression> expression);
};
class ReturnExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
public:
    explicit ReturnExpression(std::unique_ptr<Expression> expression);
};
class UnderscoreExpression : public Expression {
private:
    // nothing but _
public:
    UnderscoreExpression();
};
class BlockExpression : public Expression {
private:
    std::unique_ptr<Statement> statement;
public:
    explicit BlockExpression(std::unique_ptr<Statement> statement);
};
class ConstBlockExpression : public Expression {
private:
    std::unique_ptr<BlockExpression> blockexpression;
public:
    explicit ConstBlockExpression(std::unique_ptr<BlockExpression> blockexpression);
};
class InfiniteLoopExpression : public Expression {
private:
    std::unique_ptr<BlockExpression> blockexpression;
public:
    explicit InfiniteLoopExpression(std::unique_ptr<BlockExpression> blockexpression);
};
class PredicateLoopExpression : public Expression {
private:
    std::unique_ptr<Conditions> conditions;
    std::unique_ptr<BlockExpression> blockexpression;
public:
    PredicateLoopExpression(std::unique_ptr<Conditions> conditions,
                            std::unique_ptr<BlockExpression> blockexpression);
};
class IfExpression : public Expression {
private:
    std::unique_ptr<Conditions> conditions;
    std::unique_ptr<BlockExpression> ifblockexpression;
    std::unique_ptr<Expression> elseexpression; // block or if
public:
    IfExpression(std::unique_ptr<Conditions> conditions,
                 std::unique_ptr<BlockExpression> ifblockexpression,
                 std::unique_ptr<Expression> elseexpression);
};
class MatchExpression : public Expression {
// is it deleted?
};
class TypeCastExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    std::unique_ptr<Type> typenobounds;
public:
    TypeCastExpression(std::unique_ptr<Expression> expression,
                       std::unique_ptr<Type> typenobounds);
};  
class AssignmentExpression : public Expression {
private:
    std::unique_ptr<Expression> leftexpression;
    std::unique_ptr<Expression> rightexpression;
public:
    AssignmentExpression(std::unique_ptr<Expression> leftexpression,
                         std::unique_ptr<Expression> rightexpression);
};
class CompoundAssignmentExpression : public Expression {
private:
    std::unique_ptr<Expression> leftexpression;
    std::unique_ptr<Expression> rightexpression;
public:
    CompoundAssignmentExpression(std::unique_ptr<Expression> leftexpression,
                                 std::unique_ptr<Expression> rightexpression);
};
class ArrayElements : public Expression {
private:
    std::vector<std::unique_ptr<Expression>> expressions;
    bool istwo; 
public:
    ArrayElements(std::vector<std::unique_ptr<Expression>>&& expressions, bool istwo);
};
class TupleElements : public Expression {
private:
    std::vector<std::unique_ptr<Expression>> expressions;
public:
    explicit TupleElements(std::vector<std::unique_ptr<Expression>>&& expressions);
};
class StructExprFields : public Expression {
private:
    std::vector<std::unique_ptr<StructExprField>> structexprfields;
    std::unique_ptr<StructBase> structbase;
public:
    StructExprFields(std::vector<std::unique_ptr<StructExprField>>&& structexprfields,
                     std::unique_ptr<StructBase> structbase);
};
class StructExprField : public Expression {
private:
    std::string identifier;
    int tupleindex;
    std::unique_ptr<Expression> expression;
public:
    StructExprField(std::string identifier, int tupleindex, std::unique_ptr<Expression> expression);
};
class StructBase : public Expression {
private:
    std::unique_ptr<Expression> expression;
public:
    explicit StructBase(std::unique_ptr<Expression> expression);
};
class CallParams : public Expression {
private:
    std::vector<std::unique_ptr<Expression>> expressions;
public:
    explicit CallParams(std::vector<std::unique_ptr<Expression>>&& expressions);
};
class Conditions : public Expression {
private:
    std::unique_ptr<Expression> expression; // except structexpression
public:
    explicit Conditions(std::unique_ptr<Expression> expression);
};
// class MatchArms : public Expression {};
// class MatchArm : public Expression {};
// class MatchArmGuard : public Expression {};

class UnaryExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    Token unarytype;
public:
    UnaryExpression(std::unique_ptr<Expression> expression, Token unarytype);
};
class BinaryExpression : public Expression {
private:
    std::unique_ptr<Expression> leftexpression;
    std::unique_ptr<Expression> rightexpression;
    Token binarytype;
public:
    BinaryExpression(std::unique_ptr<Expression> leftexpression,
                     std::unique_ptr<Expression> rightexpression,
                     Token binarytype);
};

// PATTERN Syntax
class Pattern : public ASTNode {
private:
    std::vector<std::unique_ptr<PatternNoTopAlt>> patternnotopalts;
public:
    explicit Pattern(std::vector<std::unique_ptr<PatternNoTopAlt>>&& patternnotopalts);
};
class PatternWithoutRange : public ASTNode {
private:
    std::unique_ptr<ASTNode> astnode; /* LiteralPattern
    | IdentifierPattern
    | WildcardPattern
    | RestPattern
    | StructPattern
    | TupleStructPattern
    | TuplePattern
    | GroupedPattern
    | SlicePattern
    | PathPattern
    */
public:
    explicit PatternWithoutRange(std::unique_ptr<ASTNode> astnode);
};
class PatternNoTopAlt : public ASTNode {
private:
    std::unique_ptr<PatternWithoutRange> patternwithoutrange;
public:
    explicit PatternNoTopAlt(std::unique_ptr<PatternWithoutRange> patternwithoutrange);
};
class LiteralPattern : public ASTNode {
private:
    bool isnegative;
    std::unique_ptr<LiteralExpression> literalexprerssion;
public:
    LiteralPattern(bool isnegative, std::unique_ptr<LiteralExpression> literalexprerssion);
};
class IdentifierPattern : public ASTNode {
private:
    bool hasref;
    bool hasmut;
    std::string identifier;
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
public:
    IdentifierPattern(bool hasref, bool hasmut, std::string identifier, std::unique_ptr<PatternNoTopAlt> patternnotopalt);
};
class WildcardPattern : public ASTNode {
private:
    // nothing but _
public:
    WildcardPattern();
};
class RestPattern : public ASTNode {
private:
    // temporary nothing
public:
    RestPattern();
};
class StructPattern : public ASTNode {
private:
    std::unique_ptr<PathInExpression> pathinexpression;
    std::unique_ptr<StructPatternElements> structpatternelements;
public:
    StructPattern(std::unique_ptr<PathInExpression> pathinexpression,
                  std::unique_ptr<StructPatternElements> structpatternelements);
};
class TupleStructPattern : public ASTNode {
private:
    std::unique_ptr<PathInExpression> pathinexpression;
    std::unique_ptr<TupleStructItems> tuplestructitems;
public:
    TupleStructPattern(std::unique_ptr<PathInExpression> pathinexpression,
                       std::unique_ptr<TupleStructItems> tuplestructitems);
};
class TuplePattern : public ASTNode {
private:
    std::unique_ptr<TuplePatternItems> tuplepatternitems;
public:
    explicit TuplePattern(std::unique_ptr<TuplePatternItems> tuplepatternitems);
};
class GroupedPattern : public ASTNode {
private:
    std::unique_ptr<Pattern> pattern;
public:
    explicit GroupedPattern(std::unique_ptr<Pattern> pattern);
};
class SlicePattern : public ASTNode {
private:
    std::unique_ptr<SlicePatternItems> slicepatternitems;
public:
    explicit SlicePattern(std::unique_ptr<SlicePatternItems> slicepatternitems);
};
class PathPattern : public ASTNode {
private:
    std::unique_ptr<PathExpression> pathexpression;
public:
    explicit PathPattern(std::unique_ptr<PathExpression> pathexpression);
};
class StructPatternElements : public ASTNode {
private:
    std::unique_ptr<StructPatternField> structpatternfield;
    bool hascomma;
public:
    StructPatternElements(std::unique_ptr<StructPatternField> structpatternfield, bool hascomma);
};
class StructPatternFields : public ASTNode {
private:
    std::vector<StructPatternField> structpatternfields;
public:
    explicit StructPatternFields(std::vector<StructPatternField>&& structpatternfields);
};
class StructPatternField : public ASTNode {
private:
    std::string identifier_or_index;
    std::unique_ptr<Pattern> pattern;
    bool hasmut;
    bool isindex;
public:
    StructPatternField(std::string identifier_or_index, std::unique_ptr<Pattern> pattern, bool hasmut, bool isindex);
};
class TupleStructItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns; 
public:
    explicit TupleStructItems(std::vector<std::unique_ptr<Pattern>>&& patterns);
};
class TuplePatternItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns;
    bool type; // true: two, false: one
public:
    TuplePatternItems(std::vector<std::unique_ptr<Pattern>>&& patterns, bool type);
};
class SlicePatternItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns;
public:
    explicit SlicePatternItems(std::vector<std::unique_ptr<Pattern>>&& patterns);
};

// TYPE Syntax
class Type : public ASTNode {
public:
    Type();
    virtual ~Type();
};
// class TypeNoBounds : public Type {
// private:
//     std::unique_ptr<ASTNode> astnode; /*
//     `TypePath
//     | ArrayType
//     | SliceType
//     | InferredType
//     | ReferenceType
//     */
// public:
//     explicit TypeNoBounds(std::unique_ptr<ASTNode> astnode);
// };
// class ParenthesizedType : public ASTNode {
// private:
//     std::unique_ptr<Type> type;
// public:
//     explicit ParenthesizedType(std::unique_ptr<Type> type);
// };
class TypePath : public Type {
private:
    std::unique_ptr<SimplePathSegment> simplepathsegement;
public:
    explicit TypePath(std::unique_ptr<SimplePathSegment> simplepathsegement);
};
// class TupleType : public ASTNode {
// private:
//     std::vector<std::unique_ptr<Type>> types;
// public:
//     explicit TupleType(std::vector<std::unique_ptr<Type>>&& types);
// };
// class NeverType : public ASTNode {
// private:
//     // nothing but !
// public:
//     NeverType();
// };
class ArrayType : public Type {
private:
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
public:
    ArrayType(std::unique_ptr<Type> type, std::unique_ptr<Expression> expression);
};
class SliceType : public Type {
private:
    std::unique_ptr<Type> type;
public:
    explicit SliceType(std::unique_ptr<Type> type);
};
class InferredType : public Type {
private:
    // nothing but _
public:
    InferredType();
};
class ReferenceType : public Type {
private:
    std::unique_ptr<Type> type;
    bool ismut;
public:
    ReferenceType(std::unique_ptr<Type> type, bool ismut);
};

// PATH Syntax
class SimplePath : public ASTNode {
private:
    std::vector<std::unique_ptr<SimplePathSegment>> simplepathsegements;
public:
    explicit SimplePath(std::vector<std::unique_ptr<SimplePathSegment>>&& simplepathsegements);
};
class SimplePathSegment : public ASTNode {
private:
    std::string identifier;
    bool isself, isSelf;
public:
    SimplePathSegment(std::string identifier, bool isself, bool isSelf);
};
class PathInExpression : public ASTNode {
private:
    // temporary nothing
public:
    PathInExpression();
};
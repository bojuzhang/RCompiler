#include <algorithm>
#include <memory>
#include <string>
#include <vector>

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
class FunctionQualifiers;
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
class ExpressionWithoutBlock;
class ExpressionWithBlock;
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
class NegationExpression;
class ArithmeticOrLogicalExpression;
class ComparisonExpression;
class LazyBooleanExpression;
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
class TypeNoBounds;
class ParenthesizedType;
class TypePath;
class TupleType;
class NeverType;
class ArrayType;
class SliceType;
class InferredType;

// PATH Syntax
class SimplePath;
class SimplePathSegment;
class PathInExpression;


class ASTNode {};

// ITEM Syntax
class Crate : public ASTNode {
private:
    std::vector<std::unique_ptr<Item>> items;
};
class Item : public ASTNode {
private:
    std::unique_ptr<ASTNode> item;
};
class Function : public ASTNode {
private:
    std::unique_ptr<FunctionQualifiers> functionqualifiers;
    std::string indentifier_name;
    std::unique_ptr<FunctionParameters> functionparameters;
    std::unique_ptr<FunctionReturnType> functionreturntype;
    std::unique_ptr<BlockExpression> blockexpression;
};
class ConstantItem : public ASTNode {
private:
    std::string identifier;
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
};
class Module : public ASTNode {
private:
    std::string indentifier;
    std::vector<std::unique_ptr<Item>> items;
};
class StructStruct : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<StructFields> structfileds;
};
class TupleStruct : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<TupleFields> tuplefields;
};
class Enumeration : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<EnumVariants> enumvariants;
};
class InherentImpl : public ASTNode {
    std::unique_ptr<Type> type;
    std::vector<std::unique_ptr<AssociatedItem>> associateditems;
};
class FunctionQualifiers : public ASTNode {
private:
    bool isconst;
};
class FunctionParameters : public ASTNode {
private:
    std::vector<std::unique_ptr<FunctionParam>> functionparams;
};
class FunctionParam : public ASTNode {
private:
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
    std::unique_ptr<Type> type;
};
class FunctionReturnType : public ASTNode {
private:
    std::unique_ptr<Type> type;
};
class StructFields : public ASTNode {
private:
    std::vector<std::unique_ptr<StructField>> structfields; 
};
class StructField : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<Type> type;
};
class TupleFields : public ASTNode {
private:
    std::vector<std::unique_ptr<TupleField>> tuplefields; 
};
class TupleField : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<Type> type;
};
class EnumVariants : public ASTNode {
private:
    std::vector<std::unique_ptr<EnumVariant>> enumvariants; 
};
class EnumVariant : public ASTNode {
private:
    std::string indentifier;
    std::unique_ptr<ASTNode> tupleorstruct;
    std::unique_ptr<EnumVariantDiscriminant> enumvariantdiscriminant;
};
class EnumVariantTuple : public ASTNode {
private:
    std::unique_ptr<TupleFields> tuplefields;
};
class EnumVariantStruct : public ASTNode {
private:
    std::unique_ptr<StructFields> structfields;
};
class EnumVariantDiscriminant : public ASTNode {
private:
    std::unique_ptr<Expression> expression;
};
class AssociatedItem : public ASTNode {
    std::unique_ptr<ASTNode> consttantitem_or_function;
};

// STATEMENT Syntax
class Statement : public ASTNode{
private:
    std::unique_ptr<ASTNode> astnode; // item or letstatement or expressionstatement
};
class LetStatement : public ASTNode {
private:
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
};
class ExpressionStatement : public ASTNode {
private:
    std::unique_ptr<ASTNode> astnode; // expression withblock or withnotblock
};

// EXPRESSION Syntax
class Expression : public ASTNode{};
class ExpressionWithoutBlock : public ASTNode{};
class ExpressionWithBlock : public ASTNode{};
class LiteralExpression : public ASTNode {};
class PathExpression : public ASTNode {};
class GroupedExpression : public ASTNode {};
class ArrayExpression : public ASTNode {};
class IndexExpression : public ASTNode {};
class TupleExpression : public ASTNode {};
class TupleIndexingExpression : public ASTNode {};
class StructExpression : public ASTNode {};
class CallExpression : public ASTNode {};
class MethodCallExpression : public ASTNode {};
class FieldExpression : public ASTNode {};
class ContinueExpression : public ASTNode {};
class BreakExpression : public ASTNode {};
class ReturnExpression : public ASTNode {};
class UnderscoreExpression : public ASTNode {};
class BlockExpression : public ASTNode {};
class ConstBlockExpression : public ASTNode {};
class InfiniteLoopExpression : public ASTNode {};
class PredicateLoopExpression : public ASTNode {};
class IfExpression : public ASTNode {};
class MatchExpression : public ASTNode {};
class NegationExpression : public ASTNode {};
class ArithmeticOrLogicalExpression : public ASTNode {};
class ComparisonExpression : public ASTNode {};
class LazyBooleanExpression : public ASTNode {};
class TypeCastExpression : public ASTNode {};
class AssignmentExpression : public ASTNode {};
class CompoundAssignmentExpression : public ASTNode {};
class ArrayElements : public ASTNode {};
class TupleElements : public ASTNode {};
class StructExprFields : public ASTNode {};
class StructExprField : public ASTNode {};
class StructBase : public ASTNode {};
class CallParams : public ASTNode {};
class Conditions : public ASTNode {};
class MatchArms : public ASTNode {};
class MatchArm : public ASTNode {};
class MatchArmGuard : public ASTNode {};

// PATTERN Syntax
class Pattern : public ASTNode {
private:
    std::vector<std::unique_ptr<PatternNoTopAlt>> patternnotopalts;
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
};
class PatternNoTopAlt : public ASTNode {
private:
    std::unique_ptr<PatternWithoutRange> patternwithoutrange;
};
class LiteralPattern : public ASTNode {
private:
    bool isnegative;
    std::unique_ptr<LiteralExpression> literalexprerssion;
};
class IdentifierPattern : public ASTNode {
private:
    bool hasref;
    bool hasmut;
    std::string identifier;
    std::unique_ptr<PatternNoTopAlt> patternnotopalt;
};
class WildcardPattern : public ASTNode {
private:
    // nothing but _
};
class RestPattern : public ASTNode {
private:
    // temporary nothing
};
class StructPattern : public ASTNode {
private:
    std::unique_ptr<PathInExpression> pathinexpression;
    std::unique_ptr<StructPatternElements> structpatternelements;
};
class TupleStructPattern : public ASTNode {
private:
    std::unique_ptr<PathInExpression> pathinexpression;
    std::unique_ptr<TupleStructItems> tuplestructitems;
};
class TuplePattern : public ASTNode {
private:
    std::unique_ptr<TuplePatternItems> tuplepatternitems;
};
class GroupedPattern : public ASTNode {
private:
    std::unique_ptr<Pattern> pattern;
};
class SlicePattern : public ASTNode {
private:
    std::unique_ptr<SlicePatternItems> slicepatternitems;
};
class PathPattern : public ASTNode {
private:
    std::unique_ptr<PathExpression> pathexpression;
};
class StructPatternElements : public ASTNode {
private:
    std::unique_ptr<StructPatternField> structpatternfield;
    bool hascomma;
};
class StructPatternFields : public ASTNode {
private:
    std::vector<StructPatternField> structpatternfields;
};
class StructPatternField : public ASTNode {
private:
    std::string indentifier_or_index;
    std::unique_ptr<Pattern> pattern;
    bool hasmut;
    bool isindex;
};
class TupleStructItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns; 
};
class TuplePatternItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns;
    bool type; // true: two, false: one
};
class SlicePatternItems : public ASTNode {
private:
    std::vector<std::unique_ptr<Pattern>> patterns;
};

// TYPE Syntax
class Type : public ASTNode {
private:
    std::unique_ptr<TypeNoBounds> typenobounds;
};
class TypeNoBounds : public ASTNode {
private:
    std::unique_ptr<ASTNode> astnode; /* ParenthesizedType
    | TypePath
    | TupleType
    | NeverType
    | ArrayType
    | SliceType
    | InferredType
    */
};
class ParenthesizedType : public ASTNode {
private:
    std::unique_ptr<Type> type;
};
class TypePath : public ASTNode {
private:
    std::vector<std::unique_ptr<SimplePathSegment>> simplepathsegements;
};
class TupleType : public ASTNode {
private:
    std::vector<std::unique_ptr<Type>> types;
};
class NeverType : public ASTNode {
private:
    // nothing but !
};
class ArrayType : public ASTNode {
private:
    std::unique_ptr<Type> type;
    std::unique_ptr<Expression> expression;
};
class SliceType : public ASTNode {
private:
    std::unique_ptr<Type> type;
};
class InferredType : public ASTNode {
private:
    // nothing but _
};

// PATH Syntax
class SimplePath : public ASTNode {
private:
    std::vector<std::unique_ptr<SimplePathSegment>> simplepathsegements;
};
class SimplePathSegment : public ASTNode {
private:
    std::string indentifier;
    bool isself, isSelf, iscrate, issuper;
};
class PathInExpression : public ASTNode {
private:
    // temporary nothing
};
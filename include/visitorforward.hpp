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
class LiteralPattern;
class IdentifierPattern;
class WildcardPattern;
class PathPattern;
class ReferencePattern;
class ReferenceType;

// TYPE Syntax
class Type;
// class TypeNoBounds;
// class ParenthesizedType;
class TypePath;
// class TupleType;
// class NeverType;
class ArrayType;
// class SliceType;
// class InferredType;
class ReferenceType;
class UnitType;

// PATH Syntax
class SimplePath;
class SimplePathSegment;
class PathInExpression;




class ASTVisitor;
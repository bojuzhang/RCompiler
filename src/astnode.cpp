#include "astnodes.hpp"

// constrict function

// ASTNode
ASTNode::ASTNode() = default;
ASTNode::~ASTNode() = default;

// ITEM Syntax
Crate::Crate(std::vector<std::unique_ptr<Item>>&& items) 
    : items(std::move(items)) {}

Item::Item(std::unique_ptr<ASTNode> item) 
    : item(std::move(item)) {}

Function::Function(std::unique_ptr<FunctionQualifiers> functionqualifiers,
                   std::string identifier_name,
                   std::unique_ptr<FunctionParameters> functionparameters,
                   std::unique_ptr<FunctionReturnType> functionreturntype,
                   std::unique_ptr<BlockExpression> blockexpression)
    : functionqualifiers(std::move(functionqualifiers)),
      identifier_name(std::move(identifier_name)),
      functionparameters(std::move(functionparameters)),
      functionreturntype(std::move(functionreturntype)),
      blockexpression(std::move(blockexpression)) {}

ConstantItem::ConstantItem(std::string identifier,
                           std::unique_ptr<Type> type,
                           std::unique_ptr<Expression> expression)
    : identifier(std::move(identifier)),
      type(std::move(type)),
      expression(std::move(expression)) {}

Module::Module(std::string identifier,
               std::vector<std::unique_ptr<Item>>&& items)
    : identifier(std::move(identifier)),
      items(std::move(items)) {}

StructStruct::StructStruct(std::string identifier,
                           std::unique_ptr<StructFields> structfileds)
    : identifier(std::move(identifier)),
      structfileds(std::move(structfileds)) {}

TupleStruct::TupleStruct(std::string identifier,
                         std::unique_ptr<TupleFields> tuplefields)
    : identifier(std::move(identifier)),
      tuplefields(std::move(tuplefields)) {}

Enumeration::Enumeration(std::string identifier,
                         std::unique_ptr<EnumVariants> enumvariants)
    : identifier(std::move(identifier)),
      enumvariants(std::move(enumvariants)) {}

InherentImpl::InherentImpl(std::unique_ptr<Type> type,
                           std::vector<std::unique_ptr<AssociatedItem>>&& associateditems)
    : type(std::move(type)),
      associateditems(std::move(associateditems)) {}

FunctionQualifiers::FunctionQualifiers(bool isconst) 
    : isconst(isconst) {}

FunctionParameters::FunctionParameters(std::vector<std::unique_ptr<FunctionParam>>&& functionparams)
    : functionparams(std::move(functionparams)) {}

FunctionParam::FunctionParam(std::unique_ptr<PatternNoTopAlt> patternnotopalt,
                             std::unique_ptr<Type> type)
    : patternnotopalt(std::move(patternnotopalt)),
      type(std::move(type)) {}

FunctionReturnType::FunctionReturnType(std::unique_ptr<Type> type) 
    : type(std::move(type)) {}

StructFields::StructFields(std::vector<std::unique_ptr<StructField>>&& structfields)
    : structfields(std::move(structfields)) {}

StructField::StructField(std::string identifier,
                         std::unique_ptr<Type> type)
    : identifier(std::move(identifier)),
      type(std::move(type)) {}

TupleFields::TupleFields(std::vector<std::unique_ptr<TupleField>>&& tuplefields)
    : tuplefields(std::move(tuplefields)) {}

TupleField::TupleField(std::string identifier,
                       std::unique_ptr<Type> type)
    : identifier(std::move(identifier)),
      type(std::move(type)) {}

EnumVariants::EnumVariants(std::vector<std::unique_ptr<EnumVariant>>&& enumvariants)
    : enumvariants(std::move(enumvariants)) {}

EnumVariant::EnumVariant(std::string identifier,
                         std::unique_ptr<ASTNode> tupleorstruct,
                         std::unique_ptr<EnumVariantDiscriminant> enumvariantdiscriminant)
    : identifier(std::move(identifier)),
      tupleorstruct(std::move(tupleorstruct)),
      enumvariantdiscriminant(std::move(enumvariantdiscriminant)) {}

EnumVariantTuple::EnumVariantTuple(std::unique_ptr<TupleFields> tuplefields)
    : tuplefields(std::move(tuplefields)) {}

EnumVariantStruct::EnumVariantStruct(std::unique_ptr<StructFields> structfields)
    : structfields(std::move(structfields)) {}

EnumVariantDiscriminant::EnumVariantDiscriminant(std::unique_ptr<Expression> expression)
    : expression(std::move(expression)) {}

AssociatedItem::AssociatedItem(std::unique_ptr<ASTNode> consttantitem_or_function)
    : consttantitem_or_function(std::move(consttantitem_or_function)) {}

// STATEMENT Syntax
Statement::Statement(std::unique_ptr<ASTNode> astnode) 
    : astnode(std::move(astnode)) {}

LetStatement::LetStatement(std::unique_ptr<PatternNoTopAlt> patternnotopalt,
                           std::unique_ptr<Type> type,
                           std::unique_ptr<Expression> expression)
    : patternnotopalt(std::move(patternnotopalt)),
      type(std::move(type)),
      expression(std::move(expression)) {}

ExpressionStatement::ExpressionStatement(std::unique_ptr<ASTNode> astnode) 
    : astnode(std::move(astnode)) {}

// EXPRESSION Syntax
Expression::Expression() = default;

LiteralExpression::LiteralExpression(std::string literal, Token tokentype)
    : literal(std::move(literal)), tokentype(tokentype) {}

PathExpression::PathExpression(std::unique_ptr<SimplePath> simplepath)
    : simplepath(std::move(simplepath)) {}

GroupedExpression::GroupedExpression(std::unique_ptr<Expression> expression)
    : expression(std::move(expression)) {}

ArrayExpression::ArrayExpression(std::unique_ptr<ArrayElements> arrayelements)
    : arrayelements(std::move(arrayelements)) {}

IndexExpression::IndexExpression(std::unique_ptr<Expression> expressionout,
                                 std::unique_ptr<Expression> expressionin)
    : expressionout(std::move(expressionout)),
      expressionin(std::move(expressionin)) {}

TupleExpression::TupleExpression(std::unique_ptr<TupleElements> tupleelements)
    : tupleelements(std::move(tupleelements)) {}

TupleIndexingExpression::TupleIndexingExpression(std::unique_ptr<Expression> expression, int tupleindex)
    : expression(std::move(expression)), tupleindex(tupleindex) {}

StructExpression::StructExpression(std::unique_ptr<PathInExpression> pathinexpression,
                                   std::unique_ptr<ASTNode> structinfo)
    : pathinexpression(std::move(pathinexpression)),
      structinfo(std::move(structinfo)) {}

CallExpression::CallExpression(std::unique_ptr<Expression> expression,
                               std::unique_ptr<CallParams> callparams)
    : expression(std::move(expression)),
      callparams(std::move(callparams)) {}

FieldExpression::FieldExpression(std::unique_ptr<Expression> expression, std::string identifier)
    : expression(std::move(expression)), identifier(std::move(identifier)) {}

ContinueExpression::ContinueExpression() = default;

BreakExpression::BreakExpression(std::unique_ptr<Expression> expression)
    : expression(std::move(expression)) {}

ReturnExpression::ReturnExpression(std::unique_ptr<Expression> expression)
    : expression(std::move(expression)) {}

UnderscoreExpression::UnderscoreExpression() = default;

BlockExpression::BlockExpression(std::unique_ptr<Statement> statement)
    : statement(std::move(statement)) {}

ConstBlockExpression::ConstBlockExpression(std::unique_ptr<BlockExpression> blockexpression)
    : blockexpression(std::move(blockexpression)) {}

InfiniteLoopExpression::InfiniteLoopExpression(std::unique_ptr<BlockExpression> blockexpression)
    : blockexpression(std::move(blockexpression)) {}

PredicateLoopExpression::PredicateLoopExpression(std::unique_ptr<Conditions> conditions,
                                                 std::unique_ptr<BlockExpression> blockexpression)
    : conditions(std::move(conditions)),
      blockexpression(std::move(blockexpression)) {}

IfExpression::IfExpression(std::unique_ptr<Conditions> conditions,
                           std::unique_ptr<BlockExpression> ifblockexpression,
                           std::unique_ptr<Expression> elseexpression)
    : conditions(std::move(conditions)),
      ifblockexpression(std::move(ifblockexpression)),
      elseexpression(std::move(elseexpression)) {}

TypeCastExpression::TypeCastExpression(std::unique_ptr<Expression> expression,
                                       std::unique_ptr<TypeNoBounds> typenobounds)
    : expression(std::move(expression)),
      typenobounds(std::move(typenobounds)) {}

AssignmentExpression::AssignmentExpression(std::unique_ptr<Expression> leftexpression,
                                           std::unique_ptr<Expression> rightexpression)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)) {}

CompoundAssignmentExpression::CompoundAssignmentExpression(std::unique_ptr<Expression> leftexpression,
                                                           std::unique_ptr<Expression> rightexpression)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)) {}

ArrayElements::ArrayElements(std::vector<std::unique_ptr<Expression>>&& expressions, bool istwo)
    : expressions(std::move(expressions)), istwo(istwo) {}

TupleElements::TupleElements(std::vector<std::unique_ptr<Expression>>&& expressions)
    : expressions(std::move(expressions)) {}

StructExprFields::StructExprFields(std::vector<std::unique_ptr<StructExprField>>&& structexprfields,
                                   std::unique_ptr<StructBase> structbase)
    : structexprfields(std::move(structexprfields)),
      structbase(std::move(structbase)) {}

StructExprField::StructExprField(std::string identifier, int tupleindex, std::unique_ptr<Expression> expression)
    : identifier(std::move(identifier)), tupleindex(tupleindex), expression(std::move(expression)) {}

StructBase::StructBase(std::unique_ptr<Expression> expression) 
    : expression(std::move(expression)) {}

CallParams::CallParams(std::vector<std::unique_ptr<Expression>>&& expressions)
    : expressions(std::move(expressions)) {}

Conditions::Conditions(std::unique_ptr<Expression> expression) 
    : expression(std::move(expression)) {}

UnaryExpression::UnaryExpression(std::unique_ptr<Expression> expression, Token unarytype)
    : expression(std::move(expression)), unarytype(unarytype) {}

BinaryExpression::BinaryExpression(std::unique_ptr<Expression> leftexpression,
                                   std::unique_ptr<Expression> rightexpression,
                                   Token binarytype)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)),
      binarytype(binarytype) {}

// PATTERN Syntax
Pattern::Pattern(std::vector<std::unique_ptr<PatternNoTopAlt>>&& patternnotopalts)
    : patternnotopalts(std::move(patternnotopalts)) {}

PatternWithoutRange::PatternWithoutRange(std::unique_ptr<ASTNode> astnode) 
    : astnode(std::move(astnode)) {}

PatternNoTopAlt::PatternNoTopAlt(std::unique_ptr<PatternWithoutRange> patternwithoutrange)
    : patternwithoutrange(std::move(patternwithoutrange)) {}

LiteralPattern::LiteralPattern(bool isnegative, std::unique_ptr<LiteralExpression> literalexprerssion)
    : isnegative(isnegative), literalexprerssion(std::move(literalexprerssion)) {}

IdentifierPattern::IdentifierPattern(bool hasref, bool hasmut, std::string identifier, std::unique_ptr<PatternNoTopAlt> patternnotopalt)
    : hasref(hasref), hasmut(hasmut), identifier(std::move(identifier)), patternnotopalt(std::move(patternnotopalt)) {}

WildcardPattern::WildcardPattern() = default;

RestPattern::RestPattern() = default;

StructPattern::StructPattern(std::unique_ptr<PathInExpression> pathinexpression,
                             std::unique_ptr<StructPatternElements> structpatternelements)
    : pathinexpression(std::move(pathinexpression)),
      structpatternelements(std::move(structpatternelements)) {}

TupleStructPattern::TupleStructPattern(std::unique_ptr<PathInExpression> pathinexpression,
                                       std::unique_ptr<TupleStructItems> tuplestructitems)
    : pathinexpression(std::move(pathinexpression)),
      tuplestructitems(std::move(tuplestructitems)) {}

TuplePattern::TuplePattern(std::unique_ptr<TuplePatternItems> tuplepatternitems)
    : tuplepatternitems(std::move(tuplepatternitems)) {}

GroupedPattern::GroupedPattern(std::unique_ptr<Pattern> pattern) 
    : pattern(std::move(pattern)) {}

SlicePattern::SlicePattern(std::unique_ptr<SlicePatternItems> slicepatternitems)
    : slicepatternitems(std::move(slicepatternitems)) {}

PathPattern::PathPattern(std::unique_ptr<PathExpression> pathexpression)
    : pathexpression(std::move(pathexpression)) {}

StructPatternElements::StructPatternElements(std::unique_ptr<StructPatternField> structpatternfield, bool hascomma)
    : structpatternfield(std::move(structpatternfield)), hascomma(hascomma) {}

StructPatternFields::StructPatternFields(std::vector<StructPatternField>&& structpatternfields)
    : structpatternfields(std::move(structpatternfields)) {}

StructPatternField::StructPatternField(std::string identifier_or_index, std::unique_ptr<Pattern> pattern, bool hasmut, bool isindex)
    : identifier_or_index(std::move(identifier_or_index)),
      pattern(std::move(pattern)),
      hasmut(hasmut),
      isindex(isindex) {}

TupleStructItems::TupleStructItems(std::vector<std::unique_ptr<Pattern>>&& patterns)
    : patterns(std::move(patterns)) {}

TuplePatternItems::TuplePatternItems(std::vector<std::unique_ptr<Pattern>>&& patterns, bool type)
    : patterns(std::move(patterns)), type(type) {}

SlicePatternItems::SlicePatternItems(std::vector<std::unique_ptr<Pattern>>&& patterns)
    : patterns(std::move(patterns)) {}

// TYPE Syntax
Type::Type(std::unique_ptr<TypeNoBounds> typenobounds) 
    : typenobounds(std::move(typenobounds)) {}

TypeNoBounds::TypeNoBounds(std::unique_ptr<ASTNode> astnode) 
    : astnode(std::move(astnode)) {}

ParenthesizedType::ParenthesizedType(std::unique_ptr<Type> type) 
    : type(std::move(type)) {}

TypePath::TypePath(std::vector<std::unique_ptr<SimplePathSegment>>&& simplepathsegements)
    : simplepathsegements(std::move(simplepathsegements)) {}

TupleType::TupleType(std::vector<std::unique_ptr<Type>>&& types) 
    : types(std::move(types)) {}

NeverType::NeverType() = default;

ArrayType::ArrayType(std::unique_ptr<Type> type, std::unique_ptr<Expression> expression)
    : type(std::move(type)), expression(std::move(expression)) {}

SliceType::SliceType(std::unique_ptr<Type> type) 
    : type(std::move(type)) {}

InferredType::InferredType() = default;

// PATH Syntax
SimplePath::SimplePath(std::vector<std::unique_ptr<SimplePathSegment>>&& simplepathsegements)
    : simplepathsegements(std::move(simplepathsegements)) {}

SimplePathSegment::SimplePathSegment(std::string identifier, bool isself, bool isSelf, bool iscrate, bool issuper)
    : identifier(std::move(identifier)), isself(isself), isSelf(isSelf), iscrate(iscrate), issuper(issuper) {}

PathInExpression::PathInExpression() = default;
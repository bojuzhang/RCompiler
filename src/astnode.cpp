#include "astnodes.hpp"
#include <utility>
#include <iostream>

// constrict function

// ASTNode
ASTNode::ASTNode() = default;

// ITEM Syntax
Crate::Crate(std::vector<std::shared_ptr<Item>>&& items)
    : items(std::move(items)) {}

Item::Item(std::shared_ptr<ASTNode> item)
    : item(std::move(item)) {}

Function::Function(bool isconst,
                   std::string identifier_name,
                   std::shared_ptr<FunctionParameters> functionparameters,
                   std::shared_ptr<FunctionReturnType> functionreturntype,
                   std::shared_ptr<BlockExpression> blockexpression)
    : isconst(std::move(isconst)),
      identifier_name(std::move(identifier_name)),
      functionparameters(std::move(functionparameters)),
      functionreturntype(std::move(functionreturntype)),
      blockexpression(std::move(blockexpression)) {}

ConstantItem::ConstantItem(std::string identifier,
                           std::shared_ptr<Type> type,
                           std::shared_ptr<Expression> expression)
    : identifier(std::move(identifier)),
      type(std::move(type)),
      expression(std::move(expression)) {}

// Module::Module(std::string identifier,
//                std::vector<std::unique_ptr<Item>>&& items)
//     : identifier(std::move(identifier)),
//       items(std::move(items)) {}

StructStruct::StructStruct(std::string identifier,
                           std::shared_ptr<StructFields> structfileds,
                           bool issemi)
    : identifier(std::move(identifier)),
      structfileds(std::move(structfileds)),
      issemi(issemi) {}

// TupleStruct::TupleStruct(std::string identifier,
//                          std::unique_ptr<TupleFields> tuplefields)
//     : identifier(std::move(identifier)),
//       tuplefields(std::move(tuplefields)) {}

Enumeration::Enumeration(std::string identifier,
                         std::shared_ptr<EnumVariants> enumvariants)
    : identifier(std::move(identifier)),
      enumvariants(std::move(enumvariants)) {}

InherentImpl::InherentImpl(std::shared_ptr<Type> type,
                           std::vector<std::shared_ptr<AssociatedItem>>&& associateditems)
    : type(std::move(type)),
      associateditems(std::move(associateditems)) {}

// FunctionQualifiers::FunctionQualifiers(bool isconst) 
//     : isconst(isconst) {}

FunctionParameters::FunctionParameters(std::vector<std::shared_ptr<FunctionParam>>&& functionparams)
    : functionparams(std::move(functionparams)) {}

FunctionParam::FunctionParam(std::shared_ptr<Pattern> patternnotopalt,
                             std::shared_ptr<Type> type)
    : patternnotopalt(std::move(patternnotopalt)),
      type(std::move(type)) {}

FunctionReturnType::FunctionReturnType(std::shared_ptr<Type> type)
    : type(std::move(type)) {}

StructFields::StructFields(std::vector<std::shared_ptr<StructField>>&& structfields)
    : structfields(std::move(structfields)) {}

StructField::StructField(std::string identifier,
                         std::shared_ptr<Type> type)
    : identifier(std::move(identifier)),
      type(std::move(type)) {}

// TupleFields::TupleFields(std::vector<std::unique_ptr<TupleField>>&& tuplefields)
//     : tuplefields(std::move(tuplefields)) {}

// TupleField::TupleField(std::string identifier,
//                        std::unique_ptr<Type> type)
//     : identifier(std::move(identifier)),
//       type(std::move(type)) {}

EnumVariants::EnumVariants(std::vector<std::shared_ptr<EnumVariant>>&& enumvariants)
    : enumvariants(std::move(enumvariants)) {}

EnumVariant::EnumVariant(std::string identifier)
    : identifier(std::move(identifier)) {}

// EnumVariantTuple::EnumVariantTuple(std::unique_ptr<TupleFields> tuplefields)
//     : tuplefields(std::move(tuplefields)) {}

// EnumVariantStruct::EnumVariantStruct(std::unique_ptr<StructFields> structfields)
//     : structfields(std::move(structfields)) {}

// EnumVariantDiscriminant::EnumVariantDiscriminant(std::unique_ptr<Expression> expression)
//     : expression(std::move(expression)) {}

AssociatedItem::AssociatedItem(std::shared_ptr<ASTNode> consttantitem_or_function)
    : consttantitem_or_function(std::move(consttantitem_or_function)) {}

// STATEMENT Syntax
Statement::Statement(std::shared_ptr<ASTNode> astnode)
    : astnode(std::move(astnode)) {}

LetStatement::LetStatement(std::shared_ptr<Pattern> patternnotopalt,
                           std::shared_ptr<Type> type,
                           std::shared_ptr<Expression> expression)
    : patternnotopalt(std::move(patternnotopalt)),
      type(std::move(type)),
      expression(std::move(expression)) {}

ExpressionStatement::ExpressionStatement(std::shared_ptr<Expression> astnode, bool hassemi)
    : astnode(std::move(astnode)),
      hassemi(hassemi) {}

// EXPRESSION Syntax
Expression::Expression() = default;

LiteralExpression::LiteralExpression(std::string literal, Token tokentype)
    : literal(std::move(literal)), tokentype(tokentype) {}

PathExpression::PathExpression(std::shared_ptr<SimplePath> simplepath)
    : simplepath(std::move(simplepath)) {}

GroupedExpression::GroupedExpression(std::shared_ptr<Expression> expression)
    : expression(std::move(expression)) {}

ArrayExpression::ArrayExpression(std::shared_ptr<ArrayElements> arrayelements)
    : arrayelements(std::move(arrayelements)) {}

IndexExpression::IndexExpression(std::shared_ptr<Expression> expressionout,
                                 std::shared_ptr<Expression> expressionin)
    : expressionout(std::move(expressionout)),
      expressionin(std::move(expressionin)) {}

TupleExpression::TupleExpression(std::shared_ptr<TupleElements> tupleelements)
    : tupleelements(std::move(tupleelements)) {}

TupleIndexingExpression::TupleIndexingExpression(std::shared_ptr<Expression> expression, int tupleindex)
    : expression(std::move(expression)), tupleindex(tupleindex) {}

StructExpression::StructExpression(std::shared_ptr<PathInExpression> pathinexpression,
                                   std::shared_ptr<ASTNode> structinfo)
    : pathinexpression(std::move(pathinexpression)),
      structinfo(std::move(structinfo)) {}

CallExpression::CallExpression(std::shared_ptr<Expression> expression,
                               std::shared_ptr<CallParams> callparams)
    : expression(std::move(expression)),
      callparams(std::move(callparams)) {}

FieldExpression::FieldExpression(std::shared_ptr<Expression> expression, std::string identifier)
    : expression(std::move(expression)), identifier(std::move(identifier)) {}

ContinueExpression::ContinueExpression() = default;

BreakExpression::BreakExpression(std::shared_ptr<Expression> expression)
    : expression(std::move(expression)) {}

ReturnExpression::ReturnExpression(std::shared_ptr<Expression> expression)
    : expression(std::move(expression)) {}

UnderscoreExpression::UnderscoreExpression() = default;

BlockExpression::BlockExpression(std::vector<std::shared_ptr<Statement>> statements,
                    std::shared_ptr<Expression> expressionwithoutblock)
    : statements(std::move(statements)), expressionwithoutblock(std::move(expressionwithoutblock)) {}

ConstBlockExpression::ConstBlockExpression(std::shared_ptr<BlockExpression> blockexpression)
    : blockexpression(std::move(blockexpression)) {}

InfiniteLoopExpression::InfiniteLoopExpression(std::shared_ptr<BlockExpression> blockexpression)
    : blockexpression(std::move(blockexpression)) {}

PredicateLoopExpression::PredicateLoopExpression(std::shared_ptr<Conditions> conditions,
                                                 std::shared_ptr<BlockExpression> blockexpression)
    : conditions(std::move(conditions)),
      blockexpression(std::move(blockexpression)) {}

IfExpression::IfExpression(std::shared_ptr<Conditions> conditions,
                           std::shared_ptr<BlockExpression> ifblockexpression,
                           std::shared_ptr<Expression> elseexpression)
    : conditions(std::move(conditions)),
      ifblockexpression(std::move(ifblockexpression)),
      elseexpression(std::move(elseexpression)) {
}


TypeCastExpression::TypeCastExpression(std::shared_ptr<Expression> expression,
                                       std::shared_ptr<Type> typenobounds)
    : expression(std::move(expression)),
      typenobounds(std::move(typenobounds)) {}

AssignmentExpression::AssignmentExpression(std::shared_ptr<Expression> leftexpression,
                                           std::shared_ptr<Expression> rightexpression)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)) {}

CompoundAssignmentExpression::CompoundAssignmentExpression(std::shared_ptr<Expression> leftexpression,
                                                           std::shared_ptr<Expression> rightexpression,
                                                           Token type)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)),
      type(type) {}

ArrayElements::ArrayElements(std::vector<std::shared_ptr<Expression>>&& expressions, bool istwo)
    : expressions(std::move(expressions)), istwo(istwo) {}

TupleElements::TupleElements(std::vector<std::shared_ptr<Expression>>&& expressions)
    : expressions(std::move(expressions)) {}

StructExprFields::StructExprFields(std::vector<std::shared_ptr<StructExprField>>&& structexprfields,
                                   std::shared_ptr<StructBase> structbase)
    : structexprfields(std::move(structexprfields)),
      structbase(std::move(structbase)) {}

StructExprField::StructExprField(std::string identifier, int tupleindex, std::shared_ptr<Expression> expression)
    : identifier(std::move(identifier)), tupleindex(tupleindex), expression(std::move(expression)) {}

StructBase::StructBase(std::shared_ptr<Expression> expression)
    : expression(std::move(expression)) {}

CallParams::CallParams(std::vector<std::shared_ptr<Expression>>&& expressions)
    : expressions(std::move(expressions)) {}

Conditions::Conditions(std::shared_ptr<Expression> expression)
    : expression(std::move(expression)) {}

UnaryExpression::UnaryExpression(std::shared_ptr<Expression> expression, Token unarytype)
    : expression(std::move(expression)), unarytype(unarytype) {}

BinaryExpression::BinaryExpression(std::shared_ptr<Expression> leftexpression,
                                   std::shared_ptr<Expression> rightexpression,
                                   Token binarytype)
    : leftexpression(std::move(leftexpression)),
      rightexpression(std::move(rightexpression)),
      binarytype(binarytype) {}

// PATTERN Syntax
LiteralPattern::LiteralPattern(bool isnegative, std::shared_ptr<Expression> literalexprerssion)
    : isnegative(isnegative), literalexprerssion(std::move(literalexprerssion)) {}

IdentifierPattern::IdentifierPattern(bool hasref, bool hasmut, std::string identifier, std::shared_ptr<Pattern> patternnotopalt)
    : hasref(hasref), hasmut(hasmut), identifier(std::move(identifier)), patternnotopalt(std::move(patternnotopalt)) {}

WildcardPattern::WildcardPattern() = default;

PathPattern::PathPattern(std::shared_ptr<Expression> pathexpression)
    : pathexpression(std::move(pathexpression)) {}

ReferencePattern::ReferencePattern(bool singleordouble,
                                   bool hasmut,
                                   std::shared_ptr<Pattern> pattern)
    : singleordouble(singleordouble), hasmut(hasmut), pattern(std::move(pattern)) {}


// TYPE Syntax
// Type::Type(std::unique_ptr<TypeNoBounds> typenobounds) 
//     : typenobounds(std::move(typenobounds)) {}

// TypeNoBounds::TypeNoBounds(std::unique_ptr<ASTNode> astnode) 
//     : astnode(std::move(astnode)) {}

// ParenthesizedType::ParenthesizedType(std::unique_ptr<Type> type) 
//     : type(std::move(type)) {}

TypePath::TypePath(std::shared_ptr<SimplePathSegment> simplepathsegement)
    : simplepathsegement(std::move(simplepathsegement)) {}

// TupleType::TupleType(std::vector<std::unique_ptr<Type>>&& types) 
//     : types(std::move(types)) {}

// NeverType::NeverType() = default;

ArrayType::ArrayType(std::shared_ptr<Type> type, std::shared_ptr<Expression> expression)
    : type(std::move(type)), expression(std::move(expression)) {}

SliceType::SliceType(std::shared_ptr<Type> type)
    : type(std::move(type)) {}

InferredType::InferredType() = default;

ReferenceType::ReferenceType(std::shared_ptr<Type> type, bool ismut)
    : type(std::move(type)), ismut(ismut) {}

// PATH Syntax
SimplePath::SimplePath(std::vector<std::shared_ptr<SimplePathSegment>>&& simplepathsegements)
    : simplepathsegements(std::move(simplepathsegements)) {}

SimplePathSegment::SimplePathSegment(std::string identifier, bool isself, bool isSelf)
    : identifier(std::move(identifier)), isself(isself), isSelf(isSelf) {}

PathInExpression::PathInExpression() = default;
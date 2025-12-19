#pragma once

#include "astnodes.hpp"
#include "lexer.hpp"
#include <memory>
#include <utility>
#include <vector>

class Parser {
private:
    std::vector<std::pair<Token, std::string>> tokens;
    size_t pos = 0;

    enum BindingPower {
        PATH_ACCESS = 200,
        CALL_INDEX = 190,
        STRUCT_EXPR = 185,  // Added for struct expressions
        UNARY = 180,
        CAST = 170,
        MULT_DIV_MOD = 160,
        ADD_SUB = 150,
        SHIFT = 140,
        BIT_AND = 130,
        BIT_XOR = 120,
        BIT_OR = 110,
        COMPARISON = 100,
        LOGICAL_AND = 90,
        LOGICAL_OR = 80,
        ASSIGNMENT = 70,
        FLOW_CONTROL = 60
    };

    int getLeftTokenBP(Token token) {
        switch (token) {
            case Token::kDot:
                return PATH_ACCESS;
                
            case Token::kleftParenthe:
            case Token::kleftSquare:
                return CALL_INDEX;
                
            case Token::kleftCurly:
                return STRUCT_EXPR;
                
            case Token::kas:
                return CAST;
                
            case Token::kStar:
            case Token::kSlash:
            case Token::kPercent:
                return MULT_DIV_MOD;
                
            case Token::kPlus:
            case Token::kMinus:
                return ADD_SUB;
                
            case Token::kShl:
            case Token::kShr:
                return SHIFT;
                
            case Token::kAnd:
                return BIT_AND;
                
            case Token::kCaret:
                return BIT_XOR;
    
            case Token::kOr:
                return BIT_OR;
                
            case Token::kEqEq:
            case Token::kNe:
            case Token::kLt:
            case Token::kGt:
            case Token::kLe:
            case Token::kGe:
                return COMPARISON;
                
            case Token::kAndAnd:
                return LOGICAL_AND;
                
            case Token::kOrOr:
                return LOGICAL_OR;
                
            case Token::kEq:
            case Token::kPlusEq:
            case Token::kMinusEq:
            case Token::kStarEq:
            case Token::kSlashEq:
            case Token::kPercentEq:
            case Token::kAndEq:
            case Token::kCaretEq:
            case Token::kOrEq:
            case Token::kShlEq:
            case Token::kShrEq:
                return ASSIGNMENT;
                
            case Token::kreturn:
            case Token::kbreak:
                return FLOW_CONTROL;
                
            default:
                return -1; // 非运算符
        }
    }
    int getRightTokenBP(Token token) {
        switch (token) {
            case Token::kEq:
            case Token::kPlusEq:
            case Token::kMinusEq:
            case Token::kStarEq:
            case Token::kSlashEq:
            case Token::kPercentEq:
            case Token::kAndEq:
            case Token::kCaretEq:
            case Token::kOrEq:
            case Token::kShlEq:
            case Token::kShrEq:
                return getLeftTokenBP(token) - 1;
            
            default:
                return getLeftTokenBP(token) + 1;
        }
    }

public:
    Parser(std::vector<std::pair<Token, std::string>> tokens);

    Token peek();
    bool match(Token token);
    void advance();
    std::string getstring();


    // Pratt parser
    std::shared_ptr<Expression> parseExpression();
    std::shared_ptr<Expression> parseExpressionWithBlock();
    std::shared_ptr<Expression> parseExpressionPratt(int minbp = 0);
    std::shared_ptr<Expression> parsePrefixPratt();
    std::shared_ptr<Expression> parseInfixPratt(std::shared_ptr<Expression> lhs, int minbp);

    std::shared_ptr<BlockExpression> parseBlockExpression();
    std::shared_ptr<ConstBlockExpression> parseConstBlockExpression();
    std::shared_ptr<InfiniteLoopExpression> parseInfiniteLoopExpression();
    std::shared_ptr<PredicateLoopExpression> parsePredicateLoopExpression();
    std::shared_ptr<IfExpression> parseIfExpression();
    std::shared_ptr<MatchExpression> parseMatchExpression();

    std::shared_ptr<PathExpression> parsePathExpression();
    std::shared_ptr<GroupedExpression> parseGroupedExpression();
    std::shared_ptr<ArrayExpression> parseArrayExpression();
    std::shared_ptr<UnaryExpression> parseUnaryExpression();
    std::shared_ptr<BorrowExpression> parseBorrowExpression();
    std::shared_ptr<DereferenceExpression> parseDereferenceExpression();
    std::shared_ptr<BreakExpression> parseBreakExpression();
    std::shared_ptr<ReturnExpression> parseReturnExpression();
    // std::shared_ptr<TupleExpression> parseTupleExpression();

    std::shared_ptr<CallExpression> parseCallExpression();
    std::shared_ptr<CallExpression> parseCallExpressionFromInfix(std::shared_ptr<Expression> callee);
    std::shared_ptr<IndexExpression> parseIndexExpression();
    std::shared_ptr<IndexExpression> parseIndexExpressionFromInfix(std::shared_ptr<Expression> lhs);
    std::shared_ptr<TypeCastExpression> parseTypeCastExpression();
    std::shared_ptr<TypeCastExpression> parseTypeCastExpressionFromInfix(std::shared_ptr<Expression> lhs);
    std::shared_ptr<MethodCallExpression> parseMethodCallExpression();
    std::shared_ptr<StructExpression> parseStructExpressionFromInfix(std::shared_ptr<Expression> path);
    std::shared_ptr<StructExprFields> parseStructExprFields();
    std::shared_ptr<StructExprField> parseStructExprField();
    std::shared_ptr<StructBase> parseStructBase();
    
    std::shared_ptr<Crate> parseCrate();
    std::shared_ptr<Item> parseItem();

    std::shared_ptr<Function> parseFunction();
    std::shared_ptr<ConstantItem> parseConstantItem();
    // std::shared_ptr<Module> parseModule();
    std::shared_ptr<StructStruct> parseStruct();
    std::shared_ptr<Enumeration> parseEnumeration();
    std::shared_ptr<InherentImpl> parseInherentImpl();

    std::shared_ptr<FunctionParameters> parseFunctionParameters();
    std::shared_ptr<FunctionReturnType> parseFunctionReturnType();
    std::shared_ptr<FunctionParam> parseFunctionParam();

    std::shared_ptr<StructFields> parseStructFields();
    std::shared_ptr<StructField> parseStructField();

    std::shared_ptr<EnumVariants> parseEnumVariants();
    std::shared_ptr<EnumVariant> parseEnumVariant();

    std::shared_ptr<AssociatedItem> parseAssociatedItem();


    std::shared_ptr<Statement> parseStatement();
    std::shared_ptr<Conditions> parseConditions();

    std::shared_ptr<SimplePath> parseSimplePath();

    std::shared_ptr<ArrayElements> parseArrayElements();
    // std::shared_ptr<TupleElements> parseTupleElements();

    std::shared_ptr<CallParams> parseCallParams();

    std::shared_ptr<Type> parseType();

    std::shared_ptr<LetStatement> parseLetStatement();

    std::shared_ptr<ExpressionStatement> parseExpressionStatement();
    std::shared_ptr<SimplePathSegment> parseSimplePathSegment();
    std::shared_ptr<TypePath> parseTypePath();
    std::shared_ptr<ReferenceType> parseReferenceType();

    std::shared_ptr<Pattern> parsePattern();
    std::shared_ptr<ReferencePattern> parseReferencePattern();
    std::shared_ptr<LiteralPattern> parseLiteralPattern();
    std::shared_ptr<IdentifierPattern> parseIdentifierPattern();
    std::shared_ptr<PathPattern> parsePathPattern();
};
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
                return getLeftTokenBP(token);
        }
    }

public:
    Parser(std::vector<std::pair<Token, std::string>> tokens);

    Token peek();
    bool match(Token token);
    void advance();
    std::string getstring();


    // Pratt parser
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseExpressionPratt(int minbp = 0);
    std::unique_ptr<Expression> parsePrefixPratt();
    std::unique_ptr<Expression> parseInfixPratt(std::unique_ptr<Expression> lhs, int minbp);

    std::unique_ptr<BlockExpression> parseBlockExpression();
    std::unique_ptr<ConstBlockExpression> parseConstBlockExpression();
    std::unique_ptr<InfiniteLoopExpression> parseInfiniteLoopExpression();
    std::unique_ptr<PredicateLoopExpression> parsePredicateLoopExpression();
    std::unique_ptr<IfExpression> parseIfExpression();
    std::unique_ptr<MatchExpression> parseMatchExpression();

    std::unique_ptr<PathExpression> parsePathExpression();
    std::unique_ptr<GroupedExpression> parseGroupedExpression();
    std::unique_ptr<ArrayExpression> parseArrayExpression();
    std::unique_ptr<UnaryExpression> parseUnaryExpression();
    std::unique_ptr<BreakExpression> parseBreakExpression();
    std::unique_ptr<ReturnExpression> parseReturnExpression();
    // std::unique_ptr<TupleExpression> parseTupleExpression();

    std::unique_ptr<CallExpression> parseCallExpression();
    std::unique_ptr<CallExpression> parseCallExpressionFromInfix(std::unique_ptr<Expression> callee);
    std::unique_ptr<IndexExpression> parseIndexExpression();
    std::unique_ptr<TypeCastExpression> parseTypeCastExpression();
    std::unique_ptr<MethodCallExpression> parseMethodCallExpression();
    
    
    std::unique_ptr<Crate> parseCrate();
    std::unique_ptr<Item> parseItem();

    std::unique_ptr<Function> parseFunction();
    std::unique_ptr<ConstantItem> parseConstantItem();
    // std::unique_ptr<Module> parseModule();
    std::unique_ptr<StructStruct> parseStruct();
    std::unique_ptr<Enumeration> parseEnumeration();
    std::unique_ptr<InherentImpl> parseInherentImpl();

    std::unique_ptr<FunctionParameters> parseFunctionParameters();
    std::unique_ptr<FunctionReturnType> parseFunctionReturnType();
    std::unique_ptr<FunctionParam> parseFunctionParam();

    std::unique_ptr<StructFields> parseStructFields();
    std::unique_ptr<StructField> parseStructField();

    std::unique_ptr<EnumVariants> parseEnumVariants();
    std::unique_ptr<EnumVariant> parseEnumVariant();

    std::unique_ptr<AssociatedItem> parseAssociatedItem();


    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Conditions> parseConditions();

    std::unique_ptr<SimplePath> parseSimplePath();

    std::unique_ptr<ArrayElements> parseArrayElements();
    // std::unique_ptr<TupleElements> parseTupleElements();

    std::unique_ptr<CallParams> parseCallParams();

    std::unique_ptr<Type> parseType();

    std::unique_ptr<LetStatement> parseLetStatement();

    std::unique_ptr<ExpressionStatement> parseExpressionStatement();
    std::unique_ptr<SimplePathSegment> parseSimplePathSegment();
    std::unique_ptr<TypePath> parseTypePath();
    std::unique_ptr<ReferenceType> parseReferenceType();

    std::unique_ptr<Pattern> parsePattern();
    std::unique_ptr<ReferencePattern> parseReferencePattern();
    std::unique_ptr<LiteralPattern> parseLiteralPattern();
    std::unique_ptr<IdentifierPattern> parseIdentifierPattern();
    std::unique_ptr<PathPattern> parsePathPattern();
};
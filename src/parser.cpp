#include "parser.hpp"
#include "astnodes.hpp"
#include "lexser.hpp"
#include <memory>
#include <stdexcept>

Parser::Parser(std::vector<std::pair<Token, std::string>> tokens) : tokens(tokens) { }

Token Parser::peek() {
    return tokens[pos].first;
}
bool Parser::match(Token token) {
    return peek() == token;
}
void Parser::advance() {
    ++pos;
}
std::string Parser::getstring() {
    return tokens[pos].second;
}


// Pratt parser
std::unique_ptr<Expression> Parser::parseExpression() {
    auto type = peek();
    if (type == Token::kleftCurly) {
        return parseBlockExpression();
    } else if (type == Token::kconst) {
        return parseConstBlockExpression();
    } else if (type == Token::kloop) {
        return parseInfiniteLoopExpression();
    } else if (type == Token::kwhile) {
        return parsePredicateLoopExpression();
    } else if (type == Token::kif) {
        return parseIfExpression();
    } else if (type == Token::kmatch) {
        return parseMatchExpression();
    } else {
        return parseExpressionPratt(0);
    }
}
std::unique_ptr<Expression> Parser::parseExpressionPratt(int minbp) {
    return parseInfixPratt(parsePrefixPratt(), minbp);
}
std::unique_ptr<Expression> Parser::parsePrefixPratt() {
    auto type = peek();
    advance();
    switch (type) {
        case Token::kINTEGER_LITERAL:
        case Token::kCHAR_LITERAL:
        case Token::kSTRING_LITERAL:
        case Token::kC_STRING_LITERAL:
        case Token::kRAW_STRING_LITERAL:
        case Token::kRAW_C_STRING_LITERAL:
            return std::make_unique<LiteralExpression>(getstring(), type);
        // case Token::kleftParenthe:
        //     return parseGroupedExpression();
        // case Token::kleftParenthe:
        //     return parseTupleExpression(); // need to be tried;
        case Token::kleftSquare:
            return parseArrayExpression();
        case Token::kMinus:
        case Token::kNot:
            return parseUnaryExpression();
        case Token::kbreak:
            return parseBreakExpression();
        case Token::kcontinue:
            return std::make_unique<ContinueExpression>();
        case Token::kreturn:
            return parseReturnExpression();
        case Token::kUnderscore:
            return std::make_unique<UnderscoreExpression>();
        
        case Token::kPathSep:
        case Token::kIDENTIFIER:
            return parsePathExpression();
        
        default:
            throw std::runtime_error("invalid Prefix");
    }
}
std::unique_ptr<Expression> Parser::parseInfixPratt(std::unique_ptr<Expression> lhs, int minbp) {
    while (true) {
        auto type = peek();
        advance();

        int leftbp = getLeftTokenBP(type);
        if (leftbp < minbp) break;
        
        if (type == Token::kleftParenthe) {
            lhs = parseCallExpression();
        } else if (type == Token::kleftSquare) {
            lhs = parseIndexExpression();
        } else if (type == Token::kDot) {
            lhs = parseMethodCallExpression();
        } else if (type == Token::kas) {
            lhs = parseTypeCastExpression();
        }

        int rightbp = getRightTokenBP(type);
        auto rhs = parseExpressionPratt(rightbp);
        lhs = std::make_unique<BinaryExpression>(std::move(lhs), std::move(rhs), type);
    }
    return lhs;
}

std::unique_ptr<BlockExpression> Parser::parseBlockExpression() {

}
std::unique_ptr<ConstBlockExpression> Parser::parseConstBlockExpression() {

}
std::unique_ptr<InfiniteLoopExpression> Parser::parseInfiniteLoopExpression() {

}
std::unique_ptr<PredicateLoopExpression> Parser::parsePredicateLoopExpression() {

}
std::unique_ptr<IfExpression> Parser::parseIfExpression() {

}
std::unique_ptr<MatchExpression> Parser::parseMatchExpression() {

}

std::unique_ptr<PathExpression> Parser::parsePathExpression() {

}
std::unique_ptr<GroupedExpression> Parser::parseGroupedExpression() {

}
std::unique_ptr<ArrayExpression> Parser::parseArrayExpression() {

}
std::unique_ptr<UnaryExpression> Parser::parseUnaryExpression() {

}
std::unique_ptr<BreakExpression> Parser::parseBreakExpression() {

}
std::unique_ptr<ReturnExpression> Parser::parseReturnExpression() {

}
std::unique_ptr<TupleExpression> Parser::parseTupleExpression() {

}

std::unique_ptr<CallExpression> Parser::parseCallExpression() {

}
std::unique_ptr<IndexExpression> Parser::parseIndexExpression() {

}
std::unique_ptr<TypeCastExpression> Parser::parseTypeCastExpression() {

}
std::unique_ptr<MethodCallExpression> Parser::parseMethodCallExpression() {

}

#include "parser.hpp"
#include "astnodes.hpp"
#include "lexser.hpp"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

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
        case Token::kleftParenthe:
            return parseGroupedExpression();

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
    if (match(Token::krightCurly)) {
        advance();
        return std::make_unique<BlockExpression>(nullptr);
    }
    auto statement = parseStatement();
    advance();
    return std::make_unique<BlockExpression>(std::move(statement));
}
std::unique_ptr<ConstBlockExpression> Parser::parseConstBlockExpression() {
    if (!match(Token::kconst)) {
        return nullptr;
    }
    advance();
    return std::make_unique<ConstBlockExpression>(std::move(parseBlockExpression()));
}
std::unique_ptr<InfiniteLoopExpression> Parser::parseInfiniteLoopExpression() {
    if (!match(Token::kloop)) {
        return nullptr;
    }
    advance();
    return std::make_unique<InfiniteLoopExpression>(std::move(parseBlockExpression()));
}
std::unique_ptr<PredicateLoopExpression> Parser::parsePredicateLoopExpression() {
    if (!match(Token::kwhile)) {
        return nullptr;
    }
    advance();
    auto conditions = parseConditions();
    auto expression = parseBlockExpression();
    return std::make_unique<PredicateLoopExpression>(std::move(conditions), std::move(expression));
}
std::unique_ptr<IfExpression> Parser::parseIfExpression() {
    if (!match(Token::kif)) {
        return nullptr;
    }
    advance();
    auto conditions = parseConditions();
    auto blockexpression = parseBlockExpression();
    std::unique_ptr<Expression> elseexpression = nullptr;
    if (match(Token::kelse)) {
        advance();
        if (match(Token::kif)) {
            elseexpression = parseIfExpression();
        } else {
            elseexpression = parseBlockExpression();
        }
    }
    return std::make_unique<IfExpression>(std::move(conditions), std::move(blockexpression), std::move(elseexpression));
}
std::unique_ptr<MatchExpression> Parser::parseMatchExpression() {
    return nullptr;
}

std::unique_ptr<PathExpression> Parser::parsePathExpression() {
    return std::make_unique<PathExpression>(std::move(parseSimplePath()));
}
std::unique_ptr<GroupedExpression> Parser::parseGroupedExpression() {
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    auto expression = parseExpression();
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    return std::make_unique<GroupedExpression>(std::move(expression));
}
std::unique_ptr<ArrayExpression> Parser::parseArrayExpression() {
    if (!match(Token::kleftSquare)) {
        return nullptr;
    }
    advance();
    auto arrayelements = parseArrayElements();
    if (!match(Token::kRightSquare)) {
        return nullptr;
    }
    advance();
    return std::make_unique<ArrayExpression>(std::move(arrayelements));
}
std::unique_ptr<UnaryExpression> Parser::parseUnaryExpression() {
    auto unarytype = peek();
    advance();
    auto expression = parseExpression();
    return std::make_unique<UnaryExpression>(std::move(expression), unarytype);
}
std::unique_ptr<BreakExpression> Parser::parseBreakExpression() {
    if (!match(Token::kbreak)) {
        return nullptr;
    }
    advance();
    auto expression = parseExpression();
    return std::make_unique<BreakExpression>(std::move(expression));
}
std::unique_ptr<ReturnExpression> Parser::parseReturnExpression() {
    if (!match(Token::kbreak)) {
        return nullptr;
    }
    advance();
    auto expression = parseExpression();
    return std::make_unique<ReturnExpression>(std::move(expression));
}
// std::unique_ptr<TupleExpression> Parser::parseTupleExpression() {
//     if (!match(Token::kleftParenthe)) {
//         return nullptr;
//     }
//     advance();
//     auto tupleelements = parseTupleElements();
//     if (!match(Token::krightParenthe)) {
//         return nullptr;
//     }
//     advance();
//     return std::make_unique<TupleExpression>(std::move(tupleelements));
// }

std::unique_ptr<CallExpression> Parser::parseCallExpression() {
    auto expression = parseExpression();
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    auto callparams = parseCallParams();
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    return std::make_unique<CallExpression>(std::move(expression), std::move(callparams));
}
std::unique_ptr<IndexExpression> Parser::parseIndexExpression() {
    auto expressionout = parseExpression();
    if (!match(Token::kleftSquare)) {
        return nullptr;
    }
    advance();
    auto expressionin = parseExpression();
    if (!match(Token::kRightSquare)) {
        return nullptr;
    }
    advance();
    return std::make_unique<IndexExpression>(std::move(expressionout), std::move(expressionin));
}
std::unique_ptr<TypeCastExpression> Parser::parseTypeCastExpression() {
    auto expression = parseExpression();
    if (!match(Token::kas)) {
        return nullptr;
    }
    advance();
    auto typenobounds = parseType();
    return std::make_unique<TypeCastExpression>(std::move(expression), std::move(typenobounds));
}
std::unique_ptr<MethodCallExpression> Parser::parseMethodCallExpression() {
    // auto expression = parseExpression();
    // if (!match(Token::kDot)) {
    //     return nullptr;
    // }
    // advance();
    // auto pathexprsegment = 
    return nullptr;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (match(Token::kSemi)) {
        advance();
        return nullptr;
    }
    if (match(Token::klet)) {
        return std::make_unique<Statement>(std::move(parseLetStatement()));
    }
    size_t tmp = pos;
    auto item = parseItem();
    if (item != nullptr) {
        return std::make_unique<Statement>(std::move(item));
    }
    auto expressionstatement = parseExpressionStatement();
    if (expressionstatement != nullptr) {
        return std::make_unique<Statement>(std::move(expressionstatement));
    }
    return nullptr;
}
std::unique_ptr<Conditions> Parser::parseConditions() {
    auto expression = parseExpression();
    auto p = expression.get();
    if (!dynamic_cast<StructExpression*>(p)) {
        return std::make_unique<Conditions>(std::move(expression));
    }
    return nullptr;
}

std::unique_ptr<SimplePath> Parser::parseSimplePath() {
    if (match(Token::kPathSep)) {
        advance();
    }
    std::vector<std::unique_ptr<SimplePathSegment>> vec;
    vec.push_back(std::move(parseSimplePathSegment()));
    while (match(Token::kPathSep)) {
        advance();
        vec.push_back(std::move(parseSimplePathSegment()));
    }
    return std::make_unique<SimplePath>(std::move(vec));
}

std::unique_ptr<ArrayElements> Parser::parseArrayElements() {
    auto expression = parseExpression();
    std::vector<std::unique_ptr<Expression>> vec;
    vec.push_back(std::move(expression));
    if (match(Token::kSemi)) {
        advance();
        auto expression2 = parseExpression();
        vec.push_back(std::move(expression2));
        return std::make_unique<ArrayElements>(std::move(vec), true);
    }
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    while (match(Token::kComma)) {
        advance();
        auto expression2 = parseExpression();
        vec.push_back(std::move(expression2));
    }
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    return std::make_unique<ArrayElements>(std::move(vec), false);
}
// std::unique_ptr<TupleElements> Parser::parseTupleElements() {
    
// }

std::unique_ptr<CallParams> Parser::parseCallParams() {
    auto expression = parseExpression();
    std::vector<std::unique_ptr<Expression>> vec;
    vec.push_back(std::move(expression));
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    while (match(Token::kComma)) {
        advance();
        auto expression2 = parseExpression();
        vec.push_back(std::move(expression2));
    }
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    if (match(Token::kComma)) {
        advance();
    }
    return std::make_unique<CallParams>(std::move(vec));
}

std::unique_ptr<Type> Parser::parseType() {
    auto type = peek();
    switch (type) {
        case Token::kIDENTIFIER:
        case Token::kSelf:
        case Token::kself:
            return parseTypePath();
        
        case Token::kleftSquare: {
            advance();
            auto tp = parseType();
            if (match(Token::kSemi)) {
                auto expression = parseExpression();
                return std::make_unique<ArrayType>(std::move(tp), std::move(expression));
            } else if (match(Token::kRightSquare)) {
                advance();
                return std::make_unique<SliceType>(std::move(tp));
            } else {
                return nullptr;
            }
        }
        
        case Token::kAnd:
            return parseReferenceType();
        
        case Token::kUnderscore:
            return std::make_unique<InferredType>();
        
        default:
            return nullptr;
    }
}



std::unique_ptr<Crate> Parser::parseCrate() {
    std::vector<std::unique_ptr<Item>> items;
    while (pos < tokens.size()) {
        auto item = parseItem();
        if (item == nullptr) {
            break;
        }
        items.push_back(std::move(item));
    }
    return std::make_unique<Crate>(std::move(items));
}
std::unique_ptr<Item> Parser::parseItem() {
    if (match(Token::kstruct)) {
        return std::make_unique<Item>(std::move(parseStruct()));
    } else if (match(Token::kenum)) {
        return std::make_unique<Item>(std::move(parseEnumeration()));
    } else if (match(Token::kimpl)) {
        return std::make_unique<Item>(std::move(parseInherentImpl()));
    } else if (match(Token::kfn)) {
        return std::make_unique<Item>(std::move(parseFunction()));
    } else if (match(Token::kconst)) {
        if (tokens[pos + 1].first == Token::kfn) {
            return std::make_unique<Item>(std::move(parseFunction()));
        } else {
            return std::make_unique<Item>(std::move(parseConstantItem()));
        }
    }
    return nullptr;
}

std::unique_ptr<Function> Parser::parseFunction() {
    bool isconst = false;
    if (match(Token::kconst)) {
        advance();
        isconst = true;
    }
    if (!match(Token::kfn)) {
        return nullptr;
    }
    advance();
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    std::unique_ptr<FunctionParameters> parameters = nullptr;
    if (!match(Token::krightParenthe)) {
        parameters = std::move(parseFunctionParameters());
    }
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    std::unique_ptr<FunctionReturnType> type(std::move(parseFunctionReturnType()));
    std::unique_ptr<BlockExpression> expression = nullptr;
    if (!match(Token::kSemi)) {
        expression = std::move(parseBlockExpression());
    } else {
        advance();
    }
    return std::make_unique<Function>(isconst, std::move(identifier), std::move(parameters), std::move(type), std::move(expression));
}
std::unique_ptr<ConstantItem> Parser::parseConstantItem() {
    if (!match(Token::kconst)) {
        return nullptr;
    }
    advance();
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    if (!match(Token::kColon)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    std::unique_ptr<Expression> expression = nullptr;
    if (match(Token::kEq)) {
        advance();
        expression = parseExpression();
        if (expression == nullptr) {
            return nullptr;
        }
    }
    if (!match(Token::kSemi)) {
        return nullptr;
    }
    advance();
    return std::make_unique<ConstantItem>(identifier, std::move(type), std::move(expression));
}
// std::unique_ptr<Module> Parser::parseModule() {

// }
std::unique_ptr<StructStruct> Parser::parseStruct() {
    if (!match(Token::kstruct)) {
        return nullptr;
    }
    advance();
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    if (match(Token::kleftCurly)) {
        advance();
        auto structfields = parseStructFields();
        if (!match(Token::krightCurly)) {
            return nullptr;
        }
        advance();
        return std::make_unique<StructStruct>(identifier, std::move(structfields), false);
    } else if (match(Token::kSemi)) {
        return std::make_unique<StructStruct>(identifier, nullptr, true);
    } else {
        return nullptr;
    }
}
std::unique_ptr<Enumeration> Parser::parseEnumeration() {
    if (!match(Token::kenum)) {
        return nullptr;
    }
    advance();
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    if (!match(Token::kleftCurly)) {
        return nullptr;
    }
    advance();
    auto enumvariants = parseEnumVariants();
    if (!match(Token::krightCurly)) {
        return nullptr;
    }
    advance();
    return std::make_unique<Enumeration>(identifier, std::move(enumvariants));
}
std::unique_ptr<InherentImpl> Parser::parseInherentImpl() {
    if (!match(Token::kimpl)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    if (!match(Token::kleftCurly)) {
        return nullptr;
    }
    advance();
    std::vector<std::unique_ptr<AssociatedItem>> items;
    while (!match(Token::krightCurly)) {
        auto item = parseAssociatedItem();
        if (item == nullptr) {
            return nullptr;
        }
        items.push_back(std::move(item));
    }
    advance();
    return std::make_unique<InherentImpl>(std::move(type), std::move(items));
}

std::unique_ptr<FunctionParameters> Parser::parseFunctionParameters() {
    std::vector<std::unique_ptr<FunctionParam>> vec;
    auto param = parseFunctionParam();
    if (param == nullptr) {
        return nullptr;
    }
    vec.push_back(std::move(param));
    while (match(Token::kComma)) {
        advance();
        auto param = parseFunctionParam();
        if (param == nullptr) {
            return nullptr;
        }
        vec.push_back(std::move(param));
    }
    if (match(Token::kComma)) {
        advance();
    }
    return std::make_unique<FunctionParameters>(std::move(vec));
}
std::unique_ptr<FunctionReturnType> Parser::parseFunctionReturnType() {
    if (!match(Token::kRArrow)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    return std::make_unique<FunctionReturnType>(std::move(type));
}
std::unique_ptr<FunctionParam> Parser::parseFunctionParam() {
    auto pattern = parsePattern();
    if (pattern == nullptr) {
        return nullptr;
    }
    if (!match(Token::kColon)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    if (type == nullptr) {
        return nullptr;
    }
    return std::make_unique<FunctionParam>(std::move(pattern), std::move(type));
}

std::unique_ptr<StructFields> Parser::parseStructFields() {
    std::vector<std::unique_ptr<StructField>> vec;
    auto field = parseStructField();
    if (field == nullptr) {
        return nullptr;
    }
    vec.push_back(std::move(field));
    while (match(Token::kComma)) {
        advance();
        auto field = parseStructField();
        if (field == nullptr) {
            return nullptr;
        }
        vec.push_back(std::move(field));
    }
    if (match(Token::kComma)) {
        advance();
    }
    return std::make_unique<StructFields>(std::move(vec));
}
std::unique_ptr<StructField> Parser::parseStructField() {
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    if (!match(Token::kColon)) {
        return nullptr;
    }
    auto type = parseType();
    if (!type) {
        return nullptr;
    }
    return std::make_unique<StructField>(identifier, std::move(type));
}

std::unique_ptr<EnumVariants> Parser::parseEnumVariants() {
    std::vector<std::unique_ptr<EnumVariant>> vec;
    auto variant = parseEnumVariant();
    if (variant == nullptr) {
        return nullptr;
    }
    vec.push_back(std::move(variant));
    while (match(Token::kComma)) {
        advance();
        auto variant = parseEnumVariant();
        if (variant == nullptr) {
            return nullptr;
        }
        vec.push_back(std::move(variant));
    }
    if (match(Token::kComma)) {
        advance();
    }
    return std::make_unique<EnumVariants>(std::move(vec));
}
std::unique_ptr<EnumVariant> Parser::parseEnumVariant() {
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    return std::make_unique<EnumVariant>(identifier);
}

std::unique_ptr<AssociatedItem> Parser::parseAssociatedItem() {
    if (match(Token::kfn)) {
        return std::make_unique<AssociatedItem>(std::move(parseFunction()));
    } else if (match(Token::kconst)) {
        if (tokens[pos + 1].first == Token::kfn) {
            return std::make_unique<AssociatedItem>(std::move(parseFunction()));
        } else {
            return std::make_unique<AssociatedItem>(std::move(parseConstantItem()));
        }
    }
    return nullptr;
}

std::unique_ptr<LetStatement> Parser::parseLetStatement() {
    if (!match(Token::klet)) {
        return nullptr;
    }
    advance();
    auto pattern = parsePattern();
    if (!match(Token::kColon)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    std::unique_ptr<Expression> expression = nullptr;
    if (match(Token::kEq)) {
        advance();
        expression = parseExpression();
        if (expression == nullptr) {
            return nullptr;
        }
    }
    if (!match(Token::kSemi)) {
        return nullptr;
    }
    advance();
    return std::make_unique<LetStatement>(std::move(pattern), std::move(type), std::move(expression));
}

std::unique_ptr<ExpressionStatement> Parser::parseExpressionStatement() {
    auto expression = parseExpression();
    if (match(Token::kSemi)) {
        return std::make_unique<ExpressionStatement>(std::move(expression), true);
    }
    auto ptr = expression.get();
    if (dynamic_cast<BlockExpression*>(ptr)
     || dynamic_cast<ConstBlockExpression*>(ptr)
     || dynamic_cast<InfiniteLoopExpression*>(ptr)
     || dynamic_cast<PredicateLoopExpression*>(ptr)
     || dynamic_cast<IfExpression*>(ptr)
     || dynamic_cast<MatchExpression*>(ptr)) {
        return std::make_unique<ExpressionStatement>(std::move(expression), false);
    }
    return nullptr;
}
std::unique_ptr<SimplePathSegment> Parser::parseSimplePathSegment() {
    if (match(Token::kIDENTIFIER)) {
        auto str = getstring();
        advance();
        return std::make_unique<SimplePathSegment>(str, false, false);
    } else if (match(Token::kSelf)) {
        advance();
        return std::make_unique<SimplePathSegment>(std::string(), false, true);
    } else if (match(Token::kself)) {
        advance();
        return std::make_unique<SimplePathSegment>(std::string(), true, false);
    }
}
std::unique_ptr<TypePath> Parser::parseTypePath() {
    auto simplepath = parseSimplePathSegment();
    return std::make_unique<TypePath>(std::move(simplepath));
}
std::unique_ptr<ReferenceType> Parser::parseReferenceType() {
    if (!match(Token::kAnd)) {
        return nullptr;
    }
    advance();
    bool ismut = false;
    if (match(Token::kmut)) {
        ismut = true;
        advance();
    }
    auto type = parseType();
    return std::make_unique<ReferenceType>(std::move(type), ismut);
}

std::unique_ptr<Pattern> Parser::parsePattern() {
    if (match(Token::kUnderscore)) {
        advance();
        return std::make_unique<WildcardPattern>();
    } else if (match(Token::kAnd) || match(Token::kAndAnd)) {
        return parseReferencePattern();
    } else if (match(Token::kref) || match(Token::kmut) || match(Token::kIDENTIFIER)) {
        return parseIdentifierPattern();
    } else if (match(Token::kMinus)) {
        return parseLiteralPattern();
    } else {
        auto tmp = pos;
        auto p = parseLiteralPattern();
        if (p != nullptr) return p;
        pos = tmp;
        auto q = parsePathPattern();
        if (q != nullptr) return q;
        return nullptr;
    }
}

std::unique_ptr<ReferencePattern> Parser::parseReferencePattern() {
    bool singleordouble = false;
    if (match(Token::kAnd)) {
        singleordouble = false;
    } else if (match(Token::kAndAnd)) {
        singleordouble = true;
    } else {
        return nullptr;
    }
    bool ismut = false;
    if (match(Token::kmut)) {
        ismut = true;
        advance();
    }
    auto pattern = parsePattern();
    return std::make_unique<ReferencePattern>(singleordouble, ismut, std::move(pattern));
}
std::unique_ptr<LiteralPattern> Parser::parseLiteralPattern() {
    bool neg = false;
    if (match(Token::kMinus)) {
        neg = true;
        advance();
    }
    auto expression = parseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
    auto p = dynamic_cast<LiteralExpression*>(expression.get());
    if (p == nullptr) {
        return nullptr;
    }
    return std::make_unique<LiteralPattern>(neg, std::move(expression));
}
std::unique_ptr<IdentifierPattern> Parser::parseIdentifierPattern() {
    bool isref = false;
    if (match(Token::kref)) {
        isref = true;
        advance();
    }
    bool ismut = false;
    if (match(Token::kmut)) {
        ismut = true;
        advance();
    }
    auto identifier = getstring();
    advance();
    if (match(Token::kAt)) {
        advance();
        auto pattern = parsePathPattern();
        if (pattern == nullptr) {
            return nullptr;
        }
        return std::make_unique<IdentifierPattern>(isref, ismut, identifier, std::move(pattern));
    }
    return std::make_unique<IdentifierPattern>(isref, ismut, identifier, nullptr);
}
std::unique_ptr<PathPattern> Parser::parsePathPattern() {
    auto expression = parseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
    auto p = dynamic_cast<PathExpression*>(expression.get());
    if (p == nullptr) {
        return nullptr;
    }
    return std::make_unique<PathPattern>(std::move(expression));
}
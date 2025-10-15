#include "parser.hpp"
#include "astnodes.hpp"
#include "lexer.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

Parser::Parser(std::vector<std::pair<Token, std::string>> tokens) : tokens(tokens) { }

Token Parser::peek() {
    if (pos >= tokens.size()) return Token::kEnd;
    while (pos < tokens.size() && tokens[pos].first == Token::kCOMMENT) ++pos;
    return tokens[pos].first;
}
bool Parser::match(Token token) {
    return peek() == token;
}
void Parser::advance() {
    ++pos;
}
std::string Parser::getstring() {
    if (pos >= tokens.size()) return "";
    return tokens[pos].second;
}


// Pratt parser
std::shared_ptr<Expression> Parser::parseExpression() {
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
    } 
    // else if (type == Token::kmatch) {
    //     return parseMatchExpression();
    // } 
    else {
        // std::cerr << "expression pratt\n";
        return parseExpressionPratt(0);
    }
}
std::shared_ptr<Expression> Parser::parseExpressionPratt(int minbp) {
    auto lhs = parsePrefixPratt();
    if (lhs == nullptr) {
        return nullptr;
    }
    return parseInfixPratt(std::move(lhs), minbp);
}
std::shared_ptr<Expression> Parser::parsePrefixPratt() {
    auto type = peek();
    // std::cerr << "prefix: " << to_string(type) << " " << pos << "\n";
    // advance();
    switch (type) {
        case Token::kINTEGER_LITERAL:
        case Token::kCHAR_LITERAL:
        case Token::kSTRING_LITERAL:
        case Token::kC_STRING_LITERAL:
        case Token::kRAW_STRING_LITERAL:
        case Token::kRAW_C_STRING_LITERAL:
        case Token::ktrue:
        case Token::kfalse: {
            auto string = getstring();
            advance();
            return std::make_shared<LiteralExpression>(string, type);
        }
            
        case Token::kleftParenthe:
            return parseGroupedExpression();

        case Token::kleftSquare:
            return parseArrayExpression();
        case Token::kMinus:
        case Token::kNot:
            return parseUnaryExpression();
        case Token::kbreak:
            return parseBreakExpression();
        case Token::kcontinue: {
            advance();
            return std::make_shared<ContinueExpression>();
        }
        case Token::kreturn:
            return parseReturnExpression();
        case Token::kUnderscore:  {
            advance();
            return std::make_shared<UnderscoreExpression>();
        }
        
        case Token::kPathSep:
        case Token::kIDENTIFIER: 
            return parsePathExpression();            

        case Token::kEnd: {
            advance();
            return nullptr;
        }
        
        default:
            return nullptr;
    }
}
std::shared_ptr<Expression> Parser::parseInfixPratt(std::shared_ptr<Expression> lhs, int minbp) {
    while (true) {
        auto type = peek();
        // std::cerr << "INFIX: " << to_string(type) << " at position " << pos << " minbp=" << minbp << std::endl;
        if (type == Token::kEnd || type == Token::krightParenthe || type == Token::krightCurly || type == Token::kRightSquare) {
            // std::cerr << "INFIX: Breaking due to terminator: " << to_string(type) << std::endl;
            break;
        }

        int leftbp = getLeftTokenBP(type);
        // std::cerr << "INFIX: leftbp for " << to_string(type) << " is " << leftbp << std::endl;
        if (leftbp < minbp) {
            // std::cerr << "INFIX: Breaking due to precedence: leftbp " << leftbp << " < minbp " << minbp << std::endl;
            break;
        }
        advance();

        if (type == Token::kleftParenthe) {
            // std::cerr << "INFIX: Parsing call expression" << std::endl;
            lhs = parseCallExpressionFromInfix(std::move(lhs));
        } else if (type == Token::kleftSquare) {
            // std::cerr << "INFIX: Parsing index expression" << std::endl;
            lhs = parseIndexExpressionFromInfix(std::move(lhs));
        }
        // else if (type == Token::kDot) {
        //     lhs = parseMethodCallExpression();
        // }
        else if (type == Token::kas) {
            lhs = parseTypeCastExpression();
        } else {
            // std::cerr << "INFIX: Parsing binary expression with rightbp=" << getRightTokenBP(type) << std::endl;
            int rightbp = getRightTokenBP(type);
            auto rhs = parseExpressionPratt(rightbp);
            switch (type) {
                case Token::kEq: {
                    lhs = std::make_shared<AssignmentExpression>(std::move(lhs), std::move(rhs));
                    break;
                }
                case Token::kPlusEq:
                case Token::kMinusEq:
                case Token::kStarEq:
                case Token::kSlashEq:
                case Token::kPercentEq:
                case Token::kCaretEq:
                case Token::kAndEq:
                case Token::kOrEq:
                case Token::kShlEq:
                case Token::kShrEq: {
                    lhs = std::make_shared<CompoundAssignmentExpression>(std::move(lhs), std::move(rhs), type);
                    break;
                }

                default: {
                    lhs = std::make_shared<BinaryExpression>(std::move(lhs), std::move(rhs), type);
                }
            }
            
        }
    }
    return lhs;
}

std::shared_ptr<BlockExpression> Parser::parseBlockExpression() {
    if (match(Token::kleftCurly)) {
        advance();
    }
    std::vector<std::shared_ptr<Statement>> statements;
    while (!match(Token::krightCurly)) {
        auto tmp = pos;
        auto statement = parseStatement();
        if (statement != nullptr) {
            statements.push_back(std::move(statement));
            continue;
        } 
        pos = tmp;
        auto expression = parseExpression();
        if (dynamic_cast<IfExpression*>(expression.get()) != nullptr
         && dynamic_cast<BlockExpression*>(expression.get()) != nullptr
         && dynamic_cast<InfiniteLoopExpression*>(expression.get()) != nullptr
         && dynamic_cast<PredicateLoopExpression*>(expression.get()) != nullptr) {
            return std::make_shared<BlockExpression>(std::move(statements), std::move(expression));
        }
    }
    advance();
    return std::make_shared<BlockExpression>(std::move(statements), nullptr);
}
std::shared_ptr<ConstBlockExpression> Parser::parseConstBlockExpression() {
    if (!match(Token::kconst)) {
        return nullptr;
    }
    advance();
    return std::make_shared<ConstBlockExpression>(std::move(parseBlockExpression()));
}
std::shared_ptr<InfiniteLoopExpression> Parser::parseInfiniteLoopExpression() {
    if (!match(Token::kloop)) {
        return nullptr;
    }
    advance();
    return std::make_shared<InfiniteLoopExpression>(std::move(parseBlockExpression()));
}
std::shared_ptr<PredicateLoopExpression> Parser::parsePredicateLoopExpression() {
    if (!match(Token::kwhile)) {
        return nullptr;
    }
    advance();
    auto conditions = parseConditions();
    auto expression = parseBlockExpression();
    return std::make_shared<PredicateLoopExpression>(std::move(conditions), std::move(expression));
}
std::shared_ptr<IfExpression> Parser::parseIfExpression() {
    if (!match(Token::kif)) {
        return nullptr;
    }
    advance();
    auto conditions = parseConditions();
    auto blockexpression = parseBlockExpression();
    std::shared_ptr<Expression> elseexpression = nullptr;
    if (match(Token::kelse)) {
        advance();
        if (match(Token::kif)) {
            elseexpression = parseIfExpression();
        } else {
            elseexpression = parseBlockExpression();
        }
    }
    return std::make_shared<IfExpression>(std::move(conditions), std::move(blockexpression), std::move(elseexpression));
}
// std::unique_ptr<MatchExpression> Parser::parseMatchExpression() {
//     return nullptr;
// }

std::shared_ptr<PathExpression> Parser::parsePathExpression() {
    return std::make_shared<PathExpression>(std::move(parseSimplePath()));
}
std::shared_ptr<GroupedExpression> Parser::parseGroupedExpression() {
    if (match(Token::kleftParenthe)) {
        advance();
    }
    auto expression = parseExpression();
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    return std::make_shared<GroupedExpression>(std::move(expression));
}
std::shared_ptr<ArrayExpression> Parser::parseArrayExpression() {
    // std::cerr << "ARRAY: Starting array expression at position " << pos << std::endl;
    if (match(Token::kleftSquare)) {
        // std::cerr << "ARRAY: Consuming leftSquare at position " << pos << std::endl;
        advance();
    }
    auto arrayelements = parseArrayElements();
    // std::cerr << "ARRAY: Looking for RightSquare at position " << pos << ", current token: " << to_string(peek()) << std::endl;
    if (!match(Token::kRightSquare)) {
        // std::cerr << "ARRAY: ERROR - Expected RightSquare but found: " << to_string(peek()) << std::endl;
        return nullptr;
    }
    // std::cerr << "ARRAY: Consuming RightSquare at position " << pos << std::endl;
    advance();
    // std::cerr << "ARRAY: Successfully parsed array expression" << std::endl;
    return std::make_shared<ArrayExpression>(std::move(arrayelements));
}
std::shared_ptr<UnaryExpression> Parser::parseUnaryExpression() {
    auto unarytype = peek();
    advance();
    auto expression = parseExpression();
    return std::make_shared<UnaryExpression>(std::move(expression), unarytype);
}
std::shared_ptr<BreakExpression> Parser::parseBreakExpression() {
    if (match(Token::kbreak)) {
        advance();
    }
    auto expression = parseExpression();
    return std::make_shared<BreakExpression>(std::move(expression));
}
std::shared_ptr<ReturnExpression> Parser::parseReturnExpression() {
    if (match(Token::kreturn)) {
        advance();
    }
    auto expression = parseExpression();
    return std::make_shared<ReturnExpression>(std::move(expression));
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

std::shared_ptr<CallExpression> Parser::parseCallExpression() {
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
    return std::make_shared<CallExpression>(std::move(expression), std::move(callparams));
}

// 新增：用于中缀解析的函数调用解析，接收已解析的函数表达式
std::shared_ptr<CallExpression> Parser::parseCallExpressionFromInfix(std::shared_ptr<Expression> callee) {
    // 此时左括号已经被 advance() 消费了
    auto callparams = parseCallParams();
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    return std::make_shared<CallExpression>(std::move(callee), std::move(callparams));
}
std::shared_ptr<IndexExpression> Parser::parseIndexExpression() {
    // std::cerr << "parseIndexExpression called, pos=" << pos << std::endl;
    auto expressionout = parseExpression();
    if (!match(Token::kleftSquare)) {
        // std::cerr << "parseIndexExpression: expected leftSquare" << std::endl;
        return nullptr;
    }
    advance();
    auto expressionin = parseExpression();
    if (!match(Token::kRightSquare)) {
        // std::cerr << "parseIndexExpression: expected RightSquare" << std::endl;
        return nullptr;
    }
    advance();
    // std::cerr << "parseIndexExpression: success" << std::endl;
    return std::make_shared<IndexExpression>(std::move(expressionout), std::move(expressionin));
}

// 新增：用于中缀解析的索引表达式解析，接收已解析的左侧表达式
std::shared_ptr<IndexExpression> Parser::parseIndexExpressionFromInfix(std::shared_ptr<Expression> lhs) {
    // std::cerr << "parseIndexExpressionFromInfix called, pos=" << pos << std::endl;
    // 此时左方括号已经被 advance() 消费了
    auto expressionin = parseExpression();
    if (!match(Token::kRightSquare)) {
        // std::cerr << "parseIndexExpressionFromInfix: expected RightSquare" << std::endl;
        return nullptr;
    }
    advance();
    // std::cerr << "parseIndexExpressionFromInfix: success" << std::endl;
    return std::make_shared<IndexExpression>(std::move(lhs), std::move(expressionin));
}
std::shared_ptr<TypeCastExpression> Parser::parseTypeCastExpression() {
    auto expression = parseExpression();
    if (!match(Token::kas)) {
        return nullptr;
    }
    advance();
    auto typenobounds = parseType();
    return std::make_shared<TypeCastExpression>(std::move(expression), std::move(typenobounds));
}
// std::unique_ptr<MethodCallExpression> Parser::parseMethodCallExpression() {
//     // auto expression = parseExpression();
//     // if (!match(Token::kDot)) {
//     //     return nullptr;
//     // }
//     // advance();
//     // auto pathexprsegment = 
//     return nullptr;
// }

std::shared_ptr<Statement> Parser::parseStatement() {
    if (match(Token::kSemi)) {
        advance();
        return nullptr;
    }
    if (match(Token::klet)) {
        return std::make_shared<Statement>(std::move(parseLetStatement()));
    }
    size_t tmp = pos;
    auto item = parseItem();
    if (item != nullptr) {
        return std::make_shared<Statement>(std::move(item));
    }
    pos = tmp;
    auto expressionstatement = parseExpressionStatement();
    if (expressionstatement != nullptr) {
        return std::make_shared<Statement>(std::move(expressionstatement));
    }
    return nullptr;
}
std::shared_ptr<Conditions> Parser::parseConditions() {
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    auto expression = parseExpression();
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    auto p = expression.get();
    if (dynamic_cast<StructExpression*>(p) == nullptr) {
        return std::make_shared<Conditions>(std::move(expression));
    }
    return nullptr;
}

std::shared_ptr<SimplePath> Parser::parseSimplePath() {
    if (match(Token::kPathSep)) {
        advance();
    }
    std::vector<std::shared_ptr<SimplePathSegment>> vec;
    vec.push_back(std::move(parseSimplePathSegment()));
    while (match(Token::kPathSep)) {
        advance();
        vec.push_back(std::move(parseSimplePathSegment()));
    }
    return std::make_shared<SimplePath>(std::move(vec));
}

std::shared_ptr<ArrayElements> Parser::parseArrayElements() {
    // std::cerr << "ARRAY_ELEMENTS: Starting at position " << pos << std::endl;
    auto expression = parseExpression();
    std::vector<std::shared_ptr<Expression>> vec;
    vec.push_back(std::move(expression));
    if (match(Token::kSemi)) {
        // std::cerr << "ARRAY_ELEMENTS: Found semi-colon, parsing repeated array" << std::endl;
        advance();
        auto expression2 = parseExpression();
        vec.push_back(std::move(expression2));
        return std::make_shared<ArrayElements>(std::move(vec), true);
    }
    while (match(Token::kComma)) {
        // std::cerr << "ARRAY_ELEMENTS: Found comma at position " << pos << ", parsing next element" << std::endl;
        advance();
        auto expression2 = parseExpression();
        if (expression2 != nullptr) {
            vec.push_back(std::move(expression2));
        } else {
            break;
        }
    }
    // std::cerr << "ARRAY_ELEMENTS: Finished parsing " << vec.size() << " elements" << std::endl;
    return std::make_shared<ArrayElements>(std::move(vec), false);
}
// std::unique_ptr<TupleElements> Parser::parseTupleElements() {
    
// }

std::shared_ptr<CallParams> Parser::parseCallParams() {
    std::vector<std::shared_ptr<Expression>> vec;
    
    // 检查是否为空的参数列表
    if (match(Token::krightParenthe)) {
        return std::make_shared<CallParams>(std::move(vec));
    }
    
    // 解析第一个参数
    auto expression = parseExpression();
    if (expression != nullptr) {
        vec.push_back(std::move(expression));
    }
    
    // 解析后续参数
    while (match(Token::kComma)) {
        advance();
        // 检查是否是末尾的逗号（如 exit(0,)）
        if (match(Token::krightParenthe)) {
            break;
        }
        auto nextExpression = parseExpression();
        if (nextExpression != nullptr) {
            vec.push_back(std::move(nextExpression));
        }
    }
    
    return std::make_shared<CallParams>(std::move(vec));
}

std::shared_ptr<Type> Parser::parseType() {
    auto type = peek();
    switch (type) {
        case Token::kIDENTIFIER:
        case Token::kSelf:
        case Token::kself:
            return parseTypePath();
        
        case Token::kleftSquare: {
            advance();
            auto tp = parseType();
            if (!match(Token::kSemi)) {
                return nullptr;
            }
            advance();
            auto expression = parseExpression();
            if (!match(Token::kRightSquare)) {
                return nullptr;
            }
            advance();
            return std::make_shared<ArrayType>(std::move(tp), std::move(expression));
        }
        
        case Token::kAnd:
            return parseReferenceType();
        
        case Token::kUnderscore:
            return std::make_shared<InferredType>();
        
        default:
            return nullptr;
    }
}



std::shared_ptr<Crate> Parser::parseCrate() {
    std::vector<std::shared_ptr<Item>> items;
    while (pos < tokens.size()) {
        auto item = parseItem();
        if (item == nullptr) {
            break;
        }
        items.push_back(std::move(item));
    }
    return std::make_shared<Crate>(std::move(items));
}
std::shared_ptr<Item> Parser::parseItem() {
    if (match(Token::kstruct)) {
        return std::make_shared<Item>(std::move(parseStruct()));
    } else if (match(Token::kenum)) {
        return std::make_shared<Item>(std::move(parseEnumeration()));
    } else if (match(Token::kimpl)) {
        return std::make_shared<Item>(std::move(parseInherentImpl()));
    } else if (match(Token::kfn)) {
        return std::make_shared<Item>(std::move(parseFunction()));
    } else if (match(Token::kconst)) {
        if (pos + 1 < tokens.size() && tokens[pos + 1].first == Token::kfn) {
            return std::make_shared<Item>(std::move(parseFunction()));
        } else {
            return std::make_shared<Item>(std::move(parseConstantItem()));
        }
    }
    return nullptr;
}

std::shared_ptr<Function> Parser::parseFunction() {
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
    std::shared_ptr<FunctionParameters> parameters = nullptr;
    if (!match(Token::krightParenthe)) {
        parameters = std::move(parseFunctionParameters());
    }
    if (!match(Token::krightParenthe)) {
        return nullptr;
    }
    advance();
    std::shared_ptr<FunctionReturnType> type(std::move(parseFunctionReturnType()));
    std::shared_ptr<BlockExpression> expression = nullptr;
    if (!match(Token::kSemi)) {
        expression = std::move(parseBlockExpression());
    } else {
        advance();
    }
    return std::make_shared<Function>(isconst, std::move(identifier), std::move(parameters), std::move(type), std::move(expression));
}
std::shared_ptr<ConstantItem> Parser::parseConstantItem() {
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
    std::shared_ptr<Expression> expression = nullptr;
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
    return std::make_shared<ConstantItem>(identifier, std::move(type), std::move(expression));
}
// std::unique_ptr<Module> Parser::parseModule() {

// }
std::shared_ptr<StructStruct> Parser::parseStruct() {
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
        return std::make_shared<StructStruct>(identifier, std::move(structfields), false);
    } else if (match(Token::kSemi)) {
        return std::make_shared<StructStruct>(identifier, nullptr, true);
    } else {
        return nullptr;
    }
}
std::shared_ptr<Enumeration> Parser::parseEnumeration() {
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
    return std::make_shared<Enumeration>(identifier, std::move(enumvariants));
}
std::shared_ptr<InherentImpl> Parser::parseInherentImpl() {
    if (!match(Token::kimpl)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    if (!match(Token::kleftCurly)) {
        return nullptr;
    }
    advance();
    std::vector<std::shared_ptr<AssociatedItem>> items;
    while (!match(Token::krightCurly)) {
        auto item = parseAssociatedItem();
        if (item == nullptr) {
            return nullptr;
        }
        items.push_back(std::move(item));
    }
    advance();
    return std::make_shared<InherentImpl>(std::move(type), std::move(items));
}

std::shared_ptr<FunctionParameters> Parser::parseFunctionParameters() {
    std::vector<std::shared_ptr<FunctionParam>> vec;
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
    return std::make_shared<FunctionParameters>(std::move(vec));
}
std::shared_ptr<FunctionReturnType> Parser::parseFunctionReturnType() {
    if (!match(Token::kRArrow)) {
        return nullptr;
    }
    advance();
    auto type = parseType();
    return std::make_shared<FunctionReturnType>(std::move(type));
}
std::shared_ptr<FunctionParam> Parser::parseFunctionParam() {
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
    return std::make_shared<FunctionParam>(std::move(pattern), std::move(type));
}

std::shared_ptr<StructFields> Parser::parseStructFields() {
    std::vector<std::shared_ptr<StructField>> vec;
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
    return std::make_shared<StructFields>(std::move(vec));
}
std::shared_ptr<StructField> Parser::parseStructField() {
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
    return std::make_shared<StructField>(identifier, std::move(type));
}

std::shared_ptr<EnumVariants> Parser::parseEnumVariants() {
    std::vector<std::shared_ptr<EnumVariant>> vec;
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
    return std::make_shared<EnumVariants>(std::move(vec));
}
std::shared_ptr<EnumVariant> Parser::parseEnumVariant() {
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    return std::make_shared<EnumVariant>(identifier);
}

std::shared_ptr<AssociatedItem> Parser::parseAssociatedItem() {
    if (match(Token::kfn)) {
        return std::make_shared<AssociatedItem>(std::move(parseFunction()));
    } else if (match(Token::kconst)) {
        if (tokens[pos + 1].first == Token::kfn) {
            return std::make_shared<AssociatedItem>(std::move(parseFunction()));
        } else {
            return std::make_shared<AssociatedItem>(std::move(parseConstantItem()));
        }
    }
    return nullptr;
}

std::shared_ptr<LetStatement> Parser::parseLetStatement() {
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
    std::shared_ptr<Expression> expression = nullptr;
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
    return std::make_shared<LetStatement>(std::move(pattern), std::move(type), std::move(expression));
}

std::shared_ptr<ExpressionStatement> Parser::parseExpressionStatement() {
    auto expression = parseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
    if (match(Token::kSemi)) {
        advance();
        return std::make_shared<ExpressionStatement>(std::move(expression), true);
    }
    auto ptr = expression.get();
    if (dynamic_cast<BlockExpression*>(ptr) != nullptr
     || dynamic_cast<ConstBlockExpression*>(ptr) != nullptr
     || dynamic_cast<InfiniteLoopExpression*>(ptr) != nullptr
     || dynamic_cast<PredicateLoopExpression*>(ptr) != nullptr
     || dynamic_cast<IfExpression*>(ptr) != nullptr) {
        return std::make_shared<ExpressionStatement>(std::move(expression), false);
    }
    return nullptr;
}
std::shared_ptr<SimplePathSegment> Parser::parseSimplePathSegment() {
    if (match(Token::kIDENTIFIER)) {
        auto str = getstring();
        advance();
        return std::make_shared<SimplePathSegment>(str, false, false);
    } else if (match(Token::kSelf)) {
        advance();
        return std::make_shared<SimplePathSegment>(std::string(), false, true);
    } else if (match(Token::kself)) {
        advance();
        return std::make_shared<SimplePathSegment>(std::string(), true, false);
    }
    return nullptr;
}
std::shared_ptr<TypePath> Parser::parseTypePath() {
    auto simplepath = parseSimplePathSegment();
    return std::make_shared<TypePath>(std::move(simplepath));
}
std::shared_ptr<ReferenceType> Parser::parseReferenceType() {
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
    return std::make_shared<ReferenceType>(std::move(type), ismut);
}

std::shared_ptr<Pattern> Parser::parsePattern() {
    if (match(Token::kUnderscore)) {
        advance();
        return std::make_shared<WildcardPattern>();
    } else if (match(Token::kAnd) || match(Token::kAndAnd)) {
        return parseReferencePattern();
    } else if (match(Token::kref) || match(Token::kmut) || match(Token::kIDENTIFIER)) {
        return parseIdentifierPattern();
    } else if (match(Token::kMinus)) {
        return parseLiteralPattern();
    } else {
        // auto tmp = pos;
        // auto p = parseLiteralPattern();
        // if (p != nullptr) return p;
        // pos = tmp;
        // auto q = parsePathPattern();
        // if (q != nullptr) return q;
        return nullptr;
    }
}

std::shared_ptr<ReferencePattern> Parser::parseReferencePattern() {
    bool singleordouble = false;
    if (match(Token::kAnd)) {
        singleordouble = false;
    } else if (match(Token::kAndAnd)) {
        singleordouble = true;
    } else {
        return nullptr;
    }
    advance();
    bool ismut = false;
    if (match(Token::kmut)) {
        ismut = true;
        advance();
    }
    auto pattern = parsePattern();
    return std::make_shared<ReferencePattern>(singleordouble, ismut, std::move(pattern));
}
std::shared_ptr<LiteralPattern> Parser::parseLiteralPattern() {
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
    return std::make_shared<LiteralPattern>(neg, std::move(expression));
}
std::shared_ptr<IdentifierPattern> Parser::parseIdentifierPattern() {
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
    // if (match(Token::kAt)) {
    //     advance();
    //     auto pattern = parsePathPattern();
    //     if (pattern == nullptr) {
    //         return nullptr;
    //     }
    //     return std::make_unique<IdentifierPattern>(isref, ismut, identifier, std::move(pattern));
    // }
    return std::make_shared<IdentifierPattern>(isref, ismut, identifier, nullptr);
}
// std::unique_ptr<PathPattern> Parser::parsePathPattern() {
//     auto expression = parseExpression();
//     if (expression == nullptr) {
//         return nullptr;
//     }
//     auto p = dynamic_cast<PathExpression*>(expression.get());
//     if (p == nullptr) {
//         return nullptr;
//     }
//     return std::make_unique<PathPattern>(std::move(expression));
// }
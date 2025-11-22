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
std::shared_ptr<Expression> Parser::parseExpressionWithBlock() {
    auto type = peek();
    switch (type) {
        case Token::kif:
            return parseIfExpression();
        case Token::kwhile:
            return parsePredicateLoopExpression();
        case Token::kloop:
            return parseInfiniteLoopExpression();
        case Token::kleftCurly:
            return parseBlockExpression();
        default:
            return nullptr;
    }
}
std::shared_ptr<Expression> Parser::parseExpression() {
    return parseExpressionPratt(0);
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
    // advance();
    switch (type) {
        case Token::kif:
            return parseIfExpression();
        case Token::kwhile:
            return parsePredicateLoopExpression();
        case Token::kloop:
            return parseInfiniteLoopExpression();
        case Token::kleftCurly:
            return parseBlockExpression();
        case Token::kconst:
            return parseConstBlockExpression();
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
        case Token::kAnd:
        case Token::kAndAnd:
            return parseBorrowExpression();
        case Token::kStar:
            return parseDereferenceExpression();
        case Token::kbreak:
            return parseBreakExpression();
        case Token::kcontinue: {
            advance();
            return std::make_shared<ContinueExpression>();
        }
        case Token::kreturn:
            return parseReturnExpression();
        // case Token::kUnderscore:  {
        //     advance();
        //     return std::make_shared<UnderscoreExpression>();
        // }
        
        case Token::kPathSep:
        case Token::kIDENTIFIER:
        case Token::kself:
        case Token::kSelf:
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
        if (type == Token::kEnd || type == Token::krightParenthe || type == Token::krightCurly || type == Token::kRightSquare) {
            break;
        }

        int leftbp = getLeftTokenBP(type);
        if (leftbp < minbp) {
            break;
        }
        
        // 特殊处理：return、break 等控制流语句不应该作为中缀运算符
        if (type == Token::kreturn || type == Token::kbreak) {
            break;
        }
        
        advance();

        if (type == Token::kleftParenthe) {
            lhs = parseCallExpressionFromInfix(std::move(lhs));
        } else if (type == Token::kleftSquare) {
            lhs = parseIndexExpressionFromInfix(std::move(lhs));
        } else if (type == Token::kleftCurly) {
            lhs = parseStructExpressionFromInfix(std::move(lhs));
        } else if (type == Token::kDot) {
            // 处理方法调用表达式和字段访问表达式
            // 首先尝试解析为MethodCallExpression: Expression '.' SimplePathSegment '(' CallParams? ')'
            size_t backup_pos = pos;  // 保存当前位置以便回溯
            
            if (match(Token::kIDENTIFIER)) {
                auto method_name = getstring();
                advance();
                
                // 检查后面是否跟着左括号，这是方法调用的标志
                if (match(Token::kleftParenthe)) {
                    // 成功匹配方法调用模式
                    advance();  // 消费左括号
                    auto callparams = parseCallParams();
                    if (match(Token::krightParenthe)) {
                        advance();  // 消费右括号
                        lhs = std::make_shared<MethodCallExpression>(std::move(lhs), method_name, std::move(callparams));
                        continue;  // 继续处理后续的中缀表达式
                    }
                }
                
                // 如果不是方法调用，回溯到初始位置并尝试解析为FieldExpression
                pos = backup_pos;
            }
            
            // 回溯失败，尝试解析为FieldExpression
            if (!match(Token::kIDENTIFIER)) {
                return nullptr;
            }
            auto identifier = getstring();
            advance();
            lhs = std::make_shared<FieldExpression>(std::move(lhs), identifier);
        }
        else if (type == Token::kas) {
            lhs = parseTypeCastExpressionFromInfix(std::move(lhs));
        } else {
            int rightbp = getRightTokenBP(type);
            auto rhs = parseExpressionPratt(rightbp);
            if (rhs == nullptr) {
                std::cerr << "Error: no expression after operand\n";
                return nullptr;
            }
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
    if (!match(Token::kleftCurly)) {
        return nullptr;
    }
    advance();
    std::vector<std::shared_ptr<Statement>> statements;
    while (pos < tokens.size() && !match(Token::krightCurly)) {
        auto tmp = pos;
        auto statement = parseStatement();
        if (statement != nullptr) {
            statements.push_back(std::move(statement));
            continue;
        }
        pos = tmp;
        auto expression = parseExpression();
        if (expression == nullptr) {
            std::cerr << "Error: not statement nor expression in blockexpression body at pos " << pos << "\n";
            return nullptr;
        }
        if (dynamic_cast<IfExpression*>(expression.get()) == nullptr
         && dynamic_cast<BlockExpression*>(expression.get()) == nullptr
         && dynamic_cast<InfiniteLoopExpression*>(expression.get()) == nullptr
         && dynamic_cast<PredicateLoopExpression*>(expression.get()) == nullptr) {
            if (match(Token::krightCurly)) {
                advance();
                return std::make_shared<BlockExpression>(std::move(statements), std::move(expression));
            }
            std::cerr << "Error: neither statement nor expressionwithoutblock in blockexpression.\n";
            return nullptr;
        }
    }
    if (!match(Token::krightCurly)) {
        std::cerr << "Error: miss } after blockexpression at pos " << pos << "\n";
        return nullptr;
    }
    advance();
    return std::make_shared<BlockExpression>(std::move(statements), nullptr);
}
std::shared_ptr<ConstBlockExpression> Parser::parseConstBlockExpression() {
    if (!match(Token::kconst)) {
        return nullptr;
    }
    size_t tmp = pos;
    advance();
    auto expression = parseBlockExpression();
    if (expression != nullptr) {
        return std::make_shared<ConstBlockExpression>(std::move(expression));
    }
    pos = tmp;
    return nullptr;
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
    auto simplepath = std::move(parseSimplePath());
    if (simplepath == nullptr) {
        std::cerr << "Error: illegal simplepath\n";
        return nullptr;
    }
    return std::make_shared<PathExpression>(simplepath);
}
std::shared_ptr<GroupedExpression> Parser::parseGroupedExpression() {
    if (match(Token::kleftParenthe)) {
        advance();
    }
    auto expression = parseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
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
    // 使用高优先级来解析操作数，确保一元运算符只绑定到紧随其后的主表达式
    auto expression = parseExpressionPratt(UNARY);
    return std::make_shared<UnaryExpression>(std::move(expression), unarytype);
}
std::shared_ptr<BorrowExpression> Parser::parseBorrowExpression() {
    // std::cerr << "test borrowexpression\n";
    auto borrowtype = peek();
    bool isdouble = (borrowtype == Token::kAndAnd);
    advance();
    
    bool ismut = false;
    if (match(Token::kmut)) {
        ismut = true;
        advance();
    }
    
    // 使用高优先级来解析操作数，确保借用运算符只绑定到紧随其后的主表达式
    auto expression = parseExpressionPratt(UNARY);
    return std::make_shared<BorrowExpression>(std::move(expression), isdouble, ismut);
}
std::shared_ptr<DereferenceExpression> Parser::parseDereferenceExpression() {
    if (match(Token::kStar)) {
        advance();
    }
    // 使用高优先级来解析操作数，确保解引用运算符只绑定到紧随其后的主表达式
    // 而不会吞噬后面的低优先级运算符
    auto expression = parseExpressionPratt(UNARY);
    return std::make_shared<DereferenceExpression>(std::move(expression));
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

std::shared_ptr<TypeCastExpression> Parser::parseTypeCastExpressionFromInfix(std::shared_ptr<Expression> lhs) {
    // 此时 'as' token 已经被 advance() 消费了
    auto typenobounds = parseType();
    return std::make_shared<TypeCastExpression>(std::move(lhs), std::move(typenobounds));
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
        auto letstatement = std::move(parseLetStatement());
        if (letstatement == nullptr) {
            std::cerr << "Error: illegal Letstatement in statement\n";
            return nullptr;
        }
        return std::make_shared<Statement>(letstatement);
    }
    // if (match(Token::kconst)) {
    //     // 检查是否是 const 语句（在函数内部）
    //     size_t tmp = pos;
    //     advance();
    //     if (match(Token::kIDENTIFIER)) {
    //         advance();
    //         if (match(Token::kColon)) {
    //             // 这是一个 const 语句，解析为 ConstantItem
    //             pos = tmp;
    //             auto constItem = parseConstantItem();
    //             if (constItem != nullptr) {
    //                 return std::make_shared<Statement>(std::move(constItem));
    //             }
    //         }
    //     }
    //     // 如果不是 const 语句，回退
    //     pos = tmp;
    // }
    
    // 先尝试解析表达式语句，因为像 while、if、loop 等都是表达式
    size_t tmp = pos;
    auto expressionstatement = parseExpressionStatement();
    if (expressionstatement != nullptr) {
        return std::make_shared<Statement>(std::move(expressionstatement));
    }
    pos = tmp;

    // 只有在表达式解析失败时，才尝试解析 item
    tmp = pos;
    auto item = parseItem();
    if (item != nullptr) {
        return std::make_shared<Statement>(std::move(item));
    }
    pos = tmp;
    return nullptr;
}
std::shared_ptr<Conditions> Parser::parseConditions() {
    if (!match(Token::kleftParenthe)) {
        return nullptr;
    }
    advance();
    auto expression = parseExpression();
    if (expression == nullptr) {
        std::cerr << "Error: failed to parse expression in conditions\n";
        return nullptr;
    }
    if (!match(Token::krightParenthe)) {
        std::cerr << "Error: expected ) after expression in conditions, but found: " << tokens[pos].second << " at pos " << pos << "\n";
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
        auto pathsegment = std::move(parseSimplePathSegment());
        if (pathsegment == nullptr) {
            std::cerr << "Error: illegal simplepathsegment\n";
            return nullptr;
        }
        vec.push_back(pathsegment);
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
        
        case Token::kleftParenthe: {
            advance();
            if (!match(Token::krightParenthe)) {
                return nullptr;
            }
            advance();
            return std::make_shared<UnitType>();
        }
                
        default:
            return nullptr;
    }
}



std::shared_ptr<Crate> Parser::parseCrate() {
    std::vector<std::shared_ptr<Item>> items;
    while (pos < tokens.size()) {
        auto item = parseItem();
        if (item == nullptr) {
            return nullptr;
        }
        items.push_back(std::move(item));
    }
    return std::make_shared<Crate>(std::move(items));
}
std::shared_ptr<Item> Parser::parseItem() {
    std::shared_ptr<ASTNode> astnode = nullptr;
    if (match(Token::kstruct)) {
        astnode = parseStruct();
    } else if (match(Token::kenum)) {
        astnode = parseEnumeration();
    } else if (match(Token::kimpl)) {
        astnode = parseInherentImpl();
    } else if (match(Token::kfn)) {
        astnode = parseFunction();
    } else if (match(Token::kconst)) {
        if (pos + 1 < tokens.size() && tokens[pos + 1].first == Token::kfn) {
            astnode = parseFunction();
        } else {
            astnode = parseConstantItem();
        }
    }
    // 如果遇到 }，说明块结束，不应该报错
    if (match(Token::krightCurly)) {
        return nullptr;
    }
    if (astnode == nullptr) {
        // std::cerr << "Error: illegal item at pos " << pos << ", token: " << (pos < tokens.size() ? tokens[pos].second : "EOF") << "\n";
        return nullptr;
    }
    return std::make_shared<Item>(astnode);
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
        if (expression == nullptr) {
            std::cerr << "Error: illegal blockexpression in function\n";
            return nullptr;
        }
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
    if (enumvariants == nullptr) {
        return nullptr;
    }
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
    if (type == nullptr) {
        return nullptr;
    }
    if (!match(Token::kleftCurly)) {
        return nullptr;
    }
    advance();
    std::vector<std::shared_ptr<AssociatedItem>> items;
    while (!match(Token::krightCurly)) {
        auto item = parseAssociatedItem();
        if (item == nullptr) {
            // 如果遇到 }，说明 impl 块结束，不应该报错
            if (match(Token::krightCurly)) {
                break;
            }
            return nullptr;
        }
        items.push_back(std::move(item));
    }
    advance();
    return std::make_shared<InherentImpl>(std::move(type), std::move(items));
}

std::shared_ptr<FunctionParameters> Parser::parseFunctionParameters() {
    std::vector<std::shared_ptr<FunctionParam>> vec;
    bool hasSelf = false;
    bool selfIsRef = false;
    bool selfIsMut = false;
    
    auto param = parseFunctionParam();
    if (param == nullptr) {
        return nullptr;
    }
    
    // 检查是否是 self 参数
    auto refPattern = dynamic_cast<ReferencePattern*>(param->patternnotopalt.get());
    auto identPattern = dynamic_cast<IdentifierPattern*>(param->patternnotopalt.get());
    
    if (refPattern != nullptr) {
        auto innerPattern = refPattern->pattern;
        if (innerPattern != nullptr) {
            auto innerIdentPattern = dynamic_cast<IdentifierPattern*>(innerPattern.get());
            if (innerIdentPattern != nullptr && innerIdentPattern->identifier == "self") {
                hasSelf = true;
                selfIsRef = true;
                selfIsMut = refPattern->hasmut;
            }
        }
    } else if (identPattern != nullptr && identPattern->identifier == "self") {
        hasSelf = true;
        selfIsMut = identPattern->hasmut;
    }
    
    vec.push_back(std::move(param));
    
    while (match(Token::kComma)) {
        advance();
        if (match(Token::krightParenthe)) {
            break;
        }
        auto param = parseFunctionParam();
        if (param == nullptr) {
            return nullptr;
        }
        vec.push_back(std::move(param));
    }
    if (match(Token::kComma)) {
        advance();
    }
    
    auto functionParams = std::make_shared<FunctionParameters>(std::move(vec));
    functionParams->hasSelfParam = hasSelf;
    functionParams->selfIsRef = selfIsRef;
    functionParams->selfIsMut = selfIsMut;
    
    return functionParams;
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
    
    // 检查是否是特殊的 self 模式（&self, &mut self, self, mut self）
    // 这些模式不需要类型注解
    auto refPattern = dynamic_cast<ReferencePattern*>(pattern.get());
    auto identPattern = dynamic_cast<IdentifierPattern*>(pattern.get());
    bool isSpecialSelfPattern = false;
    
    if (refPattern != nullptr) {
        auto innerPattern = refPattern->pattern;
        if (innerPattern != nullptr) {
            auto innerIdentPattern = dynamic_cast<IdentifierPattern*>(innerPattern.get());
            if (innerIdentPattern != nullptr && innerIdentPattern->identifier == "self") {
                isSpecialSelfPattern = true;
            }
        }
    } else if (identPattern != nullptr && identPattern->identifier == "self") {
        isSpecialSelfPattern = true;
    }
    
    if (isSpecialSelfPattern) {
        // 对于特殊的 self 模式，不需要类型注解
        // 创建一个虚拟的类型，因为 FunctionParam 需要类型
        auto dummyType = std::make_shared<TypePath>(std::make_shared<SimplePathSegment>("", false, false));
        return std::make_shared<FunctionParam>(std::move(pattern), std::move(dummyType));
    }
    
    // 对于普通参数，需要类型注解
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
    
    // 检查是否为空的结构体
    if (match(Token::krightCurly)) {
        return std::make_shared<StructFields>(std::move(vec));
    }
    
    auto field = parseStructField();
    if (field == nullptr) {
        return nullptr;
    }
    vec.push_back(std::move(field));
    
    // 修复：正确处理字段间的逗号和可选的尾随逗号
    while (true) {
        // 检查是否有逗号
        if (match(Token::kComma)) {
            advance();
            
            // 检查逗号后是否是右大括号（尾随逗号）
            if (match(Token::krightCurly)) {
                break;
            }
            
            // 解析下一个字段
            auto nextField = parseStructField();
            if (nextField == nullptr) {
                return nullptr;
            }
            vec.push_back(std::move(nextField));
        } else if (match(Token::krightCurly)) {
            // 没有逗号，直接遇到右大括号
            break;
        } else {
            // 既不是逗号也不是右大括号，语法错误
            return nullptr;
        }
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
    advance();
    auto type = parseType();
    if (!type) {
        return nullptr;
    }
    return std::make_shared<StructField>(identifier, std::move(type));
}

std::shared_ptr<EnumVariants> Parser::parseEnumVariants() {
    std::vector<std::shared_ptr<EnumVariant>> vec;
    
    // 检查是否为空的 enum
    if (match(Token::krightCurly)) {
        return std::make_shared<EnumVariants>(std::move(vec));
    }
    
    auto variant = parseEnumVariant();
    if (variant == nullptr) {
        return nullptr;
    }
    vec.push_back(std::move(variant));
    while (match(Token::kComma)) {
        advance();
        // 检查逗号后是否是右大括号（尾随逗号）
        if (match(Token::krightCurly)) {
            break;
        }
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
    advance();
    return std::make_shared<EnumVariant>(identifier);
}

std::shared_ptr<AssociatedItem> Parser::parseAssociatedItem() {
    if (match(Token::kfn)) {
        return std::make_shared<AssociatedItem>(std::move(parseFunction()));
    } else if (match(Token::kconst)) {
        if (pos + 1 < tokens.size() && tokens[pos + 1].first == Token::kfn) {
            return std::make_shared<AssociatedItem>(std::move(parseFunction()));
        } else {
            return std::make_shared<AssociatedItem>(std::move(parseConstantItem()));
        }
    }
    // 如果遇到 }，说明 impl 块结束，不应该报错
    if (match(Token::krightCurly)) {
        return nullptr;
    }
    return nullptr;
}

std::shared_ptr<LetStatement> Parser::parseLetStatement() {
    if (!match(Token::klet)) {
        return nullptr;
    }
    advance();
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
    auto tmp = pos;
    auto expressionwithblock = parseExpressionWithBlock();
    if (expressionwithblock != nullptr) {
        bool hassemi = false;
        if (match(Token::kSemi)) {
            hassemi = true;
            advance();
        }
        return std::make_shared<ExpressionStatement>(std::move(expressionwithblock), hassemi);
    }
    pos = tmp;
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
     || dynamic_cast<IfExpression*>(ptr) != nullptr
     || dynamic_cast<ReturnExpression*>(ptr) != nullptr) {
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
    } else if (match(Token::kref) || match(Token::kmut) || match(Token::kIDENTIFIER) || match(Token::kself)) {
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
    if (pattern == nullptr) {
        return nullptr;
    }
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
    if (!match(Token::kIDENTIFIER) && !match(Token::kself)) {
        return nullptr;
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

// 新增：结构体表达式解析方法
std::shared_ptr<StructExpression> Parser::parseStructExpressionFromInfix(std::shared_ptr<Expression> path) {
    // 此时左大括号已经被 advance() 消费了
    // 检查是否有结构体字段
    std::shared_ptr<StructExprFields> structExprFields = nullptr;
    
    // 将传入的 path 转换为 PathExpression
    auto pathExpr = std::dynamic_pointer_cast<PathExpression>(path);
    if (pathExpr == nullptr) {
        std::cerr << "Error: path in struct expression must be a PathExpression\n";
        return nullptr;
    }
    
    // 检查是否为空的结构体表达式 {}
    if (match(Token::krightCurly)) {
        advance();
        return std::make_shared<StructExpression>(pathExpr, nullptr);
    }
    
    // 解析结构体字段
    structExprFields = parseStructExprFields();
    if (structExprFields == nullptr) {
        return nullptr;
    }
    
    // 检查右大括号
    if (!match(Token::krightCurly)) {
        return nullptr;
    }
    advance();
    
    return std::make_shared<StructExpression>(pathExpr, structExprFields);
}

std::shared_ptr<StructExprFields> Parser::parseStructExprFields() {
    std::vector<std::shared_ptr<StructExprField>> fields;
    
    // 解析第一个字段
    auto field = parseStructExprField();
    if (field == nullptr) {
        return nullptr;
    }
    fields.push_back(std::move(field));
    
    // 解析后续字段
    while (match(Token::kComma)) {
        advance();
        
        // 检查是否是末尾的逗号
        if (match(Token::krightCurly)) {
            break;
        }
        
        auto nextField = parseStructExprField();
        if (nextField == nullptr) {
            return nullptr;
        }
        fields.push_back(std::move(nextField));
    }
    
    return std::make_shared<StructExprFields>(std::move(fields), nullptr);
}

std::shared_ptr<StructExprField> Parser::parseStructExprField() {
    // 解析标识符
    if (!match(Token::kIDENTIFIER)) {
        return nullptr;
    }
    auto identifier = getstring();
    advance();
    
    // 解析冒号
    if (!match(Token::kColon)) {
        return nullptr;
    }
    advance();
    
    // 解析表达式
    auto expression = parseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
    
    return std::make_shared<StructExprField>(identifier, -1, std::move(expression));
}

std::shared_ptr<StructBase> Parser::parseStructBase() {
    // 暂时不实现结构体基础表达式
    return nullptr;
}
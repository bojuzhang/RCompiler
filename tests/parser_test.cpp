#include <gtest/gtest.h>
#include "parser.hpp"
#include "astnodes.hpp"
#include "lexer.hpp"
#include <memory>
#include <vector>
#include <utility>

class ParserTest : public ::testing::Test {
protected:
    std::unique_ptr<Parser> createParser(const std::vector<std::pair<Token, std::string>>& tokens) {
        return std::make_unique<Parser>(tokens);
    }
};

// Test 1: 基础parser操作测试
TEST_F(ParserTest, BasicParserOperations) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "test"},
        {Token::kINTEGER_LITERAL, "42"}
    };
    auto parser = createParser(tokens);
    
    EXPECT_EQ(parser->peek(), Token::kIDENTIFIER);
    EXPECT_TRUE(parser->match(Token::kIDENTIFIER));
    EXPECT_FALSE(parser->match(Token::kINTEGER_LITERAL));
    
    parser->advance();
    EXPECT_EQ(parser->peek(), Token::kINTEGER_LITERAL);
    EXPECT_EQ(parser->getstring(), "42");
}

// Test 2: 整数字面量表达式解析
TEST_F(ParserTest, ParseIntegerLiteralExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kINTEGER_LITERAL, "123"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto literal = dynamic_cast<LiteralExpression*>(expr.get());
    EXPECT_NE(literal, nullptr);
}

// Test 3: 字符字面量表达式解析
TEST_F(ParserTest, ParseCharLiteralExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kCHAR_LITERAL, "'a'"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto literal = dynamic_cast<LiteralExpression*>(expr.get());
    EXPECT_NE(literal, nullptr);
}

// Test 4: 字符串字面量表达式解析
TEST_F(ParserTest, ParseStringLiteralExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kSTRING_LITERAL, "\"hello\""}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto literal = dynamic_cast<LiteralExpression*>(expr.get());
    EXPECT_NE(literal, nullptr);
}

// Test 5: 标识符表达式解析
TEST_F(ParserTest, ParseIdentifierExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "variable"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto path = dynamic_cast<PathExpression*>(expr.get());
    EXPECT_NE(path, nullptr);
}

// Test 6: 块表达式解析
TEST_F(ParserTest, ParseBlockExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kleftCurly, "{"},
        {Token::kINTEGER_LITERAL, "42"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto block = dynamic_cast<BlockExpression*>(expr.get());
    EXPECT_NE(block, nullptr);
}

// Test 7: const块表达式解析
TEST_F(ParserTest, ParseConstBlockExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kconst, "const"},
        {Token::kleftCurly, "{"},
        {Token::kINTEGER_LITERAL, "42"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto constBlock = dynamic_cast<ConstBlockExpression*>(expr.get());
    EXPECT_NE(constBlock, nullptr);
}

// Test 8: 无限循环表达式解析
TEST_F(ParserTest, ParseInfiniteLoopExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kloop, "loop"},
        {Token::kleftCurly, "{"},
        {Token::kbreak, "break"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto loop = dynamic_cast<InfiniteLoopExpression*>(expr.get());
    EXPECT_NE(loop, nullptr);
}

// Test 9: while循环表达式解析
TEST_F(ParserTest, ParsePredicateLoopExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kwhile, "while"},
        {Token::ktrue, "true"},
        {Token::kleftCurly, "{"},
        {Token::kbreak, "break"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto whileLoop = dynamic_cast<PredicateLoopExpression*>(expr.get());
    EXPECT_NE(whileLoop, nullptr);
}

// Test 10: if表达式解析
TEST_F(ParserTest, ParseIfExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kif, "if"},
        {Token::ktrue, "true"},
        {Token::kleftCurly, "{"},
        {Token::kINTEGER_LITERAL, "1"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto ifExpr = dynamic_cast<IfExpression*>(expr.get());
    EXPECT_NE(ifExpr, nullptr);
}

// Test 11: match表达式解析
TEST_F(ParserTest, ParseMatchExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kmatch, "match"},
        {Token::kIDENTIFIER, "x"},
        {Token::kleftCurly, "{"},
        {Token::kUnderscore, "_"},
        {Token::kFatArrow, "=>"},
        {Token::kINTEGER_LITERAL, "42"},
        {Token::krightCurly, "}"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto matchExpr = dynamic_cast<MatchExpression*>(expr.get());
    EXPECT_NE(matchExpr, nullptr);
}

// Test 12: 一元表达式解析（前缀）
TEST_F(ParserTest, ParseUnaryExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kMinus, "-"},
        {Token::kINTEGER_LITERAL, "42"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto unary = dynamic_cast<UnaryExpression*>(expr.get());
    EXPECT_NE(unary, nullptr);
}

// Test 13: 逻辑非表达式解析
TEST_F(ParserTest, ParseNotExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kNot, "!"},
        {Token::ktrue, "true"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto unary = dynamic_cast<UnaryExpression*>(expr.get());
    EXPECT_NE(unary, nullptr);
}

// Test 14: 通配符模式解析
TEST_F(ParserTest, ParseWildcardPattern) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kUnderscore, "_"}
    };
    auto parser = createParser(tokens);
    
    auto pattern = parser->parsePattern();
    EXPECT_NE(pattern, nullptr);
    
    auto wildcard = dynamic_cast<WildcardPattern*>(pattern.get());
    EXPECT_NE(wildcard, nullptr);
}

// Test 15: 引用模式解析
TEST_F(ParserTest, ParseReferencePattern) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kAnd, "&"},
        {Token::kIDENTIFIER, "x"}
    };
    auto parser = createParser(tokens);
    
    auto pattern = parser->parsePattern();
    EXPECT_NE(pattern, nullptr);
    
    auto refPattern = dynamic_cast<ReferencePattern*>(pattern.get());
    EXPECT_NE(refPattern, nullptr);
}

// Test 16: 标识符模式解析
TEST_F(ParserTest, ParseIdentifierPattern) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "variable"}
    };
    auto parser = createParser(tokens);
    
    auto pattern = parser->parsePattern();
    EXPECT_NE(pattern, nullptr);
    
    auto idPattern = dynamic_cast<IdentifierPattern*>(pattern.get());
    EXPECT_NE(idPattern, nullptr);
}

// Test 17: 字面量模式解析
TEST_F(ParserTest, ParseLiteralPattern) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kMinus, "-"},
        {Token::kINTEGER_LITERAL, "42"}
    };
    auto parser = createParser(tokens);
    
    auto pattern = parser->parsePattern();
    EXPECT_NE(pattern, nullptr);
    
    auto litPattern = dynamic_cast<LiteralPattern*>(pattern.get());
    EXPECT_NE(litPattern, nullptr);
}

// Test 18: 路径模式解析
TEST_F(ParserTest, ParsePathPattern) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "std"},
        {Token::kPathSep, "::"},
        {Token::kIDENTIFIER, "Option"}
    };
    auto parser = createParser(tokens);
    
    auto pattern = parser->parsePattern();
    EXPECT_NE(pattern, nullptr);
    
    auto pathPattern = dynamic_cast<PathPattern*>(pattern.get());
    EXPECT_NE(pathPattern, nullptr);
}

// Test 19: 类型路径解析
TEST_F(ParserTest, ParseTypePath) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "Vec"},
        {Token::kLe, "<"},
        {Token::kIDENTIFIER, "i32"},
        {Token::kGe, ">"}
    };
    auto parser = createParser(tokens);
    
    auto typePath = parser->parseTypePath();
    EXPECT_NE(typePath, nullptr);
}

// Test 20: 引用类型解析
TEST_F(ParserTest, ParseReferenceType) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kAnd, "&"},
        {Token::kIDENTIFIER, "i32"}
    };
    auto parser = createParser(tokens);
    
    auto refType = parser->parseReferenceType();
    EXPECT_NE(refType, nullptr);
}

// Test 21: let语句解析
TEST_F(ParserTest, ParseLetStatement) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::klet, "let"},
        {Token::kIDENTIFIER, "x"},
        {Token::kEq, "="},
        {Token::kINTEGER_LITERAL, "42"},
        {Token::kSemi, ";"}
    };
    auto parser = createParser(tokens);
    
    auto stmt = parser->parseLetStatement();
    EXPECT_NE(stmt, nullptr);
}

// Test 22: 表达式语句解析
TEST_F(ParserTest, ParseExpressionStatement) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kINTEGER_LITERAL, "42"},
        {Token::kSemi, ";"}
    };
    auto parser = createParser(tokens);
    
    auto stmt = parser->parseExpressionStatement();
    EXPECT_NE(stmt, nullptr);
}

// Test 23: 简单路径解析
TEST_F(ParserTest, ParseSimplePath) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kIDENTIFIER, "std"},
        {Token::kPathSep, "::"},
        {Token::kIDENTIFIER, "vec"},
        {Token::kPathSep, "::"},
        {Token::kIDENTIFIER, "Vec"}
    };
    auto parser = createParser(tokens);
    
    auto path = parser->parseSimplePath();
    EXPECT_NE(path, nullptr);
}

// Test 24: 数组表达式解析
TEST_F(ParserTest, ParseArrayExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kleftSquare, "["},
        {Token::kINTEGER_LITERAL, "1"},
        {Token::kComma, ","},
        {Token::kINTEGER_LITERAL, "2"},
        {Token::kComma, ","},
        {Token::kINTEGER_LITERAL, "3"},
        {Token::kRightSquare, "]"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto arrayExpr = dynamic_cast<ArrayExpression*>(expr.get());
    EXPECT_NE(arrayExpr, nullptr);
}

// Test 25: break表达式解析
TEST_F(ParserTest, ParseBreakExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kbreak, "break"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto breakExpr = dynamic_cast<BreakExpression*>(expr.get());
    EXPECT_NE(breakExpr, nullptr);
}

// Test 26: continue表达式解析
TEST_F(ParserTest, ParseContinueExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kcontinue, "continue"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto continueExpr = dynamic_cast<ContinueExpression*>(expr.get());
    EXPECT_NE(continueExpr, nullptr);
}

// Test 27: return表达式解析
TEST_F(ParserTest, ParseReturnExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kreturn, "return"},
        {Token::kINTEGER_LITERAL, "42"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto returnExpr = dynamic_cast<ReturnExpression*>(expr.get());
    EXPECT_NE(returnExpr, nullptr);
}

// Test 28: 下划线表达式解析
TEST_F(ParserTest, ParseUnderscoreExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kUnderscore, "_"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto underscoreExpr = dynamic_cast<UnderscoreExpression*>(expr.get());
    EXPECT_NE(underscoreExpr, nullptr);
}

// Test 29: 分组表达式解析
TEST_F(ParserTest, ParseGroupedExpression) {
    std::vector<std::pair<Token, std::string>> tokens = {
        {Token::kleftParenthe, "("},
        {Token::kINTEGER_LITERAL, "42"},
        {Token::krightParenthe, ")"}
    };
    auto parser = createParser(tokens);
    
    auto expr = parser->parseExpression();
    EXPECT_NE(expr, nullptr);
    
    auto groupedExpr = dynamic_cast<GroupedExpression*>(expr.get());
    EXPECT_NE(groupedExpr, nullptr);
}
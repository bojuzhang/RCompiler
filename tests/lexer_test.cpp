#include "lexer.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>

class LexserTest : public ::testing::Test {
protected:
    lexser lexer;
};

// 测试用例 1: 空输入
TEST_F(LexserTest, EmptyInput) {
    std::string input = "";
    auto result = lexer.lexString(input);
    EXPECT_TRUE(result.empty());
}

// 测试用例 2: 简单标识符
TEST_F(LexserTest, SimpleIdentifier) {
    std::string input = "x";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[0].second, "x");
}

// 测试用例 3: 关键字识别 (测试最长匹配原则)
TEST_F(LexserTest, KeywordRecognition) {
    std::string input = "fn function fun for foreach";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].first, Token::kfn);
    EXPECT_EQ(result[0].second, "fn");
    EXPECT_EQ(result[1].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[1].second, "function");
    EXPECT_EQ(result[2].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[2].second, "fun");
    EXPECT_EQ(result[3].first, Token::kfor);
    EXPECT_EQ(result[3].second, "for");
    EXPECT_EQ(result[4].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[4].second, "foreach");
}

// 测试用例 4: 整数字面量 (不同进制)
TEST_F(LexserTest, IntegerLiterals) {
    std::string input = "123 0x1F 0b1010 0o77 1_000_000 55456u32";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 6);
    for (const auto& token : result) {
        EXPECT_EQ(token.first, Token::kINTEGER_LITERAL);
    }
    EXPECT_EQ(result[0].second, "123");
    EXPECT_EQ(result[1].second, "0x1F");
    EXPECT_EQ(result[2].second, "0b1010");
    EXPECT_EQ(result[3].second, "0o77");
    EXPECT_EQ(result[4].second, "1_000_000");
    EXPECT_EQ(result[5].second, "55456u32");
}

// 测试用例 5: 字符串和字符字面量
TEST_F(LexserTest, StringAndCharLiterals) {
    std::string input = "\"hello\" 'a' \"world\\n\" '\\''";
    auto result = lexer.lexString(input);
    // for (auto token : result) {
    //     std::cout << to_string(token.first) << " " << token.second << "\n";
    // }
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0].first, Token::kSTRING_LITERAL);
    EXPECT_EQ(result[0].second, "\"hello\"");
    EXPECT_EQ(result[1].first, Token::kCHAR_LITERAL);
    EXPECT_EQ(result[1].second, "'a'");
    EXPECT_EQ(result[2].first, Token::kSTRING_LITERAL);
    EXPECT_EQ(result[2].second, "\"world\\n\"");
    EXPECT_EQ(result[3].first, Token::kCHAR_LITERAL);
    EXPECT_EQ(result[3].second, "'\\''");
}

// 测试用例 6: 原始字符串字面量
TEST_F(LexserTest, RawStringLiteral) {
    std::string input = "r#\"raw\nstring\"# r##\"raw with #\"##";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, Token::kRAW_STRING_LITERAL);
    EXPECT_EQ(result[0].second, "r#\"raw\nstring\"#");
    EXPECT_EQ(result[1].first, Token::kRAW_STRING_LITERAL);
    EXPECT_EQ(result[1].second, "r##\"raw with #\"##");
}

// 测试用例 7: 字节字面量和字节字符串
TEST_F(LexserTest, ByteLiterals) {
    std::string input = "b'a' b\"bytes\" br#\"raw bytes\"#";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].first, Token::kBYTE_LITERAL);
    EXPECT_EQ(result[0].second, "b'a'");
    EXPECT_EQ(result[1].first, Token::kBYTE_STRING_LITERAL);
    EXPECT_EQ(result[1].second, "b\"bytes\"");
    EXPECT_EQ(result[2].first, Token::kRAW_BYTE_STRING_LITERAL);
    EXPECT_EQ(result[2].second, "br#\"raw bytes\"#");
}

// 测试用例 8: C字符串
TEST_F(LexserTest, CStringLiterals) {
    std::string input = "c\"C string\" cr#\"raw C string\"#";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, Token::kC_STRING_LITERAL);
    EXPECT_EQ(result[0].second, "c\"C string\"");
    EXPECT_EQ(result[1].first, Token::kRAW_C_STRING_LITERAL);
    EXPECT_EQ(result[1].second, "cr#\"raw C string\"#");
}

// 测试用例 9: 算术运算符
TEST_F(LexserTest, ArithmeticOperators) {
    std::string input = "+ - * / %";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].first, Token::kPlus);
    EXPECT_EQ(result[1].first, Token::kMinus);
    EXPECT_EQ(result[2].first, Token::kStar);
    EXPECT_EQ(result[3].first, Token::kSlash);
    EXPECT_EQ(result[4].first, Token::kPercent);
}

// 测试用例 10: 比较运算符
TEST_F(LexserTest, ComparisonOperators) {
    std::string input = "== != < > <= >= =";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 7);
    EXPECT_EQ(result[0].first, Token::kEqEq);
    EXPECT_EQ(result[1].first, Token::kNe);
    EXPECT_EQ(result[2].first, Token::kLt);
    EXPECT_EQ(result[3].first, Token::kGt);
    EXPECT_EQ(result[4].first, Token::kLe);
    EXPECT_EQ(result[5].first, Token::kGe);
    EXPECT_EQ(result[6].first, Token::kEq);
}

// 测试用例 11: 赋值运算符
TEST_F(LexserTest, AssignmentOperators) {
    std::string input = "+= -= *= /= %= ^= &= |= <<= >>=";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 10);
    EXPECT_EQ(result[0].first, Token::kPlusEq);
    EXPECT_EQ(result[1].first, Token::kMinusEq);
    EXPECT_EQ(result[2].first, Token::kStarEq);
    EXPECT_EQ(result[3].first, Token::kSlashEq);
    EXPECT_EQ(result[4].first, Token::kPercentEq);
    EXPECT_EQ(result[5].first, Token::kCaretEq);
    EXPECT_EQ(result[6].first, Token::kAndEq);
    EXPECT_EQ(result[7].first, Token::kOrEq);
    EXPECT_EQ(result[8].first, Token::kShlEq);
    EXPECT_EQ(result[9].first, Token::kShrEq);
}

// 测试用例 12: 位运算符和逻辑运算符
TEST_F(LexserTest, BitwiseAndLogicalOperators) {
    std::string input = "& | ^ ~ << >> && || !";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 9);
    EXPECT_EQ(result[0].first, Token::kAnd);
    EXPECT_EQ(result[1].first, Token::kOr);
    EXPECT_EQ(result[2].first, Token::kCaret);
    EXPECT_EQ(result[3].first, Token::kTilde);
    EXPECT_EQ(result[4].first, Token::kShl);
    EXPECT_EQ(result[5].first, Token::kShr);
    EXPECT_EQ(result[6].first, Token::kAndAnd);
    EXPECT_EQ(result[7].first, Token::kOrOr);
    EXPECT_EQ(result[8].first, Token::kNot);
}

// 测试用例 13: 其他运算符和符号
TEST_F(LexserTest, OtherOperatorsAndSymbols) {
    std::string input = "@ _ . .. ... ..= , ; : :: -> => <- # $ ?";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 16);
    EXPECT_EQ(result[0].first, Token::kAt);
    EXPECT_EQ(result[1].first, Token::kUnderscore);
    EXPECT_EQ(result[2].first, Token::kDot);
    EXPECT_EQ(result[3].first, Token::kDotDot);
    EXPECT_EQ(result[4].first, Token::kDotDotDot);
    EXPECT_EQ(result[5].first, Token::kDotDotEq);
    EXPECT_EQ(result[6].first, Token::kComma);
    EXPECT_EQ(result[7].first, Token::kSemi);
    EXPECT_EQ(result[8].first, Token::kColon);
    EXPECT_EQ(result[9].first, Token::kPathSep);
    EXPECT_EQ(result[10].first, Token::kRArrow);
    EXPECT_EQ(result[11].first, Token::kFatArrow);
    EXPECT_EQ(result[12].first, Token::kLArrow);
    EXPECT_EQ(result[13].first, Token::kPound);
    EXPECT_EQ(result[14].first, Token::kDollar);
    EXPECT_EQ(result[15].first, Token::kQuestion);
}

// 测试用例 14: 分隔符
TEST_F(LexserTest, Delimiters) {
    std::string input = "() {} []";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 6);
    EXPECT_EQ(result[0].first, Token::kleftParenthe);
    EXPECT_EQ(result[1].first, Token::krightParenthe);
    EXPECT_EQ(result[2].first, Token::kleftCurly);
    EXPECT_EQ(result[3].first, Token::krightCurly);
    EXPECT_EQ(result[4].first, Token::kleftSquare);
    EXPECT_EQ(result[5].first, Token::kRightSquare);
}

// 测试用例 15: 注释
TEST_F(LexserTest, Comments) {
    std::string input = "// Single line comment\n/* Multi-line\ncomment */ code // Another comment";
    auto result = lexer.lexString(input);
    // for (auto token : result) {
    //     std::cout << to_string(token.first) << " " << token.second << "\n";
    // }
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0].first, Token::kCOMMENT);
    EXPECT_EQ(result[1].first, Token::kCOMMENT);
    EXPECT_EQ(result[2].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[2].second, "code");
    EXPECT_EQ(result[3].first, Token::kCOMMENT);
}

// 测试用例 16: 保留关键字
TEST_F(LexserTest, ReservedKeywords) {
    std::string input = "abstract become box do final macro override priv typeof unsized virtual yield try gen";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 14);
    EXPECT_EQ(result[0].first, Token::kabstract);
    EXPECT_EQ(result[1].first, Token::kbecome);
    EXPECT_EQ(result[2].first, Token::kbox);
    EXPECT_EQ(result[3].first, Token::kdo);
    EXPECT_EQ(result[4].first, Token::kfinal);
    EXPECT_EQ(result[5].first, Token::kmacro);
    EXPECT_EQ(result[6].first, Token::koverride);
    EXPECT_EQ(result[7].first, Token::kpriv);
    EXPECT_EQ(result[8].first, Token::ktypeof);
    EXPECT_EQ(result[9].first, Token::kunsized);
    EXPECT_EQ(result[10].first, Token::kvirtual);
    EXPECT_EQ(result[11].first, Token::kyield);
    EXPECT_EQ(result[12].first, Token::ktry);
    EXPECT_EQ(result[13].first, Token::kgen);
}

// 测试用例 17: 复杂表达式
TEST_F(LexserTest, ComplexExpression) {
    std::string input = "fn add(a: i32, b: i32) -> i32 { return a + b * 2; }";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 22);
    EXPECT_EQ(result[0].first, Token::kfn);
    EXPECT_EQ(result[1].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[1].second, "add");
    EXPECT_EQ(result[2].first, Token::kleftParenthe);
    EXPECT_EQ(result[3].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[3].second, "a");
    EXPECT_EQ(result[4].first, Token::kColon);
    EXPECT_EQ(result[5].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[5].second, "i32");
    EXPECT_EQ(result[6].first, Token::kComma);
    EXPECT_EQ(result[7].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[7].second, "b");
    EXPECT_EQ(result[8].first, Token::kColon);
    EXPECT_EQ(result[9].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[9].second, "i32");
    EXPECT_EQ(result[10].first, Token::krightParenthe);
    EXPECT_EQ(result[11].first, Token::kRArrow);
    EXPECT_EQ(result[12].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[12].second, "i32");
    EXPECT_EQ(result[13].first, Token::kleftCurly);
    EXPECT_EQ(result[14].first, Token::kreturn);
    EXPECT_EQ(result[15].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[15].second, "a");
    EXPECT_EQ(result[16].first, Token::kPlus);
    EXPECT_EQ(result[17].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[17].second, "b");
    EXPECT_EQ(result[18].first, Token::kStar);
    EXPECT_EQ(result[19].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[19].second, "2");
    EXPECT_EQ(result[20].first, Token::kSemi);
    EXPECT_EQ(result[21].first, Token::krightCurly);
}

// 测试用例 18: 最大吞噬原则测试 (长匹配优先)
TEST_F(LexserTest, MaximalMunchPrinciple) {
    std::string input = "====>>=<<=->";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].first, Token::kEqEq);
    EXPECT_EQ(result[0].second, "==");
    EXPECT_EQ(result[1].first, Token::kEqEq);
    EXPECT_EQ(result[1].second, "==");
    EXPECT_EQ(result[2].first, Token::kShrEq);
    EXPECT_EQ(result[2].second, ">>=");
    EXPECT_EQ(result[3].first, Token::kShlEq);
    EXPECT_EQ(result[3].second, "<<=");
    EXPECT_EQ(result[4].first, Token::kRArrow);
    EXPECT_EQ(result[4].second, "->");
}

// 测试用例 19: 标识符与关键字混合
TEST_F(LexserTest, MixedIdentifiersAndKeywords) {
    std::string input = "function fn let_var const_fn matchmaker";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[0].second, "function");
    EXPECT_EQ(result[1].first, Token::kfn);
    EXPECT_EQ(result[1].second, "fn");
    EXPECT_EQ(result[2].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[2].second, "let_var");
    EXPECT_EQ(result[3].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[3].second, "const_fn");
    EXPECT_EQ(result[4].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[4].second, "matchmaker");
}

// 测试用例 20: 非法数字字面量 (测试错误恢复)
TEST_F(LexserTest, InvalidNumberLiterals) {
    std::string input = "0b123 0o79 123abc";
    auto result = lexer.lexString(input);
    // for (auto token : result) {
    //     std::cout << to_string(token.first) << " " << token.second << "\n";
    // }
    // 这些应该被识别为整数字面量加上标识符
    ASSERT_EQ(result.size(), 6);
    EXPECT_EQ(result[0].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[0].second, "0b1");
    EXPECT_EQ(result[1].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[1].second, "23");
    EXPECT_EQ(result[2].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[2].second, "0o7");
    EXPECT_EQ(result[3].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[3].second, "9");
    EXPECT_EQ(result[4].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[4].second, "123");
    EXPECT_EQ(result[5].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[5].second, "abc");
    // 注意: 这里的 "abc" 可能被识别为标识符，但取决于正则表达式的实现
}

// 测试用例 21: 未闭合的字符串字面量 (测试错误恢复)
TEST_F(LexserTest, UnclosedStringLiteral) {
    std::string input = "\"unclosed string // Comment\nlet x = 5;";
    auto result = lexer.lexString(input);
    // for (auto token : result) {
    //     std::cout << to_string(token.first) << " " << token.second << "\n";
    // }
    // 这应该被识别为字符串字面量（直到行尾），然后是注释，然后是代码
    ASSERT_GE(result.size(), 8);
    EXPECT_EQ(result[0].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[1].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[2].first, Token::kCOMMENT);
    EXPECT_EQ(result[3].first, Token::klet);
    EXPECT_EQ(result[4].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[5].first, Token::kEq);
    EXPECT_EQ(result[6].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[7].first, Token::kSemi);
    // 注意: 实际结果取决于正则表达式如何处理未闭合的字符串
}

// 测试用例 22: 复杂转义序列
TEST_F(LexserTest, ComplexEscapeSequences) {
    std::string input = "\"\\n\\t\\r\\\\\\'\\\"\" \"\\n\\t\\r\\\\\\'\\\"\" '\\\''";
    auto result = lexer.lexString(input);
    // for (auto token : result) {
    //     std::cout << to_string(token.first) << " " << token.second << "\n";
    // }
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].first, Token::kSTRING_LITERAL);
    EXPECT_EQ(result[1].first, Token::kSTRING_LITERAL);
    EXPECT_EQ(result[2].first, Token::kCHAR_LITERAL);
}

// 测试用例 23: 嵌套注释 (测试错误恢复)
TEST_F(LexserTest, NestedComments) {
    std::string input = "/* Outer /* Inner */ comment */ code";
    auto result = lexer.lexString(input);
    // 这应该被识别为一个注释（可能不正确嵌套），然后是代码
    ASSERT_GE(result.size(), 2);
    EXPECT_EQ(result[0].first, Token::kCOMMENT);
    EXPECT_EQ(result[1].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[1].second, "code");
}

// 测试用例 24: 混合空格和制表符
TEST_F(LexserTest, MixedWhitespace) {
    std::string input = "  \t \n  let\tx\n=\t1;";
    auto result = lexer.lexString(input);
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0].first, Token::klet);
    EXPECT_EQ(result[1].first, Token::kIDENTIFIER);
    EXPECT_EQ(result[1].second, "x");
    EXPECT_EQ(result[2].first, Token::kEq);
    EXPECT_EQ(result[3].first, Token::kINTEGER_LITERAL);
    EXPECT_EQ(result[3].second, "1");
    EXPECT_EQ(result[4].first, Token::kSemi);
}

// 测试用例 25: 完整程序片段
TEST_F(LexserTest, CompleteProgramFragment) {
    std::string input = R"(
        fn main() {
            let mut x = 5;
            let y = if x > 0 {
                "positive"
            } else {
                "non-positive"
            };
            println!("x is {}", y);
        }
    )";
    
    auto result = lexer.lexString(input);
    // 应该正确识别所有token
    ASSERT_GT(result.size(), 20);
    
    // 检查一些关键token
    bool has_fn = false, has_main = false, has_let = false, has_if = false;
    for (const auto& token : result) {
        if (token.first == Token::kfn) has_fn = true;
        if (token.first == Token::kIDENTIFIER && token.second == "main") has_main = true;
        if (token.first == Token::klet) has_let = true;
        if (token.first == Token::kif) has_if = true;
    }
    
    EXPECT_TRUE(has_fn);
    EXPECT_TRUE(has_main);
    EXPECT_TRUE(has_let);
    EXPECT_TRUE(has_if);
}
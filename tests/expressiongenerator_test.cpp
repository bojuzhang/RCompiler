#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include "expressiongenerator.hpp"
#include "irbuilder.hpp"
#include "typemapper.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include "astnodes.hpp"

class ExpressionGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建输出流
        outputStream = std::make_shared<std::ostringstream>();
        
        // 创建作用域树
        scopeTree = std::make_shared<ScopeTree>();
        
        // 创建类型映射器
        typeMapper = std::make_shared<TypeMapper>(scopeTree);
        
        // 创建 IRBuilder
        irBuilder = std::make_shared<IRBuilder>(*outputStream, scopeTree);
        
        // 创建空的节点类型映射
        std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
        
        // 创建 ExpressionGenerator
        exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    }
    
    void TearDown() override {
        exprGen.reset();
        irBuilder.reset();
        typeMapper.reset();
        scopeTree.reset();
        outputStream.reset();
    }
    
    std::shared_ptr<std::ostringstream> outputStream;
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<IRBuilder> irBuilder;
    std::unique_ptr<ExpressionGenerator> exprGen;
};

TEST_F(ExpressionGeneratorTest, LiteralExpression_Integer) {
    // 创建整数字面量表达式
    auto literalExpr = std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL);
    
    // 设置表达式类型
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    nodeTypeMap[literalExpr.get()] = std::make_shared<SimpleType>("i32");
    
    // 重新创建 ExpressionGenerator 以包含类型映射
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(literalExpr);
    
    // 打印实际输出用于调试
    std::cout << "Integer literal result: '" << result << "'" << std::endl;
    std::cout << "Full IR output: '" << outputStream->str() << "'" << std::endl;
    
    // 验证结果
    EXPECT_FALSE(result.empty());
    // 检查寄存器名格式
    EXPECT_TRUE(result[0] == '%');
    // 检查输出流中是否包含生成的指令
    EXPECT_TRUE(outputStream->str().find("42") != std::string::npos);
    
    // 检查是否有错误
    EXPECT_FALSE(exprGen->hasError());
}

TEST_F(ExpressionGeneratorTest, LiteralExpression_Boolean) {
    // 创建布尔字面量表达式
    auto literalExpr = std::make_shared<LiteralExpression>("true", Token::ktrue);
    
    // 设置表达式类型
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    nodeTypeMap[literalExpr.get()] = std::make_shared<SimpleType>("i1");
    
    // 重新创建 ExpressionGenerator
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(literalExpr);
    
    // 打印实际输出用于调试
    std::cout << "Boolean literal result: '" << result << "'" << std::endl;
    std::cout << "Full IR output: '" << outputStream->str() << "'" << std::endl;
    
    // 验证结果
    EXPECT_FALSE(result.empty());
    // 检查寄存器名格式
    EXPECT_TRUE(result[0] == '%');
    // 检查输出流中是否包含生成的指令
    EXPECT_TRUE(outputStream->str().find("1") != std::string::npos);
    
    // 检查是否有错误
    EXPECT_FALSE(exprGen->hasError());
}

TEST_F(ExpressionGeneratorTest, PathExpression_Variable) {
    // 创建变量符号
    auto currentScope = scopeTree->GetCurrentScope();
    auto varSymbol = std::make_shared<Symbol>("x", SymbolKind::Variable,
                                           std::make_shared<SimpleType>("i32"));
    currentScope->Insert("x", varSymbol);
    
    // 在 IRBuilder 中注册变量
    std::string varReg = irBuilder->newRegister("x"); // 为变量创建寄存器
    irBuilder->emitAlloca("i32"); // 为变量分配空间
    // 注意：IRBuilder 会自动管理变量寄存器映射
    
    // 创建路径表达式
    auto pathSeg = std::make_shared<SimplePathSegment>("x", false, false);
    auto path = std::make_shared<SimplePath>(std::vector<std::shared_ptr<SimplePathSegment>>{pathSeg});
    auto pathExpr = std::make_shared<PathExpression>(path);
    
    // 设置表达式类型
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    nodeTypeMap[pathExpr.get()] = std::make_shared<SimpleType>("i32");
    
    // 重新创建 ExpressionGenerator
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(pathExpr);
    
    // 打印实际输出用于调试
    std::cout << "Path expression result: '" << result << "'" << std::endl;
    std::cout << "Full IR output: '" << outputStream->str() << "'" << std::endl;
    
    // 验证结果
    EXPECT_FALSE(result.empty());
    // 检查寄存器名格式
    EXPECT_TRUE(result[0] == '%');
    // 检查输出流中是否包含加载指令
    EXPECT_TRUE(outputStream->str().find("load") != std::string::npos);
    
    // 检查是否有错误
    EXPECT_FALSE(exprGen->hasError());
}

TEST_F(ExpressionGeneratorTest, BinaryExpression_Addition) {
    // 创建二元加法表达式: 5 + 3
    auto leftLiteral = std::make_shared<LiteralExpression>("5", Token::kINTEGER_LITERAL);
    auto rightLiteral = std::make_shared<LiteralExpression>("3", Token::kINTEGER_LITERAL);
    auto binaryExpr = std::make_shared<BinaryExpression>(leftLiteral, rightLiteral, Token::kPlus);
    
    // 设置表达式类型
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    nodeTypeMap[leftLiteral.get()] = std::make_shared<SimpleType>("i32");
    nodeTypeMap[rightLiteral.get()] = std::make_shared<SimpleType>("i32");
    nodeTypeMap[binaryExpr.get()] = std::make_shared<SimpleType>("i32");
    
    // 重新创建 ExpressionGenerator
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(binaryExpr);
    
    // 打印实际输出用于调试
    std::cout << "Binary expression result: '" << result << "'" << std::endl;
    std::cout << "Full IR output: '" << outputStream->str() << "'" << std::endl;
    
    // 验证结果
    EXPECT_FALSE(result.empty());
    // 检查寄存器名格式
    EXPECT_TRUE(result[0] == '%');
    // 检查输出流中是否包含加法指令
    EXPECT_TRUE(outputStream->str().find("add i32") != std::string::npos);
    
    // 检查是否有错误
    EXPECT_FALSE(exprGen->hasError());
}

TEST_F(ExpressionGeneratorTest, TypeCastExpression) {
    // 创建类型转换表达式: (i32)some_value
    auto innerExpr = std::make_shared<LiteralExpression>("42", Token::kINTEGER_LITERAL);
    auto targetType = std::make_shared<TypePath>(std::make_shared<SimplePathSegment>("i32", false, false));
    auto typeCastExpr = std::make_shared<TypeCastExpression>(innerExpr, targetType);
    
    // 设置表达式类型
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    nodeTypeMap[innerExpr.get()] = std::make_shared<SimpleType>("i64");
    nodeTypeMap[typeCastExpr.get()] = std::make_shared<SimpleType>("i32");
    
    // 重新创建 ExpressionGenerator
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(typeCastExpr);
    
    // 打印实际输出用于调试
    std::cout << "Type cast result: '" << result << "'" << std::endl;
    std::cout << "Full IR output: '" << outputStream->str() << "'" << std::endl;
    
    // 验证结果
    EXPECT_FALSE(result.empty());
    // 检查寄存器名格式
    EXPECT_TRUE(result[0] == '%');
    // 检查输出流中是否包含类型转换指令
    EXPECT_TRUE(outputStream->str().find("trunc") != std::string::npos ||
               outputStream->str().find("sext") != std::string::npos ||
               outputStream->str().find("zext") != std::string::npos);
    
    // 检查是否有错误
    EXPECT_FALSE(exprGen->hasError());
}

TEST_F(ExpressionGeneratorTest, ErrorHandling_NullExpression) {
    // 测试空表达式的错误处理
    std::string result = exprGen->generateExpression(nullptr);
    
    // 验证结果
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(exprGen->hasError());
    
    // 检查错误信息
    auto errors = exprGen->getErrorMessages();
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(errors[0].find("Expression is null") != std::string::npos);
}

TEST_F(ExpressionGeneratorTest, ErrorHandling_UnsupportedExpression) {
    // 创建一个不支持的表达式类型（通过模拟）
    auto unsupportedExpr = std::make_shared<LiteralExpression>("test", Token::kSTRING_LITERAL);
    
    // 故意不设置类型映射以触发错误
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    
    // 重新创建 ExpressionGenerator
    exprGen = std::make_unique<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
    
    // 生成表达式
    std::string result = exprGen->generateExpression(unsupportedExpr);
    
    // 验证结果（应该有某种输出，即使可能不完整）
    // 字符串字面量应该能正常处理
    EXPECT_FALSE(result.empty());
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
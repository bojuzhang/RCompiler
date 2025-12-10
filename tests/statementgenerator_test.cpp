#include <iostream>
#include <sstream>
#include <cassert>
#include <memory>
#include <vector>
#include <functional>
#include "statementgenerator.hpp"
#include "irbuilder.hpp"
#include "typemapper.hpp"
#include "scope.hpp"
#include "symbol.hpp"

// 前向声明
class ExpressionGenerator;

// 简单的测试框架
class TestRunner {
private:
    int totalTests = 0;
    int passedTests = 0;
    
public:
    void runTest(const std::string& testName, std::function<void()> testFunc) {
        totalTests++;
        std::cout << "Running test: " << testName << "... ";
        
        try {
            testFunc();
            passedTests++;
            std::cout << "PASSED" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "FAILED: Unknown exception" << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\nTest Summary: " << passedTests << "/" << totalTests << " tests passed" << std::endl;
        if (passedTests == totalTests) {
            std::cout << "All tests PASSED!" << std::endl;
        } else {
            std::cout << "Some tests FAILED!" << std::endl;
        }
    }
};

// 测试辅助函数
void assertContains(const std::string& str, const std::string& substr) {
    if (str.find(substr) == std::string::npos) {
        throw std::runtime_error("String '" + str + "' does not contain '" + substr + "'");
    }
}

void assertEquals(const std::string& expected, const std::string& actual) {
    if (expected != actual) {
        throw std::runtime_error("Expected '" + expected + "', but got '" + actual + "'");
    }
}

void assertEquals(ExpressionGenerator* expected, ExpressionGenerator* actual) {
    if (expected != actual) {
        throw std::runtime_error("Expected pointer mismatch");
    }
}

void assertNotEquals(const std::string& expected, const std::string& actual) {
    if (expected == actual) {
        throw std::runtime_error("Expected not '" + expected + "', but got '" + actual + "'");
    }
}

// 创建测试用的作用域树
std::shared_ptr<ScopeTree> createTestScopeTree() {
    auto scopeTree = std::make_shared<ScopeTree>();
    scopeTree->EnterScope(Scope::ScopeType::Global);
    return scopeTree;
}

// 创建测试用的 StatementGenerator
std::shared_ptr<StatementGenerator> createTestStatementGenerator(std::ostringstream& output) {
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree, nullptr);
    auto typeMapper = irBuilder->getTypeMapper();
    return std::make_shared<StatementGenerator>(irBuilder, typeMapper, scopeTree);
}

// 测试构造函数和初始化
void testConstructor() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试构造成功
    assert(stmtGen != nullptr);
    
    // 测试初始状态
    assert(!stmtGen->hasError());
    assert(stmtGen->getErrorMessages().empty());
    assert(!stmtGen->isInLoopContext());
}

// 测试 ExpressionGenerator 集成
void testExpressionGeneratorIntegration() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试初始状态
    assertEquals(nullptr, stmtGen->getExpressionGenerator());
    
    // 测试设置 ExpressionGenerator
    stmtGen->setExpressionGenerator(nullptr);
    assertEquals(nullptr, stmtGen->getExpressionGenerator());
}

// 测试控制流上下文管理
void testControlFlowContextManagement() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试初始状态
    assert(!stmtGen->isInLoopContext());
    
    // 进入控制流上下文
    stmtGen->enterControlFlowContext("loop_header", "loop_body", "loop_exit");
    
    // 验证状态
    assert(stmtGen->isInLoopContext());
    
    // 测试获取目标
    try {
        std::string breakTarget = stmtGen->getCurrentBreakTarget();
        assertEquals("loop_exit", breakTarget);
        
        std::string continueTarget = stmtGen->getCurrentContinueTarget();
        assertEquals("loop_header", continueTarget);
    } catch (const std::exception& e) {
        throw std::runtime_error("Should not throw exception: " + std::string(e.what()));
    }
    
    // 添加额外的目标
    stmtGen->addBreakTarget("extra_break");
    stmtGen->addContinueTarget("extra_continue");
    
    // 验证获取最新的目标
    std::string breakTarget = stmtGen->getCurrentBreakTarget();
    assertEquals("extra_break", breakTarget);
    
    std::string continueTarget = stmtGen->getCurrentContinueTarget();
    assertEquals("extra_continue", continueTarget);
    
    // 退出控制流上下文
    stmtGen->exitControlFlowContext();
    
    // 验证状态
    assert(!stmtGen->isInLoopContext());
}

// 测试变量管理
void testVariableManagement() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试变量分配
    std::string varReg = stmtGen->allocateVariable("test_var", "i32");
    assertNotEquals("", varReg);
    
    // 测试变量在作用域中
    assert(stmtGen->isVariableInCurrentScope("test_var"));
    
    // 测试变量存储
    std::string valueReg = "%_1"; // 模拟值寄存器
    stmtGen->storeVariable("test_var", valueReg, "i32");
    
    // 验证输出包含存储指令
    std::string outputStr = output.str();
    assertContains(outputStr, "alloca");
    
    // 测试变量加载
    std::string loadedReg = stmtGen->loadVariable("test_var");
    assertNotEquals("", loadedReg);
}

// 测试错误处理
void testErrorHandling() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试初始状态
    assert(!stmtGen->hasError());
    assert(stmtGen->getErrorMessages().empty());
    
    // 测试错误报告
    stmtGen->reportError("Test error message");
    
    // 验证错误状态
    assert(stmtGen->hasError());
    auto errors = stmtGen->getErrorMessages();
    assert(1 == errors.size());
    assertEquals("Test error message", errors[0]);
    
    // 测试错误清除
    stmtGen->clearErrors();
    assert(!stmtGen->hasError());
    assert(stmtGen->getErrorMessages().empty());
}

// 测试控制流上下文异常处理
void testControlFlowContextExceptions() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试在没有循环上下文时获取 break 目标
    try {
        stmtGen->getCurrentBreakTarget();
        throw std::runtime_error("Should have thrown exception");
    } catch (const std::exception& e) {
        assertContains(e.what(), "No loop context");
    }
    
    // 测试在没有循环上下文时获取 continue 目标
    try {
        stmtGen->getCurrentContinueTarget();
        throw std::runtime_error("Should have thrown exception");
    } catch (const std::exception& e) {
        assertContains(e.what(), "No loop context");
    }
}

// 测试语句类型识别（简化版本，不使用复杂的 AST 节点）
void testStatementTypeIdentification() {
    std::ostringstream output;
    auto stmtGen = createTestStatementGenerator(output);
    
    // 测试空语句
    auto emptyStatement = std::make_shared<Statement>(nullptr);
    std::string stmtType = stmtGen->getStatementType(emptyStatement);
    assertEquals("unknown", stmtType);
}

int main() {
    TestRunner runner;
    
    std::cout << "=== StatementGenerator Test Suite ===" << std::endl;
    
    // 运行所有测试
    runner.runTest("Constructor and Initialization", testConstructor);
    runner.runTest("ExpressionGenerator Integration", testExpressionGeneratorIntegration);
    runner.runTest("Control Flow Context Management", testControlFlowContextManagement);
    runner.runTest("Variable Management", testVariableManagement);
    runner.runTest("Error Handling", testErrorHandling);
    runner.runTest("Control Flow Context Exceptions", testControlFlowContextExceptions);
    runner.runTest("Statement Type Identification", testStatementTypeIdentification);
    
    // 打印测试结果
    runner.printSummary();
    
    return 0;
}
#include <iostream>
#include <sstream>
#include <cassert>
#include <memory>
#include "irbuilder.hpp"
#include "scope.hpp"
#include "symbol.hpp"

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

// 创建测试用的作用域树
std::shared_ptr<ScopeTree> createTestScopeTree() {
    auto scopeTree = std::make_shared<ScopeTree>();
    scopeTree->EnterScope(Scope::ScopeType::Global);
    return scopeTree;
}

// 测试寄存器管理
void testRegisterManagement() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试临时寄存器生成
    std::string reg1 = builder.newRegister();
    assertContains(reg1, "%");
    
    // 测试变量寄存器生成
    std::string varReg = builder.newRegister("x", "_ptr");
    assertContains(varReg, "%x");
    assertContains(varReg, "_ptr");
    
    // 测试寄存器类型设置和获取
    builder.setRegisterType(reg1, "i32");
    assertEquals("i32", builder.getRegisterType(reg1));
    
    // 测试变量寄存器查找
    std::string xReg = builder.getVariableRegister("x");
    assertEquals(varReg, xReg);
}

// 测试基本块管理
void testBasicBlockManagement() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试基本块生成
    std::string bb1 = builder.newBasicBlock("test");
    assertContains(bb1, "test");
    
    // 测试基本块设置
    builder.setCurrentBasicBlock(bb1);
    assertEquals(bb1, builder.getCurrentBasicBlock());
    
    // 测试基本块存在性检查
    assert(builder.isBasicBlockExists(bb1));
}

// 测试指令生成
void testInstructionGeneration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试目标三元组输出
    builder.emitTargetTriple();
    std::string result = output.str();
    assertContains(result, "target triple");
    
    // 测试注释输出
    builder.emitComment("Test comment");
    result = output.str();
    assertContains(result, "; Test comment");
    
    // 测试标签输出
    builder.emitLabel("test_label");
    result = output.str();
    assertContains(result, "test_label:");
}

// 测试类型映射
void testTypeMapping() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试基本类型映射
    auto intType = std::make_shared<SimpleType>("i32");
    std::string llvmType = builder.mapRxTypeToLLVM(intType);
    assertEquals("i32", llvmType);
    
    auto boolType = std::make_shared<SimpleType>("bool");
    llvmType = builder.mapRxTypeToLLVM(boolType);
    assertEquals("i1", llvmType);
    
    // 测试类型检查
    assert(builder.isIntegerType("i32"));
    assert(builder.isPointerType("i32*"));
    assert(!builder.isPointerType("i32"));
    
    // 测试类型大小
    assert(4 == builder.getTypeSize("i32"));
    assert(1 == builder.getTypeSize("i1"));
}

// 测试算术指令
void testArithmeticInstructions() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 创建操作数寄存器
    std::string reg1 = builder.newRegister();
    std::string reg2 = builder.newRegister();
    builder.setRegisterType(reg1, "i32");
    builder.setRegisterType(reg2, "i32");
    
    // 测试加法指令
    std::string result = builder.emitAdd(reg1, reg2, "i32");
    assertContains(result, "%");
    
    std::string outputStr = output.str();
    assertContains(outputStr, "add i32");
    
    // 测试减法指令
    result = builder.emitSub(reg1, reg2, "i32");
    outputStr = output.str();
    assertContains(outputStr, "sub i32");
}

// 测试比较指令
void testComparisonInstructions() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 创建操作数寄存器
    std::string reg1 = builder.newRegister();
    std::string reg2 = builder.newRegister();
    builder.setRegisterType(reg1, "i32");
    builder.setRegisterType(reg2, "i32");
    
    // 测试比较指令
    std::string result = builder.emitIcmp("eq", reg1, reg2, "i32");
    assertContains(result, "%");
    
    std::string outputStr = output.str();
    assertContains(outputStr, "icmp eq i32");
    
    // 测试结果类型
    assertEquals("i1", builder.getRegisterType(result));
}

// 测试内存指令
void testMemoryInstructions() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试alloca指令
    std::string ptr = builder.emitAlloca("i32", 4);
    assertContains(ptr, "%");
    
    std::string outputStr = output.str();
    assertContains(outputStr, "alloca i32, align 4");
    
    // 测试store指令
    std::string value = builder.newRegister();
    builder.setRegisterType(value, "i32");
    builder.emitStore(value, ptr);
    outputStr = output.str();
    assertContains(outputStr, "store i32");
    
    // 测试load指令
    std::string loaded = builder.emitLoad(ptr, "i32");
    assertContains(loaded, "%");
    outputStr = output.str();
    assertContains(outputStr, "load i32");
}

// 测试控制流指令
void testControlFlowInstructions() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 创建基本块
    std::string bb1 = builder.newBasicBlock("test");
    std::string bb2 = builder.newBasicBlock("target");
    
    builder.setCurrentBasicBlock(bb1);
    
    // 测试无条件跳转
    builder.emitBr(bb2);
    std::string outputStr = output.str();
    assertContains(outputStr, "br label %" + bb2);
    
    // 测试条件跳转
    std::string cond = builder.newRegister();
    builder.setRegisterType(cond, "i1");
    
    std::string bb3 = builder.newBasicBlock("false_target");
    builder.emitCondBr(cond, bb2, bb3);
    outputStr = output.str();
    assertContains(outputStr, "br i1");
    assertContains(outputStr, "label %" + bb2);
    assertContains(outputStr, "label %" + bb3);
    
    // 测试返回指令
    builder.emitRetVoid();
    outputStr = output.str();
    assertContains(outputStr, "ret void");
}

// 测试函数指令
void testFunctionInstructions() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试函数声明
    std::vector<std::string> params = {"i32 %a", "i32 %b"};
    builder.emitFunctionDecl("test_func", "i32", params);
    std::string outputStr = output.str();
    assertContains(outputStr, "declare dso_local i32 @test_func(i32 %a, i32 %b)");
    
    // 测试函数定义
    builder.emitFunctionDef("main", "i32", {});
    outputStr = output.str();
    assertContains(outputStr, "define i32 @main() {");
    
    // 测试函数结束
    builder.emitFunctionEnd();
    outputStr = output.str();
    assertContains(outputStr, "}");
}

// 测试错误处理
void testErrorHandling() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试初始状态
    assert(!builder.hasError());
    
    // 测试错误报告
    builder.reportError("Test error");
    assert(builder.hasError());
    
    auto errors = builder.getErrorMessages();
    assert(1 == errors.size());
    assertContains(errors[0], "Test error");
    
    // 测试错误清除
    builder.clearErrors();
    assert(!builder.hasError());
}

// 测试作用域管理
void testScopeManagement() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    IRBuilder builder(output, scopeTree);
    
    // 测试初始作用域
    auto currentScope = builder.getCurrentScope();
    assert(currentScope != nullptr);
    
    // 测试变量寄存器在作用域中的管理
    std::string varReg = builder.newRegister("test_var");
    assert(builder.isVariableInCurrentScope("test_var"));
    
    // 测试变量寄存器查找
    std::string foundReg = builder.getVariableRegister("test_var");
    assertEquals(varReg, foundReg);
}

int main() {
    TestRunner runner;
    
    std::cout << "=== IRBuilder Test Suite ===" << std::endl;
    
    // 运行所有测试
    runner.runTest("Register Management", testRegisterManagement);
    runner.runTest("Basic Block Management", testBasicBlockManagement);
    runner.runTest("Instruction Generation", testInstructionGeneration);
    runner.runTest("Type Mapping", testTypeMapping);
    runner.runTest("Arithmetic Instructions", testArithmeticInstructions);
    runner.runTest("Comparison Instructions", testComparisonInstructions);
    runner.runTest("Memory Instructions", testMemoryInstructions);
    runner.runTest("Control Flow Instructions", testControlFlowInstructions);
    runner.runTest("Function Instructions", testFunctionInstructions);
    runner.runTest("Error Handling", testErrorHandling);
    runner.runTest("Scope Management", testScopeManagement);
    
    // 打印测试结果
    runner.printSummary();
    
    return 0;
}
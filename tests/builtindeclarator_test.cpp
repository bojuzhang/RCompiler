#include <iostream>
#include <sstream>
#include <cassert>
#include <memory>
#include <functional>
#include "builtindeclarator.hpp"
#include "irbuilder.hpp"
#include "scope.hpp"

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

void assertTrue(bool condition, const std::string& message = "") {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

void assertFalse(bool condition, const std::string& message = "") {
    if (condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

// 创建测试用的作用域树
std::shared_ptr<ScopeTree> createTestScopeTree() {
    auto scopeTree = std::make_shared<ScopeTree>();
    scopeTree->EnterScope(Scope::ScopeType::Global);
    return scopeTree;
}

// 测试基本构造函数
void testConstructor() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    assertFalse(declarator.hasDeclarationErrors());
    
    auto errors = declarator.getErrorMessages();
    assertTrue(errors.empty());
}

// 测试标准内置函数注册
void testStandardBuiltinRegistration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 测试标准内置函数是否被正确注册
    assertTrue(declarator.isBuiltinFunction("print"));
    assertTrue(declarator.isBuiltinFunction("println"));
    assertTrue(declarator.isBuiltinFunction("printInt"));
    assertTrue(declarator.isBuiltinFunction("printlnInt"));
    assertTrue(declarator.isBuiltinFunction("getString"));
    assertTrue(declarator.isBuiltinFunction("getInt"));
    assertTrue(declarator.isBuiltinFunction("malloc"));
    assertTrue(declarator.isBuiltinFunction("builtin_memset"));
    assertTrue(declarator.isBuiltinFunction("builtin_memcpy"));
    assertTrue(declarator.isBuiltinFunction("exit"));
    
    // 测试不存在的函数
    assertFalse(declarator.isBuiltinFunction("nonexistent"));
}

// 测试批量声明功能
void testBatchDeclaration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 批量声明所有内置函数
    declarator.declareBuiltinFunctions();
    
    std::string result = output.str();
    
    // 验证输出包含期望的声明
    assertContains(result, "declare dso_local void @print(ptr nocapture readonly)");
    assertContains(result, "declare dso_local void @printlnInt(i32 signext)");
    assertContains(result, "declare dso_local ptr @malloc(i32)");
    assertContains(result, "declare dso_local void @exit(i32) noreturn");
}

// 测试单个函数声明
void testSingleFunctionDeclaration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 声明单个函数
    bool result = declarator.declareBuiltinFunction("printInt");
    
    assertTrue(result);
    assertTrue(declarator.isFunctionDeclared("printInt"));
    
    std::string outputStr = output.str();
    assertContains(outputStr, "declare dso_local void @printInt(i32 signext)");
}

// 测试函数类型查询
void testFunctionTypeQuery() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 测试返回类型
    assertEquals("void", declarator.getBuiltinReturnType("print"));
    assertEquals("ptr", declarator.getBuiltinReturnType("getString"));
    assertEquals("i32", declarator.getBuiltinReturnType("getInt"));
    
    // 测试参数类型
    auto printParams = declarator.getBuiltinParameterTypes("print");
    assertTrue(printParams.size() == 1);
    assertEquals("ptr", printParams[0]);
    
    auto memcpyParams = declarator.getBuiltinParameterTypes("builtin_memcpy");
    assertTrue(memcpyParams.size() == 3);
    assertEquals("ptr", memcpyParams[0]);
    assertEquals("ptr", memcpyParams[1]);
    assertEquals("i32", memcpyParams[2]);
}

// 测试类型检查功能
void testTypeChecking() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 测试正确的函数调用
    assertTrue(declarator.isValidBuiltinCall("print", {"ptr"}));
    assertTrue(declarator.isValidBuiltinCall("printInt", {"i32"}));
    assertTrue(declarator.isValidBuiltinCall("builtin_memset", {"ptr", "i8", "i32"}));
    
    // 测试类型兼容性（指针类型兼容）
    assertTrue(declarator.isValidBuiltinCall("print", {"i8*"}));
    
    // 测试整数类型兼容性
    assertTrue(declarator.isValidBuiltinCall("printInt", {"i64"}));
    
    // 测试错误的函数调用
    assertFalse(declarator.isValidBuiltinCall("print", {"i32"})); // 类型不匹配
    assertFalse(declarator.isValidBuiltinCall("printInt", {})); // 参数数量不足
    assertFalse(declarator.isValidBuiltinCall("nonexistent", {"ptr"})); // 函数不存在
}

// 测试错误信息
void testErrorMessages() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 测试类型不匹配错误
    std::string error = declarator.getBuiltinCallError("print", {"i32"});
    assertContains(error, "Type mismatch");
    
    // 测试参数数量错误
    error = declarator.getBuiltinCallError("printInt", {});
    assertContains(error, "Parameter count mismatch");
    
    // 测试未知函数错误
    error = declarator.getBuiltinCallError("nonexistent", {"ptr"});
    assertContains(error, "Unknown builtin function");
}

// 测试结构体函数声明
void testStructFunctionDeclaration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 声明结构体函数
    std::vector<std::string> fieldTypes = {"i32", "ptr", "i8"};
    declarator.declareStructFunctions("Person", fieldTypes);
    
    std::string result = output.str();
    
    // 验证构造函数声明
    assertContains(result, "declare dso_local %Person @struct_Person_new(i32, ptr, i8)");
    
    // 验证析构函数声明（因为包含ptr字段）
    assertContains(result, "declare dso_local void @struct_Person_drop(%Person*)");
    
    // 验证比较函数声明
    assertContains(result, "declare dso_local i1 @struct_Person_eq(%Person* nocapture readonly, %Person* nocapture readonly)");
}

// 测试自定义函数注册
void testCustomFunctionRegistration() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 注册自定义函数
    std::vector<ParameterAttribute> params = {
        ParameterAttribute("i32", "signext"),
        ParameterAttribute("ptr", "nocapture readonly")
    };
    declarator.registerCustomBuiltin("customFunc", "i64", params, false, "readonly");
    
    // 验证函数被注册
    assertTrue(declarator.isBuiltinFunction("customFunc"));
    assertEquals("i64", declarator.getBuiltinReturnType("customFunc"));
    
    // 验证参数类型
    auto customParams = declarator.getBuiltinParameterTypes("customFunc");
    assertTrue(customParams.size() == 2);
    assertEquals("i32", customParams[0]);
    assertEquals("ptr", customParams[1]);
}

// 测试声明状态管理
void testDeclarationStateManagement() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 声明一个函数
    declarator.declareBuiltinFunction("print");
    assertTrue(declarator.isFunctionDeclared("print"), "print 应该被标记为已声明");
    
    // 尝试再次声明同一个函数
    declarator.declareBuiltinFunction("print");
    assertTrue(declarator.isFunctionDeclared("print"), "print 仍然应该被标记为已声明");
    
    // 清理声明状态
    declarator.clearDeclarationCache();
    assertFalse(declarator.isFunctionDeclared("print"), "清理后 print 不应该被标记为已声明");
}

// 测试错误处理
void testErrorHandling() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 测试无效函数名
    assertFalse(declarator.declareBuiltinFunction(""));
    assertFalse(declarator.declareBuiltinFunction("123invalid"));
    
    // 检查是否有错误
    assertTrue(declarator.hasDeclarationErrors());
    
    auto errors = declarator.getErrorMessages();
    assertFalse(errors.empty());
    
    // 清理错误
    declarator.clearErrors();
    assertFalse(declarator.hasDeclarationErrors());
}

// 测试重复声明警告
void testDuplicateDeclarationWarning() {
    std::ostringstream output;
    auto scopeTree = createTestScopeTree();
    auto irBuilder = std::make_shared<IRBuilder>(output, scopeTree);
    
    BuiltinDeclarator declarator(irBuilder);
    
    // 清空警告
    declarator.clearErrors();
    
    // 声明函数
    declarator.declareBuiltinFunction("print");
    
    // 再次声明同一函数
    declarator.declareBuiltinFunction("print");
    
    // 检查警告
    auto warnings = declarator.getWarningMessages();
    assertFalse(warnings.empty());
    
    bool foundWarning = false;
    for (const auto& warning : warnings) {
        if (warning.find("already declared") != std::string::npos) {
            foundWarning = true;
            break;
        }
    }
    assertTrue(foundWarning);
}

int main() {
    TestRunner runner;
    
    std::cout << "=== BuiltinDeclarator Test Suite ===" << std::endl;
    
    // 运行所有测试
    runner.runTest("Constructor", testConstructor);
    runner.runTest("Standard Builtin Registration", testStandardBuiltinRegistration);
    runner.runTest("Batch Declaration", testBatchDeclaration);
    runner.runTest("Single Function Declaration", testSingleFunctionDeclaration);
    runner.runTest("Function Type Query", testFunctionTypeQuery);
    runner.runTest("Type Checking", testTypeChecking);
    runner.runTest("Error Messages", testErrorMessages);
    runner.runTest("Struct Function Declaration", testStructFunctionDeclaration);
    runner.runTest("Custom Function Registration", testCustomFunctionRegistration);
    runner.runTest("Declaration State Management", testDeclarationStateManagement);
    runner.runTest("Error Handling", testErrorHandling);
    runner.runTest("Duplicate Declaration Warning", testDuplicateDeclarationWarning);
    
    // 打印测试结果
    runner.printSummary();
    
    return 0;
}
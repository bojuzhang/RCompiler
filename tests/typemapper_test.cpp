#include <iostream>
#include <memory>
#include <cassert>
#include <vector>
#include "typemapper.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include "typewrapper.hpp"
#include "astnodes.hpp"
#include "lexer.hpp"

// 简单的测试框架
class TypeMapperTest {
private:
    int testCount = 0;
    int passedCount = 0;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;

public:
    TypeMapperTest() {
        // 创建测试用的作用域树
        scopeTree = std::make_shared<ScopeTree>();
        typeMapper = std::make_shared<TypeMapper>(scopeTree);
    }

    void runTest(const std::string& testName, bool condition) {
        testCount++;
        if (condition) {
            passedCount++;
            std::cout << "[PASS] " << testName << std::endl;
        } else {
            std::cout << "[FAIL] " << testName << std::endl;
        }
    }

    void testBasicTypeMapping() {
        std::cout << "\n=== 测试基础类型映射 ===" << std::endl;
        
        runTest("i32 -> i32", typeMapper->mapRxTypeToLLVM("i32") == "i32");
        runTest("i64 -> i64", typeMapper->mapRxTypeToLLVM("i64") == "i64");
        runTest("u32 -> i32", typeMapper->mapRxTypeToLLVM("u32") == "i32");
        runTest("bool -> i1", typeMapper->mapRxTypeToLLVM("bool") == "i1");
        runTest("char -> i8", typeMapper->mapRxTypeToLLVM("char") == "i8");
        runTest("str -> i8*", typeMapper->mapRxTypeToLLVM("str") == "i8*");
        runTest("unit -> void", typeMapper->mapRxTypeToLLVM("unit") == "void");
    }

    void testSemanticTypeMapping() {
        std::cout << "\n=== 测试语义类型映射 ===" << std::endl;
        
        // 测试特殊语义类型
        auto intType = std::make_shared<IntType>();
        runTest("IntType -> i64", typeMapper->mapSemanticTypeToLLVM(intType) == "i64");
        
        auto signedIntType = std::make_shared<SignedIntType>();
        runTest("SignedIntType -> i32", typeMapper->mapSemanticTypeToLLVM(signedIntType) == "i32");
        
        auto unsignedIntType = std::make_shared<UnsignedIntType>();
        runTest("UnsignedIntType -> i32", typeMapper->mapSemanticTypeToLLVM(unsignedIntType) == "i32");
        
        auto simpleType = std::make_shared<SimpleType>("i32");
        runTest("SimpleType(i32) -> i32", typeMapper->mapSemanticTypeToLLVM(simpleType) == "i32");
    }

    void testArrayTypeMapping() {
        std::cout << "\n=== 测试数组类型映射 ===" << std::endl;
        
        // 测试简单数组类型
        auto elementType = std::make_shared<SimpleType>("i32");
        auto arrayType = typeMapper->mapArrayTypeToLLVM(elementType, nullptr);
        runTest("Array[i32] -> [0 x i32]", arrayType == "[0 x i32]");
        
        // 测试带大小的数组
        auto sizeExpr = std::make_shared<LiteralExpression>("10", Token::kINTEGER_LITERAL);
        auto sizedArrayType = typeMapper->mapArrayTypeToLLVM(elementType, sizeExpr);
        runTest("Array[i32; 10] -> [10 x i32]", sizedArrayType == "[10 x i32]");
        
        // 测试数组类型字符串解析
        auto arrayTypeStr = typeMapper->mapRxTypeToLLVM("[i32; 5]");
        runTest("[i32; 5] -> [5 x i32]", arrayTypeStr == "[5 x i32]");
    }

    void testReferenceTypeMapping() {
        std::cout << "\n=== 测试引用类型映射 ===" << std::endl;
        
        auto targetType = std::make_shared<SimpleType>("i32");
        
        // 测试不可变引用
        auto immRefType = typeMapper->mapReferenceTypeToLLVM(targetType, false);
        runTest("&i32 -> i32*", immRefType == "i32*");
        
        // 测试可变引用
        auto mutRefType = typeMapper->mapReferenceTypeToLLVM(targetType, true);
        runTest("&mut i32 -> i32*", mutRefType == "i32*");
        
        // 测试引用类型包装器
        auto refWrapper = std::make_shared<ReferenceTypeWrapper>(targetType, true);
        auto wrapperRefType = typeMapper->mapSemanticTypeToLLVM(refWrapper);
        runTest("ReferenceTypeWrapper(&mut i32) -> i32*", wrapperRefType == "i32*");
    }

    void testFunctionTypeMapping() {
        std::cout << "\n=== 测试函数类型映射 ===" << std::endl;
        
        std::vector<std::shared_ptr<SemanticType>> paramTypes = {
            std::make_shared<SimpleType>("i32"),
            std::make_shared<SimpleType>("i32")
        };
        auto returnType = std::make_shared<SimpleType>("i32");
        
        auto funcType = typeMapper->mapFunctionTypeToLLVM(paramTypes, returnType);
        runTest("fn(i32, i32) -> i32 (i32, i32)*", funcType == "i32 (i32, i32)*");
        
        // 测试函数类型包装器
        auto funcWrapper = std::make_shared<FunctionType>(paramTypes, returnType);
        auto wrapperFuncType = typeMapper->mapSemanticTypeToLLVM(funcWrapper);
        runTest("FunctionType wrapper", wrapperFuncType == "i32 (i32, i32)*");
    }

    void testTypeCompatibility() {
        std::cout << "\n=== 测试类型兼容性 ===" << std::endl;
        
        runTest("i32 compatible with i32", typeMapper->areTypesCompatible("i32", "i32"));
        runTest("i32 compatible with i64", typeMapper->areTypesCompatible("i32", "i64"));
        runTest("i32* compatible with i64*", typeMapper->areTypesCompatible("i32*", "i64*"));
        runTest("[10 x i32] compatible with [5 x i32]", typeMapper->areTypesCompatible("[10 x i32]", "[5 x i32]"));
        runTest("i32 not compatible with i32*", !typeMapper->areTypesCompatible("i32", "i32*"));
    }

    void testCommonType() {
        std::cout << "\n=== 测试公共类型计算 ===" << std::endl;
        
        runTest("Common(i32, i64) -> i32", typeMapper->getCommonType("i32", "i64") == "i32");
        runTest("Common(i32*, i64*) -> i32*", typeMapper->getCommonType("i32*", "i64*") == "i32*");
        runTest("Common(i32, i32) -> i32", typeMapper->getCommonType("i32", "i32") == "i32");
    }

    void testTypeAttributes() {
        std::cout << "\n=== 测试类型属性 ===" << std::endl;
        
        runTest("isIntegerType(i32)", typeMapper->isIntegerType("i32"));
        runTest("isIntegerType(i1)", typeMapper->isIntegerType("i1"));
        runTest("isIntegerType(i8)", typeMapper->isIntegerType("i8"));
        runTest("isIntegerType(i64)", typeMapper->isIntegerType("i64"));
        runTest("!isIntegerType(i32*)", !typeMapper->isIntegerType("i32*"));
        
        runTest("isPointerType(i32*)", typeMapper->isPointerType("i32*"));
        runTest("isPointerType(i8*)", typeMapper->isPointerType("i8*"));
        runTest("!isPointerType(i32)", !typeMapper->isPointerType("i32"));
        
        runTest("isArrayType([10 x i32])", typeMapper->isArrayType("[10 x i32]"));
        runTest("!isArrayType(i32)", !typeMapper->isArrayType("i32"));
        
        runTest("isStructType(%struct_Point)", typeMapper->isStructType("%struct_Point"));
        runTest("!isStructType(i32)", !typeMapper->isStructType("i32"));
    }

    void testTypeSizeAndAlignment() {
        std::cout << "\n=== 测试类型大小和对齐 ===" << std::endl;
        
        runTest("getSize(i1) == 1", typeMapper->getTypeSize("i1") == 1);
        runTest("getSize(i8) == 1", typeMapper->getTypeSize("i8") == 1);
        runTest("getSize(i32) == 4", typeMapper->getTypeSize("i32") == 4);
        runTest("getSize(i64) == 8", typeMapper->getTypeSize("i64") == 8);
        runTest("getSize(i32*) == 4", typeMapper->getTypeSize("i32*") == 4);
        runTest("getSize([10 x i32]) == 40", typeMapper->getTypeSize("[10 x i32]") == 40);
        
        runTest("getAlignment(i1) == 1", typeMapper->getTypeAlignment("i1") == 1);
        runTest("getAlignment(i8) == 1", typeMapper->getTypeAlignment("i8") == 1);
        runTest("getAlignment(i32) == 4", typeMapper->getTypeAlignment("i32") == 4);
        runTest("getAlignment(i64) == 8", typeMapper->getTypeAlignment("i64") == 8);
        runTest("getAlignment(i32*) == 4", typeMapper->getTypeAlignment("i32*") == 4);
    }


    void runAllTests() {
        std::cout << "开始 TypeMapper 测试..." << std::endl;
        
        testBasicTypeMapping();
        testSemanticTypeMapping();
        testArrayTypeMapping();
        testReferenceTypeMapping();
        testFunctionTypeMapping();
        testTypeCompatibility();
        testCommonType();
        testTypeAttributes();
        testTypeSizeAndAlignment();
        
        std::cout << "\n=== 测试结果 ===" << std::endl;
        std::cout << "总测试数: " << testCount << std::endl;
        std::cout << "通过测试: " << passedCount << std::endl;
        std::cout << "失败测试: " << (testCount - passedCount) << std::endl;
        std::cout << "成功率: " << (passedCount * 100 / testCount) << "%" << std::endl;
        
        if (passedCount == testCount) {
            std::cout << "🎉 所有测试通过！" << std::endl;
        } else {
            std::cout << "❌ 有测试失败，请检查实现。" << std::endl;
        }
    }
};

int main() {
    try {
        TypeMapperTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    }
}
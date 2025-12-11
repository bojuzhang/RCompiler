#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "scope.hpp"
#include "symbol.hpp"
#include "typecheck.hpp"
#include "typewrapper.hpp"

/**
 * TypeMapper 类 - 负责将 Rx 语言类型系统映射到 LLVM IR 类型系统
 * 
 * 核心功能：
 * 1. 完整的类型映射：支持所有 Rx 语言类型到 LLVM 类型的映射
 * 2. 类型安全映射：确保类型转换的正确性和安全性
 * 3. 语义集成：与语义分析阶段的类型信息完全集成
 * 4. 简化设计：无缓存系统，避免状态污染问题
 */
class TypeMapper {
private:
    // 作用域树引用
    std::shared_ptr<ScopeTree> scopeTree;

    std::shared_ptr<TypeChecker> typeChecker;
    
    // ==================== 基础类型映射表 ====================
    
    // Rx基础类型到LLVM类型的映射表
    static const std::unordered_map<std::string, std::string> BASIC_TYPE_MAPPING;
    
    // 类型大小映射表（针对32位机器优化）
    static const std::unordered_map<std::string, int> TYPE_SIZE_MAPPING;
    
    // 类型对齐映射表
    static const std::unordered_map<std::string, int> TYPE_ALIGNMENT_MAPPING;
    
    // ==================== 错误处理 ====================
    
    bool hasErrors;
    std::vector<std::string> errorMessages;

public:
    // 构造函数
    explicit TypeMapper(std::shared_ptr<ScopeTree> scopeTree,
                        std::shared_ptr<TypeChecker> typeChecker);
    
    // 析构函数
    ~TypeMapper() = default;
    
    // ==================== 核心映射接口 ====================
    
    /**
     * 将 Rx 类型字符串映射为 LLVM 类型
     * @param typeName Rx 类型字符串（如 "i32", "str", "[i32; 10]"）
     * @return 对应的 LLVM 类型字符串
     */
    std::string mapRxTypeToLLVM(const std::string& typeName);
    
    /**
     * 将语义类型对象映射为 LLVM 类型
     * @param type 语义类型对象指针
     * @return 对应的 LLVM 类型字符串
     */
    std::string mapSemanticTypeToLLVM(std::shared_ptr<SemanticType> type);
    
    // 新增：直接从 AST Type 节点映射到 LLVM 类型
    std::string mapASTTypeToLLVM(std::shared_ptr<Type> astType);
    
    // ==================== 复合类型映射接口 ====================
    
    /**
     * 处理数组类型的映射
     * @param elementType 数组元素类型
     * @param sizeExpression 数组大小表达式
     * @return LLVM 数组类型字符串（如 "[10 x i32]"）
     */
    std::string mapArrayTypeToLLVM(std::shared_ptr<SemanticType> elementType, 
                                   std::shared_ptr<Expression> sizeExpression = nullptr);
    
    /**
     * 处理引用类型的映射
     * @param targetType 被引用的目标类型
     * @param isMutable 是否为可变引用
     * @return LLVM 指针类型字符串（如 "i32*"）
     */
    std::string mapReferenceTypeToLLVM(std::shared_ptr<SemanticType> targetType, bool isMutable = false);
    
    /**
     * 处理函数类型的映射
     * @param parameterTypes 参数类型列表
     * @param returnType 返回值类型
     * @return LLVM 函数类型字符串
     */
    std::string mapFunctionTypeToLLVM(const std::vector<std::shared_ptr<SemanticType>>& parameterTypes,
                                      std::shared_ptr<SemanticType> returnType);
    
    /**
     * 处理结构体类型的映射
     * @param structName 结构体名称
     * @return LLVM 结构体类型字符串（如 "%struct_Point"）
     */
    std::string mapStructTypeToLLVM(const std::string& structName);
    
    // ==================== 类型信息查询接口 ====================
    
    /**
     * 获取复合类型的元素类型
     * @param compositeType 复合类型字符串
     * @return 元素类型字符串
     */
    std::string getElementType(const std::string& compositeType);
    
    /**
     * 获取数组类型的元素类型
     * @param arrayType 数组类型字符串
     * @return 元素类型字符串
     */
    std::string getArrayElementType(const std::string& arrayType);
    
    /**
     * 获取数组类型的大小
     * @param arrayType 数组类型字符串
     * @return 数组大小字符串
     */
    std::string getArraySize(const std::string& arrayType);
    
    /**
     * 获取结构体类型的字节大小
     * @param structType 结构体类型字符串
     * @return 字节大小
     */
    int getStructSize(const std::string& structType);
    
    /**
     * 获取指针类型指向的类型
     * @param pointerType 指针类型字符串
     * @return 指向的类型字符串
     */
    std::string getPointedType(const std::string& pointerType);
    
    /**
     * 检查两个类型是否兼容
     * @param type1 第一个类型
     * @param type2 第二个类型
     * @return 是否兼容
     */
    bool areTypesCompatible(const std::string& type1, const std::string& type2);
    
    /**
     * 计算两个类型的公共类型
     * @param type1 第一个类型
     * @param type2 第二个类型
     * @return 公共类型字符串
     */
    std::string getCommonType(const std::string& type1, const std::string& type2);
    
    // ==================== 类型属性接口 ====================
    
    /**
     * 检查是否为整数类型
     * @param type 类型字符串
     * @return 是否为整数类型
     */
    bool isIntegerType(const std::string& type);
    
    /**
     * 检查是否为无符号整数类型
     * @param type 类型字符串
     * @return 是否为无符号整数类型
     */
    bool isUnsignedIntegerType(const std::string& type);
    
    /**
     * 检查语义类型是否为无符号整数类型
     * @param type 语义类型对象
     * @return 是否为无符号整数类型
     */
    bool isUnsignedIntegerType(std::shared_ptr<SemanticType> type);
    
    /**
     * 检查是否为指针类型
     * @param type 类型字符串
     * @return 是否为指针类型
     */
    bool isPointerType(const std::string& type);
    
    /**
     * 检查是否为数组类型
     * @param type 类型字符串
     * @return 是否为数组类型
     */
    bool isArrayType(const std::string& type);
    
    /**
     * 检查是否为结构体类型
     * @param type 类型字符串
     * @return 是否为结构体类型
     */
    bool isStructType(const std::string& type);
    
    /**
     * 获取类型的字节大小
     * @param type 类型字符串
     * @return 字节大小
     */
    int getTypeSize(const std::string& type);
    
    /**
     * 获取类型的对齐要求
     * @param type 类型字符串
     * @return 对齐字节数
     */
    int getTypeAlignment(const std::string& type);
    
    // ==================== 错误处理接口 ====================
    
    /**
     * 检查是否有错误
     * @return 是否有错误
     */
    bool hasError() const;
    
    /**
     * 获取错误信息列表
     * @return 错误信息列表
     */
    std::vector<std::string> getErrorMessages() const;
    
    /**
     * 清空错误信息
     */
    void clearErrors();

private:
    // ==================== 私有辅助方法 ====================
    
    /**
     * 映射简单类型到 LLVM 类型
     * @param typeName 简单类型名称
     * @return LLVM 类型字符串
     */
    std::string mapSimpleTypeToLLVM(const std::string& typeName);
    
    /**
     * 处理多层嵌套类型
     * @param typeStr 类型字符串
     * @return 映射后的 LLVM 类型字符串
     */
    std::string processNestedType(const std::string& typeStr);
    
    /**
     * 处理数组类型字符串解析
     * @param arrayTypeStr 数组类型字符串（如 "[i32; 10]"）
     * @return LLVM 数组类型字符串
     */
    std::string processArrayTypeString(const std::string& arrayTypeStr);
    
    /**
     * 处理引用类型字符串解析
     * @param refTypeStr 引用类型字符串（如 "&mut i32"）
     * @return LLVM 指针类型字符串
     */
    std::string processReferenceTypeString(const std::string& refTypeStr);
    
    /**
     * 处理函数类型字符串解析
     * @param funcTypeStr 函数类型字符串
     * @return LLVM 函数类型字符串
     */
    std::string processFunctionTypeString(const std::string& funcTypeStr);
    
    /**
     * 从数组类型字符串中提取元素类型和大小
     * @param arrayTypeStr 数组类型字符串
     * @param elementType 输出参数：元素类型
     * @param arraySize 输出参数：数组大小
     * @return 是否成功解析
     */
    bool parseArrayTypeString(const std::string& arrayTypeStr, std::string& elementType, std::string& arraySize);
    
    /**
     * 从引用类型字符串中提取目标类型和可变性
     * @param refTypeStr 引用类型字符串
     * @param targetType 输出参数：目标类型
     * @param isMutable 输出参数：是否可变
     * @return 是否成功解析
     */
    bool parseReferenceTypeString(const std::string& refTypeStr, std::string& targetType, bool& isMutable);
    
    /**
     * 报告错误
     * @param message 错误信息
     */
    void reportError(const std::string& message);
    
    /**
     * 检查结构体符号是否存在
     * @param structName 结构体名称
     * @return 结构体符号指针，如果不存在则返回 nullptr
     */
    std::shared_ptr<Symbol> lookupStructSymbol(const std::string& structName);
    
    /**
     * 处理特殊的 SemanticType 类型
     * @param type 语义类型对象
     * @return LLVM 类型字符串，如果不是特殊类型则返回空字符串
     */
    std::string handleSpecialSemanticType(std::shared_ptr<SemanticType> type);
    
    // 新增：处理具体 AST Type 类型的私有方法
    std::string mapTypePathToLLVM(std::shared_ptr<TypePath> typePath);
    std::string mapArrayTypeToLLVM(std::shared_ptr<ArrayType> arrayType);
    std::string mapReferenceTypeToLLVM(std::shared_ptr<ReferenceType> refType);
    std::string mapUnitTypeToLLVM(std::shared_ptr<UnitType> unitType);
};
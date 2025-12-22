#include "typemapper.hpp"
#include "typecheck.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>

// ==================== 静态成员初始化 ====================

const std::unordered_map<std::string, std::string> TypeMapper::BASIC_TYPE_MAPPING = {
    // 基础整数类型（32位机器，所有整数都映射为i32）
    {"i32", "i32"},
    {"i64", "i32"},  // 32位机器上i64映射为i32
    {"u32", "i32"},
    {"u64", "i32"},  // 32位机器上u64映射为i32
    
    // 平台相关类型（32位机器）
    {"isize", "i32"},
    {"usize", "i32"},
    
    // 特殊语义类型（32位机器）
    {"Int", "i32"},      // 32位机器上Int映射为i32
    {"SignedInt", "i32"},
    {"UnsignedInt", "i32"},
    
    // 基础类型
    {"bool", "i1"},
    {"char", "i8"},
    {"str", "i8*"},
    {"unit", "void"},
    {"void", "void"},
    
    // 指针类型
    {"ptr", "i8*"}
};

const std::unordered_map<std::string, int> TypeMapper::TYPE_SIZE_MAPPING = {
    {"i1", 1},
    {"i8", 1},
    {"i32", 4},
    {"i64", 4},  // 32位机器上i64实际大小为4
    {"void", 0},
    {"ptr", 4}   // 32位系统上指针大小为4
};

const std::unordered_map<std::string, int> TypeMapper::TYPE_ALIGNMENT_MAPPING = {
    {"i1", 1},
    {"i8", 1},
    {"i32", 4},
    {"i64", 4},  // 32位机器上i64对齐为4
    {"void", 0},
    {"ptr", 4}   // 32位系统上指针对齐为4
};

// ==================== 构造函数和析构函数 ====================

TypeMapper::TypeMapper(std::shared_ptr<ScopeTree> scopeTree,
                       std::shared_ptr<TypeChecker> typeChecker)
    : scopeTree(scopeTree)
    , typeChecker(typeChecker)
    , hasErrors(false)
{
    // 无缓存系统，避免状态污染问题
}

// ==================== 核心映射接口 ====================

std::string TypeMapper::mapRxTypeToLLVM(const std::string& typeName) {
    // 检查是否为基础类型
    auto basicIt = BASIC_TYPE_MAPPING.find(typeName);
    if (basicIt != BASIC_TYPE_MAPPING.end()) {
        return basicIt->second;
    } else {
        // 处理复合类型
        return processNestedType(typeName);
    }
}

std::string TypeMapper::mapSemanticTypeToLLVM(std::shared_ptr<SemanticType> type) {
    if (!type) {
        reportError("Null semantic type pointer");
        return "i32"; // 默认类型
    }
    
    // 处理特殊的语义类型
    std::string result = handleSpecialSemanticType(type);
    if (!result.empty()) {
        return result;
    }
    
    // 获取类型字符串并映射
    std::string typeStr = type->tostring();
    std::string mapped = mapRxTypeToLLVM(typeStr);
    return mapped;
}

// 新增：直接从 AST Type 节点映射到 LLVM 类型
std::string TypeMapper::mapASTTypeToLLVM(std::shared_ptr<Type> astType) {
    if (!astType) {
        reportError("Null AST type pointer");
        return "i32"; // 默认类型
    }
    
    // 根据具体的 Type 子类进行处理
    if (auto typePath = std::dynamic_pointer_cast<TypePath>(astType)) {
        return mapTypePathToLLVM(typePath);
    } else if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(astType)) {
        return mapArrayTypeToLLVM(arrayType);
    } else if (auto refType = std::dynamic_pointer_cast<ReferenceType>(astType)) {
        return mapReferenceTypeToLLVM(refType);
    } else if (auto unitType = std::dynamic_pointer_cast<UnitType>(astType)) {
        return mapUnitTypeToLLVM(unitType);
    }
    
    reportError("Unknown AST type type");
    return "i32"; // 默认类型
}

// 处理 TypePath 类型
std::string TypeMapper::mapTypePathToLLVM(std::shared_ptr<TypePath> typePath) {
    if (!typePath || !typePath->simplepathsegement) {
        reportError("Invalid TypePath");
        return "i32";
    }
    
    std::string typeName = typePath->simplepathsegement->identifier;
    
    // 检查是否为 Self 类型
    if (typeName.empty() && typePath->simplepathsegement->isSelf) {
        typeName = "Self";
    }
    
    // 使用基础类型映射
    auto basicIt = BASIC_TYPE_MAPPING.find(typeName);
    if (basicIt != BASIC_TYPE_MAPPING.end()) {
        return basicIt->second;
    }
    
    // 如果不是基础类型，可能是结构体类型
    return mapStructTypeToLLVM(typeName);
}

// 处理 ArrayType 类型
std::string TypeMapper::mapArrayTypeToLLVM(std::shared_ptr<ArrayType> arrayType) {
    if (!arrayType || !arrayType->type) {
        reportError("Invalid ArrayType");
        return "i32";
    }
    
    // 递归处理元素类型
    std::string elementLLVMType = mapASTTypeToLLVM(arrayType->type);
    
    // 计算数组大小
    std::string arraySize = "0";
    if (arrayType->expression && typeChecker) {
        int64_t size = typeChecker->EvaluateArraySize(*arrayType->expression);
        arraySize = std::to_string(size);
    }
    
    // 生成LLVM数组类型
    return "[" + arraySize + " x " + elementLLVMType + "]";
}

// 处理 ReferenceType 类型
std::string TypeMapper::mapReferenceTypeToLLVM(std::shared_ptr<ReferenceType> refType) {
    if (!refType || !refType->type) {
        reportError("Invalid ReferenceType");
        return "i8*";
    }
    
    // 递归处理目标类型
    std::string targetLLVMType = mapASTTypeToLLVM(refType->type);
    
    // 对于引用类型，总是返回指针类型
    // &T -> T*, &&T -> T**, &mut T -> T*
    return targetLLVMType + "*";
}

// 处理 UnitType 类型
std::string TypeMapper::mapUnitTypeToLLVM(std::shared_ptr<UnitType> unitType) {
    return "void";
}

// ==================== 复合类型映射接口 ====================

std::string TypeMapper::mapArrayTypeToLLVM(std::shared_ptr<SemanticType> elementType,
                                             std::shared_ptr<Expression> sizeExpression) {
    if (!elementType) {
        reportError("Null element type for array mapping");
        return "i32";
    }
    
    std::string elementLLVMType = mapSemanticTypeToLLVM(elementType);
    std::string arraySize = std::to_string(typeChecker->EvaluateArraySize(*sizeExpression.get()));
    
    // 生成LLVM数组类型
    return "[" + arraySize + " x " + elementLLVMType + "]";
}

std::string TypeMapper::mapReferenceTypeToLLVM(std::shared_ptr<SemanticType> targetType, bool isMutable) {
    if (!targetType) {
        reportError("Null target type for reference mapping");
        return "i8*";
    }
    
    std::string targetLLVMType = mapSemanticTypeToLLVM(targetType);
    
    // 生成LLVM指针类型
    return targetLLVMType + "*";
}

std::string TypeMapper::mapFunctionTypeToLLVM(const std::vector<std::shared_ptr<SemanticType>>& parameterTypes,
                                               std::shared_ptr<SemanticType> returnType) {
    // 映射参数类型
    std::vector<std::string> paramLLVMTypes;
    for (const auto& paramType : parameterTypes) {
        paramLLVMTypes.push_back(mapSemanticTypeToLLVM(paramType));
    }
    
    // 映射返回类型
    std::string returnLLVMType = returnType ? mapSemanticTypeToLLVM(returnType) : "void";
    
    // 生成LLVM函数类型
    std::string result = returnLLVMType + " (";
    for (size_t i = 0; i < paramLLVMTypes.size(); ++i) {
        if (i > 0) result += ", ";
        result += paramLLVMTypes[i];
    }
    result += ")*";
    
    return result;
}

std::string TypeMapper::mapStructTypeToLLVM(const std::string& structName) {
    // // 验证结构体符号存在
    // auto structSymbol = lookupStructSymbol(structName);
    // if (!structSymbol) {
    //     reportError("Unknown struct type: " + structName);
    //     return "i8*"; // 默认指针类型
    // }
    
    // 生成LLVM结构体类型
    return "%struct_" + structName;
}

// ==================== 类型信息查询接口 ====================

std::string TypeMapper::getElementType(const std::string& compositeType) {
    if (isArrayType(compositeType)) {
        return getArrayElementType(compositeType);
    }
    
    if (isPointerType(compositeType)) {
        return getPointedType(compositeType);
    }
    
    return "";
}

std::string TypeMapper::getPointedType(const std::string& pointerType) {
    if (pointerType.length() > 1 && pointerType.back() == '*') {
        return pointerType.substr(0, pointerType.length() - 1);
    }
    return "";
}

bool TypeMapper::areTypesCompatible(const std::string& type1, const std::string& type2) {
    // 完全相同
    if (type1 == type2) {
        return true;
    }
    
    // 指针类型兼容性
    if (isPointerType(type1) && isPointerType(type2)) {
        std::string pointed1 = getPointedType(type1);
        std::string pointed2 = getPointedType(type2);
        return areTypesCompatible(pointed1, pointed2);
    }
    
    // 数组类型兼容性（忽略大小差异）
    if (isArrayType(type1) && isArrayType(type2)) {
        std::string elem1 = getArrayElementType(type1);
        std::string elem2 = getArrayElementType(type2);
        return areTypesCompatible(elem1, elem2);
    }
    
    // 整数类型兼容性（在32位机器上）
    if (isIntegerType(type1) && isIntegerType(type2)) {
        return true; // 所有整数类型在32位机器上都兼容
    }
    
    return false;
}

std::string TypeMapper::getCommonType(const std::string& type1, const std::string& type2) {
    // 相同类型直接返回
    if (type1 == type2) {
        return type1;
    }
    
    // 整数类型的公共类型为i32
    if (isIntegerType(type1) && isIntegerType(type2)) {
        return "i32";
    }
    
    // 指针类型的公共类型
    if (isPointerType(type1) && isPointerType(type2)) {
        std::string pointed1 = getPointedType(type1);
        std::string pointed2 = getPointedType(type2);
        std::string commonPointed = getCommonType(pointed1, pointed2);
        return commonPointed + "*";
    }
    
    // 默认返回第一个类型
    return type1;
}

// ==================== 类型属性接口 ====================

bool TypeMapper::isIntegerType(const std::string& type) {
    return type == "i1" || type == "i8" || type == "i32";  // 32位机器上没有i64
}

bool TypeMapper::isUnsignedIntegerType(const std::string& type) {
    // 检查LLVM类型字符串是否对应无符号类型
    // 注意：在LLVM中，无符号和有符号整数都使用相同的类型表示（如i32）
    // 所以我们需要从原始Rx类型名称来判断
    return type == "u32" || type == "u64" || type == "usize" || type == "UnsignedInt";
}

bool TypeMapper::isUnsignedIntegerType(std::shared_ptr<SemanticType> type) {
    if (!type) {
        return false;
    }
    
    // 检查是否为无符号整数类型
    if (dynamic_cast<UnsignedIntType*>(type.get())) {
        return true;
    }
    
    // 检查类型字符串
    std::string typeStr = type->tostring();
    return isUnsignedIntegerType(typeStr);
}

bool TypeMapper::isPointerType(const std::string& type) {
    return type.length() > 1 && type.back() == '*';
}

bool TypeMapper::isArrayType(const std::string& type) {
    return type.front() == '[' && type.find("x") != std::string::npos && type.back() == ']';
}

bool TypeMapper::isStructType(const std::string& type) {
    return type.find("%struct_") == 0;
}

int TypeMapper::getTypeSize(const std::string& type) {
    // 检查大小映射表
    auto it = TYPE_SIZE_MAPPING.find(type);
    if (it != TYPE_SIZE_MAPPING.end()) {
        return it->second;
    }
    
    // 指针类型在32位系统上大小为4
    if (isPointerType(type)) {
        return 4;
    }
    
    // 数组类型需要计算元素数量和元素大小
    if (isArrayType(type)) {
        std::string elementType = getArrayElementType(type);
        std::string sizeStr = getArraySize(type);
        
        try {
            int numElements = std::stoi(sizeStr);
            int elementSize = getTypeSize(elementType);
            return numElements * elementSize;
        } catch (...) {
            return 0;
        }
    }
    
    // 结构体类型需要递归计算每个字段的大小和
    if (isStructType(type)) {
        return getStructSize(type);
    }
    
    return 0;
}

int TypeMapper::getTypeAlignment(const std::string& type) {
    // 检查对齐映射表
    auto it = TYPE_ALIGNMENT_MAPPING.find(type);
    if (it != TYPE_ALIGNMENT_MAPPING.end()) {
        return it->second;
    }
    
    // 指针类型对齐到指针大小
    if (isPointerType(type)) {
        return 4;
    }
    
    // 数组类型按元素对齐
    if (isArrayType(type)) {
        std::string elementType = getArrayElementType(type);
        return getTypeAlignment(elementType);
    }
    
    // 结构体类型默认对齐为4
    if (isStructType(type)) {
        return 4;
    }
    
    return 4; // 默认对齐
}


// ==================== 错误处理接口 ====================

bool TypeMapper::hasError() const {
    return hasErrors;
}

std::vector<std::string> TypeMapper::getErrorMessages() const {
    return errorMessages;
}

void TypeMapper::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
}

// ==================== 私有辅助方法 ====================

std::string TypeMapper::mapSimpleTypeToLLVM(const std::string& typeName) {
    auto it = BASIC_TYPE_MAPPING.find(typeName);
    if (it != BASIC_TYPE_MAPPING.end()) {
        return it->second;
    }
    
    // 如果已经是LLVM类型格式，直接返回
    if (typeName.find('*') != std::string::npos ||
        typeName.find('[') != std::string::npos ||
        typeName.find('%') == 0 ||
        typeName == "i1" || typeName == "i8" || typeName == "i32" || typeName == "void") {
        return typeName;
    }
    
    // 默认返回i32
    return "i32";
}

std::string TypeMapper::processNestedType(const std::string& typeStr) {
    // 处理数组类型
    if (typeStr.front() == '[' && typeStr.find(';') != std::string::npos) {
        return processArrayTypeString(typeStr);
    }
    
    // 处理引用类型
    if (typeStr.front() == '&') {
        return processReferenceTypeString(typeStr);
    }
    
    // 处理函数类型
    if (typeStr.find("fn(") == 0) {
        return processFunctionTypeString(typeStr);
    }
    
    // 处理结构体类型
    if (typeStr.find("struct_") != std::string::npos ||
        (typeStr.length() > 0 && std::all_of(typeStr.begin(), typeStr.end(), [](char c) { return std::isalnum(c) || c == '_'; }))) {
        return mapStructTypeToLLVM(typeStr);
    }
    
    // 默认处理
    return mapSimpleTypeToLLVM(typeStr);
}

std::string TypeMapper::processArrayTypeString(const std::string& arrayTypeStr) {
    std::string elementType, arraySize;
    if (!parseArrayTypeString(arrayTypeStr, elementType, arraySize)) {
        reportError("Invalid array type string: " + arrayTypeStr);
        return "i32";
    }
    
    std::string elementLLVMType = mapRxTypeToLLVM(elementType);
    return "[" + arraySize + " x " + elementLLVMType + "]";
}

std::string TypeMapper::processReferenceTypeString(const std::string& refTypeStr) {
    std::string targetType;
    bool isMutable;
    if (!parseReferenceTypeString(refTypeStr, targetType, isMutable)) {
        reportError("Invalid reference type string: " + refTypeStr);
        return "i8*";
    }
    
    // 递归处理目标类型，以支持多层引用如 &&T
    std::string targetLLVMType = mapRxTypeToLLVM(targetType);
    
    // 对于引用类型，总是返回指针类型
    // &T -> T*, &&T -> T**, &mut T -> T*
    return targetLLVMType + "*";
}

std::string TypeMapper::processFunctionTypeString(const std::string& funcTypeStr) {
    // 简化的函数类型处理
    // 实际实现需要更复杂的解析逻辑
    return "i32 (...)*"; // 默认函数指针类型
}

bool TypeMapper::parseArrayTypeString(const std::string& arrayTypeStr, std::string& elementType, std::string& arraySize) {
    // 解析格式: [T; N]
    if (arrayTypeStr.front() != '[' || arrayTypeStr.back() != ']') {
        return false;
    }
    
    size_t semicolonPos = arrayTypeStr.find(';');
    if (semicolonPos == std::string::npos) {
        return false;
    }
    
    elementType = arrayTypeStr.substr(1, semicolonPos - 1);
    arraySize = arrayTypeStr.substr(semicolonPos + 1, arrayTypeStr.length() - semicolonPos - 2);
    
    // 去除空格
    elementType.erase(std::remove_if(elementType.begin(), elementType.end(), ::isspace), elementType.end());
    arraySize.erase(std::remove_if(arraySize.begin(), arraySize.end(), ::isspace), arraySize.end());
    
    return !elementType.empty() && !arraySize.empty();
}

bool TypeMapper::parseReferenceTypeString(const std::string& refTypeStr, std::string& targetType, bool& isMutable) {
    // 解析格式: &T, &mut T, &&T, &&mut T
    if (refTypeStr.empty() || refTypeStr[0] != '&') {
        return false;
    }
    
    isMutable = false;
    size_t pos = 1;  // 从第一个&之后开始
    
    // 跳过额外的&符号（用于多层引用）
    while (pos < refTypeStr.length() && refTypeStr[pos] == '&') {
        pos++;
    }
    
    // 跳过空格
    while (pos < refTypeStr.length() && std::isspace(refTypeStr[pos])) {
        pos++;
    }
    
    // 检查是否为可变引用
    if (pos < refTypeStr.length() && refTypeStr.substr(pos, 3) == "mut") {
        isMutable = true;
        pos += 3;
        
        // 跳过空格
        while (pos < refTypeStr.length() && std::isspace(refTypeStr[pos])) {
            pos++;
        }
    }
    
    // 提取目标类型
    targetType = refTypeStr.substr(pos);
    
    // 去除空格
    targetType.erase(std::remove_if(targetType.begin(), targetType.end(), ::isspace), targetType.end());
    
    return !targetType.empty();
}


void TypeMapper::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

std::shared_ptr<Symbol> TypeMapper::lookupStructSymbol(const std::string& structName) {
    if (!scopeTree) {
        return nullptr;
    }
    
    // 在当前作用域中查找结构体符号
    auto currentScope = scopeTree->GetCurrentScope();
    if (currentScope) {
        auto symbol = currentScope->Lookup(structName);
        if (symbol && symbol->kind == SymbolKind::Struct) {
            return symbol;
        }
    }
    
    return nullptr;
}

std::string TypeMapper::handleSpecialSemanticType(std::shared_ptr<SemanticType> type) {
    if (!type) {
        return "";
    }
    
    // 检查特殊整数类型（32位机器）
    if (dynamic_cast<IntType*>(type.get())) {
        return "i32";  // 32位机器上Int映射为i32
    }
    
    if (dynamic_cast<SignedIntType*>(type.get())) {
        return "i32";
    }
    
    if (dynamic_cast<UnsignedIntType*>(type.get())) {
        return "i32";
    }
    
    // 检查复合类型包装器
    if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(type.get())) {
        return mapArrayTypeToLLVM(arrayType->GetElementType(), arrayType->GetSizeExpression());
    }
    
    if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(type.get())) {
        return mapReferenceTypeToLLVM(refType->getTargetType(), refType->GetIsMutable());
    }
    
    if (auto funcType = dynamic_cast<FunctionType*>(type.get())) {
        return mapFunctionTypeToLLVM(funcType->GetParameterTypes(), funcType->GetReturnType());
    }
    
    // 其他类型返回空字符串，让调用者处理
    return "";
}

std::string TypeMapper::getArrayElementType(const std::string& arrayType) {
    // 解析 [N x T] 格式的数组类型
    if (arrayType.front() == '[' && arrayType.find("x") != std::string::npos) {
        size_t xPos = arrayType.find("x");
        if (xPos != std::string::npos && arrayType.back() == ']') {
            return arrayType.substr(xPos + 2, arrayType.length() - xPos - 3);
        }
    }
    return "";
}

std::string TypeMapper::getArraySize(const std::string& arrayType) {
    // 解析 [N x T] 格式的数组类型，提取N
    if (arrayType.front() == '[' && arrayType.find("x") != std::string::npos) {
        size_t xPos = arrayType.find("x");
        if (xPos != std::string::npos) {
            return arrayType.substr(1, xPos - 1);
        }
    }
    return "";
}

int TypeMapper::getStructSize(const std::string& structType) {
    // 从结构体名称中提取结构体名
    std::string structName;
    if (structType.find("%struct_") == 0) {
        structName = structType.substr(8); // 去掉 "%struct_" 前缀
    } else {
        structName = structType;
    }
    
    // 从符号表查找结构体定义
    auto structSymbol = lookupStructSymbol(structName);
    if (!structSymbol) {
        reportError("Unknown struct type for size calculation: " + structName);
        return 16; // 默认大小
    }
    
    auto structSym = std::dynamic_pointer_cast<StructSymbol>(structSymbol);
    if (!structSym) {
        return 16; // 默认大小
    }
    
    int totalSize = 0;
    int maxAlignment = 1;
    
    // 递归计算每个字段的大小
    for (const auto& field : structSym->fields) {
        if (field && field->type) {
            std::string fieldLLVMType = mapSemanticTypeToLLVM(field->type);
            int fieldSize = getTypeSize(fieldLLVMType);
            int fieldAlignment = getTypeAlignment(fieldLLVMType);
            
            // 对齐到字段的对齐边界
            totalSize = ((totalSize + fieldAlignment - 1) / fieldAlignment) * fieldAlignment;
            totalSize += fieldSize;
            
            maxAlignment = std::max(maxAlignment, fieldAlignment);
        }
    }
    
    // 对齐整个结构体到最大字段对齐
    totalSize = ((totalSize + maxAlignment - 1) / maxAlignment) * maxAlignment;
    
    return totalSize;
}
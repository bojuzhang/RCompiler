# TypeMapper 组件设计文档

## 概述

TypeMapper 组件负责将 Rx 语言的类型系统映射到 LLVM IR 类型系统。它是 IR 生成阶段的核心组件，确保类型信息的正确转换和兼容性检查。

## 设计目标

1. **完整的类型映射**：支持所有 Rx 语言类型到 LLVM 类型的映射
2. **类型安全映射**：确保类型转换的正确性和安全性
3. **语义集成**：与语义分析阶段的类型信息完全集成
4. **性能优化**：避免重复的类型映射计算

## 核心架构

### 类型映射表

TypeMapper 维护一个从 Rx 类型到 LLVM 类型的映射表，针对 32 位机器进行优化：

```cpp
// 基础类型映射（32位机器）
static const std::unordered_map<std::string, std::string> BASIC_TYPE_MAP = {
    {"i32", "i32"},
    {"u32", "u32"},
    {"isize", "i32"},  // 32位机器上isize对应i32
    {"usize", "u32"},  // 32位机器上usize对应u32
    {"bool", "i1"},
    {"str", "i8*"},
    {"char", "i8"},
    {"()", "void"}
};

// SemanticType 特殊类型映射
static const std::unordered_map<std::string, std::string> SEMANTIC_TYPE_MAP = {
    {"Int", "i32"},        // Int类型对应i32
    {"SignedInt", "i32"},  // SignedInt类型对应i32
    {"UnsignedInt", "u32"}  // UnsignedInt类型对应u32
};
```

### 复合类型映射

对于复合类型，TypeMapper 提供递归的映射策略：

1. **数组类型**：`[T; N]` → `[N x T]`
2. **引用类型**：`&T`, `&mut T` → `T*`
3. **函数类型**：`fn(A, B) -> C` → `C (A, B)*`
4. **结构体类型**：`%struct_Name`（注意避免C++转义问题）

### 类型缓存机制

```cpp
class TypeMapper {
private:
    std::shared_ptr<ScopeTree> scopeTree;
    std::unordered_map<std::string, std::string> typeCache;
    
public:
    TypeMapper(std::shared_ptr<ScopeTree> scopeTree);
    
    // 核心映射接口
    std::string mapRxTypeToLLVM(const std::string& rxType);
    std::string mapSemanticTypeToLLVM(std::shared_ptr<SemanticType> semanticType);
    
    // 复合类型映射
    std::string mapArrayTypeToLLVM(std::shared_ptr<ArrayTypeWrapper> arrayType);
    std::string mapReferenceTypeToLLVM(std::shared_ptr<ReferenceTypeWrapper> refType);
    std::string mapFunctionTypeToLLVM(std::shared_ptr<FunctionType> funcType);
    std::string mapStructTypeToLLVM(const std::string& structName);
    
    // 类型信息查询
    std::string getElementType(const std::string& compositeType);
    bool areTypesCompatible(const std::string& type1, const std::string& type2);
    std::string getCommonType(const std::string& type1, const std::string& type2);
    
    // 类型大小和对齐
    int getTypeSize(const std::string& llvmType);
    int getTypeAlignment(const std::string& llvmType);
};
```

## 实现策略

### 基础类型映射

```cpp
std::string TypeMapper::mapRxTypeToLLVM(const std::string& rxType) {
    // 检查缓存
    auto it = typeCache.find(rxType);
    if (it != typeCache.end()) {
        return it->second;
    }
    
    // SemanticType 特殊类型映射
    auto semanticIt = SEMANTIC_TYPE_MAP.find(rxType);
    if (semanticIt != SEMANTIC_TYPE_MAP.end()) {
        typeCache[rxType] = semanticIt->second;
        return semanticIt->second;
    }
    
    // 基础类型查找
    auto basicIt = BASIC_TYPE_MAP.find(rxType);
    if (basicIt != BASIC_TYPE_MAP.end()) {
        typeCache[rxType] = basicIt->second;
        return basicIt->second;
    }
    
    // 复合类型处理
    std::string result;
    if (rxType.find("[") == 0 && rxType.find("]") != std::string::npos) {
        // 数组类型：[T; N]
        result = mapArrayTypeString(rxType);
    } else if (rxType.find("&") == 0) {
        // 引用类型：&T, &mut T
        result = mapReferenceTypeString(rxType);
    } else if (rxType.find("fn(") == 0) {
        // 函数类型
        result = mapFunctionTypeString(rxType);
    } else {
        // 自定义类型（结构体、枚举等）
        result = mapCustomType(rxType);
    }
    
    typeCache[rxType] = result;
    return result;
}
```

### SemanticType 映射

```cpp
std::string TypeMapper::mapSemanticTypeToLLVM(std::shared_ptr<SemanticType> semanticType) {
    if (!semanticType) {
        return "i8*"; // 默认类型
    }
    
    std::string typeStr = semanticType->tostring();
    
    // 检查缓存
    auto it = typeCache.find(typeStr);
    if (it != typeCache.end()) {
        return it->second;
    }
    
    // 处理特殊SemanticType
    if (dynamic_cast<IntType*>(semanticType.get())) {
        typeCache[typeStr] = "i32";
        return "i32";
    } else if (dynamic_cast<SignedIntType*>(semanticType.get())) {
        typeCache[typeStr] = "i32";
        return "i32";
    } else if (dynamic_cast<UnsignedIntType*>(semanticType.get())) {
        typeCache[typeStr] = "u32";
        return "u32";
    }
    
    // 处理复合类型
    if (auto arrayType = dynamic_cast<ArrayTypeWrapper*>(semanticType.get())) {
        return mapArrayTypeToLLVM(arrayType);
    } else if (auto refType = dynamic_cast<ReferenceTypeWrapper*>(semanticType.get())) {
        return mapReferenceTypeToLLVM(refType);
    } else if (auto funcType = dynamic_cast<FunctionType*>(semanticType.get())) {
        return mapFunctionTypeToLLVM(funcType);
    } else {
        // 简单类型
        return mapRxTypeToLLVM(typeStr);
    }
}
```

### 数组类型映射

```cpp
std::string TypeMapper::mapArrayTypeToLLVM(std::shared_ptr<ArrayTypeWrapper> arrayType) {
    // 递归映射元素类型
    std::string elementType = mapSemanticTypeToLLVM(arrayType->GetElementType());
    
    // 获取数组大小
    std::string sizeStr = "0";
    if (auto sizeExpr = arrayType->GetSizeExpression()) {
        if (auto literal = dynamic_cast<LiteralExpression*>(sizeExpr.get())) {
            sizeStr = literal->literal;
        }
    }
    
    // 构造 LLVM 数组类型：[N x T]
    return "[" + sizeStr + " x " + elementType + "]";
}
```

### 引用类型映射

```cpp
std::string TypeMapper::mapReferenceTypeToLLVM(std::shared_ptr<ReferenceTypeWrapper> refType) {
    // 递归映射目标类型
    std::string targetType = mapSemanticTypeToLLVM(refType->getTargetType());
    
    // LLVM 中所有引用都是指针类型
    return targetType + "*";
}
```

### 结构体类型映射

```cpp
std::string TypeMapper::mapStructTypeToLLVM(const std::string& structName) {
    // 查找结构体符号
    auto structSymbol = scopeTree->LookupSymbol(structName);
    if (!structSymbol || structSymbol->kind != SymbolKind::Struct) {
        return "i8*"; // 未知类型，使用 void*
    }
    
    // 构造结构体类型：%struct_Name（避免C++转义问题）
    return "%struct_" + structName;
}
```

### 多层嵌套类型处理

```cpp
std::string TypeMapper::mapNestedType(const std::string& rxType) {
    std::string current = rxType;
    std::string result = current;
    
    // 处理多层嵌套：&[&[i32; 3]; 2]
    while (true) {
        if (current.find("[") == 0 && current.find("]") != std::string::npos) {
            // 处理数组类型
            size_t closeBracket = current.find_last_of("]");
            std::string innerType = current.substr(1, closeBracket - 1);
            std::string sizePart = current.substr(closeBracket + 1);
            
            std::string mappedInner = mapNestedType(innerType);
            result = "[" + sizePart + " x " + mappedInner + "]";
            break;
        } else if (current.find("&") == 0) {
            // 处理引用类型
            size_t mutPos = current.find("mut ");
            if (mutPos != std::string::npos) {
                current = current.substr(mutPos + 4); // 跳过 "mut "
            } else {
                current = current.substr(1); // 跳过 "&"
            }
            continue;
        } else {
            // 基础类型或SemanticType
            auto basicIt = BASIC_TYPE_MAP.find(current);
            if (basicIt != BASIC_TYPE_MAP.end()) {
                result = basicIt->second;
            } else {
                auto semanticIt = SEMANTIC_TYPE_MAP.find(current);
                if (semanticIt != SEMANTIC_TYPE_MAP.end()) {
                    result = semanticIt->second;
                } else {
                    result = "%struct_" + current; // 避免C++转义问题
                }
            }
            break;
        }
    }
    
    return result;
}
```

### 多层嵌套数组处理

```cpp
std::string TypeMapper::mapMultiDimensionalArray(std::shared_ptr<ArrayTypeWrapper> arrayType) {
    // 递归处理多维数组
    std::string elementTypeStr = mapSemanticTypeToLLVM(arrayType->GetElementType());
    
    // 检查元素类型是否也是数组
    if (auto innerArray = dynamic_cast<ArrayTypeWrapper*>(arrayType->GetElementType().get())) {
        // 多维数组：[[T; M]; N] -> [N x [M x T]]
        std::string innerArrayType = mapMultiDimensionalArray(innerArray);
        
        // 获取当前维度大小
        std::string sizeStr = "0";
        if (auto sizeExpr = arrayType->GetSizeExpression()) {
            if (auto literal = dynamic_cast<LiteralExpression*>(sizeExpr.get())) {
                sizeStr = literal->literal;
            }
        }
        
        return "[" + sizeStr + " x " + innerArrayType + "]";
    } else {
        // 一维数组
        return mapArrayTypeToLLVM(arrayType);
    }
}
```

### 多层嵌套引用处理

```cpp
std::string TypeMapper::mapMultiLevelReference(std::shared_ptr<ReferenceTypeWrapper> refType) {
    // 递归处理多层引用
    std::string targetTypeStr = mapSemanticTypeToLLVM(refType->getTargetType());
    
    // 检查目标类型是否也是引用
    if (auto innerRef = dynamic_cast<ReferenceTypeWrapper*>(refType->getTargetType().get())) {
        // 多层引用：&&T -> T**
        std::string innerRefType = mapMultiLevelReference(innerRef);
        return innerRefType + "*";
    } else {
        // 单层引用：&T -> T*
        return targetTypeStr + "*";
    }
}
```

## 类型兼容性检查

### 兼容性规则

```cpp
bool TypeMapper::areTypesCompatible(const std::string& type1, const std::string& type2) {
    // 完全相同
    if (type1 == type2) return true;
    
    // 指针类型兼容性
    if (isPointerType(type1) && isPointerType(type2)) {
        std::string base1 = getBaseType(type1);
        std::string base2 = getBaseType(type2);
        return areTypesCompatible(base1, base2);
    }
    
    // 数组类型兼容性
    if (isArrayType(type1) && isArrayType(type2)) {
        std::string elem1 = getElementType(type1);
        std::string elem2 = getElementType(type2);
        return areTypesCompatible(elem1, elem2);
    }
    
    // 整数类型兼容性（32位机器上的隐式转换）
    if (isIntegerType(type1) && isIntegerType(type2)) {
        return true; // LLVM 支持整数类型的隐式转换
    }
    
    return false;
}
```

### 公共类型计算

```cpp
std::string TypeMapper::getCommonType(const std::string& type1, const std::string& type2) {
    if (type1 == type2) return type1;
    
    // 整数类型的公共类型（32位机器）
    if (isIntegerType(type1) && isIntegerType(type2)) {
        // 在32位机器上，i32是大多数操作的默认类型
        return "i32";
    }
    
    // 指针类型的公共类型
    if (isPointerType(type1) && isPointerType(type2)) {
        std::string base1 = getBaseType(type1);
        std::string base2 = getBaseType(type2);
        std::string commonBase = getCommonType(base1, base2);
        return commonBase + "*";
    }
    
    // 默认返回第一个类型
    return type1;
}
```

## 类型大小和对齐信息

```cpp
int TypeMapper::getTypeSize(const std::string& llvmType) {
    static const std::unordered_map<std::string, int> SIZE_MAP = {
        {"i1", 1}, {"i8", 1}, {"i16", 2}, {"i32", 4}, {"u32", 4},
        {"void*", 4}, {"i8*", 4}  // 32位机器
    };
    
    auto it = SIZE_MAP.find(llvmType);
    return (it != SIZE_MAP.end()) ? it->second : 4; // 默认 4 字节（32位机器）
}

int TypeMapper::getTypeAlignment(const std::string& llvmType) {
    // 对齐通常与大小相同
    return getTypeSize(llvmType);
}
```

## 与其他组件的集成

### 与 IRBuilder 的集成

```cpp
class IRBuilder {
private:
    std::shared_ptr<TypeMapper> typeMapper;
    
public:
    IRBuilder(std::shared_ptr<ScopeTree> scopeTree) 
        : typeMapper(std::make_shared<TypeMapper>(scopeTree)) {}
    
    void emitAlloca(const std::string& result, const std::string& rxType, int size = 1) {
        std::string llvmType = typeMapper->mapRxTypeToLLVM(rxType);
        emitInstruction(result + " = alloca " + llvmType);
    }
    
    void emitLoad(const std::string& result, const std::string& ptr, const std::string& rxType) {
        std::string llvmType = typeMapper->mapRxTypeToLLVM(rxType);
        emitInstruction(result + " = load " + llvmType + ", " + llvmType + "* " + ptr);
    }
};
```

### 与 ExpressionGenerator 的集成

```cpp
class ExpressionGenerator {
private:
    std::shared_ptr<TypeMapper> typeMapper;
    
public:
    std::string generateBinaryOp(const std::string& left, const std::string& right, 
                               Token op, const std::string& resultType) {
        std::string llvmType = typeMapper->mapRxTypeToLLVM(resultType);
        std::string result = newRegister();
        
        switch (op) {
            case Token::kPlus:
                emitInstruction(result + " = add " + llvmType + " " + left + ", " + right);
                break;
            case Token::kMinus:
                emitInstruction(result + " = sub " + llvmType + " " + left + ", " + right);
                break;
            // ... 其他运算符
        }
        
        return result;
    }
};
```

## 错误处理

### 类型映射错误

```cpp
std::string TypeMapper::handleUnknownType(const std::string& rxType) {
    // 记录错误
    std::cerr << "Warning: Unknown type '" << rxType << "', using i8*" << std::endl;
    
    // 返回安全的默认类型
    return "i8*";
}
```

### 类型不匹配错误

```cpp
void TypeMapper::reportTypeMismatch(const std::string& expected, const std::string& actual) {
    std::cerr << "Type Error: Expected '" << expected 
              << "', but found '" << actual << "'" << std::endl;
}
```

## 性能优化

### 类型缓存策略

```cpp
class TypeMapper {
private:
    // 多级缓存
    std::unordered_map<std::string, std::string> basicTypeCache;
    std::unordered_map<std::string, std::string> arrayTypeCache;
    std::unordered_map<std::string, std::string> refTypeCache;
    std::unordered_map<std::string, std::string> semanticTypeCache;
    
public:
    std::string mapRxTypeToLLVM(const std::string& rxType) {
        // 根据类型特征选择合适的缓存
        if (isArrayTypeString(rxType)) {
            return mapArrayTypeWithCache(rxType);
        } else if (isReferenceTypeString(rxType)) {
            return mapReferenceTypeWithCache(rxType);
        } else {
            return mapBasicTypeWithCache(rxType);
        }
    }
    
    std::string mapSemanticTypeToLLVM(std::shared_ptr<SemanticType> semanticType) {
        std::string typeStr = semanticType->tostring();
        auto it = semanticTypeCache.find(typeStr);
        if (it != semanticTypeCache.end()) {
            return it->second;
        }
        
        std::string result = computeSemanticTypeMapping(semanticType);
        semanticTypeCache[typeStr] = result;
        return result;
    }
};
```

### 内存池优化

```cpp
class TypeMapper {
private:
    // 字符串池减少内存分配
    std::unordered_set<std::string> stringPool;
    
    const std::string* internString(const std::string& str) {
        auto it = stringPool.find(str);
        if (it != stringPool.end()) {
            return &(*it);
        }
        auto result = stringPool.insert(str).first;
        return &(*result);
    }
};
```

## 测试策略

### 单元测试

```cpp
// 基础类型映射测试（32位机器）
TEST(TypeMapperTest, BasicTypeMapping) {
    TypeMapper mapper(scopeTree);
    EXPECT_EQ(mapper.mapRxTypeToLLVM("i32"), "i32");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("u32"), "u32");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("isize"), "i32");  // 32位机器
    EXPECT_EQ(mapper.mapRxTypeToLLVM("usize"), "u32");  // 32位机器
    EXPECT_EQ(mapper.mapRxTypeToLLVM("bool"), "i1");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("str"), "i8*");
}

// SemanticType映射测试
TEST(TypeMapperTest, SemanticTypeMapping) {
    TypeMapper mapper(scopeTree);
    
    auto intType = std::make_shared<IntType>();
    auto signedIntType = std::make_shared<SignedIntType>();
    auto unsignedIntType = std::make_shared<UnsignedIntType>();
    
    EXPECT_EQ(mapper.mapSemanticTypeToLLVM(intType), "i32");
    EXPECT_EQ(mapper.mapSemanticTypeToLLVM(signedIntType), "i32");
    EXPECT_EQ(mapper.mapSemanticTypeToLLVM(unsignedIntType), "u32");
}

// 复合类型映射测试
TEST(TypeMapperTest, CompositeTypeMapping) {
    TypeMapper mapper(scopeTree);
    EXPECT_EQ(mapper.mapRxTypeToLLVM("[i32; 10]"), "[10 x i32]");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("&i32"), "i32*");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("&mut i32"), "i32*");
}

// 嵌套类型映射测试
TEST(TypeMapperTest, NestedTypeMapping) {
    TypeMapper mapper(scopeTree);
    EXPECT_EQ(mapper.mapRxTypeToLLVM("&[&i32; 3]"), "[3 x i32]**");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("[[i32; 3]; 2]"), "[2 x [3 x i32]]");
    EXPECT_EQ(mapper.mapRxTypeToLLVM("&&i32"), "i32**");
}

// 多维数组测试
TEST(TypeMapperTest, MultiDimensionalArrayMapping) {
    TypeMapper mapper(scopeTree);
    
    // 创建二维数组类型 [[i32; 3]; 2]
    auto innerArrayType = std::make_shared<ArrayTypeWrapper>(
        std::make_shared<SimpleType>("i32"), 
        std::make_shared<LiteralExpression>("3", Token::kINTEGER_LITERAL)
    );
    auto outerArrayType = std::make_shared<ArrayTypeWrapper>(
        innerArrayType,
        std::make_shared<LiteralExpression>("2", Token::kINTEGER_LITERAL)
    );
    
    std::string result = mapper.mapArrayTypeToLLVM(outerArrayType);
    EXPECT_EQ(result, "[2 x [3 x i32]]");
}
```

### 集成测试

```cpp
// 与 IRBuilder 集成测试
TEST(TypeMapperIntegrationTest, IRBuilderIntegration) {
    auto scopeTree = std::make_shared<ScopeTree>();
    IRBuilder builder(scopeTree);
    
    // 测试类型映射在 IR 生成中的使用
    builder.emitAlloca("%x", "i32");
    builder.emitLoad("%val", "%x", "i32");
    
    // 验证生成的 IR 包含正确的类型
    std::string ir = builder.getIR();
    EXPECT_TRUE(ir.find("%x = alloca i32") != std::string::npos);
    EXPECT_TRUE(ir.find("%val = load i32, i32* %x") != std::string::npos);
}
```

## 使用示例

### 基本使用

```cpp
// 创建 TypeMapper
auto scopeTree = semanticAnalyzer->getScopeTree();
TypeMapper typeMapper(scopeTree);

// 映射基础类型（32位机器）
std::string i32Type = typeMapper.mapRxTypeToLLVM("i32");        // "i32"
std::string u32Type = typeMapper.mapRxTypeToLLVM("u32");        // "u32"
std::string isizeType = typeMapper.mapRxTypeToLLVM("isize");   // "i32"（32位机器）
std::string usizeType = typeMapper.mapRxTypeToLLVM("usize");   // "u32"（32位机器）
std::string boolType = typeMapper.mapRxTypeToLLVM("bool");      // "i1"
std::string strType = typeMapper.mapRxTypeToLLVM("str");        // "i8*"

// 映射复合类型
std::string arrayType = typeMapper.mapRxTypeToLLVM("[i32; 10]"); // "[10 x i32]"
std::string refType = typeMapper.mapRxTypeToLLVM("&i32");        // "i32*"
```

### 高级使用

```cpp
// 从语义类型映射
auto semanticType = symbol->type;
std::string llvmType = typeMapper.mapSemanticTypeToLLVM(semanticType);

// 映射特殊SemanticType
auto intType = std::make_shared<IntType>();
auto signedIntType = std::make_shared<SignedIntType>();
auto unsignedIntType = std::make_shared<UnsignedIntType>();

std::string intLLVM = typeMapper.mapSemanticTypeToLLVM(intType);        // "i32"
std::string signedIntLLVM = typeMapper.mapSemanticTypeToLLVM(signedIntType); // "i32"
std::string unsignedIntLLVM = typeMapper.mapSemanticTypeToLLVM(unsignedIntType); // "u32"

// 类型兼容性检查
bool compatible = typeMapper.areTypesCompatible("i32", "u32");
std::string commonType = typeMapper.getCommonType("i32", "u32");

// 获取类型信息（32位机器）
int size = typeMapper.getTypeSize("i32");        // 4
int alignment = typeMapper.getTypeAlignment("i32"); // 4
```

### 多层嵌套类型示例

```cpp
// 多层嵌套数组
std::string multiDimArray = typeMapper.mapRxTypeToLLVM("[[i32; 3]; 2]"); // "[2 x [3 x i32]]"

// 多层嵌套引用
std::string multiRef = typeMapper.mapRxTypeToLLVM("&&i32"); // "i32**"

// 复杂嵌套：&[&[&i32; 3]; 2]
std::string complexNested = typeMapper.mapRxTypeToLLVM("&[&[&i32; 3]; 2]"); // "[2 x [3 x i32***]]**"
```

## 总结

TypeMapper 组件是 IR 生成阶段的核心组件，提供了完整的类型映射功能：

1. **完整的类型支持**：支持所有 Rx 语言类型到 LLVM 类型的映射，包括特殊SemanticType
2. **32位机器优化**：正确处理isize/usize在32位机器上的映射
3. **嵌套类型处理**：正确处理多层嵌套的数组和引用类型
4. **类型安全**：提供类型兼容性检查和公共类型计算
5. **性能优化**：通过缓存和内存池优化性能
6. **易于集成**：与 IRBuilder 和 ExpressionGenerator 等组件无缝集成
7. **C++兼容**：避免结构体类型名的转义问题

通过 TypeMapper，IR 生成器可以正确地处理各种复杂的类型场景，确保生成的 LLVM IR 的类型正确性和安全性。
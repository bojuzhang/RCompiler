# SymbolCollection（符号收集）组件分析

## 概述

SymbolCollection 是语义分析的第一阶段，负责遍历抽象语法树（AST）并收集所有符号信息，建立完整的作用域树结构。这个阶段为后续的常量求值、控制流分析和类型检查提供基础的符号信息。

## 整体工作逻辑

### 主要职责

1. **符号收集** - 收集函数、常量、结构体、枚举等符号
2. **作用域建立** - 建立层次化的作用域树结构
3. **类型系统初始化** - 注册内置类型和函数
4. **Impl 块处理** - 处理固有实现和关联项

### 处理流程

```
开始 → 初始化内置符号 → 遍历AST → 收集符号 → 建立作用域 → 处理Impl块 → 完成
```

## 核心接口分析

### 类结构

```cpp
class SymbolCollector : public ASTVisitor {
private:
    std::shared_ptr<ScopeTree> root;           // 作用域树根节点
    std::stack<ASTNode*> nodeStack;           // AST节点栈
    bool inFunction = false;                  // 是否在函数内
    bool inLoop = false;                      // 是否在循环内
    bool inTrait = false;                     // 是否在trait内
    bool inImpl = false;                      // 是否在impl块内
    std::string currentFunctionName;           // 当前函数名
    bool hasError = false;                    // 错误标志
};
```

### 主要接口方法

#### 1. 初始化接口

```cpp
void BeginCollection();
```

**功能**：初始化符号收集过程，注册内置类型和函数

**实现细节**：
- 创建作用域树根节点
- 注册内置类型：`i32`, `i64`, `u32`, `u64`, `bool`, `char`, `str`, `usize`, `isize`, `unit`
- 注册内置函数：`print`, `println`, `printInt`, `printlnInt`, `getString`, `getInt`, `exit`

**代码示例**：
```cpp
void SymbolCollector::BeginCollection() {
    auto globalScope = root->GetCurrentScope();
    
    // 添加内置类型
    auto builtinTypes = {
        "i32", "i64", "u32", "u64", "bool", "char", "str", "usize", "isize", "unit"
    };
    for (const auto& typeName : builtinTypes) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::BuiltinType, nullptr, false, nullptr
        );
        globalScope->Insert(typeName, typeSymbol);
    }
    
    // 添加内置函数...
}
```

#### 2. AST访问接口

作为 `ASTVisitor` 的实现，`SymbolCollector` 重写了多个 `visit` 方法来处理不同类型的AST节点：

##### Crate 访问
```cpp
void visit(Crate& node) override;
```

**功能**：处理程序根节点，采用两遍扫描策略

**实现细节**：
- 第一遍：收集结构体、枚举、函数、常量符号
- 第二遍：处理 impl 块（依赖第一遍收集的类型信息）

**代码示例**：
```cpp
void SymbolCollector::visit(Crate& node) {
    PushNode(node);
    
    // 第一遍：收集所有结构体、枚举、函数等符号
    for (const auto& item : node.items) {
        if (item) {
            if (auto structItem = dynamic_cast<StructStruct*>(item->item.get())) {
                structItem->accept(*this);
            } else if (auto enumItem = dynamic_cast<Enumeration*>(item->item.get())) {
                enumItem->accept(*this);
            }
            // ... 其他类型处理
        }
    }
    
    // 第二遍：处理 impl 块
    for (const auto& item : node.items) {
        if (item) {
            if (auto implItem = dynamic_cast<InherentImpl*>(item->item.get())) {
                implItem->accept(*this);
            }
        }
    }
    
    PopNode();
}
```

##### 函数符号收集
```cpp
void visit(Function& node) override;
```

**功能**：收集函数符号并建立函数作用域

**实现细节**：
- 创建函数符号并插入到当前作用域
- 进入函数作用域
- 收集参数符号
- 处理函数体

**代码示例**：
```cpp
void SymbolCollector::visit(Function& node) {
    PushNode(node);
    
    bool wasInFunction = inFunction;
    std::string previousFunctionName = currentFunctionName;
    inFunction = true;
    currentFunctionName = node.identifier_name;
    
    CollectFunctionSymbol(node);
    root->EnterScope(Scope::ScopeType::Function, &node);
    CollectParameterSymbols(node);
    
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    root->ExitScope();
    inFunction = wasInFunction;
    currentFunctionName = previousFunctionName;
    PopNode();
}
```

#### 3. 符号收集辅助方法

##### 函数符号收集
```cpp
void CollectFunctionSymbol(Function& node);
```

**功能**：创建并注册函数符号

**实现细节**：
- 检查函数名冲突
- 解析返回类型
- 创建 `FunctionSymbol` 对象
- 插入到作用域中

**代码示例**：
```cpp
void SymbolCollector::CollectFunctionSymbol(Function& node) {
    std::string funcName = node.identifier_name;
    
    // 检查在可见作用域范围内是否已经存在同名的函数符号
    auto existingSymbol = root->LookupSymbol(funcName);
    if (existingSymbol && existingSymbol->kind == SymbolKind::Function) {
        ReportError("Function '" + funcName + "' is already defined");
        return;
    }
    
    std::shared_ptr<SemanticType> returnType;
    if (node.functionreturntype != nullptr && node.functionreturntype->type != nullptr) {
        returnType = ResolveTypeFromNode(*node.functionreturntype->type);
    } else {
        returnType = CreateSimpleType("unit");
    }
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},
        returnType,
        false
    );
    
    funcSymbol->type = returnType;
    bool insertSuccess = root->InsertSymbol(funcName, funcSymbol);
}
```

##### 参数符号收集
```cpp
void CollectParameterSymbols(Function& node);
```

**功能**：收集函数参数符号

**实现细节**：
- 遍历函数参数列表
- 解析参数类型和可变性
- 创建参数符号并插入到函数作用域
- 更新函数符号的参数信息

**代码示例**：
```cpp
void SymbolCollector::CollectParameterSymbols(Function& node) {
    auto params = node.functionparameters;
    if (!params) return;
    
    std::string funcName = node.identifier_name;
    auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(root->LookupSymbol(funcName));
    if (!funcSymbol) {
        std::cerr << "Warning: Could not find function symbol for " << funcName << std::endl;
        return;
    }
    
    funcSymbol->parameters.clear();
    funcSymbol->parameterTypes.clear();
    for (const auto& param : params->functionparams) {
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(param->patternnotopalt.get())) {
            std::string paramName = identPattern->identifier;
            auto paramType = ResolveTypeFromNode(*param->type);
            
            // 检查参数模式是否是可变的
            bool isMutable = identPattern->hasmut;
            if (auto refType = dynamic_cast<ReferenceType*>(param->type.get())) {
                isMutable = refType->ismut;
            }
            
            auto paramSymbol = std::make_shared<Symbol>(
                paramName,
                SymbolKind::Variable,
                paramType,
                isMutable,
                &node
            );
            root->InsertSymbol(paramName, paramSymbol);
            
            funcSymbol->parameters.push_back(paramSymbol);
            funcSymbol->parameterTypes.push_back(paramType);
        }
    }
}
```

#### 4. 类型处理接口

##### 类型解析
```cpp
std::shared_ptr<SemanticType> ResolveTypeFromNode(Type& node);
```

**功能**：从AST类型节点解析语义类型

**实现细节**：
- 处理简单类型路径
- 处理数组类型
- 处理引用类型
- 创建对应的 `SemanticType` 对象

**代码示例**：
```cpp
std::shared_ptr<SemanticType> SymbolCollector::ResolveTypeFromNode(Type& node) {
    if (auto typePath = dynamic_cast<TypePath*>(&node)) {
        if (typePath->simplepathsegement) {
            std::string typeName = typePath->simplepathsegement->identifier;
            if (!typeName.empty()) {
                return CreateSimpleType(typeName);
            }
        }
    } else if (auto arrayType = dynamic_cast<ArrayType*>(&node)) {
        auto elementType = ResolveTypeFromNode(*arrayType->type);
        if (elementType) {
            return std::make_shared<ArrayTypeWrapper>(elementType, arrayType->expression, nullptr);
        }
    } else if (auto refType = dynamic_cast<ReferenceType*>(&node)) {
        auto targetType = ResolveTypeFromNode(*refType->type);
        if (targetType) {
            return std::make_shared<ReferenceTypeWrapper>(targetType, refType->ismut);
        }
    }
    return CreateSimpleType("unit");
}
```

#### 5. Impl 块处理

##### Impl 符号收集
```cpp
void visit(InherentImpl& node) override;
```

**功能**：处理 impl 块，收集关联项

**实现细节**：
- 解析目标类型
- 创建 impl 符号
- 进入 impl 作用域
- 创建 Self 类型别名
- 收集关联函数和常量

**代码示例**：
```cpp
void SymbolCollector::visit(InherentImpl& node) {
    PushNode(node);
    
    bool wasInImpl = inImpl;
    inImpl = true;
    
    root->EnterScope(Scope::ScopeType::Impl, &node);
    
    CollectImplSymbol(node);
    
    // 创建 Self 类型别名，指向目标类型
    auto targetType = GetImplTargetType(node);
    if (targetType) {
        auto selfTypeSymbol = std::make_shared<Symbol>(
            "Self", SymbolKind::TypeAlias, targetType
        );
        root->InsertSymbol("Self", selfTypeSymbol);
    }
    
    auto items = node.associateditems;
    for (const auto& item : items) {
        if (item) {
            CollectAssociatedItem(*item, implSymbol, structSymbol);
        }
    }
    
    root->ExitScope();
    inImpl = wasInImpl;
    PopNode();
}
```

##### 关联函数收集
```cpp
void CollectAssociatedFunction(Function& function, 
                           std::shared_ptr<ImplSymbol> implSymbol,
                           std::shared_ptr<StructSymbol> structSymbol);
```

**功能**：收集 impl 块中的关联函数

**实现细节**：
- 检查 self 参数类型
- 处理不同的 self 参数形式（self, &self, &mut self）
- 创建函数符号并添加到 impl 和结构体中

**代码示例**：
```cpp
void SymbolCollector::CollectAssociatedFunction(Function& function,
                                          std::shared_ptr<ImplSymbol> implSymbol,
                                          std::shared_ptr<StructSymbol> structSymbol) {
    std::string funcName = function.identifier_name;
    
    auto funcSymbol = std::make_shared<FunctionSymbol>(
        funcName,
        std::vector<std::shared_ptr<Symbol>>{},
        returnType,
        true  // 是方法
    );
    
    // 检查第一个参数是否为 self 参数
    if (function.functionparameters && !function.functionparameters->functionparams.empty()) {
        auto firstParam = function.functionparameters->functionparams[0];
        
        if (auto identPattern = dynamic_cast<IdentifierPattern*>(firstParam->patternnotopalt.get())) {
            if (identPattern->identifier == "self") {
                funcSymbol->isMethod = true;
                auto selfType = GetImplTargetType(*static_cast<InherentImpl*>(GetCurrentNode()));
                if (selfType) {
                    bool isMutable = false;
                    if (auto refType = dynamic_cast<ReferenceType*>(firstParam->type.get())) {
                        isMutable = refType->ismut;
                        selfType = std::make_shared<ReferenceTypeWrapper>(selfType, isMutable);
                    }
                    
                    auto selfSymbol = std::make_shared<Symbol>(
                        "self", SymbolKind::Variable, selfType, isMutable, &function
                    );
                    funcSymbol->parameters.push_back(selfSymbol);
                    funcSymbol->parameterTypes.push_back(selfType);
                }
            }
        }
    }
    
    implSymbol->items.push_back(funcSymbol);
    
    if (structSymbol) {
        structSymbol->methods.push_back(funcSymbol);
    }
}
```

## 与其他组件的交互

### 输出接口

```cpp
std::shared_ptr<ScopeTree> getScopeTree() { return root; }
bool HasErrors() const { return hasError; }
```

### 为后续阶段提供的信息

1. **作用域树结构** - 为类型检查提供符号查找环境
2. **符号信息** - 包含类型、可变性、作用域等信息
3. **函数签名** - 参数类型、返回类型等
4. **结构体定义** - 字段信息、方法列表等
5. **Impl 关系** - 方法的归属关系

### 使用约定

- 后续阶段不能修改符号表，只能查询
- 作用域树的结构保持不变
- 符号的可变性和类型信息不可更改

## 错误处理

### 错误类型

1. **符号重定义** - 同名符号在同一作用域内重复定义
2. **类型不存在** - 引用了未定义的类型
3. **参数错误** - 函数参数类型解析失败

### 错误报告机制

```cpp
void ReportError(const std::string& message);
```

所有错误通过标准错误输出报告，并设置 `hasError` 标志。

## 设计特点

### 1. 两遍扫描策略

采用两遍扫描确保类型定义的完整性：
- 第一遍收集类型和函数符号
- 第二遍处理依赖类型信息的 impl 块

### 2. 作用域层次化

建立清晰的作用域层次结构：
- 全局作用域
- 函数作用域
- 块作用域
- Impl 作用域

### 3. 符号分类管理

不同类型的符号分别处理：
- 内置符号预注册
- 用户符号动态收集
- 关联符号特殊处理

### 4. Self 关键字支持

在 impl 块中正确处理 Self 类型别名，支持面向对象编程特性。

## 语法检查详细分析

### 检查问题分类

SymbolCollection 组件在符号收集阶段进行基础的语法检查，主要检查以下类型的问题：

#### 1. 符号定义问题
- **重复定义**：同一作用域内重复定义相同名称的符号
- **未定义类型**：使用了未定义的类型作为变量或函数的类型
- **类型注解错误**：类型注解语法错误或引用了不存在的类型

#### 2. 作用域问题
- **作用域嵌套错误**：不正确的作用域嵌套关系
- **符号可见性错误**：在不可见的作用域中使用符号
- **生命周期问题**：符号的生命周期管理错误

#### 3. 函数定义问题
- **函数参数重复**：同一函数中参数名称重复
- **返回类型未定义**：函数返回类型引用了未定义的类型
- **参数类型未定义**：函数参数类型引用了未定义的类型

#### 4. 结构体和 Impl 问题
- **结构体字段重复**：同一结构体中字段名称重复
- **Impl 块类型不匹配**：Impl 块的类型与结构体定义不匹配
- **方法重复定义**：同一 Impl 块中方法名称重复

#### 5. 内置符号冲突
- **内置符号重定义**：用户定义的符号与内置符号冲突
- **内置类型误用**：错误使用内置类型名称

### 具体检查实现

#### 1. 符号重复定义检查

**检查内容**：
- 同一作用域内的变量、函数、结构体名称冲突
- 函数参数名称冲突
- 结构体字段名称冲突

**实现方式**：
```cpp
bool InsertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol, bool allowShadow = false) {
    // 检查 1：当前作用域是否已存在同名符号
    auto existingSymbol = LookupSymbolInCurrentScope(name);
    if (existingSymbol && !allowShadow) {
        ReportError("Symbol '" + name + "' already defined in this scope");
        return false;
    }
    
    // 检查 2：内置符号保护
    if (IsBuiltinSymbol(name) && existingSymbol == nullptr) {
        ReportError("Cannot redefine built-in symbol '" + name + "'");
        return false;
    }
    
    // 插入符号
    currentScope->symbols[name] = symbol;
    return true;
}
```

**检查的错误类型**：
- `Symbol 'x' already defined in this scope`
- `Cannot redefine built-in symbol 'i32'`
- `Function parameter 'a' already defined`

#### 2. 类型定义检查

**检查内容**：
- 类型注解的正确性
- 自定义类型的存在性
- 类型引用的完整性

**实现方式**：
```cpp
void CheckTypeAnnotation(TypeAnnotation& typeNode) {
    // 检查 1：基本类型
    if (IsBasicType(typeNode.typeName)) {
        return; // 基本类型总是有效的
    }
    
    // 检查 2：自定义类型存在性
    auto typeSymbol = LookupSymbol(typeNode.typeName);
    if (!typeSymbol) {
        ReportError("Undefined type: " + typeNode.typeName);
        return;
    }
    
    // 检查 3：类型符号的有效性
    if (typeSymbol->kind != SymbolKind::Struct && 
        typeSymbol->kind != SymbolKind::Enum &&
        typeSymbol->kind != SymbolKind::BuiltinType) {
        ReportError("'" + typeNode.typeName + "' is not a type");
    }
}
```

**检查的错误类型**：
- `Undefined type: MyStruct`
- `'foo' is not a type`
- `Invalid type annotation`

#### 3. 函数定义检查

**检查内容**：
- 函数名称的唯一性
- 参数名称的唯一性
- 参数类型和返回类型的有效性

**实现方式**：
```cpp
void CheckFunctionDefinition(Function& function) {
    // 检查 1：函数名称唯一性
    if (IsFunctionDefined(function.name)) {
        ReportError("Function '" + function.name + "' already defined");
        return;
    }
    
    // 检查 2：参数名称唯一性
    std::set<std::string> paramNames;
    for (const auto& param : function.parameters) {
        if (paramNames.count(param->name)) {
            ReportError("Parameter '" + param->name + "' already defined in function");
            return;
        }
        paramNames.insert(param->name);
        
        // 检查参数类型
        CheckTypeAnnotation(*param->type);
    }
    
    // 检查 3：返回类型
    if (function.returnType) {
        CheckTypeAnnotation(*function.returnType);
    }
}
```

**检查的错误类型**：
- `Function 'my_func' already defined`
- `Parameter 'x' already defined in function`
- `Undefined type in function return type: UnknownType`

#### 4. 结构体定义检查

**检查内容**：
- 结构体名称的唯一性
- 字段名称的唯一性
- 字段类型的有效性

**实现方式**：
```cpp
void CheckStructDefinition(Struct& structDef) {
    // 检查 1：结构体名称唯一性
    if (IsStructDefined(structDef.name)) {
        ReportError("Struct '" + structDef.name + "' already defined");
        return;
    }
    
    // 检查 2：字段名称唯一性
    std::set<std::string> fieldNames;
    for (const auto& field : structDef.fields) {
        if (fieldNames.count(field->name)) {
            ReportError("Field '" + field->name + "' already defined in struct");
            return;
        }
        fieldNames.insert(field->name);
        
        // 检查字段类型
        CheckTypeAnnotation(*field->type);
    }
}
```

**检查的错误类型**：
- `Struct 'MyStruct' already defined`
- `Field 'value' already defined in struct`
- `Undefined type in struct field: UnknownType`

#### 5. Impl 块检查

**检查内容**：
- Impl 块目标类型的存在性
- Impl 块与结构体定义的匹配
- 方法名称的唯一性

**实现方式**：
```cpp
void CheckImplBlock(ImplBlock& implBlock) {
    // 检查 1：目标类型存在性
    auto targetSymbol = LookupSymbol(implBlock.targetType);
    if (!targetSymbol) {
        ReportError("Cannot impl undefined type: " + implBlock.targetType);
        return;
    }
    
    // 检查 2：目标类型必须是结构体
    if (targetSymbol->kind != SymbolKind::Struct) {
        ReportError("Can only impl struct types, not: " + implBlock.targetType);
        return;
    }
    
    // 检查 3：方法名称唯一性
    std::set<std::string> methodNames;
    for (const auto& method : implBlock.methods) {
        if (methodNames.count(method->name)) {
            ReportError("Method '" + method->name + "' already defined in impl");
            return;
        }
        methodNames.insert(method->name);
        
        // 检查方法定义
        CheckFunctionDefinition(*method);
    }
}
```

**检查的错误类型**：
- `Cannot impl undefined type: UnknownStruct`
- `Can only impl struct types, not: i32`
- `Method 'new' already defined in impl`

#### 6. 作用域检查

**检查内容**：
- 作用域的正确嵌套
- 符号的可见性
- 作用域边界的正确性

**实现方式**：
```cpp
void CheckScopeNesting() {
    // 检查 1：作用域嵌套的合理性
    if (currentScope->parent && currentScope->kind == ScopeKind::Global) {
        ReportError("Global scope cannot be nested");
    }
    
    // 检查 2：函数作用域的正确性
    if (currentScope->kind == ScopeKind::Function && 
        !currentScope->parent->kind == ScopeKind::Global) {
        ReportError("Function can only be defined in global scope");
    }
    
    // 检查 3：块作用域的正确性
    if (currentScope->kind == ScopeKind::Block && 
        currentScope->parent->kind == ScopeKind::Global) {
        ReportError("Block scope cannot be directly in global scope");
    }
}
```

**检查的错误类型**：
- `Global scope cannot be nested`
- `Function can only be defined in global scope`
- `Invalid scope nesting`

#### 7. 内置符号检查

**检查内容**：
- 内置类型的正确性
- 内置函数的保护
- 内置符号的冲突检测

**实现方式**：
```cpp
void CheckBuiltinSymbols() {
    // 检查 1：内置类型定义
    std::vector<std::string> builtinTypes = {"i32", "u32", "bool", "char", "str"};
    for (const auto& type : builtinTypes) {
        auto symbol = LookupSymbol(type);
        if (!symbol || symbol->kind != SymbolKind::BuiltinType) {
            ReportError("Missing built-in type: " + type);
        }
    }
    
    // 检查 2：内置函数保护
    std::vector<std::string> builtinFunctions = {"print", "println", "exit"};
    for (const auto& func : builtinFunctions) {
        if (IsUserDefinedSymbol(func)) {
            ReportError("Cannot redefine built-in function: " + func);
        }
    }
}
```

**检查的错误类型**：
- `Missing built-in type: i32`
- `Cannot redefine built-in function: print`
- `Built-in symbol 'bool' is not a type`

### 错误恢复策略

#### 1. 符号插入恢复
- 重复定义时保留原有符号
- 类型错误时使用默认类型
- 作用域错误时继续处理

#### 2. 部分收集策略
- 跳过有问题的定义，继续处理其他定义
- 保持符号表的完整性
- 为后续阶段提供最大可用信息

#### 3. 错误上下文保持
- 记录完整的错误位置
- 提供符号定义的上下文信息
- 给出修复建议

### 检查性能优化

#### 1. 符号查找优化
- 哈希表快速查找
- 作用域链缓存
- 符号名称预处理

#### 2. 重复检查避免
- 检查结果缓存
- 增量式验证
- 智能检查顺序

#### 3. 内存管理优化
- 符号表的内存池
- 作用域树的预分配
- 垃圾回收策略

通过这些详细的语法检查机制，SymbolCollection 确保了 Rx 语言程序的符号定义正确性和作用域结构的合理性，为后续的语义分析阶段提供了可靠的符号信息基础。
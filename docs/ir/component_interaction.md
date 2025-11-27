# IR 生成器组件交互接口分析

## 概述

本文档详细描述了重新设计的 IR 生成器各组件之间的交互接口设计。与之前的设计不同，新的 IR 生成器不使用 LLVM C++ API，而是通过自定义的 IRBuilder 直接生成 LLVM IR 文本输出到 stdout。

## 组件架构概览

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   IRGenerator   │    │  TypeMapper     │    │ IRBuilder       │
│   (主控制器)    │◄──►│  (类型映射器)    │◄──►│ (IR构建器)      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ExpressionCodegen │    │StatementCodegen │    │FunctionCodegen  │
│ (表达式生成器)   │    │ (语句生成器)     │    │ (函数生成器)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 ▼
                     ┌─────────────────┐
                     │BuiltinDeclarator│
                     │(内置函数声明器) │
                     └─────────────────┘
```

## 核心接口定义

### 1. IRGenerator 主控制器接口

```cpp
class IRGenerator {
public:
    // 构造函数
    IRGenerator(const SymbolTable& symbolTable, const TypeChecker& typeChecker);
    
    // 主要接口
    bool generateIR(const std::vector<std::unique_ptr<Item>>& items);
    std::string getIROutput() const { return output.str(); }
    
    // 组件访问接口
    TypeMapper& getTypeMapper() { return typeMapper; }
    ExpressionCodegen& getExpressionCodegen() { return expressionCodegen; }
    StatementCodegen& getStatementCodegen() { return statementCodegen; }
    FunctionCodegen& getFunctionCodegen() { return functionCodegen; }
    BuiltinDeclarator& getBuiltinDeclarator() { return builtinDeclarator; }
    
    // IRBuilder 访问接口
    IRBuilder& getBuilder() { return builder; }
    
    // 错误处理
    void reportError(const std::string& message, const ASTNode* node = nullptr);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    
    // 符号表访问
    const SymbolInfo* getSymbol(const std::string& name) const;
    const Type* getSymbolType(const std::string& name) const;

private:
    const SymbolTable& symbolTable;
    const TypeChecker& typeChecker;
    
    std::ostringstream output;
    IRBuilder builder;
    
    TypeMapper typeMapper;
    ExpressionCodegen expressionCodegen;
    StatementCodegen statementCodegen;
    FunctionCodegen functionCodegen;
    BuiltinDeclarator builtinDeclarator;
    
    std::vector<std::string> errors;
    
    // 内部辅助方法
    void initializeModule();
    void finalizeModule();
};
```

### 2. IRBuilder 自定义 IR 构建器接口

```cpp
class IRBuilder {
public:
    IRBuilder(std::ostream& output) : output(output), regCounter(0), bbCounter(0) {}
    
    // 寄存器管理
    std::string newRegister() {
        return "_" + std::to_string(++regCounter);
    }
    
    std::string newNamedRegister(const std::string& prefix) {
        return prefix + std::to_string(++regCounter);
    }
    
    // 基本块管理
    std::string newBasicBlock(const std::string& prefix) {
        return prefix + std::to_string(++bbCounter);
    }
    
    void setCurrentBasicBlock(const std::string& bb) { currentBB = bb; }
    const std::string& getCurrentBasicBlock() const { return currentBB; }
    
    // 指令生成
    void emitInstruction(const std::string& instruction) {
        output << "  " << instruction << "\n";
    }
    
    void emitLabel(const std::string& label) {
        output << label << ":\n";
        setCurrentBasicBlock(label);
    }
    
    void emitComment(const std::string& comment) {
        output << "  ; " << comment << "\n";
    }
    
    // 类型指令
    void emitTargetTriple(const std::string& triple) {
        output << "target triple = \"" << triple << "\"\n\n";
    }
    
    void emitAlloca(const std::string& reg, const std::string& type, int align = 4) {
        emitInstruction("%" + reg + " = alloca " + type + ", align " + std::to_string(align));
    }
    
    void emitAllocaArray(const std::string& reg, const std::string& elementType, int size, int align = 4) {
        emitInstruction("%" + reg + " = alloca [" + std::to_string(size) + " x " + elementType + "], align " + std::to_string(align));
    }
    
    void emitStore(const std::string& value, const std::string& ptr, const std::string& type, int align = 4) {
        emitInstruction("store " + type + " %" + value + ", " + ptr + ", align " + std::to_string(align));
    }
    
    void emitLoad(const std::string& reg, const std::string& ptr, const std::string& type, int align = 4) {
        emitInstruction("%" + reg + " = load " + type + ", " + ptr + ", align " + std::to_string(align));
    }
    
    // 算术指令
    void emitAdd(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = add " + type + " %" + left + ", %" + right);
    }
    
    void emitSub(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = sub " + type + " %" + left + ", %" + right);
    }
    
    void emitMul(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = mul " + type + " %" + left + ", %" + right);
    }
    
    void emitDiv(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = sdiv " + type + " %" + left + ", %" + right);
    }
    
    void emitRem(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = srem " + type + " %" + left + ", %" + right);
    }
    
    // 比较指令
    void emitIcmp(const std::string& result, const std::string& cond, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = icmp " + cond + " " + type + " %" + left + ", %" + right);
    }
    
    // 位运算指令
    void emitAnd(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = and " + type + " %" + left + ", %" + right);
    }
    
    void emitOr(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = or " + type + " %" + left + ", %" + right);
    }
    
    void emitXor(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = xor " + type + " %" + left + ", %" + right);
    }
    
    void emitShl(const std::string& result, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = shl " + type + " %" + left + ", %" + right);
    }
    
    // 控制流指令
    void emitBr(const std::string& condition, const std::string& thenLabel, const std::string& elseLabel) {
        emitInstruction("br i1 %" + condition + ", label %" + thenLabel + ", label %" + elseLabel);
    }
    
    void emitBr(const std::string& label) {
        emitInstruction("br label %" + label);
    }
    
    void emitRet(const std::string& value = "", const std::string& type = "void") {
        if (type == "void") {
            emitInstruction("ret void");
        } else {
            emitInstruction("ret " + type + " %" + value);
        }
    }
    
    // 函数相关
    void emitFunctionDecl(const std::string& returnType, const std::string& name, 
                        const std::vector<std::string>& paramTypes, bool dsoLocal = true) {
        std::string decl = "declare";
        if (dsoLocal) decl += " dso_local";
        decl += " " + returnType + " @" + name + "(";
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (i > 0) decl += ", ";
            decl += paramTypes[i];
        }
        decl += ")";
        output << decl << "\n";
    }
    
    void emitFunctionDef(const std::string& returnType, const std::string& name,
                       const std::vector<std::pair<std::string, std::string>>& params) {
        output << "define " << returnType << " @" << name << "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) output << ", ";
            output << params[i].second << " %" << params[i].first;
        }
        output << ") {\n";
    }
    
    void emitFunctionEnd() {
        output << "}\n\n";
    }
    
    // 函数调用
    void emitCall(const std::string& result, const std::string& funcName, 
                  const std::vector<std::string>& args, const std::string& returnType,
                  const std::vector<std::string>& argAttrs = {}, const std::vector<std::string>& retAttrs = {}) {
        std::string call = "call " + returnType;
        if (!retAttrs.empty()) {
            for (const auto& attr : retAttrs) {
                call += " " + attr;
            }
        }
        call += " @" + funcName + "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) call += ", ";
            if (i < argAttrs.size()) {
                call += argAttrs[i] + " ";
            }
            call += args[i];
        }
        call += ")";
        if (!result.empty()) {
            emitInstruction("%" + result + " = " + call);
        } else {
            emitInstruction(call);
        }
    }
    
    // 内存操作
    void emitGetElementPtr(const std::string& result, const std::string& ptr, 
                          const std::vector<std::string>& indices, const std::string& type) {
        std::string gep = "%" + result + " = getelementptr " + type + ", " + ptr;
        for (const auto& index : indices) {
            gep += ", " + index;
        }
        emitInstruction(gep);
    }
    
    void emitBitcast(const std::string& result, const std::string& value, 
                     const std::string& fromType, const std::string& toType) {
        emitInstruction("%" + result + " = bitcast " + fromType + " %" + value + " to " + toType);
    }
    
    // 结构体操作
    void emitExtractValue(const std::string& result, const std::string& value, 
                        const std::vector<int>& indices, const std::string& type) {
        std::string extract = "%" + result + " = extractvalue " + type + " %" + value;
        for (int index : indices) {
            extract += ", " + std::to_string(index);
        }
        emitInstruction(extract);
    }
    
    void emitInsertValue(const std::string& result, const std::string& value, const std::string& element,
                       const std::vector<int>& indices, const std::string& type) {
        std::string insert = "%" + result + " = insertvalue " + type + " %" + value + ", " + element;
        for (int index : indices) {
            insert += ", " + std::to_string(index);
        }
        emitInstruction(insert);
    }

private:
    std::ostream& output;
    size_t regCounter;
    size_t bbCounter;
    std::string currentBB;
};
```

### 3. TypeMapper 类型映射器接口

```cpp
class TypeMapper {
public:
    TypeMapper() = default;
    
    // 基本类型映射
    std::string mapType(const Type* rxType);
    std::string mapBasicType(BasicTypeKind kind);
    std::string mapArrayType(const ArrayType* arrayType);
    std::string mapStructType(const StructType* structType);
    std::string mapReferenceType(const ReferenceType* refType);
    std::string mapFunctionType(const FunctionType* funcType);
    
    // 类型大小和对齐
    int getTypeSize(const std::string& llvmType);
    int getTypeAlignment(const std::string& llvmType);
    
    // 类型转换
    std::string generateTypeConversion(const std::string& value, 
                                    const Type* fromType, 
                                    const Type* toType,
                                    IRBuilder& builder);
    
    // 类型检查
    bool isCompatible(const Type* rxType1, const Type* rxType2);
    bool canImplicitlyConvert(const Type* fromType, const Type* toType);
    
    // 类型属性
    bool isIntegerType(const std::string& llvmType);
    bool isPointerType(const std::string& llvmType);
    bool isArrayType(const std::string& llvmType);
    bool isStructType(const std::string& llvmType);
    
    // 类型解析
    std::string getElementType(const std::string& llvmType);
    std::string getPointedType(const std::string& llvmType);

private:
    std::unordered_map<const Type*, std::string> typeCache;
    
    // 内部辅助方法
    std::string createLLVMStructType(const StructType* structType);
    std::string createLLVMArrayType(const ArrayType* arrayType);
    std::string createLLVMFunctionType(const FunctionType* funcType);
};
```

### 4. ExpressionCodegen 表达式生成器接口

```cpp
class ExpressionCodegen {
public:
    ExpressionCodegen(IRGenerator& generator);
    
    // 主要接口
    std::string generateExpression(const Expression* expr);
    
    // 具体表达式类型处理
    std::string generateLiteral(const LiteralExpression* lit);
    std::string generatePath(const PathExpression* path);
    std::string generateBinary(const BinaryExpression* binary);
    std::string generateUnary(const UnaryExpression* unary);
    std::string generateCall(const CallExpression* call);
    std::string generateIndex(const IndexExpression* index);
    std::string generateField(const FieldExpression* field);
    std::string generateBlock(const BlockExpression* block);  // 新增：BlockExpression 处理
    std::string generateIf(const IfExpression* ifExpr);
    std::string generateLoop(const LoopExpression* loopExpr);
    std::string generateBreak(const BreakExpression* breakExpr);
    std::string generateContinue(const ContinueExpression* continueExpr);
    std::string generateReturn(const ReturnExpression* returnExpr);
    
    // 辅助方法
    std::string generateArithmeticOperation(const std::string& left, const std::string& right, 
                                         BinaryOperator op, const std::string& type);
    std::string generateComparisonOperation(const std::string& left, const std::string& right, 
                                          BinaryOperator op, const std::string& type);
    std::string generateLogicalOperation(const std::string& left, const std::string& right, 
                                      BinaryOperator op, const std::string& type);
    
    // 值管理
    void storeExpressionResult(const std::string& value, const std::string& type);
    std::string loadExpressionValue(const std::string& ptr, const std::string& type);

private:
    IRGenerator& generator;
    std::unordered_map<const Expression*, std::string> expressionCache;
    
    // 内部辅助方法
    std::string loadVariable(const std::string& name);
    std::string generateArrayAccess(const std::string& array, const std::string& index);
    std::string generateStructFieldAccess(const std::string& structPtr, unsigned fieldIndex);
    std::string generateStringLiteral(const std::string& str);
};
```

### 5. StatementCodegen 语句生成器接口

```cpp
class StatementCodegen {
public:
    StatementCodegen(IRGenerator& generator);
    
    // 主要接口
    void generateStatement(const Statement* stmt);
    
    // 具体语句类型处理
    void generateLet(const LetStatement* letStmt);
    void generateExpression(const ExpressionStatement* exprStmt);
    void generateIf(const IfStatement* ifStmt);
    void generateWhile(const WhileStatement* whileStmt);
    void generateLoop(const LoopStatement* loopStmt);
    void generateBreak(const BreakStatement* breakStmt);
    void generateContinue(const ContinueStatement* continueStmt);
    void generateReturn(const ReturnStatement* returnStmt);
    
    // 基本块管理
    std::string createBasicBlock(const std::string& name);
    void setCurrentBasicBlock(const std::string& block);
    const std::string& getCurrentBasicBlock() const;
    
    // 控制流上下文
    void enterLoop(const std::string& continueTarget, const std::string& breakTarget);
    void exitLoop();
    void enterFunction(const std::string& functionName);
    void exitFunction();
    
    // 变量管理
    void allocateVariable(const std::string& name, const std::string& type, const std::string& value = "");
    void storeVariable(const std::string& name, const std::string& value);
    std::string loadVariable(const std::string& name);

private:
    IRGenerator& generator;
    
    // 控制流上下文栈
    struct LoopContext {
        std::string continueTarget;
        std::string breakTarget;
    };
    std::vector<LoopContext> loopStack;
    
    struct FunctionContext {
        std::string functionName;
        std::string returnBlock;
        std::string returnValue;
        bool hasReturn;
    };
    std::vector<FunctionContext> functionStack;
    
    // 变量符号表
    std::unordered_map<std::string, std::string> variableMap; // name -> register
    std::unordered_map<std::string, std::string> variableTypes; // name -> type
};
```

### 6. FunctionCodegen 函数生成器接口

```cpp
class FunctionCodegen {
public:
    FunctionCodegen(IRGenerator& generator);
    
    // 主要接口
    void generateFunction(const Function* function);
    void generateFunctionDecl(const Function* function);
    void generateFunctionBody(const Function* function);
    
    // 参数处理
    void generateParameters(const Function* function);
    std::string generateArgumentLoad(const Parameter* param, const std::string& arg);
    
    // 返回值处理
    void generateReturnStatement(const ReturnExpression* returnExpr);
    std::string generateReturnValue(const Expression* returnExpr);
    
    // 函数调用
    std::string generateFunctionCall(const CallExpression* call);
    std::string generateBuiltinCall(const CallExpression* call);
    
    // 内联函数处理
    bool canInline(const Function* function);
    std::string generateInlineCall(const CallExpression* call);
    
    // 函数签名生成
    std::string generateFunctionSignature(const Function* function);
    std::vector<std::pair<std::string, std::string>> generateParameterList(const Function* function);

private:
    IRGenerator& generator;
    std::unordered_map<const Function*, std::string> functionCache;
    
    // 内部辅助方法
    std::string createFunctionType(const Function* function);
    void generateParameterAlloca(const Parameter* param, const std::string& arg);
    void generatePrologue(const Function* function);
    void generateEpilogue(const Function* function);
};
```

### 7. BuiltinDeclarator 内置函数声明器接口

```cpp
class BuiltinDeclarator {
public:
    BuiltinDeclarator(IRBuilder& builder);
    
    // 初始化内置函数声明
    void declareBuiltinFunctions();
    
    // 具体内置函数声明
    void declarePrint();
    void declarePrintln();
    void declarePrintInt();
    void declarePrintlnInt();
    void declareGetString();
    void declareGetInt();
    void declareMalloc();
    void declareMemcpy();
    void declareMemset();
    void declareExit();
    
    // 内置函数查找
    bool isBuiltinFunction(const std::string& name);
    std::string getBuiltinFunctionType(const std::string& name);
    std::vector<std::string> getBuiltinParameterTypes(const std::string& name);
    
    // 特殊函数声明
    void declareStructFunctions(const StructType* structType);

private:
    IRBuilder& builder;
    std::unordered_map<std::string, std::string> builtinFunctions;
    
    // 内部辅助方法
    void declareFunction(const std::string& name, const std::string& returnType, 
                       const std::vector<std::string>& paramTypes, 
                       const std::vector<std::string>& paramAttrs = {});
};
```

## 组件间交互协议

### 1. 数据传递协议

#### 类型信息传递
```cpp
// TypeMapper → ExpressionCodegen
std::string exprType = typeMapper.mapType(rxExpr->getType());

// TypeMapper → FunctionCodegen  
std::string funcType = typeMapper.createFunctionType(rxFunc->getType());
```

#### 值传递协议
```cpp
// ExpressionCodegen → StatementCodegen
std::string condition = expressionCodegen.generateExpression(ifStmt->getCondition());

// FunctionCodegen → ExpressionCodegen
std::string returnValue = expressionCodegen.generateExpression(returnExpr->getValue());
```

#### 寄存器管理协议
```cpp
// IRBuilder → 所有组件
std::string newReg = builder.newRegister();
builder.emitAlloca(newReg, "i32", 4);

// 组件使用寄存器
builder.emitStore(valueReg, "%" + varName, "i32", 4);
```

### 2. 控制流协议

#### 基本块管理
```cpp
// StatementCodegen 控制基本块切换
void StatementCodegen::generateIf(const IfStatement* ifStmt) {
    std::string thenBB = generator.getBuilder().newBasicBlock("if.then");
    std::string elseBB = generator.getBuilder().newBasicBlock("if.else");
    std::string endBB = generator.getBuilder().newBasicBlock("if.end");
    
    // 生成条件
    std::string condition = generator.getExpressionCodegen().generateExpression(ifStmt->getCondition());
    
    // 创建条件分支
    generator.getBuilder().emitBr(condition, thenBB, elseBB);
    
    // 生成 then 分支
    generator.getBuilder().emitLabel(thenBB);
    generateStatement(ifStmt->getThenBranch());
    generator.getBuilder().emitBr(endBB);
    
    // 生成 else 分支
    if (ifStmt->getElseBranch()) {
        generator.getBuilder().emitLabel(elseBB);
        generateStatement(ifStmt->getElseBranch());
        generator.getBuilder().emitBr(endBB);
    }
    
    // 设置合并点
    generator.getBuilder().emitLabel(endBB);
}
```

#### 循环控制流
```cpp
// StatementCodegen 循环上下文管理
void StatementCodegen::enterLoop(const std::string& continueTarget, const std::string& breakTarget) {
    loopStack.push_back({continueTarget, breakTarget});
}

void StatementCodegen::generateBreak(const BreakStatement* breakStmt) {
    if (loopStack.empty()) {
        generator.reportError("break statement outside loop", breakStmt);
        return;
    }
    generator.getBuilder().emitBr(loopStack.back().breakTarget);
}
```

#### BlockExpression 处理的职责划分
```cpp
// ExpressionCodegen 处理 BlockExpression
std::string ExpressionCodegen::generateBlock(const BlockExpression* block) {
    // 创建新作用域
    generator.getBuilder().enterScope();
    
    // 处理块内语句（通过 StatementCodegen）
    for (const auto& stmt : block->statements) {
        generator.getStatementCodegen().generateStatement(stmt);
    }
    
    // 处理尾表达式
    std::string result;
    if (block->expressionwithoutblock) {
        result = generateExpression(block->expressionwithoutblock);
    } else {
        result = generateUnitValue();
    }
    
    // 退出作用域
    generator.getBuilder().exitScope();
    
    return result;
}
```

### 3. 错误处理协议

#### 错误传播
```cpp
// 所有组件通过 IRGenerator 报告错误
void Component::reportError(const std::string& message, const ASTNode* node) {
    generator.reportError(message, node);
}

// IRGenerator 统一错误管理
void IRGenerator::reportError(const std::string& message, const ASTNode* node) {
    std::string fullMessage = "IR Generation Error: " + message;
    if (node) {
        fullMessage += " at line " + std::to_string(node->getLine());
    }
    errors.push_back(fullMessage);
}
```

#### 错误恢复
```cpp
// 表达式生成中的错误处理
std::string ExpressionCodegen::generateExpression(const Expression* expr) {
    try {
        // 生成表达式 IR
        return dispatchExpression(expr);
    } catch (const GenerationError& e) {
        generator.reportError(e.what(), expr);
        return generator.getBuilder().newRegister(); // 返回空寄存器
    }
}
```

## 初始化和清理协议

### 1. 组件初始化顺序
```cpp
// IRGenerator 构造函数中的初始化顺序
IRGenerator::IRGenerator(const SymbolTable& symbolTable, const TypeChecker& typeChecker)
    : symbolTable(symbolTable), typeChecker(typeChecker),
      output(), builder(output),
      
      // 按依赖顺序初始化组件
      typeMapper(),
      expressionCodegen(*this),
      statementCodegen(*this),
      functionCodegen(*this),
      builtinDeclarator(builder) {
    
    // 初始化模块
    initializeModule();
    
    // 声明内置函数
    builtinDeclarator.declareBuiltinFunctions();
}
```

### 2. 模块初始化
```cpp
void IRGenerator::initializeModule() {
    // 输出目标三元组
    builder.emitTargetTriple("riscv32-unknown-unknown-elf");
    
    // 输出空行分隔
    output << "\n";
}
```

### 3. 模块完成
```cpp
void IRGenerator::finalizeModule() {
    // 检查是否有未完成的函数
    if (!functionStack.empty()) {
        reportError("Unclosed function at end of module");
    }
    
    // 输出结束标记（如果需要）
    // LLVM IR 不需要特殊的结束标记
}
```

## 输出管理协议

### 1. 文本输出格式
```cpp
// IRBuilder 确保正确的 LLVM IR 格式
void IRBuilder::emitInstruction(const std::string& instruction) {
    output << "  " << instruction << "\n";  // 两个空格缩进
}

void IRBuilder::emitLabel(const std::string& label) {
    output << label << ":\n";  // 标签后跟冒号
}
```

### 2. 寄存器命名约定
```cpp
// IRBuilder 的寄存器命名
std::string IRBuilder::newRegister() {
    return "_" + std::to_string(++regCounter);  // _1, _2, _3...
}

std::string IRBuilder::newNamedRegister(const std::string& prefix) {
    return prefix + std::to_string(++regCounter);  // var1, var2...
}
```

### 3. 基本块命名约定
```cpp
// IRBuilder 的基本块命名
std::string IRBuilder::newBasicBlock(const std::string& prefix) {
    return prefix + std::to_string(++bbCounter);  // bb1, bb2, if.then1...
}
```

## 性能优化接口

### 1. 缓存管理
```cpp
// TypeMapper 类型缓存
class TypeMapper {
private:
    std::unordered_map<const Type*, std::string> typeCache;
    
public:
    void clearTypeCache() { typeCache.clear(); }
    
    std::string mapType(const Type* rxType) {
        auto it = typeCache.find(rxType);
        if (it != typeCache.end()) {
            return it->second;
        }
        
        std::string llvmType = createLLVMType(rxType);
        typeCache[rxType] = llvmType;
        return llvmType;
    }
};
```

### 2. 表达式缓存
```cpp
// ExpressionCodegen 表达式结果缓存
class ExpressionCodegen {
private:
    std::unordered_map<const Expression*, std::string> expressionCache;
    
public:
    std::string generateExpression(const Expression* expr) {
        auto it = expressionCache.find(expr);
        if (it != expressionCache.end()) {
            return it->second;
        }
        
        std::string result = dispatchExpression(expr);
        expressionCache[expr] = result;
        return result;
    }
};
```

## 总结

重新设计的组件交互接口具有以下特点：

1. **无 LLVM 依赖**：完全自定义的 IRBuilder 系统
2. **文本输出**：直接生成 LLVM IR 文本
3. **寄存器管理**：自动分配和命名寄存器
4. **基本块管理**：自动生成和管理基本块
5. **类型安全**：保持类型系统的正确性
6. **错误处理**：统一的错误报告机制
7. **性能优化**：缓存机制和高效的字符串操作

这种设计确保了 IR 生成器的独立性、可控性和与现有编译器架构的良好集成，同时满足了不使用 LLVM C++ API 的要求。
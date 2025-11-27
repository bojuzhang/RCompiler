# StatementGenerator 组件设计文档

## 概述

StatementGenerator 组件负责将 Rx 语言的语句转换为 LLVM IR 文本。它是 IR 生成阶段的核心组件之一，处理各种类型的语句生成，包括变量声明、赋值语句、控制流语句等。

**重要架构说明**：根据 AST 节点设计，Statement 使用组合模式，控制流语句（if、loop、while、break、continue、return）都是 Expression 的子类，BlockExpression 包含 Statement 向量。这导致 StatementGenerator 和 ExpressionGenerator 之间存在循环依赖关系，需要通过前向声明和接口分离来解决。

## 设计目标

1. **完整的语句支持**：支持所有 Rx 语言语句类型的 IR 生成
2. **循环依赖解决**：通过接口分离解决 StatementGenerator 和 ExpressionGenerator 的循环依赖
3. **控制流管理**：正确处理复杂控制流结构
4. **作用域感知**：与 ScopeTree 和 IRBuilder 的作用域管理集成
5. **错误处理**：提供完善的错误检测和恢复机制

## 核心架构

### 语句分类

根据 AST 节点结构，StatementGenerator 处理以下语句类型：

1. **Let 语句**（LetStatement）
   - 变量声明：`let x: i32 = 42`
   - 类型推断：`let y = 10`
   - 可变变量：`let mut z = 5`

2. **表达式语句**（ExpressionStatement）
   - 函数调用语句：`func(arg1, arg2)`（有分号）
   - 方法调用语句：`obj.method(arg)`（有分号）
   - 赋值表达式语句：`x = value`（有分号）

3. **项语句**（Item）
   - 函数定义：`fn name() -> type { body }`
   - 结构体定义：`struct Name { fields }`
   - 常量定义：`const NAME: type = value`

4. **复合语句**（通过 Statement 组合处理）
   - 注意：BlockExpression 现在由 ExpressionGenerator 处理，StatementGenerator 不再处理

### 循环依赖解决策略

由于以下循环依赖关系：
- `StatementGenerator` → `ExpressionGenerator`（处理表达式语句中的表达式）
- `ExpressionGenerator` → `StatementGenerator`（处理 BlockExpression 中的语句）

采用以下解决策略：

1. **前向声明**：两个组件互相前向声明
2. **接口分离**：将循环依赖的操作分离到独立接口
3. **延迟初始化**：通过构造函数后初始化解决循环引用
4. **职责明确**：
   - `StatementGenerator`：只处理真正的语句（Let、Item、ExpressionStatement）
   - `ExpressionGenerator`：处理所有表达式，包括 BlockExpression 和控制流表达式
   - `BlockExpression`：完全由 `ExpressionGenerator` 处理，通过调用 `StatementGenerator` 处理其中的语句

### 组件接口设计

```cpp
// 前向声明
class ExpressionGenerator;

class StatementGenerator {
public:
    StatementGenerator(std::shared_ptr<IRBuilder> irBuilder,
                    std::shared_ptr<TypeMapper> typeMapper,
                    std::shared_ptr<ScopeTree> scopeTree,
                    const NodeTypeMap& nodeTypeMap);
    
    // 设置 ExpressionGenerator 引用（延迟初始化）
    void setExpressionGenerator(std::shared_ptr<ExpressionGenerator> exprGen);
    
    // 主要生成接口
    void generateStatement(std::shared_ptr<Statement> stmt);
    
    // 各类语句的生成方法
    void generateLetStatement(std::shared_ptr<LetStatement> stmt);
    void generateExpressionStatement(std::shared_ptr<ExpressionStatement> stmt);
    void generateItemStatement(std::shared_ptr<Item> item);
    
    // 工具方法
    std::string getStatementType(std::shared_ptr<Statement> stmt);
    bool isStatementTerminator(std::shared_ptr<Statement> stmt);
    void handleUnreachableCode();

private:
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<ExpressionGenerator> exprGenerator;  // 延迟初始化
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    const NodeTypeMap& nodeTypeMap;
    
    // 控制流上下文管理
    struct ControlFlowContext {
        std::string loopEndLabel;
        std::string loopBreakLabel;
        std::vector<std::string> breakTargets;
        std::vector<std::string> continueTargets;
    };
    
    std::vector<ControlFlowContext> controlFlowStack;
    
    // 辅助方法
    void enterControlFlowContext(const std::string& loopType);
    void exitControlFlowContext();
    void addBreakTarget(const std::string& label);
    void addContinueTarget(const std::string& label);
    std::string getCurrentBreakTarget();
    std::string getCurrentContinueTarget();
};

// 注意：BlockExpression 现在完全由 ExpressionGenerator 处理
// StatementGenerator 只提供 generateStatement 方法供 ExpressionGenerator 调用
```

## 实现策略

### Let 语句生成

```cpp
void StatementGenerator::generateLetStatement(std::shared_ptr<LetStatement> stmt) {
    // 获取变量名和模式
    std::string varName = getVariableName(stmt->patternnotopalt);
    
    // 确定变量类型
    std::string varType;
    if (stmt->type) {
        // 显式类型
        varType = typeMapper->mapRxTypeToLLVM(getTypeName(stmt->type));
    } else {
        // 类型推断
        if (stmt->expression) {
            varType = getExpressionType(stmt->expression);
        } else {
            varType = "i32"; // 默认类型
        }
    }
    
    // 分配变量空间
    std::string varPtrReg = irBuilder->newRegister(varName, "ptr");
    irBuilder->emitAlloca(varPtrReg, varType);
    
    // 生成初始化表达式
    if (stmt->expression && exprGenerator) {
        std::string initReg = exprGenerator->generateExpression(stmt->expression);
        
        // 类型转换（如果需要）
        std::string initType = getExpressionType(stmt->expression);
        if (initType != varType) {
            initReg = generateImplicitConversion(initReg, initType, varType);
        }
        
        // 存储初始值
        irBuilder->emitStore(initReg, varPtrReg, varType);
    }
    
    // 将变量注册到当前作用域
    auto currentScope = scopeTree->GetCurrentScope();
    auto symbol = std::make_shared<Symbol>(varName, SymbolKind::Variable,
                                          createSemanticType(varType), false, stmt);
    scopeTree->InsertSymbol(varName, symbol);
    
    // 通过 IRBuilder 注册变量寄存器
    irBuilder->setVariableRegister(varName, varPtrReg);
}
```

### 表达式语句生成

```cpp
void StatementGenerator::generateExpressionStatement(std::shared_ptr<ExpressionStatement> stmt) {
    // 表达式语句存储的是 Expression（可能是控制流表达式）
    // 注意：BlockExpression 现在由 ExpressionGenerator 完全处理
    if (!exprGenerator) {
        reportError("ExpressionGenerator not set for ExpressionStatement");
        return;
    }
    
    // 检查是否为控制流表达式
    if (auto ifExpr = std::dynamic_pointer_cast<IfExpression>(stmt->astnode)) {
        exprGenerator->generateIfExpression(ifExpr);
    } else if (auto loopExpr = std::dynamic_pointer_cast<InfiniteLoopExpression>(stmt->astnode)) {
        exprGenerator->generateLoopExpression(loopExpr);
    } else if (auto whileExpr = std::dynamic_pointer_cast<PredicateLoopExpression>(stmt->astnode)) {
        exprGenerator->generateWhileExpression(whileExpr);
    } else if (auto breakExpr = std::dynamic_pointer_cast<BreakExpression>(stmt->astnode)) {
        exprGenerator->generateBreakExpression(breakExpr);
    } else if (auto returnExpr = std::dynamic_pointer_cast<ReturnExpression>(stmt->astnode)) {
        exprGenerator->generateReturnExpression(returnExpr);
    } else if (auto blockExpr = std::dynamic_pointer_cast<BlockExpression>(stmt->astnode)) {
        // BlockExpression 现在由 ExpressionGenerator 处理
        exprGenerator->generateBlockExpression(blockExpr);
    } else {
        // 普通表达式，生成并丢弃结果
        std::string exprReg = exprGenerator->generateExpression(stmt->astnode);
        
        // 对于有分号的表达式语句，丢弃结果
        if (stmt->hassemi) {
            irBuilder->emitComment("Expression result discarded");
        }
    }
}
```

### 项语句生成

```cpp
void StatementGenerator::generateItemStatement(std::shared_ptr<Item> item) {
    // 项语句存储的是 Item，需要根据实际类型处理
    if (auto function = std::dynamic_pointer_cast<Function>(item->item)) {
        generateFunctionDefinition(function);
    } else if (auto structDef = std::dynamic_pointer_cast<StructStruct>(item->item)) {
        generateStructDefinition(structDef);
    } else if (auto constItem = std::dynamic_pointer_cast<ConstantItem>(item->item)) {
        generateConstantDefinition(constItem);
    } else if (auto implDef = std::dynamic_pointer_cast<InherentImpl>(item->item)) {
        generateImplDefinition(implDef);
    } else {
        reportError("Unsupported item type in statement");
    }
}
```

### 控制流语句生成

#### if 语句

```cpp
void StatementGenerator::generateIfStatement(std::shared_ptr<IfExpression> stmt) {
    // 生成条件表达式
    std::string condReg = exprGenerator->generateExpression(stmt->conditions->expression);
    
    // 创建基本块
    std::string thenBB = irBuilder->newBasicBlock("if.then");
    std::string elseBB = irBuilder->newBasicBlock("if.else");
    std::string endBB = irBuilder->newBasicBlock("if.end");
    
    // 生成条件跳转
    irBuilder->emitCondBr(condReg, thenBB, elseBB);
    
    // 生成 then 分支
    irBuilder->setCurrentBasicBlock(thenBB);
    irBuilder->enterScope(); // then 块的作用域
    // 注意：BlockExpression 现在由 ExpressionGenerator 处理
    if (auto blockExpr = std::dynamic_pointer_cast<BlockExpression>(stmt->ifblockexpression)) {
        exprGenerator->generateBlockExpression(blockExpr);
    } else {
        generateStatement(std::make_shared<Statement>(stmt->ifblockexpression));
    }
    irBuilder->exitScope();
    
    // 检查 then 分支是否以终止符结束
    if (!isStatementTerminator(stmt->ifblockexpression)) {
        irBuilder->emitBr(endBB);
    }
    
    // 生成 else 分支
    irBuilder->setCurrentBasicBlock(elseBB);
    irBuilder->enterScope(); // else 块的作用域
    
    if (stmt->elseexpression) {
        generateStatement(std::make_shared<Statement>(stmt->elseexpression));
    }
    
    irBuilder->exitScope();
    
    // 检查 else 分支是否以终止符结束
    if (!stmt->elseexpression || !isStatementTerminator(stmt->elseexpression)) {
        irBuilder->emitBr(endBB);
    }
    
    // 设置结束基本块
    irBuilder->setCurrentBasicBlock(endBB);
}
```

#### loop 语句

```cpp
void StatementGenerator::generateLoopStatement(std::shared_ptr<InfiniteLoopExpression> stmt) {
    // 创建基本块
    std::string loopBB = irBuilder->newBasicBlock("loop.head");
    std::string endBB = irBuilder->newBasicBlock("loop.end");
    
    // 进入循环上下文
    enterControlFlowContext("loop");
    addBreakTarget(endBB);
    
    // 跳转到循环开始
    irBuilder->emitBr(loopBB);
    
    // 生成循环体
    irBuilder->setCurrentBasicBlock(loopBB);
    irBuilder->enterScope(); // 循环体作用域
    
    // 生成循环体语句
    // 注意：BlockExpression 现在由 ExpressionGenerator 处理
    if (auto blockExpr = std::dynamic_pointer_cast<BlockExpression>(stmt->blockexpression)) {
        exprGenerator->generateBlockExpression(blockExpr);
    } else {
        generateStatement(std::make_shared<Statement>(stmt->blockexpression));
    }
    
    irBuilder->exitScope();
    
    // 如果循环体没有 break，继续循环
    irBuilder->emitBr(loopBB);
    
    // 设置循环结束基本块
    irBuilder->setCurrentBasicBlock(endBB);
    
    // 退出循环上下文
    exitControlFlowContext();
}
```

#### while 语句

```cpp
void StatementGenerator::generateWhileStatement(std::shared_ptr<PredicateLoopExpression> stmt) {
    // 创建基本块
    std::string condBB = irBuilder->newBasicBlock("while.cond");
    std::string bodyBB = irBuilder->newBasicBlock("while.body");
    std::string endBB = irBuilder->newBasicBlock("while.end");
    
    // 进入循环上下文
    enterControlFlowContext("while");
    addBreakTarget(endBB);
    
    // 跳转到条件检查
    irBuilder->emitBr(condBB);
    
    // 生成条件检查
    irBuilder->setCurrentBasicBlock(condBB);
    std::string condReg = exprGenerator->generateExpression(stmt->conditions->expression);
    irBuilder->emitCondBr(condReg, bodyBB, endBB);
    
    // 生成循环体
    irBuilder->setCurrentBasicBlock(bodyBB);
    irBuilder->enterScope(); // 循环体作用域
    
    // 注意：BlockExpression 现在由 ExpressionGenerator 处理
    if (auto blockExpr = std::dynamic_pointer_cast<BlockExpression>(stmt->blockexpression)) {
        exprGenerator->generateBlockExpression(blockExpr);
    } else {
        generateStatement(std::make_shared<Statement>(stmt->blockexpression));
    }
    
    irBuilder->exitScope();
    
    // 跳回条件检查
    irBuilder->emitBr(condBB);
    
    // 设置循环结束基本块
    irBuilder->setCurrentBasicBlock(endBB);
    
    // 退出循环上下文
    exitControlFlowContext();
}
```

### 跳转语句生成

#### break 语句

```cpp
void StatementGenerator::generateBreakStatement(std::shared_ptr<BreakExpression> stmt) {
    // 获取当前 break 目标
    std::string breakTarget = getCurrentBreakTarget();
    if (breakTarget.empty()) {
        reportError("break statement outside of loop");
        return;
    }
    
    // 如果有 break 值，生成表达式
    if (stmt->expression) {
        std::string valueReg = exprGenerator->generateExpression(stmt->expression);
        
        // 存储 break 值到特殊位置
        std::string breakValuePtr = irBuilder->getVariableRegister("__break_value");
        if (breakValuePtr.empty()) {
            // 创建 break 值存储
            breakValuePtr = irBuilder->newRegister("__break_value", "ptr");
            irBuilder->emitAlloca(breakValuePtr, "i32");
            irBuilder->setVariableRegister("__break_value", breakValuePtr);
        }
        
        irBuilder->emitStore(valueReg, breakValuePtr, "i32");
    }
    
    // 跳转到循环结束
    irBuilder->emitBr(breakTarget);
    
    // 标记代码为不可达
    handleUnreachableCode();
}
```

#### continue 语句

```cpp
void StatementGenerator::generateContinueStatement(std::shared_ptr<ContinueExpression> stmt) {
    // 获取当前 continue 目标
    std::string continueTarget = getCurrentContinueTarget();
    if (continueTarget.empty()) {
        reportError("continue statement outside of loop");
        return;
    }
    
    // 跳转到循环条件检查或循环开始
    irBuilder->emitBr(continueTarget);
    
    // 标记代码为不可达
    handleUnreachableCode();
}
```

#### return 语句

```cpp
void StatementGenerator::generateReturnStatement(std::shared_ptr<ReturnExpression> stmt) {
    // 获取当前函数返回类型
    std::string returnType = getCurrentFunctionReturnType();
    
    if (stmt->expression) {
        // 有返回值的 return
        std::string valueReg = exprGenerator->generateExpression(stmt->expression);
        std::string valueType = getExpressionType(stmt->expression);
        
        // 类型转换（如果需要）
        if (valueType != returnType) {
            valueReg = generateImplicitConversion(valueReg, valueType, returnType);
        }
        
        irBuilder->emitRet(valueReg, returnType);
    } else {
        // 无返回值的 return
        if (returnType != "void") {
            reportError("Empty return in non-void function");
        }
        irBuilder->emitRet("", "void");
    }
    
    // 标记代码为不可达
    handleUnreachableCode();
}
```

## 控制流上下文管理

### 上下文栈管理

```cpp
void StatementGenerator::enterControlFlowContext(const std::string& loopType) {
    ControlFlowContext context;
    context.loopEndLabel = irBuilder->newBasicBlock(loopType + ".end");
    context.loopBreakLabel = context.loopEndLabel;
    
    controlFlowStack.push_back(context);
}

void StatementGenerator::exitControlFlowContext() {
    if (!controlFlowStack.empty()) {
        controlFlowStack.pop_back();
    }
}

void StatementGenerator::addBreakTarget(const std::string& label) {
    if (!controlFlowStack.empty()) {
        controlFlowStack.back().breakTargets.push_back(label);
        controlFlowStack.back().loopBreakLabel = label;
    }
}

void StatementGenerator::addContinueTarget(const std::string& label) {
    if (!controlFlowStack.empty()) {
        controlFlowStack.back().continueTargets.push_back(label);
    }
}

std::string StatementGenerator::getCurrentBreakTarget() {
    if (controlFlowStack.empty()) {
        return "";
    }
    return controlFlowStack.back().loopBreakLabel;
}

std::string StatementGenerator::getCurrentContinueTarget() {
    if (controlFlowStack.empty()) {
        return "";
    }
    
    // 对于 while 循环，continue 跳转到条件检查
    // 对于 loop 循环，continue 跳转到循环开始
    if (controlFlowStack.back().continueTargets.empty()) {
        return controlFlowStack.back().loopEndLabel;
    }
    return controlFlowStack.back().continueTargets.back();
}
```

### 不可达代码处理

```cpp
void StatementGenerator::handleUnreachableCode() {
    // 创建不可达基本块
    std::string unreachableBB = irBuilder->newBasicBlock("unreachable");
    irBuilder->setCurrentBasicBlock(unreachableBB);
    
    // 添加 unreachable 指令
    irBuilder->emitInstruction("unreachable");
    
    // 设置标志，表示后续代码不可达
    setUnreachableFlag(true);
}
```

## 与其他组件的集成

### 与 ExpressionGenerator 的集成

```cpp
class StatementGenerator {
private:
    std::shared_ptr<ExpressionGenerator> exprGenerator;
    
public:
    void generateLetStatement(std::shared_ptr<LetStatement> stmt) {
        // 使用 ExpressionGenerator 生成初始化表达式
        if (stmt->expression) {
            std::string initReg = exprGenerator->generateExpression(stmt->expression);
            // ... 处理初始化值
        }
    }
};
```

### 与 IRBuilder 的集成

```cpp
class StatementGenerator {
private:
    std::shared_ptr<IRBuilder> irBuilder;
    
public:
    void generateStatement(std::shared_ptr<Statement> stmt) {
        // 使用 IRBuilder 管理基本块和寄存器
        std::string currentBB = irBuilder->getCurrentBasicBlock();
        
        // 生成语句代码
        // ... 语句生成逻辑 ...
        
        // 同步作用域
        irBuilder->syncWithScopeTree();
    }
};
```

### 与 ScopeTree 的集成

```cpp
// 注意：BlockExpression 现在完全由 ExpressionGenerator 处理
// StatementGenerator 不再直接处理 BlockExpression
// ExpressionGenerator 会调用 StatementGenerator::generateStatement 来处理 BlockExpression 中的语句
```

## 错误处理

### 语句错误检测

```cpp
class StatementGenerator {
private:
    bool hasErrors;
    std::vector<std::string> errorMessages;
    
public:
    void reportError(const std::string& message) {
        hasErrors = true;
        errorMessages.push_back(message);
        std::cerr << "Statement Error: " << message << std::endl;
    }
    
    void reportControlFlowError(const std::string& stmtType, const std::string& context) {
        reportError("Invalid " + stmtType + " statement in " + context);
    }
    
    bool hasGenerationErrors() const {
        return hasErrors;
    }
    
    const std::vector<std::string>& getErrorMessages() const {
        return errorMessages;
    }
};
```

### 错误恢复策略

```cpp
void StatementGenerator::generateStatementWithErrorRecovery(std::shared_ptr<Statement> stmt) {
    try {
        generateStatement(stmt);
    } catch (const GenerationError& error) {
        reportError(error.what());
        
        // 生成错误恢复代码
        generateErrorRecoveryCode(stmt);
    }
}

void StatementGenerator::generateErrorRecoveryCode(std::shared_ptr<Statement> stmt) {
    // 根据语句类型生成恢复代码
    if (auto letStmt = std::dynamic_pointer_cast<LetStatement>(stmt)) {
        // 为 let 语句生成默认值
        generateDefaultInitialization(letStmt);
    } else if (auto exprStmt = std::dynamic_pointer_cast<ExpressionStatement>(stmt)) {
        // 跳过表达式语句
        irBuilder->emitComment("Error: skipping expression statement");
    }
    // ... 其他语句类型的恢复策略
}
```

## 性能优化

### 死代码消除

```cpp
class StatementGenerator {
private:
    bool unreachableFlag = false;
    
public:
    void generateStatement(std::shared_ptr<Statement> stmt) {
        // 检查是否在不可达代码中
        if (unreachableFlag) {
            irBuilder->emitComment("Unreachable code skipped");
            return;
        }
        
        // 正常生成语句
        // ... 语句生成逻辑 ...
    }
    
    void setUnreachableFlag(bool flag) {
        unreachableFlag = flag;
    }
};
```

### 基本块合并

```cpp
void StatementGenerator::optimizeBasicBlocks() {
    // 检查相邻的基本块是否可以合并
    std::string currentBB = irBuilder->getCurrentBasicBlock();
    
    if (canMergeWithNext(currentBB)) {
        irBuilder->mergeBasicBlocks(currentBB, getNextBasicBlock(currentBB));
    }
}
```

## 测试策略

### 单元测试

```cpp
// Let 语句测试
TEST(StatementGeneratorTest, LetStatement) {
    setupGenerator();
    
    auto letStmt = createLetStatement("x", "i32", createLiteral("42"));
    generator->generateLetStatement(letStmt);
    
    // 验证生成的 IR 包含正确的 alloca 和 store
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("%x_ptr = alloca i32") != std::string::npos);
    EXPECT_TRUE(ir.find("store i32 42, i32* %x_ptr") != std::string::npos);
}

// 控制流测试
TEST(StatementGeneratorTest, IfStatement) {
    setupGenerator();
    
    auto ifStmt = createIfStatement(
        createLiteral("true"),
        createBlock({createReturnStatement(createLiteral("1"))}),
        createBlock({createReturnStatement(createLiteral("0"))})
    );
    
    generator->generateIfStatement(ifStmt);
    
    // 验证基本块和跳转
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(ir.find("if.then:") != std::string::npos);
    EXPECT_TRUE(ir.find("if.else:") != std::string::npos);
    EXPECT_TRUE(ir.find("if.end:") != std::string::npos);
}
```

### 集成测试

```cpp
// 复杂控制流测试
TEST(StatementGeneratorIntegrationTest, ComplexControlFlow) {
    setupGenerator();
    
    // 测试嵌套的控制流结构
    auto complexStmt = createNestedLoopWithIfAndBreak();
    generator->generateStatement(complexStmt);
    
    // 验证控制流正确性
    std::string ir = getGeneratedIR();
    EXPECT_TRUE(verifyControlFlowCorrectness(ir));
}
```

## 使用示例

### 基本使用

```cpp
// 创建 StatementGenerator
auto irBuilder = std::make_shared<IRBuilder>(scopeTree);
auto exprGenerator = std::make_shared<ExpressionGenerator>(irBuilder, typeMapper, scopeTree, nodeTypeMap);
auto scopeTree = semanticAnalyzer->getScopeTree();
auto nodeTypeMap = typeChecker->getNodeTypeMap();

StatementGenerator stmtGen(irBuilder, exprGenerator, typeMapper, scopeTree, nodeTypeMap);

// 生成 let 语句
auto letStmt = std::make_shared<LetStatement>(
    createIdentifierPattern("x"),
    createType("i32"),
    createLiteral("42")
);
stmtGen.generateLetStatement(letStmt);
// 输出：
// %x_ptr = alloca i32
// store i32 42, i32* %x_ptr

// 生成 if 语句
auto ifStmt = createIfStatement(
    createBinaryExpression(createVariable("x"), ">", createLiteral("0")),
    createBlock({createReturnStatement(createLiteral("1"))}),
    createBlock({createReturnStatement(createLiteral("0"))})
);
stmtGen.generateIfStatement(ifStmt);
// 输出：
// %cond = icmp sgt i32 %x_val, 0
// br i1 %cond, label %if.then, label %if.else
// if.then:
// ret i32 1
// if.else:
// ret i32 0
// if.end:
```

### 高级使用

```cpp
// 生成复杂控制流
auto loopStmt = createLoopStatement(
    createBlock({
        createIfStatement(
            createBinaryExpression(createVariable("i"), ">", createLiteral("10")),
            createBlock({createBreakStatement(createLiteral("i"))}),
            nullptr
        ),
        createExpressionStatement(createUnaryExpression("++", createVariable("i")))
    })
);

stmtGen.generateLoopStatement(loopStmt);
// 输出：
// br label %loop.head
// loop.head:
// %cond = icmp sgt i32 %i_val, 10
// br i1 %cond, label %if.then, label %if.cont
// if.then:
// store i32 %i_val, i32* %__break_value_ptr
// br label %loop.end
// if.cont:
// %i_inc = add i32 %i_val, 1
// store i32 %i_inc, i32* %i_ptr
// br label %loop.head
// loop.end:
```

## 总结

StatementGenerator 组件是 IR 生成阶段的核心组件，提供了完整的语句生成功能：

1. **完整的语句支持**：支持所有 Rx 语言语句类型的 IR 生成
2. **与 ExpressionGenerator 协作**：无缝集成表达式生成，共享寄存器管理
3. **职责明确**：只处理真正的语句（Let、Item、ExpressionStatement），不处理 BlockExpression
4. **控制流管理**：正确处理复杂嵌套控制流结构
5. **作用域感知**：与 ScopeTree 和 IRBuilder 完全集成
6. **错误处理**：完善的错误检测和恢复机制
7. **性能优化**：死代码消除、基本块合并等优化
8. **易于扩展**：清晰的接口设计，便于添加新的语句类型

通过 StatementGenerator，IR 生成器可以将复杂的 Rx 语言语句正确地转换为高效的 LLVM IR 代码，并与 ExpressionGenerator、IRBuilder、TypeMapper 等组件形成完整的代码生成流水线。
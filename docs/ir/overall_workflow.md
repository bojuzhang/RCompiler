# IR 生成整体工作流程

## 概述

IR 生成阶段是 Rx 语言编译器的最后阶段，负责将经过语义分析的 AST 转换为 LLVM-15 兼容的 LLVM IR 文本，并直接输出到 stdout。本文档描述了 IR 生成的整体工作流程、主要阶段和关键决策点。

## 编译器流水线中的位置

```
源代码 → 词法分析 → 语法分析 → 语义分析 → IR 生成 → stdout → LLVM 编译
```

IR 生成阶段是编译器的最后阶段，直接生成 LLVM IR 文本输出，与 builtin.c 联合编译运行。

## 整体架构

### 主要组件

IR 生成器由以下主要组件组成：

1. **IRGenerator（主控制器）**
   - 协调整个 IR 生成过程
   - 管理 IR 文本输出
   - 处理顶层声明

2. **IRBuilder（IR 构建器）**
   - 自定义的 IR 指令生成系统
   - 管理寄存器分配和基本块
   - 生成 LLVM IR 文本指令

3. **TypeMapper（类型映射器）**
   - 将 Rx 类型系统映射到 LLVM 类型系统
   - 处理类型转换和类型检查

4. **ExpressionGenerator（表达式代码生成器）**
   - 为各种表达式生成 LLVM IR 文本
   - 管理表达式值的计算和寄存器分配
   - 处理 BlockExpression 中的语句（通过 StatementGenerator 接口）

5. **StatementGenerator（语句代码生成器）**
   - 为控制流语句生成 LLVM IR 文本
   - 管理基本块和跳转指令
   - 处理 Let 语句、Item 语句和 ExpressionStatement

6. **FunctionCodegen（函数代码生成器）**
   - 处理函数定义和调用
   - 管理函数参数和返回值

7. **BuiltinDeclarator（内置函数声明器）**
   - 生成内置函数的声明
   - 管理外部函数接口

### 数据流

```
AST → 符号表 → 类型信息 → IRGenerator → ExpressionGenerator/StatementGenerator → IRBuilder → LLVM IR 文本 → stdout
                ↓
        控制流信息 → 基本块构建 → 控制流图 → IR 文本输出
```

### 循环依赖解决

由于 AST 节点设计中存在循环依赖关系：
- `Statement` 使用组合模式，包含 `Expression`（控制流语句）
- `BlockExpression` 包含 `Statement` 向量和可选的 `Expression`
- 这导致 `StatementGenerator` 和 `ExpressionGenerator` 之间的循环依赖

**解决策略**：
1. **前向声明**：两个组件互相前向声明
2. **接口分离**：将循环依赖的操作分离到独立接口
3. **延迟初始化**：通过构造函数后初始化解决循环引用
4. **职责明确**：
   - `StatementGenerator`：处理真正的语句（Let、Item、ExpressionStatement）
   - `ExpressionGenerator`：处理表达式和控制流表达式
   - `BlockExpression`：由 `ExpressionGenerator` 调用 `StatementGenerator` 处理其中的语句

## 详细工作流程

### 阶段 1：初始化准备

1. **创建 IR 输出流**
   ```cpp
   std::ostringstream irOutput;
   IRGenerator generator(irOutput);
   ```

2. **设置目标平台**
   ```cpp
   generator.emitTargetTriple("riscv32-unknown-unknown-elf");
   generator.emitDataLayout(); // 如果需要
   ```

3. **声明内置函数**
   ```cpp
   generator.declareBuiltinFunctions();
   // 输出：
   // declare dso_local void @print(ptr)
   // declare dso_local void @println(ptr)
   // declare dso_local void @printInt(i32)
   // declare dso_local void @printlnInt(i32)
   // declare dso_local ptr @getString()
   // declare dso_local i32 @getInt()
   // declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
   // declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
   ```

### 阶段 2：顶层处理

1. **遍历 AST 顶层节点**
   - 处理函数定义
   - 处理全局常量
   - 处理结构体定义

2. **建立全局符号表**
   - 收集所有全局符号
   - 创建对应的符号记录

### 阶段 3：函数生成

对于每个函数：

1. **创建函数定义**
   ```cpp
   // 示例：fn add(a: i32, b: i32) -> i32
   generator.startFunction("add", "i32", {"i32", "i32"}, {"a", "b"});
   // 输出：
   // define i32 @add(i32 %a, i32 %b) {
   ```

2. **创建入口基本块**
   ```cpp
   generator.startBasicBlock("start");
   // 输出：
   // start:
   ```

3. **处理函数体**
   - 为参数分配栈空间（如果需要）
   - 生成函数体语句的 IR
   - 处理返回语句

4. **结束函数定义**
   ```cpp
   generator.endFunction();
   // 输出：
   // }
   ```

### 阶段 4：表达式和语句生成

#### 表达式生成

对于每个表达式：

1. **确定表达式类型**
   - 从语义分析阶段获取类型信息
   - 映射到对应的 LLVM 类型

2. **生成表达式 IR**
   - 字面量：创建常量值
   - 变量：加载变量值
   - 二元操作：生成算术/逻辑指令
   - 函数调用：生成调用指令
   - 控制流表达式：生成基本块和跳转指令

3. **寄存器管理**
   ```cpp
   // 示例：加法表达式 a + b
   std::string leftReg = generateExpression(expr->getLeft());
   std::string rightReg = generateExpression(expr->getRight());
   std::string resultReg = irBuilder->newRegister();
   irBuilder->emitAdd(resultReg, leftReg, rightReg, "i32");
   // 输出：%_3 = add i32 %_4, %_5
   ```

#### 语句生成

对于每个语句：

1. **语句类型识别**
   - Let 语句：变量声明和初始化
   - ExpressionStatement：表达式语句（有分号）
   - Item 语句：函数定义、结构体定义等

2. **循环依赖处理**
   ```cpp
   // ExpressionGenerator 处理 BlockExpression 时调用 StatementGenerator
   std::string ExpressionGenerator::generateBlockExpression(std::shared_ptr<BlockExpression> expr) {
       if (!statementGenerator) {
           reportError("StatementGenerator not set");
           return generateUnitValue();
       }
       
       // 处理块内的所有语句
       for (const auto& stmt : expr->statements) {
           statementGenerator->generateStatement(stmt);
       }
       
       // 处理结尾表达式
       if (expr->expressionwithoutblock) {
           return generateExpression(expr->expressionwithoutblock);
       } else {
           return generateUnitValue();
       }
   }
   ```

3. **寄存器和作用域管理**
   ```cpp
   // StatementGenerator 处理 Let 语句
   void StatementGenerator::generateLetStatement(std::shared_ptr<LetStatement> stmt) {
       // 分配变量空间
       std::string varPtrReg = irBuilder->newRegister(varName, "ptr");
       irBuilder->emitAlloca(varPtrReg, varType);
       
       // 生成初始化表达式（调用 ExpressionGenerator）
       if (stmt->expression && exprGenerator) {
           std::string initReg = exprGenerator->generateExpression(stmt->expression);
           irBuilder->emitStore(initReg, varPtrReg, varType);
       }
       
       // 注册变量到作用域
       scopeTree->InsertSymbol(varName, symbol);
       irBuilder->setVariableRegister(varName, varPtrReg);
   }
   ```

### 阶段 5：语句生成

对于每个语句：

1. **控制流语句**
   ```cpp
   // if 语句
   std::string condReg = generateExpression(ifStmt->getCondition());
   std::string thenBB = generator.newBasicBlock("if.then");
   std::string elseBB = generator.newBasicBlock("if.else");
   std::string endBB = generator.newBasicBlock("if.end");
   
   generator.emitInstruction("br i1 %" + condReg + ", label %" + thenBB + ", label %" + elseBB);
   // 输出：br i1 %_3, label %bb4, label %bb5
   ```

2. **声明语句**
   ```cpp
   // 变量声明
   std::string varName = varDecl->getName();
   std::string llvmType = typeMapper.mapType(varDecl->getType());
   std::string allocaReg = generator.newRegister();
   generator.emitInstruction("%" + allocaReg + " = alloca " + llvmType + ", align 4");
   // 输出：%_6 = alloca i32, align 4
   ```

3. **赋值语句**
   ```cpp
   // 赋值语句 x = value
   std::string valueReg = generateExpression(assignStmt->getValue());
   std::string varPtr = getVariablePointer(assignStmt->getVariableName());
   generator.emitInstruction("store i32 %" + valueReg + ", " + varPtr + ", align 4");
   // 输出：store i32 %_7, i32* %_6, align 4
   ```

### 阶段 6：内存管理

1. **栈分配**
   ```cpp
   // 局部变量分配
   std::string allocaReg = newRegister();
   emitInstruction("%" + allocaReg + " = alloca " + llvmType + ", align " + alignment);
   // 输出：%_8 = alloca i32, align 4
   ```

2. **堆分配**
   ```cpp
   // 调用 malloc
   std::string sizeReg = generateExpression(sizeExpr);
   std::string mallocReg = newRegister();
   emitInstruction("%" + mallocReg + " = call ptr @malloc(i32 %" + sizeReg + ")");
   // 输出：%_9 = call ptr @malloc(i32 %_10)
   ```

3. **内存操作**
   ```cpp
   // 调用 memset
   std::string memsetReg = newRegister();
   emitInstruction("%" + memsetReg + " = call ptr @builtin_memset(ptr " + destPtr + ", i8 0, i32 " + size + ")");
   // 输出：%_11 = call ptr @builtin_memset(ptr %_12, i8 0, i32 64)
   ```

### 阶段 7：内置函数处理

1. **函数声明**
   ```cpp
   // 声明所有内置函数
   void declareBuiltinFunctions() {
       emitInstruction("declare dso_local void @print(ptr)");
       emitInstruction("declare dso_local void @println(ptr)");
       emitInstruction("declare dso_local void @printInt(i32)");
       emitInstruction("declare dso_local void @printlnInt(i32)");
       emitInstruction("declare dso_local ptr @getString()");
       emitInstruction("declare dso_local i32 @getInt()");
       emitInstruction("declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)");
       emitInstruction("declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)");
   }
   ```

2. **函数调用**
   ```cpp
   // 生成 printlnInt 调用
   std::string valueReg = generateExpression(callExpr->getArgument(0));
   emitInstruction("call void @printlnInt(i32 %" + valueReg + ")");
   // 输出：call void @printlnInt(i32 %_13)
   ```

## 自定义 IR 构建器设计

### IRBuilder 核心功能

```cpp
class IRBuilder {
public:
    IRBuilder(std::ostream& output) : output(output), regCounter(0), bbCounter(0) {}
    
    // 寄存器管理
    std::string newRegister() {
        return "_" + std::to_string(++regCounter);
    }
    
    // 基本块管理
    std::string newBasicBlock(const std::string& prefix) {
        return prefix + std::to_string(++bbCounter);
    }
    
    // 指令生成
    void emitInstruction(const std::string& instruction) {
        output << "  " << instruction << "\n";
    }
    
    void emitLabel(const std::string& label) {
        output << label << ":\n";
    }
    
    // 类型指令
    void emitAlloca(const std::string& reg, const std::string& type, int align) {
        emitInstruction("%" + reg + " = alloca " + type + ", align " + std::to_string(align));
    }
    
    void emitStore(const std::string& value, const std::string& ptr, const std::string& type, int align) {
        emitInstruction("store " + type + " %" + value + ", " + ptr + ", align " + std::to_string(align));
    }
    
    void emitLoad(const std::string& reg, const std::string& ptr, const std::string& type, int align) {
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
    
    // 比较指令
    void emitIcmp(const std::string& result, const std::string& cond, const std::string& left, const std::string& right, const std::string& type) {
        emitInstruction("%" + result + " = icmp " + cond + " " + type + " %" + left + ", %" + right);
    }
    
    // 控制流指令
    void emitBr(const std::string& condition, const std::string& thenLabel, const std::string& elseLabel) {
        emitInstruction("br i1 %" + condition + ", label %" + thenLabel + ", label %" + elseLabel);
    }
    
    void emitBr(const std::string& label) {
        emitInstruction("br label %" + label);
    }
    
    void emitRet(const std::string& value, const std::string& type) {
        if (type == "void") {
            emitInstruction("ret void");
        } else {
            emitInstruction("ret " + type + " %" + value);
        }
    }
    
    // 函数调用
    void emitCall(const std::string& result, const std::string& funcName, const std::vector<std::string>& args, const std::string& returnType) {
        std::string call = "call " + returnType + " @" + funcName + "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) call += ", ";
            call += args[i];
        }
        call += ")";
        if (!result.empty()) {
            emitInstruction("%" + result + " = " + call);
        } else {
            emitInstruction(call);
        }
    }

private:
    std::ostream& output;
    size_t regCounter;
    size_t bbCounter;
};
```

## 关键设计决策

### 1. 直接文本输出

- 不使用 LLVM C++ API
- 直接生成 LLVM IR 文本
- 输出到 stdout，与 builtin.c 联合编译

### 2. 自定义寄存器管理

- 自动分配寄存器名称（_1, _2, _3...）
- 跟踪寄存器类型和生命周期
- 支持寄存器重用

### 3. 基本块管理

- 自动生成基本块标签
- 维护当前基本块上下文
- 支持嵌套控制流

### 4. 类型安全

- 严格遵循 Rx 语言的类型系统
- 在编译时进行类型检查
- 生成类型正确的 LLVM IR

## 错误处理策略

### 1. 编译时错误

- 类型不匹配
- 未定义的符号
- 语法错误（应该在语义分析阶段捕获）

### 2. 错误报告

- 提供详细的错误信息
- 包含源代码位置
- 建议修复方案

### 3. 错误恢复

- 尝试继续生成 IR
- 插入默认值或跳过错误代码
- 确保生成的 IR 语法正确

## 输出格式

### 1. 目标三元组
```
target triple = "riscv32-unknown-unknown-elf"
```

### 2. 内置函数声明
```
declare dso_local void @print(ptr)
declare dso_local void @println(ptr)
declare dso_local void @printInt(i32)
declare dso_local void @printlnInt(i32)
declare dso_local ptr @getString()
declare dso_local i32 @getInt()
declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
```

### 3. 函数定义格式
```
define i32 @main() {
start:
  %_1 = alloca [4 x i8], align 4
  %_2 = alloca [68 x i8], align 4
  call void @new(ptr sret([68 x i8]) align 4 %_2)
  store i32 0, ptr %_1, align 4
  br label %bb2

bb2:
  ; ... 函数体
}
```

## 性能考虑

### 1. 代码质量

- 生成简洁高效的 IR
- 避免不必要的临时变量
- 合理使用寄存器

### 2. 输出效率

- 使用字符串流缓冲输出
- 减少频繁的 I/O 操作
- 优化字符串拼接

### 3. 内存使用

- 及时释放临时对象
- 避免内存泄漏
- 优化数据结构

## 测试策略

### 1. 单元测试

- 测试各个组件的独立功能
- 验证类型映射的正确性
- 测试表达式和语句的 IR 生成

### 2. 集成测试

- 测试完整的 IR 生成流程
- 使用标准测试用例
- 验证生成的 IR 可正确编译

### 3. 回归测试

- 确保新功能不破坏现有功能
- 使用自动化测试框架
- 定期运行完整测试套件

## 与现有代码的集成

### 1. 语义分析接口

```cpp
class IRGenerator {
public:
    IRGenerator(const SymbolTable& symbolTable, const TypeChecker& typeChecker);
    bool generateIR(const std::vector<std::unique_ptr<Item>>& items);
    std::string getIROutput() const { return output.str(); }

private:
    const SymbolTable& symbolTable;
    const TypeChecker& typeChecker;
    std::ostringstream output;
    IRBuilder builder;
    TypeMapper typeMapper;
    // ... 其他组件
};
```

### 2. 主程序集成

```cpp
int main(int argc, char* argv[]) {
    // ... 词法分析、语法分析、语义分析
    
    // IR 生成
    IRGenerator irGenerator(symbolTable, typeChecker);
    bool success = irGenerator.generateIR(ast);
    
    if (success) {
        std::cout << irGenerator.getIROutput();
        return 0;
    } else {
        std::cerr << "IR generation failed" << std::endl;
        return 1;
    }
}
```

## 总结

重新设计的 IR 生成模块具有以下特点：

1. **直接文本输出**：不依赖 LLVM C++ API，直接生成 LLVM IR 文本
2. **自定义构建器**：实现自己的 IRBuilder 系统来管理寄存器和基本块
3. **内置函数声明**：只声明内置函数，与 builtin.c 联合编译
4. **stdout 输出**：直接输出到 stdout，便于管道操作
5. **模块化设计**：保持清晰的组件分离和接口定义

这种设计确保了 IR 生成器的独立性、可控性和与现有编译器架构的良好集成。
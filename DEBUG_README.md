# RCompiler basic20.rx 段错误调试指南

## 概述

本调试套件用于诊断RCompiler在处理basic20.rx测试用例时发生的类型检查阶段段错误。问题主要集中在`src/typecheck.cpp`中`inferCallExpressionType()`函数第646-647行附近的空指针解引用。

## 文件说明

### 调试代码修改
- `src/typecheck.cpp`: 添加了符号查找过程的详细调试信息
- `src/symboltable.cpp`: 添加了作用域树管理的调试信息
- `src/symbolcollection.cpp`: 添加了符号收集过程的调试信息
- `src/semantic.cpp`: 添加了语义分析阶段转换的调试信息

### 调试工具脚本
- `debug_basic20.sh`: 完整的自动化调试脚本
- `test_basic20_simple.sh`: 简化的测试脚本
- `analyze_debug_log.py`: 调试日志分析工具

## 使用方法

### 1. 快速测试

```bash
# 运行简化测试
./test_basic20_simple.sh
```

### 2. 完整调试

```bash
# 运行完整调试脚本
./debug_basic20.sh
```

### 3. 分析调试日志

```bash
# 分析生成的调试日志
python3 analyze_debug_log.py build/debug_basic20.log
```

### 4. 手动调试

```bash
# 进入构建目录
cd build

# 编译调试版本
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 使用文件流重定向运行测试
./rcompiler < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx > debug_output.txt 2>&1

# 分析输出
cat debug_output.txt
```

## 关键调试点

### 1. 作用域树状态
- 监控`scopeTree`指针是否为空
- 检查`currentNode`指针的有效性
- 验证作用域进入和退出的平衡性

### 2. 符号查找过程
- 追踪`findSymbol()`调用
- 监控`lookupSymbol()`执行
- 验证函数符号`mul`是否正确收集

### 3. 类型推断流程
- 观察`inferCallExpressionType()`执行
- 检查函数调用解析过程
- 验证返回类型获取

## 预期调试输出

### 正常情况下的输出模式
```
DEBUG: collectFunctionSymbol() called for function: 'mul'
DEBUG: Symbol insertion succeeded
DEBUG: findSymbol() called with name: 'mul'
DEBUG: scopeTree->lookupSymbol() returned: valid symbol
DEBUG: Found function symbol
DEBUG: Returning symbol type: [[i32; 3]; 3]
```

### 异常情况下的输出模式
```
ERROR: scopeTree is null in findSymbol()!
ERROR: Current scope is null in findSymbol()!
ERROR: currentNode is null in ScopeTree::lookupSymbol()!
```

## 常见问题诊断

### 1. 作用域树空指针
**症状**: `ERROR: scopeTree is null in findSymbol()!`
**原因**: 符号收集和类型检查阶段之间的作用域树传递失败
**解决**: 检查`CompleteSemanticAnalyzer::runSymbolCollection()`和`runTypeChecking()`之间的状态传递

### 2. 当前作用域空指针
**症状**: `ERROR: Current scope is null in findSymbol()!`
**原因**: 作用域栈管理错误，可能在作用域退出时丢失当前作用域
**解决**: 检查`ScopeTree::exitScope()`实现

### 3. 函数符号未找到
**症状**: `findSymbol() returned: null` 对于函数'mul'
**原因**: 符号收集阶段未能正确收集函数符号
**解决**: 检查`SymbolCollector::collectFunctionSymbol()`实现

## GDB调试

### 手动GDB调试
```bash
gdb ./rcompiler
(gdb) set args < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx
(gdb) break TypeChecker::inferCallExpressionType
(gdb) break TypeChecker::findSymbol
(gdb) run
```

### 自动化GDB调试
```bash
gdb -batch -x debug_basic20.gdb ./rcompiler
```

## Valgrind内存检查

```bash
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
         --track-origins=yes --verbose --log-file=valgrind.log \
         ./rcompiler < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx
```

## 注意事项

1. **禁止修改功能性代码**: 所有调试代码仅添加打印语句，不修改算法逻辑
2. **文件流重定向**: 必须使用文件流重定向进行测试，以重现段错误
3. **调试版本**: 确保使用Debug模式编译以获取完整的调试信息
4. **日志分析**: 使用`analyze_debug_log.py`快速定位问题根因

## 预期根因

基于代码分析，最可能的段错误原因是：
- 作用域树在符号收集和类型检查阶段之间状态不一致
- `scopeTree->currentNode`在类型检查时变为空指针
- 在`inferCallExpressionType()`中调用`findSymbol("mul")`时访问空指针

## 验证步骤

1. 运行调试脚本确认段错误发生
2. 分析调试日志定位空指针出现位置
3. 检查作用域管理逻辑找到状态丢失原因
4. 验证符号收集阶段的作用域树构建
5. 确认类型检查阶段的作用域树传递
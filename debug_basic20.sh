#!/bin/bash

echo "=== RCompiler basic20.rx 段错误调试脚本 ==="
echo "当前时间: $(date)"
echo "工作目录: $(pwd)"
echo ""

# 设置变量
RCOMPILER_BIN="./code"
TEST_FILE="../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx"
DEBUG_LOG="debug_basic20.log"
GDB_SCRIPT="debug_basic20.gdb"

# 检查当前目录并调整路径
if [ ! -f "$TEST_FILE" ]; then
    TEST_FILE="RCompiler-Testcases/semantic-1/src/basic20/basic20.rx"
fi

# 检查文件是否存在
if [ ! -f "$TEST_FILE" ]; then
    echo "错误: 测试文件 $TEST_FILE 不存在"
    exit 1
fi

echo "=== 1. 编译调试版本 ==="
cd build
make clean
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

if [ $? -ne 0 ]; then
    echo "错误: 编译失败"
    exit 1
fi

echo "编译完成"
echo ""

echo "=== 2. 运行调试版本并收集输出 ==="
echo "运行命令: $RCOMPILER_BIN < $TEST_FILE > $DEBUG_LOG 2>&1"

# 运行程序并重定向输出
$RCOMPILER_BIN < $TEST_FILE > $DEBUG_LOG 2>&1
EXIT_CODE=$?

echo "程序退出代码: $EXIT_CODE"
echo ""

echo "=== 3. 分析调试输出 ==="
if [ -f "$DEBUG_LOG" ]; then
    echo "调试日志大小: $(wc -l < $DEBUG_LOG) 行"
    echo ""
    echo "=== 最后50行调试输出 ==="
    tail -50 "$DEBUG_LOG"
    echo ""
    
    # 查找关键错误信息
    echo "=== 查找关键错误信息 ==="
    grep -E "(ERROR|SEGMENTATION|null pointer|scopeTree.*null|currentNode.*null)" "$DEBUG_LOG" || echo "未找到关键错误信息"
    echo ""
    
    # 查找符号查找过程
    echo "=== 符号查找过程 ==="
    grep -E "(findSymbol|lookupSymbol|Function.*mul)" "$DEBUG_LOG" || echo "未找到符号查找信息"
    echo ""
    
    # 查找作用域管理信息
    echo "=== 作用域管理信息 ==="
    grep -E "(enterScope|exitScope|getCurrentScope)" "$DEBUG_LOG" || echo "未找到作用域管理信息"
else
    echo "错误: 调试日志文件未生成"
fi

echo ""
echo "=== 4. 如果发生段错误，使用GDB获取更多信息 ==="

# 创建GDB脚本
cat > "$GDB_SCRIPT" << 'EOF'
set pagination off
set logging file gdb_basic20.log
set logging on

# 设置运行参数
set args < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx

# 设置断点
break TypeChecker::inferCallExpressionType
break TypeChecker::findSymbol
break ScopeTree::lookupSymbol
break ScopeTree::getCurrentScope
break SymbolCollector::collectFunctionSymbol

echo "=== 开始调试运行 ==="
run

# 如果段错误，打印详细信息
if $_siginfo.si_signo == SIGSEGV
    echo "=== 段错误检测到 ==="
    bt full
    info locals
    p scopeTree
    p scopeTree->currentNode
    p scopeTree->root
    echo "=== 作用域栈状态 ==="
    p scopeTree->scopeStack
    echo "=== 当前作用域符号表 ==="
    if scopeTree && scopeTree->currentNode
        p scopeTree->currentNode->symbols
    end
end

echo "=== 调试完成 ==="
quit
EOF

echo "GDB脚本已创建: $GDB_SCRIPT"
echo "如需使用GDB，请运行: gdb -batch -x $GDB_SCRIPT $RCOMPILER_BIN"

echo ""
echo "=== 5. 使用Valgrind进行内存检查 ==="
if command -v valgrind &> /dev/null; then
    echo "运行Valgrind检查..."
    valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
             --track-origins=yes --verbose --log-file=valgrind_basic20.log \
             $RCOMPILER_BIN < $TEST_FILE
    echo "Valgrind日志已保存到: valgrind_basic20.log"
else
    echo "Valgrind未安装，跳过内存检查"
fi

echo ""
echo "=== 调试完成 ==="
echo "请查看以下文件获取详细信息:"
echo "  - $DEBUG_LOG: 程序调试输出"
echo "  - gdb_basic20.log: GDB调试日志（如果运行了GDB）"
echo "  - valgrind_basic20.log: Valgrind内存检查日志（如果运行了Valgrind）"
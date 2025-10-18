#!/bin/bash

echo "=== 简化的basic20.rx测试脚本 ==="
echo "使用文件流重定向进行测试"
echo ""

# 进入构建目录
cd build

# 编译调试版本
echo "编译调试版本..."
make clean && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

if [ $? -ne 0 ]; then
    echo "编译失败"
    exit 1
fi

echo "编译完成，开始测试..."
echo ""

# 使用文件流重定向运行测试
echo "运行命令: ./code < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx"
echo "=== 开始运行 ==="

./code < ../RCompiler-Testcases/semantic-1/src/basic20/basic20.rx

echo "=== 运行结束 ==="
echo ""
echo "如果发生段错误，请运行 ./debug_basic20.sh 获取详细调试信息"
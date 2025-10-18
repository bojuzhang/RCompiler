#!/usr/bin/env python3
"""
RCompiler调试日志分析脚本
用于分析basic20.rx测试用例的调试输出，快速定位段错误原因
"""

import sys
import re
from collections import defaultdict

def analyze_debug_log(log_file):
    """分析调试日志文件"""
    print(f"=== 分析调试日志: {log_file} ===")
    
    try:
        with open(log_file, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"错误: 日志文件 {log_file} 不存在")
        return
    
    print(f"日志总行数: {len(lines)}")
    print()
    
    # 分析关键模式
    patterns = {
        'ERROR': r'ERROR:',
        'scopeTree_null': r'scopeTree.*null',
        'currentNode_null': r'currentNode.*null',
        'findSymbol': r'findSymbol.*called',
        'lookupSymbol': r'lookupSymbol.*called',
        'inferCallExpressionType': r'inferCallExpressionType.*called',
        'collectFunctionSymbol': r'collectFunctionSymbol.*called',
        'enterScope': r'enterScope.*called',
        'exitScope': r'exitScope.*called',
        'mul_function': r'mul',
        'segmentation': r'SEGMENTATION|segmentation'
    }
    
    # 统计模式出现次数
    pattern_counts = defaultdict(int)
    pattern_lines = defaultdict(list)
    
    for i, line in enumerate(lines, 1):
        for pattern_name, pattern_regex in patterns.items():
            if re.search(pattern_regex, line):
                pattern_counts[pattern_name] += 1
                if pattern_name in ['ERROR', 'scopeTree_null', 'currentNode_null', 'segmentation']:
                    pattern_lines[pattern_name].append((i, line.strip()))
    
    # 打印统计结果
    print("=== 模式统计 ===")
    for pattern_name, count in pattern_counts.items():
        print(f"{pattern_name}: {count} 次")
    print()
    
    # 打印关键错误信息
    print("=== 关键错误信息 ===")
    if pattern_lines['ERROR']:
        print("ERROR 信息:")
        for line_num, line in pattern_lines['ERROR']:
            print(f"  行 {line_num}: {line}")
        print()
    
    if pattern_lines['scopeTree_null']:
        print("scopeTree 空指针信息:")
        for line_num, line in pattern_lines['scopeTree_null']:
            print(f"  行 {line_num}: {line}")
        print()
    
    if pattern_lines['currentNode_null']:
        print("currentNode 空指针信息:")
        for line_num, line in pattern_lines['currentNode_null']:
            print(f"  行 {line_num}: {line}")
        print()
    
    # 分析符号查找过程
    print("=== 符号查找过程分析 ===")
    find_symbol_calls = []
    lookup_symbol_calls = []
    
    for i, line in enumerate(lines, 1):
        if 'findSymbol() called with name' in line:
            match = re.search(r"name: '([^']+)'", line)
            if match:
                find_symbol_calls.append((i, match.group(1)))
        elif 'lookupSymbol() called with name' in line:
            match = re.search(r"name: '([^']+)'", line)
            if match:
                lookup_symbol_calls.append((i, match.group(1)))
    
    print(f"findSymbol 调用次数: {len(find_symbol_calls)}")
    for line_num, name in find_symbol_calls:
        print(f"  行 {line_num}: 查找符号 '{name}'")
    
    print(f"lookupSymbol 调用次数: {len(lookup_symbol_calls)}")
    for line_num, name in lookup_symbol_calls:
        print(f"  行 {line_num}: 查找符号 '{name}'")
    print()
    
    # 分析作用域管理
    print("=== 作用域管理分析 ===")
    enter_scope_calls = []
    exit_scope_calls = []
    
    for i, line in enumerate(lines, 1):
        if 'enterScope() called' in line:
            match = re.search(r"type: (\d+)", line)
            scope_type = match.group(1) if match else "unknown"
            enter_scope_calls.append((i, scope_type))
        elif 'exitScope() called' in line:
            exit_scope_calls.append((i, "exit"))
    
    print(f"enterScope 调用次数: {len(enter_scope_calls)}")
    print(f"exitScope 调用次数: {len(exit_scope_calls)}")
    
    # 检查作用域平衡
    if len(enter_scope_calls) != len(exit_scope_calls):
        print("警告: 作用域进入和退出不匹配!")
        print(f"  进入: {len(enter_scope_calls)}, 退出: {len(exit_scope_calls)}")
    else:
        print("作用域进入和退出匹配")
    print()
    
    # 分析函数符号收集
    print("=== 函数符号收集分析 ===")
    function_symbols = []
    for i, line in enumerate(lines, 1):
        if 'collectFunctionSymbol() called for function' in line:
            match = re.search(r"function: '([^']+)'", line)
            if match:
                function_symbols.append((i, match.group(1)))
    
    print(f"收集的函数符号数量: {len(function_symbols)}")
    for line_num, func_name in function_symbols:
        print(f"  行 {line_num}: 函数 '{func_name}'")
    print()
    
    # 查找最后几行，了解崩溃前的状态
    print("=== 最后10行日志 ===")
    for line in lines[-10:]:
        print(f"  {line.strip()}")
    print()
    
    # 给出诊断建议
    print("=== 诊断建议 ===")
    if pattern_counts['scopeTree_null'] > 0:
        print("⚠️  发现 scopeTree 空指针问题")
        print("   建议: 检查符号收集和类型检查阶段之间的作用域树传递")
    
    if pattern_counts['currentNode_null'] > 0:
        print("⚠️  发现 currentNode 空指针问题")
        print("   建议: 检查作用域栈管理，确保作用域正确进入和退出")
    
    if pattern_counts['ERROR'] > 0:
        print("⚠️  发现错误信息")
        print("   建议: 重点关注错误信息中提到的问题")
    
    if 'mul' not in [name for _, name in function_symbols]:
        print("⚠️  未找到 mul 函数符号")
        print("   建议: 检查函数符号收集过程")
    
    if len(enter_scope_calls) != len(exit_scope_calls):
        print("⚠️  作用域不平衡")
        print("   建议: 检查作用域管理逻辑")
    
    print("分析完成")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("用法: python3 analyze_debug_log.py <debug_log_file>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    analyze_debug_log(log_file)
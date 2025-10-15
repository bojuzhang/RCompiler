#!/usr/bin/env python3
"""
RCompiler Semantic-1 测试评测脚本
自动评测 RCompiler-Testcases/semantic-1 目录下的所有测试点
"""

import os
import re
import subprocess
import sys
import json
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from collections import defaultdict

class TestEvaluator:
    def __init__(self, test_dir: str = "../RCompiler-Testcases/semantic-1", compiler_path: str = "../build/code", timeout: int = 15):
        self.test_dir = Path(test_dir)
        self.compiler_path = Path(compiler_path)
        self.timeout = timeout
        self.test_results = []
        self.start_time = None
        self.end_time = None
        
    def find_all_rx_files(self) -> List[Path]:
        """查找所有 .rx 测试文件"""
        rx_files = []
        for root, dirs, files in os.walk(self.test_dir):
            for file in files:
                if file.endswith('.rx'):
                    rx_files.append(Path(root) / file)
        return sorted(rx_files)
    
    def extract_verdict_from_file(self, file_path: Path) -> Optional[int]:
        """从文件注释中提取 Verdict"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 查找 Verdict 行
            verdict_match = re.search(r'Verdict:\s*(\w+)', content)
            if verdict_match:
                verdict = verdict_match.group(1)
                if verdict == "Success" or verdict == "Pass":
                    return 0
                elif verdict == "Fail":
                    return -1
                else:
                    print(f"警告: {file_path} 中未知的 Verdict 值: {verdict}")
                    return None
            else:
                print(f"警告: {file_path} 中未找到 Verdict")
                return None
        except Exception as e:
            print(f"错误: 读取文件 {file_path} 时出错: {e}")
            return None
    
    def run_compiler(self, file_path: Path) -> int:
        """运行编译器分析测试文件"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 运行编译器
            result = subprocess.run(
                [str(self.compiler_path)],
                input=content,
                text=True,
                capture_output=True,
                timeout=self.timeout  # 15秒超时
            )
            
            # 解析输出 - 从多行输出中提取最后的数字
            output_lines = result.stdout.strip().split('\n')
            if output_lines:
                # 找到最后一个非空行
                last_line = ""
                for line in reversed(output_lines):
                    if line.strip():
                        last_line = line.strip()
                        break
                
                if last_line == "0":
                    return 0
                elif last_line == "-1":
                    return -1
                else:
                    print(f"警告: {file_path} 编译器输出异常: {last_line}")
                    return -2  # 表示异常
            else:
                print(f"警告: {file_path} 编译器输出为空")
                return -2
                
        except subprocess.TimeoutExpired:
            print(f"错误: {file_path} 编译器运行超时")
            return -2
        except Exception as e:
            print(f"错误: 运行编译器处理 {file_path} 时出错: {e}")
            return -2
    
    def evaluate_single_test(self, file_path: Path) -> Dict:
        """评测单个测试文件"""
        expected = self.extract_verdict_from_file(file_path)
        actual = self.run_compiler(file_path)
        
        # 判断测试结果
        if expected is None:
            status = "ERROR"
            passed = False
        elif actual == -2:
            status = "ERROR"
            passed = False
        elif expected == actual:
            status = "PASS"
            passed = True
        else:
            status = "FAIL"
            passed = False
        
        return {
            'file_path': file_path,
            'test_name': file_path.parent.name,
            'expected': expected,
            'actual': actual,
            'status': status,
            'passed': passed
        }
    
    def run_all_tests(self) -> None:
        """运行所有测试"""
        self.start_time = datetime.now()
        print("正在查找测试文件...")
        rx_files = self.find_all_rx_files()
        print(f"找到 {len(rx_files)} 个测试文件\n")
        
        print("开始评测...")
        for i, file_path in enumerate(rx_files, 1):
            print(f"评测进度: {i}/{len(rx_files)} - {file_path.name}")
            result = self.evaluate_single_test(file_path)
            self.test_results.append(result)
        
        self.end_time = datetime.now()
        print("\n评测完成!")
    
    def print_results(self) -> None:
        """打印评测结果"""
        if not self.test_results:
            print("没有测试结果")
            return
        
        stats = self.get_statistics()
        
        print("\n" + "="*80)
        print("评测结果汇总")
        print("="*80)
        print(f"总测试数: {stats['total_tests']}")
        print(f"通过: {stats['passed_tests']} ({stats['pass_rate']:.1f}%)")
        print(f"失败: {stats['failed_tests']} ({stats['fail_rate']:.1f}%)")
        print(f"错误: {stats['error_tests']} ({stats['error_rate']:.1f}%)")
        print(f"执行时间: {stats['execution_time']:.2f} 秒")
        
        # 按期望结果分类统计
        print("\n" + "="*80)
        print("按期望结果分类统计")
        print("="*80)
        
        print(f"\n期望结果为 0 的测试用例 (共 {stats['expected_0_stats']['total']} 个):")
        print(f"  实际为 0 (正确): {stats['expected_0_stats']['actual_0']} ({stats['expected_0_stats']['actual_0']/stats['expected_0_stats']['total']*100:.1f}%)")
        print(f"  实际为 -1 (错误): {stats['expected_0_stats']['actual_minus1']} ({stats['expected_0_stats']['actual_minus1']/stats['expected_0_stats']['total']*100:.1f}%)")
        print(f"  实际为 -2 (异常): {stats['expected_0_stats']['actual_minus2']} ({stats['expected_0_stats']['actual_minus2']/stats['expected_0_stats']['total']*100:.1f}%)")
        
        print(f"\n期望结果为 -1 的测试用例 (共 {stats['expected_minus1_stats']['total']} 个):")
        print(f"  实际为 0 (错误): {stats['expected_minus1_stats']['actual_0']} ({stats['expected_minus1_stats']['actual_0']/stats['expected_minus1_stats']['total']*100:.1f}%)")
        print(f"  实际为 -1 (正确): {stats['expected_minus1_stats']['actual_minus1']} ({stats['expected_minus1_stats']['actual_minus1']/stats['expected_minus1_stats']['total']*100:.1f}%)")
        print(f"  实际为 -2 (异常): {stats['expected_minus1_stats']['actual_minus2']} ({stats['expected_minus1_stats']['actual_minus2']/stats['expected_minus1_stats']['total']*100:.1f}%)")
        
        # 按模块分组统计（显示前10个）
        print("\n" + "="*80)
        print("按测试模块分组统计 (前10个)")
        print("="*80)
        print(f"{'模块':<15} {'总数':<6} {'通过':<6} {'失败':<6} {'错误':<6} {'通过率':<8}")
        print("-" * 65)
        
        sorted_modules = sorted(stats['module_stats'].items(),
                              key=lambda x: x[1]['passed']/x[1]['total'] if x[1]['total'] > 0 else 0,
                              reverse=True)
        
        for module, module_stat in sorted_modules[:10]:
            pass_rate = module_stat['passed'] / module_stat['total'] * 100 if module_stat['total'] > 0 else 0
            print(f"{module:<15} {module_stat['total']:<6} {module_stat['passed']:<6} "
                  f"{module_stat['failed']:<6} {module_stat['error']:<6} {pass_rate:<8.1f}%")
        
        # 详细结果
        print("\n" + "="*80)
        print("详细结果")
        print("="*80)
        print(f"{'测试名称':<20} {'期望':<8} {'实际':<8} {'状态':<8} {'文件路径'}")
        print("-" * 80)
        
        for result in self.test_results:
            expected_str = str(result['expected']) if result['expected'] is not None else "N/A"
            actual_str = str(result['actual']) if result['actual'] != -2 else "ERROR"
            
            # 根据状态设置颜色
            status = result['status']
            if status == "PASS":
                status_display = f"✓ {status}"
            elif status == "FAIL":
                status_display = f"✗ {status}"
            else:
                status_display = f"⚠ {status}"
            
            print(f"{result['test_name']:<20} {expected_str:<8} {actual_str:<8} {status_display:<8} {result['file_path']}")
        
        # 失败和错误的测试详情
        failed_or_error = [r for r in self.test_results if not r['passed']]
        if failed_or_error:
            print("\n" + "="*80)
            print("失败/错误测试详情")
            print("="*80)
            
            for result in failed_or_error:
                print(f"\n测试: {result['file_path']}")
                print(f"期望: {result['expected']}")
                print(f"实际: {result['actual']}")
                print(f"状态: {result['status']}")
                
                # 如果是错误，显示更多信息
                if result['status'] == 'ERROR':
                    if result['expected'] is None:
                        print("原因: 无法从文件注释中提取 Verdict")
                    elif result['actual'] == -2:
                        print("原因: 编译器运行异常或超时")
    
    def get_statistics(self) -> Dict:
        """获取详细统计信息"""
        if not self.test_results:
            return {}
        
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r['passed'])
        failed_tests = sum(1 for r in self.test_results if not r['passed'] and r['status'] != 'ERROR')
        error_tests = sum(1 for r in self.test_results if r['status'] == 'ERROR')
        
        # 按期望结果分类统计
        expected_0_tests = [r for r in self.test_results if r['expected'] == 0]
        expected_minus1_tests = [r for r in self.test_results if r['expected'] == -1]
        
        # 期望为0的测试结果分布
        expected_0_stats = {
            'total': len(expected_0_tests),
            'actual_0': sum(1 for r in expected_0_tests if r['actual'] == 0),
            'actual_minus1': sum(1 for r in expected_0_tests if r['actual'] == -1),
            'actual_minus2': sum(1 for r in expected_0_tests if r['actual'] == -2),
        }
        
        # 期望为-1的测试结果分布
        expected_minus1_stats = {
            'total': len(expected_minus1_tests),
            'actual_0': sum(1 for r in expected_minus1_tests if r['actual'] == 0),
            'actual_minus1': sum(1 for r in expected_minus1_tests if r['actual'] == -1),
            'actual_minus2': sum(1 for r in expected_minus1_tests if r['actual'] == -2),
        }
        
        # 按测试类型/模块分组统计
        module_stats = defaultdict(lambda: {'total': 0, 'passed': 0, 'failed': 0, 'error': 0})
        for result in self.test_results:
            module = result['test_name'].split('_')[0]  # 取前缀作为模块名
            module_stats[module]['total'] += 1
            if result['passed']:
                module_stats[module]['passed'] += 1
            elif result['status'] == 'ERROR':
                module_stats[module]['error'] += 1
            else:
                module_stats[module]['failed'] += 1
        
        return {
            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'failed_tests': failed_tests,
            'error_tests': error_tests,
            'pass_rate': passed_tests / total_tests * 100 if total_tests > 0 else 0,
            'fail_rate': failed_tests / total_tests * 100 if total_tests > 0 else 0,
            'error_rate': error_tests / total_tests * 100 if total_tests > 0 else 0,
            'expected_0_stats': expected_0_stats,
            'expected_minus1_stats': expected_minus1_stats,
            'module_stats': dict(module_stats),
            'execution_time': (self.end_time - self.start_time).total_seconds() if self.end_time and self.start_time else 0
        }
    
    def generate_report(self, output_file: str = None) -> None:
        """生成详细的测试报告文件"""
        if not self.test_results:
            print("没有测试结果，无法生成报告")
            return
        
        if output_file is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_file = f"test_report_{timestamp}.md"
        
        stats = self.get_statistics()
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("# RCompiler Semantic-1 测试评测报告\n\n")
            f.write(f"**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"**执行时间**: {stats['execution_time']:.2f} 秒\n\n")
            
            # 总体统计
            f.write("## 总体统计\n\n")
            f.write("| 指标 | 数量 | 百分比 |\n")
            f.write("|------|------|--------|\n")
            f.write(f"| 总测试数 | {stats['total_tests']} | 100.0% |\n")
            f.write(f"| 通过 | {stats['passed_tests']} | {stats['pass_rate']:.1f}% |\n")
            f.write(f"| 失败 | {stats['failed_tests']} | {stats['fail_rate']:.1f}% |\n")
            f.write(f"| 错误 | {stats['error_tests']} | {stats['error_rate']:.1f}% |\n\n")
            
            # 按期望结果分类统计
            f.write("## 按期望结果分类统计\n\n")
            
            f.write("### 期望结果为 0 的测试用例\n\n")
            f.write("| 实际结果 | 数量 | 占比 |\n")
            f.write("|----------|------|------|\n")
            exp0 = stats['expected_0_stats']
            if exp0['total'] > 0:
                f.write(f"| 0 (正确) | {exp0['actual_0']} | {exp0['actual_0']/exp0['total']*100:.1f}% |\n")
                f.write(f"| -1 (错误) | {exp0['actual_minus1']} | {exp0['actual_minus1']/exp0['total']*100:.1f}% |\n")
                f.write(f"| -2 (异常) | {exp0['actual_minus2']} | {exp0['actual_minus2']/exp0['total']*100:.1f}% |\n")
            f.write(f"| **总计** | {exp0['total']} | 100.0% |\n\n")
            
            f.write("### 期望结果为 -1 的测试用例\n\n")
            f.write("| 实际结果 | 数量 | 占比 |\n")
            f.write("|----------|------|------|\n")
            exp_minus1 = stats['expected_minus1_stats']
            if exp_minus1['total'] > 0:
                f.write(f"| 0 (错误) | {exp_minus1['actual_0']} | {exp_minus1['actual_0']/exp_minus1['total']*100:.1f}% |\n")
                f.write(f"| -1 (正确) | {exp_minus1['actual_minus1']} | {exp_minus1['actual_minus1']/exp_minus1['total']*100:.1f}% |\n")
                f.write(f"| -2 (异常) | {exp_minus1['actual_minus2']} | {exp_minus1['actual_minus2']/exp_minus1['total']*100:.1f}% |\n")
            f.write(f"| **总计** | {exp_minus1['total']} | 100.0% |\n\n")
            
            # 按模块分组统计
            f.write("## 按测试模块分组统计\n\n")
            f.write("| 模块 | 总数 | 通过 | 失败 | 错误 | 通过率 |\n")
            f.write("|------|------|------|------|------|--------|\n")
            
            # 按通过率排序
            sorted_modules = sorted(stats['module_stats'].items(),
                                  key=lambda x: x[1]['passed']/x[1]['total'] if x[1]['total'] > 0 else 0,
                                  reverse=True)
            
            for module, module_stat in sorted_modules:
                pass_rate = module_stat['passed'] / module_stat['total'] * 100 if module_stat['total'] > 0 else 0
                f.write(f"| {module} | {module_stat['total']} | {module_stat['passed']} | "
                       f"{module_stat['failed']} | {module_stat['error']} | {pass_rate:.1f}% |\n")
            f.write("\n")
            
            # 详细测试结果
            f.write("## 详细测试结果\n\n")
            f.write("| 测试名称 | 期望 | 实际 | 状态 | 文件路径 |\n")
            f.write("|----------|------|------|------|----------|\n")
            
            # 按状态排序：通过、失败、错误
            sorted_results = sorted(self.test_results, key=lambda x: (
                0 if x['status'] == 'PASS' else 1 if x['status'] == 'FAIL' else 2,
                x['test_name']
            ))
            
            for result in sorted_results:
                expected_str = str(result['expected']) if result['expected'] is not None else "N/A"
                actual_str = str(result['actual']) if result['actual'] != -2 else "ERROR"
                status_emoji = "✅" if result['status'] == 'PASS' else "❌" if result['status'] == 'FAIL' else "⚠️"
                f.write(f"| {result['test_name']} | {expected_str} | {actual_str} | "
                       f"{status_emoji} {result['status']} | {result['file_path']} |\n")
            
            # 失败和错误的测试详情
            failed_or_error = [r for r in self.test_results if not r['passed']]
            if failed_or_error:
                f.write("\n## 失败/错误测试详情\n\n")
                
                for result in failed_or_error:
                    f.write(f"### {result['test_name']}\n\n")
                    f.write(f"- **文件路径**: `{result['file_path']}`\n")
                    f.write(f"- **期望结果**: {result['expected']}\n")
                    f.write(f"- **实际结果**: {result['actual']}\n")
                    f.write(f"- **状态**: {result['status']}\n")
                    
                    if result['status'] == 'ERROR':
                        if result['expected'] is None:
                            f.write("- **失败原因**: 无法从文件注释中提取 Verdict\n")
                        elif result['actual'] == -2:
                            f.write("- **失败原因**: 编译器运行异常或超时\n")
                    f.write("\n")
        
        print(f"详细报告已生成: {output_file}")

def main():
    """主函数"""
    print("RCompiler Semantic-1 测试评测脚本")
    print("="*50)
    
    # 检查编译器是否存在
    compiler_path = Path("../build/code")
    if not compiler_path.exists():
        print(f"错误: 编译器不存在于 {compiler_path}")
        print("请先编译项目: cd build && make")
        sys.exit(1)
    
    # 检查测试目录是否存在
    test_dir = Path("../RCompiler-Testcases/semantic-1")
    if not test_dir.exists():
        print(f"错误: 测试目录不存在于 {test_dir}")
        sys.exit(1)
    
    # 创建评测器并运行测试
    evaluator = TestEvaluator()
    evaluator.run_all_tests()
    evaluator.print_results()
    evaluator.generate_report()

if __name__ == "__main__":
    main()
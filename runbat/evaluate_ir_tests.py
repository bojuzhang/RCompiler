#!/usr/bin/env python3
"""
RCompiler IR阶段测试评测脚本
自动评测 RCompiler-Testcases/IR-1 目录下的所有测试点
"""

import os
import re
import subprocess
import sys
import json
import tempfile
import shutil
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from collections import defaultdict

class IREvaluator:
    def __init__(self, test_dir: str = "../RCompiler-Testcases/IR-1/src", compiler_path: str = "../build/code", 
                 builtin_path: str = "../builtin/builtin.c", timeout: int = 30):
        self.test_dir = Path(test_dir)
        self.compiler_path = Path(compiler_path)
        self.builtin_path = Path(builtin_path)
        self.timeout = timeout
        self.test_results = []
        self.start_time = None
        self.end_time = None
        self.temp_dir = None
        
    def setup_temp_dir(self):
        """设置临时目录用于编译"""
        self.temp_dir = tempfile.mkdtemp(prefix="ir_eval_")
        return self.temp_dir
        
    def cleanup_temp_dir(self):
        """清理临时目录"""
        if self.temp_dir and os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
            
    def find_test_directories(self) -> List[Path]:
        """查找所有测试目录（包含.rx文件的目录）"""
        test_dirs = []
        if not self.test_dir.exists():
            return test_dirs
            
        for item in self.test_dir.iterdir():
            if item.is_dir():
                # 检查目录中是否有.rx文件
                rx_files = list(item.glob("*.rx"))
                if rx_files:
                    test_dirs.append(item)
        return sorted(test_dirs)
    
    def find_test_files(self, test_dir: Path) -> Dict[str, Path]:
        """在测试目录中查找测试文件"""
        files = {}
        
        # 查找.rx文件（测试代码）
        rx_files = list(test_dir.glob("*.rx"))
        if rx_files:
            files['rx'] = rx_files[0]  # 取第一个.rx文件
            
        # 查找.in文件（输入数据）
        in_files = list(test_dir.glob("*.in"))
        if in_files:
            files['in'] = in_files[0]
            
        # 查找.out文件（期望结果）
        out_files = list(test_dir.glob("*.out"))
        if out_files:
            files['out'] = out_files[0]
            
        return files
    
    def generate_ir_code(self, rx_file: Path) -> Tuple[bool, str, str]:
        """运行编译器生成IR代码"""
        try:
            with open(rx_file, 'r', encoding='utf-8') as f:
                source_code = f.read()
            
            # 运行编译器生成IR代码
            result = subprocess.run(
                [str(self.compiler_path)],
                input=source_code,
                text=True,
                capture_output=True,
                timeout=self.timeout
            )
            
            if result.returncode != 0:
                return False, "", f"编译器运行失败，返回码: {result.returncode}, 错误: {result.stderr}"
            
            ir_code = result.stdout
            if not ir_code.strip():
                return False, "", "编译器输出为空"
                
            return True, ir_code, ""
            
        except subprocess.TimeoutExpired:
            return False, "", "编译器运行超时"
        except Exception as e:
            return False, "", f"运行编译器时出错: {str(e)}"
    
    def compile_ir_to_executable(self, ir_code: str, test_name: str) -> Tuple[bool, str, str]:
        """将IR代码与builtin.c联合编译成可执行文件"""
        if not self.temp_dir:
            self.setup_temp_dir()
            
        try:
            # 保存IR代码到临时文件
            ir_file = Path(self.temp_dir) / f"{test_name}.ll"
            with open(ir_file, 'w', encoding='utf-8') as f:
                f.write(ir_code)
            
            # 编译IR代码和builtin.c
            exec_file = Path(self.temp_dir) / f"{test_name}"
            compile_cmd = [
                "clang-15",
                "-O0",  # 不优化，便于调试
                str(ir_file),
                str(self.builtin_path),
                "-o", str(exec_file)
            ]
            
            result = subprocess.run(
                compile_cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            if result.returncode != 0:
                return False, "", f"clang编译失败，返回码: {result.returncode}, 错误: {result.stderr}"
            
            if not exec_file.exists():
                return False, "", "编译后可执行文件不存在"
                
            return True, str(exec_file), ""
            
        except subprocess.TimeoutExpired:
            return False, "", "clang编译超时"
        except Exception as e:
            return False, "", f"编译时出错: {str(e)}"
    
    def run_executable(self, exec_file: str, input_file: Optional[Path] = None) -> Tuple[bool, str, str]:
        """运行可执行文件并获取输出"""
        try:
            # 准备输入
            input_data = ""
            if input_file and input_file.exists():
                with open(input_file, 'r', encoding='utf-8') as f:
                    input_data = f.read()
            
            # 运行可执行文件
            result = subprocess.run(
                [exec_file],
                input=input_data,
                text=True,
                capture_output=True,
                timeout=self.timeout
            )
            
            if result.returncode != 0:
                return False, "", f"程序运行失败，返回码: {result.returncode}, 错误: {result.stderr}"
            
            output = result.stdout
            return True, output, ""
            
        except subprocess.TimeoutExpired:
            return False, "", "程序运行超时"
        except Exception as e:
            return False, "", f"运行程序时出错: {str(e)}"
    
    def compare_output(self, actual_output: str, expected_file: Path) -> Tuple[bool, str]:
        """比较实际输出与期望结果"""
        try:
            if not expected_file.exists():
                return False, "期望输出文件不存在"
            
            with open(expected_file, 'r', encoding='utf-8') as f:
                expected_output = f.read()
            
            # 标准化行尾和空白字符
            actual_normalized = actual_output.strip() + '\n'
            expected_normalized = expected_output.strip() + '\n'
            
            if actual_normalized == expected_normalized:
                return True, "输出匹配"
            else:
                # 显示差异
                diff_msg = f"输出不匹配\n期望: {repr(expected_normalized)}\n实际: {repr(actual_normalized)}"
                return False, diff_msg
                
        except Exception as e:
            return False, f"比较输出时出错: {str(e)}"
    
    def evaluate_single_test(self, test_dir: Path) -> Dict:
        """评测单个测试点"""
        test_name = test_dir.name
        files = self.find_test_files(test_dir)
        
        # 初始化结果
        result = {
            'test_name': test_name,
            'test_dir': test_dir,
            'files': files,
            'status': 'UNKNOWN',
            'error_msg': '',
            'ir_success': False,
            'compile_success': False,
            'run_success': False,
            'output_match': False
        }
        
        # 步骤1: 生成IR代码
        if 'rx' not in files:
            result['status'] = 'ERROR'
            result['error_msg'] = '测试目录中缺少.rx文件'
            return result
            
        ir_success, ir_code, ir_error = self.generate_ir_code(files['rx'])
        result['ir_success'] = ir_success
        
        if not ir_success:
            result['status'] = 'IR_OUTPUT_FAILED'
            result['error_msg'] = f"IR输出失败: {ir_error}"
            return result
        
        # 步骤2: 编译IR代码
        compile_success, exec_file, compile_error = self.compile_ir_to_executable(ir_code, test_name)
        result['compile_success'] = compile_success
        
        if not compile_success:
            result['status'] = 'IR_COMPILE_FAILED'
            result['error_msg'] = f"IR编译失败: {compile_error}"
            return result
        
        # 步骤3: 运行可执行文件
        input_file = files.get('in')
        run_success, actual_output, run_error = self.run_executable(exec_file, input_file)
        result['run_success'] = run_success
        
        if not run_success:
            result['status'] = 'CODE_RUN_FAILED'
            result['error_msg'] = f"代码运行失败: {run_error}"
            return result
        
        # 步骤4: 比较输出
        if 'out' in files:
            output_match, compare_msg = self.compare_output(actual_output, files['out'])
            result['output_match'] = output_match
            
            if output_match:
                result['status'] = 'PASS'
                result['error_msg'] = '正确执行'
            else:
                result['status'] = 'OUTPUT_MISMATCH'
                result['error_msg'] = f"输出与预期不同: {compare_msg}"
        else:
            # 没有期望输出文件，只要能运行就算通过
            result['status'] = 'PASS'
            result['error_msg'] = '正确执行（无期望输出文件）'
            result['output_match'] = True
        
        # 保存实际输出用于调试
        result['actual_output'] = actual_output
        
        return result
    
    def run_all_tests(self) -> None:
        """运行所有测试"""
        self.start_time = datetime.now()
        print("正在查找测试目录...")
        test_dirs = self.find_test_directories()
        print(f"找到 {len(test_dirs)} 个测试目录\n")
        
        if not test_dirs:
            print("警告: 未找到任何测试目录")
            print(f"请确保测试目录 {self.test_dir} 存在且包含子目录，每个子目录包含 .rx 文件")
            return
        
        # 设置临时目录
        self.setup_temp_dir()
        
        try:
            print("开始评测...")
            for i, test_dir in enumerate(test_dirs, 1):
                print(f"评测进度: {i}/{len(test_dirs)} - {test_dir.name}")
                result = self.evaluate_single_test(test_dir)
                self.test_results.append(result)
        finally:
            # 清理临时目录
            self.cleanup_temp_dir()
        
        self.end_time = datetime.now()
        print("\n评测完成!")
    
    def print_results(self) -> None:
        """打印评测结果"""
        if not self.test_results:
            print("没有测试结果")
            return
        
        stats = self.get_statistics()
        
        print("\n" + "="*80)
        print("IR阶段评测结果汇总")
        print("="*80)
        print(f"总测试数: {stats['total_tests']}")
        print(f"通过: {stats['passed_tests']} ({stats['pass_rate']:.1f}%)")
        print(f"IR输出失败: {stats['ir_output_failed']} ({stats['ir_output_failed_rate']:.1f}%)")
        print(f"IR编译失败: {stats['ir_compile_failed']} ({stats['ir_compile_failed_rate']:.1f}%)")
        print(f"代码运行失败: {stats['code_run_failed']} ({stats['code_run_failed_rate']:.1f}%)")
        print(f"输出不匹配: {stats['output_mismatch']} ({stats['output_mismatch_rate']:.1f}%)")
        print(f"其他错误: {stats['other_errors']} ({stats['other_errors_rate']:.1f}%)")
        print(f"执行时间: {stats['execution_time']:.2f} 秒")
        
        # 详细结果
        print("\n" + "="*80)
        print("详细结果")
        print("="*80)
        print(f"{'测试名称':<20} {'IR输出':<8} {'IR编译':<8} {'代码运行':<8} {'输出匹配':<8} {'状态':<15} {'错误信息'}")
        print("-" * 100)
        
        for result in self.test_results:
            ir_status = "✓" if result['ir_success'] else "✗"
            compile_status = "✓" if result['compile_success'] else "✗"
            run_status = "✓" if result['run_success'] else "✗"
            output_status = "✓" if result['output_match'] else "✗"
            
            # 状态显示
            status_display = result['status']
            if result['status'] == 'PASS':
                status_display = "✓ 通过"
            elif result['status'] == 'IR_OUTPUT_FAILED':
                status_display = "✗ IR输出失败"
            elif result['status'] == 'IR_COMPILE_FAILED':
                status_display = "✗ IR编译失败"
            elif result['status'] == 'CODE_RUN_FAILED':
                status_display = "✗ 代码运行失败"
            elif result['status'] == 'OUTPUT_MISMATCH':
                status_display = "✗ 输出不匹配"
            else:
                status_display = f"⚠ {result['status']}"
            
            error_msg = result['error_msg'][:50] + "..." if len(result['error_msg']) > 50 else result['error_msg']
            
            print(f"{result['test_name']:<20} {ir_status:<8} {compile_status:<8} {run_status:<8} "
                  f"{output_status:<8} {status_display:<15} {error_msg}")
        
        # 失败测试详情
        failed_tests = [r for r in self.test_results if r['status'] != 'PASS']
        if failed_tests:
            print("\n" + "="*80)
            print("失败测试详情")
            print("="*80)
            
            for result in failed_tests:
                print(f"\n测试: {result['test_name']}")
                print(f"状态: {result['status']}")
                print(f"错误信息: {result['error_msg']}")
                if 'actual_output' in result and result['actual_output']:
                    print(f"实际输出: {repr(result['actual_output'])}")
    
    def get_statistics(self) -> Dict:
        """获取详细统计信息"""
        if not self.test_results:
            return {}
        
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r['status'] == 'PASS')
        ir_output_failed = sum(1 for r in self.test_results if r['status'] == 'IR_OUTPUT_FAILED')
        ir_compile_failed = sum(1 for r in self.test_results if r['status'] == 'IR_COMPILE_FAILED')
        code_run_failed = sum(1 for r in self.test_results if r['status'] == 'CODE_RUN_FAILED')
        output_mismatch = sum(1 for r in self.test_results if r['status'] == 'OUTPUT_MISMATCH')
        other_errors = sum(1 for r in self.test_results if r['status'] not in ['PASS', 'IR_OUTPUT_FAILED', 'IR_COMPILE_FAILED', 'CODE_RUN_FAILED', 'OUTPUT_MISMATCH'])
        
        return {
            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'ir_output_failed': ir_output_failed,
            'ir_compile_failed': ir_compile_failed,
            'code_run_failed': code_run_failed,
            'output_mismatch': output_mismatch,
            'other_errors': other_errors,
            'pass_rate': passed_tests / total_tests * 100 if total_tests > 0 else 0,
            'ir_output_failed_rate': ir_output_failed / total_tests * 100 if total_tests > 0 else 0,
            'ir_compile_failed_rate': ir_compile_failed / total_tests * 100 if total_tests > 0 else 0,
            'code_run_failed_rate': code_run_failed / total_tests * 100 if total_tests > 0 else 0,
            'output_mismatch_rate': output_mismatch / total_tests * 100 if total_tests > 0 else 0,
            'other_errors_rate': other_errors / total_tests * 100 if total_tests > 0 else 0,
            'execution_time': (self.end_time - self.start_time).total_seconds() if self.end_time and self.start_time else 0
        }
    
    def generate_report(self, output_file: str = None) -> None:
        """生成详细的测试报告文件"""
        if not self.test_results:
            print("没有测试结果，无法生成报告")
            return
        
        if output_file is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_file = f"ir_test_report_{timestamp}.md"
        
        stats = self.get_statistics()
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("# RCompiler IR阶段测试评测报告\n\n")
            f.write(f"**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"**执行时间**: {stats['execution_time']:.2f} 秒\n\n")
            
            # 总体统计
            f.write("## 总体统计\n\n")
            f.write("| 指标 | 数量 | 百分比 |\n")
            f.write("|------|------|--------|\n")
            f.write(f"| 总测试数 | {stats['total_tests']} | 100.0% |\n")
            f.write(f"| 通过 | {stats['passed_tests']} | {stats['pass_rate']:.1f}% |\n")
            f.write(f"| IR输出失败 | {stats['ir_output_failed']} | {stats['ir_output_failed_rate']:.1f}% |\n")
            f.write(f"| IR编译失败 | {stats['ir_compile_failed']} | {stats['ir_compile_failed_rate']:.1f}% |\n")
            f.write(f"| 代码运行失败 | {stats['code_run_failed']} | {stats['code_run_failed_rate']:.1f}% |\n")
            f.write(f"| 输出不匹配 | {stats['output_mismatch']} | {stats['output_mismatch_rate']:.1f}% |\n")
            f.write(f"| 其他错误 | {stats['other_errors']} | {stats['other_errors_rate']:.1f}% |\n\n")
            
            # 详细测试结果
            f.write("## 详细测试结果\n\n")
            f.write("| 测试名称 | IR输出 | IR编译 | 代码运行 | 输出匹配 | 状态 | 错误信息 |\n")
            f.write("|----------|--------|--------|----------|----------|------|----------|\n")
            
            for result in self.test_results:
                ir_status = "✅" if result['ir_success'] else "❌"
                compile_status = "✅" if result['compile_success'] else "❌"
                run_status = "✅" if result['run_success'] else "❌"
                output_status = "✅" if result['output_match'] else "❌"
                
                status_emoji = "✅" if result['status'] == 'PASS' else "❌"
                
                error_msg = result['error_msg'].replace('|', '\\|')[:30] + "..." if len(result['error_msg']) > 30 else result['error_msg']
                
                f.write(f"| {result['test_name']} | {ir_status} | {compile_status} | {run_status} | "
                       f"{output_status} | {status_emoji} {result['status']} | {error_msg} |\n")
            
            # 失败测试详情
            failed_tests = [r for r in self.test_results if r['status'] != 'PASS']
            if failed_tests:
                f.write("\n## 失败测试详情\n\n")
                
                for result in failed_tests:
                    f.write(f"### {result['test_name']}\n\n")
                    f.write(f"- **测试目录**: `{result['test_dir']}`\n")
                    f.write(f"- **状态**: {result['status']}\n")
                    f.write(f"- **错误信息**: {result['error_msg']}\n")
                    
                    if 'rx' in result['files']:
                        f.write(f"- **测试文件**: `{result['files']['rx']}`\n")
                    if 'in' in result['files']:
                        f.write(f"- **输入文件**: `{result['files']['in']}`\n")
                    if 'out' in result['files']:
                        f.write(f"- **期望输出文件**: `{result['files']['out']}`\n")
                    
                    if 'actual_output' in result and result['actual_output']:
                        f.write(f"- **实际输出**: `{repr(result['actual_output'])}`\n")
                    
                    f.write("\n")
        
        print(f"详细报告已生成: {output_file}")

def main():
    """主函数"""
    print("RCompiler IR阶段测试评测脚本")
    print("="*50)
    
    # 检查编译器是否存在
    compiler_path = Path("../build/code")
    if not compiler_path.exists():
        print(f"错误: 编译器不存在于 {compiler_path}")
        print("请先编译项目: cd build && make")
        sys.exit(1)
    
    # 检查builtin文件是否存在
    builtin_path = Path("../builtin/builtin.c")
    if not builtin_path.exists():
        print(f"错误: builtin文件不存在于 {builtin_path}")
        sys.exit(1)
    
    # 检查clang-15是否可用
    try:
        subprocess.run(["clang-15", "--version"], capture_output=True, check=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("错误: clang-15 不可用，请确保已安装 clang-15")
        sys.exit(1)
    
    # 创建评测器并运行测试
    evaluator = IREvaluator()
    evaluator.run_all_tests()
    evaluator.print_results()
    evaluator.generate_report()

if __name__ == "__main__":
    main()
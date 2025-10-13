# AST图形化可视化工具

## 概述

AST图形化可视化工具是RCompiler项目的一个独立组件，用于将抽象语法树（AST）以图形化方式展示。该工具使用Graphviz生成DOT格式的描述文件，并可以渲染为PNG或SVG格式的图片，提供直观的AST结构可视化。

## 功能特性

- **图形化AST展示**：生成清晰的树形结构图，显示节点类型和层次关系
- **多种输出格式**：支持PNG、SVG和DOT格式
- **批量处理**：可以批量处理目录中的所有.rx文件
- **详细信息控制**：可选择显示或隐藏节点详细信息
- **独立构建**：作为独立的可视化模块，不修改原有代码库

## 文件结构

```
visualization/
├── ast_graph_visualizer.hpp          # 图形化可视化器头文件
├── ast_graph_visualizer.cpp          # 图形化可视化器实现
├── ast_graph_visualizer_main.cpp     # 主程序文件
├── CMakeLists.txt                    # CMake构建配置
├── README_Graph_Visualizer.md       # 本文档
├── test_graph_visualizer.rx          # 测试文件
├── simple_test.rx                    # 简单测试文件
├── basic_test.rx                     # 基础测试文件
└── build/                            # 构建目录
    ├── ast_graph_visualizer          # 可执行文件
    └── ...                           # 生成的图片文件
```

## 构建说明

### 前置要求

- CMake 3.16 或更高版本
- C++20 兼容的编译器
- Graphviz（用于渲染PNG/SVG图片）

### 安装Graphviz

**Ubuntu/Debian:**
```bash
sudo apt-get install graphviz
```

**CentOS/RHEL:**
```bash
sudo yum install graphviz
```

**macOS:**
```bash
brew install graphviz
```

### 构建步骤

1. 进入visualization目录：
```bash
cd visualization
```

2. 创建构建目录：
```bash
mkdir build && cd build
```

3. 配置和构建：
```bash
cmake ..
make
```

构建成功后，会在build目录下生成`ast_graph_visualizer`可执行文件。

## 使用方法

### 基本用法

```bash
# 可视化单个文件（默认PNG格式）
./ast_graph_visualizer <file.rx>

# 指定输出格式
./ast_graph_visualizer <file.rx> png    # PNG格式
./ast_graph_visualizer <file.rx> svg    # SVG格式
./ast_graph_visualizer <file.rx> dot    # DOT格式
```

### 批量处理

```bash
# 批量处理目录中的所有.rx文件
./ast_graph_visualizer --dir <directory>

# 指定输出格式的批量处理
./ast_graph_visualizer --dir <directory> png
./ast_graph_visualizer --dir <directory> svg
```

### 生成DOT文件

```bash
# 仅生成DOT文件，不渲染图片
./ast_graph_visualizer --dot <file.rx>
```

### 简单模式

```bash
# 简单模式（不显示详细信息）
./ast_graph_visualizer --simple <file.rx>
```

### 交互式模式

```bash
# 启动交互式模式
./ast_graph_visualizer
```

在交互式模式中，可以使用以下命令：
- `file <path> [format]` - 可视化单个文件
- `dir <path> [format]` - 可视化目录中的所有.rx文件
- `dot <path>` - 生成DOT文件
- `details <on/off>` - 开启/关闭详细信息显示
- `help` - 显示帮助
- `quit` - 退出

## 使用示例

### 示例1：基本可视化

```bash
# 可视化单个文件
./ast_graph_visualizer ../../RCompiler-Testcases/semantic-1/src/basic2/basic2.rx
```

输出：
```
AST visualization saved to: basic2.png
AST可视化已保存到: basic2.png
```

### 示例2：生成SVG格式

```bash
./ast_graph_visualizer ../../RCompiler-Testcases/semantic-1/src/basic2/basic2.rx svg
```

输出：
```
AST visualization saved to: basic2.svg
AST可视化已保存到: basic2.svg
```

### 示例3：批量处理

```bash
./ast_graph_visualizer --dir ../../RCompiler-Testcases/semantic-1/src/basic1/
```

输出：
```
AST visualization saved to: ast_output/basic1.png
Processed 1 files in directory: ../../RCompiler-Testcases/semantic-1/src/basic1/
处理完成，成功处理 1 个文件
```

### 示例4：生成DOT文件

```bash
./ast_graph_visualizer --dot ../../RCompiler-Testcases/semantic-1/src/basic2/basic2.rx
```

输出：
```
DOT文件已保存到: basic2.dot
```

## 输出格式

### PNG格式
- 适合查看和分享
- 分辨率高，清晰度好
- 支持所有图片查看器

### SVG格式
- 矢量图形，可无限缩放
- 适合嵌入文档和网页
- 文件较小，加载速度快

### DOT格式
- Graphviz原生格式
- 可进一步自定义样式
- 适合调试和二次开发

## 技术实现

### 核心组件

1. **ASTGraphVisualizer类**：继承自ASTVisitor，实现所有AST节点类型的访问
2. **ASTGraphVisualizerTool类**：封装文件处理和可视化逻辑
3. **Graphviz集成**：使用系统dot命令进行图形渲染

### 关键特性

- **节点ID生成**：为每个AST节点生成唯一标识符
- **层次关系维护**：使用栈结构维护父子节点关系
- **标签格式化**：根据详细信息设置动态生成节点标签
- **错误处理**：完善的错误处理和用户反馈

### 支持的AST节点类型

- **Item节点**：Crate, Function, ConstantItem, StructStruct, Enumeration, InherentImpl
- **Statement节点**：Statement, LetStatement, ExpressionStatement
- **Expression节点**：所有表达式类型（LiteralExpression, BinaryExpression等）
- **Pattern节点**：Pattern, LiteralPattern, IdentifierPattern, WildcardPattern, PathPattern
- **Type节点**：Type, TypePath, ArrayType, SliceType, InferredType, ReferenceType
- **Path节点**：SimplePath, SimplePathSegment, PathInExpression
- **其他节点**：FunctionParameters, StructFields等

## 故障排除

### 常见问题

1. **Graphviz未找到**
   ```
   Warning: Graphviz dot not found. PNG/SVG rendering will not work.
   ```
   解决方案：安装Graphviz

2. **文件无法打开**
   ```
   Error: Cannot open file: <filename>
   ```
   解决方案：检查文件路径和权限

3. **解析失败**
   ```
   Error visualizing file <filename>: invalid Prefix
   ```
   解决方案：检查源代码语法是否正确

### 调试技巧

1. **使用DOT格式调试**：
   ```bash
   ./ast_graph_visualizer --dot <file.rx>
   cat <filename>.dot
   ```

2. **启用详细信息模式**：
   ```bash
   ./ast_graph_visualizer --simple <file.rx>  # 关闭详细信息
   ./ast_graph_visualizer <file.rx>           # 开启详细信息
   ```

3. **检查构建日志**：
   ```bash
   make VERBOSE=1
   ```

## 扩展开发

### 添加新的输出格式

1. 在`ASTGraphVisualizer`类中添加新的渲染方法
2. 在主程序中添加命令行选项
3. 更新帮助文档

### 自定义节点样式

修改`ASTGraphVisualizer`构造函数中的DOT配置：
```cpp
dotOutput << "digraph AST {\n";
dotOutput << "    node [shape=box, style=filled, fillcolor=lightblue];\n";
dotOutput << "    edge [arrowhead=vee];\n";
```

### 添加过滤功能

可以在`visit`方法中添加条件判断，只可视化特定类型的节点。

## 性能考虑

- **大文件处理**：对于大型AST，建议使用DOT格式，然后手动调整Graphviz参数
- **批量处理**：批量处理时会创建输出目录，确保有足够的磁盘空间
- **内存使用**：工具会将整个AST加载到内存中，注意处理大文件时的内存消耗

## 版本历史

- **v1.0**：初始版本，支持基本的AST图形化可视化
- 支持PNG、SVG、DOT输出格式
- 支持单文件和批量处理
- 完整的命令行界面

## 贡献指南

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 创建Pull Request

## 许可证

本工具遵循与RCompiler项目相同的许可证。

## 联系方式

如有问题或建议，请通过项目的Issue系统反馈。
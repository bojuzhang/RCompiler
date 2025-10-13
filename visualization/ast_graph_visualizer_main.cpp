#include "ast_graph_visualizer.hpp"
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

// 从标准输入读取源代码
std::string readFromStdin() {
    std::string sourceCode;
    std::string line;
    
    while (std::getline(std::cin, line)) {
        sourceCode += line + "\n";
    }
    
    return sourceCode;
}

int main() {
    try {
        // 从标准输入读取源代码
        std::cout << "请输入Rust源代码（按Ctrl+D结束输入）：" << std::endl;
        std::string sourceCode = readFromStdin();
        
        if (sourceCode.empty()) {
            std::cerr << "错误：没有输入源代码" << std::endl;
            return 1;
        }
        
        // 创建词法分析器和语法分析器
        Lexer lexer;
        auto tokens = lexer.lexString(sourceCode);
        Parser parser(tokens);
        
        // 解析AST
        auto crate = parser.parseCrate();
        if (!crate) {
            std::cerr << "错误：语法解析失败" << std::endl;
            return 1;
        }
        
        // 创建图形化可视化器
        ASTGraphVisualizer visualizer(true);
        
        // 生成AST可视化
        crate->accept(visualizer);
        
        // 生成文件名，存储到build/visualization目录
        std::filesystem::path currentPath = std::filesystem::current_path();
        std::filesystem::path outputDir = currentPath / "build" / "visualization";
        
        // 确保输出目录存在
        std::filesystem::create_directories(outputDir);
        
        // 生成唯一文件名
        std::string filename = "ast_visualization_" + std::to_string(getpid());
        std::filesystem::path baseFilename = outputDir / filename;
        std::string pngFilename = baseFilename.string() + ".png";
        
        // 保存并渲染为PNG
        if (visualizer.visualizeToFile(baseFilename, "png")) {
            std::cout << "AST可视化已生成: " << pngFilename << std::endl;
            std::cout << "文件已保留，请使用图片查看器打开" << std::endl;
        } else {
            std::cerr << "错误：无法生成PNG文件" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
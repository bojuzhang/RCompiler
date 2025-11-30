#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "irgenerator.hpp"
#include <iostream>
#include <string>
#include <memory>

int main() {
    try {
        // 第一步：从 stdin 读取源代码
        Lexer lexer;
        std::string sourceCode = lexer.GetString();
        
        // 第二步：词法分析
        auto tokens = lexer.lexString(sourceCode);
        if (tokens.empty()) {
            std::cerr << "Error: No tokens generated from input" << std::endl;
            exit(1);
        }
        
        // 第三步：语法分析
        Parser parser(tokens);
        auto crate = parser.parseCrate();
        if (crate == nullptr) {
            std::cerr << "Error: Parsing failed" << std::endl;
            exit(1);
        }
        
        // 第四步：语义分析
        CompleteSemanticAnalyzer semanticAnalyzer(crate);
        bool semanticSuccess = semanticAnalyzer.Analyze();
        if (!semanticSuccess) {
            std::cerr << "Error: Semantic analysis failed" << std::endl;
            exit(1);
        }
        
        // 第五步：获取语义分析结果
        auto scopeTree = semanticAnalyzer.getScopeTree();
        auto typeChecker = semanticAnalyzer.getTypeChecker();
        
        if (!scopeTree || !typeChecker) {
            std::cerr << "Error: Failed to get semantic analysis results" << std::endl;
            exit(1);
        }
        
        // 第六步：IR 生成
        IRGenerator irGenerator(scopeTree, typeChecker, std::cout);
        
        // 生成 IR
        bool irSuccess = irGenerator.generateIR(crate->items);
        if (!irSuccess) {
            std::cerr << "Error: IR generation failed" << std::endl;
            
            // 输出错误信息
            auto errors = irGenerator.getErrors();
            for (const auto& error : errors) {
                std::cerr << "IR Error: " << error << std::endl;
            }
            
            exit(1);
        }
        
        // 第七步：输出生成的 IR 到 stdout
        std::cout << irGenerator.getIROutput() << std::flush;
        
        // 成功完成
        exit(0);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        exit(1);
    }
}
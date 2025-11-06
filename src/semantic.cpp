#include "semantic.hpp"
#include <iostream>

CompleteSemanticAnalyzer::CompleteSemanticAnalyzer(std::shared_ptr<Crate> ast)
    : ast(ast) {}

bool CompleteSemanticAnalyzer::Analyze() {
    hasErrors = false;
    
    std::cerr << "=== Starting Complete Semantic Analysis ===" << std::endl;
    
    // 第一步：符号收集
    if (!RunSymbolCollection()) {
        hasErrors = true;
        return false;
    }
    
    // 第二步：常量求值
    if (!RunConstantEvaluation()) {
        hasErrors = true;
        return false;
    }
    
    // 第三步：控制流分析
    if (!RunControlFlowAnalysis()) {
        hasErrors = true;
        return false;
    }
    
    // 第四步：类型检查
    if (!RunTypeChecking()) {
        hasErrors = true;
        return false;
    }
    
    std::cerr << "=== Complete Semantic Analysis Completed ===" << std::endl;
    return !hasErrors;
}

bool CompleteSemanticAnalyzer::HasAnalysisErrors() const {
    return hasErrors;
}

bool CompleteSemanticAnalyzer::RunSymbolCollection() {
    std::cerr << "Step 1: Symbol Collection" << std::endl;
    
    SymbolCollector collector;
    collector.BeginCollection();
    ast->accept(collector);
    // collector.endCollection();
    
    scopeTree = collector.getScopeTree();
    
    // 修复：符号收集完成后，不要重置作用域树的当前作用域
    // 这样后续的类型检查阶段可以正确访问到符号收集阶段收集的符号
    // scopeTree->GoToNode(nullptr);
    
    
    // 检查符号收集过程中是否有错误
    if (collector.HasErrors()) {
        std::cerr << "Symbol collection failed with errors" << std::endl;
        return false;
    }
    
    return true;
}

bool CompleteSemanticAnalyzer::RunConstantEvaluation() {
    std::cerr << "Step 2: Constant Evaluation" << std::endl;
    
    constantEvaluator = std::make_shared<ConstantEvaluator>(scopeTree);
    ast->accept(*constantEvaluator);
    
    return !constantEvaluator->HasEvaluationErrors();
}

bool CompleteSemanticAnalyzer::RunControlFlowAnalysis() {
    std::cerr << "Step 3: Control Flow Analysis" << std::endl;
    
    controlFlowAnalyzer = std::make_shared<ControlFlowAnalyzer>(scopeTree, constantEvaluator);
    ast->accept(*controlFlowAnalyzer);
    
    return !controlFlowAnalyzer->HasAnalysisErrors();
}

bool CompleteSemanticAnalyzer::RunTypeChecking() {
    std::cerr << "Step 4: Type Checking" << std::endl;
    
    typeChecker = std::make_shared<TypeChecker>(scopeTree, constantEvaluator);
    ast->accept(*typeChecker);
    
    return !typeChecker->HasTypeErrors();
}
#include "semantic.hpp"
#include <iostream>

CompleteSemanticAnalyzer::CompleteSemanticAnalyzer(std::unique_ptr<Crate> ast) 
    : ast(std::move(ast)) {}

bool CompleteSemanticAnalyzer::analyze() {
    hasErrors = false;
    
    std::cout << "=== Starting Complete Semantic Analysis ===" << std::endl;
    
    // 第一步：符号收集
    if (!runSymbolCollection()) {
        hasErrors = true;
        return false;
    }
    
    // 第二步：常量求值
    if (!runConstantEvaluation()) {
        hasErrors = true;
        return false;
    }
    
    // 第三步：控制流分析
    if (!runControlFlowAnalysis()) {
        hasErrors = true;
        return false;
    }
    
    // 第四步：类型检查
    if (!runTypeChecking()) {
        hasErrors = true;
        return false;
    }
    
    // 第五步：类型推断
    if (!runTypeInference()) {
        hasErrors = true;
        return false;
    }
    
    std::cout << "=== Complete Semantic Analysis Completed ===" << std::endl;
    return !hasErrors;
}

bool CompleteSemanticAnalyzer::hasAnalysisErrors() const {
    return hasErrors;
}

bool CompleteSemanticAnalyzer::runSymbolCollection() {
    std::cout << "Step 1: Symbol Collection" << std::endl;
    
    SymbolCollector collector;
    collector.beginCollection();
    ast->accept(collector);
    collector.endCollection();
    
    scopeTree = collector.getScopeTree();
    return true;
}

bool CompleteSemanticAnalyzer::runConstantEvaluation() {
    std::cout << "Step 2: Constant Evaluation" << std::endl;
    
    constantEvaluator = std::make_shared<ConstantEvaluator>(scopeTree);
    ast->accept(*constantEvaluator);
    
    return !constantEvaluator->hasEvaluationErrors();
}

bool CompleteSemanticAnalyzer::runControlFlowAnalysis() {
    std::cout << "Step 3: Control Flow Analysis" << std::endl;
    
    controlFlowAnalyzer = std::make_shared<ControlFlowAnalyzer>(scopeTree, constantEvaluator);
    ast->accept(*controlFlowAnalyzer);
    
    return !controlFlowAnalyzer->hasAnalysisErrors();
}

bool CompleteSemanticAnalyzer::runTypeChecking() {
    std::cout << "Step 4: Type Checking" << std::endl;
    
    typeChecker = std::make_shared<TypeChecker>(scopeTree);
    ast->accept(*typeChecker);
    
    return !typeChecker->hasTypeErrors();
}

bool CompleteSemanticAnalyzer::runTypeInference() {
    std::cout << "Step 5: Type Inference" << std::endl;
    
    typeInferenceChecker = std::make_shared<TypeInferenceChecker>(
        scopeTree, controlFlowAnalyzer, constantEvaluator);
    ast->accept(*typeInferenceChecker);
    
    return !typeInferenceChecker->hasInferenceErrors();
}
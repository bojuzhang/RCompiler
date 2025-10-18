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
    
    // 第五步：类型推断
    if (!RunTypeInference()) {
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

bool CompleteSemanticAnalyzer::RunTypeInference() {
    std::cerr << "Step 5: Type Inference" << std::endl;
    
    typeInferenceChecker = std::make_shared<TypeInferenceChecker>(
        scopeTree, controlFlowAnalyzer, constantEvaluator);
    ast->accept(*typeInferenceChecker);
    
    return !typeInferenceChecker->HasInferenceErrors();
}
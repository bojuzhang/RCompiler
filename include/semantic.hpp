#pragma once

#include "symbolcollection.hpp"
#include "typecheck.hpp"
#include "constevaluator.hpp"
#include "controlflow.hpp"
#include "typeinference.hpp"
#include <memory>

class CompleteSemanticAnalyzer {
private:
    std::unique_ptr<Crate> ast;
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ConstantEvaluator> constantEvaluator;
    std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer;
    std::shared_ptr<TypeChecker> typeChecker;
    std::shared_ptr<TypeInferenceChecker> typeInferenceChecker;
    
    bool hasErrors = false;

public:
    CompleteSemanticAnalyzer(std::unique_ptr<Crate> ast);
    
    bool analyze();
    bool hasAnalysisErrors() const;
    
private:
    bool runSymbolCollection();
    bool runConstantEvaluation(); 
    bool runControlFlowAnalysis();
    bool runTypeChecking();
    bool runTypeInference();
};
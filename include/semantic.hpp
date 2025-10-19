#pragma once

#include "symbolcollection.hpp"
#include "typecheck.hpp"
#include "constevaluator.hpp"
#include "controlflow.hpp"
#include "typeinference.hpp"
#include <memory>

class CompleteSemanticAnalyzer {
private:
    std::shared_ptr<Crate> ast;
    std::shared_ptr<ScopeTree> scopeTree;
    std::shared_ptr<ConstantEvaluator> constantEvaluator;
    std::shared_ptr<ControlFlowAnalyzer> controlFlowAnalyzer;
    std::shared_ptr<TypeChecker> typeChecker;
    
    bool hasErrors = false;

public:
    CompleteSemanticAnalyzer(std::shared_ptr<Crate> ast);
    
    bool Analyze();
    bool HasAnalysisErrors() const;
    
private:
    bool RunSymbolCollection();
    bool RunConstantEvaluation();
    bool RunControlFlowAnalysis();
    bool RunTypeChecking();
};
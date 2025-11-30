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
    
    // 访问器方法
    std::shared_ptr<ScopeTree> getScopeTree() const;
    std::shared_ptr<TypeChecker> getTypeChecker() const;
    const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& getNodeTypeMap() const;
    
private:
    bool RunSymbolCollection();
    bool RunConstantEvaluation();
    bool RunControlFlowAnalysis();
    bool RunTypeChecking();
};
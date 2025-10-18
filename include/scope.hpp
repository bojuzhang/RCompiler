#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "symbol.hpp"

class Scope {
public:
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    std::shared_ptr<Scope> parent;
    std::vector<std::shared_ptr<Scope>> childrens;
    int depth;
    bool isFunctionScope;
    
public:
    enum class ScopeType {
        Global,
        Function,
        Block,
        Loop,
        Struct,
        Enum,
        Trait,
        Impl
    };

    Scope(std::shared_ptr<Scope> parent = nullptr, bool isFunctionScope = false);
    
    bool Insert(const std::string& name, std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> Lookup(const std::string& name, bool iscurrent = 0);
    
    std::shared_ptr<Scope> AddChild(bool isFunctionScope = false);
    std::shared_ptr<Scope> GetParent() const;
    const std::vector<std::shared_ptr<Scope>>& GetChildren() const;
    int GetDepth() const;
    bool IsInFunctionScope() const;
};

class ScopeTree {
public:
    std::shared_ptr<Scope> root;
    std::shared_ptr<Scope> currentNode;
    std::unordered_map<ASTNode*, std::shared_ptr<Scope>> nodeToScopeMap;
    std::stack<std::shared_ptr<Scope>> scopeStack;
    
public:
    ScopeTree();
    
    void EnterScope(Scope::ScopeType type, ASTNode* node = nullptr);
    void ExitScope();
    void GoToNode(ASTNode* node);
    
    std::shared_ptr<Scope> GetCurrentScope();
    std::shared_ptr<Scope> GetRootScope();
    std::shared_ptr<Scope> FindScopeForNode(ASTNode* node);
    std::vector<std::shared_ptr<Scope>> GetPathToCurrentScope();
    
    bool InsertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> LookupSymbol(const std::string& name);
    std::shared_ptr<Symbol> LookupSymbolInCurrentScope(const std::string& name);
};
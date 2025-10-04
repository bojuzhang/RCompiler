#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "symbol.hpp"

class Scope {
private:
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
    
    bool insert(const std::string& name, std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> lookup(const std::string& name, bool iscurrent = 0);
    
    std::shared_ptr<Scope> addchild(bool isFunctionScope = false);
    std::shared_ptr<Scope> getparent() const;
    const std::vector<std::shared_ptr<Scope>>& getchildren() const;
    int getdep() const;
    bool isInFunctionScope() const;
};

class ScopeTree {
private:
    std::shared_ptr<Scope> root;
    std::shared_ptr<Scope> currentNode;
    std::unordered_map<ASTNode*, std::shared_ptr<Scope>> nodeToScopeMap;
    
public:
    ScopeTree();
    
    // 树导航
    void enterScope(Scope::ScopeType type, ASTNode* node = nullptr);
    void exitScope();
    void gotoNode(ASTNode* node);
    
    // 查询
    std::shared_ptr<Scope> getCurrentScope();
    std::shared_ptr<Scope> getRootScope();
    std::shared_ptr<Scope> findScopeForNode(ASTNode* node);
    std::vector<std::shared_ptr<Scope>> getPathToCurrentScope();
    
    // 符号操作代理
    bool insertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> lookupSymbol(const std::string& name);
    std::shared_ptr<Symbol> lookupSymbolInCurrentScope(const std::string& name);
};
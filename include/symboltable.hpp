#pragma once

#include "symbol.hpp"
#include "scope.hpp"
#include <memory>
#include <stack>
#include <vector>

class SymbolTable {
private:
    std::shared_ptr<Scope> globalScope;
    std::shared_ptr<Scope> currentScope;
    std::stack<std::shared_ptr<Scope>> scopeStack;
    
    // 多次扫描状态
    int currentPass;
    std::vector<std::string> deferredSymbols;  // 延迟解析的符号
    
public:
    SymbolTable();
    
    // 作用域管理
    void enterScope(bool isFunctionScope = false);
    void exitScope();
    std::shared_ptr<Scope> getCurrentScope();
    std::shared_ptr<Scope> getGlobalScope();
    
    // 符号管理
    bool insertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> lookupSymbol(const std::string& name);
    std::shared_ptr<Symbol> lookupSymbolInCurrentScope(const std::string& name);
    
    // 多次扫描支持
    void beginPass(int passNumber);
    void endPass();
    int getCurrentPass() const;
    void deferSymbolResolution(const std::string& symbolName);
    void resolveDeferredSymbols();
    
    // 内置类型和函数
    void initializeBuiltins();
};
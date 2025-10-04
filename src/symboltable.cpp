#include "symboltable.hpp"
#include "symbol.hpp"
#include <utility>

Symbol::Symbol(const std::string& name, SymbolKind kind, 
           std::shared_ptr<Type> type, 
           bool ismutable, ASTNode* node)
    : name(name), kind(kind), type(type), ismutable(ismutable), node(std::move(node)) {}

// Scope实现
Scope::Scope(std::shared_ptr<Scope> parent, bool isFunctionScope) 
    : parent(parent), depth(parent ? parent->depth + 1 : 0), 
      isFunctionScope(isFunctionScope) {}

bool Scope::insert(const std::string& name, std::shared_ptr<Symbol> symbol) {
    if (symbols.find(name) != symbols.end()) {
        return false;
    }
    symbols[name] = symbol;
    return true;
}

std::shared_ptr<Symbol> Scope::lookup(const std::string& name, bool iscurrent) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (!iscurrent && parent) {
        return parent->lookup(name);
    }
    return nullptr;
}

std::shared_ptr<Scope> Scope::addchild(bool isFunctionScope) {
    auto child = std::make_shared<Scope>("", isFunctionScope);
    childrens.push_back(child);
    return child;
}

// SymbolTable实现
SymbolTable::SymbolTable() : currentPass(0) {
    globalScope = std::make_shared<Scope>();
    currentScope = globalScope;
    initializeBuiltins();
}

void SymbolTable::enterScope(bool isFunctionScope) {
    scopeStack.push(currentScope);
    currentScope = currentScope->addchild(isFunctionScope);
}

void SymbolTable::exitScope() {
    if (!scopeStack.empty()) {
        currentScope = scopeStack.top();
        scopeStack.pop();
    }
}

bool SymbolTable::insertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol) {
    return currentScope->insert(name, symbol);
}

std::shared_ptr<Symbol> SymbolTable::lookupSymbol(const std::string& name) {
    return currentScope->lookup(name);
}

void SymbolTable::beginPass(int passNumber) {
    currentPass = passNumber;
    deferredSymbols.clear();
}

void SymbolTable::endPass() {
    // 可以在这里处理pass结束的清理工作
}

void SymbolTable::initializeBuiltins() {
    // 添加内置类型
    auto builtinTypes = {
        "i32", "i64", "f32", "f64", "bool", "char", "str", "usize", "isize", "unit"
    };
    
    for (const auto& typeName : builtinTypes) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::BuiltinType, nullptr, false, 0
        );
        globalScope->insert(typeName, typeSymbol);
    }
    
    // 添加内置函数
    // 这里可以添加print, len等内置函数
}
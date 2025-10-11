#include "symboltable.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include <memory>
#include <utility>

Symbol::Symbol(const std::string& name, SymbolKind kind, 
           std::shared_ptr<SemanticType> type, 
           bool ismutable, ASTNode* node)
    : name(name), kind(kind), type(type), ismutable(ismutable), node(std::move(node)) {}

FunctionSymbol::FunctionSymbol(const std::string& name, 
                   const std::vector<std::shared_ptr<Symbol>>& parameters,
                   std::shared_ptr<SemanticType> returntype,
                   bool isMethod)
    : Symbol(name, SymbolKind::Function, returntype, false, nullptr), parameters(parameters), returntype(returntype), isMethod(isMethod) {}

ConstantSymbol::ConstantSymbol(const std::string& name, std::shared_ptr<SemanticType> type) 
    : Symbol(name, SymbolKind::Constant, type) {}

StructSymbol::StructSymbol(const std::string& name)
    : Symbol(name, SymbolKind::Struct) {}

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
    auto child = std::make_shared<Scope>(std::make_shared<Scope>(*this), isFunctionScope);
    childrens.push_back(child);
    return child;
}

std::shared_ptr<Scope> Scope::getparent() const {
    return parent;
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
            typeName, SymbolKind::BuiltinType, nullptr, false, nullptr
        );
        globalScope->insert(typeName, typeSymbol);
    }
    
    // 添加内置函数
    auto builtinFunctions = {
        "print", "println", "printInt", "printlnInt", "getString", "getInt", "exit"
    };
    
    for (const auto& typeName : builtinFunctions) {
        auto typeSymbol = std::make_shared<Symbol>(
            typeName, SymbolKind::Function, nullptr, false, nullptr
        );
        globalScope->insert(typeName, typeSymbol);
    }
}

ScopeTree::ScopeTree() {
    root = std::make_shared<Scope>(nullptr, false);
    currentNode = root;
}

void ScopeTree::enterScope(Scope::ScopeType type, ASTNode* node) {
    bool isFunctionScope = (type == Scope::ScopeType::Function);
    auto newScope = currentNode->addchild(isFunctionScope);
    currentNode = newScope;
    if (node) {
        nodeToScopeMap[node] = currentNode;
    }
}

void ScopeTree::exitScope() {
    if (currentNode->getparent()) {
        currentNode = currentNode->getparent();
    } 
}

void ScopeTree::gotoNode(ASTNode* node) {
    auto it = nodeToScopeMap.find(node);
    if (it != nodeToScopeMap.end()) {
        currentNode = it->second;
    } 
}

std::shared_ptr<Scope> ScopeTree::getCurrentScope() {
    return currentNode;
}

std::shared_ptr<Scope> ScopeTree::getRootScope() {
    return root;
}

std::shared_ptr<Scope> ScopeTree::findScopeForNode(ASTNode* node) {
    auto it = nodeToScopeMap.find(node);
    if (it != nodeToScopeMap.end()) {
        return it->second;
    }
    // 如果没有直接映射，尝试在父节点中查找
    // 这里需要根据AST结构实现，简化处理返回nullptr
    return nullptr;
}

std::vector<std::shared_ptr<Scope>> ScopeTree::getPathToCurrentScope() {
    std::vector<std::shared_ptr<Scope>> path;
    auto current = currentNode;
    while (current) {
        path.push_back(current);
        current = current->getparent();
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool ScopeTree::insertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol) {
    bool success = currentNode->insert(name, symbol);
    return success;
}

std::shared_ptr<Symbol> ScopeTree::lookupSymbol(const std::string& name) {
    auto symbol = currentNode->lookup(name, false);
    return symbol;
}

std::shared_ptr<Symbol> ScopeTree::lookupSymbolInCurrentScope(const std::string& name) {
    auto symbol = currentNode->lookup(name, true);
    return symbol;
}

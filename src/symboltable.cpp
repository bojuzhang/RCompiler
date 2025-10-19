#include "astnodes.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include <memory>
#include <utility>
#include <iostream>

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

bool Scope::Insert(const std::string& name, std::shared_ptr<Symbol> symbol) {
    if (symbols.find(name) != symbols.end()) {
        return false;
    }
    symbols[name] = symbol;
    return true;
}

std::shared_ptr<Symbol> Scope::Lookup(const std::string& name, bool iscurrent) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (!iscurrent && parent) {
        return parent->Lookup(name);
    }
    return nullptr;
}

std::shared_ptr<Scope> Scope::AddChild(bool isFunctionScope) {
    auto child = std::make_shared<Scope>(shared_from_this(), isFunctionScope);
    childrens.push_back(child);
    return child;
}

std::shared_ptr<Scope> Scope::GetParent() const {
    return parent;
}

ScopeTree::ScopeTree() {
    root = std::make_shared<Scope>(nullptr, false);
    currentNode = root;
}

void ScopeTree::EnterScope(Scope::ScopeType type, ASTNode* node) {
    if (!currentNode) {
        return;
    }
    
    bool isFunctionScope = (type == Scope::ScopeType::Function);
    auto newScope = currentNode->AddChild(isFunctionScope);
    currentNode = newScope;
    
    if (node) {
        nodeToScopeMap[node] = currentNode;
    }
}

void ScopeTree::ExitScope() {
    if (!currentNode) {
        return;
    }
    
    if (currentNode->GetParent()) {
        currentNode = currentNode->GetParent();
    }
}

void ScopeTree::GoToNode(ASTNode* node) {
    if (node == nullptr) {
        // 如果node为null，重置到根作用域
        currentNode = root;
        return;
    }
    
    auto it = nodeToScopeMap.find(node);
    if (it != nodeToScopeMap.end()) {
        currentNode = it->second;
    }
}

std::shared_ptr<Scope> ScopeTree::GetCurrentScope() {
    return currentNode;
}

std::shared_ptr<Scope> ScopeTree::GetRootScope() {
    return root;
}

std::shared_ptr<Scope> ScopeTree::FindScopeForNode(ASTNode* node) {
    auto it = nodeToScopeMap.find(node);
    if (it != nodeToScopeMap.end()) {
        return it->second;
    }

    // 简化处理返回nullptr
    return nullptr;
}

std::vector<std::shared_ptr<Scope>> ScopeTree::GetPathToCurrentScope() {
    std::vector<std::shared_ptr<Scope>> path;
    auto current = currentNode;
    while (current) {
        path.push_back(current);
        current = current->GetParent();
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool ScopeTree::InsertSymbol(const std::string& name, std::shared_ptr<Symbol> symbol) {
    bool success = currentNode->Insert(name, symbol);
    return success;
}

std::shared_ptr<Symbol> ScopeTree::LookupSymbol(const std::string& name) {
    if (!currentNode) {
        return nullptr;
    }
    
    auto symbol = currentNode->Lookup(name, false);
    return symbol;
}

std::shared_ptr<Symbol> ScopeTree::LookupSymbolInCurrentScope(const std::string& name) {
    auto symbol = currentNode->Lookup(name, true);
    return symbol;
}

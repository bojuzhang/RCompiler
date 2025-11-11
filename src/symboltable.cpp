#include "astnodes.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include <memory>
#include <utility>
#include <iostream>
#include <unordered_set>

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

void ScopeTree::EnterExistingScope(ASTNode* node) {
    if (!node) {
        return;
    }
    
    // 查找已经存在的作用域
    auto it = nodeToScopeMap.find(node);
    if (it != nodeToScopeMap.end()) {
        currentNode = it->second;
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

// 可视化 scopetree 的辅助函数
void ScopeTree::VisualizeScopeTree() {
    std::cerr << "\n=== Scope Tree Visualization ===" << std::endl;
    if (!root) {
        std::cerr << "No scope tree available." << std::endl;
        return;
    }
    
    // 获取从根到当前节点的路径
    auto pathToCurrent = GetPathToCurrentScope();
    std::unordered_set<std::shared_ptr<Scope>> currentPathSet(pathToCurrent.begin(), pathToCurrent.end());
    
    // 从根节点开始可视化
    VisualizeScope(root, currentNode, "", true, currentPathSet.find(root) != currentPathSet.end(), currentPathSet);
    std::cerr << "==============================\n" << std::endl;
}

// 递归可视化 scope 的辅助函数
void ScopeTree::VisualizeScope(std::shared_ptr<Scope> scope, std::shared_ptr<Scope> currentScope,
                              const std::string& prefix, bool isLast, bool isCurrentPath,
                              const std::unordered_set<std::shared_ptr<Scope>>& currentPathSet) {
    if (!scope) return;
    
    // 判断是否是当前节点
    bool isCurrent = (scope == currentScope);
    
    // 打印当前 scope 信息
    std::cerr << prefix;
    if (isLast) {
        std::cerr << "└── ";
    } else {
        std::cerr << "├── ";
    }
    
    // 高亮当前节点
    if (isCurrent) {
        std::cerr << "[CURRENT] ";
    }
    
    // 打印 scope 类型信息
    std::cerr << GetScopeTypeString(scope) << " (depth: " << scope->depth << ")";
    
    // 如果是当前路径上的节点，添加标记
    if (isCurrentPath && !isCurrent) {
        std::cerr << " [PATH]";
    }
    
    std::cerr << std::endl;
    
    // 打印 symbols
    std::string symbolPrefix = prefix + (isLast ? "    " : "│   ");
    if (!scope->symbols.empty()) {
        std::cerr << symbolPrefix << "Symbols: ";
        bool first = true;
        for (const auto& [name, symbol] : scope->symbols) {
            if (!first) {
                std::cerr << ", ";
            }
            std::cerr << name << ":" << GetSymbolKindString(symbol->kind);
            if (symbol->type) {
                std::cerr << "(" << symbol->type->tostring() << ")";
            }
            if (symbol->ismutable) {
                std::cerr << "[mut]";
            }
            first = false;
        }
        std::cerr << std::endl;
    }
    
    // 递归处理子节点
    for (size_t i = 0; i < scope->childrens.size(); ++i) {
        auto child = scope->childrens[i];
        bool childIsLast = (i == scope->childrens.size() - 1);
        bool childIsCurrentPath = isCurrentPath || (currentPathSet.find(child) != currentPathSet.end());
        VisualizeScope(child, currentScope, symbolPrefix, childIsLast, childIsCurrentPath, currentPathSet);
    }
}

// 获取 Scope 类型的字符串表示
std::string ScopeTree::GetScopeTypeString(std::shared_ptr<Scope> scope) {
    if (!scope) return "Unknown";
    
    if (scope->isFunctionScope) {
        return "FunctionScope";
    }
    
    // 这里可以根据需要添加更多的 scope 类型判断
    // 由于 Scope 类中没有直接的 ScopeType 字段，我们使用简单的判断
    if (scope->depth == 0) {
        return "GlobalScope";
    } else if (scope->parent && scope->parent->isFunctionScope) {
        return "BlockScope";
    } else {
        return "Scope";
    }
}

// 获取 Symbol 类型的字符串表示
std::string ScopeTree::GetSymbolKindString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Variable: return "Var";
        case SymbolKind::Function: return "Fn";
        case SymbolKind::Struct: return "Struct";
        case SymbolKind::Enum: return "Enum";
        case SymbolKind::Module: return "Mod";
        case SymbolKind::Constant: return "Const";
        case SymbolKind::TypeAlias: return "Type";
        case SymbolKind::BuiltinType: return "Builtin";
        case SymbolKind::Trait: return "Trait";
        case SymbolKind::Impl: return "Impl";
        case SymbolKind::Method: return "Method";
        case SymbolKind::Variant: return "Variant";
        case SymbolKind::AssociatedConstant: return "AssocConst";
        case SymbolKind::SelfType: return "Self";
        case SymbolKind::Loop: return "Loop";
        default: return "Unknown";
    }
}

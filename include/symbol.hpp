#pragma once

#include <memory>
#include <string>
#include <vector>
#include "astnodes.hpp"

class SemanticType {
public:
    virtual ~SemanticType() = default;
    virtual std::string tostring() const = 0;
};

class SimpleType : public SemanticType {
private:
    std::string typeName;
public:
    SimpleType(const std::string& name) : typeName(name) {}
    std::string tostring() const override { return typeName; }
};

enum class SymbolKind {
    Variable,
    Function,
    Struct,
    Enum,
    Module,
    Constant,
    TypeAlias,
    BuiltinType,
    Trait,
    Impl,
    Method,
    AssociatedConstant,
    SelfType,
    Loop
};

class Symbol {
public:
    std::string name;
    SymbolKind kind;
    std::shared_ptr<SemanticType> type;
    bool ismutable;
    ASTNode* node; 
    
    Symbol(const std::string& name, SymbolKind kind, 
           std::shared_ptr<SemanticType> type = nullptr, 
           bool ismutable = false, ASTNode* node = nullptr);
    
    virtual ~Symbol() = default;
};

class FunctionSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> parameters;
    std::shared_ptr<SemanticType> returntype;
    bool isMethod;
    
    FunctionSymbol(const std::string& name, 
                   const std::vector<std::shared_ptr<Symbol>>& parameters,
                   std::shared_ptr<SemanticType> returntype,
                   bool isMethod = false);
};

class StructSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> fields;
    
    StructSymbol(const std::string& name);
};

class EnumSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> variants;
    
    EnumSymbol(const std::string& name);
};

class ConstantSymbol : public Symbol {
public:
    ConstantSymbol(const std::string& name, std::shared_ptr<SemanticType> type);
};

class TraitSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> associatedItems;
    
    TraitSymbol(const std::string& name);
};
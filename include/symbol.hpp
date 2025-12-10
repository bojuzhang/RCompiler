#pragma once

#include <memory>
#include <string>
#include <vector>
#include "astnodes.hpp"

// 前向声明以避免循环依赖
class TypeChecker;

class SemanticType {
public:
    virtual ~SemanticType() = default;
    virtual std::string tostring() const = 0;
    std::shared_ptr<TypeChecker> typeChecker;
};

class SimpleType : public SemanticType {
private:
    std::string typeName;
public:
    SimpleType(const std::string& name) : typeName(name) {}
    std::string tostring() const override { return typeName; }
};

// 特殊整数类型，用于类型推断
class IntType : public SemanticType {
public:
    std::string tostring() const override { return "Int"; }
};

class SignedIntType : public SemanticType {
public:
    std::string tostring() const override { return "SignedInt"; }
};

class UnsignedIntType : public SemanticType {
public:
    std::string tostring() const override { return "UnsignedInt"; }
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
    Variant,
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
    std::vector<std::shared_ptr<SemanticType>> parameterTypes;
    
    FunctionSymbol(const std::string& name, 
                   const std::vector<std::shared_ptr<Symbol>>& parameters,
                   std::shared_ptr<SemanticType> returntype,
                   bool isMethod = false);
};

class StructSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> fields;
    std::vector<std::shared_ptr<FunctionSymbol>> methods;
    
    StructSymbol(const std::string& name);
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

class EnumSymbol : public Symbol {
public:
    std::vector<std::shared_ptr<Symbol>> variants;
    std::vector<std::shared_ptr<SemanticType>> genericParameters;
    
    EnumSymbol(const std::string& name) 
        : Symbol(name, SymbolKind::Enum, std::make_shared<SimpleType>(name)) {}
};

class VariantSymbol : public Symbol {
public:
    enum class VariantKind {
        Unit,      // 无数据，如 None
        Tuple,     // 元组变体，如 Some(T)
        Struct     // 结构变体，如 Point { x: i32, y: i32 }
    };
    
    VariantKind variantKind;
    std::vector<std::shared_ptr<SemanticType>> tupleFields;
    std::vector<std::shared_ptr<Symbol>> structFields;
    
    VariantSymbol(const std::string& name, VariantKind kind) 
        : Symbol(name, SymbolKind::Variant), variantKind(kind) {}
};

class ImplSymbol : public Symbol {
public:
    std::shared_ptr<SemanticType> targetType;    // 实现的目标类型
    std::string traitName;                       // 实现的trait名称（如果是trait实现）
    std::vector<std::shared_ptr<Symbol>> items;  // 关联项（方法、常量等）
    bool isTraitImpl;                            // 是否是trait实现
    
    ImplSymbol(const std::string& name, std::shared_ptr<SemanticType> targetType)
        : Symbol(name, SymbolKind::Impl), targetType(targetType), isTraitImpl(false) {}
};
#pragma once

#include "symbol.hpp"
#include <memory>

class ArrayTypeWrapper : public SemanticType {
private:
    std::shared_ptr<SemanticType> elementType;
    Expression* sizeExpression;
    
public:
    ArrayTypeWrapper(std::shared_ptr<SemanticType> elementType, Expression* sizeExpr = nullptr)
        : elementType(elementType), sizeExpression(sizeExpr) {}
    
    std::string tostring() const override {
        return "[" + elementType->tostring() + "]";
    }
    
    std::shared_ptr<SemanticType> getElementType() const { return elementType; }
    Expression* getSizeExpression() const { return sizeExpression; }
};

class SliceTypeWrapper : public SemanticType {
private:
    std::shared_ptr<SemanticType> elementType;
    
public:
    SliceTypeWrapper(std::shared_ptr<SemanticType> elementType)
        : elementType(elementType) {}
    
    std::string tostring() const override {
        return "[" + elementType->tostring() + "]";
    }
    
    std::shared_ptr<SemanticType> getElementType() const { return elementType; }
};

class TypeVariable : public SemanticType {
private:
    std::string name;
    
public:
    TypeVariable(const std::string& name) : name(name) {}
    
    std::string tostring() const override {
        return name;
    }
    
    std::string getName() const { return name; }
};

// 函数类型
class FunctionType : public SemanticType {
private:
    std::vector<std::shared_ptr<SemanticType>> parameterTypes;
    std::shared_ptr<SemanticType> returnType;
    
public:
    FunctionType(const std::vector<std::shared_ptr<SemanticType>>& params, std::shared_ptr<SemanticType> returnType)
        : parameterTypes(params), returnType(returnType) {}
    
    std::string tostring() const override {
        std::string result = "fn(";
        for (size_t i = 0; i < parameterTypes.size(); ++i) {
            if (i > 0) result += ", ";
            result += parameterTypes[i]->tostring();
        }
        result += ") -> " + returnType->tostring();
        return result;
    }
    
    const std::vector<std::shared_ptr<SemanticType>>& getParameterTypes() const { return parameterTypes; }
    std::shared_ptr<SemanticType> getReturnType() const { return returnType; }
};

// 元组类型
class TupleType : public SemanticType {
private:
    std::vector<std::shared_ptr<SemanticType>> elementTypes;
    
public:
    TupleType(const std::vector<std::shared_ptr<SemanticType>>& elements) : elementTypes(elements) {}
    
    std::string tostring() const override {
        std::string result = "(";
        for (size_t i = 0; i < elementTypes.size(); ++i) {
            if (i > 0) result += ", ";
            result += elementTypes[i]->tostring();
        }
        if (elementTypes.size() == 1) {
            result += ",";  // 单元素元组需要逗号
        }
        result += ")";
        return result;
    }
    
    const std::vector<std::shared_ptr<SemanticType>>& getElementTypes() const { return elementTypes; }
};

// 泛型类型
class GenericType : public SemanticType {
private:
    std::string baseName;
    std::vector<std::shared_ptr<SemanticType>> typeArguments;
    
public:
    GenericType(const std::string& baseName, const std::vector<std::shared_ptr<SemanticType>>& args)
        : baseName(baseName), typeArguments(args) {}
    
    std::string tostring() const override {
        std::string result = baseName;
        if (!typeArguments.empty()) {
            result += "<";
            for (size_t i = 0; i < typeArguments.size(); ++i) {
                if (i > 0) result += ", ";
                result += typeArguments[i]->tostring();
            }
            result += ">";
        }
        return result;
    }
    
    std::string getBaseName() const { return baseName; }
    const std::vector<std::shared_ptr<SemanticType>>& getTypeArguments() const { return typeArguments; }
};
#pragma once

#include "symbol.hpp"
#include "astnodes.hpp"
#include "constevaluator.hpp"
#include <iostream>
#include <memory>

class ArrayTypeWrapper : public SemanticType {
private:
    std::shared_ptr<SemanticType> elementType;
    std::shared_ptr<Expression> sizeExpression;
    ConstantEvaluator* constantEvaluator; // 添加常量求值器引用
    
public:
    ArrayTypeWrapper(std::shared_ptr<SemanticType> elementType, std::shared_ptr<Expression> sizeExpr = nullptr, ConstantEvaluator* constEval = nullptr)
        : elementType(elementType), sizeExpression(sizeExpr), constantEvaluator(constEval) {}
    
    std::string tostring() const override;
    
    std::shared_ptr<SemanticType> GetElementType() const { return elementType; }
    std::shared_ptr<Expression> GetSizeExpression() const { return sizeExpression; }
    void SetConstantEvaluator(ConstantEvaluator* eval) { constantEvaluator = eval; }
};

class ReferenceTypeWrapper : public SemanticType {
private:
    std::shared_ptr<SemanticType> targetType;
    bool isMutable;
    
public:
    ReferenceTypeWrapper(std::shared_ptr<SemanticType> targetType, bool isMutable = false)
        : targetType(targetType), isMutable(isMutable) {}
    
    std::string tostring() const override {
        std::string result = "&";
        if (isMutable) {
            result += "mut ";
        }
        if (typeChecker) targetType->typeChecker = typeChecker;
        result += targetType->tostring();
        return result;
    }
    
    std::shared_ptr<SemanticType> getTargetType() const { return targetType; }
    bool GetIsMutable() const { return isMutable; }
};

class TypeVariable : public SemanticType {
private:
    std::string name;
    
public:
    TypeVariable(const std::string& name) : name(name) {}
    
    std::string tostring() const override {
        return name;
    }
    
    std::string GetName() const { return name; }
};

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
    
    const std::vector<std::shared_ptr<SemanticType>>& GetParameterTypes() const { return parameterTypes; }
    std::shared_ptr<SemanticType> GetReturnType() const { return returnType; }
};

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
            result += ",";
        }
        result += ")";
        return result;
    }
    
    const std::vector<std::shared_ptr<SemanticType>>& GetElementTypes() const { return elementTypes; }
};

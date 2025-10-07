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
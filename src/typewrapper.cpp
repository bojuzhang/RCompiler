#include "typewrapper.hpp"
#include "constevaluator.hpp"
#include "astnodes.hpp"
#include "lexer.hpp"
#include "typecheck.hpp"
#include <iostream>

std::string ArrayTypeWrapper::tostring() const {
    std::string result = "[" + elementType->tostring();
    if (sizeExpression) {
        result += "; ";
        // 尝试获取大小值
        if (auto literal = dynamic_cast<LiteralExpression*>(sizeExpression.get())) {
            result += literal->literal;
        } else if (typeChecker) {
            result += std::to_string(typeChecker->EvaluateArraySize(*sizeExpression.get()));
        } else if (constantEvaluator) {
            // 尝试使用常量求值器获取大小
            if (auto pathExpr = dynamic_cast<PathExpression*>(sizeExpression.get())) {
                if (pathExpr->simplepath && !pathExpr->simplepath->simplepathsegements.empty()) {
                    std::string constName = pathExpr->simplepath->simplepathsegements[0]->identifier;
                    auto constValue = constantEvaluator->GetConstantValue(constName);
                    if (auto intConst = dynamic_cast<IntConstant*>(constValue.get())) {
                        result += std::to_string(intConst->getValue());
                    } else {
                        result += "?"; // 无法求值
                    }
                } else {
                    result += "?"; // 复杂表达式
                }
            } else {
                result += "?"; // 其他类型的表达式
            }
        } else {
            result += "?"; // 未知大小
        }
    }
    result += "]";
    return result;
}
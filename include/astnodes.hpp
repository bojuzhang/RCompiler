#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "lexer.hpp"
#include "visitorforward.hpp"
#include "visitor.hpp"

// 使用shared_ptr替代unique_ptr以解决语义检查阶段的指针失效问题
// 这样可以安全地在多个分析器之间共享AST节点，而不会导致指针失效


class ASTNode {
public:
    ASTNode();
    virtual ~ASTNode() = default;

    virtual void accept(ASTVisitor &visitor) = 0;
};

// ITEM Syntax
class Crate : public ASTNode {
public:
    std::vector<std::shared_ptr<Item>> items;
public:
    Crate(std::vector<std::shared_ptr<Item>>&& items);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class Item : public ASTNode {
public:
    std::shared_ptr<ASTNode> item;
public:
    Item(std::shared_ptr<ASTNode> item);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class Function : public ASTNode {
public:
    // std::unique_ptr<FunctionQualifiers> functionqualifiers;
    bool isconst;
    std::string identifier_name;
    std::shared_ptr<FunctionParameters> functionparameters;
    std::shared_ptr<FunctionReturnType> functionreturntype;
    std::shared_ptr<BlockExpression> blockexpression;
public:
    Function(bool isconst,
             std::string identifier_name,
             std::shared_ptr<FunctionParameters> functionparameters,
             std::shared_ptr<FunctionReturnType> functionreturntype,
             std::shared_ptr<BlockExpression> blockexpression);
             
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ConstantItem : public ASTNode {
public:
    std::string identifier;
    std::shared_ptr<Type> type;
    std::shared_ptr<Expression> expression;
public:
    ConstantItem(std::string identifier,
                 std::shared_ptr<Type> type,
                 std::shared_ptr<Expression> expression);
                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class Module : public ASTNode {
// public:
//     std::string identifier;
//     std::vector<std::unique_ptr<Item>> items;
// public:
//     Module(std::string identifier,
//            std::vector<std::unique_ptr<Item>>&& items);
           
//     void accept(ASTVisitor& visitor) override {
//         visitor.visit(*this);
//     }
// };
class StructStruct : public ASTNode {
public:
    std::string identifier;
    std::shared_ptr<StructFields> structfileds;
    bool issemi;
public:
    StructStruct(std::string identifier,
                 std::shared_ptr<StructFields> structfileds,
                 bool issemi);
                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class TupleStruct : public ASTNode {
// public:
//     std::string identifier;
//     std::unique_ptr<TupleFields> tuplefields;
// public:
//     TupleStruct(std::string identifier,
//                 std::unique_ptr<TupleFields> tuplefields);
                
//     void accept(ASTVisitor& visitor) override {
//         visitor.visit(*this);
//     }
// };
class Enumeration : public ASTNode {
public:
    std::string identifier;
    std::shared_ptr<EnumVariants> enumvariants;
public:
    Enumeration(std::string identifier,
                std::shared_ptr<EnumVariants> enumvariants);
                
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class InherentImpl : public ASTNode {
public:
    std::shared_ptr<Type> type;
    std::vector<std::shared_ptr<AssociatedItem>> associateditems;
public:
    InherentImpl(std::shared_ptr<Type> type,
                 std::vector<std::shared_ptr<AssociatedItem>>&& associateditems);
                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class FunctionQualifiers : public ASTNode {
// public:
//     bool isconst;
// public:
//     explicit FunctionQualifiers(bool isconst);
// };
class FunctionParameters : public ASTNode {
public:
    std::vector<std::shared_ptr<FunctionParam>> functionparams;
public:
    explicit FunctionParameters(std::vector<std::shared_ptr<FunctionParam>>&& functionparams);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class FunctionParam : public ASTNode {
public:
    std::shared_ptr<Pattern> patternnotopalt;
    std::shared_ptr<Type> type;
public:
    FunctionParam(std::shared_ptr<Pattern> patternnotopalt,
                  std::shared_ptr<Type> type);
                  
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class FunctionReturnType : public ASTNode {
public:
    std::shared_ptr<Type> type;
public:
    explicit FunctionReturnType(std::shared_ptr<Type> type);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructFields : public ASTNode {
public:
    std::vector<std::shared_ptr<StructField>> structfields;
public:
    explicit StructFields(std::vector<std::shared_ptr<StructField>>&& structfields);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructField : public ASTNode {
public:
    std::string identifier;
    std::shared_ptr<Type> type;
public:
    StructField(std::string identifier,
                std::shared_ptr<Type> type);
                
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class TupleFields : public ASTNode {
// public:
//     std::vector<std::unique_ptr<TupleField>> tuplefields; 
// public:
//     explicit TupleFields(std::vector<std::unique_ptr<TupleField>>&& tuplefields);
    
//     void accept(ASTVisitor& visitor) override {
//         visitor.visit(*this);
//     }
// };
// class TupleField : public ASTNode {
// public:
//     std::string identifier;
//     std::unique_ptr<Type> type;
// public:
//     TupleField(std::string identifier,
//                std::unique_ptr<Type> type);
               
//     void accept(ASTVisitor& visitor) override {
//         visitor.visit(*this);
//     }
// };
class EnumVariants : public ASTNode {
public:
    std::vector<std::shared_ptr<EnumVariant>> enumvariants;
public:
    explicit EnumVariants(std::vector<std::shared_ptr<EnumVariant>>&& enumvariants);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class EnumVariant : public ASTNode {
public:
    std::string identifier;
public:
    EnumVariant(std::string identifier);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class EnumVariantTuple : public ASTNode {
// public:
//     std::unique_ptr<TupleFields> tuplefields;
// public:
//     explicit EnumVariantTuple(std::unique_ptr<TupleFields> tuplefields);
// };
// class EnumVariantStruct : public ASTNode {
// public:
//     std::unique_ptr<StructFields> structfields;
// public:
//     explicit EnumVariantStruct(std::unique_ptr<StructFields> structfields);
// };
// class EnumVariantDiscriminant : public ASTNode {
// public:
//     std::unique_ptr<Expression> expression;
// public:
//     explicit EnumVariantDiscriminant(std::unique_ptr<Expression> expression);
// };
class AssociatedItem : public ASTNode {
public:
    std::shared_ptr<ASTNode> consttantitem_or_function;
public:
    explicit AssociatedItem(std::shared_ptr<ASTNode> consttantitem_or_function);
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};

// STATEMENT Syntax
class Statement : public ASTNode{
public:
    std::shared_ptr<ASTNode> astnode; // item or letstatement or expressionstatement
public:
    explicit Statement(std::shared_ptr<ASTNode> astnode);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class LetStatement : public ASTNode {
public:
    std::shared_ptr<Pattern> patternnotopalt;
    std::shared_ptr<Type> type;
    std::shared_ptr<Expression> expression;
public:
    LetStatement(std::shared_ptr<Pattern> patternnotopalt,
                 std::shared_ptr<Type> type,
                 std::shared_ptr<Expression> expression);
                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ExpressionStatement : public ASTNode {
public:
    std::shared_ptr<Expression> astnode;
    bool hassemi;
public:
    explicit ExpressionStatement(std::shared_ptr<Expression> astnode, bool hassemi);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};

// EXPRESSION Syntax
class Expression : public ASTNode {
public:
    Expression();
    virtual ~Expression() = default;
};

class LiteralExpression : public Expression {
public:
    std::string literal;
    Token tokentype;
public:
    LiteralExpression(std::string literal, Token tokentype);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class PathExpression : public Expression {
public:
    std::shared_ptr<SimplePath> simplepath;
public:
    explicit PathExpression(std::shared_ptr<SimplePath> simplepath);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class GroupedExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
public:
    explicit GroupedExpression(std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ArrayExpression : public Expression {
public:
    std::shared_ptr<ArrayElements> arrayelements;
public:
    explicit ArrayExpression(std::shared_ptr<ArrayElements> arrayelements);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class IndexExpression : public Expression {
public:
    std::shared_ptr<Expression> expressionout;
    std::shared_ptr<Expression> expressionin;
public:
    IndexExpression(std::shared_ptr<Expression> expressionout,
                    std::shared_ptr<Expression> expressionin);
                    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class TupleExpression : public Expression {
public:
    std::shared_ptr<TupleElements> tupleelements;
public:
    explicit TupleExpression(std::shared_ptr<TupleElements> tupleelements);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class TupleIndexingExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
    int tupleindex;
public:
    TupleIndexingExpression(std::shared_ptr<Expression> expression, int tupleindex);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructExpression : public Expression {
public:
    std::shared_ptr<PathInExpression> pathinexpression;
    std::shared_ptr<ASTNode> structinfo; // exprfields or base
public:
    StructExpression(std::shared_ptr<PathInExpression> pathinexpression,
                     std::shared_ptr<ASTNode> structinfo);
                     
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class CallExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
    std::shared_ptr<CallParams> callparams;
public:
    CallExpression(std::shared_ptr<Expression> expression,
                   std::shared_ptr<CallParams> callparams);
                   
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class MethodCallExpression : public Expression {
// public:
//     // is it deleted?
//     std::unique_ptr<Expression> expression;
//     // std::unique_ptr<Path>
//     std::unique_ptr<CallParams> callparams;
// };
class FieldExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
    std::string identifier;
public:
    FieldExpression(std::shared_ptr<Expression> expression, std::string identifier);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ContinueExpression : public Expression {
public:
    // nothing but continue
public:
    ContinueExpression();
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class BreakExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
public:
    explicit BreakExpression(std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ReturnExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
public:
    explicit ReturnExpression(std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class UnderscoreExpression : public Expression {
public:
    // nothing but _
public:
    UnderscoreExpression();
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class BlockExpression : public Expression {
public:
    std::vector<std::shared_ptr<Statement>> statements;
    std::shared_ptr<Expression> expressionwithoutblock;
public:
    BlockExpression(std::vector<std::shared_ptr<Statement>> statements,
                    std::shared_ptr<Expression> expressionwithoutblock);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ConstBlockExpression : public Expression {
public:
    std::shared_ptr<BlockExpression> blockexpression;
public:
    explicit ConstBlockExpression(std::shared_ptr<BlockExpression> blockexpression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class InfiniteLoopExpression : public Expression {
public:
    std::shared_ptr<BlockExpression> blockexpression;
public:
    explicit InfiniteLoopExpression(std::shared_ptr<BlockExpression> blockexpression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class PredicateLoopExpression : public Expression {
public:
    std::shared_ptr<Conditions> conditions;
    std::shared_ptr<BlockExpression> blockexpression;
public:
    PredicateLoopExpression(std::shared_ptr<Conditions> conditions,
                            std::shared_ptr<BlockExpression> blockexpression);
                            
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class IfExpression : public Expression {
public:
    std::shared_ptr<Conditions> conditions;
    std::shared_ptr<BlockExpression> ifblockexpression;
    std::shared_ptr<Expression> elseexpression; // block or if
public:
    IfExpression(std::shared_ptr<Conditions> conditions,
                 std::shared_ptr<BlockExpression> ifblockexpression,
                 std::shared_ptr<Expression> elseexpression);
                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class MatchExpression : public Expression {
// // is it deleted?
// };
class TypeCastExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
    std::shared_ptr<Type> typenobounds;
public:
    TypeCastExpression(std::shared_ptr<Expression> expression,
                       std::shared_ptr<Type> typenobounds);
                       
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};  
class AssignmentExpression : public Expression {
public:
    std::shared_ptr<Expression> leftexpression;
    std::shared_ptr<Expression> rightexpression;
public:
    AssignmentExpression(std::shared_ptr<Expression> leftexpression,
                         std::shared_ptr<Expression> rightexpression);
                         
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class CompoundAssignmentExpression : public Expression {
public:
    std::shared_ptr<Expression> leftexpression;
    std::shared_ptr<Expression> rightexpression;
    Token type;
public:
    CompoundAssignmentExpression(std::shared_ptr<Expression> leftexpression,
                                 std::shared_ptr<Expression> rightexpression,
                                 Token type);
                                 
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ArrayElements : public Expression {
public:
    std::vector<std::shared_ptr<Expression>> expressions;
    bool istwo;
public:
    ArrayElements(std::vector<std::shared_ptr<Expression>>&& expressions, bool istwo);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class TupleElements : public Expression {
public:
    std::vector<std::shared_ptr<Expression>> expressions;
public:
    explicit TupleElements(std::vector<std::shared_ptr<Expression>>&& expressions);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructExprFields : public Expression {
public:
    std::vector<std::shared_ptr<StructExprField>> structexprfields;
    std::shared_ptr<StructBase> structbase;
public:
    StructExprFields(std::vector<std::shared_ptr<StructExprField>>&& structexprfields,
                     std::shared_ptr<StructBase> structbase);
                     
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructExprField : public Expression {
public:
    std::string identifier;
    int tupleindex;
    std::shared_ptr<Expression> expression;
public:
    StructExprField(std::string identifier, int tupleindex, std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class StructBase : public Expression {
public:
    std::shared_ptr<Expression> expression;
public:
    explicit StructBase(std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class CallParams : public Expression {
public:
    std::vector<std::shared_ptr<Expression>> expressions;
public:
    explicit CallParams(std::vector<std::shared_ptr<Expression>>&& expressions);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class Conditions : public Expression {
public:
    std::shared_ptr<Expression> expression; // except structexpression
public:
    explicit Conditions(std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class MatchArms : public Expression {};
// class MatchArm : public Expression {};
// class MatchArmGuard : public Expression {};

class UnaryExpression : public Expression {
public:
    std::shared_ptr<Expression> expression;
    Token unarytype;
public:
    UnaryExpression(std::shared_ptr<Expression> expression, Token unarytype);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class BinaryExpression : public Expression {
public:
    std::shared_ptr<Expression> leftexpression;
    std::shared_ptr<Expression> rightexpression;
    Token binarytype;
public:
    BinaryExpression(std::shared_ptr<Expression> leftexpression,
                     std::shared_ptr<Expression> rightexpression,
                     Token binarytype);
                     
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};

// PATTERN Syntax
class Pattern : public ASTNode {
public:
    Pattern() = default;
    virtual ~Pattern() = default;
};
class LiteralPattern : public Pattern {
public:
    bool isnegative;
    std::shared_ptr<Expression> literalexprerssion;
public:
    LiteralPattern(bool isnegative, std::shared_ptr<Expression> literalexprerssion);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class IdentifierPattern : public Pattern {
public:
    bool hasref;
    bool hasmut;
    std::string identifier;
    std::shared_ptr<Pattern> patternnotopalt;
public:
    IdentifierPattern(bool hasref, bool hasmut, std::string identifier, std::shared_ptr<Pattern> patternnotopalt);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class WildcardPattern : public Pattern {
public:
    // nothing but _
public:
    WildcardPattern();
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class PathPattern : public Pattern {
public:
    std::shared_ptr<Expression> pathexpression;
public:
    explicit PathPattern(std::shared_ptr<Expression> pathexpression);
};
class ReferencePattern : public Pattern {
public:
    bool singleordouble;
    bool hasmut;
    std::shared_ptr<Pattern> pattern;
public:
    ReferencePattern(bool singleordouble, bool hasmut, std::shared_ptr<Pattern> pattern);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};


// TYPE Syntax
class Type : public ASTNode {
public:
    Type() = default;
    virtual ~Type() = default;
};
// class TypeNoBounds : public Type {
// public:
//     std::unique_ptr<ASTNode> astnode; /*
//     `TypePath
//     | ArrayType
//     | SliceType
//     | InferredType
//     | ReferenceType
//     */
// public:
//     explicit TypeNoBounds(std::unique_ptr<ASTNode> astnode);
// };
// class ParenthesizedType : public ASTNode {
// public:
//     std::unique_ptr<Type> type;
// public:
//     explicit ParenthesizedType(std::unique_ptr<Type> type);
// };
class TypePath : public Type {
public:
    std::shared_ptr<SimplePathSegment> simplepathsegement;
public:
    explicit TypePath(std::shared_ptr<SimplePathSegment> simplepathsegement);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
// class TupleType : public ASTNode {
// public:
//     std::vector<std::unique_ptr<Type>> types;
// public:
//     explicit TupleType(std::vector<std::unique_ptr<Type>>&& types);
// };
// class NeverType : public ASTNode {
// public:
//     // nothing but !
// public:
//     NeverType();
// };
class ArrayType : public Type {
public:
    std::shared_ptr<Type> type;
    std::shared_ptr<Expression> expression;
public:
    ArrayType(std::shared_ptr<Type> type, std::shared_ptr<Expression> expression);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class SliceType : public Type {
public:
    std::shared_ptr<Type> type;
public:
    explicit SliceType(std::shared_ptr<Type> type);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class InferredType : public Type {
public:
    // nothing but _
public:
    InferredType();
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class ReferenceType : public Type {
public:
    std::shared_ptr<Type> type;
    bool ismut;
public:
    ReferenceType(std::shared_ptr<Type> type, bool ismut);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};

// PATH Syntax
class SimplePath : public ASTNode {
public:
    std::vector<std::shared_ptr<SimplePathSegment>> simplepathsegements;
public:
    explicit SimplePath(std::vector<std::shared_ptr<SimplePathSegment>>&& simplepathsegements);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class SimplePathSegment : public ASTNode {
public:
    std::string identifier;
    bool isself, isSelf;
public:
    SimplePathSegment(std::string identifier, bool isself, bool isSelf);
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
class PathInExpression : public ASTNode {
public:
    // temporary nothing
public:
    PathInExpression();
    
    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
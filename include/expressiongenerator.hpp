#pragma once

#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <unordered_map>
#include "astnodes.hpp"
#include "irbuilder.hpp"
#include "typemapper.hpp"
#include "scope.hpp"
#include "symbol.hpp"

// 前向声明以解决循环依赖
class StatementGenerator;

/**
 * ExpressionGenerator 类 - 负责将 Rx 语言的表达式转换为 LLVM IR 文本
 * 
 * 重要架构说明：
 * - ExpressionGenerator 处理所有表达式类型，包括 BlockExpression 和控制流表达式
 * - Statement 使用组合模式，控制流语句（if、loop、while、break、continue、return）都是 Expression 的子类
 * - BlockExpression 包含 Statement 向量，导致 StatementGenerator 和 ExpressionGenerator 之间存在循环依赖
 * - 通过前向声明和接口分离来解决循环依赖问题
 * 
 * 职责划分：
 * - ExpressionGenerator：处理所有表达式，包括 BlockExpression 和控制流表达式
 * - StatementGenerator：只处理真正的语句（Let、Item、ExpressionStatement）
 * - BlockExpression：完全由 ExpressionGenerator 处理，通过调用 StatementGenerator 处理其中的语句
 */
class ExpressionGenerator {
private:
    // 核心依赖组件
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    
    // StatementGenerator 引用（延迟初始化以解决循环依赖）
    StatementGenerator* statementGenerator;
    
    // 语义分析阶段的类型信息映射
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    
    // 循环上下文管理
    struct LoopContext {
        std::string loopStartBB;
        std::string loopEndBB;
        std::string loopBodyBB;
        bool hasBreak;
        std::string breakValueReg;
        std::string breakValueType;
        
        LoopContext() : hasBreak(false) {}
        
        LoopContext(const std::string& start, const std::string& end, const std::string& body)
            : loopStartBB(start), loopEndBB(end), loopBodyBB(body), hasBreak(false) {}
    };
    
    std::stack<LoopContext> loopContextStack;
    
    // 错误处理
    bool hasErrors;
    std::vector<std::string> errorMessages;
    
    // 字符串字面量管理
    int stringCounter;
    std::unordered_map<std::string, std::string> stringConstants;

public:
    // 构造函数
    ExpressionGenerator(std::shared_ptr<IRBuilder> irBuilder,
                        std::shared_ptr<TypeMapper> typeMapper,
                        std::shared_ptr<ScopeTree> scopeTree,
                        const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& nodeTypeMap);
    
    // 析构函数
    ~ExpressionGenerator() = default;
    
    // ==================== StatementGenerator 集成接口 ====================
    
    /**
     * 设置 StatementGenerator 引用（延迟初始化）
     * @param stmtGen StatementGenerator 实例指针
     */
    void setStatementGenerator(StatementGenerator* stmtGen);
    
    /**
     * 获取 StatementGenerator 引用
     * @return StatementGenerator 实例指针
     */
    StatementGenerator* getStatementGenerator() const;
    
    // ==================== 主要生成接口 ====================
    
    /**
     * 表达式生成的统一入口点
     * @param expression 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateExpression(std::shared_ptr<Expression> expression);
    
    // ==================== 各类表达式生成方法 ====================
    
    /**
     * 处理字面量表达式（整数、字符串、布尔值、字符等）
     * @param literalExpr 字面量表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateLiteralExpression(std::shared_ptr<LiteralExpression> literalExpr);
    
    /**
     * 处理路径表达式（变量访问、常量访问、函数名引用）
     * @param pathExpr 路径表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generatePathExpression(std::shared_ptr<PathExpression> pathExpr);
    
    /**
     * 处理数组表达式（数组初始化）
     * @param arrayExpr 数组表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateArrayExpression(std::shared_ptr<ArrayExpression> arrayExpr);
    
    /**
     * 处理索引表达式（数组索引和指针索引）
     * @param indexExpr 索引表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateIndexExpression(std::shared_ptr<IndexExpression> indexExpr);
    
    /**
     * 处理元组表达式（元组创建）
     * @param tupleExpr 元组表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateTupleExpression(std::shared_ptr<TupleExpression> tupleExpr);
    
    /**
     * 处理结构体表达式（结构体创建和更新）
     * @param structExpr 结构体表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateStructExpression(std::shared_ptr<StructExpression> structExpr);
    
    /**
     * 处理函数调用表达式（普通函数调用）
     * @param callExpr 函数调用表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateCallExpression(std::shared_ptr<CallExpression> callExpr);
    
    /**
     * 处理方法调用表达式（实例方法和静态方法调用）
     * @param methodCallExpr 方法调用表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateMethodCallExpression(std::shared_ptr<MethodCallExpression> methodCallExpr);
    
    /**
     * 处理字段访问表达式（结构体字段访问和元组索引访问）
     * @param fieldExpr 字段访问表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateFieldExpression(std::shared_ptr<FieldExpression> fieldExpr);
    
    /**
     * 处理一元表达式（取负、逻辑非、解引用、取引用等）
     * @param unaryExpr 一元表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateUnaryExpression(std::shared_ptr<UnaryExpression> unaryExpr);
    
    /**
     * 处理二元表达式（算术、位运算、比较、逻辑运算）
     * @param binaryExpr 二元表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateBinaryExpression(std::shared_ptr<BinaryExpression> binaryExpr);
    
    /**
     * 处理赋值表达式（简单赋值和嵌套赋值）
     * @param assignExpr 赋值表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateAssignmentExpression(std::shared_ptr<AssignmentExpression> assignExpr);
    
    /**
     * 处理复合赋值表达式（+=、-= 等复合赋值）
     * @param compoundAssignExpr 复合赋值表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateCompoundAssignmentExpression(std::shared_ptr<CompoundAssignmentExpression> compoundAssignExpr);
    
    /**
     * 处理类型转换表达式（显式类型转换）
     * @param typeCastExpr 类型转换表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateTypeCastExpression(std::shared_ptr<TypeCastExpression> typeCastExpr);
    
    // ==================== 块表达式处理 ====================
    
    /**
     * 处理块表达式（包含语句和尾表达式）
     * @param blockExpr 块表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateBlockExpression(std::shared_ptr<BlockExpression> blockExpr);
    
    /**
     * 处理块表达式（包含语句和尾表达式），支持控制是否生成值
     * @param blockExpr 块表达式节点
     * @param needsValue 是否需要生成返回值
     * @return 生成的结果寄存器名称，如果不需要值则返回空字符串
     */
    std::string generateBlockExpression(std::shared_ptr<BlockExpression> blockExpr, bool needsValue);
    
    /**
     * 生成块中的所有语句，委托给 StatementGenerator
     * @param statements 语句列表
     * @return 是否成功生成所有语句
     */
    bool generateBlockStatements(const std::vector<std::shared_ptr<Statement>>& statements);
    
    /**
     * 生成单元类型的默认值
     * @return 生成的结果寄存器名称
     */
    std::string generateUnitValue();
    
    // ==================== 控制流表达式处理 ====================
    
    /**
     * 处理 if 表达式，包括 then/else 分支和 phi 节点
     * @param ifExpr if 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateIfExpression(std::shared_ptr<IfExpression> ifExpr);
    
    /**
     * 处理无限循环表达式，支持 break 值传递
     * @param loopExpr loop 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateLoopExpression(std::shared_ptr<InfiniteLoopExpression> loopExpr);
    
    /**
     * 处理条件循环表达式，返回值恒定为 unit
     * @param whileExpr while 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateWhileExpression(std::shared_ptr<PredicateLoopExpression> whileExpr);
    
    /**
     * 处理 break 表达式，支持带值和不带值
     * @param breakExpr break 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateBreakExpression(std::shared_ptr<BreakExpression> breakExpr);
    
    /**
     * 处理 continue 表达式，跳转到循环开始或条件检查
     * @param continueExpr continue 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateContinueExpression(std::shared_ptr<ContinueExpression> continueExpr);
    
    /**
     * 处理返回表达式，与 FunctionCodegen 协作
     * @param returnExpr return 表达式节点
     * @return 生成的结果寄存器名称
     */
    std::string generateReturnExpression(std::shared_ptr<ReturnExpression> returnExpr);
    
    // ==================== 工具方法 ====================
    
    /**
     * 从 NodeTypeMap 获取表达式的类型信息
     * @param expression 表达式节点
     * @return 类型信息字符串
     */
    std::string getExpressionType(std::shared_ptr<Expression> expression);
    
    /**
     * 判断表达式是否为左值（可赋值）
     * @param expression 表达式节点
     * @return 是否为左值
     */
    bool isLValue(std::shared_ptr<Expression> expression);
    
    /**
     * 获取左值表达式的指针
     * @param expression 左值表达式节点
     * @return 指针寄存器名称
     */
    std::string getLValuePointer(std::shared_ptr<Expression> expression);
    
    /**
     * 生成算术运算的 IR 代码
     * @param left 左操作数寄存器
     * @param right 右操作数寄存器
     * @param opType 运算符类型
     * @param resultType 结果类型
     * @return 运算结果寄存器名称
     */
    std::string generateArithmeticOperation(const std::string& left, const std::string& right, 
                                           Token opType, const std::string& resultType);
    
    /**
     * 生成比较运算的 IR 代码
     * @param left 左操作数寄存器
     * @param right 右操作数寄存器
     * @param opType 运算符类型
     * @param resultType 结果类型
     * @return 比较结果寄存器名称
     */
    std::string generateComparisonOperation(const std::string& left, const std::string& right, 
                                           Token opType, const std::string& resultType);
    
    /**
     * 生成逻辑运算的 IR 代码
     * @param left 左操作数寄存器
     * @param right 右操作数寄存器
     * @param opType 运算符类型
     * @param resultType 结果类型
     * @return 逻辑运算结果寄存器名称
     */
    std::string generateLogicalOperation(const std::string& left, const std::string& right, 
                                        Token opType, const std::string& resultType);
    
    /**
     * 存储表达式结果
     * @param expression 表达式节点
     * @param valueReg 值寄存器
     * @return 存储的指针寄存器名称
     */
    std::string storeExpressionResult(std::shared_ptr<Expression> expression, const std::string& valueReg);
    
    /**
     * 加载表达式值
     * @param expression 表达式节点
     * @return 加载的值寄存器名称
     */
    std::string loadExpressionValue(std::shared_ptr<Expression> expression);
    
    // ==================== 循环上下文管理 ====================
    
    /**
     * 进入循环上下文
     * @param startBB 循环开始基本块
     * @param endBB 循环结束基本块
     * @param bodyBB 循环体基本块
     */
    void enterLoopContext(const std::string& startBB, const std::string& endBB, const std::string& bodyBB);
    
    /**
     * 退出当前循环上下文
     */
    void exitLoopContext();
    
    /**
     * 获取当前循环上下文
     * @return 当前循环上下文，如果不存在则抛出异常
     */
    LoopContext getCurrentLoopContext();
    
    /**
     * 检查是否在循环上下文中
     * @return 是否在循环中
     */
    bool isInLoopContext() const;
    
    /**
     * 设置 break 值
     * @param valueReg break 值寄存器
     * @param valueType break 值类型
     */
    void setBreakValue(const std::string& valueReg, const std::string& valueType);
    
    // ==================== 错误处理接口 ====================
    
    /**
     * 检查是否有错误
     * @return 是否有错误
     */
    bool hasError() const;
    
    /**
     * 获取错误信息列表
     * @return 错误信息列表
     */
    std::vector<std::string> getErrorMessages() const;
    
    /**
     * 清空错误信息
     */
    void clearErrors();
    
    /**
     * 报告错误
     * @param message 错误信息
     */
    void reportError(const std::string& message);

private:
    // ==================== 私有辅助方法 ====================
    
    /**
     * 验证 StatementGenerator 是否已设置
     * @return 是否已设置
     */
    bool validateStatementGenerator();
    
    /**
     * 将 Token 类型的运算符映射为 LLVM 指令
     * @param token 运算符 Token
     * @param isSigned 是否为有符号运算
     * @return LLVM 指令字符串
     */
    std::string mapOperatorToLLVMInstruction(Token token, bool isSigned = true);
    
    /**
     * 生成字符串字面量的全局常量
     * @param stringValue 字符串值
     * @return 全局常量名称
     */
    std::string generateStringConstant(const std::string& stringValue);
    
    /**
     * 处理字符串转义字符
     * @param input 输入字符串
     * @return 转义后的字符串
     */
    std::string escapeString(const std::string& input);
    
    /**
     * 获取比较条件的 LLVM 表示
     * @param token 比较运算符 Token
     * @return LLVM 比较条件字符串
     */
    std::string getComparisonCondition(Token token);
    
    /**
     * 生成类型转换的 IR 代码
     * @param valueReg 源值寄存器
     * @param fromType 源类型
     * @param toType 目标类型
     * @return 转换后的值寄存器名称
     */
    std::string generateTypeConversion(const std::string& valueReg, 
                                     const std::string& fromType, 
                                     const std::string& toType);
    
    /**
     * 检查是否需要类型转换
     * @param fromType 源类型
     * @param toType 目标类型
     * @return 是否需要转换
     */
    bool needsTypeConversion(const std::string& fromType, const std::string& toType);
    
    /**
     * 分析左值表达式
     * @param expression 左值表达式
     * @return 左值信息（指针寄存器和类型）
     */
    std::pair<std::string, std::string> analyzeLValue(std::shared_ptr<Expression> expression);
    
    /**
     * 处理数组索引的地址计算
     * @param basePtr 基础指针寄存器
     * @param indexReg 索引寄存器
     * @param elementType 元素类型
     * @return 元素地址寄存器名称
     */
    std::string generateArrayIndexAddress(const std::string& basePtr, 
                                        const std::string& indexReg, 
                                        const std::string& elementType);
    
    /**
     * 处理字段访问的地址计算
     * @param basePtr 基础指针寄存器
     * @param fieldName 字段名
     * @param structType 结构体类型
     * @return 字段地址寄存器名称
     */
    std::string generateFieldAccessAddress(const std::string& basePtr, 
                                         const std::string& fieldName, 
                                         const std::string& structType);
    
    /**
     * 获取结构体字段的索引
     * @param structName 结构体名称
     * @param fieldName 字段名
     * @return 字段索引
     */
    int getStructFieldIndex(const std::string& structName, const std::string& fieldName);
    
    /**
     * 生成函数调用的参数列表
     * @param callParams 调用参数节点
     * @return 参数寄存器列表
     */
    std::vector<std::string> generateCallArguments(std::shared_ptr<CallParams> callParams);
    
    /**
     * 检查函数是否为内置函数
     * @param functionName 函数名
     * @return 是否为内置函数
     */
    bool isBuiltinFunction(const std::string& functionName);
    
    /**
     * 生成内置函数调用
     * @param functionName 函数名
     * @param args 参数寄存器列表
     * @return 调用结果寄存器名称
     */
    std::string generateBuiltinCall(const std::string& functionName,
                                   const std::vector<std::string>& args);
    
    /**
     * 将 Type 节点转换为字符串表示
     * @param type Type 节点指针
     * @return 类型字符串表示
     */
    std::string typeToStringHelper(std::shared_ptr<Type> type);
    
    /**
     * 从符号表获取函数返回类型
     * @param functionName 函数名
     * @return 返回类型的 LLVM 类型字符串
     */
    std::string getFunctionReturnType(const std::string& functionName);
    
    /**
     * 从结构体定义获取字段类型
     * @param structName 结构体名
     * @param fieldName 字段名
     * @return 字段类型的 LLVM 类型字符串
     */
    std::string getStructFieldType(const std::string& structName, const std::string& fieldName);
};
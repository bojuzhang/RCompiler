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
class ExpressionGenerator;
class StatementGenerator;

/**
 * FunctionCodegen 类 - 负责将 Rx 语言的函数定义和调用转换为 LLVM IR 文本
 * 
 * 重要架构说明：
 * - FunctionCodegen 处理函数定义、声明和调用的 IR 生成
 * - 与 ExpressionGenerator 和 StatementGenerator 协作处理函数体
 * - 管理函数上下文栈，支持嵌套函数调用
 * - 处理内置函数调用的特殊逻辑
 * 
 * 职责划分：
 * - 函数定义生成：生成函数签名、参数和函数体的 IR
 * - 函数体处理：处理函数体语句和表达式
 * - 返回值处理：处理基本返回值和尾表达式返回
 * - 内置函数支持：支持内置函数调用
 * - 组件协作：与其他 IR 生成组件协作
 */
class FunctionCodegen {
private:
    // 核心依赖组件
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;
    
    // ExpressionGenerator 和 StatementGenerator 引用（延迟初始化）
    ExpressionGenerator* expressionGenerator;
    StatementGenerator* statementGenerator;
    
    // 语义分析阶段的类型信息映射
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    
    // ==================== 函数上下文管理 ====================
    
    /**
     * 函数上下文结构
     * 包含函数生成过程中需要的所有状态信息
     */
    struct FunctionContext {
        std::string functionName;           // 函数名
        std::string returnType;            // 返回类型
        std::string returnBlock;            // 返回基本块
        std::string returnValueReg;         // 返回值寄存器
        std::vector<std::pair<std::string, std::string>> returnInputs; // PHI节点输入
        bool hasReturnStatement;           // 是否有返回语句
        bool hasMultipleReturns;           // 是否有多个返回点
        std::vector<std::string> parameters; // 参数列表
        std::unordered_map<std::string, std::string> parameterRegisters; // 参数寄存器映射
        
        FunctionContext() 
            : hasReturnStatement(false)
            , hasMultipleReturns(false) {}
            
        FunctionContext(const std::string& name, const std::string& retType)
            : functionName(name)
            , returnType(retType)
            , hasReturnStatement(false)
            , hasMultipleReturns(false) {}
    };
    
    // 函数上下文栈，支持嵌套函数
    std::stack<FunctionContext> functionStack;
    
    // 当前函数上下文指针
    FunctionContext* currentFunction;
    
    // ==================== 内置函数类型缓存 ====================
    
    // 内置函数类型信息缓存
    std::unordered_map<std::string, std::string> builtinFunctionTypes;
    
    // ==================== 错误处理 ====================
    
    bool hasErrors;
    std::vector<std::string> errorMessages;

public:
    // 构造函数
    FunctionCodegen(std::shared_ptr<IRBuilder> irBuilder,
                    std::shared_ptr<TypeMapper> typeMapper,
                    std::shared_ptr<ScopeTree> scopeTree,
                    const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& nodeTypeMap);
    
    // 析构函数
    ~FunctionCodegen() = default;
    
    // ==================== 依赖组件设置接口 ====================
    
    /**
     * 设置 ExpressionGenerator 引用（延迟初始化）
     * @param exprGen ExpressionGenerator 实例指针
     */
    void setExpressionGenerator(ExpressionGenerator* exprGen);
    
    /**
     * 设置 StatementGenerator 引用（延迟初始化）
     * @param stmtGen StatementGenerator 实例指针
     */
    void setStatementGenerator(StatementGenerator* stmtGen);
    
    /**
     * 获取 ExpressionGenerator 引用
     * @return ExpressionGenerator 实例指针
     */
    ExpressionGenerator* getExpressionGenerator() const;
    
    /**
     * 获取 StatementGenerator 引用
     * @return StatementGenerator 实例指针
     */
    StatementGenerator* getStatementGenerator() const;
    
    // ==================== 主要生成接口 ====================
    
    /**
     * 生成完整函数定义
     * @param function 函数节点
     * @return 是否成功生成
     */
    bool generateFunction(std::shared_ptr<Function> function);
    
    /**
     * 生成函数声明
     * @param function 函数节点
     * @return 是否成功生成
     */
    bool generateFunctionDeclaration(std::shared_ptr<Function> function);
    
    /**
     * 生成函数体
     * @param function 函数节点
     * @return 是否成功生成
     */
    bool generateFunctionBody(std::shared_ptr<Function> function);
    
    /**
     * 预处理函数，收集所有内部函数
     * @param function 函数节点
     * @return 收集的内部函数列表
     */
    std::vector<std::shared_ptr<Function>> preprocessNestedFunctions(std::shared_ptr<Function> function);
    
    /**
     * 生成内部函数列表
     * @param nestedFunctions 内部函数列表
     * @return 是否成功生成
     */
    bool generateNestedFunctions(const std::vector<std::shared_ptr<Function>>& nestedFunctions);
    
    /**
     * 递归收集语句中的内部函数
     * @param statement 语句节点
     * @param nestedFunctions 收集的内部函数列表
     */
    void collectNestedFunctions(std::shared_ptr<Statement> statement,
                             std::vector<std::shared_ptr<Function>>& nestedFunctions);
    
    // ==================== 内置函数调用生成 ====================
    
    /**
     * 生成内置函数调用的 IR 代码
     * @param functionName 函数名
     * @param args 参数寄存器列表
     * @return 调用结果寄存器名称
     */
    std::string generateBuiltinCall(const std::string& functionName,
                                   const std::vector<std::string>& args);
    
    // ==================== 参数处理接口 ====================
    
    /**
     * 生成函数参数的处理代码，包括栈分配和寄存器映射
     * @param function 函数节点
     * @return 参数寄存器列表
     */
    std::vector<std::string> generateParameters(std::shared_ptr<Function> function);
    
    /**
     * 生成参数加载代码，处理值类型和引用类型
     * @param param 参数节点
     * @param index 参数索引
     * @return 参数寄存器名称
     */
    std::string generateArgumentLoad(std::shared_ptr<FunctionParam> param, int index);
    
    /**
     * 为参数分配栈空间并存储参数值
     * @param param 参数节点
     * @param index 参数索引
     * @return 参数存储寄存器名称
     */
    std::string generateParameterAlloca(std::shared_ptr<FunctionParam> param, int index);
    
    // ==================== 返回值处理接口 ====================
    
    /**
     * 生成 return 语句的 IR 代码
     * @param returnExpr return 表达式节点
     * @return 是否成功生成
     */
    bool generateReturnStatement(std::shared_ptr<ReturnExpression> returnExpr);
    
    /**
     * 生成返回值表达式的代码
     * @param expression 返回值表达式
     * @return 返回值寄存器名称
     */
    std::string generateReturnValue(std::shared_ptr<Expression> expression);
    
    /**
     * 生成返回值的 PHI 节点，处理多返回点
     * @return PHI 节点寄存器名称
     */
    std::string generateReturnPhi();
    
    /**
     * 处理尾表达式返回的情况
     * @param expression 尾表达式
     * @return 返回值寄存器名称
     */
    std::string generateTailExpressionReturn(std::shared_ptr<Expression> expression);
    
    /**
     * 生成默认返回值，用于无显式返回的情况
     * @return 默认返回值寄存器名称
     */
    std::string generateDefaultReturn();
    
    // ==================== 函数签名生成接口 ====================
    
    /**
     * 生成函数签名字符串
     * @param function 函数节点
     * @return 函数签名字符串
     */
    std::string generateFunctionSignature(std::shared_ptr<Function> function);
    
    /**
     * 生成参数列表，包含名称和类型对
     * @param function 函数节点
     * @return 参数列表字符串
     */
    std::string generateParameterList(std::shared_ptr<Function> function);
    
    /**
     * 生成函数类型字符串
     * @param function 函数节点
     * @return 函数类型字符串
     */
    std::string generateFunctionType(std::shared_ptr<Function> function);
    
    // ==================== 工具方法接口 ====================
    
    /**
     * 获取函数名称，处理名称修饰
     * @param function 函数节点
     * @return 修饰后的函数名
     */
    std::string getFunctionName(std::shared_ptr<Function> function);
    
    /**
     * 获取修饰后的函数名
     * @param baseName 基础函数名
     * @return 修饰后的函数名
     */
    std::string getMangledName(const std::string& baseName);
    
    /**
     * 检查函数是否为内置函数
     * @param functionName 函数名
     * @return 是否为内置函数
     */
    bool isBuiltinFunction(const std::string& functionName);
    
    /**
     * 获取内置函数的类型信息
     * @param functionName 函数名
     * @return 函数类型字符串
     */
    std::string getBuiltinFunctionType(const std::string& functionName);
    
    /**
     * 生成函数调用的参数处理代码
     * @param callParams 调用参数节点
     * @return 参数寄存器列表
     */
    std::vector<std::string> generateCallArguments(std::shared_ptr<CallParams> callParams);
    
    /**
     * 处理结构体参数的特殊传递方式
     * @param structReg 结构体寄存器
     * @param structType 结构体类型
     * @return 处理后的参数寄存器
     */
    std::string generateStructArgument(const std::string& structReg, const std::string& structType);
    
    // ==================== 函数上下文管理接口 ====================
    
    /**
     * 进入函数上下文
     * @param functionName 函数名
     * @param returnType 返回类型
     */
    void enterFunction(const std::string& functionName, const std::string& returnType);
    
    /**
     * 退出当前函数上下文
     */
    void exitFunction();
    
    /**
     * 检查是否在函数上下文中
     * @return 是否在函数中
     */
    bool isInFunction() const;
    
    /**
     * 获取当前函数上下文
     * @return 当前函数上下文指针
     */
    FunctionContext* getCurrentFunction();
    
    /**
     * 获取当前函数名
     * @return 当前函数名
     */
    std::string getCurrentFunctionName() const;
    
    /**
     * 获取当前函数返回类型
     * @return 当前函数返回类型
     */
    std::string getCurrentFunctionReturnType() const;
    
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
     * 验证 ExpressionGenerator 是否已设置
     * @return 是否已设置
     */
    bool validateExpressionGenerator();
    
    /**
     * 验证 StatementGenerator 是否已设置
     * @return 是否已设置
     */
    bool validateStatementGenerator();
    
    /**
     * 生成函数序言代码
     * @param function 函数节点
     */
    void generatePrologue(std::shared_ptr<Function> function);
    
    /**
     * 生成函数尾声代码
     * @param function 函数节点
     */
    void generateEpilogue(std::shared_ptr<Function> function);
    
    /**
     * 处理返回值的通用逻辑
     * @param valueReg 返回值寄存器
     * @param valueType 返回值类型
     * @return 处理后的返回值寄存器
     */
    std::string handleReturnValue(const std::string& valueReg, const std::string& valueType);
    
    /**
     * 设置参数作用域和符号表
     * @param function 函数节点
     */
    void setupParameterScope(std::shared_ptr<Function> function);
    
    /**
     * 初始化内置函数类型缓存
     */
    void initializeBuiltinFunctionTypes();
    
    /**
     * 获取参数的 LLVM 类型
     * @param param 参数节点
     * @param paramIndex 参数索引（用于识别 self 参数）
     * @return LLVM 类型字符串
     */
    std::string getParameterLLVMType(std::shared_ptr<FunctionParam> param, size_t paramIndex = 0);
    
    /**
     * 获取函数返回类型的 LLVM 表示
     * @param function 函数节点
     * @return 返回类型字符串
     */
    std::string getFunctionReturnLLVMType(std::shared_ptr<Function> function);
    
    /**
     * 检查函数是否需要返回值
     * @param function 函数节点
     * @return 是否需要返回值
     */
    bool functionNeedsReturnValue(std::shared_ptr<Function> function);
    
    /**
     * 生成函数调用的完整 IR
     * @param functionName 函数名
     * @param args 参数列表
     * @param returnType 返回类型
     * @return 调用结果寄存器名称
     */
    std::string generateFunctionCall(const std::string& functionName,
                                   const std::vector<std::string>& args,
                                   const std::string& returnType);
    
    /**
     * 处理大结构体参数的传递优化
     * @param paramReg 参数寄存器
     * @param paramType 参数类型
     * @return 优化后的参数寄存器
     */
    std::string optimizeLargeStructParameter(const std::string& paramReg, const std::string& paramType);
    
    /**
     * 生成类型转换代码
     * @param valueReg 源值寄存器
     * @param fromType 源类型
     * @param toType 目标类型
     * @return 转换后的值寄存器
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
     * 从 AST 节点获取类型信息
     * @param node AST 节点
     * @return 类型字符串
     */
    std::string getNodeLLVMType(std::shared_ptr<ASTNode> node);
    
    /**
     * 创建函数返回基本块
     * @return 返回基本块名称
     */
    std::string createReturnBlock();
    
    /**
     * 添加返回值到 PHI 节点
     * @param valueReg 返回值寄存器
     * @param sourceBlock 来源基本块
     */
    void addReturnValueToPhi(const std::string& valueReg, const std::string& sourceBlock);
    
    /**
     * 完成函数返回处理
     */
    void finalizeFunctionReturn();
    
    /**
     * 验证函数签名的有效性
     * @param function 函数节点
     * @return 是否有效
     */
    bool validateFunctionSignature(std::shared_ptr<Function> function);
    
    /**
     * 处理函数参数的默认值
     * @param param 参数节点
     * @param index 参数索引
     * @return 默认值寄存器名称
     */
    std::string handleParameterDefaultValue(std::shared_ptr<FunctionParam> param, int index);
    
    /**
     * 从函数参数的 pattern 中提取参数名称
     * @param param 函数参数节点
     * @return 参数名称字符串
     */
    std::string getParameterName(std::shared_ptr<FunctionParam> param);
};
#pragma once

#include <memory>
#include <string>
#include <unordered_set>
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
class FunctionCodegen;

/**
 * StatementGenerator 类 - 负责将 Rx 语言的语句转换为 LLVM IR 文本
 * 
 * 重要架构说明：
 * - Statement 使用组合模式，控制流语句（if、loop、while、break、continue、return）都是 Expression 的子类
 * - BlockExpression 包含 Statement 向量，导致 StatementGenerator 和 ExpressionGenerator 之间存在循环依赖
 * - 通过前向声明和接口分离来解决循环依赖问题
 * 
 * 职责划分：
 * - StatementGenerator：只处理真正的语句（Let、Item、ExpressionStatement）
 * - ExpressionGenerator：处理所有表达式，包括 BlockExpression 和控制流表达式
 * - BlockExpression：完全由 ExpressionGenerator 处理，通过调用 StatementGenerator 处理其中的语句
 */
class StatementGenerator {
private:
    // 核心依赖组件
    std::shared_ptr<IRBuilder> irBuilder;
    std::shared_ptr<TypeMapper> typeMapper;
    std::shared_ptr<ScopeTree> scopeTree;

    std::unordered_map<ExpressionStatement*, std::string> mapExprStatementToRegname;
    
    // ExpressionGenerator 和 FunctionCodegen 引用（延迟初始化以解决循环依赖）
    ExpressionGenerator* expressionGenerator;
    FunctionCodegen* functionCodegen;
    
    // 控制流上下文管理
    struct ControlFlowContext {
        std::string loopHeader;
        std::string loopBody;
        std::string loopExit;
        std::vector<std::string> breakTargets;
        std::vector<std::string> continueTargets;
        
        ControlFlowContext() = default;
        
        ControlFlowContext(const std::string& header, const std::string& body, const std::string& exit)
            : loopHeader(header), loopBody(body), loopExit(exit) {
            breakTargets.push_back(exit);
            continueTargets.push_back(header);
        }
    };
    
    std::stack<ControlFlowContext> controlFlowStack;
    
    // 错误处理
    bool hasErrors;
    std::vector<std::string> errorMessages;
    
    // 不可达代码跟踪
    bool isUnreachable;

public:
    // 构造函数
    StatementGenerator(std::shared_ptr<IRBuilder> irBuilder,
                      std::shared_ptr<TypeMapper> typeMapper,
                      std::shared_ptr<ScopeTree> scopeTree);
    
    // 析构函数
    ~StatementGenerator() = default;
    
    // ==================== ExpressionGenerator 集成接口 ====================
    
    /**
     * 设置 ExpressionGenerator 引用（延迟初始化）
     * @param exprGen ExpressionGenerator 实例指针
     */
    void setExpressionGenerator(ExpressionGenerator* exprGen);
    
    /**
     * 获取 ExpressionGenerator 引用
     * @return ExpressionGenerator 实例指针
     */
    ExpressionGenerator* getExpressionGenerator() const;
    
    /**
     * 设置 FunctionCodegen 引用（延迟初始化）
     * @param funcGen FunctionCodegen 实例指针
     */
    void setFunctionCodegen(FunctionCodegen* funcGen);
    
    /**
     * 获取 FunctionCodegen 引用
     * @return FunctionCodegen 实例指针
     */
    FunctionCodegen* getFunctionCodegen() const;
    
    // ==================== 主要生成接口 ====================
    
    /**
     * 语句生成的统一入口点
     * @param statement 语句节点
     * @return 是否成功生成
     */
    bool generateStatement(std::shared_ptr<Statement> statement);
    
    /**
     * 生成语句列表
     * @param statements 语句列表
     * @return 是否成功生成所有语句
     */
    bool generateStatements(const std::vector<std::shared_ptr<Statement>>& statements);
    
    // ==================== 专用生成方法 ====================
    
    /**
     * 处理 Let 语句（变量声明）
     * @param letStatement Let 语句节点
     * @return 是否成功生成
     */
    bool generateLetStatement(std::shared_ptr<LetStatement> letStatement);
    
    /**
     * 处理表达式语句
     * @param exprStatement 表达式语句节点
     * @return 是否成功生成
     */
    bool generateExpressionStatement(std::shared_ptr<ExpressionStatement> exprStatement);
    
    /**
     * 处理项语句（函数、结构体等定义）
     * @param item 项节点
     * @return 是否成功生成
     */
    bool generateItemStatement(std::shared_ptr<Item> item);
    
    // ==================== 控制流上下文管理 ====================
    
    /**
     * 进入控制流上下文
     * @param header 循环头基本块
     * @param body 循环体基本块
     * @param exit 循环退出基本块
     */
    void enterControlFlowContext(const std::string& header, const std::string& body, const std::string& exit);
    
    /**
     * 退出当前控制流上下文
     */
    void exitControlFlowContext();
    
    /**
     * 获取当前控制流上下文
     * @return 当前控制流上下文，如果不存在则抛出异常
     */
    ControlFlowContext getCurrentControlFlowContext();
    
    /**
     * 检查是否在循环上下文中
     * @return 是否在循环中
     */
    bool isInLoopContext() const;
    
    /**
     * 添加 break 目标
     * @param target break 目标基本块
     */
    void addBreakTarget(const std::string& target);
    
    /**
     * 添加 continue 目标
     * @param target continue 目标基本块
     */
    void addContinueTarget(const std::string& target);
    
    /**
     * 获取当前 break 目标
     * @return break 目标基本块名称
     */
    std::string getCurrentBreakTarget();
    
    /**
     * 获取当前 continue 目标
     * @return continue 目标基本块名称
     */
    std::string getCurrentContinueTarget();
    
    // ==================== 变量管理接口 ====================
    
    /**
     * 为变量分配存储空间
     * @param variableName 变量名
     * @param type 变量类型
     * @return 分配的寄存器名称
     */
    std::string allocateVariable(const std::string& variableName, const std::string& type);
    
    /**
     * 存储变量值
     * @param variableName 变量名
     * @param valueReg 值寄存器
     * @param type 值类型
     */
    void storeVariable(const std::string& variableName, const std::string& valueReg, const std::string& type);
    
    /**
     * 加载变量值
     * @param variableName 变量名
     * @return 加载的值寄存器名称
     */
    std::string loadVariable(const std::string& variableName);
    
    // ==================== 工具方法 ====================
    
    /**
     * 获取语句的类型信息
     * @param statement 语句节点
     * @return 类型信息字符串
     */
    std::string getStatementType(std::shared_ptr<Statement> statement);
    std::string getStatementRegname(ExpressionStatement* exprStatement);
    
    /**
     * 检查语句是否为终止符
     * @param statement 语句节点
     * @return 是否为终止符
     */
    bool isStatementTerminator(std::shared_ptr<Statement> statement);
    
    /**
     * 处理不可达代码
     * @param statement 不可达的语句节点
     */
    void handleUnreachableCode(std::shared_ptr<Statement> statement);
    
    /**
     * 从模式中提取变量名
     * @param pattern 模式节点
     * @return 变量名，如果无法提取则返回空字符串
     */
    std::string extractVariableName(std::shared_ptr<Pattern> pattern);
    
    /**
     * 检查变量是否在当前作用域中
     * @param variableName 变量名
     * @return 是否在当前作用域中
     */
    bool isVariableInCurrentScope(const std::string& variableName);
    
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
     * 处理函数定义项
     * @param function 函数节点
     * @return 是否成功生成
     */
    bool generateFunctionItem(std::shared_ptr<Function> function);
    
    /**
     * 处理结构体定义项
     * @param structDef 结构体节点
     * @return 是否成功生成
     */
    bool generateStructItem(std::shared_ptr<StructStruct> structDef);
    
    /**
     * 处理常量定义项
     * @param constant 常量节点
     * @return 是否成功生成
     */
    bool generateConstantItem(std::shared_ptr<ConstantItem> constant);
    
    /**
     * 处理 impl 块项
     * @param impl impl 块节点
     * @return 是否成功生成
     */
    bool generateImplItem(std::shared_ptr<InherentImpl> impl);
    
    /**
     * 创建不可达基本块
     * @return 不可达基本块名称
     */
    std::string createUnreachableBlock();
    
    /**
     * 设置不可达状态
     */
    void setUnreachable();
    
    /**
     * 重置可达状态
     */
    void resetReachable();
    
    /**
     * 检查当前是否不可达
     * @return 是否不可达
     */
    bool checkUnreachable() const;
    
    /**
     * 验证 ExpressionGenerator 是否已设置
     * @return 是否已设置
     */
    bool validateExpressionGenerator();
    
    /**
     * 获取变量的 LLVM 类型
     * @param variableName 变量名
     * @return LLVM 类型字符串
     */
    std::string getVariableLLVMType(const std::string& variableName);
    
    /**
     * 生成变量注册到符号表
     * @param variableName 变量名
     * @param type 变量类型
     * @param registerName 寄存器名
     */
    void registerVariable(const std::string& variableName, const std::string& type, const std::string& registerName);
    
private:
    /**
     * 将 Type 节点转换为字符串表示
     * @param type Type 节点指针
     * @return 类型字符串表示
     */
    std::string typeToStringHelper(std::shared_ptr<Type> type);
};
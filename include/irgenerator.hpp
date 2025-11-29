#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include "astnodes.hpp"
#include "irbuilder.hpp"
#include "typemapper.hpp"
#include "scope.hpp"
#include "symbol.hpp"
#include "typewrapper.hpp"

// 前向声明
class ExpressionGenerator;
class StatementGenerator;
class FunctionCodegen;
class BuiltinDeclarator;
class TypeChecker;

/**
 * IRGenerator 类 - Rx 语言编译器 IR 生成模块的主控制器
 * 
 * 核心职责：
 * 1. 统一协调：协调整个 IR 生成过程，管理所有子组件的交互
 * 2. 直接输出：生成 LLVM IR 文本并直接输出到 stdout
 * 3. 错误处理：提供统一的错误检测、报告和恢复机制
 * 4. 模块管理：管理各个子组件的生命周期和依赖关系
 * 5. 性能优化：确保高效的 IR 生成和输出
 * 
 * 设计特点：
 * - 无 LLVM 依赖：完全自定义的 IRBuilder 系统
 * - 文本输出：直接生成 LLVM IR 文本
 * - 寄存器管理：自动分配和命名寄存器
 * - 基本块管理：自动生成和管理基本块
 * - 类型安全：保持类型系统的正确性
 * - 错误处理：统一的错误报告机制
 */
class IRGenerator {
private:
    // ==================== 核心组件 ====================
    
    // IR 构建器 - 负责生成 LLVM IR 文本
    std::shared_ptr<IRBuilder> irBuilder;
    
    // 类型映射器 - 负责类型系统映射
    std::shared_ptr<TypeMapper> typeMapper;
    
    // 表达式生成器 - 负责表达式 IR 生成
    std::unique_ptr<ExpressionGenerator> expressionGenerator;
    
    // 语句生成器 - 负责语句 IR 生成
    std::unique_ptr<StatementGenerator> statementGenerator;
    
    // 函数代码生成器 - 负责函数 IR 生成
    std::unique_ptr<FunctionCodegen> functionCodegen;
    
    // 内置函数声明器 - 负责内置函数声明
    std::unique_ptr<BuiltinDeclarator> builtinDeclarator;
    
    // ==================== 输入数据 ====================
    
    // 语义分析阶段的符号表
    std::shared_ptr<ScopeTree> scopeTree;
    
    // 语义分析阶段的类型检查器
    std::shared_ptr<TypeChecker> typeChecker;
    
    // AST 节点到类型的映射
    std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>> nodeTypeMap;
    
    // ==================== 输出管理 ====================
    
    // IR 文本输出流
    std::ostringstream outputStream;
    
    // 输出缓冲区管理
    std::string outputBuffer;
    
    // 输出格式控制
    enum class OutputFormat {
        STANDARD,    // 标准 LLVM IR 格式
        DEBUG,        // 调试格式（包含更多注释）
        COMPACT       // 紧凑格式（最小化注释）
    } outputFormat;
    
    // ==================== 错误处理 ====================
    
    // 错误状态
    bool generationHasErrors;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;
    
    // ==================== 生成状态管理 ====================
    
    // 当前生成上下文
    struct GenerationContext {
        std::string currentFunction;
        std::string currentModule;
        bool isInFunction;
        bool isInGlobalScope;
        
        GenerationContext() : isInFunction(false), isInGlobalScope(true) {}
    } currentContext;
    
    // 组件初始化状态
    bool componentsInitialized;
    
    // ==================== 性能优化 ====================
    
    // 内存池管理
    struct MemoryPool {
        std::vector<std::unique_ptr<char[]>> pools;
        size_t currentPoolSize;
        size_t usedBytes;
        
        MemoryPool() : currentPoolSize(4096), usedBytes(0) {}
    } memoryPool;
    
    // 输出缓冲区阈值
    static const size_t OUTPUT_BUFFER_THRESHOLD = 8192;

public:
    // ==================== 构造函数和析构函数 ====================
    
    /**
     * 构造函数
     * @param scopeTree 语义分析阶段的符号表
     * @param typeChecker 语义分析阶段的类型检查器
     * @param outputStream 输出流，默认为 std::cout
     */
    IRGenerator(std::shared_ptr<ScopeTree> scopeTree,
                std::shared_ptr<TypeChecker> typeChecker,
                std::ostream& outputStream = std::cout);
    
    /**
     * 析构函数
     */
    ~IRGenerator();
    
    // ==================== 主要生成接口 ====================
    
    /**
     * 主要的 IR 生成入口点
     * @param topLevelItems AST 顶层项列表
     * @return 是否成功生成
     */
    bool generateIR(const std::vector<std::shared_ptr<Item>>& topLevelItems);
    
    /**
     * 生成单个顶层项
     * @param item 顶层项
     * @return 是否成功生成
     */
    bool generateTopLevelItem(std::shared_ptr<Item> item);
    
    // ==================== 输出管理接口 ====================
    
    /**
     * 获取生成的 IR 文本
     * @return IR 文本字符串
     */
    std::string getIROutput() const;
    
    /**
     * 强制刷新输出缓冲区
     */
    void flushOutput();
    
    /**
     * 设置输出格式
     * @param format 输出格式
     */
    void setOutputFormat(OutputFormat format);
    
    /**
     * 获取当前输出格式
     * @return 当前输出格式
     */
    OutputFormat getOutputFormat() const;
    
    // ==================== 组件访问接口 ====================
    
    /**
     * 获取 IRBuilder 实例
     * @return IRBuilder 共享指针
     */
    std::shared_ptr<IRBuilder> getIRBuilder() const;
    
    /**
     * 获取 TypeMapper 实例
     * @return TypeMapper 共享指针
     */
    std::shared_ptr<TypeMapper> getTypeMapper() const;
    
    /**
     * 获取 ExpressionGenerator 实例
     * @return ExpressionGenerator 指针
     */
    ExpressionGenerator* getExpressionGenerator() const;
    
    /**
     * 获取 StatementGenerator 实例
     * @return StatementGenerator 指针
     */
    StatementGenerator* getStatementGenerator() const;
    
    /**
     * 获取 FunctionCodegen 实例
     * @return FunctionCodegen 指针
     */
    FunctionCodegen* getFunctionCodegen() const;
    
    /**
     * 获取 BuiltinDeclarator 实例
     * @return BuiltinDeclarator 指针
     */
    BuiltinDeclarator* getBuiltinDeclarator() const;
    
    // ==================== 符号表访问接口 ====================
    
    /**
     * 根据名称获取符号信息
     * @param name 符号名称
     * @return 符号共享指针
     */
    std::shared_ptr<Symbol> getSymbol(const std::string& name) const;
    
    /**
     * 获取符号的类型信息
     * @param name 符号名称
     * @return 类型字符串
     */
    std::string getSymbolType(const std::string& name) const;
    
    /**
     * 获取节点类型映射
     * @return 节点类型映射的引用
     */
    const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& getNodeTypeMap() const;
    
    // ==================== 错误处理接口 ====================
    
    /**
     * 检查是否有生成错误
     * @return 是否有错误
     */
    bool hasGenerationErrors() const;
    
    /**
     * 获取所有错误信息
     * @return 错误信息列表
     */
    std::vector<std::string> getErrors() const;
    
    /**
     * 获取所有警告信息
     * @return 警告信息列表
     */
    std::vector<std::string> getWarnings() const;
    
    /**
     * 清理错误状态
     */
    void clearErrors();
    
    /**
     * 报告错误
     * @param message 错误信息
     * @param node 相关的 AST 节点（可选）
     */
    void reportError(const std::string& message, std::shared_ptr<ASTNode> node = nullptr);
    
    /**
     * 报告警告
     * @param message 警告信息
     * @param node 相关的 AST 节点（可选）
     */
    void reportWarning(const std::string& message, std::shared_ptr<ASTNode> node = nullptr);

private:
    // ==================== 内部初始化方法 ====================
    
    /**
     * 初始化所有子组件
     * @return 是否成功初始化
     */
    bool initializeComponents();
    
    /**
     * 设置组件间的依赖关系
     * @return 是否成功设置
     */
    bool setupComponentDependencies();
    
    /**
     * 初始化 LLVM 模块
     * @return 是否成功初始化
     */
    bool initializeModule();
    
    /**
     * 声明所有内置函数
     * @return 是否成功声明
     */
    bool declareBuiltinFunctions();
    
    // ==================== 生成方法 ====================
    
    /**
     * 生成所有顶层项
     * @param topLevelItems 顶层项列表
     * @return 是否成功生成
     */
    bool generateTopLevelItems(const std::vector<std::shared_ptr<Item>>& topLevelItems);
    
    /**
     * 生成函数定义
     * @param function 函数节点
     * @return 是否成功生成
     */
    bool generateFunction(std::shared_ptr<Function> function);
    
    /**
     * 生成结构体定义
     * @param structDef 结构体节点
     * @return 是否成功生成
     */
    bool generateStruct(std::shared_ptr<StructStruct> structDef);
    
    /**
     * 生成常量定义
     * @param constant 常量节点
     * @return 是否成功生成
     */
    bool generateConstant(std::shared_ptr<ConstantItem> constant);
    
    /**
     * 生成 impl 块
     * @param impl impl 块节点
     * @return 是否成功生成
     */
    bool generateImpl(std::shared_ptr<InherentImpl> impl);
    
    // ==================== 工具方法 ====================
    
    /**
     * 验证生成结果
     * @return 是否验证通过
     */
    bool validateGeneration();
    
    /**
     * 完成模块生成
     */
    void finalizeModule();
    
    /**
     * 获取节点的源代码位置信息
     * @param node AST 节点
     * @return 位置信息字符串
     */
    std::string getNodeLocation(std::shared_ptr<ASTNode> node) const;
    
    /**
     * 格式化错误信息
     * @param message 错误信息
     * @param location 位置信息
     * @return 格式化的错误信息
     */
    std::string formatErrorMessage(const std::string& message, const std::string& location) const;
    
    /**
     * 格式化警告信息
     * @param message 警告信息
     * @param location 位置信息
     * @return 格式化的警告信息
     */
    std::string formatWarningMessage(const std::string& message, const std::string& location) const;
    
    // ==================== 内存管理 ====================
    
    /**
     * 从内存池分配内存
     * @param size 分配大小
     * @return 分配的内存指针
     */
    void* allocateFromPool(size_t size);
    
    /**
     * 清理内存池
     */
    void cleanupMemoryPool();
    
    // ==================== 输出优化 ====================
    
    /**
     * 批量输出缓冲区内容
     */
    void flushOutputBuffer();
    
    /**
     * 检查是否需要刷新缓冲区
     * @return 是否需要刷新
     */
    bool shouldFlushBuffer() const;
    
    /**
     * 优化输出格式
     * @param output 原始输出
     * @return 优化后的输出
     */
    std::string optimizeOutput(const std::string& output) const;
    
    // ==================== 调试支持 ====================
    
    /**
     * 生成调试注释
     * @param message 注释内容
     * @param node 相关的 AST 节点（可选）
     */
    void emitDebugComment(const std::string& message, std::shared_ptr<ASTNode> node = nullptr);
    
    /**
     * 检查是否启用了调试模式
     * @return 是否启用调试模式
     */
    bool isDebugEnabled() const;
    
    /**
     * 生成生成统计信息
     * @return 统计信息字符串
     */
    std::string generateStatistics() const;
};
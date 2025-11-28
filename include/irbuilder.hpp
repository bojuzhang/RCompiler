#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <memory>
#include "scope.hpp"
#include "symbol.hpp"
#include "typemapper.hpp"

// 前向声明
class ASTNode;
class SemanticType;

// 寄存器信息结构
struct RegisterInfo {
    std::string name;
    std::string llvmType;
    std::shared_ptr<Scope> scope;
    bool isPointer;
    bool isTemporary;
    
    RegisterInfo(const std::string& n, const std::string& type, 
                std::shared_ptr<Scope> s, bool isPtr = false, bool isTemp = false)
        : name(n), llvmType(type), scope(s), isPointer(isPtr), isTemporary(isTemp) {}
};

// 基本块信息结构
struct BasicBlockInfo {
    std::string name;
    std::string prefix;
    int counter;
    bool isFinished;
    
    BasicBlockInfo(const std::string& n, const std::string& p, int c = 0)
        : name(n), prefix(p), counter(c), isFinished(false) {}
};

// 控制流上下文结构
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

// 作用域信息结构
struct ScopeInfo {
    std::shared_ptr<Scope> scope;
    std::unordered_map<std::string, int> variableCounters;
    std::unordered_set<std::string> registers;
    
    ScopeInfo(std::shared_ptr<Scope> s) : scope(s) {}
};

// IRBuilder 类 - 自定义 LLVM IR 文本生成器
class IRBuilder {
private:
    // 输出流
    std::ostream& outputStream;
    
    // 作用域树引用
    std::shared_ptr<ScopeTree> scopeTree;
    
    // 类型映射器
    std::shared_ptr<TypeMapper> typeMapper;
    
    // 寄存器管理
    int registerCounter;
    std::unordered_map<std::string, std::shared_ptr<RegisterInfo>> registers;
    std::vector<ScopeInfo> scopeStack;
    
    // 基本块管理
    int basicBlockCounter;
    std::unordered_map<std::string, std::shared_ptr<BasicBlockInfo>> basicBlocks;
    std::string currentBasicBlock;
    std::vector<ControlFlowContext> controlFlowStack;
    
    // 缩进管理
    int indentLevel;
    static const int INDENT_SIZE = 2;
    
    // 错误状态
    bool hasErrors;
    std::vector<std::string> errorMessages;

public:
    // 构造函数
    explicit IRBuilder(std::ostream& output, std::shared_ptr<ScopeTree> scopeTree);
    
    // 析构函数
    ~IRBuilder() = default;
    
    // 获取TypeMapper实例
    std::shared_ptr<TypeMapper> getTypeMapper() const { return typeMapper; }
    
    // ==================== 寄存器管理接口 ====================
    
    // 生成新的临时寄存器
    std::string newRegister();
    
    // 为指定变量生成寄存器
    std::string newRegister(const std::string& variableName);
    
    // 生成带类型区分的寄存器
    std::string newRegister(const std::string& variableName, const std::string& suffix);
    
    // 作用域同步方法
    void syncWithScopeTree();
    std::shared_ptr<Scope> getCurrentScope();
    void cleanupScopeRegisters();
    
    // 寄存器查询方法
    std::string getVariableRegister(const std::string& variableName);
    std::string getSymbolRegister(std::shared_ptr<Symbol> symbol);
    std::string getRegisterType(const std::string& registerName);
    void setRegisterType(const std::string& registerName, const std::string& type);
    std::shared_ptr<Scope> getRegisterScope(const std::string& registerName);
    
    // 作用域检查方法
    bool isVariableInCurrentScope(const std::string& variableName);
    
    // ==================== 基本块管理接口 ====================
    
    // 基本块创建方法
    std::string newBasicBlock(const std::string& prefix = "");
    std::string newBasicBlock(const std::string& prefix, int counter);
    std::string newNestedBasicBlock(const std::string& prefix = "");
    
    // 基本块控制方法
    void setCurrentBasicBlock(const std::string& basicBlockName);
    std::string getCurrentBasicBlock();
    void finishCurrentBasicBlock();
    
    // 控制流上下文管理
    void enterControlFlowContext(const std::string& header, const std::string& body, const std::string& exit);
    void exitControlFlowContext();
    ControlFlowContext getCurrentControlFlowContext();
    
    // 基本块验证方法
    bool isBasicBlockExists(const std::string& basicBlockName);
    
    // ==================== 基础指令生成接口 ====================
    
    // 基础指令生成
    void emitInstruction(const std::string& instruction);
    void emitComment(const std::string& comment);
    
    // ==================== 类型相关指令 ====================
    
    // 类型映射方法
    std::string mapRxTypeToLLVM(std::shared_ptr<SemanticType> type);
    std::string getPointerElementType(const std::string& pointerType);
    std::string getArrayElementType(const std::string& arrayType);
    
    // 类型检查方法
    bool isPointerType(const std::string& type);
    bool isIntegerType(const std::string& type);
    int getTypeSize(const std::string& type);
    
    // 类型相关指令
    std::string emitAlloca(const std::string& type, int alignment = 0);
    void emitStore(const std::string& value, const std::string& pointer, int alignment = 0);
    std::string emitLoad(const std::string& pointer, const std::string& type, int alignment = 0);
    std::string emitGetElementPtr(const std::string& pointer, const std::vector<std::string>& indices, const std::string& resultType);
    
    // ==================== 算术运算指令 ====================
    
    std::string emitAdd(const std::string& left, const std::string& right, const std::string& type);
    std::string emitSub(const std::string& left, const std::string& right, const std::string& type);
    std::string emitMul(const std::string& left, const std::string& right, const std::string& type);
    std::string emitDiv(const std::string& left, const std::string& right, const std::string& type);
    std::string emitRem(const std::string& left, const std::string& right, const std::string& type);
    
    // ==================== 比较和逻辑运算指令 ====================
    
    std::string emitIcmp(const std::string& condition, const std::string& left, const std::string& right, const std::string& type);
    std::string emitAnd(const std::string& left, const std::string& right, const std::string& type);
    std::string emitOr(const std::string& left, const std::string& right, const std::string& type);
    std::string emitXor(const std::string& left, const std::string& right, const std::string& type);
    
    // ==================== 位运算指令 ====================
    
    std::string emitShl(const std::string& left, const std::string& right, const std::string& type);
    std::string emitShr(const std::string& left, const std::string& right, const std::string& type);
    
    // ==================== 控制流指令 ====================
    
    void emitBr(const std::string& target);
    void emitCondBr(const std::string& condition, const std::string& trueTarget, const std::string& falseTarget);
    void emitRet(const std::string& value = "");
    void emitRetVoid();
    
    // ==================== 函数相关指令 ====================
    
    void emitFunctionDecl(const std::string& functionName, const std::string& returnType, const std::vector<std::string>& parameters);
    void emitFunctionDef(const std::string& functionName, const std::string& returnType, const std::vector<std::string>& parameters);
    void emitFunctionEnd();
    std::string emitCall(const std::string& functionName, const std::vector<std::string>& args, const std::string& returnType = "");
    
    // ==================== 内存操作指令 ====================
    
    std::string emitMemcpy(const std::string& dest, const std::string& src, const std::string& size);
    std::string emitMemset(const std::string& dest, const std::string& value, const std::string& size);
    
    // ==================== 类型转换指令 ====================
    
    std::string emitBitcast(const std::string& value, const std::string& fromType, const std::string& toType);
    std::string emitTrunc(const std::string& value, const std::string& fromType, const std::string& toType);
    std::string emitZExt(const std::string& value, const std::string& fromType, const std::string& toType);
    std::string emitSExt(const std::string& value, const std::string& fromType, const std::string& toType);
    
    // ==================== 目标和模块设置 ====================
    
    void emitTargetTriple(const std::string& triple = "riscv32-unknown-unknown-elf");
    void emitLabel(const std::string& label);
    
    // ==================== 错误处理 ====================
    
    bool hasError() const;
    std::vector<std::string> getErrorMessages() const;
    void reportError(const std::string& message);
    void clearErrors();

private:
    // ==================== 私有辅助方法 ====================
    
    // 缩进管理
    std::string getIndent();
    void emitWithIndent(const std::string& text);
    
    // 寄存器管理辅助方法
    std::string generateRegisterName(const std::string& variableName, const std::string& suffix = "");
    void addRegister(const std::string& name, const std::string& type, bool isPointer = false, bool isTemporary = false);
    void enterScope();
    void exitScope();
    
    // 基本块管理辅助方法
    std::string generateBasicBlockName(const std::string& prefix);
    
    // 类型映射辅助方法
    std::string mapSimpleTypeToLLVM(const std::string& typeName);
    int getAlignmentForType(const std::string& type);
    
    // 验证辅助方法
    bool validateRegister(const std::string& registerName);
    bool validateBasicBlock(const std::string& basicBlockName);
    bool validateType(const std::string& type);
};
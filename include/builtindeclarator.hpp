#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

// 前向声明
class IRBuilder;
class SemanticType;

// 参数属性结构
struct ParameterAttribute {
    std::string type;        // 参数类型
    std::string attribute;   // LLVM 属性 (如 "nocapture", "readonly", "signext" 等)
    
    ParameterAttribute(const std::string& t, const std::string& attr = "") 
        : type(t), attribute(attr) {}
};

// 内置函数信息结构
struct BuiltinFunctionInfo {
    std::string name;                                    // 函数名
    std::string returnType;                              // 返回类型
    std::vector<ParameterAttribute> parameters;         // 参数列表
    bool isVariadic;                                     // 是否可变参数
    bool isDeclared;                                     // 是否已声明
    std::string additionalAttributes;                    // 额外属性 (如 "noreturn")
    
    BuiltinFunctionInfo(const std::string& n, const std::string& ret, 
                        const std::vector<ParameterAttribute>& params = {},
                        bool variadic = false, const std::string& attrs = "")
        : name(n), returnType(ret), parameters(params), isVariadic(variadic), 
          isDeclared(false), additionalAttributes(attrs) {}
};

// BuiltinDeclarator 类 - 内置函数声明器
class BuiltinDeclarator {
private:
    // IRBuilder 共享指针
    std::shared_ptr<IRBuilder> irBuilder;
    
    // 内置函数信息映射
    std::unordered_map<std::string, std::shared_ptr<BuiltinFunctionInfo>> builtinFunctions;
    
    // 已声明函数集合
    std::unordered_set<std::string> declaredFunctions;
    
    // 错误状态
    bool hasErrors;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;

public:
    // 构造函数
    explicit BuiltinDeclarator(std::shared_ptr<IRBuilder> builder);
    
    // 析构函数
    ~BuiltinDeclarator() = default;
    
    // ==================== 主要声明接口 ====================
    
    // 批量声明所有标准内置函数
    void declareBuiltinFunctions();
    
    // 声明指定名称的单个内置函数
    bool declareBuiltinFunction(const std::string& name);
    
    // ==================== 具体函数声明方法 ====================
    
    // 输出函数声明
    void declarePrint();
    void declarePrintln();
    void declarePrintInt();
    void declarePrintlnInt();
    
    // 输入函数声明
    void declareGetString();
    void declareGetInt();
    
    // 内存管理函数声明
    void declareMalloc();
    void declareMemset();
    void declareMemcpy();
    
    // 系统函数声明
    void declareExit();
    
    // ==================== 结构体相关函数声明 ====================
    
    // 为指定结构体声明所有相关函数
    void declareStructFunctions(const std::string& structName, 
                                const std::vector<std::string>& fieldTypes);
    
    // 声明结构体构造函数
    void declareStructConstructor(const std::string& structName, 
                                  const std::vector<std::string>& fieldTypes);
    
    // 声明结构体析构函数
    void declareStructDestructor(const std::string& structName);
    
    // ==================== 内置函数查询接口 ====================
    
    // 检查指定函数是否为内置函数
    bool isBuiltinFunction(const std::string& name) const;
    
    // 获取内置函数的完整 LLVM 类型签名
    std::string getBuiltinFunctionType(const std::string& name) const;
    
    // 获取内置函数的参数类型列表
    std::vector<std::string> getBuiltinParameterTypes(const std::string& name) const;
    
    // 获取内置函数的返回类型
    std::string getBuiltinReturnType(const std::string& name) const;
    
    // ==================== 类型检查接口 ====================
    
    // 验证内置函数调用的参数类型是否匹配
    bool isValidBuiltinCall(const std::string& name, 
                           const std::vector<std::string>& argTypes) const;
    
    // 获取类型不匹配时的详细错误信息
    std::string getBuiltinCallError(const std::string& name, 
                                   const std::vector<std::string>& argTypes) const;
    
    // ==================== 自定义内置函数注册 ====================
    
    // 注册自定义内置函数
    void registerCustomBuiltin(const std::string& name, const std::string& returnType,
                               const std::vector<ParameterAttribute>& parameters,
                               bool isVariadic = false, const std::string& attributes = "");
    
    // ==================== 声明状态管理 ====================
    
    // 检查函数是否已声明
    bool isFunctionDeclared(const std::string& name) const;
    
    // 标记函数为已声明状态
    void markFunctionDeclared(const std::string& name);
    
    // 清理声明状态
    void clearDeclarationCache();
    
    // ==================== 错误处理 ====================
    
    // 检查是否有声明错误
    bool hasDeclarationErrors() const;
    
    // 获取所有错误消息
    std::vector<std::string> getErrorMessages() const;
    
    // 获取所有警告消息
    std::vector<std::string> getWarningMessages() const;
    
    // 清理错误状态
    void clearErrors();

private:
    // ==================== 内部辅助方法 ====================
    
    // 初始化标准内置函数
    void initializeBuiltinFunctions();
    
    // 注册所有标准内置函数
    void registerStandardBuiltins();
    
    // 通过 IRBuilder 输出函数声明
    void emitFunctionDeclaration(const std::string& name, const std::string& returnType,
                                const std::vector<std::string>& parameters,
                                const std::string& attributes = "");
    
    // 构建参数列表字符串
    std::string buildParameterList(const std::vector<ParameterAttribute>& parameters) const;
    
    // 构建完整的函数签名
    std::string buildFunctionSignature(const std::string& name, const std::string& returnType,
                                       const std::vector<ParameterAttribute>& parameters,
                                       const std::string& attributes = "") const;
    
    // 验证参数类型匹配
    bool validateParameterTypes(const std::vector<ParameterAttribute>& expected,
                               const std::vector<std::string>& actual) const;
    
    // 验证内置函数名称的有效性
    bool isValidBuiltinName(const std::string& name) const;
    
    // 检查类型兼容性
    bool isTypeCompatible(const std::string& expected, const std::string& actual) const;
    
    // 判断是否需要析构函数
    bool needsDestructor(const std::vector<std::string>& fieldTypes) const;
    
    // 错误报告方法
    void reportError(const std::string& message);
    void reportWarning(const std::string& message);
    
    // 获取函数信息
    std::shared_ptr<BuiltinFunctionInfo> getFunctionInfo(const std::string& name) const;
};
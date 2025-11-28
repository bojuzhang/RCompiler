#include "builtindeclarator.hpp"
#include "irbuilder.hpp"
#include <sstream>
#include <algorithm>

// ==================== 构造函数和基础结构 ====================

BuiltinDeclarator::BuiltinDeclarator(std::shared_ptr<IRBuilder> builder)
    : irBuilder(builder)
    , hasErrors(false)
{
    if (!irBuilder) {
        reportError("IRBuilder pointer is null");
        return;
    }
    
    // 初始化标准内置函数
    initializeBuiltinFunctions();
}

// ==================== 主要声明接口 ====================

void BuiltinDeclarator::declareBuiltinFunctions() {
    // 按功能分组批量声明，提高效率
    
    // 输出函数组
    declarePrint();
    declarePrintln();
    declarePrintInt();
    declarePrintlnInt();
    
    // 输入函数组
    declareGetString();
    declareGetInt();
    
    // 内存管理函数组
    declareMalloc();
    declareMemset();
    declareMemcpy();
    
    // 系统函数组
    declareExit();
}

bool BuiltinDeclarator::declareBuiltinFunction(const std::string& name) {
    if (!isValidBuiltinName(name)) {
        reportError("Invalid builtin function name: " + name);
        return false;
    }
    
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        reportError("Unknown builtin function: " + name);
        return false;
    }
    
    if (funcInfo->isDeclared) {
        reportWarning("Function already declared: " + name);
        return true;
    }
    
    // 构建参数列表
    std::vector<std::string> paramStrings;
    for (const auto& param : funcInfo->parameters) {
        std::string paramStr = param.type;
        if (!param.attribute.empty()) {
            paramStr += " " + param.attribute;
        }
        paramStrings.push_back(paramStr);
    }
    
    // 输出函数声明
    emitFunctionDeclaration(funcInfo->name, funcInfo->returnType, 
                           paramStrings, funcInfo->additionalAttributes);
    
    // 标记为已声明
    markFunctionDeclared(name);
    
    return true;
}

// ==================== 具体函数声明方法 ====================

void BuiltinDeclarator::declarePrint() {
    declareBuiltinFunction("print");
}

void BuiltinDeclarator::declarePrintln() {
    declareBuiltinFunction("println");
}

void BuiltinDeclarator::declarePrintInt() {
    declareBuiltinFunction("printInt");
}

void BuiltinDeclarator::declarePrintlnInt() {
    declareBuiltinFunction("printlnInt");
}

void BuiltinDeclarator::declareGetString() {
    declareBuiltinFunction("getString");
}

void BuiltinDeclarator::declareGetInt() {
    declareBuiltinFunction("getInt");
}

void BuiltinDeclarator::declareMalloc() {
    declareBuiltinFunction("malloc");
}

void BuiltinDeclarator::declareMemset() {
    declareBuiltinFunction("builtin_memset");
}

void BuiltinDeclarator::declareMemcpy() {
    declareBuiltinFunction("builtin_memcpy");
}

void BuiltinDeclarator::declareExit() {
    declareBuiltinFunction("exit");
}

// ==================== 结构体相关函数声明 ====================

void BuiltinDeclarator::declareStructFunctions(const std::string& structName, 
                                             const std::vector<std::string>& fieldTypes) {
    if (structName.empty()) {
        reportError("Struct name cannot be empty");
        return;
    }
    
    // 声明构造函数
    declareStructConstructor(structName, fieldTypes);
    
    // 根据字段类型判断是否需要析构函数
    if (needsDestructor(fieldTypes)) {
        declareStructDestructor(structName);
    }
    
    // 声明比较函数
    std::string eqFuncName = "struct_" + structName + "_eq";
    std::vector<ParameterAttribute> eqParams = {
        ParameterAttribute("%" + structName + "*", "nocapture readonly"),
        ParameterAttribute("%" + structName + "*", "nocapture readonly")
    };
    registerCustomBuiltin(eqFuncName, "i1", eqParams);
    declareBuiltinFunction(eqFuncName);
}

void BuiltinDeclarator::declareStructConstructor(const std::string& structName, 
                                                const std::vector<std::string>& fieldTypes) {
    std::string funcName = "struct_" + structName + "_new";
    
    std::vector<ParameterAttribute> params;
    for (const auto& fieldType : fieldTypes) {
        params.emplace_back(fieldType);
    }
    
    registerCustomBuiltin(funcName, "%" + structName, params);
    declareBuiltinFunction(funcName);
}

void BuiltinDeclarator::declareStructDestructor(const std::string& structName) {
    std::string funcName = "struct_" + structName + "_drop";
    
    std::vector<ParameterAttribute> params = {
        ParameterAttribute("%" + structName + "*")
    };
    
    registerCustomBuiltin(funcName, "void", params);
    declareBuiltinFunction(funcName);
}

// ==================== 内置函数查询接口 ====================

bool BuiltinDeclarator::isBuiltinFunction(const std::string& name) const {
    return builtinFunctions.find(name) != builtinFunctions.end();
}

std::string BuiltinDeclarator::getBuiltinFunctionType(const std::string& name) const {
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        return "";
    }
    
    return buildFunctionSignature(name, funcInfo->returnType, funcInfo->parameters, 
                                  funcInfo->additionalAttributes);
}

std::vector<std::string> BuiltinDeclarator::getBuiltinParameterTypes(const std::string& name) const {
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        return {};
    }
    
    std::vector<std::string> paramTypes;
    for (const auto& param : funcInfo->parameters) {
        paramTypes.push_back(param.type);
    }
    
    return paramTypes;
}

std::string BuiltinDeclarator::getBuiltinReturnType(const std::string& name) const {
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        return "";
    }
    
    return funcInfo->returnType;
}

// ==================== 类型检查接口 ====================

bool BuiltinDeclarator::isValidBuiltinCall(const std::string& name, 
                                          const std::vector<std::string>& argTypes) const {
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        return false;
    }
    
    // 检查参数数量（考虑可变参数）
    if (!funcInfo->isVariadic && argTypes.size() != funcInfo->parameters.size()) {
        return false;
    }
    
    if (funcInfo->isVariadic && argTypes.size() < funcInfo->parameters.size()) {
        return false;
    }
    
    // 验证参数类型
    return validateParameterTypes(funcInfo->parameters, argTypes);
}

std::string BuiltinDeclarator::getBuiltinCallError(const std::string& name, 
                                                  const std::vector<std::string>& argTypes) const {
    auto funcInfo = getFunctionInfo(name);
    if (!funcInfo) {
        return "Unknown builtin function: " + name;
    }
    
    // 检查参数数量
    if (!funcInfo->isVariadic && argTypes.size() != funcInfo->parameters.size()) {
        std::ostringstream oss;
        oss << "Parameter count mismatch for " << name 
            << ": expected " << funcInfo->parameters.size() 
            << ", got " << argTypes.size();
        return oss.str();
    }
    
    if (funcInfo->isVariadic && argTypes.size() < funcInfo->parameters.size()) {
        std::ostringstream oss;
        oss << "Insufficient parameters for variadic function " << name 
            << ": minimum " << funcInfo->parameters.size() 
            << ", got " << argTypes.size();
        return oss.str();
    }
    
    // 检查参数类型
    for (size_t i = 0; i < funcInfo->parameters.size(); ++i) {
        if (!isTypeCompatible(funcInfo->parameters[i].type, argTypes[i])) {
            std::ostringstream oss;
            oss << "Type mismatch at parameter " << (i + 1) << " for " << name 
                << ": expected " << funcInfo->parameters[i].type 
                << ", got " << argTypes[i];
            return oss.str();
        }
    }
    
    return "";
}

// ==================== 自定义内置函数注册 ====================

void BuiltinDeclarator::registerCustomBuiltin(const std::string& name, const std::string& returnType,
                                            const std::vector<ParameterAttribute>& parameters,
                                            bool isVariadic, const std::string& attributes) {
    if (!isValidBuiltinName(name)) {
        reportError("Invalid builtin function name: " + name);
        return;
    }
    
    auto funcInfo = std::make_shared<BuiltinFunctionInfo>(name, returnType, parameters, 
                                                         isVariadic, attributes);
    builtinFunctions[name] = funcInfo;
}

// ==================== 声明状态管理 ====================

bool BuiltinDeclarator::isFunctionDeclared(const std::string& name) const {
    return declaredFunctions.find(name) != declaredFunctions.end();
}

void BuiltinDeclarator::markFunctionDeclared(const std::string& name) {
    declaredFunctions.insert(name);
    
    auto funcInfo = getFunctionInfo(name);
    if (funcInfo) {
        funcInfo->isDeclared = true;
    }
}

void BuiltinDeclarator::clearDeclarationCache() {
    declaredFunctions.clear();
    
    // 重置所有函数的声明状态
    for (auto& pair : builtinFunctions) {
        pair.second->isDeclared = false;
    }
}

// ==================== 错误处理 ====================

bool BuiltinDeclarator::hasDeclarationErrors() const {
    return hasErrors;
}

std::vector<std::string> BuiltinDeclarator::getErrorMessages() const {
    return errorMessages;
}

std::vector<std::string> BuiltinDeclarator::getWarningMessages() const {
    return warningMessages;
}

void BuiltinDeclarator::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
    warningMessages.clear();
}

// ==================== 内部辅助方法 ====================

void BuiltinDeclarator::initializeBuiltinFunctions() {
    registerStandardBuiltins();
}

void BuiltinDeclarator::registerStandardBuiltins() {
    // 输出函数
    registerCustomBuiltin("print", "void", {ParameterAttribute("ptr", "nocapture readonly")});
    registerCustomBuiltin("println", "void", {ParameterAttribute("ptr", "nocapture readonly")});
    registerCustomBuiltin("printInt", "void", {ParameterAttribute("i32", "signext")});
    registerCustomBuiltin("printlnInt", "void", {ParameterAttribute("i32", "signext")});
    
    // 输入函数
    registerCustomBuiltin("getString", "ptr", {});
    registerCustomBuiltin("getInt", "i32", {});
    
    // 内存管理函数
    registerCustomBuiltin("malloc", "ptr", {ParameterAttribute("i32")});
    registerCustomBuiltin("builtin_memset", "ptr", {
        ParameterAttribute("ptr", "nocapture writeonly"),
        ParameterAttribute("i8"),
        ParameterAttribute("i32")
    });
    registerCustomBuiltin("builtin_memcpy", "ptr", {
        ParameterAttribute("ptr", "nocapture writeonly"),
        ParameterAttribute("ptr", "nocapture readonly"),
        ParameterAttribute("i32")
    });
    
    // 系统函数
    registerCustomBuiltin("exit", "void", {ParameterAttribute("i32")}, false, "noreturn");
}

void BuiltinDeclarator::emitFunctionDeclaration(const std::string& name, const std::string& returnType,
                                               const std::vector<std::string>& parameters,
                                               const std::string& attributes) {
    if (!irBuilder) {
        reportError("IRBuilder is null, cannot emit declaration");
        return;
    }
    
    // 构建声明字符串
    std::string declaration = "declare dso_local " + returnType + " @" + name + "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) declaration += ", ";
        declaration += parameters[i];
    }
    
    declaration += ")";
    
    // 添加额外属性
    if (!attributes.empty()) {
        declaration += " " + attributes;
    }
    
    // 通过 IRBuilder 输出
    irBuilder->emitInstruction(declaration);
}

std::string BuiltinDeclarator::buildParameterList(const std::vector<ParameterAttribute>& parameters) const {
    std::string result;
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) result += ", ";
        
        result += parameters[i].type;
        if (!parameters[i].attribute.empty()) {
            result += " " + parameters[i].attribute;
        }
    }
    
    return result;
}

std::string BuiltinDeclarator::buildFunctionSignature(const std::string& name, const std::string& returnType,
                                                     const std::vector<ParameterAttribute>& parameters,
                                                     const std::string& attributes) const {
    std::string signature = returnType + " @" + name + "(";
    signature += buildParameterList(parameters);
    signature += ")";
    
    if (!attributes.empty()) {
        signature += " " + attributes;
    }
    
    return signature;
}

bool BuiltinDeclarator::validateParameterTypes(const std::vector<ParameterAttribute>& expected,
                                              const std::vector<std::string>& actual) const {
    for (size_t i = 0; i < expected.size(); ++i) {
        if (i >= actual.size()) {
            return false;
        }
        
        if (!isTypeCompatible(expected[i].type, actual[i])) {
            return false;
        }
    }
    
    return true;
}

bool BuiltinDeclarator::isValidBuiltinName(const std::string& name) const {
    if (name.empty()) {
        return false;
    }
    
    // 检查名称格式：字母开头，包含字母、数字、下划线
    if (!std::isalpha(name[0])) {
        return false;
    }
    
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool BuiltinDeclarator::isTypeCompatible(const std::string& expected, const std::string& actual) const {
    // 完全匹配
    if (expected == actual) {
        return true;
    }
    
    // 指针兼容性：所有指针类型在 LLVM 中互相兼容
    bool expectedIsPtr = (expected.find('*') != std::string::npos) || (expected == "ptr");
    bool actualIsPtr = (actual.find('*') != std::string::npos) || (actual == "ptr");
    if (expectedIsPtr && actualIsPtr) {
        return true;
    }
    
    // 整数兼容性：所有整数类型支持隐式转换
    std::vector<std::string> intTypes = {"i1", "i8", "i16", "i32", "i64"};
    bool expectedIsInt = std::find(intTypes.begin(), intTypes.end(), expected) != intTypes.end();
    bool actualIsInt = std::find(intTypes.begin(), intTypes.end(), actual) != intTypes.end();
    if (expectedIsInt && actualIsInt) {
        return true;
    }
    
    return false;
}

bool BuiltinDeclarator::needsDestructor(const std::vector<std::string>& fieldTypes) const {
    // 检查字段类型是否包含指针或需要资源管理的类型
    for (const auto& fieldType : fieldTypes) {
        if (fieldType.find('*') != std::string::npos || fieldType == "ptr") {
            return true;
        }
        
        // 可以扩展检查其他需要资源管理的类型
        // 例如：包含字符串、数组等
    }
    
    return false;
}

void BuiltinDeclarator::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

void BuiltinDeclarator::reportWarning(const std::string& message) {
    warningMessages.push_back(message);
}

std::shared_ptr<BuiltinFunctionInfo> BuiltinDeclarator::getFunctionInfo(const std::string& name) const {
    auto it = builtinFunctions.find(name);
    return (it != builtinFunctions.end()) ? it->second : nullptr;
}
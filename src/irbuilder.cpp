#include "irbuilder.hpp"
#include "typecheck.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>

// 构造函数
IRBuilder::IRBuilder(std::ostream& output, std::shared_ptr<ScopeTree> scopeTree,
                     std::shared_ptr<TypeChecker> typeChecker)
    : outputStream(output)
    , scopeTree(scopeTree)
    , typeChecker(typeChecker)
    , typeMapper(std::make_shared<TypeMapper>(scopeTree, typeChecker))
    , registerCounter(0)
    , basicBlockCounter(0)
    , indentLevel(0)
    , hasErrors(false)
{
    // 初始化作用域栈
    if (scopeTree && scopeTree->GetCurrentScope()) {
        scopeStack.emplace_back(scopeTree->GetCurrentScope());
    }
}

// ==================== 寄存器管理接口 ====================

std::string IRBuilder::newRegister() {
    std::string regName = "%var_" + std::to_string(registerCounter++);
    addRegister(regName, "i32", false, true);
    return regName;
}

std::string IRBuilder::newRegister(const std::string& variableName) {
    return newRegister(variableName, "");
}

std::string IRBuilder::newRegister(const std::string& variableName, const std::string& suffix) {
    std::string regName = generateRegisterName(variableName, suffix);
    
    // 确定寄存器类型
    std::string llvmType = "i32"; // 默认类型
    if (!suffix.empty()) {
        if (suffix == "_ptr") {
            llvmType = "i32*";
        } else if (suffix == "_val") {
            llvmType = "i32";
        } else if (suffix == "_addr") {
            llvmType = "i32*";
        }
    }
    
    addRegister(regName, llvmType, suffix == "_ptr" || suffix == "_addr", false);
    return regName;
}

void IRBuilder::syncWithScopeTree() {
    if (scopeTree && scopeTree->GetCurrentScope()) {
        auto currentScope = scopeTree->GetCurrentScope();
        
        // 检查是否需要进入新的作用域
        if (scopeStack.empty() || scopeStack.back().scope != currentScope) {
            // 清理旧作用域的寄存器
            if (!scopeStack.empty()) {
                cleanupScopeRegisters();
            }
            
            // 进入新作用域
            scopeStack.emplace_back(currentScope);
        }
    }
}

std::shared_ptr<Scope> IRBuilder::getCurrentScope() {
    if (!scopeStack.empty()) {
        return scopeStack.back().scope;
    }
    return nullptr;
}

void IRBuilder::cleanupScopeRegisters() {
    if (!scopeStack.empty()) {
        auto& scopeInfo = scopeStack.back();
        
        // 清理该作用域的所有寄存器
        for (const auto& regName : scopeInfo.registers) {
            registers.erase(regName);
        }
        
        // 移除作用域
        scopeStack.pop_back();
    }
}

std::string IRBuilder::getVariableRegister(const std::string& variableName) {
    // 从当前作用域开始查找
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (it->variableCounters.find(variableName) != it->variableCounters.end()) {
            // 找到变量，返回对应的寄存器
            int counter = it->variableCounters[variableName];
            
            // 构建期望的变量寄存器名模式
            std::string expectedPattern;
            if (counter == 1) {
                expectedPattern = "%" + variableName + "_ptr";
            } else {
                expectedPattern = "%" + variableName + "_ptr_" + std::to_string(counter);
            }
            
            // 首先尝试精确匹配
            if (it->registers.find(expectedPattern) != it->registers.end()) {
                return expectedPattern;
            }
            
            // 如果精确匹配失败，尝试查找以变量名开头的寄存器
            std::string basePattern = "%" + variableName + "_ptr";
            std::string bestMatch;
            int bestCounter = -1;
            
            for (const auto& regName : it->registers) {
                if (regName.find(basePattern) == 0) {
                    // 检查它是否是指针类型
                    auto regIt = registers.find(regName);
                    if (regIt != registers.end() && regIt->second->llvmType.find('*') != std::string::npos) {
                        // 解析寄存器名中的计数器
                        int regCounter = extractCounterFromRegisterName(regName, variableName);
                        if (regCounter > bestCounter) {
                            bestCounter = regCounter;
                            bestMatch = regName;
                        }
                    }
                }
            }
            
            if (!bestMatch.empty()) {
                return bestMatch;
            }
        }
    }
    
    return "";
}

std::string IRBuilder::getSymbolRegister(std::shared_ptr<Symbol> symbol) {
    if (!symbol) {
        return "";
    }
    
    return getVariableRegister(symbol->name);
}

std::string IRBuilder::getRegisterType(const std::string& registerName) {
    auto it = registers.find(registerName);
    if (it != registers.end()) {
        return it->second->llvmType;
    }
    return "";
}

void IRBuilder::setRegisterType(const std::string& registerName, const std::string& type) {
    auto it = registers.find(registerName);
    if (it != registers.end()) {
        it->second->llvmType = type;
    }
}

std::shared_ptr<Scope> IRBuilder::getRegisterScope(const std::string& registerName) {
    auto it = registers.find(registerName);
    if (it != registers.end()) {
        return it->second->scope;
    }
    return nullptr;
}

bool IRBuilder::isVariableInCurrentScope(const std::string& variableName) {
    if (scopeStack.empty()) {
        return false;
    }
    
    const auto& currentScopeInfo = scopeStack.back();
    return currentScopeInfo.variableCounters.find(variableName) != currentScopeInfo.variableCounters.end();
}

void IRBuilder::registerVariableToCurrentScope(const std::string& variableName, const std::string& registerName, const std::string& type) {
    if (scopeStack.empty()) {
        return;
    }
    
    auto& currentScopeInfo = scopeStack.back();
    
    // 将变量名添加到作用域的变量计数器中（如果不存在）
    if (currentScopeInfo.variableCounters.find(variableName) == currentScopeInfo.variableCounters.end()) {
        currentScopeInfo.variableCounters[variableName] = 1;
    }
    
    // 将寄存器名添加到作用域的寄存器集合中
    currentScopeInfo.registers.insert(registerName);
    
    // 注册寄存器信息
    auto currentScope = getCurrentScope();
    auto regInfo = std::make_shared<RegisterInfo>(registerName, type, currentScope, type.find('*') != std::string::npos, false);
    registers[registerName] = regInfo;
}

// ==================== 基本块管理接口 ====================

std::string IRBuilder::newBasicBlock(const std::string& prefix) {
    return newBasicBlock(prefix, ++basicBlockCounter);
}

std::string IRBuilder::newBasicBlock(const std::string& prefix, int counter) {
    std::string blockName = generateBasicBlockName(prefix);
    
    auto blockInfo = std::make_shared<BasicBlockInfo>(blockName, prefix, counter);
    basicBlocks[blockName] = blockInfo;
    
    return blockName;
}

std::string IRBuilder::newNestedBasicBlock(const std::string& prefix) {
    std::string nestedPrefix = prefix;
    if (!currentBasicBlock.empty()) {
        nestedPrefix = currentBasicBlock + "." + prefix;
    }
    return newBasicBlock(nestedPrefix);
}

void IRBuilder::setCurrentBasicBlock(const std::string& basicBlockName) {
    if (validateBasicBlock(basicBlockName)) {
        currentBasicBlock = basicBlockName;
    } else {
        reportError("Invalid basic block: " + basicBlockName);
    }
}

std::string IRBuilder::getCurrentBasicBlock() {
    return currentBasicBlock;
}

void IRBuilder::finishCurrentBasicBlock() {
    if (!currentBasicBlock.empty()) {
        auto it = basicBlocks.find(currentBasicBlock);
        if (it != basicBlocks.end()) {
            it->second->isFinished = true;
        }
    }
}

void IRBuilder::enterControlFlowContext(const std::string& header, const std::string& body, const std::string& exit) {
    controlFlowStack.emplace_back(header, body, exit);
}

void IRBuilder::exitControlFlowContext() {
    if (!controlFlowStack.empty()) {
        controlFlowStack.pop_back();
    }
}

ControlFlowContext IRBuilder::getCurrentControlFlowContext() {
    if (controlFlowStack.empty()) {
        return ControlFlowContext();
    }
    return controlFlowStack.back();
}

bool IRBuilder::isBasicBlockExists(const std::string& basicBlockName) {
    return basicBlocks.find(basicBlockName) != basicBlocks.end();
}

// ==================== 基础指令生成接口 ====================

void IRBuilder::emitInstruction(const std::string& instruction) {
    emitWithIndent(instruction);
    outputStream << "\n";
}

void IRBuilder::emitComment(const std::string& comment) {
    emitWithIndent("; " + comment);
    outputStream << "\n";
}

// ==================== 目标和模块设置 ====================

void IRBuilder::emitTargetTriple(const std::string& triple) {
    outputStream << "target triple = \"" << triple << "\"\n\n";
}

void IRBuilder::emitLabel(const std::string& label) {
    outputStream << label << ":\n";
}

// ==================== 错误处理 ====================

bool IRBuilder::hasError() const {
    return hasErrors;
}

std::vector<std::string> IRBuilder::getErrorMessages() const {
    return errorMessages;
}

void IRBuilder::reportError(const std::string& message) {
    hasErrors = true;
    errorMessages.push_back(message);
}

void IRBuilder::clearErrors() {
    hasErrors = false;
    errorMessages.clear();
}

// ==================== 私有辅助方法 ====================

std::string IRBuilder::getIndent() {
    return std::string(indentLevel * INDENT_SIZE, ' ');
}

void IRBuilder::emitWithIndent(const std::string& text) {
    outputStream << getIndent() << text;
}

std::string IRBuilder::generateRegisterName(const std::string& variableName, const std::string& suffix) {
    std::string regName = "%" + variableName;
    
    if (!scopeStack.empty()) {
        auto& scopeInfo = scopeStack.back();
        
        // 更新变量计数器
        if (scopeInfo.variableCounters.find(variableName) == scopeInfo.variableCounters.end()) {
            scopeInfo.variableCounters[variableName] = 0;
        }
        
        int counter = ++scopeInfo.variableCounters[variableName];
        
        // 添加后缀
        if (!suffix.empty()) {
            regName += suffix;
        }
        
        // 如果有多个同名变量，添加计数器
        if (counter > 1) {
            regName += "_" + std::to_string(counter);
        }
        
        // 记录寄存器到当前作用域
        scopeInfo.registers.insert(regName);
    }
    
    return regName;
}

void IRBuilder::addRegister(const std::string& name, const std::string& type, bool isPointer, bool isTemporary) {
    auto currentScope = getCurrentScope();
    auto regInfo = std::make_shared<RegisterInfo>(name, type, currentScope, isPointer, isTemporary);
    registers[name] = regInfo;
}

std::string IRBuilder::generateBasicBlockName(const std::string& prefix) {
    if (prefix.empty()) {
        return "bb" + std::to_string(++basicBlockCounter);
    }
    // 对于特殊的基本块名称（如 entry），不添加计数器
    if (prefix == "entry") {
        return "entry";
    }
    return prefix + std::to_string(++basicBlockCounter);
}

bool IRBuilder::validateRegister(const std::string& registerName) {
    return registers.find(registerName) != registers.end();
}

bool IRBuilder::validateBasicBlock(const std::string& basicBlockName) {
    return basicBlocks.find(basicBlockName) != basicBlocks.end();
}

bool IRBuilder::validateType(const std::string& type) {
    // 简单的类型验证
    return !type.empty() && (type == "i1" || type == "i8" || type == "i32" ||
                             type == "void" || type.find('*') != std::string::npos ||
                             type.find('[') != std::string::npos || type.find('%') == 0);
}

// ==================== 类型映射方法 ====================

std::string IRBuilder::mapRxTypeToLLVM(std::shared_ptr<SemanticType> type) {
    if (!type) {
        return "i32"; // 默认类型
    }
    
    // 使用TypeMapper进行类型映射
    return typeMapper->mapSemanticTypeToLLVM(type);
}

std::string IRBuilder::getPointerElementType(const std::string& pointerType) {
    if (pointerType.length() > 1 && pointerType.back() == '*') {
        return pointerType.substr(0, pointerType.length() - 1);
    }
    return "";
}

std::string IRBuilder::getArrayElementType(const std::string& arrayType) {
    // 使用TypeMapper获取数组元素类型
    return typeMapper->getElementType(arrayType);
}

bool IRBuilder::isPointerType(const std::string& type) {
    // 使用TypeMapper检查指针类型
    return typeMapper->isPointerType(type);
}

bool IRBuilder::isIntegerType(const std::string& type) {
    // 使用TypeMapper检查整数类型
    return typeMapper->isIntegerType(type);
}

int IRBuilder::getTypeSize(const std::string& type) {
    // 使用TypeMapper获取类型大小
    return typeMapper->getTypeSize(type);
}

std::string IRBuilder::mapSimpleTypeToLLVM(const std::string& typeName) {
    // 基本类型映射（32位机器）
    if (typeName == "i32" || typeName == "SignedInt") return "i32";
    if (typeName == "i64" || typeName == "Int") return "i32";  // 32位机器上i64映射为i32
    if (typeName == "u32" || typeName == "UnsignedInt") return "i32";
    if (typeName == "u64") return "i32";  // 32位机器上u64映射为i32
    if (typeName == "bool") return "i1";
    if (typeName == "char") return "i8";
    if (typeName == "str") return "i8*";
    if (typeName == "unit" || typeName == "void") return "void";
    
    // 如果已经是LLVM类型格式，直接返回
    if (typeName.find('*') != std::string::npos || 
        typeName.find('[') != std::string::npos ||
        typeName.find('%') == 0) {
        return typeName;
    }
    
    // 默认返回i32
    return "i32";
}

int IRBuilder::getAlignmentForType(const std::string& type) {
    // 使用TypeMapper获取类型对齐
    return typeMapper->getTypeAlignment(type);
}

// ==================== 类型相关指令 ====================

std::string IRBuilder::emitAlloca(const std::string& type, int alignment) {
    std::string result = newRegister();
    std::string alignStr = "";
    
    if (alignment > 0) {
        alignStr = ", align " + std::to_string(alignment);
    } else {
        // 自动计算对齐
        int autoAlign = getAlignmentForType(type);
        if (autoAlign > 0) {
            alignStr = ", align " + std::to_string(autoAlign);
        }
    }
    
    std::string instruction = result + " = alloca " + type + alignStr;
    emitInstruction(instruction);
    
    // alloca 指令的结果是指向分配类型的指针
    setRegisterType(result, type + "*");
    return result;
}

void IRBuilder::emitStore(const std::string& value, const std::string& pointer, int alignment) {
    std::string valueType = getRegisterType(value);
    std::string pointerType = getRegisterType(pointer);
    
    // 对于函数参数，如果无法从寄存器映射中获取类型，尝试从函数签名推断
    if (valueType.empty() && value.find("param_") == 0) {
        // 检查当前是否在函数上下文中，并且这是self参数
        // 对于self参数，我们需要特殊处理类型推断
        // 这里我们检查指针类型，如果是指向指针的，说明这是self参数的存储
        if (!pointerType.empty() && pointerType.find("**") != std::string::npos) {
            valueType = pointerType.substr(0, pointerType.length() - 1); // 去掉一个 *
        } else if (!pointerType.empty()) {
            // 从指针类型推断值类型
            // 如果pointerType是"T*"，那么valueType应该是"T"
            if (pointerType.back() == '*') {
                valueType = pointerType.substr(0, pointerType.length() - 1);
            } else {
                valueType = "i32"; // 默认参数类型
            }
        } else {
            // 如果没有指针类型信息，尝试从参数名推断
            // 对于基本类型参数，我们需要确保类型匹配
            // 这里我们假设参数类型与指针类型匹配，但由于无法推断，使用默认类型
            valueType = "i32"; // 默认参数类型
        }
    }
    
    if (!validateRegister(value) && value.find("param_") != 0) {
        // 如果是函数参数但不在寄存器映射中，允许继续
        // 函数参数在函数签名中定义，不需要在寄存器映射中
    } else if (!validateRegister(value)) {
        reportError("Invalid register for store value: " + value);
        return;
    }
    
    if (!validateRegister(pointer)) {
        reportError("Invalid register for store pointer: " + pointer);
        return;
    }
    
    std::string alignStr = "";
    if (alignment > 0) {
        alignStr = ", align " + std::to_string(alignment);
    } else {
        // 自动计算对齐
        if (!valueType.empty()) {
            int autoAlign = getAlignmentForType(valueType);
            if (autoAlign > 0) {
                alignStr = ", align " + std::to_string(autoAlign);
            }
        }
    }
    
    // 确保valueType不为空
    if (valueType.empty()) {
        valueType = "i32"; // 默认类型
    }
    
    std::string instruction = "store " + valueType + " " + value + ", " + pointerType + " " + pointer + alignStr;
    emitInstruction(instruction);
}

std::string IRBuilder::emitLoad(const std::string& pointer, const std::string& type, int alignment) {
    std::string result = newRegister();
    std::string pointerType = getRegisterType(pointer);
    
    if (!validateRegister(pointer)) {
        reportError("Invalid register for load pointer: " + pointer);
        return result;
    }
    
    // 如果没有指定类型，尝试从指针类型推断
    std::string elementType = type;
    if (elementType.empty() && !pointerType.empty()) {
        elementType = getPointerElementType(pointerType);
    }
    
    if (elementType.empty()) {
        reportError("Cannot determine element type for load from: " + pointer);
        return result;
    }
    
    std::string alignStr = "";
    if (alignment > 0) {
        alignStr = ", align " + std::to_string(alignment);
    } else {
        int autoAlign = getAlignmentForType(elementType);
        if (autoAlign > 0) {
            alignStr = ", align " + std::to_string(autoAlign);
        }
    }
    
    std::string instruction = result + " = load " + elementType + ", " + pointerType + " " + pointer + alignStr;
    emitInstruction(instruction);
    
    setRegisterType(result, elementType);
    return result;
}

std::string IRBuilder::emitGetElementPtr(const std::string& pointer, const std::vector<std::string>& indices, const std::string& resultType) {
    std::string result = newRegister();
    std::string pointerType = getRegisterType(pointer);
    
    if (!validateRegister(pointer)) {
        reportError("Invalid register for getelementptr: " + pointer);
        return result;
    }
    
    if (indices.empty()) {
        reportError("getelementptr requires at least one index");
        return result;
    }
    
    // 构建索引列表
    std::string indicesStr;
    for (size_t i = 0; i < indices.size(); ++i) {
        if (i > 0) indicesStr += ", ";
        
        // 检查索引是否是寄存器
        if (indices[i].front() == '%') {
            if (validateRegister(indices[i])) {
                std::string indexType = getRegisterType(indices[i]);
                indicesStr += indexType + " " + indices[i];
            } else {
                reportError("Invalid index register: " + indices[i]);
                return result;
            }
        } else {
            // 直接是数字字面量
            indicesStr += "i32 " + indices[i];
        }
    }
    
    // 确定结果类型
    std::string resType = resultType;
    if (resType.empty() && !pointerType.empty()) {
        resType = pointerType; // 默认使用指针类型
    }
    
    std::string instruction = result + " = getelementptr " + resType + ", " + pointerType + " " + pointer + ", " + indicesStr;
    emitInstruction(instruction);
    
    // 对于数组索引，结果类型应该是指向元素类型的指针
    std::string elementType;
    if (resType.find('[') == 0 && resType.find(" x ") != std::string::npos) {
        // 是数组类型，提取元素类型
        size_t xPos = resType.find(" x ");
        if (xPos != std::string::npos) {
            elementType = resType.substr(xPos + 3, resType.length() - xPos - 4); // 去掉 [N x T] 中的 T
        }
    } else if (resType.back() == '*') {
        // 已经是指针类型，去掉 *
        elementType = resType.substr(0, resType.length() - 1);
    } else {
        // 其他情况，使用原类型
        elementType = resType;
    }
    
    setRegisterType(result, elementType + "*");
    return result;
}

// ==================== 算术运算指令 ====================

std::string IRBuilder::emitAdd(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for add operation");
        return result;
    }
    
    std::string instruction = result + " = add " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitSub(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for sub operation");
        return result;
    }
    
    std::string instruction = result + " = sub " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitMul(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for mul operation");
        return result;
    }
    
    std::string instruction = result + " = mul " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitDiv(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for div operation");
        return result;
    }
    
    std::string instruction = result + " = sdiv " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitRem(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for rem operation");
        return result;
    }
    
    std::string instruction = result + " = srem " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

// ==================== 比较和逻辑运算指令 ====================

std::string IRBuilder::emitIcmp(const std::string& condition, const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for icmp operation");
        return result;
    }
    
    std::string instruction = result + " = icmp " + condition + " " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, "i1");
    return result;
}

std::string IRBuilder::emitAnd(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for and operation");
        return result;
    }
    
    std::string instruction = result + " = and " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitOr(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for or operation");
        return result;
    }
    
    std::string instruction = result + " = or " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitXor(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for xor operation");
        return result;
    }
    
    std::string instruction = result + " = xor " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

// ==================== 位运算指令 ====================

std::string IRBuilder::emitShl(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for shl operation");
        return result;
    }
    
    std::string instruction = result + " = shl " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

std::string IRBuilder::emitShr(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = newRegister();
    
    if (!validateRegister(left) || !validateRegister(right)) {
        reportError("Invalid registers for shr operation");
        return result;
    }
    
    std::string instruction = result + " = ashr " + type + " " + left + ", " + right;
    emitInstruction(instruction);
    
    setRegisterType(result, type);
    return result;
}

// ==================== 控制流指令 ====================

void IRBuilder::emitBr(const std::string& target) {
    if (!validateBasicBlock(target)) {
        reportError("Invalid basic block for branch: " + target);
        return;
    }
    
    std::string instruction = "br label %" + target;
    emitInstruction(instruction);
    finishCurrentBasicBlock();
}

void IRBuilder::emitCondBr(const std::string& condition, const std::string& trueTarget, const std::string& falseTarget) {
    if (!validateRegister(condition)) {
        reportError("Invalid condition register for conditional branch: " + condition);
        return;
    }
    
    if (!validateBasicBlock(trueTarget)) {
        reportError("Invalid true target basic block: " + trueTarget);
        return;
    }
    
    if (!validateBasicBlock(falseTarget)) {
        reportError("Invalid false target basic block: " + falseTarget);
        return;
    }
    
    std::string instruction = "br i1 " + condition + ", label %" + trueTarget + ", label %" + falseTarget;
    emitInstruction(instruction);
    finishCurrentBasicBlock();
}

void IRBuilder::emitRet(const std::string& value) {
    if (value.empty()) {
        emitRetVoid();
        return;
    }
    
    if (!validateRegister(value)) {
        reportError("Invalid register for return value: " + value);
        return;
    }
    
    std::string valueType = getRegisterType(value);
    std::string instruction = "ret " + valueType + " " + value;
    emitInstruction(instruction);
    finishCurrentBasicBlock();
}

void IRBuilder::emitRetVoid() {
    std::string instruction = "ret void";
    emitInstruction(instruction);
    finishCurrentBasicBlock();
}

// ==================== 函数相关指令 ====================

void IRBuilder::emitFunctionDecl(const std::string& functionName, const std::string& returnType, const std::vector<std::string>& parameters) {
    std::string instruction = "declare dso_local " + returnType + " @" + functionName + "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) instruction += ", ";
        instruction += parameters[i];
    }
    
    instruction += ")";
    emitInstruction(instruction);
}

void IRBuilder::emitFunctionDef(const std::string& functionName, const std::string& returnType, const std::vector<std::string>& parameters) {
    std::string instruction = "define " + returnType + " @" + functionName + "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) instruction += ", ";
        instruction += parameters[i];
    }
    
    instruction += ") {";
    emitInstruction(instruction);
    
    // 增加缩进级别
    indentLevel++;
    
    // 创建入口基本块
    std::string entryBlock = newBasicBlock("entry");
    emitLabel(entryBlock);
    setCurrentBasicBlock(entryBlock);
}

void IRBuilder::emitFunctionEnd() {
    // 减少缩进级别
    indentLevel--;
    emitInstruction("}");
    
    // 清理函数相关的寄存器和基本块
    currentBasicBlock = "";
}

std::string IRBuilder::emitCall(const std::string& functionName, const std::vector<std::string>& args, const std::string& returnType) {
    std::string instruction;
    std::string result;
    
    // 如果有返回值，分配结果寄存器
    if (!returnType.empty() && returnType != "void") {
        result = newRegister();
        instruction = result + " = call " + returnType + " @" + functionName + "(";
    } else {
        instruction = "call void @" + functionName + "(";
    }
    
    // 构建参数列表
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) instruction += ", ";
        
        if (args[i].front() == '%') {
            if (validateRegister(args[i])) {
                std::string argType = getRegisterType(args[i]);
                instruction += argType + " " + args[i];
            } else {
                reportError("Invalid argument register: " + args[i]);
                return result;
            }
        } else {
            // 直接是字面量
            instruction += "i32 " + args[i]; // 默认i32类型
        }
    }
    
    instruction += ")";
    emitInstruction(instruction);
    
    if (!result.empty()) {
        setRegisterType(result, returnType);
    }
    
    return result;
}

// ==================== 内存操作指令 ====================

std::string IRBuilder::emitMemcpy(const std::string& dest, const std::string& src, const std::string& size) {
    std::string result = newRegister();
    
    // 对于函数参数，允许不在寄存器映射中的情况
    if (!validateRegister(dest)) {
        reportError("Invalid destination register for memcpy: " + dest);
        return result;
    }
    
    // src可能是函数参数（如%param_0），允许不在寄存器映射中
    if (!validateRegister(src) && src.find("param_") == std::string::npos) {
        reportError("Invalid source register for memcpy: " + src);
        return result;
    }
    
    if (!validateRegister(size)) {
        reportError("Invalid size register for memcpy: " + size);
        return result;
    }
    
    std::string instruction = result + " = call ptr @builtin_memcpy(ptr " + dest + ", ptr " + src + ", i32 " + size + ")";
    emitInstruction(instruction);
    
    setRegisterType(result, "ptr");
    return result;
}

std::string IRBuilder::emitMemset(const std::string& dest, const std::string& value, const std::string& size) {
    std::string result = newRegister();
    
    if (!validateRegister(dest) || !validateRegister(size)) {
        reportError("Invalid registers for memset");
        return result;
    }
    
    std::string instruction = result + " = call ptr @builtin_memset(ptr " + dest + ", i8 " + value + ", i32 " + size + ")";
    emitInstruction(instruction);
    
    setRegisterType(result, "ptr");
    return result;
}

// ==================== 聚合类型操作接口 ====================

bool IRBuilder::isAggregateType(const std::string& type) {
    // 使用 TypeMapper 检查是否为数组或结构体类型
    return typeMapper->isArrayType(type) || typeMapper->isStructType(type);
}

std::string IRBuilder::emitAggregateCopy(const std::string& dest, const std::string& src, const std::string& type) {
    if (!validateRegister(dest) || !validateRegister(src)) {
        reportError("Invalid registers for aggregate copy");
        return "";
    }
    
    if (!isAggregateType(type)) {
        reportError("Type is not aggregate: " + type);
        return "";
    }
    
    // 计算类型大小
    int typeSize = typeMapper->getTypeSize(type);
    if (typeSize <= 0) {
        reportError("Invalid type size for aggregate copy: " + std::to_string(typeSize));
        return "";
    }
    
    // 生成大小寄存器
    std::string sizeReg = newRegister();
    std::string sizeInstruction = sizeReg + " = add i32 0, " + std::to_string(typeSize);
    emitInstruction(sizeInstruction);
    setRegisterType(sizeReg, "i32");
    
    // 调用 builtin_memcpy
    return emitMemcpy(dest, src, sizeReg);
}

// ==================== 类型转换指令 ====================

std::string IRBuilder::emitBitcast(const std::string& value, const std::string& fromType, const std::string& toType) {
    std::string result = newRegister();
    
    if (!validateRegister(value)) {
        reportError("Invalid register for bitcast: " + value);
        return result;
    }
    
    std::string instruction = result + " = bitcast " + fromType + " " + value + " to " + toType;
    emitInstruction(instruction);
    
    setRegisterType(result, toType);
    return result;
}

std::string IRBuilder::emitTrunc(const std::string& value, const std::string& fromType, const std::string& toType) {
    std::string result = newRegister();
    
    if (!validateRegister(value)) {
        reportError("Invalid register for trunc: " + value);
        return result;
    }
    
    std::string instruction = result + " = trunc " + fromType + " " + value + " to " + toType;
    emitInstruction(instruction);
    
    setRegisterType(result, toType);
    return result;
}

std::string IRBuilder::emitZExt(const std::string& value, const std::string& fromType, const std::string& toType) {
    std::string result = newRegister();
    
    if (!validateRegister(value)) {
        reportError("Invalid register for zext: " + value);
        return result;
    }
    
    std::string instruction = result + " = zext " + fromType + " " + value + " to " + toType;
    emitInstruction(instruction);
    
    setRegisterType(result, toType);
    return result;
}

std::string IRBuilder::emitSExt(const std::string& value, const std::string& fromType, const std::string& toType) {
    std::string result = newRegister();
    
    if (!validateRegister(value)) {
        reportError("Invalid register for sext: " + value);
        return result;
    }
    
    std::string instruction = result + " = sext " + fromType + " " + value + " to " + toType;
    emitInstruction(instruction);
    
    setRegisterType(result, toType);
    return result;
}

int IRBuilder::extractCounterFromRegisterName(const std::string& regName, const std::string& variableName) {
    std::string basePattern = "%" + variableName + "_ptr";
    
    if (regName == basePattern) {
        return 1; // 第一个变量没有后缀
    }
    
    if (regName.find(basePattern + "_") == 0) {
        std::string counterStr = regName.substr(basePattern.length() + 1);
        try {
            return std::stoi(counterStr);
        } catch (const std::exception&) {
            return -1;
        }
    }
    
    return -1;
}
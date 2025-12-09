#include "irgenerator.hpp"
#include "expressiongenerator.hpp"
#include "statementgenerator.hpp"
#include "functioncodegen.hpp"
#include "builtindeclarator.hpp"
#include "typecheck.hpp"
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <vector>
#include <iostream>

// ==================== 构造函数和析构函数 ====================

IRGenerator::IRGenerator(std::shared_ptr<ScopeTree> scopeTree,
                        std::shared_ptr<TypeChecker> typeChecker,
                        std::ostream& outputStream)
    : scopeTree(scopeTree)
    , typeChecker(typeChecker)
    , outputStream()
    , outputFormat(OutputFormat::STANDARD)
    , generationHasErrors(false)
    , componentsInitialized(false)
{
    // 验证输入参数
    if (!scopeTree) {
        reportError("ScopeTree cannot be null");
        return;
    }
    
    // 初始化输出流
    this->outputStream.str("");
    
    // 初始化节点类型映射（如果有的话）
    if (typeChecker) {
        // 从 typeChecker 获取节点类型映射
        nodeTypeMap = typeChecker->getNodeTypeMap();
    }
    
    // 初始化组件
    if (!initializeComponents()) {
        reportError("Failed to initialize components");
        return;
    }
    
    // 设置组件依赖关系
    if (!setupComponentDependencies()) {
        reportError("Failed to setup component dependencies");
        return;
    }
    
    // 初始化模块
    if (!initializeModule()) {
        reportError("Failed to initialize module");
        return;
    }
    
    // 声明内置函数
    if (!declareBuiltinFunctions()) {
        reportError("Failed to declare builtin functions");
        return;
    }
}

IRGenerator::~IRGenerator() {
    // 清理内存池
    cleanupMemoryPool();
    
    // 清理组件
    expressionGenerator.reset();
    statementGenerator.reset();
    functionCodegen.reset();
    builtinDeclarator.reset();
    irBuilder.reset();
    typeMapper.reset();
}

// ==================== 主要生成接口 ====================

bool IRGenerator::generateIR(const std::vector<std::shared_ptr<Item>>& topLevelItems) {
    try {
        // 清理之前的错误状态
        clearErrors();
        
        // 保存当前输出（包含内置函数声明）
        std::string currentOutput = outputStream.str();
        
        // 重置输出流
        outputStream.str("");
        outputBuffer.clear();
        
        // 恢复内置函数声明
        outputStream << currentOutput;
        
        // 生成所有顶层项
        bool success = generateTopLevelItems(topLevelItems);
        
        // 验证生成结果
        if (success) {
            success = validateGeneration();
        }
        
        // 完成模块生成
        if (success) {
            finalizeModule();
        }
        
        // 刷新输出缓冲区
        flushOutputBuffer();
        
        return success && !generationHasErrors;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateIR: " + std::string(e.what()));
        return false;
    }
}

bool IRGenerator::generateTopLevelItem(std::shared_ptr<Item> item) {
    if (!item) {
        reportError("Top level item is null");
        return false;
    }
    
    try {
        // 根据 Item 的实际类型进行分发
        if (auto function = std::dynamic_pointer_cast<Function>(item->item)) {
            return generateFunction(function);
        }
        else if (auto structDef = std::dynamic_pointer_cast<StructStruct>(item->item)) {
            return generateStruct(structDef);
        }
        else if (auto constant = std::dynamic_pointer_cast<ConstantItem>(item->item)) {
            return generateConstant(constant);
        }
        else if (auto impl = std::dynamic_pointer_cast<InherentImpl>(item->item)) {
            return generateImpl(impl);
        }
        else {
            reportError("Unsupported top level item type");
            return false;
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in generateTopLevelItem: " + std::string(e.what()));
        return false;
    }
}

// ==================== 输出管理接口 ====================

std::string IRGenerator::getIROutput() const {
    return outputStream.str();
}

void IRGenerator::flushOutput() {
    // 刷新输出流到标准输出
    std::cout << outputStream.str() << std::flush;
    outputStream.str("");
}

void IRGenerator::setOutputFormat(OutputFormat format) {
    outputFormat = format;
}

IRGenerator::OutputFormat IRGenerator::getOutputFormat() const {
    return outputFormat;
}

// ==================== 组件访问接口 ====================

std::shared_ptr<IRBuilder> IRGenerator::getIRBuilder() const {
    return irBuilder;
}

std::shared_ptr<TypeMapper> IRGenerator::getTypeMapper() const {
    return typeMapper;
}

ExpressionGenerator* IRGenerator::getExpressionGenerator() const {
    return expressionGenerator.get();
}

StatementGenerator* IRGenerator::getStatementGenerator() const {
    return statementGenerator.get();
}

FunctionCodegen* IRGenerator::getFunctionCodegen() const {
    return functionCodegen.get();
}

BuiltinDeclarator* IRGenerator::getBuiltinDeclarator() const {
    return builtinDeclarator.get();
}

// ==================== 符号表访问接口 ====================

std::shared_ptr<Symbol> IRGenerator::getSymbol(const std::string& name) const {
    if (!scopeTree) {
        return nullptr;
    }
    
    return scopeTree->LookupSymbol(name);
}

std::string IRGenerator::getSymbolType(const std::string& name) const {
    auto symbol = getSymbol(name);
    if (!symbol || !symbol->type) {
        return "";
    }
    
    return symbol->type->tostring();
}

const std::unordered_map<ASTNode*, std::shared_ptr<SemanticType>>& IRGenerator::getNodeTypeMap() const {
    return nodeTypeMap;
}

// ==================== 错误处理接口 ====================

bool IRGenerator::hasGenerationErrors() const {
    return generationHasErrors;
}

std::vector<std::string> IRGenerator::getErrors() const {
    return errorMessages;
}

std::vector<std::string> IRGenerator::getWarnings() const {
    return warningMessages;
}

void IRGenerator::clearErrors() {
    generationHasErrors = false;
    errorMessages.clear();
    warningMessages.clear();
}

void IRGenerator::reportError(const std::string& message, std::shared_ptr<ASTNode> node) {
    generationHasErrors = true;
    
    std::string location = getNodeLocation(node);
    std::string formattedMessage = formatErrorMessage(message, location);
    
    errorMessages.push_back(formattedMessage);
    
    // 在调试模式下，也输出到标准错误
    if (isDebugEnabled()) {
        std::cerr << "Error: " << formattedMessage << std::endl;
    }
}

void IRGenerator::reportWarning(const std::string& message, std::shared_ptr<ASTNode> node) {
    std::string location = getNodeLocation(node);
    std::string formattedMessage = formatWarningMessage(message, location);
    
    warningMessages.push_back(formattedMessage);
    
    // 在调试模式下，也输出到标准错误
    if (isDebugEnabled()) {
        std::cerr << "Warning: " << formattedMessage << std::endl;
    }
}

// ==================== 内部初始化方法 ====================

bool IRGenerator::initializeComponents() {
    try {
        // 创建 IRBuilder
        irBuilder = std::make_shared<IRBuilder>(outputStream, scopeTree);
        if (!irBuilder) {
            reportError("Failed to create IRBuilder");
            return false;
        }
        
        // 创建 TypeMapper
        typeMapper = irBuilder->getTypeMapper();
        if (!typeMapper) {
            reportError("Failed to get TypeMapper from IRBuilder");
            return false;
        }
        
        // 创建 ExpressionGenerator
        expressionGenerator = std::make_unique<ExpressionGenerator>(
            irBuilder, typeMapper, scopeTree, nodeTypeMap);
        if (!expressionGenerator) {
            reportError("Failed to create ExpressionGenerator");
            return false;
        }
        
        // 创建 StatementGenerator
        statementGenerator = std::make_unique<StatementGenerator>(
            irBuilder, typeMapper, scopeTree);
        if (!statementGenerator) {
            reportError("Failed to create StatementGenerator");
            return false;
        }
        
        // 创建 FunctionCodegen
        functionCodegen = std::make_unique<FunctionCodegen>(
            irBuilder, typeMapper, scopeTree, nodeTypeMap);
        if (!functionCodegen) {
            reportError("Failed to create FunctionCodegen");
            return false;
        }
        
        // 创建 BuiltinDeclarator
        builtinDeclarator = std::make_unique<BuiltinDeclarator>(irBuilder);
        if (!builtinDeclarator) {
            reportError("Failed to create BuiltinDeclarator");
            return false;
        }
        
        componentsInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in initializeComponents: " + std::string(e.what()));
        return false;
    }
}

bool IRGenerator::setupComponentDependencies() {
    try {
        if (!componentsInitialized) {
            reportError("Components not initialized");
            return false;
        }
        
        // 设置 ExpressionGenerator 和 StatementGenerator 的相互引用
        expressionGenerator->setStatementGenerator(statementGenerator.get());
        statementGenerator->setExpressionGenerator(expressionGenerator.get());
        
        // 设置 FunctionCodegen 的依赖
        functionCodegen->setExpressionGenerator(expressionGenerator.get());
        functionCodegen->setStatementGenerator(statementGenerator.get());
        
        // 设置 StatementGenerator 的 FunctionCodegen 引用
        statementGenerator->setFunctionCodegen(functionCodegen.get());
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in setupComponentDependencies: " + std::string(e.what()));
        return false;
    }
}

bool IRGenerator::initializeModule() {
    try {
        if (!irBuilder) {
            reportError("IRBuilder not initialized");
            return false;
        }
        
        // 输出目标三元组
        irBuilder->emitTargetTriple("riscv32-unknown-unknown-elf");
        
        // 添加模块注释
        if (outputFormat == OutputFormat::DEBUG) {
            irBuilder->emitComment("Generated by Rx Compiler IR Generator");
            irBuilder->emitComment("Target: RISC-V 32-bit");
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in initializeModule: " + std::string(e.what()));
        return false;
    }
}

bool IRGenerator::declareBuiltinFunctions() {
    try {
        if (!builtinDeclarator) {
            reportError("BuiltinDeclarator not initialized");
            return false;
        }
        
        // 声明所有标准内置函数
        builtinDeclarator->declareBuiltinFunctions();
        
        // 检查是否有声明错误
        if (builtinDeclarator->hasDeclarationErrors()) {
            auto errors = builtinDeclarator->getErrorMessages();
            for (const auto& error : errors) {
                reportError("Builtin declaration error: " + error);
            }
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in declareBuiltinFunctions: " + std::string(e.what()));
        return false;
    }
}

// ==================== 生成方法 ====================

bool IRGenerator::generateTopLevelItems(const std::vector<std::shared_ptr<Item>>& topLevelItems) {
    bool success = true;
    
    for (const auto& item : topLevelItems) {
        if (!generateTopLevelItem(item)) {
            success = false;
            // 继续处理其他项，收集所有错误
        }
    }
    
    return success;
}

bool IRGenerator::generateFunction(std::shared_ptr<Function> function) {
    if (!function) {
        reportError("Function is null");
        return false;
    }
    
    try {
        if (!functionCodegen) {
            reportError("FunctionCodegen not initialized");
            return false;
        }
        
        // 更新生成上下文
        currentContext.currentFunction = function->identifier_name;
        currentContext.isInFunction = true;
        currentContext.isInGlobalScope = false;
        
        // 生成调试注释
        emitDebugComment("Generating function: " + function->identifier_name, function);
        
        // 委托给 FunctionCodegen 生成函数
        bool success = functionCodegen->generateFunction(function);
        
        // 恢复生成上下文
        currentContext.currentFunction.clear();
        currentContext.isInFunction = false;
        currentContext.isInGlobalScope = true;
        
        return success;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateFunction: " + std::string(e.what()), function);
        return false;
    }
}

bool IRGenerator::generateStruct(std::shared_ptr<StructStruct> structDef) {
    if (!structDef) {
        reportError("Struct definition is null");
        return false;
    }
    
    try {
        // 生成调试注释
        emitDebugComment("Generating struct: " + structDef->identifier, structDef);
        
        // 生成结构体类型定义
        std::string structName = structDef->identifier;
        std::string structType = "%struct_" + structName;
        
        // 收集字段信息
        std::vector<std::string> fieldTypes;
        if (structDef->structfields) {
            for (const auto& field : structDef->structfields->structfields) {
                if (field && field->type) {
                    std::string fieldType = typeMapper->mapSemanticTypeToLLVM(
                        nodeTypeMap[field->type.get()]);
                    if (!fieldType.empty()) {
                        fieldTypes.push_back(fieldType);
                    } else {
                        fieldTypes.push_back("i32"); // 默认类型
                    }
                }
            }
        }
        
        // 生成结构体类型定义
        std::string structDefStr = structType + " = type {";
        for (size_t i = 0; i < fieldTypes.size(); ++i) {
            if (i > 0) structDefStr += ", ";
            structDefStr += fieldTypes[i];
        }
        structDefStr += "}";
        
        // 输出结构体定义
        irBuilder->emitInstruction(structDefStr);
        
        if (outputFormat == OutputFormat::DEBUG) {
            irBuilder->emitComment("Struct definition: " + structDef->identifier);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateStruct: " + std::string(e.what()), structDef);
        return false;
    }
}

bool IRGenerator::generateConstant(std::shared_ptr<ConstantItem> constant) {
    if (!constant) {
        reportError("Constant definition is null");
        return false;
    }
    
    try {
        // 生成调试注释
        emitDebugComment("Generating constant: " + constant->identifier, constant);
        
        // TODO: 实现常量定义生成
        // 这里需要与 ExpressionGenerator 协作生成常量值
        
        if (outputFormat == OutputFormat::DEBUG) {
            irBuilder->emitComment("Constant definition: " + constant->identifier);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateConstant: " + std::string(e.what()), constant);
        return false;
    }
}

bool IRGenerator::generateImpl(std::shared_ptr<InherentImpl> impl) {
    if (!impl) {
        reportError("Impl block is null");
        return false;
    }
    
    try {
        // 生成调试注释
        emitDebugComment("Generating impl block", impl);
        
        // 获取 impl 的目标类型
        std::string targetType;
        if (impl->type) {
            // 将 Type 转换为字符串表示
            if (auto typePath = std::dynamic_pointer_cast<TypePath>(impl->type)) {
                if (typePath->simplepathsegement) {
                    targetType = typePath->simplepathsegement->identifier;
                }
            }
        }
        
        if (outputFormat == OutputFormat::DEBUG) {
            irBuilder->emitComment("Impl for type: " + targetType);
        }
        
        // 生成 impl 块中的所有关联项
        for (const auto& item : impl->associateditems) {
            if (item && item->consttantitem_or_function) {
                if (auto function = std::dynamic_pointer_cast<Function>(item->consttantitem_or_function)) {
                    // 为 impl 中的函数生成特殊的方法名
                    std::string originalName = function->identifier_name;
                    function->identifier_name = targetType + "_" + originalName;
                    
                    if (outputFormat == OutputFormat::DEBUG) {
                        irBuilder->emitComment("Generating method: " + originalName + " -> " + function->identifier_name);
                    }
                    
                    // 委托给 FunctionCodegen 生成函数
                    bool success = functionCodegen->generateFunction(function);
                    
                    // 恢复原始函数名（虽然在这个上下文中可能不需要）
                    function->identifier_name = originalName;
                    
                    if (!success) {
                        reportError("Failed to generate method: " + originalName);
                        return false;
                    }
                }
                else if (auto constant = std::dynamic_pointer_cast<ConstantItem>(item->consttantitem_or_function)) {
                    if (outputFormat == OutputFormat::DEBUG) {
                        irBuilder->emitComment("Associated constant: " + constant->identifier);
                    }
                    // TODO: 实现关联常量生成
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in generateImpl: " + std::string(e.what()), impl);
        return false;
    }
}

// ==================== 工具方法 ====================

bool IRGenerator::validateGeneration() {
    try {
        // 检查是否有未完成的函数
        if (!currentContext.currentFunction.empty()) {
            reportError("Unclosed function: " + currentContext.currentFunction);
            return false;
        }
        
        // 检查输出是否为空
        std::string output = outputStream.str();
        
        // 对于空 AST，只检查是否有基本的内置函数声明
        // 如果只有内置函数声明，这是正常的
        if (output.find("declare dso_local void @print") != std::string::npos) {
            return true; // 有内置函数声明，认为是有效的
        }
        
        if (output.empty()) {
            reportError("Generated IR is empty");
            return false;
        }
        
        // 验证 IRBuilder 状态
        if (irBuilder && irBuilder->hasError()) {
            auto errors = irBuilder->getErrorMessages();
            for (const auto& error : errors) {
                reportError("IRBuilder error: " + error);
            }
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        reportError("Exception in validateGeneration: " + std::string(e.what()));
        return false;
    }
}

void IRGenerator::finalizeModule() {
    try {
        // 生成模块结束注释
        if (outputFormat == OutputFormat::DEBUG) {
            irBuilder->emitComment("Module generation completed");
            
            // 生成统计信息
            std::string stats = generateStatistics();
            irBuilder->emitComment("Statistics: " + stats);
        }
        
        // 确保所有基本块都已正确关闭
        if (irBuilder) {
            // TODO: 检查是否有未完成的基本块
        }
    }
    catch (const std::exception& e) {
        reportError("Exception in finalizeModule: " + std::string(e.what()));
    }
}

std::string IRGenerator::getNodeLocation(std::shared_ptr<ASTNode> node) const {
    if (!node) {
        return "unknown location";
    }
    
    // TODO: 从 AST 节点获取位置信息
    // 暂时返回通用位置
    return "AST node";
}

std::string IRGenerator::formatErrorMessage(const std::string& message, const std::string& location) const {
    if (location.empty()) {
        return "Error: " + message;
    }
    return "Error at " + location + ": " + message;
}

std::string IRGenerator::formatWarningMessage(const std::string& message, const std::string& location) const {
    if (location.empty()) {
        return "Warning: " + message;
    }
    return "Warning at " + location + ": " + message;
}

// ==================== 内存管理 ====================

void* IRGenerator::allocateFromPool(size_t size) {
    // 简化的内存池实现
    // 在实际项目中，这里应该有更复杂的内存管理逻辑
    return operator new(size);
}

void IRGenerator::cleanupMemoryPool() {
    // 清理内存池
    memoryPool.pools.clear();
    memoryPool.usedBytes = 0;
}

// ==================== 输出优化 ====================

void IRGenerator::flushOutputBuffer() {
    // 检查是否需要优化输出
    std::string output = outputStream.str();
    if (outputFormat == OutputFormat::COMPACT) {
        output = optimizeOutput(output);
    }
    
    // 更新输出缓冲区
    outputBuffer = output;
}

bool IRGenerator::shouldFlushBuffer() const {
    return outputStream.str().length() >= OUTPUT_BUFFER_THRESHOLD;
}

std::string IRGenerator::optimizeOutput(const std::string& output) const {
    // 简单的输出优化：移除多余的空行
    std::string optimized = output;
    
    // 移除连续的空行
    size_t pos = 0;
    while ((pos = optimized.find("\n\n\n", pos)) != std::string::npos) {
        optimized.replace(pos, 3, "\n\n");
    }
    
    return optimized;
}

// ==================== 调试支持 ====================

void IRGenerator::emitDebugComment(const std::string& message, std::shared_ptr<ASTNode> node) {
    if (isDebugEnabled() && irBuilder) {
        std::string location = getNodeLocation(node);
        std::string comment = message;
        if (!location.empty()) {
            comment += " (" + location + ")";
        }
        irBuilder->emitComment(comment);
    }
}

bool IRGenerator::isDebugEnabled() const {
    return outputFormat == OutputFormat::DEBUG;
}

std::string IRGenerator::generateStatistics() const {
    std::ostringstream stats;
    
    stats << "Functions: " << "TODO"; // TODO: 统计函数数量
    stats << ", Errors: " << errorMessages.size();
    stats << ", Warnings: " << warningMessages.size();
    
    return stats.str();
}
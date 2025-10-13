#pragma once

#include "../include/visitor.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <memory>

class ASTGraphVisualizer : public ASTVisitor {
private:
    std::ostringstream dotOutput;
    int nodeIdCounter;
    std::vector<int> nodeStack;
    bool showNodeDetails;
    
    // 辅助方法
    std::string generateNodeId();
    std::string getNodeLabel(const std::string& nodeName, const std::string& details = "");
    void addNode(const std::string& nodeId, const std::string& label);
    void addEdge(const std::string& fromId, const std::string& toId);
    void pushNode(int nodeId);
    void popNode();
    int getCurrentNodeId();
    
public:
    ASTGraphVisualizer(bool showDetails = true);
    virtual ~ASTGraphVisualizer() = default;
    
    // 获取DOT输出
    std::string getDotOutput();
    void clear();
    
    // 保存DOT文件并渲染为图片
    bool saveDotFile(const std::string& filename);
    bool renderToPng(const std::string& dotFilename, const std::string& pngFilename);
    bool renderToSvg(const std::string& dotFilename, const std::string& svgFilename);
    bool visualizeToFile(const std::string& baseFilename, const std::string& format = "png");
    
    // Visitor接口实现 - Item节点
    void visit(Crate& node) override;
    void visit(Item& node) override;
    void visit(Function& node) override;
    void visit(ConstantItem& node) override;
    void visit(StructStruct& node) override;
    void visit(Enumeration& node) override;
    void visit(InherentImpl& node) override;
    
    // Visitor接口实现 - Statement节点
    void visit(Statement& node) override;
    void visit(LetStatement& node) override;
    void visit(ExpressionStatement& node) override;
    
    // Visitor接口实现 - Expression节点
    void visit(Expression& node) override;
    void visit(LiteralExpression& node) override;
    void visit(PathExpression& node) override;
    void visit(GroupedExpression& node) override;
    void visit(ArrayExpression& node) override;
    void visit(IndexExpression& node) override;
    void visit(TupleExpression& node) override;
    void visit(StructExpression& node) override;
    void visit(CallExpression& node) override;
    void visit(MethodCallExpression& node) override;
    void visit(FieldExpression& node) override;
    void visit(ContinueExpression& node) override;
    void visit(BreakExpression& node) override;
    void visit(ReturnExpression& node) override;
    void visit(UnderscoreExpression& node) override;
    void visit(BlockExpression& node) override;
    void visit(ConstBlockExpression& node) override;
    void visit(InfiniteLoopExpression& node) override;
    void visit(PredicateLoopExpression& node) override;
    void visit(IfExpression& node) override;
    void visit(MatchExpression& node) override;
    void visit(TypeCastExpression& node) override;
    void visit(AssignmentExpression& node) override;
    void visit(CompoundAssignmentExpression& node) override;
    void visit(UnaryExpression& node) override;
    void visit(BinaryExpression& node) override;
    
    // Visitor接口实现 - Pattern节点
    void visit(Pattern& node) override;
    void visit(LiteralPattern& node) override;
    void visit(IdentifierPattern& node) override;
    void visit(WildcardPattern& node) override;
    void visit(PathPattern& node) override;
    
    // Visitor接口实现 - Type节点
    void visit(Type& node) override;
    void visit(TypePath& node) override;
    void visit(ArrayType& node) override;
    void visit(SliceType& node) override;
    void visit(InferredType& node) override;
    void visit(ReferenceType& node) override;
    
    // Visitor接口实现 - Path节点
    void visit(SimplePath& node) override;
    void visit(SimplePathSegment& node) override;
    void visit(PathInExpression& node) override;
    
    // Visitor接口实现 - 其他节点
    void visit(FunctionParameters& node) override;
    void visit(FunctionParam& node) override;
    void visit(FunctionReturnType& node) override;
    void visit(StructFields& node) override;
    void visit(StructField& node) override;
    void visit(EnumVariants& node) override;
    void visit(EnumVariant& node) override;
    void visit(AssociatedItem& node) override;
    void visit(ArrayElements& node);
    void visit(TupleElements& node);
    void visit(StructExprFields& node);
    void visit(StructExprField& node);
    void visit(StructBase& node);
    void visit(CallParams& node);
    void visit(Conditions& node);
};

// AST图形化可视化工具类
class ASTGraphVisualizerTool {
private:
    std::unique_ptr<ASTGraphVisualizer> visualizer;
    std::string lastOutputFormat;
    std::string lastOutputFile;
    
public:
    ASTGraphVisualizerTool(bool showDetails = true);
    
    // 可视化单个文件
    bool visualizeFile(const std::string& filename, const std::string& outputPath = "", const std::string& format = "png");
    
    // 批量可视化目录中的文件
    std::vector<std::string> visualizeDirectory(const std::string& dirPath, const std::string& outputDir = "", const std::string& format = "png");
    
    // 获取DOT输出
    std::string getDotOutput(const std::string& filename);
    
    // 设置详细信息显示
    void setShowDetails(bool show);
    
    // 获取最后的输出信息
    std::string getLastOutputFormat() const;
    std::string getLastOutputFile() const;
};
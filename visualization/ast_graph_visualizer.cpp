#include "ast_graph_visualizer.hpp"
#include "../include/astnodes.hpp"
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

ASTGraphVisualizer::ASTGraphVisualizer(bool showDetails) 
    : nodeIdCounter(0), showNodeDetails(showDetails) {
    dotOutput << "digraph AST {\n";
    dotOutput << "    node [shape=box, style=filled, fillcolor=lightblue];\n";
    dotOutput << "    edge [arrowhead=vee];\n";
}

std::string ASTGraphVisualizer::generateNodeId() {
    return "node_" + std::to_string(nodeIdCounter++);
}

std::string ASTGraphVisualizer::getNodeLabel(const std::string& nodeName, const std::string& details) {
    std::string label = nodeName;
    if (showNodeDetails && !details.empty()) {
        label += "\\n" + details;
    }
    return label;
}

void ASTGraphVisualizer::addNode(const std::string& nodeId, const std::string& label) {
    dotOutput << "    " << nodeId << " [label=\"" << label << "\"];\n";
}

void ASTGraphVisualizer::addEdge(const std::string& fromId, const std::string& toId) {
    dotOutput << "    " << fromId << " -> " << toId << ";\n";
}

void ASTGraphVisualizer::pushNode(int nodeId) {
    nodeStack.push_back(nodeId);
}

void ASTGraphVisualizer::popNode() {
    if (!nodeStack.empty()) {
        nodeStack.pop_back();
    }
}

int ASTGraphVisualizer::getCurrentNodeId() {
    return nodeStack.empty() ? -1 : nodeStack.back();
}

std::string ASTGraphVisualizer::getDotOutput() {
    return dotOutput.str() + "}\n";
}

void ASTGraphVisualizer::clear() {
    dotOutput.str("");
    dotOutput.clear();
    nodeIdCounter = 0;
    nodeStack.clear();
    dotOutput << "digraph AST {\n";
    dotOutput << "    node [shape=box, style=filled, fillcolor=lightblue];\n";
    dotOutput << "    edge [arrowhead=vee];\n";
}

bool ASTGraphVisualizer::saveDotFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    file << getDotOutput();
    file.close();
    return true;
}

bool ASTGraphVisualizer::renderToPng(const std::string& dotFilename, const std::string& pngFilename) {
    std::string command = "dot -Tpng " + dotFilename + " -o " + pngFilename;
    int result = std::system(command.c_str());
    return result == 0;
}

bool ASTGraphVisualizer::renderToSvg(const std::string& dotFilename, const std::string& svgFilename) {
    std::string command = "dot -Tsvg " + dotFilename + " -o " + svgFilename;
    int result = std::system(command.c_str());
    return result == 0;
}

bool ASTGraphVisualizer::visualizeToFile(const std::string& baseFilename, const std::string& format) {
    std::string dotFilename = baseFilename + ".dot";
    
    if (!saveDotFile(dotFilename)) {
        return false;
    }
    
    if (format == "png") {
        return renderToPng(dotFilename, baseFilename + ".png");
    } else if (format == "svg") {
        return renderToSvg(dotFilename, baseFilename + ".svg");
    }
    
    return false;
}

// Item节点访问实现
void ASTGraphVisualizer::visit(Crate& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Crate", std::to_string(node.items.size()) + " items"));
    pushNode(std::stoi(nodeId.substr(5)));
    
    for (auto& item : node.items) {
        item->accept(*this);
    }
    
    popNode();
}

void ASTGraphVisualizer::visit(Item& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Item"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.item) {
        node.item->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(Function& node) {
    std::string details = "name: " + node.identifier_name;
    if (node.isconst) details += ", const";
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Function", details));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    
    if (node.functionparameters) {
        node.functionparameters->accept(*this);
    }
    if (node.functionreturntype) {
        node.functionreturntype->accept(*this);
    }
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    
    popNode();
}

void ASTGraphVisualizer::visit(ConstantItem& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ConstantItem", "name: " + node.identifier));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructStruct& node) {
    std::string details = "name: " + node.identifier;
    details += node.issemi ? " (unit)" : " (named fields)";
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructStruct", details));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.structfileds) {
        node.structfileds->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(Enumeration& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Enumeration", "name: " + node.identifier));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.enumvariants) {
        node.enumvariants->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(InherentImpl& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("InherentImpl"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    for (auto& item : node.associateditems) {
        item->accept(*this);
    }
    popNode();
}

// Statement节点访问实现
void ASTGraphVisualizer::visit(Statement& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Statement"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(LetStatement& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("LetStatement"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.patternnotopalt) {
        node.patternnotopalt->accept(*this);
    }
    if (node.type) {
        node.type->accept(*this);
    }
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ExpressionStatement& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ExpressionStatement", node.hassemi ? "with semicolon" : "without semicolon"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.astnode) {
        node.astnode->accept(*this);
    }
    popNode();
}

// Expression节点访问实现
void ASTGraphVisualizer::visit(Expression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Expression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(LiteralExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("LiteralExpression", node.literal + " (" + to_string(node.tokentype) + ")"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(PathExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("PathExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.simplepath) {
        node.simplepath->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(GroupedExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("GroupedExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ArrayExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ArrayExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.arrayelements) {
        node.arrayelements->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(IndexExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("IndexExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expressionout) {
        node.expressionout->accept(*this);
    }
    if (node.expressionin) {
        node.expressionin->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(TupleExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("TupleExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.tupleelements) {
        node.tupleelements->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.pathinexpression) {
        node.pathinexpression->accept(*this);
    }
    if (node.structinfo) {
        node.structinfo->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(CallExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("CallExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    if (node.callparams) {
        node.callparams->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(MethodCallExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("MethodCallExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(FieldExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("FieldExpression", "field: " + node.identifier));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ContinueExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ContinueExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(BreakExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("BreakExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ReturnExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ReturnExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(UnderscoreExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("UnderscoreExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(BlockExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("BlockExpression", std::to_string(node.statements.size()) + " statements"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    if (node.expressionwithoutblock) {
        node.expressionwithoutblock->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ConstBlockExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ConstBlockExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(InfiniteLoopExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("InfiniteLoopExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(PredicateLoopExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("PredicateLoopExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    if (node.blockexpression) {
        node.blockexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(IfExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("IfExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.conditions) {
        node.conditions->accept(*this);
    }
    if (node.ifblockexpression) {
        node.ifblockexpression->accept(*this);
    }
    if (node.elseexpression) {
        node.elseexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(MatchExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("MatchExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(TypeCastExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("TypeCastExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    if (node.typenobounds) {
        node.typenobounds->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(AssignmentExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("AssignmentExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(CompoundAssignmentExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("CompoundAssignmentExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(UnaryExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("UnaryExpression", to_string(node.unarytype)));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(BinaryExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("BinaryExpression", to_string(node.binarytype)));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.leftexpression) {
        node.leftexpression->accept(*this);
    }
    if (node.rightexpression) {
        node.rightexpression->accept(*this);
    }
    popNode();
}

// Pattern节点访问实现
void ASTGraphVisualizer::visit(Pattern& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Pattern"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(LiteralPattern& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("LiteralPattern", node.isnegative ? "negative" : "positive"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.literalexprerssion) {
        node.literalexprerssion->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(IdentifierPattern& node) {
    std::string details = "name: " + node.identifier;
    if (node.hasref) details += ", ref";
    if (node.hasmut) details += ", mut";
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("IdentifierPattern", details));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.patternnotopalt) {
        node.patternnotopalt->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(WildcardPattern& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("WildcardPattern"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(PathPattern& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("PathPattern"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.pathexpression) {
        node.pathexpression->accept(*this);
    }
    popNode();
}

// Type节点访问实现
void ASTGraphVisualizer::visit(Type& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Type"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(TypePath& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("TypePath"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.simplepathsegement) {
        node.simplepathsegement->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ArrayType& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ArrayType"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ReferenceType& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ReferenceType", node.ismut ? "mutable" : "immutable"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    popNode();
}

// Path节点访问实现
void ASTGraphVisualizer::visit(SimplePath& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("SimplePath", std::to_string(node.simplepathsegements.size()) + " segments"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& segment : node.simplepathsegements) {
        if (segment) {
            segment->accept(*this);
        }
    }
    popNode();
}

void ASTGraphVisualizer::visit(SimplePathSegment& node) {
    std::string details = node.identifier;
    if (node.isself) details = "self";
    if (node.isSelf) details = "Self";
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("SimplePathSegment", details));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(PathInExpression& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("PathInExpression"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

// 其他节点访问实现
void ASTGraphVisualizer::visit(FunctionParameters& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("FunctionParameters", std::to_string(node.functionparams.size()) + " params"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& param : node.functionparams) {
        param->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(FunctionParam& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("FunctionParam"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.patternnotopalt) {
        node.patternnotopalt->accept(*this);
    }
    if (node.type) {
        node.type->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(FunctionReturnType& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("FunctionReturnType"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructFields& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructFields", std::to_string(node.structfields.size()) + " fields"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& field : node.structfields) {
        field->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructField& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructField", "name: " + node.identifier));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.type) {
        node.type->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(EnumVariants& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("EnumVariants", std::to_string(node.enumvariants.size()) + " variants"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& variant : node.enumvariants) {
        variant->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(EnumVariant& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("EnumVariant", "name: " + node.identifier));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
}

void ASTGraphVisualizer::visit(AssociatedItem& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("AssociatedItem"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.consttantitem_or_function) {
        node.consttantitem_or_function->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(ArrayElements& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("ArrayElements", std::to_string(node.expressions.size()) + " elements" + (node.istwo ? " (syntax [expr; expr])" : "")));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& expr : node.expressions) {
        expr->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(TupleElements& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("TupleElements", std::to_string(node.expressions.size()) + " elements"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& expr : node.expressions) {
        expr->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructExprFields& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructExprFields", std::to_string(node.structexprfields.size()) + " fields"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& field : node.structexprfields) {
        field->accept(*this);
    }
    if (node.structbase) {
        node.structbase->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructExprField& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructExprField", "name: " + node.identifier + (node.tupleindex >= 0 ? ", index: " + std::to_string(node.tupleindex) : "")));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(StructBase& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("StructBase"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(CallParams& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("CallParams", std::to_string(node.expressions.size()) + " params"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    for (auto& expr : node.expressions) {
        expr->accept(*this);
    }
    popNode();
}

void ASTGraphVisualizer::visit(Conditions& node) {
    std::string nodeId = generateNodeId();
    addNode(nodeId, getNodeLabel("Conditions"));
    
    int parentId = getCurrentNodeId();
    if (parentId != -1) {
        addEdge("node_" + std::to_string(parentId), nodeId);
    }
    
    pushNode(std::stoi(nodeId.substr(5)));
    if (node.expression) {
        node.expression->accept(*this);
    }
    popNode();
}

// ASTGraphVisualizerTool实现
ASTGraphVisualizerTool::ASTGraphVisualizerTool(bool showDetails) 
    : visualizer(std::make_unique<ASTGraphVisualizer>(showDetails)) {
}

bool ASTGraphVisualizerTool::visualizeFile(const std::string& filename, const std::string& outputPath, const std::string& format) {
    try {
        // 读取文件内容
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << filename << std::endl;
            return false;
        }
        std::string sourceCode(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();
        
        // 创建词法分析器和语法分析器
        Lexer lexer;
        auto tokens = lexer.lexString(sourceCode);
        Parser parser(tokens);
        
        // 解析文件
        auto crate = parser.parseCrate();
        if (!crate) {
            std::cerr << "Error: Failed to parse file: " << filename << std::endl;
            return false;
        }
        
        // 清除之前的可视化结果
        visualizer->clear();
        
        // 生成AST可视化
        crate->accept(*visualizer);
        
        // 确定输出路径
        std::string outputPathFinal = outputPath;
        if (outputPathFinal.empty()) {
            // 使用输入文件名作为基础，改变扩展名
            std::filesystem::path inputPath(filename);
            outputPathFinal = inputPath.stem().string();
        }
        
        // 保存并渲染
        bool success = visualizer->visualizeToFile(outputPathFinal, format);
        if (success) {
            lastOutputFormat = format;
            lastOutputFile = outputPathFinal + "." + format;
            std::cout << "AST visualization saved to: " << lastOutputFile << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "Error visualizing file " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> ASTGraphVisualizerTool::visualizeDirectory(const std::string& dirPath, const std::string& outputDir, const std::string& format) {
    std::vector<std::string> processedFiles;
    
    try {
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
            std::cerr << "Error: Directory does not exist or is not a directory: " << dirPath << std::endl;
            return processedFiles;
        }
        
        std::string outputDirFinal = outputDir;
        if (outputDirFinal.empty()) {
            outputDirFinal = "ast_output";
        }
        
        // 创建输出目录
        std::filesystem::create_directories(outputDirFinal);
        
        // 遍历目录中的所有.rx文件
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".rx") {
                std::string inputFile = entry.path().string();
                std::filesystem::path inputPath(inputFile);
                std::string outputFile = outputDirFinal + "/" + inputPath.stem().string();
                
                if (visualizeFile(inputFile, outputFile, format)) {
                    processedFiles.push_back(inputFile);
                }
            }
        }
        
        std::cout << "Processed " << processedFiles.size() << " files in directory: " << dirPath << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing directory " << dirPath << ": " << e.what() << std::endl;
    }
    
    return processedFiles;
}

std::string ASTGraphVisualizerTool::getDotOutput(const std::string& filename) {
    try {
        // 读取文件内容
        std::ifstream file(filename);
        if (!file.is_open()) {
            return "";
        }
        std::string sourceCode(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();
        
        Lexer lexer;
        auto tokens = lexer.lexString(sourceCode);
        Parser parser(tokens);
        
        auto crate = parser.parseCrate();
        if (!crate) {
            return "";
        }
        
        visualizer->clear();
        crate->accept(*visualizer);
        
        return visualizer->getDotOutput();
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating DOT output for " << filename << ": " << e.what() << std::endl;
        return "";
    }
}

void ASTGraphVisualizerTool::setShowDetails(bool show) {
    visualizer = std::make_unique<ASTGraphVisualizer>(show);
}

std::string ASTGraphVisualizerTool::getLastOutputFormat() const {
    return lastOutputFormat;
}

std::string ASTGraphVisualizerTool::getLastOutputFile() const {
    return lastOutputFile;
}
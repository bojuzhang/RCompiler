#include "parser.hpp"
#include "semantic.hpp"
#include "lexer.hpp"
#include <iostream>
#include <string>
#include <utility>

int main() {
    Lexer lex;
    auto tokens = lex.lexString(lex.getString());
    Parser parser(tokens);
    auto crate = parser.parseCrate();
    if (crate == nullptr) {
        std::cout << "-1\n";
        return 0;
    }
    CompleteSemanticAnalyzer semantic(crate);
    bool success = semantic.analyze();
    std::cout << (success ? "0\n" : "-1\n");
}
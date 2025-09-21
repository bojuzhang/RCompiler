#include "lexer.hpp"
#include <iostream>

int main() {
    lexser lex;
    auto s = lex.getString();
    auto ve = lex.lexString(s);

    for (auto [token, s] : ve) {
        std::cout << to_string(token) << " " << s << "\n";
    }
}
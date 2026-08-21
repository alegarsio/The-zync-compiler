#ifndef TRANSPILER_HPP
#define TRANSPILER_HPP

#include "lexer.hpp"
#include <vector>
#include <string>

class Transpiler {
public:
    explicit Transpiler(const std::vector<Token>& tokens);
    std::string translate();

private:
    std::vector<Token> tokens;
    size_t index;
    bool needsIOStream;

    const Token& current() const;
    const Token& peekNext() const;
    void advance();
    bool match(TokenType type);
    std::string parseStatement();
};

#endif
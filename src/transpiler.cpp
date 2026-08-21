#include "../include/transpiler.hpp"
#include <sstream>

Transpiler::Transpiler(const std::vector<Token>& tokens) : tokens(tokens), index(0), needsIOStream(false) {}

const Token& Transpiler::current() const {
    if (index >= tokens.size()) return tokens.back();
    return tokens[index];
}

const Token& Transpiler::peekNext() const {
    if (index + 1 < tokens.size()) return tokens[index + 1];
    return tokens.back();
}

void Transpiler::advance() {
    if (index < tokens.size()) index++;
}

bool Transpiler::match(TokenType type) {
    if (current().type == type) {
        advance();
        return true;
    }
    return false;
}

std::string Transpiler::parseStatement() {
    std::ostringstream stmt;

    if (current().type == TokenType::PRINT || current().type == TokenType::PRINTLN) {
        bool isPrintln = (current().type == TokenType::PRINTLN);
        needsIOStream = true;
        advance();

        if (match(TokenType::LPAREN)) {
            std::string content;
            if (current().type == TokenType::STRING_LITERAL) {
                content = "\"" + current().value + "\"";
                advance();
            } else if (current().type == TokenType::NUMBER_LITERAL || current().type == TokenType::IDENTIFIER) {
                content = current().value;
                advance();
            }

            if (match(TokenType::RPAREN)) {
                stmt << "    std::cout << " << content;
                if (isPrintln) {
                    stmt << " << std::endl;\n";
                } else {
                    stmt << ";\n";
                }
            }
        }
    } else {
        advance();
    }

    return stmt.str();
}

std::string Transpiler::translate() {
    std::ostringstream body;

    while (current().type != TokenType::END_OF_FILE) {
        if (current().type == TokenType::FN) {
            advance();

            if (current().type == TokenType::IDENTIFIER) {
                std::string fnName = current().value;
                advance();

                if (match(TokenType::LPAREN) && match(TokenType::RPAREN)) {
                    if (fnName == "main") {
                        body << "int main()";
                    } else {
                        body << "void " << fnName << "()";
                    }

                    if (match(TokenType::LBRACE)) {
                        body << " {\n";
                        while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE) {
                            body << parseStatement();
                        }
                        if (match(TokenType::RBRACE)) {
                            if (fnName == "main") {
                                body << "    return 0;\n";
                            }
                            body << "}\n";
                        }
                    }
                }
            }
        } else {
            advance();
        }
    }

    std::ostringstream finalCode;
    if (needsIOStream) {
        finalCode << "#include <iostream>\n\n";
    }
    finalCode << body.str();

    return finalCode.str();
}
#pragma once

#include <string>
#include <vector>

enum class TokenType {
    END_OF_FILE,
    IDENTIFIER,
    NUMBER_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    PKG,
    MOD,
    PUB,
    ASYNC,
    AWAIT,
    IMPORT,
    FN,
    RETURN,
    RECORD,
    ENUM,
    TRAIT,
    IMPL,
    MATCH,
    VAR,
    VAL,
    IN_KW,
    COMPTIME,
    TEST,
    ASSERT,
    ASSERT_EQ,
    ASSERT_NE,
    PRINT,
    PRINTLN,
    IF,
    ELSE,
    FOR_KW,
    BREAK,
    CONTINUE,
    TRUE,
    FALSE,
    UNDERSCORE,
    AT_C,
    LOGICAL_AND,
    LOGICAL_OR,
    EQUALS,
    NOT_EQUALS,
    NOT,
    LESS,
    LESS_EQUALS,
    GREATER,
    GREATER_EQUALS,
    ARROW,
    FAT_ARROW,
    ASSIGN,
    MINUS,
    MINUS_MINUS,
    PLUS,
    PLUS_PLUS,
    STAR,
    SLASH,
    DOT,
    COMMA,
    COLON,
    DOUBLE_COLON,
    SEMICOLON,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    UNKNOWN,
    AT_CPP_HEADER,
    AT_C_HEADER
};

struct Token {
    TokenType type;
    std::string value;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string src;
    size_t pos;

    char peek() const;
    char get();
    void skipWhitespace();
};
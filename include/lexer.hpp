#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>

enum class TokenType {
    FN,
    RETURN,
    PKG,
    IMPORT,
    RECORD,
    TRAIT,
    FOR_KW,
    IMPL,
    MATCH,
    VAR,
    COMPTIME,
    ASSERT,
    ASSERT_EQ,
    ASSERT_NE,
    TEST,
    PRINT,
    PRINTLN,
    IF,
    ELSE,
    BREAK,
    CONTINUE,
    TRUE,
    FALSE,
    IDENTIFIER,
    UNDERSCORE,
    STRING_LITERAL,
    CHAR_LITERAL,
    NUMBER_LITERAL,
    ASSIGN,
    FAT_ARROW,
    ARROW,
    DOUBLE_COLON,
    DOT,
    EQUALS,
    NOT_EQUALS,
    LESS,
    LESS_EQUALS,
    GREATER,
    GREATER_EQUALS,
    LOGICAL_AND,
    LOGICAL_OR,
    NOT,
    PLUS_PLUS,
    MINUS_MINUS,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    COMMA,
    COLON,
    SEMICOLON,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    END_OF_FILE,
    UNKNOWN
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

#endif
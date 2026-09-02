#include "../include/lexer.hpp"
#include <cctype>

Lexer::Lexer(const std::string& source) : src(source), pos(0) {}

char Lexer::peek() const {
    if (pos >= src.size()) return '\0';
    return src[pos];
}

char Lexer::get() {
    if (pos >= src.size()) return '\0';
    return src[pos++];
}

void Lexer::skipWhitespace() {
    while (pos < src.size()) {
        char c = src[pos];
        if (std::isspace(static_cast<unsigned char>(c))) {
            pos++;
        } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            pos += 2;
            while (pos < src.size() && src[pos] != '\n') {
                pos++;
            }
        } else {
            break;
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < src.size()) {
        skipWhitespace();
        if (pos >= src.size()) break;

        char c = peek();

        if (c == '@') {
            if (pos + 1 < src.size() && src[pos + 1] == 'c' && (pos + 2 >= src.size() || !std::isalpha(static_cast<unsigned char>(src[pos + 2])))) {
                get();
                get();
                tokens.push_back({TokenType::AT_C, "@c"});
                continue;
            }
            if (pos + 10 <= src.size()) {
                std::string sub = src.substr(pos, 10);
                if (sub == "@CPPheader" || sub == "@CPPHeader") {
                    for (int i = 0; i < 10; ++i) get();
                    tokens.push_back({TokenType::AT_CPP_HEADER, "@CPPheader"});
                    continue;
                }
            }
            if (pos + 8 <= src.size()) {
                std::string sub = src.substr(pos, 8);
                if (sub == "@CHeader" || sub == "@Cheader") {
                    for (int i = 0; i < 8; ++i) get();
                    tokens.push_back({TokenType::AT_C_HEADER, "@CHeader"});
                    continue;
                }
            }
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string ident;
            while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
                ident += get();
            }

            if (ident == "fn") {
                tokens.push_back({TokenType::FN, ident});
            } else if (ident == "async") {
                tokens.push_back({TokenType::ASYNC, ident});
            } else if (ident == "await") {
                tokens.push_back({TokenType::AWAIT, ident});
            } else if (ident == "return") {
                tokens.push_back({TokenType::RETURN, ident});
            } else if (ident == "pkg") {
                tokens.push_back({TokenType::PKG, ident});
            } else if (ident == "mod") {
                tokens.push_back({TokenType::MOD, ident});
            } else if (ident == "pub") {
                tokens.push_back({TokenType::PUB, ident});
            } else if (ident == "import") {
                tokens.push_back({TokenType::IMPORT, ident});
            } else if (ident == "record") {
                tokens.push_back({TokenType::RECORD, ident});
            } else if (ident == "enum") {
                tokens.push_back({TokenType::ENUM, ident});
            } else if (ident == "trait") {
                tokens.push_back({TokenType::TRAIT, ident});
            } else if (ident == "impl") {
                tokens.push_back({TokenType::IMPL, ident});
            } else if (ident == "match") {
                tokens.push_back({TokenType::MATCH, ident});
            } else if (ident == "var") {
                tokens.push_back({TokenType::VAR, ident});
            } else if (ident == "val") {
                tokens.push_back({TokenType::VAL, ident});
            } else if (ident == "in") {
                tokens.push_back({TokenType::IN_KW, ident});
            } else if (ident == "cmpt" || ident == "comptime") {
                tokens.push_back({TokenType::COMPTIME, ident});
            } else if (ident == "test" || ident == "TEST") {
                tokens.push_back({TokenType::TEST, ident});
            } else if (ident == "assert") {
                tokens.push_back({TokenType::ASSERT, ident});
            } else if (ident == "assert_eq" || ident == "ASSERT_EQ") {
                tokens.push_back({TokenType::ASSERT_EQ, ident});
            } else if (ident == "assert_ne" || ident == "ASSERT_NE") {
                tokens.push_back({TokenType::ASSERT_NE, ident});
            } else if (ident == "print") {
                tokens.push_back({TokenType::PRINT, ident});
            } else if (ident == "println") {
                tokens.push_back({TokenType::PRINTLN, ident});
            } else if (ident == "if") {
                tokens.push_back({TokenType::IF, ident});
            } else if (ident == "else") {
                tokens.push_back({TokenType::ELSE, ident});
            } else if (ident == "for") {
                tokens.push_back({TokenType::FOR_KW, ident});
            } else if (ident == "break") {
                tokens.push_back({TokenType::BREAK, ident});
            } else if (ident == "continue") {
                tokens.push_back({TokenType::CONTINUE, ident});
            } else if (ident == "true") {
                tokens.push_back({TokenType::TRUE, ident});
            } else if (ident == "false") {
                tokens.push_back({TokenType::FALSE, ident});
            } else if (ident == "_") {
                tokens.push_back({TokenType::UNDERSCORE, ident});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, ident});
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string num;
            bool hasDot = false;
            while (std::isdigit(static_cast<unsigned char>(peek())) || (peek() == '.' && !hasDot && std::isdigit(static_cast<unsigned char>(pos + 1 < src.size() ? src[pos + 1] : ' ')))) {
                if (peek() == '.') {
                    hasDot = true;
                }
                num += get();
            }
            if (peek() == 'f' || peek() == 'F' || peek() == 'L') {
                num += get();
            }
            tokens.push_back({TokenType::NUMBER_LITERAL, num});
        } else if (c == '"') {
            get();
            std::string str;
            while (peek() != '\0') {
                if (peek() == '\\' && pos + 1 < src.size()) {
                    str += get();
                    str += get();
                } else if (peek() == '"') {
                    get();
                    break;
                } else {
                    str += get();
                }
            }
            tokens.push_back({TokenType::STRING_LITERAL, str});
        } else if (c == '\'') {
            get();
            std::string ch;
            while (peek() != '\'' && peek() != '\0') {
                if (peek() == '\\' && pos + 1 < src.size()) {
                    ch += get();
                }
                ch += get();
            }
            if (peek() == '\'') {
                get();
            }
            tokens.push_back({TokenType::CHAR_LITERAL, ch});
        } else if (c == '&') {
            get();
            if (peek() == '&') {
                get();
                tokens.push_back({TokenType::LOGICAL_AND, "&&"});
            } else {
                tokens.push_back({TokenType::UNKNOWN, "&"});
            }
        } else if (c == '|') {
            get();
            if (peek() == '|') {
                get();
                tokens.push_back({TokenType::LOGICAL_OR, "||"});
            } else {
                tokens.push_back({TokenType::UNKNOWN, "|"});
            }
        } else if (c == '=') {
            get();
            if (peek() == '=') {
                get();
                tokens.push_back({TokenType::EQUALS, "=="});
            } else if (peek() == '>') {
                get();
                tokens.push_back({TokenType::FAT_ARROW, "=>"});
            } else {
                tokens.push_back({TokenType::ASSIGN, "="});
            }
        } else if (c == '!') {
            get();
            if (peek() == '=') {
                get();
                tokens.push_back({TokenType::NOT_EQUALS, "!="});
            } else {
                tokens.push_back({TokenType::NOT, "!"});
            }
        } else if (c == '<') {
            get();
            if (peek() == '=') {
                get();
                tokens.push_back({TokenType::LESS_EQUALS, "<="});
            } else {
                tokens.push_back({TokenType::LESS, "<"});
            }
        } else if (c == '>') {
            get();
            if (peek() == '=') {
                get();
                tokens.push_back({TokenType::GREATER_EQUALS, ">="});
            } else {
                tokens.push_back({TokenType::GREATER, ">"});
            }
        } else if (c == '-') {
            get();
            if (peek() == '>') {
                get();
                tokens.push_back({TokenType::ARROW, "->"});
            } else if (peek() == '-') {
                get();
                tokens.push_back({TokenType::MINUS_MINUS, "--"});
            } else {
                tokens.push_back({TokenType::MINUS, "-"});
            }
        } else if (c == '+') {
            get();
            if (peek() == '+') {
                get();
                tokens.push_back({TokenType::PLUS_PLUS, "++"});
            } else {
                tokens.push_back({TokenType::PLUS, "+"});
            }
        } else if (c == '.') {
            tokens.push_back({TokenType::DOT, std::string(1, get())});
        } else if (c == '*') {
            tokens.push_back({TokenType::STAR, std::string(1, get())});
        } else if (c == '/') {
            tokens.push_back({TokenType::SLASH, std::string(1, get())});
        } else if (c == ',') {
            tokens.push_back({TokenType::COMMA, std::string(1, get())});
        } else if (c == ':') {
            get();
            if (peek() == ':') {
                get();
                tokens.push_back({TokenType::DOUBLE_COLON, "::"});
            } else {
                tokens.push_back({TokenType::COLON, ":"});
            }
        } else if (c == ';') {
            tokens.push_back({TokenType::SEMICOLON, std::string(1, get())});
        } else if (c == '(') {
            tokens.push_back({TokenType::LPAREN, std::string(1, get())});
        } else if (c == ')') {
            tokens.push_back({TokenType::RPAREN, std::string(1, get())});
        } else if (c == '{') {
            tokens.push_back({TokenType::LBRACE, std::string(1, get())});
        } else if (c == '}') {
            tokens.push_back({TokenType::RBRACE, std::string(1, get())});
        } else if (c == '[') {
            tokens.push_back({TokenType::LBRACKET, std::string(1, get())});
        } else if (c == ']') {
            tokens.push_back({TokenType::RBRACKET, std::string(1, get())});
        } else {
            tokens.push_back({TokenType::UNKNOWN, std::string(1, get())});
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, ""});
    return tokens;
}
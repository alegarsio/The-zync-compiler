#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>
#include <string>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ProgramNode> parseProgram();

private:
    std::vector<Token> tokens;
    size_t index;

    const Token& current() const;
    const Token& peekNext() const;
    void advance();
    bool match(TokenType type);

    std::string parseType();
    std::vector<Parameter> parseParameterList();

    std::vector<std::unique_ptr<ImportNode>> parseImport();
    std::unique_ptr<RecordNode> parseRecord();
    std::unique_ptr<TraitNode> parseTrait();
    std::unique_ptr<ImplNode> parseImpl();
    std::unique_ptr<PackageNode> parsePackage();
    std::unique_ptr<FunctionNode> parseFunction();
    std::unique_ptr<TestBlockNode> parseTestBlock();

    std::unique_ptr<StatementNode> parseStatement();
    std::vector<std::unique_ptr<StatementNode>> parseBlock();
    std::unique_ptr<StatementNode> parseIf();
    std::unique_ptr<StatementNode> parseMatch();
    std::unique_ptr<StatementNode> parseFor();
    std::unique_ptr<StatementNode> parseVariableDecl();
    std::unique_ptr<StatementNode> parseAssignment();
    std::unique_ptr<StatementNode> parseIncDec();
    std::unique_ptr<StatementNode> parsePrint();

    std::unique_ptr<ExpressionNode> parseExpression();
    std::unique_ptr<ExpressionNode> parseLogicalOr();
    std::unique_ptr<ExpressionNode> parseLogicalAnd();
    std::unique_ptr<ExpressionNode> parseEquality();
    std::unique_ptr<ExpressionNode> parseRelational();
    std::unique_ptr<ExpressionNode> parseAdditive();
    std::unique_ptr<ExpressionNode> parseMultiplicative();
    std::unique_ptr<ExpressionNode> parseUnary();
    std::unique_ptr<ExpressionNode> parsePostfix();
    std::unique_ptr<ExpressionNode> parsePrimary();
    std::unique_ptr<ExpressionNode> parseArrayLiteral();
    std::unique_ptr<ExpressionNode> parseMapLiteral();
};


#endif
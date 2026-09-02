#include "../include/parser.hpp"
#include <unordered_set>
#include <iostream>
#include <sstream>
#include <cstdlib>

static const std::unordered_set<std::string> knownCppStdHeaders = {
    "cmath", "cassert", "iostream", "vector", "map", "string", "algorithm",
    "chrono", "random", "thread", "mutex", "filesystem", "cstdio",
    "cstdlib", "cstring", "ctime", "memory", "utility", "sstream",
    "fstream", "functional", "numeric", "cctype", "tuple", "list",
    "deque", "set", "unordered_set", "unordered_map", "queue", "stack", "array", "future"};

Parser::Parser(const std::vector<Token> &tokens) : tokens(tokens), index(0) {}

const Token &Parser::current() const
{
    if (index >= tokens.size())
        return tokens.back();
    return tokens[index];
}

const Token &Parser::peekNext() const
{
    if (index + 1 < tokens.size())
        return tokens[index + 1];
    return tokens.back();
}

void Parser::advance()
{
    if (index < tokens.size())
        index++;
}

bool Parser::match(TokenType type)
{
    if (current().type == type)
    {
        advance();
        return true;
    }
    return false;
}

Visibility Parser::parseVisibility()
{
    if (match(TokenType::PUB))
    {
        return Visibility::PUBLIC;
    }
    return Visibility::PRIVATE;
}

std::string Parser::parseType()
{
    if (current().type == TokenType::VAR || current().type == TokenType::VAL)
    {
        advance();
        return "auto";
    }
    if (current().type == TokenType::NUMBER_LITERAL)
    {
        std::string num = current().value;
        advance();
        return num;
    }
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string base = current().value;
        advance();

        while (match(TokenType::DOUBLE_COLON))
        {
            if (current().type == TokenType::IDENTIFIER)
            {
                base += "::" + current().value;
                advance();
            }
        }

        if (match(TokenType::LESS))
        {
            std::string inner = parseType();
            while (match(TokenType::COMMA))
            {
                inner += ", " + parseType();
            }
            match(TokenType::GREATER);
            return base + "<" + inner + ">";
        }
        return base;
    }
    return "";
}

std::vector<Parameter> Parser::parseParameterList()
{
    std::vector<Parameter> params;
    if (match(TokenType::LPAREN))
    {
        if (current().type != TokenType::RPAREN)
        {
            while (current().type != TokenType::RPAREN && current().type != TokenType::END_OF_FILE)
            {
                if (current().type == TokenType::IDENTIFIER)
                {
                    std::string paramName = current().value;
                    advance();
                    std::string paramType = "";
                    if (match(TokenType::COLON))
                    {
                        paramType = parseType();
                    }
                    params.push_back({paramName, paramType});
                }
                if (!match(TokenType::COMMA))
                    break;
            }
        }
        match(TokenType::RPAREN);
    }
    return params;
}

std::vector<std::unique_ptr<ImportNode>> Parser::parseImport()
{
    std::vector<std::unique_ptr<ImportNode>> result;
    advance();

    auto parseSingleTarget = [&]() -> std::unique_ptr<ImportNode>
    {
        match(TokenType::PKG);

        if (match(TokenType::AT_CPP_HEADER)) {
            std::string path = "";
            if (current().type == TokenType::STRING_LITERAL || current().type == TokenType::IDENTIFIER) {
                path = current().value;
                advance();
            }

            if (path.length() >= 4 && (path.substr(path.length() - 4) == ".hpp" || path.substr(path.length() - 2) == ".h")) {
                return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, path);
            }

            if (path.rfind("wrapper/", 0) == 0) {
                std::string depName = path.substr(8);
                return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, "wrapper/" + depName + ".hpp");
            }

            if (path.rfind("native/", 0) == 0) {
                std::string nativeName = path.substr(7);
                return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, "native/" + nativeName + "/" + nativeName + ".hpp");
            }

            return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, path + ".hpp");
        }

        if (match(TokenType::AT_C_HEADER))
        {
            std::string path = "";
            if (current().type == TokenType::STRING_LITERAL || current().type == TokenType::IDENTIFIER)
            {
                path = current().value;
                advance();
            }

            if (path.length() < 2 || (path.substr(path.length() - 2) != ".h" && path.substr(path.length() - 4) != ".hpp"))
            {
                path += ".h";
            }

            return std::make_unique<ImportNode>(ImportKind::C_HEADER, path);
        }

        if (match(TokenType::AT_C))
        {
            if (current().type == TokenType::STRING_LITERAL)
            {
                std::string headerName = current().value;
                advance();
                return std::make_unique<ImportNode>(ImportKind::CPP_SYS_HEADER, headerName);
            }
            if (current().type == TokenType::IDENTIFIER)
            {
                std::string headerName = current().value;
                advance();
                if (match(TokenType::DOT))
                {
                    if (current().type == TokenType::IDENTIFIER)
                    {
                        headerName += "." + current().value;
                        advance();
                    }
                }
                return std::make_unique<ImportNode>(ImportKind::CPP_SYS_HEADER, headerName);
            }
        }

        if (match(TokenType::LESS))
        {
            std::string headerName = "";
            while (current().type != TokenType::GREATER && current().type != TokenType::END_OF_FILE)
            {
                headerName += current().value;
                advance();
            }
            match(TokenType::GREATER);
            return std::make_unique<ImportNode>(ImportKind::CPP_SYS_HEADER, headerName);
        }

        if (current().type == TokenType::STRING_LITERAL)
        {
            std::string path = current().value;
            advance();
            if (path.length() >= 4 && (path.substr(path.length() - 4) == ".hpp" || path.substr(path.length() - 2) == ".h"))
            {
                return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, path);
            }
            if (path.length() >= 3 && path.substr(path.length() - 3) == ".zy")
            {
                return std::make_unique<ImportNode>(ImportKind::ZYNC_FILE, path);
            }
            return std::make_unique<ImportNode>(ImportKind::ZYNC_FILE, path + ".zy");
        }

        if (current().type == TokenType::IDENTIFIER)
        {
            std::string path = current().value;
            advance();

            while (match(TokenType::SLASH))
            {
                if (current().type == TokenType::IDENTIFIER)
                {
                    path += "/" + current().value;
                    advance();
                }
            }

            if (path.find('/') != std::string::npos)
            {
                if (match(TokenType::DOT))
                {
                    if (current().type == TokenType::IDENTIFIER)
                    {
                        path += "." + current().value;
                        advance();
                    }
                }
                if (path.length() >= 4 && (path.substr(path.length() - 4) == ".hpp" || path.substr(path.length() - 2) == ".h"))
                {
                    return std::make_unique<ImportNode>(ImportKind::CPP_USER_HEADER, path);
                }
                if (path.length() < 3 || path.substr(path.length() - 3) != ".zy")
                {
                    path += ".zy";
                }
                return std::make_unique<ImportNode>(ImportKind::ZYNC_FILE, path);
            }

            while (match(TokenType::DOUBLE_COLON))
            {
                if (current().type == TokenType::IDENTIFIER)
                {
                    path += "::" + current().value;
                    advance();
                }
            }

            if (knownCppStdHeaders.find(path) != knownCppStdHeaders.end())
            {
                return std::make_unique<ImportNode>(ImportKind::CPP_SYS_HEADER, path);
            }

            return std::make_unique<ImportNode>(ImportKind::PACKAGE, path);
        }

        return nullptr;
    };

    if (match(TokenType::LPAREN))
    {
        while (current().type != TokenType::RPAREN && current().type != TokenType::END_OF_FILE)
        {
            auto node = parseSingleTarget();
            if (node)
            {
                result.push_back(std::move(node));
            }
            match(TokenType::COMMA);
        }
        match(TokenType::RPAREN);
        match(TokenType::SEMICOLON);
        return result;
    }

    auto singleNode = parseSingleTarget();
    if (singleNode)
    {
        result.push_back(std::move(singleNode));
    }
    match(TokenType::SEMICOLON);

    return result;
}

std::unique_ptr<RecordNode> Parser::parseRecord(Visibility vis)
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string recordName = current().value;
        advance();

        auto recordNode = std::make_unique<RecordNode>(recordName, vis);
        if (match(TokenType::LBRACE))
        {
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                Visibility fieldVis = parseVisibility();
                if (current().type == TokenType::IDENTIFIER)
                {
                    std::string fieldName = current().value;
                    advance();
                    match(TokenType::COLON);
                    std::string fieldType = parseType();
                    recordNode->fields.push_back({fieldName, fieldType, fieldVis});
                    match(TokenType::COMMA);
                    match(TokenType::SEMICOLON);
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
        }
        return recordNode;
    }
    return nullptr;
}

std::unique_ptr<EnumNode> Parser::parseEnum(Visibility vis)
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string enumName = current().value;
        advance();

        std::string underlyingType = "";
        if (match(TokenType::COLON))
        {
            underlyingType = parseType();
        }

        auto enumNode = std::make_unique<EnumNode>(enumName, underlyingType, vis);
        if (match(TokenType::LBRACE))
        {
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                if (current().type == TokenType::IDENTIFIER)
                {
                    std::string variantName = current().value;
                    advance();
                    std::string variantVal = "";

                    if (match(TokenType::ASSIGN))
                    {
                        if (current().type == TokenType::NUMBER_LITERAL || current().type == TokenType::STRING_LITERAL || current().type == TokenType::CHAR_LITERAL || current().type == TokenType::IDENTIFIER)
                        {
                            variantVal = current().value;
                            advance();
                        }
                    }

                    enumNode->variants.push_back({variantName, variantVal});
                    match(TokenType::COMMA);
                    match(TokenType::SEMICOLON);
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
        }
        return enumNode;
    }
    return nullptr;
}

std::unique_ptr<ModNode> Parser::parseMod(Visibility vis)
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string modName = current().value;
        advance();

        while (match(TokenType::DOUBLE_COLON))
        {
            if (current().type == TokenType::IDENTIFIER)
            {
                modName += "::" + current().value;
                advance();
            }
        }

        if (match(TokenType::LBRACE))
        {
            auto modNode = std::make_unique<ModNode>(modName, vis);
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                Visibility memberVis = parseVisibility();

                if (current().type == TokenType::MOD)
                {
                    auto subMod = parseMod(memberVis);
                    if (subMod)
                        modNode->members.push_back(std::move(subMod));
                }
                else if (current().type == TokenType::RECORD)
                {
                    auto rec = parseRecord(memberVis);
                    if (rec)
                        modNode->members.push_back(std::move(rec));
                }
                else if (current().type == TokenType::ENUM)
                {
                    auto en = parseEnum(memberVis);
                    if (en)
                        modNode->members.push_back(std::move(en));
                }
                else if (current().type == TokenType::TRAIT)
                {
                    auto tr = parseTrait(memberVis);
                    if (tr)
                        modNode->members.push_back(std::move(tr));
                }
                else if (current().type == TokenType::IMPL)
                {
                    auto im = parseImpl();
                    if (im)
                        modNode->members.push_back(std::move(im));
                }
                else if (current().type == TokenType::TEST)
                {
                    auto test = parseTestBlock();
                    if (test)
                        modNode->members.push_back(std::move(test));
                }
                else if (current().type == TokenType::FN || current().type == TokenType::COMPTIME || current().type == TokenType::ASYNC)
                {
                    if (current().type == TokenType::COMPTIME && peekNext().type != TokenType::FN && peekNext().type != TokenType::ASYNC)
                    {
                        auto v = parseVariableDecl(memberVis);
                        if (v)
                            modNode->members.push_back(std::move(v));
                    }
                    else
                    {
                        auto fn = parseFunction(memberVis);
                        if (fn)
                            modNode->members.push_back(std::move(fn));
                    }
                }
                else if (current().type == TokenType::VAR || current().type == TokenType::VAL || current().type == TokenType::IDENTIFIER)
                {
                    auto v = parseVariableDecl(memberVis);
                    if (v)
                        modNode->members.push_back(std::move(v));
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
            return modNode;
        }
    }
    return nullptr;
}

std::unique_ptr<TraitNode> Parser::parseTrait(Visibility vis)
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string traitName = current().value;
        advance();

        auto traitNode = std::make_unique<TraitNode>(traitName, vis);
        if (match(TokenType::LBRACE))
        {
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                match(TokenType::COMPTIME);
                if (match(TokenType::FN))
                {
                    if (current().type == TokenType::IDENTIFIER)
                    {
                        std::string fnName = current().value;
                        advance();

                        std::vector<Parameter> params = parseParameterList();
                        std::string retType = "void";
                        if (match(TokenType::ARROW))
                        {
                            retType = parseType();
                        }
                        match(TokenType::SEMICOLON);

                        traitNode->methods.push_back({fnName, std::move(params), retType});
                    }
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
        }
        return traitNode;
    }
    return nullptr;
}

std::unique_ptr<ImplNode> Parser::parseImpl()
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string firstIdent = current().value;
        advance();

        while (match(TokenType::DOUBLE_COLON))
        {
            if (current().type == TokenType::IDENTIFIER)
            {
                firstIdent += "::" + current().value;
                advance();
            }
        }

        std::string traitName = "";
        std::string targetName = firstIdent;

        if (current().type == TokenType::FOR_KW)
        {
            advance();
            if (current().type == TokenType::IDENTIFIER)
            {
                traitName = firstIdent;
                targetName = current().value;
                advance();
                while (match(TokenType::DOUBLE_COLON))
                {
                    if (current().type == TokenType::IDENTIFIER)
                    {
                        targetName += "::" + current().value;
                        advance();
                    }
                }
            }
        }

        auto implNode = std::make_unique<ImplNode>(traitName, targetName);
        if (match(TokenType::LBRACE))
        {
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                Visibility fnVis = parseVisibility();
                if (current().type == TokenType::FN || current().type == TokenType::COMPTIME || current().type == TokenType::ASYNC)
                {
                    auto fn = parseFunction(fnVis);
                    if (fn)
                        implNode->methods.push_back(std::move(fn));
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
        }
        return implNode;
    }
    return nullptr;
}

std::unique_ptr<PackageNode> Parser::parsePackage()
{
    advance();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string pkgName = current().value;
        advance();

        while (match(TokenType::DOUBLE_COLON))
        {
            if (current().type == TokenType::IDENTIFIER)
            {
                pkgName += "::" + current().value;
                advance();
            }
        }

        if (match(TokenType::LBRACE))
        {
            auto pkgNode = std::make_unique<PackageNode>(pkgName);
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                Visibility memberVis = parseVisibility();

                if (current().type == TokenType::PKG)
                {
                    auto subPkg = parsePackage();
                    if (subPkg)
                        pkgNode->members.push_back(std::move(subPkg));
                }
                else if (current().type == TokenType::MOD)
                {
                    auto subMod = parseMod(memberVis);
                    if (subMod)
                        pkgNode->members.push_back(std::move(subMod));
                }
                else if (current().type == TokenType::RECORD)
                {
                    auto rec = parseRecord(memberVis);
                    if (rec)
                        pkgNode->members.push_back(std::move(rec));
                }
                else if (current().type == TokenType::ENUM)
                {
                    auto en = parseEnum(memberVis);
                    if (en)
                        pkgNode->members.push_back(std::move(en));
                }
                else if (current().type == TokenType::TRAIT)
                {
                    auto tr = parseTrait(memberVis);
                    if (tr)
                        pkgNode->members.push_back(std::move(tr));
                }
                else if (current().type == TokenType::IMPL)
                {
                    auto im = parseImpl();
                    if (im)
                        pkgNode->members.push_back(std::move(im));
                }
                else if (current().type == TokenType::TEST)
                {
                    auto test = parseTestBlock();
                    if (test)
                        pkgNode->members.push_back(std::move(test));
                }
                else if (current().type == TokenType::FN || current().type == TokenType::COMPTIME || current().type == TokenType::ASYNC)
                {
                    if (current().type == TokenType::COMPTIME && peekNext().type != TokenType::FN && peekNext().type != TokenType::ASYNC)
                    {
                        auto v = parseVariableDecl(memberVis);
                        if (v)
                            pkgNode->members.push_back(std::move(v));
                    }
                    else
                    {
                        auto fn = parseFunction(memberVis);
                        if (fn)
                            pkgNode->members.push_back(std::move(fn));
                    }
                }
                else if (current().type == TokenType::VAR || current().type == TokenType::VAL || current().type == TokenType::IDENTIFIER)
                {
                    auto v = parseVariableDecl(memberVis);
                    if (v)
                        pkgNode->members.push_back(std::move(v));
                }
                else
                {
                    advance();
                }
            }
            match(TokenType::RBRACE);
            return pkgNode;
        }
    }
    return nullptr;
}

std::unique_ptr<TestBlockNode> Parser::parseTestBlock()
{
    advance();

    std::string testName = "unnamed_test";
    if (match(TokenType::LPAREN))
    {
        if (current().type == TokenType::IDENTIFIER || current().type == TokenType::STRING_LITERAL)
        {
            testName = current().value;
            advance();
        }
        match(TokenType::RPAREN);
    }
    else if (current().type == TokenType::IDENTIFIER || current().type == TokenType::STRING_LITERAL)
    {
        testName = current().value;
        advance();
    }

    auto testNode = std::make_unique<TestBlockNode>(testName);
    testNode->body = parseBlock();
    return testNode;
}

std::unique_ptr<ExpressionNode> Parser::parseArrayLiteral()
{
    advance();
    auto arrayLit = std::make_unique<ArrayLiteralNode>();

    if (current().type != TokenType::RBRACKET)
    {
        arrayLit->elements.push_back(parseExpression());
        while (match(TokenType::COMMA))
        {
            if (current().type == TokenType::RBRACKET)
                break;
            arrayLit->elements.push_back(parseExpression());
        }
    }

    match(TokenType::RBRACKET);
    return arrayLit;
}

std::unique_ptr<ExpressionNode> Parser::parseMapLiteral()
{
    advance();
    auto mapLit = std::make_unique<MapLiteralNode>();

    if (current().type != TokenType::RBRACE)
    {
        auto key = parseExpression();
        match(TokenType::COLON);
        auto val = parseExpression();
        mapLit->entries.push_back({std::move(key), std::move(val)});

        while (match(TokenType::COMMA))
        {
            if (current().type == TokenType::RBRACE)
                break;
            auto nextKey = parseExpression();
            match(TokenType::COLON);
            auto nextVal = parseExpression();
            mapLit->entries.push_back({std::move(nextKey), std::move(nextVal)});
        }
    }

    match(TokenType::RBRACE);
    return mapLit;
}

std::unique_ptr<ExpressionNode> Parser::parsePrimary()
{
    if (match(TokenType::COMPTIME))
    {
        if (current().type == TokenType::LBRACE)
        {
            auto compBlock = std::make_unique<ComptimeBlockExprNode>();
            compBlock->body = parseBlock();
            return compBlock;
        }
    }
    if (match(TokenType::AWAIT))
    {
        auto target = parsePostfix();
        return std::make_unique<AwaitExprNode>(std::move(target));
    }
    if (match(TokenType::MATCH))
    {
        bool hasParen = match(TokenType::LPAREN);
        auto target = parseExpression();
        if (hasParen)
            match(TokenType::RPAREN);

        auto matchExpr = std::make_unique<MatchExprNode>();
        matchExpr->target = std::move(target);

        if (match(TokenType::LBRACE))
        {
            while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
            {
                MatchArm arm;
                if (current().type == TokenType::UNDERSCORE || (current().type == TokenType::IDENTIFIER && current().value == "_"))
                {
                    arm.isWildcard = true;
                    advance();
                }
                else
                {
                    arm.isWildcard = false;
                    auto pat = parseExpression();
                    if (pat)
                        arm.patterns.push_back(std::move(pat));

                    while (match(TokenType::COMMA))
                    {
                        if (current().type == TokenType::FAT_ARROW || current().type == TokenType::IF || (current().type == TokenType::IDENTIFIER && current().value == "when"))
                        {
                            break;
                        }
                        auto nextPat = parseExpression();
                        if (nextPat)
                            arm.patterns.push_back(std::move(nextPat));
                    }
                    if (match(TokenType::IF) || (current().type == TokenType::IDENTIFIER && current().value == "when" && (advance(), true)))
                    {
                        arm.guard = parseExpression();
                    }
                }

                match(TokenType::FAT_ARROW);

                if (current().type == TokenType::LBRACE)
                {
                    arm.isExpressionBody = false;
                    arm.body = parseBlock();
                }
                else
                {
                    arm.isExpressionBody = true;
                    arm.exprBody = parseExpression();
                }
                match(TokenType::COMMA);
                match(TokenType::SEMICOLON);
                matchExpr->arms.push_back(std::move(arm));
            }
            match(TokenType::RBRACE);
        }
        return matchExpr;
    }
    if (current().type == TokenType::UNDERSCORE)
    {
        advance();
        return std::make_unique<IdentifierNode>("_");
    }
    if (current().type == TokenType::STRING_LITERAL)
    {
        std::string val = current().value;
        advance();
        return std::make_unique<LiteralNode>(val, LiteralType::STRING);
    }
    if (current().type == TokenType::CHAR_LITERAL)
    {
        std::string val = current().value;
        advance();
        return std::make_unique<LiteralNode>(val, LiteralType::CHAR);
    }
    if (current().type == TokenType::NUMBER_LITERAL)
    {
        std::string val = current().value;
        advance();
        return std::make_unique<LiteralNode>(val, LiteralType::NUMBER);
    }
    if (current().type == TokenType::TRUE || current().type == TokenType::FALSE)
    {
        std::string val = current().value;
        advance();
        return std::make_unique<LiteralNode>(val, LiteralType::BOOLEAN);
    }
    if (current().type == TokenType::LBRACKET)
    {
        return parseArrayLiteral();
    }
    if (current().type == TokenType::LBRACE)
    {
        size_t look = index + 1;
        int d = 0;
        bool hasMapColon = false;
        while (look < tokens.size() && tokens[look].type != TokenType::RBRACE && tokens[look].type != TokenType::END_OF_FILE)
        {
            if (tokens[look].type == TokenType::LBRACE)
                d++;
            if (tokens[look].type == TokenType::RBRACE)
                d--;
            if (tokens[look].type == TokenType::COLON && d == 0)
            {
                hasMapColon = true;
                break;
            }
            look++;
        }
        if (hasMapColon)
        {
            return parseMapLiteral();
        }
        return nullptr;
    }

    bool isAsyncLambda = false;
    if (current().type == TokenType::ASYNC && peekNext().type == TokenType::LPAREN)
    {
        isAsyncLambda = true;
        advance();
    }

    if (current().type == TokenType::LPAREN)
    {
        size_t lookAhead = index;
        int parenDepth = 0;
        bool isLambda = isAsyncLambda;

        if (!isLambda)
        {
            while (lookAhead < tokens.size())
            {
                if (tokens[lookAhead].type == TokenType::LPAREN)
                    parenDepth++;
                else if (tokens[lookAhead].type == TokenType::RPAREN)
                {
                    parenDepth--;
                    if (parenDepth == 0)
                    {
                        size_t afterParen = lookAhead + 1;
                        if (afterParen < tokens.size())
                        {
                            if (tokens[afterParen].type == TokenType::FAT_ARROW)
                            {
                                isLambda = true;
                            }
                            else if (tokens[afterParen].type == TokenType::ARROW)
                            {
                                size_t afterArrow = afterParen + 1;
                                while (afterArrow < tokens.size() && tokens[afterArrow].type != TokenType::FAT_ARROW && tokens[afterArrow].type != TokenType::SEMICOLON && tokens[afterArrow].type != TokenType::LBRACE)
                                {
                                    afterArrow++;
                                }
                                if (afterArrow < tokens.size() && tokens[afterArrow].type == TokenType::FAT_ARROW)
                                {
                                    isLambda = true;
                                }
                            }
                        }
                        break;
                    }
                }
                lookAhead++;
            }
        }

        if (isLambda)
        {
            auto lambda = std::make_unique<LambdaNode>();
            lambda->params = parseParameterList();

            if (match(TokenType::ARROW))
            {
                lambda->returnType = parseType();
            }

            match(TokenType::FAT_ARROW);

            if (current().type == TokenType::LBRACE)
            {
                size_t look = index + 1;
                int d = 1;
                bool isBlock = false;

                while (look < tokens.size() && d > 0 && tokens[look].type != TokenType::END_OF_FILE)
                {
                    if (tokens[look].type == TokenType::LBRACE)
                    {
                        d++;
                    }
                    else if (tokens[look].type == TokenType::RBRACE)
                    {
                        d--;
                        if (d == 0)
                            break;
                    }

                    if (d == 1)
                    {
                        TokenType t = tokens[look].type;
                        if (t == TokenType::VAR || t == TokenType::VAL || t == TokenType::RETURN ||
                            t == TokenType::IF || t == TokenType::FOR_KW ||
                            t == TokenType::PRINT || t == TokenType::PRINTLN ||
                            t == TokenType::BREAK || t == TokenType::CONTINUE)
                        {
                            isBlock = true;
                            break;
                        }
                    }
                    look++;
                }

                if (isBlock)
                {
                    lambda->isExpressionBody = false;
                    lambda->blockBody = parseBlock();
                }
                else
                {
                    lambda->isExpressionBody = true;
                    lambda->exprBody = parseMapLiteral();
                }
            }
            else
            {
                lambda->isExpressionBody = true;
                lambda->exprBody = parseExpression();
            }
            return lambda;
        }

        match(TokenType::LPAREN);
        auto expr = parseExpression();
        match(TokenType::RPAREN);
        return expr;
    }
    if (current().type == TokenType::IDENTIFIER)
    {
        std::vector<std::string> path;

        while (true)
        {
            std::string segment = current().value;
            advance();

            if (current().type == TokenType::LESS)
            {
                size_t lookAhead = index;
                int depth = 0;
                bool isTemplate = false;

                while (lookAhead < tokens.size())
                {
                    if (tokens[lookAhead].type == TokenType::LESS)
                        depth++;
                    else if (tokens[lookAhead].type == TokenType::GREATER)
                    {
                        depth--;
                        if (depth == 0)
                        {
                            if (lookAhead + 1 < tokens.size() && (tokens[lookAhead + 1].type == TokenType::LPAREN || tokens[lookAhead + 1].type == TokenType::LBRACE || tokens[lookAhead + 1].type == TokenType::DOUBLE_COLON))
                            {
                                isTemplate = true;
                            }
                            break;
                        }
                    }
                    else if (tokens[lookAhead].type == TokenType::SEMICOLON || tokens[lookAhead].type == TokenType::RBRACE)
                    {
                        break;
                    }
                    lookAhead++;
                }

                if (isTemplate)
                {
                    match(TokenType::LESS);
                    std::string genArgs = parseType();
                    while (match(TokenType::COMMA))
                    {
                        genArgs += ", " + parseType();
                    }
                    match(TokenType::GREATER);
                    segment += "<" + genArgs + ">";
                }
            }

            path.push_back(segment);

            if (!match(TokenType::DOUBLE_COLON))
            {
                break;
            }
        }

        if (path.size() > 1 || path[0].find('<') != std::string::npos)
        {
            return std::make_unique<ScopedIdentifierNode>(path);
        }
        return std::make_unique<IdentifierNode>(path[0]);
    }
    return nullptr;
}

std::unique_ptr<ExpressionNode> Parser::parsePostfix()
{
    auto expr = parsePrimary();

    while (true)
    {
        if (current().type == TokenType::LBRACKET)
        {
            std::vector<std::unique_ptr<ExpressionNode>> indices;
            while (match(TokenType::LBRACKET))
            {
                indices.push_back(parseExpression());
                match(TokenType::RBRACKET);
            }
            expr = std::make_unique<IndexAccessNode>(std::move(expr), std::move(indices));
        }
        else if (match(TokenType::LPAREN))
        {
            std::vector<std::unique_ptr<ExpressionNode>> args;
            if (current().type != TokenType::RPAREN)
            {
                args.push_back(parseExpression());
                while (match(TokenType::COMMA))
                {
                    if (current().type == TokenType::RPAREN)
                        break;
                    args.push_back(parseExpression());
                }
            }
            match(TokenType::RPAREN);
            expr = std::make_unique<FunctionCallNode>(std::move(expr), std::move(args));
        }
        else if (match(TokenType::DOT))
        {
            if (current().type == TokenType::IDENTIFIER || current().type == TokenType::NUMBER_LITERAL)
            {
                std::string memberName = current().value;
                advance();
                expr = std::make_unique<MemberAccessNode>(std::move(expr), memberName);
            }
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseUnary()
{
    if (current().type == TokenType::NOT || current().type == TokenType::MINUS)
    {
        std::string op = current().value;
        advance();
        auto right = parseUnary();
        return std::make_unique<UnaryOpNode>(op, std::move(right));
    }
    return parsePostfix();
}

std::unique_ptr<ExpressionNode> Parser::parseMultiplicative()
{
    auto left = parseUnary();

    while (current().type == TokenType::STAR || current().type == TokenType::SLASH)
    {
        std::string op = current().value;
        advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseAdditive()
{
    auto left = parseMultiplicative();

    while (current().type == TokenType::PLUS || current().type == TokenType::MINUS)
    {
        std::string op = current().value;
        advance();
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseRelational()
{
    auto left = parseAdditive();

    while (current().type == TokenType::LESS || current().type == TokenType::LESS_EQUALS ||
           current().type == TokenType::GREATER || current().type == TokenType::GREATER_EQUALS)
    {
        std::string op = current().value;
        advance();
        auto right = parseAdditive();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseEquality()
{
    auto left = parseRelational();

    while (current().type == TokenType::EQUALS || current().type == TokenType::NOT_EQUALS)
    {
        std::string op = current().value;
        advance();
        auto right = parseRelational();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalAnd()
{
    auto left = parseEquality();

    while (current().type == TokenType::LOGICAL_AND)
    {
        std::string op = current().value;
        advance();
        auto right = parseEquality();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalOr()
{
    auto left = parseLogicalAnd();

    while (current().type == TokenType::LOGICAL_OR)
    {
        std::string op = current().value;
        advance();
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseExpression()
{
    return parseLogicalOr();
}

std::vector<std::unique_ptr<StatementNode>> Parser::parseBlock()
{
    std::vector<std::unique_ptr<StatementNode>> stmts;
    if (match(TokenType::LBRACE))
    {
        while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
        {
            auto stmt = parseStatement();
            if (stmt)
            {
                stmts.push_back(std::move(stmt));
            }
        }
        match(TokenType::RBRACE);
    }
    return stmts;
}

std::unique_ptr<StatementNode> Parser::parseIf()
{
    advance();

    bool hasParen = match(TokenType::LPAREN);
    auto condition = parseExpression();
    if (hasParen)
    {
        match(TokenType::RPAREN);
    }

    auto ifNode = std::make_unique<IfNode>();
    ifNode->condition = std::move(condition);
    ifNode->thenBody = parseBlock();

    while (current().type == TokenType::ELSE)
    {
        advance();

        if (current().type == TokenType::IF)
        {
            advance();
            bool elseIfParen = match(TokenType::LPAREN);
            auto elseIfCond = parseExpression();
            if (elseIfParen)
            {
                match(TokenType::RPAREN);
            }
            auto elseIfBody = parseBlock();
            ifNode->elseIfBranches.push_back({std::move(elseIfCond), std::move(elseIfBody)});
        }
        else
        {
            ifNode->elseBody = parseBlock();
            break;
        }
    }

    return ifNode;
}

std::unique_ptr<StatementNode> Parser::parseMatch()
{
    advance();

    bool hasParen = match(TokenType::LPAREN);
    auto target = parseExpression();
    if (hasParen)
    {
        match(TokenType::RPAREN);
    }

    auto matchNode = std::make_unique<MatchNode>();
    matchNode->target = std::move(target);

    if (match(TokenType::LBRACE))
    {
        while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
        {
            MatchArm arm;
            if (current().type == TokenType::UNDERSCORE || (current().type == TokenType::IDENTIFIER && current().value == "_"))
            {
                arm.isWildcard = true;
                advance();
            }
            else
            {
                arm.isWildcard = false;
                auto pat = parseExpression();
                if (pat)
                    arm.patterns.push_back(std::move(pat));

                while (match(TokenType::COMMA))
                {
                    if (current().type == TokenType::FAT_ARROW || current().type == TokenType::IF || (current().type == TokenType::IDENTIFIER && current().value == "when"))
                    {
                        break;
                    }
                    auto nextPat = parseExpression();
                    if (nextPat)
                        arm.patterns.push_back(std::move(nextPat));
                }
                if (match(TokenType::IF) || (current().type == TokenType::IDENTIFIER && current().value == "when" && (advance(), true)))
                {
                    arm.guard = parseExpression();
                }
            }

            match(TokenType::FAT_ARROW);

            if (current().type == TokenType::LBRACE)
            {
                arm.isExpressionBody = false;
                arm.body = parseBlock();
            }
            else
            {
                auto stmt = parseStatement();
                if (stmt)
                {
                    arm.isExpressionBody = false;
                    arm.body.push_back(std::move(stmt));
                }
            }
            match(TokenType::COMMA);
            match(TokenType::SEMICOLON);
            matchNode->arms.push_back(std::move(arm));
        }
        match(TokenType::RBRACE);
    }
    return matchNode;
}

std::unique_ptr<StatementNode> Parser::parseAssignment()
{
    auto target = parsePostfix();
    match(TokenType::ASSIGN);
    auto valExpr = parseExpression();
    match(TokenType::SEMICOLON);
    return std::make_unique<AssignmentNode>(std::move(target), std::move(valExpr));
}

std::unique_ptr<StatementNode> Parser::parseIncDec()
{
    std::string varName = current().value;
    advance();
    std::string op = current().value;
    advance();
    match(TokenType::SEMICOLON);
    return std::make_unique<IncDecNode>(varName, op);
}

std::unique_ptr<StatementNode> Parser::parseVariableDecl(Visibility vis)
{
    bool isComptime = match(TokenType::COMPTIME);
    bool isMut = true;

    if (match(TokenType::VAR))
    {
        isMut = true;
        if (current().type == TokenType::IDENTIFIER)
        {
            std::string varName = current().value;
            advance();

            std::string typeName = "auto";
            if (match(TokenType::COLON))
            {
                typeName = parseType();
            }

            if (match(TokenType::ASSIGN))
            {
                auto valExpr = parseExpression();
                match(TokenType::SEMICOLON);
                return std::make_unique<VariableDeclNode>(typeName, varName, std::move(valExpr), isComptime, isMut, vis);
            }
        }
        return nullptr;
    }

    if (match(TokenType::VAL))
    {
        isMut = false;
        if (current().type == TokenType::IDENTIFIER)
        {
            std::string varName = current().value;
            advance();

            std::string typeName = "auto";
            if (match(TokenType::COLON))
            {
                typeName = parseType();
            }

            if (match(TokenType::ASSIGN))
            {
                auto valExpr = parseExpression();
                match(TokenType::SEMICOLON);
                return std::make_unique<VariableDeclNode>(typeName, varName, std::move(valExpr), isComptime, isMut, vis);
            }
        }
        return nullptr;
    }

    std::string typeName = parseType();
    if (current().type == TokenType::IDENTIFIER)
    {
        std::string varName = current().value;
        advance();

        if (match(TokenType::ASSIGN))
        {
            auto valExpr = parseExpression();
            match(TokenType::SEMICOLON);
            return std::make_unique<VariableDeclNode>(typeName, varName, std::move(valExpr), isComptime, true, vis);
        }
    }
    return nullptr;
}

std::unique_ptr<StatementNode> Parser::parseFor()
{
    advance();

    auto forNode = std::make_unique<ForNode>();

    if (current().type == TokenType::LBRACE)
    {
        forNode->kind = ForKind::INFINITE;
        forNode->body = parseBlock();
        return forNode;
    }

    bool hasParen = match(TokenType::LPAREN);

    size_t lookAhead = index;
    int parenDepth = 0;
    bool hasColon = false;
    bool hasSemicolon = false;

    while (lookAhead < tokens.size() && tokens[lookAhead].type != TokenType::LBRACE && tokens[lookAhead].type != TokenType::END_OF_FILE)
    {
        if (tokens[lookAhead].type == TokenType::LPAREN)
            parenDepth++;
        if (tokens[lookAhead].type == TokenType::RPAREN)
        {
            if (parenDepth > 0)
                parenDepth--;
        }
        if (tokens[lookAhead].type == TokenType::COLON)
        {
            hasColon = true;
            break;
        }
        if (tokens[lookAhead].type == TokenType::SEMICOLON)
        {
            hasSemicolon = true;
            break;
        }
        lookAhead++;
    }

    if (hasColon)
    {
        forNode->kind = ForKind::FOR_RANGE;

        if (current().type == TokenType::VAR || current().type == TokenType::VAL)
        {
            advance();
        }

        std::string iterName = "";
        if (current().type == TokenType::IDENTIFIER || current().type == TokenType::UNDERSCORE)
        {
            iterName = current().value;
            advance();

            if (match(TokenType::COMMA))
            {
                if (current().type == TokenType::VAR || current().type == TokenType::VAL)
                {
                    advance();
                }
                std::string secondVar = current().value;
                advance();
                iterName = "[" + iterName + ", " + secondVar + "]";
            }
        }

        if (hasParen)
        {
            if (current().type == TokenType::RPAREN)
            {
                advance();
            }
        }

        match(TokenType::COLON);

        forNode->iteratorVar = iterName;
        forNode->iterable = parseExpression();

        if (hasParen && current().type == TokenType::RPAREN)
        {
            advance();
        }

        forNode->body = parseBlock();
        return forNode;
    }

    if (hasSemicolon)
    {
        forNode->kind = ForKind::CONTROLLED;

        if (current().type != TokenType::SEMICOLON)
        {
            if (current().type == TokenType::IDENTIFIER && peekNext().type == TokenType::ASSIGN)
            {
                forNode->init = parseAssignment();
            }
            else
            {
                forNode->init = parseVariableDecl();
            }
        }
        else
        {
            match(TokenType::SEMICOLON);
        }

        if (current().type != TokenType::SEMICOLON)
        {
            forNode->condition = parseExpression();
        }
        match(TokenType::SEMICOLON);

        if (current().type != TokenType::RPAREN && current().type != TokenType::LBRACE)
        {
            if (current().type == TokenType::IDENTIFIER)
            {
                if (peekNext().type == TokenType::PLUS_PLUS || peekNext().type == TokenType::MINUS_MINUS)
                {
                    std::string varName = current().value;
                    advance();
                    std::string op = current().value;
                    advance();
                    forNode->increment = std::make_unique<IncDecNode>(varName, op);
                }
                else if (peekNext().type == TokenType::ASSIGN)
                {
                    auto target = parsePostfix();
                    match(TokenType::ASSIGN);
                    forNode->increment = std::make_unique<AssignmentNode>(std::move(target), parseExpression());
                }
            }
        }

        if (hasParen)
        {
            match(TokenType::RPAREN);
        }

        forNode->body = parseBlock();
        return forNode;
    }

    forNode->kind = ForKind::CONDITIONAL;
    forNode->condition = parseExpression();
    if (hasParen)
    {
        match(TokenType::RPAREN);
    }
    forNode->body = parseBlock();
    return forNode;
}

std::unique_ptr<StatementNode> Parser::parsePrint()
{
    bool isPrintln = (current().type == TokenType::PRINTLN);
    advance();

    if (match(TokenType::LPAREN))
    {
        auto expr = parseExpression();
        if (match(TokenType::RPAREN))
        {
            match(TokenType::SEMICOLON);
            return std::make_unique<PrintNode>(isPrintln, std::move(expr));
        }
    }
    return nullptr;
}

std::unique_ptr<StatementNode> Parser::parseStatement()
{
    if (current().type == TokenType::RETURN)
    {
        advance();
        std::unique_ptr<ExpressionNode> retVal = nullptr;
        if (current().type != TokenType::SEMICOLON && current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE)
        {
            retVal = parseExpression();
        }
        match(TokenType::SEMICOLON);
        return std::make_unique<ReturnNode>(std::move(retVal));
    }
    if (current().type == TokenType::FOR_KW)
    {
        return parseFor();
    }
    if (current().type == TokenType::IF)
    {
        return parseIf();
    }
    if (current().type == TokenType::MATCH)
    {
        return parseMatch();
    }
    if (current().type == TokenType::BREAK)
    {
        advance();
        match(TokenType::SEMICOLON);
        return std::make_unique<BreakNode>();
    }
    if (current().type == TokenType::CONTINUE)
    {
        advance();
        match(TokenType::SEMICOLON);
        return std::make_unique<ContinueNode>();
    }
    if (current().type == TokenType::PRINT || current().type == TokenType::PRINTLN)
    {
        return parsePrint();
    }
    if (current().type == TokenType::ASSERT)
    {
        advance();
        bool hasParen = match(TokenType::LPAREN);
        auto cond = parseExpression();
        if (hasParen)
        {
            match(TokenType::RPAREN);
        }
        match(TokenType::SEMICOLON);
        return std::make_unique<AssertNode>(std::move(cond));
    }
    if (current().type == TokenType::ASSERT_EQ)
    {
        advance();
        match(TokenType::LPAREN);
        auto left = parseExpression();
        match(TokenType::COMMA);
        auto right = parseExpression();
        match(TokenType::RPAREN);
        match(TokenType::SEMICOLON);
        return std::make_unique<AssertEqNode>(std::move(left), std::move(right));
    }
    if (current().type == TokenType::ASSERT_NE)
    {
        advance();
        match(TokenType::LPAREN);
        auto left = parseExpression();
        match(TokenType::COMMA);
        auto right = parseExpression();
        match(TokenType::RPAREN);
        match(TokenType::SEMICOLON);
        return std::make_unique<AssertNeNode>(std::move(left), std::move(right));
    }
    if (current().type == TokenType::COMPTIME)
    {
        if (peekNext().type == TokenType::FN || peekNext().type == TokenType::ASYNC)
        {
            return nullptr;
        }
        return parseVariableDecl();
    }
    if (current().type == TokenType::VAR || current().type == TokenType::VAL)
    {
        return parseVariableDecl();
    }
    if (current().type == TokenType::IDENTIFIER || current().type == TokenType::AWAIT)
    {
        if (current().type == TokenType::IDENTIFIER && (peekNext().type == TokenType::PLUS_PLUS || peekNext().type == TokenType::MINUS_MINUS))
        {
            return parseIncDec();
        }

        size_t lookAhead = index + 1;
        int angleDepth = 0;
        while (lookAhead < tokens.size())
        {
            if (tokens[lookAhead].type == TokenType::LESS)
            {
                angleDepth++;
            }
            else if (tokens[lookAhead].type == TokenType::GREATER)
            {
                if (angleDepth > 0)
                    angleDepth--;
            }
            else if (angleDepth == 0 && (tokens[lookAhead].type == TokenType::IDENTIFIER))
            {
                if (lookAhead + 1 < tokens.size() && tokens[lookAhead + 1].type == TokenType::ASSIGN)
                {
                    return parseVariableDecl();
                }
            }
            else if (tokens[lookAhead].type == TokenType::SEMICOLON || tokens[lookAhead].type == TokenType::LBRACE || tokens[lookAhead].type == TokenType::RBRACE)
            {
                break;
            }
            lookAhead++;
        }

        auto expr = parsePostfix();
        if (match(TokenType::ASSIGN))
        {
            auto valExpr = parseExpression();
            match(TokenType::SEMICOLON);
            return std::make_unique<AssignmentNode>(std::move(expr), std::move(valExpr));
        }
        match(TokenType::SEMICOLON);
        return std::make_unique<ExpressionStatementNode>(std::move(expr));
    }
    advance();
    return nullptr;
}

std::unique_ptr<FunctionNode> Parser::parseFunction(Visibility vis)
{
    bool isComptime = match(TokenType::COMPTIME);
    bool isAsync = match(TokenType::ASYNC);
    match(TokenType::FN);

    if (current().type == TokenType::IDENTIFIER)
    {
        std::string fnName = current().value;
        advance();

        std::vector<Parameter> params = parseParameterList();

        std::string retType = isAsync ? "Task<void>" : "void";
        if (match(TokenType::ARROW))
        {
            retType = parseType();
        }

        auto funcNode = std::make_unique<FunctionNode>(fnName, retType, isComptime, isAsync, vis);
        funcNode->params = std::move(params);
        funcNode->body = parseBlock();
        return funcNode;
    }
    return nullptr;
}

std::unique_ptr<ProgramNode> Parser::parseProgram(const std::string &defaultPkgName)
{
    auto program = std::make_unique<ProgramNode>();
    std::string currentTopPkg = "";
    std::unique_ptr<PackageNode> activePkgNode = nullptr;

    auto flushActivePkg = [&]()
    {
        if (activePkgNode)
        {
            program->packages.push_back(std::move(activePkgNode));
            activePkgNode = nullptr;
        }
    };

    if (current().type == TokenType::PKG && peekNext().type == TokenType::IDENTIFIER)
    {
        advance();
        std::string declaredPkg = current().value;
        advance();

        if (defaultPkgName != "main" && !defaultPkgName.empty() && declaredPkg != defaultPkgName)
        {
            std::cerr << "\033[1;31m[Package Error]\033[0m Package name mismatch! "
                      << "Declared 'pkg " << declaredPkg << "', but file is '"
                      << defaultPkgName << ".zy'. (Expected 'pkg " << defaultPkgName << "' or omit it entirely).\n";
            std::exit(1);
        }

        currentTopPkg = declaredPkg;
        program->packageName = currentTopPkg;
        match(TokenType::SEMICOLON);

        if (currentTopPkg != "main")
        {
            activePkgNode = std::make_unique<PackageNode>(currentTopPkg);
        }
    }
    else
    {
        program->packageName = defaultPkgName;
    }

    while (current().type != TokenType::END_OF_FILE)
    {
        Visibility topVis = parseVisibility();

        if (current().type == TokenType::PKG)
        {
            size_t look = index + 1;
            while (look < tokens.size() && tokens[look].type == TokenType::IDENTIFIER)
            {
                look++;
                if (look < tokens.size() && tokens[look].type == TokenType::DOUBLE_COLON)
                {
                    look++;
                }
                else
                {
                    break;
                }
            }
            if (look < tokens.size() && tokens[look].type == TokenType::LBRACE)
            {
                auto pkg = parsePackage();
                if (pkg)
                {
                    if (activePkgNode)
                    {
                        activePkgNode->members.push_back(std::move(pkg));
                    }
                    else
                    {
                        program->packages.push_back(std::move(pkg));
                    }
                }
            }
            else
            {
                advance();
                if (current().type == TokenType::IDENTIFIER)
                {
                    std::string pkgName = current().value;
                    advance();
                    while (match(TokenType::DOUBLE_COLON))
                    {
                        if (current().type == TokenType::IDENTIFIER)
                        {
                            pkgName += "::" + current().value;
                            advance();
                        }
                    }
                    match(TokenType::SEMICOLON);

                    flushActivePkg();
                    currentTopPkg = pkgName;

                    if (program->packageName.empty())
                    {
                        program->packageName = pkgName;
                    }

                    if (currentTopPkg != "main")
                    {
                        activePkgNode = std::make_unique<PackageNode>(currentTopPkg);
                    }
                }
            }
        }
        else if (current().type == TokenType::MOD)
        {
            auto mod = parseMod(topVis);
            if (mod)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(mod));
                }
                else
                {
                    program->modules.push_back(std::move(mod));
                }
            }
        }
        else if (current().type == TokenType::IMPORT)
        {
            auto imps = parseImport();
            for (auto &imp : imps)
            {
                if (imp)
                    program->imports.push_back(std::move(imp));
            }
        }
        else if (current().type == TokenType::RECORD)
        {
            auto rec = parseRecord(topVis);
            if (rec)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(rec));
                }
                else
                {
                    program->records.push_back(std::move(rec));
                }
            }
        }
        else if (current().type == TokenType::ENUM)
        {
            auto en = parseEnum(topVis);
            if (en)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(en));
                }
                else
                {
                    program->enums.push_back(std::move(en));
                }
            }
        }
        else if (current().type == TokenType::TRAIT)
        {
            auto tr = parseTrait(topVis);
            if (tr)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(tr));
                }
                else
                {
                    program->traits.push_back(std::move(tr));
                }
            }
        }
        else if (current().type == TokenType::IMPL)
        {
            auto im = parseImpl();
            if (im)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(im));
                }
                else
                {
                    program->impls.push_back(std::move(im));
                }
            }
        }
        else if (current().type == TokenType::TEST)
        {
            auto test = parseTestBlock();
            if (test)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(test));
                }
                else
                {
                    program->tests.push_back(std::move(test));
                }
            }
        }
        else if (current().type == TokenType::COMPTIME)
        {
            if (peekNext().type == TokenType::FN || peekNext().type == TokenType::ASYNC)
            {
                auto fn = parseFunction(topVis);
                if (fn)
                {
                    if (activePkgNode)
                    {
                        activePkgNode->members.push_back(std::move(fn));
                    }
                    else
                    {
                        program->functions.push_back(std::move(fn));
                    }
                }
            }
            else
            {
                advance();
            }
        }
        else if (current().type == TokenType::FN || current().type == TokenType::ASYNC)
        {
            auto fn = parseFunction(topVis);
            if (fn)
            {
                if (activePkgNode)
                {
                    activePkgNode->members.push_back(std::move(fn));
                }
                else
                {
                    program->functions.push_back(std::move(fn));
                }
            }
        }
        else
        {
            advance();
        }
    }

    flushActivePkg();
    return program;
}
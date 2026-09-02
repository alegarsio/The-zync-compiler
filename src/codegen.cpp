#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <filesystem>
#include "../include/codegen.hpp"

namespace fs = std::filesystem;

static const std::unordered_set<std::string> cppPrimitiveTypes = {
    "int", "double", "float", "char", "bool", "void", "long", "short", "signed", "unsigned"};

static const std::unordered_set<std::string> cppKeywords = {
    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
    "atomic_noexcept", "auto", "bitand", "bitor", "break", "case", "catch",
    "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
    "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
    "for", "friend", "goto", "if", "inline", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
    "private", "protected", "public", "reflexpr", "register", "reinterpret_cast",
    "requires", "return", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "synchronized", "template", "this",
    "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
    "union", "using", "virtual", "volatile", "wchar_t", "while", "xor", "xor_eq"};

static std::string sanitizeName(const std::string &name)
{
    if (name == "Ok" || name == "Err" || name == "Result" || name == "Tuple" || name == "_" || name == "auto" || name == "Task")
    {
        return name;
    }
    if (name.front() == '[' && name.back() == ']')
    {
        return name;
    }
    if (cppPrimitiveTypes.find(name) != cppPrimitiveTypes.end())
    {
        return name;
    }
    if (cppKeywords.find(name) != cppKeywords.end())
    {
        return "_" + name;
    }
    return name;
}

CodeGen::CodeGen(const ProgramNode *program)
    : root(program), needsIOStream(false), needsString(false), needsVector(false), needsMap(false),
      currentBoundVar(""), currentBoundVal("")
{
    if (root)
    {
        for (const auto &tr : root->traits)
        {
            traitMap[tr->name] = tr.get();
        }
        for (const auto &rec : root->records)
        {
            recordMap[rec->name] = rec.get();
        }
        for (const auto &en : root->enums)
        {
            enumMap[en->name] = en.get();
        }
        for (const auto &mod : root->modules)
        {
            for (const auto &m : mod->members)
            {
                if (auto rec = dynamic_cast<const RecordNode *>(m.get()))
                {
                    recordMap[mod->name + "::" + rec->name] = rec;
                    recordMap[rec->name] = rec;
                }
                if (auto en = dynamic_cast<const EnumNode *>(m.get()))
                {
                    enumMap[mod->name + "::" + en->name] = en;
                    enumMap[en->name] = en;
                }
                if (auto tr = dynamic_cast<const TraitNode *>(m.get()))
                {
                    traitMap[mod->name + "::" + tr->name] = tr;
                    traitMap[tr->name] = tr;
                }
            }
        }
        for (const auto &pkg : root->packages)
        {
            for (const auto &m : pkg->members)
            {
                if (auto rec = dynamic_cast<const RecordNode *>(m.get()))
                {
                    recordMap[pkg->name + "::" + rec->name] = rec;
                    recordMap[rec->name] = rec;
                }
                if (auto en = dynamic_cast<const EnumNode *>(m.get()))
                {
                    enumMap[pkg->name + "::" + en->name] = en;
                    enumMap[en->name] = en;
                }
                if (auto tr = dynamic_cast<const TraitNode *>(m.get()))
                {
                    traitMap[pkg->name + "::" + tr->name] = tr;
                    traitMap[tr->name] = tr;
                }
            }
        }
        for (const auto &im : root->impls)
        {
            std::string fullTarget = im->targetName;
            std::string shortTarget = im->targetName;
            size_t colonPos = shortTarget.rfind("::");
            if (colonPos != std::string::npos)
            {
                shortTarget = shortTarget.substr(colonPos + 2);
            }

            if (!im->traitName.empty())
            {
                recordTraitsMap[fullTarget].push_back(im->traitName);
                if (shortTarget != fullTarget)
                {
                    recordTraitsMap[shortTarget].push_back(im->traitName);
                }
            }
            for (const auto &fn : im->methods)
            {
                implMethodsMap[fullTarget].push_back(fn.get());
                if (shortTarget != fullTarget)
                {
                    implMethodsMap[shortTarget].push_back(fn.get());
                }
            }
        }
    }
}

std::string CodeGen::getIndent(int level)
{
    return std::string(level * 4, ' ');
}

std::string CodeGen::convertType(const std::string &zyType)
{
    if (zyType.empty())
        return "";

    if (zyType == "var" || zyType == "val" || zyType == "auto")
    {
        return "auto";
    }

    if (zyType.rfind("Task<", 0) == 0 && zyType.back() == '>')
    {
        std::string inner = zyType.substr(5, zyType.size() - 6);
        while (!inner.empty() && inner.front() == ' ') inner.erase(0, 1);
        while (!inner.empty() && inner.back() == ' ') inner.pop_back();
        return "std::future<" + convertType(inner) + ">";
    }

    if (zyType.rfind("Tuple<", 0) == 0 && zyType.back() == '>')
    {
        std::string inner = zyType.substr(6, zyType.size() - 7);
        std::vector<std::string> args;
        int depth = 0;
        std::string curr = "";
        for (size_t i = 0; i < inner.size(); ++i)
        {
            if (inner[i] == '<')
                depth++;
            else if (inner[i] == '>')
                depth--;

            if (inner[i] == ',' && depth == 0)
            {
                while (!curr.empty() && curr[0] == ' ')
                    curr.erase(0, 1);
                while (!curr.empty() && curr.back() == ' ')
                    curr.pop_back();
                args.push_back(curr);
                curr = "";
            }
            else
            {
                curr += inner[i];
            }
        }
        if (!curr.empty())
        {
            while (!curr.empty() && curr[0] == ' ')
                curr.erase(0, 1);
            while (!curr.empty() && curr.back() == ' ')
                curr.pop_back();
            args.push_back(curr);
        }

        std::string res = "std::tuple<";
        for (size_t i = 0; i < args.size(); ++i)
        {
            res += convertType(args[i]);
            if (i + 1 < args.size())
                res += ", ";
        }
        res += ">";
        return res;
    }

    if (zyType.rfind("vector<", 0) == 0 && zyType.back() == '>')
    {
        needsVector = true;
        std::string inner = zyType.substr(7, zyType.size() - 8);
        return "std::vector<" + convertType(inner) + ">";
    }

    if (zyType.rfind("array<", 0) == 0 && zyType.back() == '>')
    {
        needsVector = true;
        std::string inner = zyType.substr(6, zyType.size() - 7);
        return "std::vector<" + convertType(inner) + ">";
    }
    if (zyType.rfind("map<", 0) == 0 && zyType.back() == '>')
    {
        needsMap = true;
        std::string inner = zyType.substr(4, zyType.size() - 5);
        int depth = 0;
        size_t commaPos = std::string::npos;
        for (size_t i = 0; i < inner.size(); ++i)
        {
            if (inner[i] == '<')
                depth++;
            else if (inner[i] == '>')
                depth--;
            else if (inner[i] == ',' && depth == 0)
            {
                commaPos = i;
                break;
            }
        }
        if (commaPos != std::string::npos)
        {
            std::string keyType = inner.substr(0, commaPos);
            std::string valType = inner.substr(commaPos + 1);
            while (!keyType.empty() && keyType.front() == ' ')
                keyType.erase(0, 1);
            while (!keyType.empty() && keyType.back() == ' ')
                keyType.pop_back();
            while (!valType.empty() && valType.front() == ' ')
                valType.erase(0, 1);
            while (!valType.empty() && valType.back() == ' ')
                valType.pop_back();
            return "std::map<" + convertType(keyType) + ", " + convertType(valType) + ">";
        }
    }

    size_t genericPos = zyType.find('<');
    if (genericPos != std::string::npos && zyType.back() == '>')
    {
        std::string base = zyType.substr(0, genericPos);
        std::string inner = zyType.substr(genericPos + 1, zyType.size() - genericPos - 2);

        std::vector<std::string> args;
        int depth = 0;
        std::string curr = "";
        for (size_t i = 0; i < inner.size(); ++i)
        {
            if (inner[i] == '<')
                depth++;
            else if (inner[i] == '>')
                depth--;

            if (inner[i] == ',' && depth == 0)
            {
                while (!curr.empty() && curr[0] == ' ')
                    curr.erase(0, 1);
                while (!curr.empty() && curr.back() == ' ')
                    curr.pop_back();
                args.push_back(curr);
                curr = "";
            }
            else
            {
                curr += inner[i];
            }
        }
        if (!curr.empty())
        {
            while (!curr.empty() && curr[0] == ' ')
                curr.erase(0, 1);
            while (!curr.empty() && curr.back() == ' ')
                curr.pop_back();
            args.push_back(curr);
        }

        std::string res = sanitizeName(base) + "<";
        for (size_t i = 0; i < args.size(); ++i)
        {
            res += convertType(args[i]);
            if (i + 1 < args.size())
                res += ", ";
        }
        res += ">";
        return res;
    }

    if (zyType == "string")
    {
        needsString = true;
        return "std::string";
    }
    if (zyType == "long")
        return "long long";
    if (zyType == "float")
        return "float";
    if (zyType == "double")
        return "double";
    if (zyType == "int")
        return "int";
    if (zyType == "char")
        return "char";
    if (zyType == "bool")
        return "bool";
    if (zyType == "void")
        return "void";

    if (traitMap.find(zyType) != traitMap.end())
    {
        return sanitizeName(zyType) + "&";
    }

    return sanitizeName(zyType);
}

std::string CodeGen::genExpression(const ExpressionNode *expr)
{
    if (!expr)
        return "";

    if (auto awaitExpr = dynamic_cast<const AwaitExprNode *>(expr))
    {
        return genExpression(awaitExpr->target.get()) + ".get()";
    }

    if (auto compBlock = dynamic_cast<const ComptimeBlockExprNode *>(expr))
    {
        std::ostringstream out;
        out << "[]() constexpr {\n";
        for (const auto &s : compBlock->body)
        {
            out << genStatement(s.get(), 2);
        }
        out << "    }()";
        return out.str();
    }

    if (auto matchExpr = dynamic_cast<const MatchExprNode *>(expr))
    {
        static int matchExprCounter = 0;
        std::string tempVar = "__match_val_" + std::to_string(matchExprCounter++);
        std::ostringstream out;
        out << "([&]() {\n";
        out << "        auto " << tempVar << " = " << genExpression(matchExpr->target.get()) << ";\n";
        bool first = true;
        for (const auto &arm : matchExpr->arms)
        {
            if (arm.isWildcard)
            {
                if (first)
                {
                    out << "        {\n";
                }
                else
                {
                    out << " else {\n";
                }
                if (arm.isExpressionBody)
                {
                    out << "            return " << genExpression(arm.exprBody.get()) << ";\n";
                }
                else
                {
                    for (const auto &s : arm.body)
                    {
                        out << genStatement(s.get(), 3);
                    }
                }
                out << "        }";
            }
            else
            {
                std::ostringstream condStream;
                std::string bindDecl = "";
                std::string boundName = "";

                for (size_t pIdx = 0; pIdx < arm.patterns.size(); ++pIdx)
                {
                    const auto *pExpr = arm.patterns[pIdx].get();
                    if (!pExpr)
                        continue;
                    if (pIdx > 0)
                        condStream << " || ";

                    if (auto fnCall = dynamic_cast<const FunctionCallNode *>(pExpr))
                    {
                        if (auto calleeId = dynamic_cast<const IdentifierNode *>(fnCall->callee.get()))
                        {
                            if (calleeId->name == "Ok" && fnCall->args.size() == 1)
                            {
                                condStream << tempVar << ".is_ok";
                                if (auto bindId = dynamic_cast<const IdentifierNode *>(fnCall->args[0].get()))
                                {
                                    if (bindId->name != "_")
                                    {
                                        bindDecl = "            auto " + sanitizeName(bindId->name) + " = " + tempVar + ".ok_val;\n";
                                    }
                                }
                                continue;
                            }
                            else if (calleeId->name == "Err" && fnCall->args.size() == 1)
                            {
                                condStream << "!" << tempVar << ".is_ok";
                                if (auto bindId = dynamic_cast<const IdentifierNode *>(fnCall->args[0].get()))
                                {
                                    if (bindId->name != "_")
                                    {
                                        bindDecl = "            auto " + sanitizeName(bindId->name) + " = " + tempVar + ".err_val;\n";
                                    }
                                }
                                continue;
                            }
                        }
                    }

                    if (auto idNode = dynamic_cast<const IdentifierNode *>(pExpr))
                    {
                        if (idNode->name == "_")
                        {
                            condStream << "true";
                            continue;
                        }
                        if (arm.guard && arm.patterns.size() == 1)
                        {
                            boundName = idNode->name;
                            bindDecl = "            auto " + sanitizeName(idNode->name) + " = " + tempVar + ";\n";
                            condStream << "true";
                            continue;
                        }
                    }

                    std::string rhs = genExpression(pExpr);
                    if (!rhs.empty())
                    {
                        condStream << tempVar << " == " << rhs;
                    }
                    else
                    {
                        condStream << "true";
                    }
                }

                std::string condExpr = condStream.str();
                if (arm.guard)
                {
                    if (!boundName.empty())
                    {
                        currentBoundVar = boundName;
                        currentBoundVal = tempVar;
                    }
                    std::string guardStr = genExpression(arm.guard.get());
                    currentBoundVar = "";
                    currentBoundVal = "";

                    if (condExpr == "true")
                    {
                        condExpr = guardStr;
                    }
                    else
                    {
                        condExpr = "(" + condExpr + ") && (" + guardStr + ")";
                    }
                }

                if (first)
                {
                    out << "        if (" << condExpr << ") {\n";
                    first = false;
                }
                else
                {
                    out << " else if (" << condExpr << ") {\n";
                }

                if (!bindDecl.empty())
                {
                    out << bindDecl;
                }

                if (arm.isExpressionBody)
                {
                    out << "            return " << genExpression(arm.exprBody.get()) << ";\n";
                }
                else
                {
                    for (const auto &s : arm.body)
                    {
                        out << genStatement(s.get(), 3);
                    }
                }
                out << "        }";
            }
        }
        out << "\n    })()";
        return out.str();
    }

    if (auto lit = dynamic_cast<const LiteralNode *>(expr))
    {
        if (lit->litType == LiteralType::STRING)
        {
            return "std::string(\"" + lit->value + "\")";
        }
        if (lit->litType == LiteralType::CHAR)
        {
            return "'" + lit->value + "'";
        }
        return lit->value;
    }
    if (auto id = dynamic_cast<const IdentifierNode *>(expr))
    {
        if (!currentBoundVar.empty() && id->name == currentBoundVar)
        {
            return currentBoundVal;
        }
        if (id->name == "self")
        {
            return "(*this)";
        }
        if (id->name == "Tuple")
        {
            return "std::make_tuple";
        }
        if (id->name == "_")
        {
            return "";
        }
        if (id->name.rfind("vector<", 0) == 0 && id->name.back() == '>')
        {
            needsVector = true;
            std::string inner = id->name.substr(7, id->name.size() - 8);
            return "std::vector<" + convertType(inner) + ">{}";
        }
        if (id->name.rfind("array<", 0) == 0 && id->name.back() == '>')
        {
            needsVector = true;
            std::string inner = id->name.substr(6, id->name.size() - 7);
            return "std::vector<" + convertType(inner) + ">{}";
        }
        if (id->name.rfind("map<", 0) == 0 && id->name.back() == '>')
        {
            needsMap = true;
            std::string inner = id->name.substr(4, id->name.size() - 5);
            int depth = 0;
            size_t commaPos = std::string::npos;
            for (size_t i = 0; i < inner.size(); ++i)
            {
                if (inner[i] == '<')
                    depth++;
                else if (inner[i] == '>')
                    depth--;
                else if (inner[i] == ',' && depth == 0)
                {
                    commaPos = i;
                    break;
                }
            }
            if (commaPos != std::string::npos)
            {
                std::string keyType = inner.substr(0, commaPos);
                std::string valType = inner.substr(commaPos + 1);
                while (!keyType.empty() && keyType.front() == ' ')
                    keyType.erase(0, 1);
                while (!keyType.empty() && keyType.back() == ' ')
                    keyType.pop_back();
                while (!valType.empty() && valType.front() == ' ')
                    valType.erase(0, 1);
                while (!valType.empty() && valType.back() == ' ')
                    valType.pop_back();
                return "std::map<" + convertType(keyType) + ", " + convertType(valType) + ">{}";
            }
        }
        return sanitizeName(id->name);
    }
    if (auto scopedId = dynamic_cast<const ScopedIdentifierNode *>(expr))
    {
        std::ostringstream out;
        for (size_t i = 0; i < scopedId->path.size(); ++i)
        {
            std::string segment = scopedId->path[i];
            if (segment.rfind("vector<", 0) == 0 && segment.back() == '>')
            {
                needsVector = true;
                std::string inner = segment.substr(7, segment.size() - 8);
                out << "std::vector<" + convertType(inner) + ">";
            }
            else if (segment.rfind("array<", 0) == 0 && segment.back() == '>')
            {
                needsVector = true;
                std::string inner = segment.substr(6, segment.size() - 7);
                out << "std::vector<" + convertType(inner) + ">";
            }
            else if (segment.rfind("map<", 0) == 0 && segment.back() == '>')
            {
                needsMap = true;
                std::string inner = segment.substr(4, segment.size() - 5);
                int depth = 0;
                size_t commaPos = std::string::npos;
                for (size_t j = 0; j < inner.size(); ++j)
                {
                    if (inner[j] == '<')
                        depth++;
                    else if (inner[j] == '>')
                        depth--;
                    else if (inner[j] == ',' && depth == 0)
                    {
                        commaPos = j;
                        break;
                    }
                }
                if (commaPos != std::string::npos)
                {
                    std::string keyType = inner.substr(0, commaPos);
                    std::string valType = inner.substr(commaPos + 1);
                    while (!keyType.empty() && keyType.front() == ' ')
                        keyType.erase(0, 1);
                    while (!keyType.empty() && keyType.back() == ' ')
                        keyType.pop_back();
                    while (!valType.empty() && valType.front() == ' ')
                        valType.erase(0, 1);
                    while (!valType.empty() && valType.back() == ' ')
                        valType.pop_back();
                    out << "std::map<" + convertType(keyType) + ", " + convertType(valType) + ">";
                }
            }
            else
            {
                size_t genPos = segment.find('<');
                if (genPos != std::string::npos && segment.back() == '>')
                {
                    std::string base = segment.substr(0, genPos);
                    std::string inner = segment.substr(genPos + 1, segment.size() - genPos - 2);
                    out << sanitizeName(base) << "<" << convertType(inner) << ">";
                }
                else
                {
                    out << sanitizeName(segment);
                }
            }
            if (i + 1 < scopedId->path.size())
            {
                out << "::";
            }
        }
        return out.str();
    }
    if (auto memAccess = dynamic_cast<const MemberAccessNode *>(expr))
    {
        if (auto idTarget = dynamic_cast<const IdentifierNode *>(memAccess->target.get()))
        {
            if (idTarget->name == "self")
            {
                return "this->" + sanitizeName(memAccess->member);
            }
        }
        bool isDigit = true;
        for (char c : memAccess->member)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                isDigit = false;
                break;
            }
        }
        if (isDigit && !memAccess->member.empty())
        {
            return "std::get<" + memAccess->member + ">(" + genExpression(memAccess->target.get()) + ")";
        }

        return genExpression(memAccess->target.get()) + "." + sanitizeName(memAccess->member);
    }
    if (auto lambda = dynamic_cast<const LambdaNode *>(expr))
    {
        std::ostringstream out;
        bool isTaskRet = (lambda->returnType.rfind("Task<", 0) == 0);
        std::string innerTaskType = "void";
        if (isTaskRet && lambda->returnType.back() == '>')
        {
            std::string raw = lambda->returnType.substr(5, lambda->returnType.size() - 6);
            while (!raw.empty() && raw.front() == ' ') raw.erase(0, 1);
            while (!raw.empty() && raw.back() == ' ') raw.pop_back();
            innerTaskType = convertType(raw);
        }

        out << "[=](";
        for (size_t i = 0; i < lambda->params.size(); ++i)
        {
            out << convertType(lambda->params[i].type) << " " << sanitizeName(lambda->params[i].name);
            if (i + 1 < lambda->params.size())
                out << ", ";
        }
        out << ") ";

        if (!lambda->returnType.empty())
        {
            out << "-> " << convertType(lambda->returnType) << " ";
        }

        if (isTaskRet)
        {
            out << "{\n";
            out << getIndent(2) << "return std::async(std::launch::async, [=]() mutable -> " << innerTaskType << " {\n";
            if (lambda->isExpressionBody)
            {
                out << getIndent(3) << "return " << genExpression(lambda->exprBody.get()) << ";\n";
            }
            else
            {
                for (const auto &stmt : lambda->blockBody)
                {
                    out << genStatement(stmt.get(), 3);
                }
            }
            out << getIndent(2) << "});\n";
            out << getIndent(1) << "}";
        }
        else
        {
            if (lambda->isExpressionBody)
            {
                out << "{ return " << genExpression(lambda->exprBody.get()) << "; }";
            }
            else
            {
                out << "{\n";
                for (size_t i = 0; i < lambda->blockBody.size(); ++i)
                {
                    const auto &stmt = lambda->blockBody[i];
                    if (i == lambda->blockBody.size() - 1 && !lambda->returnType.empty() && lambda->returnType != "void")
                    {
                        if (auto exprStmt = dynamic_cast<const ExpressionStatementNode *>(stmt.get()))
                        {
                            out << getIndent(2) << "return " << genExpression(exprStmt->expr.get()) << ";\n";
                            continue;
                        }
                    }
                    out << genStatement(stmt.get(), 2);
                }
                out << "    }";
            }
        }
        return out.str();
    }
    if (auto fnCall = dynamic_cast<const FunctionCallNode *>(expr))
    {
        if (auto idCallee = dynamic_cast<const IdentifierNode *>(fnCall->callee.get()))
        {
            if (idCallee->name == "Tuple")
            {
                std::ostringstream out;
                out << "std::make_tuple(";
                for (size_t i = 0; i < fnCall->args.size(); ++i)
                {
                    out << genExpression(fnCall->args[i].get());
                    if (i + 1 < fnCall->args.size())
                        out << ", ";
                }
                out << ")";
                return out.str();
            }
        }
        std::ostringstream out;
        out << genExpression(fnCall->callee.get()) << "(";
        for (size_t i = 0; i < fnCall->args.size(); ++i)
        {
            out << genExpression(fnCall->args[i].get());
            if (i + 1 < fnCall->args.size())
            {
                out << ", ";
            }
        }
        out << ")";
        return out.str();
    }
    if (auto arrLit = dynamic_cast<const ArrayLiteralNode *>(expr))
    {
        needsVector = true;
        std::ostringstream out;
        out << "std::vector{";
        for (size_t i = 0; i < arrLit->elements.size(); ++i)
        {
            out << genExpression(arrLit->elements[i].get());
            if (i + 1 < arrLit->elements.size())
            {
                out << ", ";
            }
        }
        out << "}";
        return out.str();
    }
    if (auto mapLit = dynamic_cast<const MapLiteralNode *>(expr))
    {
        needsMap = true;
        std::ostringstream out;
        out << "{";
        for (size_t i = 0; i < mapLit->entries.size(); ++i)
        {
            out << "{" << genExpression(mapLit->entries[i].first.get()) << ", "
                << genExpression(mapLit->entries[i].second.get()) << "}";
            if (i + 1 < mapLit->entries.size())
                out << ", ";
        }
        out << "}";
        return out.str();
    }
    if (auto idxAccess = dynamic_cast<const IndexAccessNode *>(expr))
    {
        std::ostringstream out;
        out << genExpression(idxAccess->target.get());
        for (const auto &idx : idxAccess->indices)
        {
            out << "[" << genExpression(idx.get()) << "]";
        }
        return out.str();
    }
    if (auto unOp = dynamic_cast<const UnaryOpNode *>(expr))
    {
        return unOp->op + genExpression(unOp->right.get());
    }
    if (auto binOp = dynamic_cast<const BinaryOpNode *>(expr))
    {
        return genExpression(binOp->left.get()) + " " + binOp->op + " " + genExpression(binOp->right.get());
    }
    return "";
}

std::string CodeGen::genStatement(const StatementNode *stmt, int indentLevel)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    if (auto varDecl = dynamic_cast<const VariableDeclNode *>(stmt))
    {
        std::string cppType = convertType(varDecl->type);
        if (varDecl->isComptime)
        {
            out << ind << "constexpr auto " << sanitizeName(varDecl->name) << " = " << genExpression(varDecl->value.get()) << ";\n";
        }
        else
        {
            bool isFutureType = (varDecl->type.rfind("Task<", 0) == 0 || cppType.rfind("std::future", 0) == 0);
            std::string prefix = (varDecl->isMutable || isFutureType) ? "" : "const ";
            out << ind << prefix << cppType << " " << sanitizeName(varDecl->name) << " = " << genExpression(varDecl->value.get()) << ";\n";
        }
        return out.str();
    }

    if (auto assign = dynamic_cast<const AssignmentNode *>(stmt))
    {
        out << ind << genExpression(assign->target.get()) << " = " << genExpression(assign->value.get()) << ";\n";
        return out.str();
    }

    if (auto incDec = dynamic_cast<const IncDecNode *>(stmt))
    {
        out << ind << sanitizeName(incDec->name) << incDec->op << ";\n";
        return out.str();
    }

    if (auto ret = dynamic_cast<const ReturnNode *>(stmt))
    {
        if (ret->value)
        {
            out << ind << "return " << genExpression(ret->value.get()) << ";\n";
        }
        else
        {
            out << ind << "return;\n";
        }
        return out.str();
    }

    if (auto exprStmt = dynamic_cast<const ExpressionStatementNode *>(stmt))
    {
        out << ind << genExpression(exprStmt->expr.get()) << ";\n";
        return out.str();
    }

    if (auto printNode = dynamic_cast<const PrintNode *>(stmt))
    {
        needsIOStream = true;
        out << ind << "std::cout << " << genExpression(printNode->argument.get());
        if (printNode->isPrintln)
        {
            out << " << std::endl;\n";
        }
        else
        {
            out << ";\n";
        }
        return out.str();
    }

    if (auto assertNode = dynamic_cast<const AssertNode *>(stmt))
    {
        out << ind << "assert(" << genExpression(assertNode->condition.get()) << ");\n";
        return out.str();
    }

    if (auto eqNode = dynamic_cast<const AssertEqNode *>(stmt))
    {
        out << ind << "assert((" << genExpression(eqNode->left.get()) << ") == (" << genExpression(eqNode->right.get()) << "));\n";
        return out.str();
    }

    if (auto neNode = dynamic_cast<const AssertNeNode *>(stmt))
    {
        out << ind << "assert((" << genExpression(neNode->left.get()) << ") != (" << genExpression(neNode->right.get()) << "));\n";
        return out.str();
    }

    if (dynamic_cast<const BreakNode *>(stmt))
    {
        return ind + "break;\n";
    }

    if (dynamic_cast<const ContinueNode *>(stmt))
    {
        return ind + "continue;\n";
    }

    if (auto ifNode = dynamic_cast<const IfNode *>(stmt))
    {
        out << ind << "if (" << genExpression(ifNode->condition.get()) << ") {\n";
        for (const auto &s : ifNode->thenBody)
        {
            out << genStatement(s.get(), indentLevel + 1);
        }
        out << ind << "}";

        for (const auto &branch : ifNode->elseIfBranches)
        {
            out << " else if (" << genExpression(branch.condition.get()) << ") {\n";
            for (const auto &s : branch.body)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}";
        }

        if (!ifNode->elseBody.empty())
        {
            out << " else {\n";
            for (const auto &s : ifNode->elseBody)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}";
        }
        out << "\n";
        return out.str();
    }

    if (auto matchNode = dynamic_cast<const MatchNode *>(stmt))
    {
        static int matchVarCounter = 0;
        std::string tempVar = "__match_val_" + std::to_string(matchVarCounter++);
        out << ind << "{\n";
        out << ind << "    auto " << tempVar << " = " << genExpression(matchNode->target.get()) << ";\n";
        bool first = true;
        for (const auto &arm : matchNode->arms)
        {
            if (arm.isWildcard)
            {
                if (first)
                {
                    out << ind << "    {\n";
                }
                else
                {
                    out << " else {\n";
                }
                for (const auto &s : arm.body)
                {
                    out << genStatement(s.get(), indentLevel + 2);
                }
                out << ind << "    }";
            }
            else
            {
                std::ostringstream condStream;
                std::string bindDecl = "";
                std::string boundName = "";

                for (size_t pIdx = 0; pIdx < arm.patterns.size(); ++pIdx)
                {
                    const auto *pExpr = arm.patterns[pIdx].get();
                    if (!pExpr)
                        continue;
                    if (pIdx > 0)
                        condStream << " || ";

                    if (auto fnCall = dynamic_cast<const FunctionCallNode *>(pExpr))
                    {
                        if (auto calleeId = dynamic_cast<const IdentifierNode *>(fnCall->callee.get()))
                        {
                            if (calleeId->name == "Ok" && fnCall->args.size() == 1)
                            {
                                condStream << tempVar << ".is_ok";
                                if (auto bindId = dynamic_cast<const IdentifierNode *>(fnCall->args[0].get()))
                                {
                                    if (bindId->name != "_")
                                    {
                                        bindDecl = getIndent(indentLevel + 2) + "auto " + sanitizeName(bindId->name) + " = " + tempVar + ".ok_val;\n";
                                    }
                                }
                                continue;
                            }
                            else if (calleeId->name == "Err" && fnCall->args.size() == 1)
                            {
                                condStream << "!" << tempVar << ".is_ok";
                                if (auto bindId = dynamic_cast<const IdentifierNode *>(fnCall->args[0].get()))
                                {
                                    if (bindId->name != "_")
                                    {
                                        bindDecl = getIndent(indentLevel + 2) + "auto " + sanitizeName(bindId->name) + " = " + tempVar + ".err_val;\n";
                                    }
                                }
                                continue;
                            }
                        }
                    }

                    if (auto idNode = dynamic_cast<const IdentifierNode *>(pExpr))
                    {
                        if (idNode->name == "_")
                        {
                            condStream << "true";
                            continue;
                        }
                        if (arm.guard && arm.patterns.size() == 1)
                        {
                            boundName = idNode->name;
                            bindDecl = getIndent(indentLevel + 2) + "auto " + sanitizeName(idNode->name) + " = " + tempVar + ";\n";
                            condStream << "true";
                            continue;
                        }
                    }

                    std::string rhs = genExpression(pExpr);
                    if (!rhs.empty())
                    {
                        condStream << tempVar << " == " << rhs;
                    }
                    else
                    {
                        condStream << "true";
                    }
                }

                std::string condExpr = condStream.str();
                if (arm.guard)
                {
                    if (!boundName.empty())
                    {
                        currentBoundVar = boundName;
                        currentBoundVal = tempVar;
                    }
                    std::string guardStr = genExpression(arm.guard.get());
                    currentBoundVar = "";
                    currentBoundVal = "";

                    if (condExpr == "true")
                    {
                        condExpr = guardStr;
                    }
                    else
                    {
                        condExpr = "(" + condExpr + ") && (" + guardStr + ")";
                    }
                }

                if (first)
                {
                    out << ind << "    if (" << condExpr << ") {\n";
                    first = false;
                }
                else
                {
                    out << " else if (" << condExpr << ") {\n";
                }

                if (!bindDecl.empty())
                {
                    out << bindDecl;
                }

                for (const auto &s : arm.body)
                {
                    out << genStatement(s.get(), indentLevel + 2);
                }
                out << ind << "    }";
            }
        }
        if (!matchNode->arms.empty())
        {
            out << "\n";
        }
        out << ind << "}\n";
        return out.str();
    }

    if (auto forNode = dynamic_cast<const ForNode *>(stmt))
    {
        if (forNode->kind == ForKind::FOR_RANGE)
        {
            std::string varName;
            if (forNode->iteratorVar.front() == '[' && forNode->iteratorVar.back() == ']')
            {
                varName = forNode->iteratorVar;
            }
            else
            {
                varName = forNode->iteratorVar == "_" ? "__unused" : sanitizeName(forNode->iteratorVar);
            }

            out << ind << "for (auto&& " << varName << " : " << genExpression(forNode->iterable.get()) << ") {\n";
            for (const auto &s : forNode->body)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}\n";
            return out.str();
        }

        if (forNode->kind == ForKind::INFINITE)
        {
            out << ind << "while (true) {\n";
            for (const auto &s : forNode->body)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}\n";
            return out.str();
        }

        if (forNode->kind == ForKind::CONDITIONAL)
        {
            out << ind << "while (" << genExpression(forNode->condition.get()) << ") {\n";
            for (const auto &s : forNode->body)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}\n";
            return out.str();
        }

        if (forNode->kind == ForKind::CONTROLLED)
        {
            std::string initStr = "";
            if (forNode->init)
            {
                if (auto vd = dynamic_cast<const VariableDeclNode *>(forNode->init.get()))
                {
                    std::string prefix = vd->isMutable ? "" : "const ";
                    initStr = prefix + convertType(vd->type) + " " + sanitizeName(vd->name) + " = " + genExpression(vd->value.get());
                }
                else if (auto as = dynamic_cast<const AssignmentNode *>(forNode->init.get()))
                {
                    initStr = genExpression(as->target.get()) + " = " + genExpression(as->value.get());
                }
            }

            std::string condStr = forNode->condition ? genExpression(forNode->condition.get()) : "";

            std::string incStr = "";
            if (forNode->increment)
            {
                if (auto as = dynamic_cast<const AssignmentNode *>(forNode->increment.get()))
                {
                    incStr = genExpression(as->target.get()) + " = " + genExpression(as->value.get());
                }
                else if (auto id = dynamic_cast<const IncDecNode *>(forNode->increment.get()))
                {
                    incStr = sanitizeName(id->name) + id->op;
                }
            }

            out << ind << "for (" << initStr << "; " << condStr << "; " << incStr << ") {\n";
            for (const auto &s : forNode->body)
            {
                out << genStatement(s.get(), indentLevel + 1);
            }
            out << ind << "}\n";
            return out.str();
        }
    }

    return "";
}

std::string CodeGen::genFunction(const FunctionNode *fn, int indentLevel, const std::string &enclosingRecord, bool isVirtualOverride)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);
    std::string retType = (fn->name == "main") ? "int" : convertType(fn->returnType);

    bool hasSelf = (!fn->params.empty() && fn->params[0].name == "self");

    if (fn->isComptime)
    {
        out << ind << "constexpr ";
    }
    else if (!enclosingRecord.empty() && !hasSelf)
    {
        out << ind << "static ";
    }
    else
    {
        out << ind;
    }

    std::string fnName = (fn->name == "main") ? "main" : sanitizeName(fn->name);

    if (fn->isAsync && fn->name != "main")
    {
        std::string innerType = "void";
        if (fn->returnType.rfind("Task<", 0) == 0 && fn->returnType.back() == '>')
        {
            std::string rawInner = fn->returnType.substr(5, fn->returnType.size() - 6);
            while (!rawInner.empty() && rawInner.front() == ' ') rawInner.erase(0, 1);
            while (!rawInner.empty() && rawInner.back() == ' ') rawInner.pop_back();
            innerType = convertType(rawInner);
        }

        out << retType << " " << fnName << "(";
        size_t startParam = hasSelf ? 1 : 0;
        for (size_t i = startParam; i < fn->params.size(); ++i)
        {
            out << convertType(fn->params[i].type) << " " << sanitizeName(fn->params[i].name);
            if (i + 1 < fn->params.size())
            {
                out << ", ";
            }
        }
        out << ") {\n";
        out << ind << "    return std::async(std::launch::async, [=]() mutable -> " << innerType << " {\n";
        for (const auto &stmt : fn->body)
        {
            out << genStatement(stmt.get(), indentLevel + 2);
        }
        out << ind << "    });\n";
        out << ind << "}\n";
        return out.str();
    }

    out << retType << " " << fnName << "(";

    size_t startParam = hasSelf ? 1 : 0;
    for (size_t i = startParam; i < fn->params.size(); ++i)
    {
        out << convertType(fn->params[i].type) << " " << sanitizeName(fn->params[i].name);
        if (i + 1 < fn->params.size())
        {
            out << ", ";
        }
    }
    out << ")";

    if (isVirtualOverride)
    {
        out << " override";
    }

    out << " {\n";

    bool hasExplicitReturn = false;
    for (const auto &stmt : fn->body)
    {
        if (dynamic_cast<const ReturnNode *>(stmt.get()))
        {
            hasExplicitReturn = true;
        }
        out << genStatement(stmt.get(), indentLevel + 1);
    }

    if (fn->name == "main" && !hasExplicitReturn)
    {
        out << ind << "    return 0;\n";
    }
    out << ind << "}\n";

    return out.str();
}

std::string CodeGen::genEnumDefinition(const EnumNode *en, int indentLevel)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    out << ind << "enum class " << sanitizeName(en->name);
    if (!en->underlyingType.empty())
    {
        out << " : " << convertType(en->underlyingType);
    }
    out << " {\n";

    for (size_t i = 0; i < en->variants.size(); ++i)
    {
        const auto &v = en->variants[i];
        out << ind << "    " << sanitizeName(v.name);
        if (!v.value.empty())
        {
            out << " = " << v.value;
        }
        if (i + 1 < en->variants.size())
        {
            out << ",";
        }
        out << "\n";
    }

    out << ind << "};\n";
    return out.str();
}

std::string CodeGen::genTraitDefinition(const TraitNode *tr, int indentLevel)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    out << ind << "struct " << sanitizeName(tr->name) << " {\n";
    out << ind << "    virtual ~" << sanitizeName(tr->name) << "() = default;\n";

    for (const auto &m : tr->methods)
    {
        out << ind << "    virtual " << convertType(m.returnType) << " " << sanitizeName(m.name) << "(";
        bool hasSelf = (!m.params.empty() && m.params[0].name == "self");
        size_t startParam = hasSelf ? 1 : 0;
        for (size_t i = startParam; i < m.params.size(); ++i)
        {
            out << convertType(m.params[i].type) << " " << sanitizeName(m.params[i].name);
            if (i + 1 < m.params.size())
                out << ", ";
        }
        out << ") = 0;\n";
    }

    out << ind << "};\n";
    return out.str();
}

std::string CodeGen::genRecordDefinition(const RecordNode *rec, int indentLevel)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    out << ind << "struct " << sanitizeName(rec->name);

    auto trIt = recordTraitsMap.find(rec->name);
    if (trIt != recordTraitsMap.end() && !trIt->second.empty())
    {
        out << " : ";
        for (size_t i = 0; i < trIt->second.size(); ++i)
        {
            out << "public " << sanitizeName(trIt->second[i]);
            if (i + 1 < trIt->second.size())
                out << ", ";
        }
    }

    out << " {\n";

    std::vector<RecordField> privFields;
    std::vector<RecordField> pubFields;

    for (const auto &f : rec->fields)
    {
        if (f.visibility == Visibility::PUBLIC)
            pubFields.push_back(f);
        else
            privFields.push_back(f);
    }

    auto resolveFieldType = [&](const std::string &t) -> std::string {
        if (t == rec->name) {
            return "std::shared_ptr<" + sanitizeName(rec->name) + ">";
        }
        return convertType(t);
    };

    if (!privFields.empty())
    {
        out << ind << "private:\n";
        for (const auto &f : privFields)
        {
            out << ind << "    " << resolveFieldType(f.type) << " " << sanitizeName(f.name) << ";\n";
        }
    }

    out << ind << "public:\n";
    for (const auto &f : pubFields)
    {
        out << ind << "    " << resolveFieldType(f.type) << " " << sanitizeName(f.name) << ";\n";
    }

    out << ind << "    " << sanitizeName(rec->name) << "() = default;\n";

    if (!rec->fields.empty())
    {
        out << ind << "    " << sanitizeName(rec->name) << "(";
        for (size_t i = 0; i < rec->fields.size(); ++i)
        {
            out << resolveFieldType(rec->fields[i].type) << " " << sanitizeName(rec->fields[i].name);
            if (i + 1 < rec->fields.size())
                out << ", ";
        }
        out << ") : ";
        for (size_t i = 0; i < rec->fields.size(); ++i)
        {
            std::string fName = sanitizeName(rec->fields[i].name);
            out << fName << "(" << fName << ")";
            if (i + 1 < rec->fields.size())
                out << ", ";
        }
        out << " {}\n";

        bool hasRecursiveField = false;
        for (const auto &f : rec->fields) {
            if (f.type == rec->name) {
                hasRecursiveField = true;
                break;
            }
        }

        if (hasRecursiveField) {
            out << ind << "    " << sanitizeName(rec->name) << "(";
            for (size_t i = 0; i < rec->fields.size(); ++i)
            {
                if (rec->fields[i].type == rec->name) {
                    out << "const " << sanitizeName(rec->name) << "& " << sanitizeName(rec->fields[i].name);
                } else {
                    out << convertType(rec->fields[i].type) << " " << sanitizeName(rec->fields[i].name);
                }
                if (i + 1 < rec->fields.size())
                    out << ", ";
            }
            out << ") : ";
            for (size_t i = 0; i < rec->fields.size(); ++i)
            {
                std::string fName = sanitizeName(rec->fields[i].name);
                if (rec->fields[i].type == rec->name) {
                    out << fName << "(std::make_shared<" << sanitizeName(rec->name) << ">(" << fName << "))";
                } else {
                    out << fName << "(" << fName << ")";
                }
                if (i + 1 < rec->fields.size())
                    out << ", ";
            }
            out << " {}\n";
        }
    }

    out << "\n"
        << ind << "    std::string to_json() const {\n";
    out << ind << "        std::ostringstream __ss;\n";
    out << ind << "        __ss << \"{\";\n";
    for (size_t i = 0; i < rec->fields.size(); ++i)
    {
        std::string fName = sanitizeName(rec->fields[i].name);
        std::string fType = rec->fields[i].type;
        if (i > 0)
            out << ind << "        __ss << \", \";\n";
        out << ind << "        __ss << \"\\\"" << fName << "\\\": \";\n";
        if (fType == rec->name)
        {
            out << ind << "        if (" << fName << ") __ss << " << fName << "->to_json(); else __ss << \"null\";\n";
        }
        else if (fType == "string")
        {
            out << ind << "        __ss << \"\\\"\" << " << fName << " << \"\\\"\";\n";
        }
        else if (fType == "bool")
        {
            out << ind << "        __ss << (" << fName << " ? \"true\" : \"false\");\n";
        }
        else if (enumMap.find(fType) != enumMap.end())
        {
            out << ind << "        __ss << static_cast<long long>(" << fName << ");\n";
        }
        else
        {
            out << ind << "        __ss << " << fName << ";\n";
        }
    }
    out << ind << "        __ss << \"}\";\n";
    out << ind << "        return __ss.str();\n";
    out << ind << "    }\n";

    out << "\n"
        << ind << "    friend std::ostream& operator<<(std::ostream& os, const " << sanitizeName(rec->name) << "& obj) {\n";
    out << ind << "        os << obj.to_json();\n";
    out << ind << "        return os;\n";
    out << ind << "    }\n";

    auto it = implMethodsMap.find(rec->name);
    if (it != implMethodsMap.end())
    {
        out << "\n";
        for (const auto *fn : it->second)
        {
            bool isVirtual = false;
            if (trIt != recordTraitsMap.end())
            {
                for (const auto &traitName : trIt->second)
                {
                    auto trEntry = traitMap.find(traitName);
                    if (trEntry != traitMap.end())
                    {
                        for (const auto &tm : trEntry->second->methods)
                        {
                            if (tm.name == fn->name)
                            {
                                isVirtual = true;
                                break;
                            }
                        }
                    }
                }
            }
            out << genFunction(fn, indentLevel + 1, rec->name, isVirtual) << "\n";
        }
    }

    out << ind << "};\n";
    return out.str();
}

std::string CodeGen::genMod(const ModNode *mod, int indentLevel)
{
    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    out << ind << "class " << sanitizeName(mod->name) << " {\n";

    std::vector<const ASTNode *> privTypes;
    std::vector<const ASTNode *> pubTypes;
    std::vector<const FunctionNode *> privFuncs;
    std::vector<const FunctionNode *> pubFuncs;
    std::vector<const ASTNode *> privOthers;
    std::vector<const ASTNode *> pubOthers;

    for (const auto &member : mod->members)
    {
        if (auto tr = dynamic_cast<const TraitNode *>(member.get()))
        {
            if (tr->visibility == Visibility::PUBLIC) pubTypes.push_back(tr);
            else privTypes.push_back(tr);
        }
        else if (auto rec = dynamic_cast<const RecordNode *>(member.get()))
        {
            if (rec->visibility == Visibility::PUBLIC) pubTypes.push_back(rec);
            else privTypes.push_back(rec);
        }
        else if (auto en = dynamic_cast<const EnumNode *>(member.get()))
        {
            if (en->visibility == Visibility::PUBLIC) pubTypes.push_back(en);
            else privTypes.push_back(en);
        }
        else if (auto fn = dynamic_cast<const FunctionNode *>(member.get()))
        {
            if (fn->visibility == Visibility::PUBLIC) pubFuncs.push_back(fn);
            else privFuncs.push_back(fn);
        }
        else if (auto subMod = dynamic_cast<const ModNode *>(member.get()))
        {
            if (subMod->visibility == Visibility::PUBLIC) pubOthers.push_back(subMod);
            else privOthers.push_back(subMod);
        }
        else
        {
            privOthers.push_back(member.get());
        }
    }

    out << ind << "public:\n";
    out << ind << "    " << sanitizeName(mod->name) << "() = delete;\n\n";

    for (const auto *t : pubTypes)
    {
        if (auto tr = dynamic_cast<const TraitNode *>(t))
        {
            out << genTraitDefinition(tr, indentLevel + 1) << "\n";
        }
        else if (auto rec = dynamic_cast<const RecordNode *>(t))
        {
            out << genRecordDefinition(rec, indentLevel + 1) << "\n";
        }
        else if (auto en = dynamic_cast<const EnumNode *>(t))
        {
            out << genEnumDefinition(en, indentLevel + 1) << "\n";
        }
    }

    if (!privTypes.empty() || !privFuncs.empty() || !privOthers.empty())
    {
        out << ind << "private:\n";
        for (const auto *t : privTypes)
        {
            if (auto tr = dynamic_cast<const TraitNode *>(t))
            {
                out << genTraitDefinition(tr, indentLevel + 1) << "\n";
            }
            else if (auto rec = dynamic_cast<const RecordNode *>(t))
            {
                out << genRecordDefinition(rec, indentLevel + 1) << "\n";
            }
            else if (auto en = dynamic_cast<const EnumNode *>(t))
            {
                out << genEnumDefinition(en, indentLevel + 1) << "\n";
            }
        }
        for (const auto *f : privFuncs)
        {
            out << genFunction(f, indentLevel + 1, mod->name) << "\n";
        }
        for (const auto *o : privOthers)
        {
            if (auto subMod = dynamic_cast<const ModNode *>(o))
            {
                out << genMod(subMod, indentLevel + 1) << "\n";
            }
            else if (auto stmt = dynamic_cast<const StatementNode *>(o))
            {
                out << genStatement(stmt, indentLevel + 1);
            }
        }
    }

    if (!pubFuncs.empty() || !pubOthers.empty())
    {
        out << ind << "public:\n";
        for (const auto *f : pubFuncs)
        {
            out << genFunction(f, indentLevel + 1, mod->name) << "\n";
        }
        for (const auto *o : pubOthers)
        {
            if (auto subMod = dynamic_cast<const ModNode *>(o))
            {
                out << genMod(subMod, indentLevel + 1) << "\n";
            }
            else if (auto stmt = dynamic_cast<const StatementNode *>(o))
            {
                out << genStatement(stmt, indentLevel + 1);
            }
        }
    }

    out << ind << "};\n";
    return out.str();
}

std::string CodeGen::genPackage(const PackageNode *pkg, int indentLevel)
{
    if (pkg->name == "main")
    {
        std::ostringstream out;
        for (const auto &member : pkg->members)
        {
            if (auto tr = dynamic_cast<const TraitNode *>(member.get()))
            {
                out << genTraitDefinition(tr, indentLevel) << "\n";
            }
            else if (auto rec = dynamic_cast<const RecordNode *>(member.get()))
            {
                out << genRecordDefinition(rec, indentLevel) << "\n";
            }
            else if (auto en = dynamic_cast<const EnumNode *>(member.get()))
            {
                out << genEnumDefinition(en, indentLevel) << "\n";
            }
            else if (auto fn = dynamic_cast<const FunctionNode *>(member.get()))
            {
                out << genFunction(fn, indentLevel) << "\n";
            }
            else if (auto subPkg = dynamic_cast<const PackageNode *>(member.get()))
            {
                out << genPackage(subPkg, indentLevel) << "\n";
            }
            else if (auto subMod = dynamic_cast<const ModNode *>(member.get()))
            {
                out << genMod(subMod, indentLevel) << "\n";
            }
            else if (auto stmt = dynamic_cast<const StatementNode *>(member.get()))
            {
                out << genStatement(stmt, indentLevel);
            }
        }
        return out.str();
    }

    std::ostringstream out;
    std::string ind = getIndent(indentLevel);

    out << ind << "namespace " << sanitizeName(pkg->name) << " {\n";
    for (const auto &member : pkg->members)
    {
        if (auto tr = dynamic_cast<const TraitNode *>(member.get()))
        {
            out << genTraitDefinition(tr, indentLevel + 1) << "\n";
        }
        else if (auto rec = dynamic_cast<const RecordNode *>(member.get()))
        {
            out << genRecordDefinition(rec, indentLevel + 1) << "\n";
        }
        else if (auto en = dynamic_cast<const EnumNode *>(member.get()))
        {
            out << genEnumDefinition(en, indentLevel + 1) << "\n";
        }
        else if (auto fn = dynamic_cast<const FunctionNode *>(member.get()))
        {
            out << genFunction(fn, indentLevel + 1) << "\n";
        }
        else if (auto subPkg = dynamic_cast<const PackageNode *>(member.get()))
        {
            out << genPackage(subPkg, indentLevel + 1) << "\n";
        }
        else if (auto subMod = dynamic_cast<const ModNode *>(member.get()))
        {
            out << genMod(subMod, indentLevel + 1) << "\n";
        }
        else if (auto stmt = dynamic_cast<const StatementNode *>(member.get()))
        {
            out << genStatement(stmt, indentLevel + 1);
        }
    }
    out << ind << "}\n";

    return out.str();
}

std::string CodeGen::generate()
{
    if (!root)
        return "";

    std::ostringstream customIncludes;
    std::unordered_set<std::string> importedPackages;

    for (const auto &imp : root->imports)
    {
        if (imp->kind == ImportKind::C_HEADER)
        {
            customIncludes << "extern \"C\" {\n";
            customIncludes << "    #include \"" << imp->target << "\"\n";
            customIncludes << "}\n";
        }
        else if (imp->kind == ImportKind::CPP_SYS_HEADER)
        {
            customIncludes << "#include <" << imp->target << ">\n";
        }
        else if (imp->kind == ImportKind::CPP_USER_HEADER)
        {
            customIncludes << "#include \"" << imp->target << "\"\n";
        }
        else if (imp->kind == ImportKind::PACKAGE)
        {
            if (imp->target != "std" && imp->target != "main")
            {
                importedPackages.insert(imp->target);
            }
        }
    }

    for (const auto &pkg : root->packages)
    {
        if (pkg->name != "main" && pkg->name != "std")
        {
            importedPackages.insert(pkg->name);
        }
    }

    std::ostringstream enumCode;
    for (const auto &en : root->enums)
    {
        enumCode << genEnumDefinition(en.get(), 0) << "\n";
    }

    std::ostringstream traitForwardDecls;
    for (const auto &tr : root->traits)
    {
        traitForwardDecls << "struct " << sanitizeName(tr->name) << ";\n";
    }

    std::ostringstream traitsCode;
    for (const auto &tr : root->traits)
    {
        traitsCode << genTraitDefinition(tr.get(), 0) << "\n";
    }

    std::ostringstream recordForwardDecls;
    for (const auto &rec : root->records)
    {
        recordForwardDecls << "struct " << sanitizeName(rec->name) << ";\n";
    }

    std::ostringstream recordsCode;
    for (const auto &rec : root->records)
    {
        recordsCode << genRecordDefinition(rec.get(), 0) << "\n";
    }

    std::ostringstream modulesCode;
    for (const auto &m : root->modules)
    {
        modulesCode << genMod(m.get(), 0) << "\n";
    }

    std::ostringstream packagesCode;
    for (const auto &pkg : root->packages)
    {
        packagesCode << genPackage(pkg.get(), 0) << "\n";
    }

    std::ostringstream prototypes;
    std::ostringstream functionsCode;

    bool hasUserMain = false;
    for (const auto &fn : root->functions)
    {
        if (fn->name == "main")
        {
            hasUserMain = true;
        }
        else if (!fn->isComptime)
        {
            prototypes << convertType(fn->returnType) << " " << sanitizeName(fn->name) << "(";
            size_t startParam = (!fn->params.empty() && fn->params[0].name == "self") ? 1 : 0;
            for (size_t i = startParam; i < fn->params.size(); ++i)
            {
                prototypes << convertType(fn->params[i].type) << " " << sanitizeName(fn->params[i].name);
                if (i + 1 < fn->params.size())
                {
                    prototypes << ", ";
                }
            }
            prototypes << ");\n";
        }
        functionsCode << genFunction(fn.get(), 0) << "\n";
    }

    for (const auto &pkg : root->packages)
    {
        for (const auto &member : pkg->members)
        {
            if (auto fn = dynamic_cast<const FunctionNode *>(member.get()))
            {
                if (fn->name == "main")
                {
                    hasUserMain = true;
                }
            }
        }
    }

    for (const auto &mod : root->modules)
    {
        for (const auto &member : mod->members)
        {
            if (auto fn = dynamic_cast<const FunctionNode *>(member.get()))
            {
                if (fn->name == "main")
                {
                    hasUserMain = true;
                }
            }
        }
    }

    std::ostringstream testFunctions;
    std::ostringstream testRunnerMain;

    if (!root->tests.empty())
    {
        testRunnerMain << "int main() {\n";
        testRunnerMain << "    std::cout << \"\\n\\033[1;36mrunning \" << " << root->tests.size() << " << \" test(s)\\033[0m\\n\" << std::endl;\n";
        testRunnerMain << "    auto __suite_start = std::chrono::high_resolution_clock::now();\n";
        testRunnerMain << "    int __passed_count = 0;\n";

        for (size_t i = 0; i < root->tests.size(); ++i)
        {
            const auto &t = root->tests[i];
            std::string tFn = "__zipc_test_" + std::to_string(i);

            testFunctions << "void " << tFn << "() {\n";
            for (const auto &s : t->body)
            {
                testFunctions << genStatement(s.get(), 1);
            }
            testFunctions << "}\n\n";

            testRunnerMain << "    {\n";
            testRunnerMain << "        std::cout << \"test \" << \"" << t->name << "\" << \" ... \" << std::flush;\n";
            testRunnerMain << "        auto __t_start = std::chrono::high_resolution_clock::now();\n";
            testRunnerMain << "        " << tFn << "();\n";
            testRunnerMain << "        auto __t_end = std::chrono::high_resolution_clock::now();\n";
            testRunnerMain << "        double __dur = std::chrono::duration<double, std::milli>(__t_end - __t_start).count();\n";
            testRunnerMain << "        std::cout << \"\\033[1;32mok\\033[0m \" << \"(\" << __dur << \" ms)\" << std::endl;\n";
            testRunnerMain << "        __passed_count++;\n";
            testRunnerMain << "    }\n";
        }

        testRunnerMain << "    auto __suite_end = std::chrono::high_resolution_clock::now();\n";
        testRunnerMain << "    double __total_dur = std::chrono::duration<double, std::milli>(__suite_end - __suite_start).count();\n";
        testRunnerMain << "    std::cout << \"\\ntest result: \\033[1;32mok\\033[0m. \" << __passed_count << \" passed; 0 failed; finished in \" << __total_dur << \" ms\\n\" << std::endl;\n";
        testRunnerMain << "    return 0;\n";
        testRunnerMain << "}\n";
    }
    else if (!hasUserMain)
    {
        testRunnerMain << "int main() {\n";
        testRunnerMain << "    std::cout << \"\\033[1;33m[Zync Runner]\033[0m No main() or TEST() block found in target source.\" << std::endl;\n";
        testRunnerMain << "    return 0;\n";
        testRunnerMain << "}\n";
    }

    std::ostringstream fullCode;
    fullCode << "#include <iostream>\n";
    fullCode << "#include <string>\n";
    fullCode << "#include <sstream>\n";
    fullCode << "#include <chrono>\n";
    fullCode << "#include <tuple>\n";
    fullCode << "#include <cassert>\n";
    fullCode << "#include <vector>\n";
    fullCode << "#include <set>\n";
    fullCode << "#include <future>\n";
    fullCode << "#include <type_traits>\n";
    if (needsMap)
        fullCode << "#include <map>\n";

    fullCode << "\nusing namespace std;\n\n";

    fullCode << "template <typename T>\n";
    fullCode << "using Task = std::future<T>;\n\n";

    fullCode << "template <typename T, typename E>\n";
    fullCode << "struct Result {\n";
    fullCode << "    bool is_ok;\n";
    fullCode << "    T ok_val;\n";
    fullCode << "    E err_val;\n";
    fullCode << "    Result() : is_ok(false), ok_val(T{}), err_val(E{}) {}\n";
    fullCode << "    static Result<T, E> createOk(const T& v) {\n";
    fullCode << "        Result<T, E> r;\n";
    fullCode << "        r.is_ok = true;\n";
    fullCode << "        r.ok_val = v;\n";
    fullCode << "        return r;\n";
    fullCode << "    }\n";
    fullCode << "    static Result<T, E> createErr(const E& e) {\n";
    fullCode << "        Result<T, E> r;\n";
    fullCode << "        r.is_ok = false;\n";
    fullCode << "        r.err_val = e;\n";
    fullCode << "        return r;\n";
    fullCode << "    }\n";
    fullCode << "};\n\n";

    fullCode << "template <typename T>\n";
    fullCode << "struct __Ok_Holder {\n";
    fullCode << "    T val;\n";
    fullCode << "    template <typename E>\n";
    fullCode << "    operator Result<T, E>() const {\n";
    fullCode << "        return Result<T, E>::createOk(val);\n";
    fullCode << "    }\n";
    fullCode << "};\n";
    fullCode << "template <typename T>\n";
    fullCode << "__Ok_Holder<T> Ok(const T& val) { return __Ok_Holder<T>{val}; }\n\n";

    fullCode << "template <typename E>\n";
    fullCode << "struct __Err_Holder {\n";
    fullCode << "    E err;\n";
    fullCode << "    template <typename T>\n";
    fullCode << "    operator Result<T, E>() const {\n";
    fullCode << "        return Result<T, E>::createErr(err);\n";
    fullCode << "    }\n";
    fullCode << "};\n";
    fullCode << "template <typename E>\n";
    fullCode << "__Err_Holder<E> Err(const E& err) { return __Err_Holder<E>{err}; }\n\n";

    fullCode << "template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::string> && !std::is_same_v<std::decay_t<T>, char*> && !std::is_same_v<std::decay_t<T>, const char*> && !std::is_same_v<std::decay_t<T>, char>>>\n";
    fullCode << "std::string operator+(const std::string& s, const T& v) {\n";
    fullCode << "    std::ostringstream ss; ss << s << v; return ss.str();\n";
    fullCode << "}\n";
    fullCode << "template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::string> && !std::is_same_v<std::decay_t<T>, char*> && !std::is_same_v<std::decay_t<T>, const char*> && !std::is_same_v<std::decay_t<T>, char>>>\n";
    fullCode << "std::string operator+(const T& v, const std::string& s) {\n";
    fullCode << "    std::ostringstream ss; ss << v << s; return ss.str();\n";
    fullCode << "}\n\n";

    if (!customIncludes.str().empty())
    {
        fullCode << customIncludes.str();
    }
    fullCode << "\n";

    if (!enumCode.str().empty())
    {
        fullCode << enumCode.str() << "\n";
    }

    if (!traitForwardDecls.str().empty())
    {
        fullCode << traitForwardDecls.str() << "\n";
    }

    if (!traitsCode.str().empty())
    {
        fullCode << traitsCode.str() << "\n";
    }

    if (!recordForwardDecls.str().empty())
    {
        fullCode << recordForwardDecls.str() << "\n";
    }

    if (!recordsCode.str().empty())
    {
        fullCode << recordsCode.str() << "\n";
    }

    if (!modulesCode.str().empty())
    {
        fullCode << modulesCode.str() << "\n";
    }

    if (!packagesCode.str().empty())
    {
        for (const auto &pkg : root->packages)
        {
            packagesCode << genPackage(pkg.get(), 0) << "\n";
        }
    }

    for (const auto &pkg : importedPackages)
    {
        fullCode << "using namespace " << sanitizeName(pkg) << ";\n";
    }
    fullCode << "\n";

    if (!prototypes.str().empty())
    {
        fullCode << prototypes.str() << "\n";
    }

    fullCode << functionsCode.str();

    if (!root->tests.empty())
    {
        fullCode << "\n"
                 << testFunctions.str();
    }

    if (!hasUserMain)
    {
        fullCode << "\n"
                 << testRunnerMain.str();
    }

    return fullCode.str();
}

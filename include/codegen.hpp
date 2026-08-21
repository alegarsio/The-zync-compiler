#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>

class CodeGen {
public:
    explicit CodeGen(const ProgramNode* program);
    std::string generate();

private:
    const ProgramNode* root;
    bool needsIOStream;
    bool needsString;
    bool needsVector;
    bool needsMap;

    std::string currentBoundVar;
    std::string currentBoundVal;

    std::unordered_map<std::string, const TraitNode*> traitMap;
    std::unordered_map<std::string, const RecordNode*> recordMap;
    std::unordered_map<std::string, std::vector<std::string>> recordTraitsMap;
    std::unordered_map<std::string, std::vector<const FunctionNode*>> implMethodsMap;

    std::string convertType(const std::string& zyType);
    std::string getIndent(int level);

    std::string genExpression(const ExpressionNode* expr);
    std::string genStatement(const StatementNode* stmt, int indentLevel);
    std::string genFunction(const FunctionNode* fn, int indentLevel, const std::string& enclosingRecord = "", bool isVirtualOverride = false);
    std::string genTraitDefinition(const TraitNode* tr, int indentLevel);
    std::string genRecordDefinition(const RecordNode* rec, int indentLevel);
    std::string genPackage(const PackageNode* pkg, int indentLevel);
};

#endif
#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <utility>

enum class LiteralType {
    STRING,
    CHAR,
    NUMBER,
    BOOLEAN
};

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct ExpressionNode : public ASTNode {};

struct LiteralNode : public ExpressionNode {
    std::string value;
    LiteralType litType;

    LiteralNode(const std::string& val, LiteralType type)
        : value(val), litType(type) {}
};

struct IdentifierNode : public ExpressionNode {
    std::string name;

    explicit IdentifierNode(const std::string& n) : name(n) {}
};

struct ScopedIdentifierNode : public ExpressionNode {
    std::vector<std::string> path;

    explicit ScopedIdentifierNode(std::vector<std::string> p) : path(std::move(p)) {}
};

struct MemberAccessNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> target;
    std::string member;

    MemberAccessNode(std::unique_ptr<ExpressionNode> t, const std::string& m)
        : target(std::move(t)), member(m) {}
};

struct FunctionCallNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> callee;
    std::vector<std::unique_ptr<ExpressionNode>> args;

    FunctionCallNode(std::unique_ptr<ExpressionNode> c, std::vector<std::unique_ptr<ExpressionNode>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct Parameter {
    std::string name;
    std::string type;
};

struct StatementNode : public ASTNode {};

struct ComptimeBlockExprNode : public ExpressionNode {
    std::vector<std::unique_ptr<StatementNode>> body;
};

struct LambdaNode : public ExpressionNode {
    std::vector<Parameter> params;
    std::string returnType;
    bool isExpressionBody;
    std::unique_ptr<ExpressionNode> exprBody;
    std::vector<std::unique_ptr<StatementNode>> blockBody;

    LambdaNode() : returnType(""), isExpressionBody(false) {}
};

struct ArrayLiteralNode : public ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
};

struct MapLiteralNode : public ExpressionNode {
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, std::unique_ptr<ExpressionNode>>> entries;
};

struct IndexAccessNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> target;
    std::vector<std::unique_ptr<ExpressionNode>> indices;

    IndexAccessNode(std::unique_ptr<ExpressionNode> t, std::vector<std::unique_ptr<ExpressionNode>> idxs)
        : target(std::move(t)), indices(std::move(idxs)) {}
};

struct UnaryOpNode : public ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> right;

    UnaryOpNode(const std::string& o, std::unique_ptr<ExpressionNode> r)
        : op(o), right(std::move(r)) {}
};

struct BinaryOpNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> left;
    std::string op;
    std::unique_ptr<ExpressionNode> right;

    BinaryOpNode(std::unique_ptr<ExpressionNode> l, const std::string& o, std::unique_ptr<ExpressionNode> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
};

struct MatchArm {
    bool isWildcard;
    std::vector<std::unique_ptr<ExpressionNode>> patterns;
    std::unique_ptr<ExpressionNode> guard;
    bool isExpressionBody;
    std::unique_ptr<ExpressionNode> exprBody;
    std::vector<std::unique_ptr<StatementNode>> body;

    MatchArm() : isWildcard(false), isExpressionBody(false) {}
};

struct MatchExprNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> target;
    std::vector<MatchArm> arms;
};

struct MatchNode : public StatementNode {
    std::unique_ptr<ExpressionNode> target;
    std::vector<MatchArm> arms;
};

struct VariableDeclNode : public StatementNode {
    std::string type;
    std::string name;
    std::unique_ptr<ExpressionNode> value;
    bool isComptime;

    VariableDeclNode(const std::string& t, const std::string& n, std::unique_ptr<ExpressionNode> v, bool comptime = false)
        : type(t), name(n), value(std::move(v)), isComptime(comptime) {}
};

struct AssignmentNode : public StatementNode {
    std::unique_ptr<ExpressionNode> target;
    std::unique_ptr<ExpressionNode> value;

    AssignmentNode(std::unique_ptr<ExpressionNode> t, std::unique_ptr<ExpressionNode> v)
        : target(std::move(t)), value(std::move(v)) {}
};

struct IncDecNode : public StatementNode {
    std::string name;
    std::string op;

    IncDecNode(const std::string& n, const std::string& o)
        : name(n), op(o) {}
};

struct ReturnNode : public StatementNode {
    std::unique_ptr<ExpressionNode> value;

    explicit ReturnNode(std::unique_ptr<ExpressionNode> v)
        : value(std::move(v)) {}
};

struct ExpressionStatementNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;

    explicit ExpressionStatementNode(std::unique_ptr<ExpressionNode> e)
        : expr(std::move(e)) {}
};

struct PrintNode : public StatementNode {
    bool isPrintln;
    std::unique_ptr<ExpressionNode> argument;

    PrintNode(bool println, std::unique_ptr<ExpressionNode> arg)
        : isPrintln(println), argument(std::move(arg)) {}
};

struct AssertNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;

    explicit AssertNode(std::unique_ptr<ExpressionNode> cond)
        : condition(std::move(cond)) {}
};

struct AssertEqNode : public StatementNode {
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;

    AssertEqNode(std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : left(std::move(l)), right(std::move(r)) {}
};

struct AssertNeNode : public StatementNode {
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;

    AssertNeNode(std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r)
        : left(std::move(l)), right(std::move(r)) {}
};

struct BreakNode : public StatementNode {};
struct ContinueNode : public StatementNode {};

enum class ForKind {
    INFINITE,
    CONDITIONAL,
    CONTROLLED
};

struct ForNode : public StatementNode {
    ForKind kind;
    std::unique_ptr<StatementNode> init;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> increment;
    std::vector<std::unique_ptr<StatementNode>> body;
};

struct IfBranch {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StatementNode>> body;
};

struct IfNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StatementNode>> thenBody;
    std::vector<IfBranch> elseIfBranches;
    std::vector<std::unique_ptr<StatementNode>> elseBody;
};

struct RecordField {
    std::string name;
    std::string type;
};

struct RecordNode : public ASTNode {
    std::string name;
    std::vector<RecordField> fields;

    explicit RecordNode(const std::string& n) : name(n) {}
};

struct TraitMethodSignature {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
};

struct TraitNode : public ASTNode {
    std::string name;
    std::vector<TraitMethodSignature> methods;

    explicit TraitNode(const std::string& n) : name(n) {}
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    std::vector<std::unique_ptr<StatementNode>> body;
    bool isComptime;

    FunctionNode(const std::string& n, const std::string& retType, bool comptime = false)
        : name(n), returnType(retType), isComptime(comptime) {}
};

struct TestBlockNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<StatementNode>> body;

    explicit TestBlockNode(const std::string& n) : name(n) {}
};

struct ImplNode : public ASTNode {
    std::string traitName;
    std::string targetName;
    std::vector<std::unique_ptr<FunctionNode>> methods;

    ImplNode(const std::string& traitN, const std::string& targetN)
        : traitName(traitN), targetName(targetN) {}
};

enum class ImportKind {
    ZYNC_FILE,
    CPP_USER_HEADER,
    CPP_SYS_HEADER,
    PACKAGE
};

struct ImportNode : public ASTNode {
    ImportKind kind;
    std::string target;

    ImportNode(ImportKind k, const std::string& t) : kind(k), target(t) {}
};

struct PackageNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> members;

    explicit PackageNode(const std::string& n) : name(n) {}
};

struct ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<ImportNode>> imports;
    std::vector<std::unique_ptr<PackageNode>> packages;
    std::vector<std::unique_ptr<TraitNode>> traits;
    std::vector<std::unique_ptr<RecordNode>> records;
    std::vector<std::unique_ptr<ImplNode>> impls;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    std::vector<std::unique_ptr<TestBlockNode>> tests;
};

#endif
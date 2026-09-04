#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <utility>

enum class Visibility {
    PUBLIC,
    PRIVATE
};

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

struct AttributeNode {
    std::string target;
    std::string action;
    std::vector<std::string> args;
};

using Attribute = AttributeNode;

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

struct AwaitExprNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> target;

    explicit AwaitExprNode(std::unique_ptr<ExpressionNode> t)
        : target(std::move(t)) {}
};

struct Parameter {
    std::string name;
    std::string type;
};

using Param = Parameter;

struct StatementNode : public ASTNode {
    int lineNumber = 1;
    virtual ~StatementNode() = default;
};

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
    LambdaNode(std::vector<Parameter> p, const std::string& ret, std::vector<std::unique_ptr<StatementNode>> body)
        : params(std::move(p)), returnType(ret), isExpressionBody(false), exprBody(nullptr), blockBody(std::move(body)) {}
    LambdaNode(std::vector<Parameter> p, const std::string& ret, std::unique_ptr<ExpressionNode> expr)
        : params(std::move(p)), returnType(ret), isExpressionBody(true), exprBody(std::move(expr)), blockBody() {}
};

struct ArrayLiteralNode : public ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;

    ArrayLiteralNode() = default;
    explicit ArrayLiteralNode(std::vector<std::unique_ptr<ExpressionNode>> elems)
        : elements(std::move(elems)) {}
};

struct MapLiteralNode : public ExpressionNode {
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, std::unique_ptr<ExpressionNode>>> entries;

    MapLiteralNode() = default;
    explicit MapLiteralNode(std::vector<std::pair<std::unique_ptr<ExpressionNode>, std::unique_ptr<ExpressionNode>>> e)
        : entries(std::move(e)) {}
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

    MatchNode() = default;
    MatchNode(std::unique_ptr<ExpressionNode> t, std::vector<MatchArm> a)
        : target(std::move(t)), arms(std::move(a)) {}
};

struct VariableDeclNode : public StatementNode {
    std::string type;
    std::string name;
    std::unique_ptr<ExpressionNode> value;
    bool isComptime;
    bool isMutable;
    Visibility visibility;

    VariableDeclNode(const std::string& t, const std::string& n, std::unique_ptr<ExpressionNode> v, bool comptime = false, bool isMut = true, Visibility vis = Visibility::PRIVATE)
        : type(t), name(n), value(std::move(v)), isComptime(comptime), isMutable(isMut), visibility(vis) {}
    VariableDeclNode(const std::string& n, const std::string& t, std::unique_ptr<ExpressionNode> v, bool isMut = true)
        : type(t), name(n), value(std::move(v)), isComptime(false), isMutable(isMut), visibility(Visibility::PRIVATE) {}
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
    PrintNode(std::unique_ptr<ExpressionNode> arg, bool println)
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
    CONTROLLED,
    FOR_RANGE
};

struct ForNode : public StatementNode {
    ForKind kind;
    std::string iteratorVar;
    std::unique_ptr<ExpressionNode> iterable;
    std::unique_ptr<StatementNode> init;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> increment;
    std::vector<std::unique_ptr<StatementNode>> body;

    ForNode() = default;

    explicit ForNode(std::vector<std::unique_ptr<StatementNode>> b)
        : kind(ForKind::INFINITE), iteratorVar(""), iterable(nullptr), init(nullptr), condition(nullptr), increment(nullptr), body(std::move(b)) {}

    ForNode(std::unique_ptr<ExpressionNode> cond, std::vector<std::unique_ptr<StatementNode>> b)
        : kind(ForKind::CONDITIONAL), iteratorVar(""), iterable(nullptr), init(nullptr), condition(std::move(cond)), increment(nullptr), body(std::move(b)) {}

    ForNode(const std::string& iter, std::unique_ptr<ExpressionNode> itExpr, std::vector<std::unique_ptr<StatementNode>> b)
        : kind(ForKind::FOR_RANGE), iteratorVar(iter), iterable(std::move(itExpr)), init(nullptr), condition(nullptr), increment(nullptr), body(std::move(b)) {}
};

struct ElseIfBranch {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StatementNode>> body;
};

using IfBranch = ElseIfBranch;

struct IfNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StatementNode>> thenBody;
    std::vector<ElseIfBranch> elseIfBranches;
    std::vector<std::unique_ptr<StatementNode>> elseBody;

    IfNode() = default;

    IfNode(std::unique_ptr<ExpressionNode> cond, std::vector<std::unique_ptr<StatementNode>> thenB,
           std::vector<ElseIfBranch> elseIfs, std::vector<std::unique_ptr<StatementNode>> elseB)
        : condition(std::move(cond)), thenBody(std::move(thenB)), elseIfBranches(std::move(elseIfs)), elseBody(std::move(elseB)) {}
};

struct RecordField {
    std::string name;
    std::string type;
    Visibility visibility;
};

struct RecordNode : public ASTNode {
    std::string name;
    std::vector<RecordField> fields;
    Visibility visibility;

    explicit RecordNode(const std::string& n, Visibility vis = Visibility::PRIVATE)
        : name(n), visibility(vis) {}
};

struct EnumVariant {
    std::string name;
    std::string value;
};

struct EnumNode : public ASTNode {
    std::string name;
    std::string underlyingType;
    std::vector<EnumVariant> variants;
    Visibility visibility;

    explicit EnumNode(const std::string& n, Visibility vis = Visibility::PRIVATE)
        : name(n), underlyingType(""), visibility(vis) {}
    EnumNode(const std::string& n, const std::string& uType, Visibility vis = Visibility::PRIVATE)
        : name(n), underlyingType(uType), visibility(vis) {}
};

struct TraitMethod {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
};

using TraitMethodSignature = TraitMethod;

struct TraitNode : public ASTNode {
    std::string name;
    std::vector<TraitMethod> methods;
    Visibility visibility;

    explicit TraitNode(const std::string& n, Visibility vis = Visibility::PRIVATE)
        : name(n), visibility(vis) {}
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    std::vector<std::unique_ptr<StatementNode>> body;
    bool isComptime;
    bool isAsync;
    Visibility visibility;
    std::vector<AttributeNode> attributes;

    FunctionNode(const std::string& n, Visibility vis = Visibility::PRIVATE)
        : name(n), returnType("void"), isComptime(false), isAsync(false), visibility(vis) {}
    FunctionNode(const std::string& n, const std::string& retType, bool comptime = false, bool asyncFlag = false, Visibility vis = Visibility::PRIVATE)
        : name(n), returnType(retType), isComptime(comptime), isAsync(asyncFlag), visibility(vis) {}
};

struct TestNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<StatementNode>> body;

    explicit TestNode(const std::string& n) : name(n) {}
};

using TestBlockNode = TestNode;

struct ImplNode : public ASTNode {
    std::string traitName;
    std::string targetName;
    std::vector<std::unique_ptr<FunctionNode>> methods;

    ImplNode(const std::string& targetN, const std::string& traitN = "")
        : traitName(traitN), targetName(targetN) {}
};

enum class ImportKind {
    ZYNC_FILE,
    CPP_USER_HEADER,
    CPP_SYS_HEADER,
    C_HEADER,
    PACKAGE
};

struct ImportNode : public ASTNode {
    ImportKind kind;
    std::string target;

    ImportNode(ImportKind k, const std::string& t) : kind(k), target(t) {}
};

struct ModNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> members;
    Visibility visibility;
    std::vector<AttributeNode> attributes;

    explicit ModNode(const std::string& n, Visibility vis = Visibility::PRIVATE)
        : name(n), visibility(vis) {}
};

struct PackageNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> members;

    explicit PackageNode(const std::string& n) : name(n) {}
};

struct ProgramNode : public ASTNode {
    std::string packageName;
    std::vector<std::unique_ptr<ImportNode>> imports;
    std::vector<std::unique_ptr<PackageNode>> packages;
    std::vector<std::unique_ptr<ModNode>> modules;
    std::vector<std::unique_ptr<EnumNode>> enums;
    std::vector<std::unique_ptr<TraitNode>> traits;
    std::vector<std::unique_ptr<RecordNode>> records;
    std::vector<std::unique_ptr<ImplNode>> impls;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    std::vector<std::unique_ptr<TestNode>> tests;

    ProgramNode() : packageName("") {}
};

#endif
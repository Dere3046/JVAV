#ifndef AST_HPP
#define AST_HPP
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "int128.hpp"

struct ASTNode;
struct Type;
struct Expr;
struct Stmt;
struct Decl;
struct Program;

enum TypeKind {
    TYPE_INT, TYPE_CHAR, TYPE_BOOL, TYPE_VOID,
    TYPE_PTR, TYPE_ARRAY,
    TYPE_BYTE, TYPE_UINT,
    TYPE_STRUCT, TYPE_UNION
};

struct Type {
    TypeKind kind;
    std::shared_ptr<Type> sub = nullptr;
    int arraySize = 0;
    std::string structName;
    bool isVolatile = false;
    std::string toString() const;
};

// Forward declarations for struct definitions
struct StructField;
struct StructDecl;
struct UnionDecl;

// Base expression
struct Expr {
    enum Kind {
        EXPR_NUMBER, EXPR_STRING, EXPR_CHAR, EXPR_BOOL,
        EXPR_IDENT, EXPR_BINARY, EXPR_UNARY,
        EXPR_CALL, EXPR_INDEX, EXPR_ASSIGN, EXPR_BORROW,
        EXPR_SIZEOF, EXPR_CAST, EXPR_FIELD, EXPR_OFFSETOF,
        EXPR_TERNARY, EXPR_ARRAY_LITERAL, EXPR_STRUCT_LITERAL
    };
    Kind kind;
    std::shared_ptr<Type> type;
    int line;
    int col;
    Expr(Kind k, int l, int col = 1) : kind(k), line(l), col(col) {}
    virtual ~Expr() = default;
};

struct NumberExpr : Expr {
    Int128 value;
    NumberExpr(Int128 v, int l, int col = 1) : Expr(EXPR_NUMBER, l, col), value(v) {}
};

struct StringExpr : Expr {
    std::string value;
    StringExpr(const std::string &v, int l, int col = 1) : Expr(EXPR_STRING, l, col), value(v) {}
};

struct CharExpr : Expr {
    char value;
    CharExpr(char v, int l, int col = 1) : Expr(EXPR_CHAR, l, col), value(v) {}
};

struct BoolExpr : Expr {
    bool value;
    BoolExpr(bool v, int l, int col = 1) : Expr(EXPR_BOOL, l, col), value(v) {}
};

struct IdentExpr : Expr {
    std::string name;
    IdentExpr(const std::string &n, int l, int col = 1) : Expr(EXPR_IDENT, l, col), name(n) {}
};

struct BinaryExpr : Expr {
    std::string op;
    std::shared_ptr<Expr> left, right;
    BinaryExpr(const std::string &o, std::shared_ptr<Expr> L, std::shared_ptr<Expr> R, int l, int col = 1)
        : Expr(EXPR_BINARY, l, col), op(o), left(L), right(R) {}
};

struct UnaryExpr : Expr {
    std::string op;
    std::shared_ptr<Expr> operand;
    UnaryExpr(const std::string &o, std::shared_ptr<Expr> e, int l, int col = 1)
        : Expr(EXPR_UNARY, l, col), op(o), operand(e) {}
};

struct CallExpr : Expr {
    std::shared_ptr<Expr> callee;
    std::vector<std::shared_ptr<Expr>> args;
    CallExpr(std::shared_ptr<Expr> c, const std::vector<std::shared_ptr<Expr>> &a, int l, int col = 1)
        : Expr(EXPR_CALL, l, col), callee(c), args(a) {}
};

struct IndexExpr : Expr {
    std::shared_ptr<Expr> base, index;
    IndexExpr(std::shared_ptr<Expr> b, std::shared_ptr<Expr> i, int l, int col = 1)
        : Expr(EXPR_INDEX, l, col), base(b), index(i) {}
};

struct AssignExpr : Expr {
    std::shared_ptr<Expr> left, right;
    AssignExpr(std::shared_ptr<Expr> L, std::shared_ptr<Expr> R, int l, int col = 1)
        : Expr(EXPR_ASSIGN, l, col), left(L), right(R) {}
};

struct BorrowExpr : Expr {
    std::shared_ptr<Expr> operand;
    bool mutableBorrow;
    BorrowExpr(std::shared_ptr<Expr> e, bool mut_, int l, int col = 1)
        : Expr(EXPR_BORROW, l, col), operand(e), mutableBorrow(mut_) {}
};

struct ArrayLiteralExpr : Expr {
    std::vector<std::shared_ptr<Expr>> elements;
    ArrayLiteralExpr(const std::vector<std::shared_ptr<Expr>> &elts, int l, int col = 1)
        : Expr(EXPR_ARRAY_LITERAL, l, col), elements(elts) {}
};

struct StructLiteralExpr : Expr {
    std::string structName;
    std::vector<std::pair<std::string, std::shared_ptr<Expr>>> fields;
    StructLiteralExpr(const std::string &n, const std::vector<std::pair<std::string, std::shared_ptr<Expr>>> &f, int l, int col = 1)
        : Expr(EXPR_STRUCT_LITERAL, l, col), structName(n), fields(f) {}
};

struct SizeofExpr : Expr {
    std::shared_ptr<Type> targetType;
    std::shared_ptr<Expr> targetExpr;
    SizeofExpr(std::shared_ptr<Type> t, std::shared_ptr<Expr> e, int l, int col = 1)
        : Expr(EXPR_SIZEOF, l, col), targetType(t), targetExpr(e) {}
};

struct CastExpr : Expr {
    std::shared_ptr<Type> castType;
    std::shared_ptr<Expr> operand;
    CastExpr(std::shared_ptr<Type> t, std::shared_ptr<Expr> e, int l, int col = 1)
        : Expr(EXPR_CAST, l, col), castType(t), operand(e) {}
};

struct FieldExpr : Expr {
    std::shared_ptr<Expr> base;
    std::string field;
    bool isArrow;
    FieldExpr(std::shared_ptr<Expr> b, const std::string &f, bool arrow, int l, int col = 1)
        : Expr(EXPR_FIELD, l, col), base(b), field(f), isArrow(arrow) {}
};

struct OffsetofExpr : Expr {
    std::shared_ptr<Type> targetType;
    std::string field;
    OffsetofExpr(std::shared_ptr<Type> t, const std::string &f, int l, int col = 1)
        : Expr(EXPR_OFFSETOF, l, col), targetType(t), field(f) {}
};

struct TernaryExpr : Expr {
    std::shared_ptr<Expr> cond, thenExpr, elseExpr;
    TernaryExpr(std::shared_ptr<Expr> c, std::shared_ptr<Expr> t, std::shared_ptr<Expr> e, int l, int col = 1)
        : Expr(EXPR_TERNARY, l, col), cond(c), thenExpr(t), elseExpr(e) {}
};

// Statement
struct Stmt {
    enum Kind {
        STMT_BLOCK, STMT_VAR, STMT_CONST, STMT_EXPR,
        STMT_IF, STMT_WHILE, STMT_FOR, STMT_RETURN,
        STMT_ASM, STMT_BREAK, STMT_CONTINUE, STMT_SWITCH,
        STMT_DO_WHILE
    };
    Kind kind;
    int line;
    int col;
    Stmt(Kind k, int l, int col = 1) : kind(k), line(l), col(col) {}
    virtual ~Stmt() = default;
};

struct BlockStmt : Stmt {
    std::vector<std::shared_ptr<Stmt>> stmts;
    BlockStmt(int l, int col = 1) : Stmt(STMT_BLOCK, l, col) {}
};

struct VarStmt : Stmt {
    std::string name;
    std::shared_ptr<Type> varType;
    std::shared_ptr<Expr> init;
    VarStmt(const std::string &n, std::shared_ptr<Type> t, std::shared_ptr<Expr> i, int l, int col = 1)
        : Stmt(STMT_VAR, l, col), name(n), varType(t), init(i) {}
};

struct ConstStmt : Stmt {
    std::string name;
    std::shared_ptr<Expr> value;
    ConstStmt(const std::string &n, std::shared_ptr<Expr> v, int l, int col = 1)
        : Stmt(STMT_CONST, l, col), name(n), value(v) {}
};

struct ExprStmt : Stmt {
    std::shared_ptr<Expr> expr;
    ExprStmt(std::shared_ptr<Expr> e, int l, int col = 1) : Stmt(STMT_EXPR, l, col), expr(e) {}
};

struct IfStmt : Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> thenBranch, elseBranch;
    IfStmt(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> t, std::shared_ptr<Stmt> e, int l, int col = 1)
        : Stmt(STMT_IF, l, col), cond(c), thenBranch(t), elseBranch(e) {}
};

struct WhileStmt : Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> body;
    WhileStmt(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> b, int l, int col = 1)
        : Stmt(STMT_WHILE, l, col), cond(c), body(b) {}
};

struct ForStmt : Stmt {
    std::shared_ptr<Stmt> init;
    std::shared_ptr<Expr> cond, step;
    std::shared_ptr<Stmt> body;
    ForStmt(std::shared_ptr<Stmt> i, std::shared_ptr<Expr> c, std::shared_ptr<Expr> s, std::shared_ptr<Stmt> b, int l, int col = 1)
        : Stmt(STMT_FOR, l, col), init(i), cond(c), step(s), body(b) {}
};

struct ReturnStmt : Stmt {
    std::shared_ptr<Expr> value;
    ReturnStmt(std::shared_ptr<Expr> v, int l, int col = 1) : Stmt(STMT_RETURN, l, col), value(v) {}
};

struct InlineAsmStmt : Stmt {
    std::vector<std::string> instructions;
    InlineAsmStmt(const std::vector<std::string> &ins, int l, int col = 1)
        : Stmt(STMT_ASM, l, col), instructions(ins) {}
};

struct BreakStmt : Stmt {
    BreakStmt(int l, int col = 1) : Stmt(STMT_BREAK, l, col) {}
};

struct ContinueStmt : Stmt {
    ContinueStmt(int l, int col = 1) : Stmt(STMT_CONTINUE, l, col) {}
};

struct CaseClause {
    std::shared_ptr<Expr> value;  // nullptr for default
    std::shared_ptr<Stmt> body;
    CaseClause(std::shared_ptr<Expr> v, std::shared_ptr<Stmt> b)
        : value(v), body(b) {}
};

struct SwitchStmt : Stmt {
    std::shared_ptr<Expr> expr;
    std::vector<CaseClause> cases;
    SwitchStmt(std::shared_ptr<Expr> e, const std::vector<CaseClause> &c, int l, int col = 1)
        : Stmt(STMT_SWITCH, l, col), expr(e), cases(c) {}
};

struct DoWhileStmt : Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> body;
    DoWhileStmt(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> b, int l, int col = 1)
        : Stmt(STMT_DO_WHILE, l, col), cond(c), body(b) {}
};

// Declaration
struct Decl {
    enum Kind { DECL_FUNC, DECL_VAR, DECL_CONST, DECL_IMPORT, DECL_SYSCALL, DECL_STRUCT, DECL_UNION, DECL_ENUM, DECL_TYPEDEF };
    Kind kind;
    int line;
    int col;
    Decl(Kind k, int l, int col = 1) : kind(k), line(l), col(col) {}
    virtual ~Decl() = default;
};

struct Param {
    std::string name;
    std::shared_ptr<Type> ptype;
};

struct StructField {
    std::string name;
    std::shared_ptr<Type> type;
};

struct FuncDecl : Decl {
    std::string name;
    std::shared_ptr<Type> retType;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
    FuncDecl(const std::string &n, std::shared_ptr<Type> r, const std::vector<Param> &p, std::shared_ptr<BlockStmt> b, int l, int col = 1)
        : Decl(DECL_FUNC, l, col), name(n), retType(r), params(p), body(b) {}
};

struct StructDecl : Decl {
    std::string name;
    std::vector<StructField> fields;
    StructDecl(const std::string &n, const std::vector<StructField> &f, int l, int col = 1)
        : Decl(DECL_STRUCT, l, col), name(n), fields(f) {}
};

struct UnionDecl : Decl {
    std::string name;
    std::vector<StructField> fields;
    UnionDecl(const std::string &n, const std::vector<StructField> &f, int l, int col = 1)
        : Decl(DECL_UNION, l, col), name(n), fields(f) {}
};

struct GlobalVarDecl : Decl {
    std::string name;
    std::shared_ptr<Type> varType;
    std::shared_ptr<Expr> init;
    GlobalVarDecl(const std::string &n, std::shared_ptr<Type> t, std::shared_ptr<Expr> i, int l, int col = 1)
        : Decl(DECL_VAR, l, col), name(n), varType(t), init(i) {}
};

struct GlobalConstDecl : Decl {
    std::string name;
    std::shared_ptr<Expr> value;
    GlobalConstDecl(const std::string &n, std::shared_ptr<Expr> v, int l, int col = 1)
        : Decl(DECL_CONST, l, col), name(n), value(v) {}
};

struct ImportDecl : Decl {
    std::string path;
    std::shared_ptr<Program> module;
    ImportDecl(const std::string &p, int l, int col = 1) : Decl(DECL_IMPORT, l, col), path(p) {}
};

struct EnumDecl : Decl {
    std::string name;
    std::vector<std::pair<std::string, std::shared_ptr<Expr>>> members;
    EnumDecl(const std::string &n, const std::vector<std::pair<std::string, std::shared_ptr<Expr>>> &m, int l, int col = 1)
        : Decl(DECL_ENUM, l, col), name(n), members(m) {}
};

struct TypedefDecl : Decl {
    std::string name;
    std::shared_ptr<Type> targetType;
    TypedefDecl(const std::string &n, std::shared_ptr<Type> t, int l, int col = 1)
        : Decl(DECL_TYPEDEF, l, col), name(n), targetType(t) {}
};

struct SyscallDecl : Decl {
    std::string name;
    int cmdId;
    int argCount;
    SyscallDecl(const std::string &n, int cmd, int args, int l, int col = 1)
        : Decl(DECL_SYSCALL, l, col), name(n), cmdId(cmd), argCount(args) {}
};

struct Program {
    std::vector<std::shared_ptr<Decl>> decls;
};

#endif

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
    TYPE_PTR, TYPE_ARRAY
};

struct Type {
    TypeKind kind;
    std::shared_ptr<Type> sub;  // for ptr< T > or array< T >
    int arraySize;              // for array< T >[size]
    std::string toString() const;
};

// Base expression
struct Expr {
    enum Kind {
        EXPR_NUMBER, EXPR_STRING, EXPR_CHAR, EXPR_BOOL,
        EXPR_IDENT, EXPR_BINARY, EXPR_UNARY,
        EXPR_CALL, EXPR_INDEX, EXPR_ASSIGN, EXPR_BORROW
    };
    Kind kind;
    std::shared_ptr<Type> type;
    int line;
    Expr(Kind k, int l) : kind(k), line(l) {}
    virtual ~Expr() = default;
};

struct NumberExpr : Expr {
    Int128 value;
    NumberExpr(Int128 v, int l) : Expr(EXPR_NUMBER, l), value(v) {}
};

struct StringExpr : Expr {
    std::string value;
    StringExpr(const std::string &v, int l) : Expr(EXPR_STRING, l), value(v) {}
};

struct CharExpr : Expr {
    char value;
    CharExpr(char v, int l) : Expr(EXPR_CHAR, l), value(v) {}
};

struct BoolExpr : Expr {
    bool value;
    BoolExpr(bool v, int l) : Expr(EXPR_BOOL, l), value(v) {}
};

struct IdentExpr : Expr {
    std::string name;
    IdentExpr(const std::string &n, int l) : Expr(EXPR_IDENT, l), name(n) {}
};

struct BinaryExpr : Expr {
    std::string op;
    std::shared_ptr<Expr> left, right;
    BinaryExpr(const std::string &o, std::shared_ptr<Expr> L, std::shared_ptr<Expr> R, int l)
        : Expr(EXPR_BINARY, l), op(o), left(L), right(R) {}
};

struct UnaryExpr : Expr {
    std::string op;
    std::shared_ptr<Expr> operand;
    UnaryExpr(const std::string &o, std::shared_ptr<Expr> e, int l)
        : Expr(EXPR_UNARY, l), op(o), operand(e) {}
};

struct CallExpr : Expr {
    std::shared_ptr<Expr> callee;
    std::vector<std::shared_ptr<Expr>> args;
    CallExpr(std::shared_ptr<Expr> c, const std::vector<std::shared_ptr<Expr>> &a, int l)
        : Expr(EXPR_CALL, l), callee(c), args(a) {}
};

struct IndexExpr : Expr {
    std::shared_ptr<Expr> base, index;
    IndexExpr(std::shared_ptr<Expr> b, std::shared_ptr<Expr> i, int l)
        : Expr(EXPR_INDEX, l), base(b), index(i) {}
};

struct AssignExpr : Expr {
    std::shared_ptr<Expr> left, right;
    AssignExpr(std::shared_ptr<Expr> L, std::shared_ptr<Expr> R, int l)
        : Expr(EXPR_ASSIGN, l), left(L), right(R) {}
};

struct BorrowExpr : Expr {
    std::shared_ptr<Expr> operand;
    bool mutableBorrow;
    BorrowExpr(std::shared_ptr<Expr> e, bool mut_, int l)
        : Expr(EXPR_BORROW, l), operand(e), mutableBorrow(mut_) {}
};

// Statement
struct Stmt {
    enum Kind {
        STMT_BLOCK, STMT_VAR, STMT_CONST, STMT_EXPR,
        STMT_IF, STMT_WHILE, STMT_FOR, STMT_RETURN
    };
    Kind kind;
    int line;
    Stmt(Kind k, int l) : kind(k), line(l) {}
    virtual ~Stmt() = default;
};

struct BlockStmt : Stmt {
    std::vector<std::shared_ptr<Stmt>> stmts;
    BlockStmt(int l) : Stmt(STMT_BLOCK, l) {}
};

struct VarStmt : Stmt {
    std::string name;
    std::shared_ptr<Type> varType;
    std::shared_ptr<Expr> init;
    VarStmt(const std::string &n, std::shared_ptr<Type> t, std::shared_ptr<Expr> i, int l)
        : Stmt(STMT_VAR, l), name(n), varType(t), init(i) {}
};

struct ConstStmt : Stmt {
    std::string name;
    std::shared_ptr<Expr> value;
    ConstStmt(const std::string &n, std::shared_ptr<Expr> v, int l)
        : Stmt(STMT_CONST, l), name(n), value(v) {}
};

struct ExprStmt : Stmt {
    std::shared_ptr<Expr> expr;
    ExprStmt(std::shared_ptr<Expr> e, int l) : Stmt(STMT_EXPR, l), expr(e) {}
};

struct IfStmt : Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> thenBranch, elseBranch;
    IfStmt(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> t, std::shared_ptr<Stmt> e, int l)
        : Stmt(STMT_IF, l), cond(c), thenBranch(t), elseBranch(e) {}
};

struct WhileStmt : Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> body;
    WhileStmt(std::shared_ptr<Expr> c, std::shared_ptr<Stmt> b, int l)
        : Stmt(STMT_WHILE, l), cond(c), body(b) {}
};

struct ForStmt : Stmt {
    std::shared_ptr<Stmt> init;
    std::shared_ptr<Expr> cond, step;
    std::shared_ptr<Stmt> body;
    ForStmt(std::shared_ptr<Stmt> i, std::shared_ptr<Expr> c, std::shared_ptr<Expr> s, std::shared_ptr<Stmt> b, int l)
        : Stmt(STMT_FOR, l), init(i), cond(c), step(s), body(b) {}
};

struct ReturnStmt : Stmt {
    std::shared_ptr<Expr> value;
    ReturnStmt(std::shared_ptr<Expr> v, int l) : Stmt(STMT_RETURN, l), value(v) {}
};

// Declaration
struct Decl {
    enum Kind { DECL_FUNC, DECL_VAR, DECL_CONST, DECL_IMPORT };
    Kind kind;
    int line;
    Decl(Kind k, int l) : kind(k), line(l) {}
    virtual ~Decl() = default;
};

struct Param {
    std::string name;
    std::shared_ptr<Type> ptype;
};

struct FuncDecl : Decl {
    std::string name;
    std::shared_ptr<Type> retType;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
    FuncDecl(const std::string &n, std::shared_ptr<Type> r, const std::vector<Param> &p, std::shared_ptr<BlockStmt> b, int l)
        : Decl(DECL_FUNC, l), name(n), retType(r), params(p), body(b) {}
};

struct GlobalVarDecl : Decl {
    std::string name;
    std::shared_ptr<Type> varType;
    std::shared_ptr<Expr> init;
    GlobalVarDecl(const std::string &n, std::shared_ptr<Type> t, std::shared_ptr<Expr> i, int l)
        : Decl(DECL_VAR, l), name(n), varType(t), init(i) {}
};

struct GlobalConstDecl : Decl {
    std::string name;
    std::shared_ptr<Expr> value;
    GlobalConstDecl(const std::string &n, std::shared_ptr<Expr> v, int l)
        : Decl(DECL_CONST, l), name(n), value(v) {}
};

struct ImportDecl : Decl {
    std::string path;
    std::shared_ptr<Program> module;  // parsed AST of imported module
    ImportDecl(const std::string &p, int l) : Decl(DECL_IMPORT, l), path(p) {}
};

struct Program {
    std::vector<std::shared_ptr<Decl>> decls;
};

#endif

#include "sema.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <filesystem>
namespace fs = std::filesystem;
using namespace std;

void Sema::enterScope() {
    scopeLevel++;
    if ((int)scopes.size() <= scopeLevel) scopes.resize(scopeLevel + 1);
    scopes[scopeLevel].clear();
}

void Sema::exitScope() {
    scopeLevel--;
}

bool Sema::declare(const string &name, Symbol::Kind k, shared_ptr<Type> type, int level, bool global) {
    if (scopes[level].find(name) != scopes[level].end()) {
        error = "Redeclaration of '" + name + "'";
        return false;
    }
    scopes[level][name] = {k, type, level, global};
    return true;
}

Symbol* Sema::lookup(const string &name) {
    for (int i = scopeLevel; i >= 0; i--) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

bool Sema::analyze(shared_ptr<Program> prog) {
    return analyze(prog, "");
}

bool Sema::analyze(shared_ptr<Program> prog, const string &bp) {
    basePath = bp;
    scopeLevel = -1;
    scopes.clear();
    importedFiles.clear();
    enterScope();
    auto intType = make_shared<Type>(Type{TYPE_INT});
    auto voidType = make_shared<Type>(Type{TYPE_VOID});
    declare("putint", Symbol::SYM_FUNC, intType, 0, true);
    declare("putchar", Symbol::SYM_FUNC, voidType, 0, true);
    for (auto &d : prog->decls) {
        if (!checkDecl(d)) return false;
    }
    exitScope();
    return error.empty();
}

bool Sema::checkDecl(shared_ptr<Decl> d) {
    if (d->kind == Decl::DECL_FUNC) {
        auto fd = dynamic_pointer_cast<FuncDecl>(d);
        auto retType = fd->retType ? fd->retType : make_shared<Type>(Type{TYPE_VOID});
        if (!declare(fd->name, Symbol::SYM_FUNC, retType, 0, true)) return false;
        enterScope();
        for (auto &p : fd->params) {
            auto pt = p.ptype ? p.ptype : make_shared<Type>(Type{TYPE_INT});
            if (!declare(p.name, Symbol::SYM_VAR, pt, scopeLevel, false)) return false;
        }
        if (!checkStmt(fd->body)) return false;
        exitScope();
    } else if (d->kind == Decl::DECL_VAR) {
        auto vd = dynamic_pointer_cast<GlobalVarDecl>(d);
        auto t = vd->varType ? vd->varType : make_shared<Type>(Type{TYPE_INT});
        if (!declare(vd->name, Symbol::SYM_VAR, t, 0, true)) return false;
    } else if (d->kind == Decl::DECL_CONST) {
        auto cd = dynamic_pointer_cast<GlobalConstDecl>(d);
        if (!declare(cd->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), 0, true)) return false;
    } else if (d->kind == Decl::DECL_IMPORT) {
        auto id = dynamic_pointer_cast<ImportDecl>(d);
        fs::path p = id->path;
        if (!basePath.empty() && p.is_relative()) {
            p = fs::path(basePath) / p;
        }
        string absPath = fs::weakly_canonical(p).string();
        if (importedFiles.find(absPath) != importedFiles.end()) return true;
        importedFiles.insert(absPath);

        Lexer lex;
        if (!lex.tokenize(absPath)) { error = "Import lex error: " + lex.getError(); return false; }
        FrontParser par;
        if (!par.parse(lex.getTokens())) { error = "Import parse error: " + par.getError(); return false; }

        Sema subSema;
        string subBase = fs::path(absPath).parent_path().string();
        if (!subSema.analyze(par.getProgram(), subBase)) {
            error = "Import sema error in " + id->path + ": " + subSema.getError();
            return false;
        }

        // Import symbols into current scope
        for (auto &subd : par.getProgram()->decls) {
            if (subd->kind == Decl::DECL_FUNC) {
                auto fd = dynamic_pointer_cast<FuncDecl>(subd);
                auto retType = fd->retType ? fd->retType : make_shared<Type>(Type{TYPE_VOID});
                if (!declare(fd->name, Symbol::SYM_FUNC, retType, 0, true)) return false;
            } else if (subd->kind == Decl::DECL_VAR) {
                auto vd = dynamic_pointer_cast<GlobalVarDecl>(subd);
                auto t = vd->varType ? vd->varType : make_shared<Type>(Type{TYPE_INT});
                if (!declare(vd->name, Symbol::SYM_VAR, t, 0, true)) return false;
            } else if (subd->kind == Decl::DECL_CONST) {
                auto cd = dynamic_pointer_cast<GlobalConstDecl>(subd);
                if (!declare(cd->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), 0, true)) return false;
            }
        }

        id->module = par.getProgram();
    }
    return true;
}

bool Sema::checkStmt(shared_ptr<Stmt> s) {
    if (!s) return true;
    switch (s->kind) {
        case Stmt::STMT_BLOCK: {
            auto b = dynamic_pointer_cast<BlockStmt>(s);
            for (auto &st : b->stmts) if (!checkStmt(st)) return false;
            break;
        }
        case Stmt::STMT_VAR: {
            auto v = dynamic_pointer_cast<VarStmt>(s);
            auto t = v->varType ? v->varType : make_shared<Type>(Type{TYPE_INT});
            if (!declare(v->name, Symbol::SYM_VAR, t, scopeLevel, false)) return false;
            if (v->init && !checkExpr(v->init)) return false;
            break;
        }
        case Stmt::STMT_CONST: {
            auto c = dynamic_pointer_cast<ConstStmt>(s);
            if (!declare(c->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), scopeLevel, false)) return false;
            if (!checkExpr(c->value)) return false;
            break;
        }
        case Stmt::STMT_EXPR: {
            auto e = dynamic_pointer_cast<ExprStmt>(s);
            if (!checkExpr(e->expr)) return false;
            break;
        }
        case Stmt::STMT_IF: {
            auto i = dynamic_pointer_cast<IfStmt>(s);
            if (!checkExpr(i->cond)) return false;
            if (!checkStmt(i->thenBranch)) return false;
            if (i->elseBranch && !checkStmt(i->elseBranch)) return false;
            break;
        }
        case Stmt::STMT_WHILE: {
            auto w = dynamic_pointer_cast<WhileStmt>(s);
            if (!checkExpr(w->cond)) return false;
            if (!checkStmt(w->body)) return false;
            break;
        }
        case Stmt::STMT_FOR: {
            auto f = dynamic_pointer_cast<ForStmt>(s);
            enterScope();
            if (f->init && !checkStmt(f->init)) return false;
            if (f->cond && !checkExpr(f->cond)) return false;
            if (f->step && !checkExpr(f->step)) return false;
            if (!checkStmt(f->body)) return false;
            exitScope();
            break;
        }
        case Stmt::STMT_RETURN: {
            auto r = dynamic_pointer_cast<ReturnStmt>(s);
            if (r->value && !checkExpr(r->value)) return false;
            break;
        }
    }
    return true;
}

bool Sema::checkExpr(shared_ptr<Expr> e) {
    if (!e) return true;
    switch (e->kind) {
        case Expr::EXPR_NUMBER:
        case Expr::EXPR_CHAR:
        case Expr::EXPR_BOOL:
            e->type = make_shared<Type>(Type{TYPE_INT});
            break;
        case Expr::EXPR_STRING:
            e->type = make_shared<Type>(Type{TYPE_ARRAY});
            e->type->sub = make_shared<Type>(Type{TYPE_CHAR});
            break;
        case Expr::EXPR_IDENT: {
            auto id = dynamic_pointer_cast<IdentExpr>(e);
            auto sym = lookup(id->name);
            if (!sym) { error = "Undefined identifier: " + id->name; return false; }
            id->type = sym->type;
            break;
        }
        case Expr::EXPR_BINARY: {
            auto b = dynamic_pointer_cast<BinaryExpr>(e);
            if (!checkExpr(b->left) || !checkExpr(b->right)) return false;
            b->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_UNARY: {
            auto u = dynamic_pointer_cast<UnaryExpr>(e);
            if (!checkExpr(u->operand)) return false;
            u->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_CALL: {
            auto c = dynamic_pointer_cast<CallExpr>(e);
            if (!checkExpr(c->callee)) return false;
            for (auto &a : c->args) if (!checkExpr(a)) return false;
            c->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_INDEX: {
            auto i = dynamic_pointer_cast<IndexExpr>(e);
            if (!checkExpr(i->base) || !checkExpr(i->index)) return false;
            i->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_ASSIGN: {
            auto a = dynamic_pointer_cast<AssignExpr>(e);
            if (!checkExpr(a->left) || !checkExpr(a->right)) return false;
            a->type = a->right->type;
            break;
        }
    }
    return true;
}

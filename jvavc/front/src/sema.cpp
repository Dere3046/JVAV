#include "sema.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <filesystem>
namespace fs = std::filesystem;
using namespace std;

// ------------------------------------------------------------------
// Error reporting
// ------------------------------------------------------------------
void Sema::report(SemaLevel level, const string &msg, int line, const string &hint) {
    errors.push_back({level, msg, line, hint});
    if (level == SEM_ERROR && firstError.empty()) firstError = msg;
}

bool Sema::hasErrors() const {
    for (auto &e : errors) if (e.level == SEM_ERROR) return true;
    return false;
}

void Sema::printErrors(ostream &os) const {
    int errCount = 0, warnCount = 0;
    for (auto &e : errors) {
        if (e.level == SEM_ERROR) {
            os << "[MimiWorld Error] line " << e.line << ": " << e.msg << "\n";
            errCount++;
        } else {
            os << "[MimiWorld Warning] line " << e.line << ": " << e.msg << "\n";
            warnCount++;
        }
        if (!e.hint.empty()) os << "  --> hint: " << e.hint << "\n";
    }
    os << "\n" << errCount << " error(s), " << warnCount << " warning(s)\n";
}

// ------------------------------------------------------------------
// Scope management
// ------------------------------------------------------------------
void Sema::enterScope() {
    scopeLevel++;
    if ((int)scopes.size() <= scopeLevel) scopes.resize(scopeLevel + 1);
    scopes[scopeLevel].clear();
}

void Sema::exitScope() {
    checkUnusedInScope(scopeLevel);
    scopeLevel--;
}

bool Sema::declare(const string &name, Symbol::Kind k, shared_ptr<Type> type, int level, bool global) {
    if (scopes[level].find(name) != scopes[level].end()) {
        report(SEM_ERROR, "Redeclaration of '" + name + "'", 0,
               "'" + name + "' was already declared in this scope");
        return false;
    }
    Symbol sym;
    sym.kind = k;
    sym.type = type;
    sym.scopeLevel = level;
    sym.isGlobal = global;
    sym.isCopy = isCopyType(type);
    sym.initialized = (k == Symbol::SYM_CONST || k == Symbol::SYM_FUNC);
    sym.moved = false;
    sym.borrowCount = 0;
    sym.mutBorrowed = false;
    sym.used = false;
    scopes[level][name] = sym;
    return true;
}

Symbol* Sema::lookup(const string &name) {
    for (int i = scopeLevel; i >= 0; i--) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

// ------------------------------------------------------------------
// Type helpers
// ------------------------------------------------------------------
bool Sema::isCopyType(shared_ptr<Type> t) {
    if (!t) return true;
    return t->kind == TYPE_INT || t->kind == TYPE_CHAR || t->kind == TYPE_BOOL;
}

bool Sema::typeEqual(shared_ptr<Type> a, shared_ptr<Type> b) {
    if (!a || !b) return true;
    if (a->kind != b->kind) return false;
    if (a->kind == TYPE_PTR || a->kind == TYPE_ARRAY)
        return typeEqual(a->sub, b->sub);
    return true;
}

bool Sema::typeCompatible(shared_ptr<Type> dst, shared_ptr<Type> src) {
    if (!dst || !src) return true;
    // JVAV is weakly typed; allow numeric coercion
    if (dst->kind == TYPE_INT && (src->kind == TYPE_INT || src->kind == TYPE_CHAR || src->kind == TYPE_BOOL))
        return true;
    if (dst->kind == TYPE_BOOL && (src->kind == TYPE_INT || src->kind == TYPE_BOOL || src->kind == TYPE_CHAR))
        return true;
    if (dst->kind == TYPE_VOID && src->kind != TYPE_VOID) return false;
    return typeEqual(dst, src);
}

string Sema::typeStr(shared_ptr<Type> t) {
    if (!t) return "?";
    return t->toString();
}

// ------------------------------------------------------------------
// Ownership helpers
// ------------------------------------------------------------------
void Sema::markInitialized(const string &name) {
    Symbol *sym = lookup(name);
    if (sym) { sym->initialized = true; sym->moved = false; }
}

void Sema::markMoved(const string &name, int line) {
    Symbol *sym = lookup(name);
    if (!sym) return;
    if (sym->isCopy) return;  // Copy types are never moved
    if (sym->mutBorrowed) {
        report(SEM_ERROR, "Cannot move '" + name + "' because it is mutably borrowed", line,
               "drop the mutable borrow before moving '" + name + "'");
        return;
    }
    if (sym->borrowCount > 0) {
        report(SEM_ERROR, "Cannot move '" + name + "' because it is borrowed", line,
               "drop the borrow before moving '" + name + "'");
        return;
    }
    sym->moved = true;
}

void Sema::markUsed(const string &name) {
    Symbol *sym = lookup(name);
    if (sym) sym->used = true;
}

bool Sema::checkUsable(const string &name, int line) {
    Symbol *sym = lookup(name);
    if (!sym) return true;
    if (!sym->initialized) {
        report(SEM_ERROR, "Use of possibly uninitialized variable '" + name + "'", line,
               "assign a value to '" + name + "' before using it");
        return false;
    }
    if (sym->moved) {
        report(SEM_ERROR, "Use of moved value '" + name + "'", line,
               "reassign '" + name + "' or use a borrow (&" + name + ") instead");
        return false;
    }
    if (sym->mutBorrowed) {
        report(SEM_ERROR, "Cannot use '" + name + "' while mutably borrowed", line,
               "wait until the mutable borrow ends");
        return false;
    }
    return true;
}

bool Sema::checkBorrow(const string &name, bool mut_, int line) {
    Symbol *sym = lookup(name);
    if (!sym) return true;
    if (!sym->initialized) {
        report(SEM_ERROR, "Cannot borrow '" + name + "' because it is not initialized", line,
               "assign a value to '" + name + "' before borrowing");
        return false;
    }
    if (sym->moved) {
        report(SEM_ERROR, "Cannot borrow '" + name + "' because it has been moved", line,
               "reassign '" + name + "' before borrowing");
        return false;
    }
    if (mut_) {
        if (sym->borrowCount > 0) {
            report(SEM_ERROR, "Cannot mutably borrow '" + name + "' because it is already borrowed", line,
                   "drop existing borrows before taking &mut");
            return false;
        }
        if (sym->mutBorrowed) {
            report(SEM_ERROR, "Cannot mutably borrow '" + name + "' because it is already mutably borrowed", line,
                   "only one &mut borrow is allowed at a time");
            return false;
        }
        sym->mutBorrowed = true;
    } else {
        if (sym->mutBorrowed) {
            report(SEM_ERROR, "Cannot borrow '" + name + "' because it is mutably borrowed", line,
                   "wait until the &mut borrow ends");
            return false;
        }
        sym->borrowCount++;
    }
    return true;
}

void Sema::checkUnusedInScope(int level) {
    for (auto &p : scopes[level]) {
        const string &name = p.first;
        Symbol &sym = p.second;
        if (sym.kind == Symbol::SYM_VAR && !sym.used && !sym.isGlobal) {
            report(SEM_WARNING, "Unused variable '" + name + "'", 0,
                   "remove the declaration or prefix with _ to suppress");
        }
        if (sym.kind == Symbol::SYM_VAR && sym.isCopy && !sym.initialized && !sym.isGlobal) {
            // uninitialized warning already handled at use site
        }
    }
}

// ------------------------------------------------------------------
// Main analysis entry
// ------------------------------------------------------------------
bool Sema::analyze(shared_ptr<Program> prog) {
    return analyze(prog, "");
}

bool Sema::analyze(shared_ptr<Program> prog, const string &bp) {
    basePath = bp;
    scopeLevel = -1;
    scopes.clear();
    importedFiles.clear();
    errors.clear();
    firstError.clear();
    inLoop = false;

    enterScope();
    auto intType = make_shared<Type>(Type{TYPE_INT});
    auto voidType = make_shared<Type>(Type{TYPE_VOID});
    declare("putint", Symbol::SYM_FUNC, intType, 0, true);
    declare("putchar", Symbol::SYM_FUNC, voidType, 0, true);
    auto ptrIntType = make_shared<Type>(Type{TYPE_PTR});
    ptrIntType->sub = make_shared<Type>(Type{TYPE_INT});
    declare("alloc", Symbol::SYM_FUNC, ptrIntType, 0, true);
    declare("free", Symbol::SYM_FUNC, voidType, 0, true);

    for (auto &d : prog->decls) {
        checkDecl(d);
    }
    exitScope();
    return !hasErrors();
}

// ------------------------------------------------------------------
// Declarations
// ------------------------------------------------------------------
bool Sema::checkDecl(shared_ptr<Decl> d) {
    if (d->kind == Decl::DECL_FUNC) {
        auto fd = dynamic_pointer_cast<FuncDecl>(d);
        auto retType = fd->retType ? fd->retType : make_shared<Type>(Type{TYPE_VOID});
        if (!declare(fd->name, Symbol::SYM_FUNC, retType, 0, true)) return false;

        auto prevRet = curRetType;
        curRetType = retType;
        enterScope();
        for (auto &p : fd->params) {
            auto pt = p.ptype ? p.ptype : make_shared<Type>(Type{TYPE_INT});
            if (!declare(p.name, Symbol::SYM_VAR, pt, scopeLevel, false)) return false;
            markInitialized(p.name);  // parameters are initialized
        }
        checkStmt(fd->body);
        exitScope();
        curRetType = prevRet;
    } else if (d->kind == Decl::DECL_VAR) {
        auto vd = dynamic_pointer_cast<GlobalVarDecl>(d);
        auto t = vd->varType ? vd->varType : make_shared<Type>(Type{TYPE_INT});
        if (!declare(vd->name, Symbol::SYM_VAR, t, 0, true)) return false;
        if (vd->init) {
            checkExpr(vd->init);
            markInitialized(vd->name);
        }
    } else if (d->kind == Decl::DECL_CONST) {
        auto cd = dynamic_pointer_cast<GlobalConstDecl>(d);
        if (!declare(cd->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), 0, true)) return false;
        if (cd->value) checkExpr(cd->value);
    } else if (d->kind == Decl::DECL_IMPORT) {
        return processImport(dynamic_pointer_cast<ImportDecl>(d));
    }
    return true;
}

bool Sema::processImport(shared_ptr<ImportDecl> id) {
    fs::path p = id->path;
    if (!basePath.empty() && p.is_relative()) {
        p = fs::path(basePath) / p;
    }
    string absPath = fs::weakly_canonical(p).string();
    if (importedFiles.find(absPath) != importedFiles.end()) return true;
    importedFiles.insert(absPath);

    Lexer lex;
    if (!lex.tokenize(absPath)) {
        report(SEM_ERROR, "Import failed: cannot read '" + id->path + "'", id->line,
               "check that the file exists and is readable");
        return false;
    }
    FrontParser par;
    if (!par.parse(lex.getTokens())) {
        report(SEM_ERROR, "Import parse error in '" + id->path + "': " + par.getError(), id->line, "");
        return false;
    }

    Sema subSema;
    string subBase = fs::path(absPath).parent_path().string();
    if (!subSema.analyze(par.getProgram(), subBase)) {
        report(SEM_ERROR, "Import semantic error in '" + id->path + "': " + subSema.getError(), id->line, "");
        return false;
    }

    for (auto &subd : par.getProgram()->decls) {
        if (subd->kind == Decl::DECL_FUNC) {
            auto fd = dynamic_pointer_cast<FuncDecl>(subd);
            auto retType = fd->retType ? fd->retType : make_shared<Type>(Type{TYPE_VOID});
            declare(fd->name, Symbol::SYM_FUNC, retType, 0, true);
        } else if (subd->kind == Decl::DECL_VAR) {
            auto vd = dynamic_pointer_cast<GlobalVarDecl>(subd);
            auto t = vd->varType ? vd->varType : make_shared<Type>(Type{TYPE_INT});
            declare(vd->name, Symbol::SYM_VAR, t, 0, true);
            if (vd->init) markInitialized(vd->name);
        } else if (subd->kind == Decl::DECL_CONST) {
            auto cd = dynamic_pointer_cast<GlobalConstDecl>(subd);
            declare(cd->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), 0, true);
        }
    }
    id->module = par.getProgram();
    return true;
}

// ------------------------------------------------------------------
// Statements
// ------------------------------------------------------------------
bool Sema::checkStmt(shared_ptr<Stmt> s) {
    if (!s) return true;
    switch (s->kind) {
        case Stmt::STMT_BLOCK: {
            auto b = dynamic_pointer_cast<BlockStmt>(s);
            enterScope();
            for (auto &st : b->stmts) checkStmt(st);
            exitScope();
            break;
        }
        case Stmt::STMT_VAR: {
            auto v = dynamic_pointer_cast<VarStmt>(s);
            auto t = v->varType ? v->varType : make_shared<Type>(Type{TYPE_INT});
            if (!declare(v->name, Symbol::SYM_VAR, t, scopeLevel, false)) return false;
            if (v->init) {
                checkExpr(v->init);
                // Check if init is a move of another variable
                if (v->init->kind == Expr::EXPR_IDENT) {
                    auto id = dynamic_pointer_cast<IdentExpr>(v->init);
                    markMoved(id->name, v->line);
                }
                markInitialized(v->name);
            }
            break;
        }
        case Stmt::STMT_CONST: {
            auto c = dynamic_pointer_cast<ConstStmt>(s);
            if (!declare(c->name, Symbol::SYM_CONST, make_shared<Type>(Type{TYPE_INT}), scopeLevel, false)) return false;
            if (c->value) checkExpr(c->value);
            break;
        }
        case Stmt::STMT_EXPR: {
            auto e = dynamic_pointer_cast<ExprStmt>(s);
            if (!checkExpr(e->expr)) return false;
            break;
        }
        case Stmt::STMT_IF: {
            auto i = dynamic_pointer_cast<IfStmt>(s);
            checkExpr(i->cond);
            checkStmt(i->thenBranch);
            if (i->elseBranch) checkStmt(i->elseBranch);
            break;
        }
        case Stmt::STMT_WHILE: {
            auto w = dynamic_pointer_cast<WhileStmt>(s);
            checkExpr(w->cond);
            bool prevLoop = inLoop;
            inLoop = true;
            checkStmt(w->body);
            inLoop = prevLoop;
            break;
        }
        case Stmt::STMT_FOR: {
            auto f = dynamic_pointer_cast<ForStmt>(s);
            enterScope();
            if (f->init) checkStmt(f->init);
            if (f->cond) checkExpr(f->cond);
            if (f->step) checkExpr(f->step);
            bool prevLoop = inLoop;
            inLoop = true;
            checkStmt(f->body);
            inLoop = prevLoop;
            exitScope();
            break;
        }
        case Stmt::STMT_RETURN: {
            auto r = dynamic_pointer_cast<ReturnStmt>(s);
            if (r->value) {
                checkExpr(r->value);
                if (curRetType && curRetType->kind == TYPE_VOID) {
                    report(SEM_ERROR, "Cannot return a value from a void function", r->line,
                           "remove the return value or change the function return type");
                }
            } else {
                if (curRetType && curRetType->kind != TYPE_VOID) {
                    report(SEM_ERROR, "Missing return value in non-void function", r->line,
                           "add a return value of type " + typeStr(curRetType));
                }
            }
            break;
        }
    }
    return true;
}

// ------------------------------------------------------------------
// Expressions
// ------------------------------------------------------------------
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
            if (!sym) {
                report(SEM_ERROR, "Undefined identifier '" + id->name + "'", id->line,
                       "declare '" + id->name + "' before use");
                return false;
            }
            if (sym->kind == Symbol::SYM_FUNC) {
                report(SEM_ERROR, "Cannot use function '" + id->name + "' as a value", id->line,
                       "call the function with () instead");
                return false;
            }
            checkUsable(id->name, id->line);
            markUsed(id->name);
            id->type = sym->type;
            break;
        }
        case Expr::EXPR_BINARY: {
            auto b = dynamic_pointer_cast<BinaryExpr>(e);
            checkExpr(b->left);
            checkExpr(b->right);
            b->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_UNARY: {
            auto u = dynamic_pointer_cast<UnaryExpr>(e);
            checkExpr(u->operand);
            u->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_CALL: {
            auto c = dynamic_pointer_cast<CallExpr>(e);
            string fname;
            if (c->callee->kind == Expr::EXPR_IDENT) {
                fname = dynamic_pointer_cast<IdentExpr>(c->callee)->name;
            }
            auto sym = lookup(fname);
            if (!sym) {
                report(SEM_ERROR, "Undefined function '" + fname + "'", c->line,
                       "declare '" + fname + "' before calling");
                return false;
            }
            if (sym->kind != Symbol::SYM_FUNC) {
                report(SEM_ERROR, "'" + fname + "' is not a function", c->line,
                       "use a function name for the call");
                return false;
            }
            // Note: we don't enforce exact param counts because JVAV is weakly typed
            for (auto &a : c->args) {
                checkExpr(a);
                // Move non-Copy args for user-defined functions
                if (a->kind == Expr::EXPR_IDENT) {
                    auto id = dynamic_pointer_cast<IdentExpr>(a);
                    Symbol *s = lookup(id->name);
                    if (s && !s->isCopy && s->kind == Symbol::SYM_VAR) {
                        markMoved(id->name, c->line);
                    }
                }
            }
            c->type = sym->type;
            break;
        }
        case Expr::EXPR_INDEX: {
            auto i = dynamic_pointer_cast<IndexExpr>(e);
            checkExpr(i->base);
            checkExpr(i->index);
            i->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_ASSIGN: {
            auto a = dynamic_pointer_cast<AssignExpr>(e);
            if (!checkAssignTarget(a->left)) return false;
            checkExpr(a->right);
            // Move non-Copy value on assignment
            if (a->right->kind == Expr::EXPR_IDENT) {
                auto id = dynamic_pointer_cast<IdentExpr>(a->right);
                markMoved(id->name, a->line);
            }
            a->type = a->right->type;
            if (a->left->kind == Expr::EXPR_IDENT) {
                auto id = dynamic_pointer_cast<IdentExpr>(a->left);
                markInitialized(id->name);
            }
            break;
        }
        case Expr::EXPR_BORROW: {
            auto b = dynamic_pointer_cast<BorrowExpr>(e);
            if (b->operand->kind != Expr::EXPR_IDENT) {
                report(SEM_ERROR, "Borrow expression must target a variable", b->line,
                       "use &name or &mut name");
                return false;
            }
            auto id = dynamic_pointer_cast<IdentExpr>(b->operand);
            checkBorrow(id->name, b->mutableBorrow, b->line);
            markUsed(id->name);
            e->type = make_shared<Type>(Type{TYPE_PTR});
            e->type->sub = id->type ? id->type : make_shared<Type>(Type{TYPE_INT});
            break;
        }
    }
    return true;
}

bool Sema::checkAssignTarget(shared_ptr<Expr> e) {
    if (e->kind == Expr::EXPR_IDENT) return true;
    if (e->kind == Expr::EXPR_INDEX) {
        auto idx = dynamic_pointer_cast<IndexExpr>(e);
        if (idx->base->kind == Expr::EXPR_IDENT) {
            auto id = dynamic_pointer_cast<IdentExpr>(idx->base);
            return checkUsable(id->name, e->line);
        }
        return true;
    }
    report(SEM_ERROR, "Invalid assignment target", e->line,
           "only variables and index expressions can be assigned to");
    return false;
}

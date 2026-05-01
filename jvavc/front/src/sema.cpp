#include "sema.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
namespace fs = std::filesystem;
using namespace std;

// ------------------------------------------------------------------
// Error reporting
// ------------------------------------------------------------------
void Sema::report(SemaLevel level, const string &msg, int line, int col, const string &hint) {
    errors.push_back({level, msg, currentFile, line, col, hint});
    if (level == SEM_ERROR && firstError.empty()) firstError = msg;
}

bool Sema::hasErrors() const {
    for (auto &e : errors) if (e.level == SEM_ERROR) return true;
    return false;
}

static vector<pair<int, string>> getSourceLines(const string &file) {
    vector<pair<int, string>> lines;
    ifstream f(file);
    if (!f) return lines;
    string s;
    int n = 1;
    while (getline(f, s)) {
        if (!s.empty() && s.back() == '\r') s.pop_back();
        lines.push_back({n, s});
        ++n;
    }
    return lines;
}

static void printSourceSnippet(ostream &os, const vector<pair<int, string>> &lines,
                                int targetLine, int col, int contextLines = 1) {
    if (lines.empty() || targetLine <= 0) return;
    int targetIdx = -1;
    for (int i = 0; i < (int)lines.size(); ++i) {
        if (lines[i].first == targetLine) { targetIdx = i; break; }
    }
    if (targetIdx == -1) return;

    int ctxStart = max(0, targetIdx - contextLines);
    int ctxEnd = min((int)lines.size() - 1, targetIdx + contextLines);
    int maxLineNum = lines[ctxEnd].first;
    int w = (int)to_string(maxLineNum).length();

    os << string(w + 1, ' ') << "|\n";
    for (int i = ctxStart; i <= ctxEnd; ++i) {
        os << " " << setw(w) << lines[i].first << " | " << lines[i].second << "\n";
        if (i == targetIdx) {
            os << string(w + 1, ' ') << "| ";
            for (int j = 1; j < col; ++j) os << " ";
            os << "^\n";
        }
    }
}

void Sema::printErrors(ostream &os) const {
    int errCount = 0, warnCount = 0;
    for (auto &e : errors) {
        if (e.level == SEM_ERROR) {
            os << "error[E" << setw(4) << setfill('0') << (1000 + errCount) << "]: " << e.msg << "\n";
            errCount++;
        } else {
            os << "warning[W" << setw(4) << setfill('0') << (2000 + warnCount) << "]: " << e.msg << "\n";
            warnCount++;
        }
        if (!e.file.empty() && e.line > 0) {
            os << " --> " << e.file << ":" << e.line << ":" << e.col << "\n";
            auto lines = getSourceLines(e.file);
            printSourceSnippet(os, lines, e.line, e.col, 1);
        } else if (e.line > 0) {
            os << " --> line " << e.line << "\n";
        }
        if (!e.hint.empty()) {
            os << "   = help: " << e.hint << "\n";
        }
    }
    os << "\n" << errCount << " error(s) and " << warnCount << " warning(s) generated\n";
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
        report(SEM_ERROR, "the name `" + name + "` is defined multiple times", 0, 1,
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
// Struct / Union registry
// ------------------------------------------------------------------
map<string, shared_ptr<StructDecl>> structDefs;
map<string, shared_ptr<UnionDecl>> unionDefs;

static void registerStruct(shared_ptr<StructDecl> sd) {
    structDefs[sd->name] = sd;
}

static void registerUnion(shared_ptr<UnionDecl> ud) {
    unionDefs[ud->name] = ud;
}

static shared_ptr<StructDecl> findStruct(const string &name) {
    auto it = structDefs.find(name);
    if (it != structDefs.end()) return it->second;
    return nullptr;
}

static shared_ptr<UnionDecl> findUnion(const string &name) {
    auto it = unionDefs.find(name);
    if (it != unionDefs.end()) return it->second;
    return nullptr;
}

// ------------------------------------------------------------------
// Type helpers
// ------------------------------------------------------------------
static int typeSize(shared_ptr<Type> t) {
    if (!t) return 1;
    switch (t->kind) {
        case TYPE_INT: case TYPE_CHAR: case TYPE_BOOL: case TYPE_BYTE: case TYPE_UINT: case TYPE_VOID:
            return 1;
        case TYPE_PTR:
            return 1;
        case TYPE_ARRAY: {
            int subSize = typeSize(t->sub);
            if (t->arraySize > 0) return t->arraySize * subSize;
            return 1; // dynamic array
        }
        case TYPE_STRUCT: {
            auto sd = findStruct(t->structName);
            if (!sd) return 1;
            int sz = 0;
            for (auto &f : sd->fields) sz += typeSize(f.type);
            return sz;
        }
        case TYPE_UNION: {
            auto ud = findUnion(t->structName);
            if (!ud) return 1;
            int sz = 0;
            for (auto &f : ud->fields) sz = max(sz, typeSize(f.type));
            return sz;
        }
    }
    return 1;
}

static int getFieldOffset(shared_ptr<Type> t, const string &field) {
    if (!t) return 0;
    if (t->kind == TYPE_PTR) t = t->sub;
    if (!t) return 0;
    if (t->kind == TYPE_STRUCT) {
        auto sd = findStruct(t->structName);
        if (!sd) return 0;
        int off = 0;
        for (auto &f : sd->fields) {
            if (f.name == field) return off;
            off += typeSize(f.type);
        }
    } else if (t->kind == TYPE_UNION) {
        auto ud = findUnion(t->structName);
        if (!ud) return 0;
        for (auto &f : ud->fields) {
            if (f.name == field) return 0;
        }
    }
    return 0;
}

static shared_ptr<Type> getFieldType(shared_ptr<Type> t, const string &field) {
    if (!t) return nullptr;
    if (t->kind == TYPE_PTR) t = t->sub;
    if (!t) return nullptr;
    if (t->kind == TYPE_STRUCT) {
        auto sd = findStruct(t->structName);
        if (!sd) return nullptr;
        for (auto &f : sd->fields) {
            if (f.name == field) return f.type;
        }
    } else if (t->kind == TYPE_UNION) {
        auto ud = findUnion(t->structName);
        if (!ud) return nullptr;
        for (auto &f : ud->fields) {
            if (f.name == field) return f.type;
        }
    }
    return nullptr;
}

bool Sema::isCopyType(shared_ptr<Type> t) {
    if (!t) return true;
    return t->kind == TYPE_INT || t->kind == TYPE_CHAR || t->kind == TYPE_BOOL
        || t->kind == TYPE_BYTE || t->kind == TYPE_UINT;
}

bool Sema::typeEqual(shared_ptr<Type> a, shared_ptr<Type> b) {
    if (!a || !b) return true;
    if (a->kind != b->kind) return false;
    if (a->kind == TYPE_STRUCT || a->kind == TYPE_UNION)
        return a->structName == b->structName;
    if (a->kind == TYPE_ARRAY) {
        if (a->arraySize != b->arraySize) return false;
        return typeEqual(a->sub, b->sub);
    }
    if (a->kind == TYPE_PTR || a->kind == TYPE_ARRAY)
        return typeEqual(a->sub, b->sub);
    return true;
}

bool Sema::typeCompatible(shared_ptr<Type> dst, shared_ptr<Type> src) {
    if (!dst || !src) return true;
    // JVAV is weakly typed; allow numeric coercion
    if (dst->kind == TYPE_INT && (src->kind == TYPE_INT || src->kind == TYPE_CHAR || src->kind == TYPE_BOOL || src->kind == TYPE_BYTE || src->kind == TYPE_UINT))
        return true;
    if (dst->kind == TYPE_UINT && (src->kind == TYPE_INT || src->kind == TYPE_CHAR || src->kind == TYPE_BOOL || src->kind == TYPE_BYTE || src->kind == TYPE_UINT))
        return true;
    if (dst->kind == TYPE_BOOL && (src->kind == TYPE_INT || src->kind == TYPE_BOOL || src->kind == TYPE_CHAR || src->kind == TYPE_BYTE || src->kind == TYPE_UINT))
        return true;
    if (dst->kind == TYPE_VOID && src->kind != TYPE_VOID) return false;
    // Pointer compatibility: allow any ptr to ptr cast
    if (dst->kind == TYPE_PTR && src->kind == TYPE_PTR) return true;
    if (dst->kind == TYPE_PTR && src->kind == TYPE_ARRAY) return true;
    if (dst->kind == TYPE_ARRAY && src->kind == TYPE_PTR) return true;
    if (dst->kind == TYPE_ARRAY && src->kind == TYPE_ARRAY)
        return typeEqual(dst->sub, src->sub);
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
    if (sym->isCopy) return;
    if (sym->mutBorrowed) {
        report(SEM_ERROR, "cannot move out of `" + name + "` because it is mutably borrowed", line, 1,
               "drop the mutable borrow before moving '" + name + "'");
        return;
    }
    if (sym->borrowCount > 0) {
        report(SEM_ERROR, "cannot move out of `" + name + "` because it is borrowed", line, 1,
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
        report(SEM_ERROR, "use of possibly-uninitialized variable `" + name + "`", line, 1,
               "assign a value to '" + name + "' before using it");
        return false;
    }
    if (sym->moved) {
        report(SEM_ERROR, "use of moved value `" + name + "`", line, 1,
               "reassign '" + name + "' or use a borrow (&" + name + ") instead");
        return false;
    }
    if (sym->mutBorrowed) {
        report(SEM_ERROR, "cannot use `" + name + "` because it is mutably borrowed", line, 1,
               "wait until the mutable borrow ends");
        return false;
    }
    return true;
}

bool Sema::checkBorrow(const string &name, bool mut_, int line) {
    Symbol *sym = lookup(name);
    if (!sym) return true;
    if (!sym->initialized) {
        report(SEM_ERROR, "cannot borrow uninitialized variable `" + name + "`", line, 1,
               "assign a value to '" + name + "' before borrowing");
        return false;
    }
    if (sym->moved) {
        report(SEM_ERROR, "cannot borrow moved value `" + name + "`", line, 1,
               "reassign '" + name + "' before borrowing");
        return false;
    }
    if (mut_) {
        if (sym->borrowCount > 0) {
            report(SEM_ERROR, "cannot mutably borrow `" + name + "` because it is already borrowed", line, 1,
                   "drop existing borrows before taking &mut");
            return false;
        }
        if (sym->mutBorrowed) {
            report(SEM_ERROR, "cannot mutably borrow `" + name + "` because it is already mutably borrowed", line, 1,
                   "only one &mut borrow is allowed at a time");
            return false;
        }
        sym->mutBorrowed = true;
    } else {
        if (sym->mutBorrowed) {
            report(SEM_ERROR, "cannot borrow `" + name + "` because it is mutably borrowed", line, 1,
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
            if (!name.empty() && name[0] != '_') {
                report(SEM_WARNING, "Unused variable '" + name + "'", 0, 1,
                       "remove the declaration or prefix with _ to suppress");
            }
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
    structDefs.clear();
    unionDefs.clear();

    // Pre-register all struct/union declarations
    for (auto &d : prog->decls) {
        if (d->kind == Decl::DECL_STRUCT) {
            registerStruct(dynamic_pointer_cast<StructDecl>(d));
        } else if (d->kind == Decl::DECL_UNION) {
            registerUnion(dynamic_pointer_cast<UnionDecl>(d));
        }
    }

    enterScope();
    auto intType = make_shared<Type>(Type{TYPE_INT});
    auto voidType = make_shared<Type>(Type{TYPE_VOID});
    declare("putint", Symbol::SYM_FUNC, intType, 0, true);
    declare("putchar", Symbol::SYM_FUNC, voidType, 0, true);
    auto ptrIntType = make_shared<Type>(Type{TYPE_PTR});
    ptrIntType->sub = make_shared<Type>(Type{TYPE_INT});
    declare("alloc", Symbol::SYM_FUNC, ptrIntType, 0, true);
    declare("free", Symbol::SYM_FUNC, voidType, 0, true);
    declare("exit", Symbol::SYM_FUNC, voidType, 0, true);
    declare("putstr", Symbol::SYM_FUNC, voidType, 0, true);
    declare("sleep", Symbol::SYM_FUNC, voidType, 0, true);

    for (auto &d : prog->decls) {
        checkDecl(d);
    }
    exitScope();
    return !hasErrors();
}

// ------------------------------------------------------------------
// Declarations
// ------------------------------------------------------------------
static bool hasReturn(shared_ptr<Stmt> s) {
    if (!s) return false;
    if (s->kind == Stmt::STMT_RETURN) return true;
    if (s->kind == Stmt::STMT_BLOCK) {
        for (auto &st : dynamic_pointer_cast<BlockStmt>(s)->stmts)
            if (hasReturn(st)) return true;
    }
    if (s->kind == Stmt::STMT_IF) {
        auto i = dynamic_pointer_cast<IfStmt>(s);
        return hasReturn(i->thenBranch) || hasReturn(i->elseBranch);
    }
    if (s->kind == Stmt::STMT_WHILE) {
        return hasReturn(dynamic_pointer_cast<WhileStmt>(s)->body);
    }
    if (s->kind == Stmt::STMT_FOR) {
        return hasReturn(dynamic_pointer_cast<ForStmt>(s)->body);
    }
    return false;
}

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
            if (pt->kind == TYPE_VOID) {
                report(SEM_ERROR, "cannot use `void` as a parameter type", fd->line, 1,
                       "use a concrete type such as `int`, `char`, or `bool`");
                return false;
            }
            if (!declare(p.name, Symbol::SYM_VAR, pt, scopeLevel, false)) return false;
            markInitialized(p.name);
        }
        checkStmt(fd->body);
        if (retType->kind != TYPE_VOID && !hasReturn(fd->body)) {
            report(SEM_ERROR, "missing return statement in function returning `" + typeStr(retType) + "`", fd->line, 1,
                   "add a return statement at the end of the function");
            return false;
        }
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
    } else if (d->kind == Decl::DECL_SYSCALL) {
        auto sd = dynamic_pointer_cast<SyscallDecl>(d);
        auto intType = make_shared<Type>(Type{TYPE_INT});
        declare(sd->name, Symbol::SYM_FUNC, intType, 0, true);
    } else if (d->kind == Decl::DECL_STRUCT) {
        auto sd = dynamic_pointer_cast<StructDecl>(d);
        // Validate fields
        for (auto &f : sd->fields) {
            if (f.type->kind == TYPE_VOID) {
                report(SEM_ERROR, "field `" + f.name + "` cannot have `void` type", sd->line, 1);
                return false;
            }
        }
    } else if (d->kind == Decl::DECL_UNION) {
        auto ud = dynamic_pointer_cast<UnionDecl>(d);
        for (auto &f : ud->fields) {
            if (f.type->kind == TYPE_VOID) {
                report(SEM_ERROR, "field `" + f.name + "` cannot have `void` type", ud->line, 1);
                return false;
            }
        }
    }
    return true;
}

bool Sema::processImport(shared_ptr<ImportDecl> id) {
    fs::path p = id->path;
    string absPath;
    if (p.is_absolute()) {
        absPath = fs::weakly_canonical(p).string();
    } else {
        bool found = false;
        vector<string> paths = importPaths;
        if (!basePath.empty()) paths.insert(paths.begin(), basePath);
        for (auto &dir : paths) {
            fs::path cand = fs::path(dir) / p;
            if (fs::exists(cand)) {
                absPath = fs::weakly_canonical(cand).string();
                found = true;
                break;
            }
        }
        if (!found) {
            absPath = fs::weakly_canonical(fs::path(basePath.empty() ? "." : basePath) / p).string();
        }
    }
    if (importedFiles.find(absPath) != importedFiles.end()) return true;
    importedFiles.insert(absPath);

    Lexer lex;
    if (!lex.tokenize(absPath)) {
        if (id->path.rfind("std/", 0) == 0 || id->path.rfind("std\\", 0) == 0) {
            report(SEM_ERROR, "cannot find standard library `" + id->path + "`", id->line, 1,
                   "ensure JVAV is installed correctly and the std/ directory is accessible from the compiler");
        } else {
            report(SEM_ERROR, "cannot read import file `" + id->path + "`", id->line, 1,
                   "check that the file exists and is readable");
        }
        return false;
    }
    FrontParser par;
    if (!par.parse(lex.getTokens())) {
        report(SEM_ERROR, "parse error in imported file `" + id->path + "`: " + par.getError(), id->line, 1, "");
        return false;
    }

    Sema subSema;
    string subBase = fs::path(absPath).parent_path().string();
    if (!subSema.analyze(par.getProgram(), subBase)) {
        report(SEM_ERROR, "semantic error in imported file `" + id->path + "`: " + subSema.getError(), id->line, 1, "");
        return false;
    }

    // Import struct/union definitions
    for (auto &subd : par.getProgram()->decls) {
        if (subd->kind == Decl::DECL_STRUCT) {
            auto sd = dynamic_pointer_cast<StructDecl>(subd);
            registerStruct(sd);
        } else if (subd->kind == Decl::DECL_UNION) {
            auto ud = dynamic_pointer_cast<UnionDecl>(subd);
            registerUnion(ud);
        }
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
        } else if (subd->kind == Decl::DECL_SYSCALL) {
            auto sd = dynamic_pointer_cast<SyscallDecl>(subd);
            declare(sd->name, Symbol::SYM_FUNC, make_shared<Type>(Type{TYPE_INT}), 0, true);
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
                    report(SEM_ERROR, "cannot return a value from a function with `void` return type", r->line, 1,
                           "remove the return value or change the function return type");
                }
            } else {
                if (curRetType && curRetType->kind != TYPE_VOID) {
                    report(SEM_ERROR, "missing return value in function with non-void return type", r->line, 1,
                           "add a return value of type " + typeStr(curRetType));
                }
            }
            break;
        }
        case Stmt::STMT_ASM: {
            // Inline assembly is opaque to semantic analysis
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
                report(SEM_ERROR, "cannot find value `" + id->name + "` in this scope", id->line, 1,
                       "declare '" + id->name + "' before use");
                return false;
            }
            if (sym->kind == Symbol::SYM_FUNC) {
                report(SEM_ERROR, "cannot use function `" + id->name + "` as a value", id->line, 1,
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
                report(SEM_ERROR, "cannot find function `" + fname + "` in this scope", c->line, 1,
                       "declare '" + fname + "' before calling");
                return false;
            }
            if (sym->kind != Symbol::SYM_FUNC) {
                report(SEM_ERROR, "`" + fname + "` is not a function", c->line, 1,
                       "use a function name for the call");
                return false;
            }
            for (auto &a : c->args) {
                checkExpr(a);
                if (a->kind == Expr::EXPR_IDENT) {
                    auto id = dynamic_pointer_cast<IdentExpr>(a);
                    Symbol *s = lookup(id->name);
                    if (s && !s->isCopy && s->kind == Symbol::SYM_VAR) {
                        if (fname != "fread" && fname != "fwrite" && fname != "putstr" && fname != "mmap_file") {
                            markMoved(id->name, c->line);
                        }
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
            auto baseType = i->base->type;
            if (baseType && (baseType->kind == TYPE_PTR || baseType->kind == TYPE_ARRAY)) {
                auto elemType = baseType->sub;
                if (elemType && (elemType->kind == TYPE_STRUCT || elemType->kind == TYPE_UNION)) {
                    // For struct/union elements, return pointer to element
                    i->type = make_shared<Type>(*baseType);
                } else {
                    i->type = elemType ? elemType : make_shared<Type>(Type{TYPE_INT});
                }
            } else {
                i->type = make_shared<Type>(Type{TYPE_INT});
            }
            break;
        }
        case Expr::EXPR_ASSIGN: {
            auto a = dynamic_pointer_cast<AssignExpr>(e);
            if (!checkAssignTarget(a->left)) return false;
            checkExpr(a->right);
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
                report(SEM_ERROR, "borrow expression must target a variable", b->line, 1,
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
        case Expr::EXPR_SIZEOF: {
            auto s = dynamic_pointer_cast<SizeofExpr>(e);
            if (s->targetType) {
                // Validate type exists
                if (s->targetType->kind == TYPE_STRUCT && !findStruct(s->targetType->structName)) {
                    report(SEM_ERROR, "unknown struct type `" + s->targetType->structName + "`", s->line, 1);
                    return false;
                }
                if (s->targetType->kind == TYPE_UNION && !findUnion(s->targetType->structName)) {
                    report(SEM_ERROR, "unknown union type `" + s->targetType->structName + "`", s->line, 1);
                    return false;
                }
            } else if (s->targetExpr) {
                checkExpr(s->targetExpr);
            }
            e->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_OFFSETOF: {
            auto o = dynamic_pointer_cast<OffsetofExpr>(e);
            if (o->targetType->kind == TYPE_STRUCT) {
                auto sd = findStruct(o->targetType->structName);
                if (!sd) {
                    report(SEM_ERROR, "unknown struct type `" + o->targetType->structName + "`", o->line, 1);
                    return false;
                }
                bool found = false;
                for (auto &f : sd->fields) {
                    if (f.name == o->field) { found = true; break; }
                }
                if (!found) {
                    report(SEM_ERROR, "struct `" + o->targetType->structName + "` has no field `" + o->field + "`", o->line, 1);
                    return false;
                }
            } else if (o->targetType->kind == TYPE_UNION) {
                auto ud = findUnion(o->targetType->structName);
                if (!ud) {
                    report(SEM_ERROR, "unknown union type `" + o->targetType->structName + "`", o->line, 1);
                    return false;
                }
                bool found = false;
                for (auto &f : ud->fields) {
                    if (f.name == o->field) { found = true; break; }
                }
                if (!found) {
                    report(SEM_ERROR, "union `" + o->targetType->structName + "` has no field `" + o->field + "`", o->line, 1);
                    return false;
                }
            } else {
                report(SEM_ERROR, "offsetof requires a struct or union type", o->line, 1);
                return false;
            }
            e->type = make_shared<Type>(Type{TYPE_INT});
            break;
        }
        case Expr::EXPR_CAST: {
            auto c = dynamic_pointer_cast<CastExpr>(e);
            checkExpr(c->operand);
            e->type = c->castType;
            break;
        }
        case Expr::EXPR_FIELD: {
            auto f = dynamic_pointer_cast<FieldExpr>(e);
            checkExpr(f->base);
            auto baseType = f->base->type;
            if (!baseType) {
                report(SEM_ERROR, "cannot access field on unknown type", f->line, 1);
                return false;
            }
            // Resolve through pointer if needed
            shared_ptr<Type> structType = baseType;
            if (structType->kind == TYPE_PTR) structType = structType->sub;
            if (!structType || (structType->kind != TYPE_STRUCT && structType->kind != TYPE_UNION)) {
                report(SEM_ERROR, "field access requires a struct or union type", f->line, 1);
                return false;
            }
            auto ft = getFieldType(structType, f->field);
            if (!ft) {
                report(SEM_ERROR, "type `" + typeStr(structType) + "` has no field `" + f->field + "`", f->line, 1);
                return false;
            }
            f->type = ft;
            break;
        }
    }
    return true;
}

bool Sema::checkAssignTarget(shared_ptr<Expr> e) {
    if (e->kind == Expr::EXPR_IDENT) {
        auto id = dynamic_pointer_cast<IdentExpr>(e);
        markUsed(id->name);
        return true;
    }
    if (e->kind == Expr::EXPR_INDEX) {
        auto idx = dynamic_pointer_cast<IndexExpr>(e);
        checkExpr(e);
        if (idx->base->kind == Expr::EXPR_IDENT) {
            auto id = dynamic_pointer_cast<IdentExpr>(idx->base);
            bool ok = checkUsable(id->name, e->line);
            markUsed(id->name);
            return ok;
        }
        return true;
    }
    if (e->kind == Expr::EXPR_FIELD) {
        auto f = dynamic_pointer_cast<FieldExpr>(e);
        checkExpr(e);
        return true;
    }
    report(SEM_ERROR, "invalid assignment target", e->line, 1,
           "only variables, index expressions, and field accesses can be assigned to");
    return false;
}

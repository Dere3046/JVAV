#include "codegen.hpp"
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <iostream>
namespace fs = std::filesystem;
using namespace std;

static string escapeString(const string &s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default:
                if (isprint((unsigned char)c)) out += c;
                else {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)c);
                    out += buf;
                }
        }
    }
    return out;
}

string CodeGenerator::nextLabel(const string &prefix) {
    return "." + prefix + "_" + to_string(labelCounter++);
}

void CodeGenerator::emit(const string &s) {
    out << s << "\n";
}

void CodeGenerator::collectLocals(shared_ptr<Stmt> s) {
    if (!s) return;
    switch (s->kind) {
        case Stmt::STMT_BLOCK: {
            auto b = dynamic_pointer_cast<BlockStmt>(s);
            for (auto &st : b->stmts) collectLocals(st);
            break;
        }
        case Stmt::STMT_VAR: {
            auto v = dynamic_pointer_cast<VarStmt>(s);
            localOffsets[v->name] = -(++localSize);
            break;
        }
        case Stmt::STMT_IF: {
            auto i = dynamic_pointer_cast<IfStmt>(s);
            collectLocals(i->thenBranch);
            if (i->elseBranch) collectLocals(i->elseBranch);
            break;
        }
        case Stmt::STMT_WHILE: {
            auto w = dynamic_pointer_cast<WhileStmt>(s);
            collectLocals(w->body);
            break;
        }
        case Stmt::STMT_FOR: {
            auto f = dynamic_pointer_cast<ForStmt>(s);
            if (f->init) collectLocals(f->init);
            collectLocals(f->body);
            break;
        }
        default: break;
    }
}

void CodeGenerator::loadVar(const string &name, int reg) {
    auto cit = constValues.find(name);
    if (cit != constValues.end()) {
        emit("    LDI R" + to_string(reg) + ", " + to_string(cit->second));
        return;
    }
    auto it = localOffsets.find(name);
    if (it != localOffsets.end()) {
        emit("    LDI R4, " + to_string(it->second));
        emit("    ADD R4, R6, R4");
        emit("    LDR R" + to_string(reg) + ", [R4]");
    } else {
        emit("    LDR R" + to_string(reg) + ", [" + name + "]");
    }
}

void CodeGenerator::storeVar(const string &name, int reg) {
    auto it = localOffsets.find(name);
    if (it != localOffsets.end()) {
        emit("    LDI R4, " + to_string(it->second));
        emit("    ADD R4, R6, R4");
        emit("    STR [R4], R" + to_string(reg));
    } else {
        emit("    STR [" + name + "], R" + to_string(reg));
    }
}

void CodeGenerator::genFuncDecl(shared_ptr<FuncDecl> d) {
    localOffsets.clear();
    localSize = 0;
    curFuncRetLabel = "." + d->name + "_ret";

    for (size_t i = 0; i < d->params.size(); i++) {
        localOffsets[d->params[i].name] = (int)(i + 5);
    }

    collectLocals(d->body);

    emit("    .global " + d->name);
    emit(d->name + ":");
    emit("    PUSH R6");
    emit("    PUSH R1");
    emit("    PUSH R2");
    emit("    PUSH R3");
    emit("    MOV R6, SP");

    for (size_t i = 0; i < d->params.size() && i < 4; i++) {
        int off = localOffsets[d->params[i].name];
        emit("    LDI R4, " + to_string(off));
        emit("    ADD R4, R6, R4");
        emit("    STR [R4], R" + to_string((int)i));
    }

    if (localSize > 0) {
        emit("    LDI R4, " + to_string(localSize));
        emit("    SUB SP, SP, R4");
    }

    genStmt(d->body);

    emit(curFuncRetLabel + ":");
    emit("    MOV SP, R6");
    emit("    POP R3");
    emit("    POP R2");
    emit("    POP R1");
    emit("    POP R6");
    emit("    RET");
}

void CodeGenerator::genStmt(shared_ptr<Stmt> s) {
    if (!s) return;
    switch (s->kind) {
        case Stmt::STMT_BLOCK: {
            auto b = dynamic_pointer_cast<BlockStmt>(s);
            for (auto &st : b->stmts) genStmt(st);
            break;
        }
        case Stmt::STMT_VAR: {
            auto v = dynamic_pointer_cast<VarStmt>(s);
            if (v->init) {
                genExpr(v->init, 0);
                storeVar(v->name, 0);
            }
            break;
        }
        case Stmt::STMT_CONST: {
            auto c = dynamic_pointer_cast<ConstStmt>(s);
            genExpr(c->value, 0);
            storeVar(c->name, 0);
            break;
        }
        case Stmt::STMT_EXPR: {
            auto e = dynamic_pointer_cast<ExprStmt>(s);
            genExpr(e->expr, 0);
            break;
        }
        case Stmt::STMT_IF: {
            auto i = dynamic_pointer_cast<IfStmt>(s);
            string elseLabel = nextLabel("else");
            string endLabel = nextLabel("endif");
            genCondJump(i->cond, elseLabel);
            genStmt(i->thenBranch);
            emit("    JMP " + endLabel);
            emit(elseLabel + ":");
            if (i->elseBranch) genStmt(i->elseBranch);
            emit(endLabel + ":");
            break;
        }
        case Stmt::STMT_WHILE: {
            auto w = dynamic_pointer_cast<WhileStmt>(s);
            string loopLabel = nextLabel("loop");
            string endLabel = nextLabel("endwhile");
            emit(loopLabel + ":");
            genCondJump(w->cond, endLabel);
            genStmt(w->body);
            emit("    JMP " + loopLabel);
            emit(endLabel + ":");
            break;
        }
        case Stmt::STMT_FOR: {
            auto f = dynamic_pointer_cast<ForStmt>(s);
            string loopLabel = nextLabel("loop");
            string endLabel = nextLabel("endfor");
            if (f->init) genStmt(f->init);
            emit(loopLabel + ":");
            if (f->cond) genCondJump(f->cond, endLabel);
            genStmt(f->body);
            if (f->step) genExpr(f->step, 0);
            emit("    JMP " + loopLabel);
            emit(endLabel + ":");
            break;
        }
        case Stmt::STMT_RETURN: {
            auto r = dynamic_pointer_cast<ReturnStmt>(s);
            if (r->value) {
                genExpr(r->value, 0);
            }
            emit("    JMP " + curFuncRetLabel);
            break;
        }
        case Stmt::STMT_ASM: {
            auto a = dynamic_pointer_cast<InlineAsmStmt>(s);
            for (auto &ins : a->instructions) {
                emit("    " + ins);
            }
            break;
        }
    }
}

void CodeGenerator::genCondJump(shared_ptr<Expr> e, const string &falseLabel) {
    genExpr(e, 0);
    emit("    LDI R4, 0");
    emit("    CMP R0, R4");
    emit("    JE " + falseLabel);
}

static bool exprHasCall(shared_ptr<Expr> e) {
    if (!e) return false;
    switch (e->kind) {
        case Expr::EXPR_CALL: return true;
        case Expr::EXPR_BINARY: {
            auto b = dynamic_pointer_cast<BinaryExpr>(e);
            return exprHasCall(b->left) || exprHasCall(b->right);
        }
        case Expr::EXPR_UNARY: {
            auto u = dynamic_pointer_cast<UnaryExpr>(e);
            return exprHasCall(u->operand);
        }
        case Expr::EXPR_ASSIGN: {
            auto a = dynamic_pointer_cast<AssignExpr>(e);
            return exprHasCall(a->left) || exprHasCall(a->right);
        }
        case Expr::EXPR_INDEX: {
            auto i = dynamic_pointer_cast<IndexExpr>(e);
            return exprHasCall(i->base) || exprHasCall(i->index);
        }
        case Expr::EXPR_BORROW: {
            auto b = dynamic_pointer_cast<BorrowExpr>(e);
            return exprHasCall(b->operand);
        }
        case Expr::EXPR_CAST: {
            auto c = dynamic_pointer_cast<CastExpr>(e);
            return exprHasCall(c->operand);
        }
        case Expr::EXPR_FIELD: {
            auto f = dynamic_pointer_cast<FieldExpr>(e);
            return exprHasCall(f->base);
        }
        default: return false;
    }
}

// Forward declarations for helpers used in codegen
extern map<string, shared_ptr<StructDecl>> structDefs;
extern map<string, shared_ptr<UnionDecl>> unionDefs;
static int typeSize(shared_ptr<Type> t);
static int getFieldOffset(shared_ptr<Type> t, const string &field);

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
            return 1;
        }
        case TYPE_STRUCT: {
            auto it = structDefs.find(t->structName);
            if (it == structDefs.end()) return 1;
            int sz = 0;
            for (auto &f : it->second->fields) sz += typeSize(f.type);
            return sz;
        }
        case TYPE_UNION: {
            auto it = unionDefs.find(t->structName);
            if (it == unionDefs.end()) return 1;
            int sz = 0;
            for (auto &f : it->second->fields) sz = max(sz, typeSize(f.type));
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
        auto it = structDefs.find(t->structName);
        if (it == structDefs.end()) return 0;
        int off = 0;
        for (auto &f : it->second->fields) {
            if (f.name == field) return off;
            off += typeSize(f.type);
        }
    } else if (t->kind == TYPE_UNION) {
        auto it = unionDefs.find(t->structName);
        if (it == unionDefs.end()) return 0;
        for (auto &f : it->second->fields) {
            if (f.name == field) return 0;
        }
    }
    return 0;
}

static Int128 evaluateConstExpr(shared_ptr<Expr> e) {
    if (!e) return 0;
    switch (e->kind) {
        case Expr::EXPR_NUMBER:
            return dynamic_pointer_cast<NumberExpr>(e)->value;
        case Expr::EXPR_SIZEOF: {
            auto s = dynamic_pointer_cast<SizeofExpr>(e);
            if (s->targetType) return typeSize(s->targetType);
            if (s->targetExpr && s->targetExpr->type) return typeSize(s->targetExpr->type);
            return 1;
        }
        case Expr::EXPR_OFFSETOF: {
            auto o = dynamic_pointer_cast<OffsetofExpr>(e);
            return getFieldOffset(o->targetType, o->field);
        }
        case Expr::EXPR_BINARY: {
            auto b = dynamic_pointer_cast<BinaryExpr>(e);
            Int128 l = evaluateConstExpr(b->left);
            Int128 r = evaluateConstExpr(b->right);
            if (b->op == "+") return l + r;
            if (b->op == "-") return l - r;
            if (b->op == "*") return l * r;
            if (b->op == "/") return r != 0 ? l / r : 0;
            if (b->op == "%") return r != 0 ? l % r : 0;
            if (b->op == "&") return l & r;
            if (b->op == "|") return l | r;
            if (b->op == "^") return l ^ r;
            if (b->op == "<<") return l << (int)(long long)r;
            if (b->op == ">>") return l >> (int)(long long)r;
            return 0;
        }
        case Expr::EXPR_UNARY: {
            auto u = dynamic_pointer_cast<UnaryExpr>(e);
            Int128 v = evaluateConstExpr(u->operand);
            if (u->op == "-") return -v;
            if (u->op == "~") return ~v;
            return v;
        }
        default: return 0;
    }
}

void CodeGenerator::genExpr(shared_ptr<Expr> e, int destReg) {
    if (!e) return;
    switch (e->kind) {
        case Expr::EXPR_NUMBER: {
            auto n = dynamic_pointer_cast<NumberExpr>(e);
            emit("    LDI R" + to_string(destReg) + ", " + to_string((long long)n->value));
            break;
        }
        case Expr::EXPR_CHAR: {
            auto c = dynamic_pointer_cast<CharExpr>(e);
            emit("    LDI R" + to_string(destReg) + ", " + to_string((int)c->value));
            break;
        }
        case Expr::EXPR_BOOL: {
            auto b = dynamic_pointer_cast<BoolExpr>(e);
            emit("    LDI R" + to_string(destReg) + ", " + to_string(b->value ? 1 : 0));
            break;
        }
        case Expr::EXPR_STRING: {
            auto str = dynamic_pointer_cast<StringExpr>(e);
            string lbl = ".str_" + to_string(stringCounter++);
            stringLabels.push_back(lbl + ":\n    DB \"" + escapeString(str->value) + "\", 0");
            emit("    LDI R" + to_string(destReg) + ", " + lbl);
            break;
        }
        case Expr::EXPR_IDENT: {
            auto id = dynamic_pointer_cast<IdentExpr>(e);
            loadVar(id->name, destReg);
            break;
        }
        case Expr::EXPR_BINARY: {
            auto b = dynamic_pointer_cast<BinaryExpr>(e);
            string op = b->op;
            if (op == "&&") {
                string falseLabel = nextLabel("and_false");
                string endLabel = nextLabel("and_end");
                genExpr(b->left, destReg);
                emit("    LDI R4, 0");
                emit("    CMP R" + to_string(destReg) + ", R4");
                emit("    JE " + falseLabel);
                genExpr(b->right, destReg);
                emit("    JMP " + endLabel);
                emit(falseLabel + ":");
                emit("    LDI R" + to_string(destReg) + ", 0");
                emit(endLabel + ":");
            } else if (op == "||") {
                string trueLabel = nextLabel("or_true");
                string endLabel = nextLabel("or_end");
                genExpr(b->left, destReg);
                emit("    LDI R4, 0");
                emit("    CMP R" + to_string(destReg) + ", R4");
                emit("    JNE " + trueLabel);
                genExpr(b->right, destReg);
                emit("    JMP " + endLabel);
                emit(trueLabel + ":");
                emit("    LDI R" + to_string(destReg) + ", 1");
                emit(endLabel + ":");
            } else {
                int r2 = destReg + 1;
                if (r2 > 3) r2 = 0;
                bool saveLeft = exprHasCall(b->right);
                genExpr(b->left, destReg);
                if (saveLeft) {
                    emit("    PUSH R" + to_string(destReg));
                }
                genExpr(b->right, r2);
                if (saveLeft) {
                    emit("    POP R" + to_string(destReg));
                }
                if (op == "+") emit("    ADD R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "-") emit("    SUB R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "*") emit("    MUL R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "/") emit("    DIV R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "%") emit("    MOD R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "&") emit("    AND R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "|") emit("    OR R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "^") emit("    XOR R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == "<<") emit("    SHL R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else if (op == ">>") emit("    SHR R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
                else {
                    emit("    CMP R" + to_string(destReg) + ", R" + to_string(r2));
                    string trueLabel = nextLabel("cmp_true");
                    string endLabel = nextLabel("cmp_end");
                    if (op == "==") emit("    JE " + trueLabel);
                    else if (op == "!=") emit("    JNE " + trueLabel);
                    else if (op == "<") emit("    JL " + trueLabel);
                    else if (op == ">") emit("    JG " + trueLabel);
                    else if (op == "<=") {
                        emit("    JE " + trueLabel);
                        emit("    JL " + trueLabel);
                    }
                    else if (op == ">=") {
                        emit("    JE " + trueLabel);
                        emit("    JG " + trueLabel);
                    }
                    emit("    LDI R" + to_string(destReg) + ", 0");
                    emit("    JMP " + endLabel);
                    emit(trueLabel + ":");
                    emit("    LDI R" + to_string(destReg) + ", 1");
                    emit(endLabel + ":");
                }
            }
            break;
        }
        case Expr::EXPR_UNARY: {
            auto u = dynamic_pointer_cast<UnaryExpr>(e);
            genExpr(u->operand, destReg);
            if (u->op == "-") {
                emit("    LDI R4, 0");
                emit("    SUB R" + to_string(destReg) + ", R4, R" + to_string(destReg));
            } else if (u->op == "!") {
                string zeroLabel = nextLabel("not_zero");
                string endLabel = nextLabel("not_end");
                emit("    LDI R4, 0");
                emit("    CMP R" + to_string(destReg) + ", R4");
                emit("    JE " + zeroLabel);
                emit("    LDI R" + to_string(destReg) + ", 0");
                emit("    JMP " + endLabel);
                emit(zeroLabel + ":");
                emit("    LDI R" + to_string(destReg) + ", 1");
                emit(endLabel + ":");
            } else if (u->op == "~") {
                emit("    NOT R" + to_string(destReg) + ", R" + to_string(destReg));
            }
            break;
        }
        case Expr::EXPR_CALL: {
            auto c = dynamic_pointer_cast<CallExpr>(e);
            string name;
            if (c->callee->kind == Expr::EXPR_IDENT) {
                name = dynamic_pointer_cast<IdentExpr>(c->callee)->name;
            }

            int n = (int)c->args.size();
            for (auto &arg : c->args) {
                genExpr(arg, 0);
                emit("    PUSH R0");
            }
            for (int i = 0; i < min(n, 4); i++) {
                int off = n - 1 - i;
                emit("    LDI R4, " + to_string(off));
                emit("    ADD R4, SP, R4");
                emit("    LDR R" + to_string(i) + ", [R4]");
            }
            emit("    CALL " + name);
            if (n > 0) {
                emit("    LDI R4, " + to_string(n));
                emit("    ADD SP, SP, R4");
            }
            if (destReg != 0) {
                emit("    MOV R" + to_string(destReg) + ", R0");
            }
            break;
        }
        case Expr::EXPR_BORROW: {
            auto b = dynamic_pointer_cast<BorrowExpr>(e);
            if (b->operand->kind == Expr::EXPR_IDENT) {
                auto id = dynamic_pointer_cast<IdentExpr>(b->operand);
                auto it = localOffsets.find(id->name);
                if (it != localOffsets.end()) {
                    emit("    LDI R" + to_string(destReg) + ", " + to_string(it->second));
                    emit("    ADD R" + to_string(destReg) + ", R6, R" + to_string(destReg));
                } else {
                    emit("    ; unknown borrow target " + id->name);
                }
            } else {
                emit("    ; borrow of non-ident not supported");
            }
            break;
        }
        case Expr::EXPR_INDEX: {
            auto i = dynamic_pointer_cast<IndexExpr>(e);
            genExpr(i->base, destReg);
            int r2 = (destReg + 1) <= 3 ? destReg + 1 : 3;
            genExpr(i->index, r2);
            // Scale index by element size if needed
            auto baseType = i->base->type;
            if (baseType && (baseType->kind == TYPE_PTR || baseType->kind == TYPE_ARRAY) && baseType->sub) {
                int elemSize = typeSize(baseType->sub);
                if (elemSize > 1) {
                    emit("    LDI R4, " + to_string(elemSize));
                    emit("    MUL R" + to_string(r2) + ", R" + to_string(r2) + ", R4");
                }
            }
            emit("    ADD R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
            // Only load if result is not a pointer/struct/union
            bool shouldLoad = true;
            if (i->type) {
                auto tk = i->type->kind;
                if (tk == TYPE_PTR || tk == TYPE_STRUCT || tk == TYPE_UNION) shouldLoad = false;
            }
            if (shouldLoad) {
                emit("    LDR R" + to_string(destReg) + ", [R" + to_string(destReg) + "]");
            }
            break;
        }
        case Expr::EXPR_ASSIGN: {
            auto a = dynamic_pointer_cast<AssignExpr>(e);
            genExpr(a->right, 0);
            if (a->left->kind == Expr::EXPR_IDENT) {
                auto id = dynamic_pointer_cast<IdentExpr>(a->left);
                storeVar(id->name, 0);
            } else if (a->left->kind == Expr::EXPR_INDEX) {
                auto idx = dynamic_pointer_cast<IndexExpr>(a->left);
                int raddr = (destReg + 1) <= 3 ? destReg + 1 : 3;
                genExpr(idx->base, raddr);
                int ridx = (raddr + 1) <= 3 ? raddr + 1 : 3;
                genExpr(idx->index, ridx);
                auto baseType = idx->base->type;
                if (baseType && (baseType->kind == TYPE_PTR || baseType->kind == TYPE_ARRAY) && baseType->sub) {
                    int elemSize = typeSize(baseType->sub);
                    if (elemSize > 1) {
                        emit("    LDI R4, " + to_string(elemSize));
                        emit("    MUL R" + to_string(ridx) + ", R" + to_string(ridx) + ", R4");
                    }
                }
                emit("    ADD R" + to_string(raddr) + ", R" + to_string(raddr) + ", R" + to_string(ridx));
                emit("    STR [R" + to_string(raddr) + "], R0");
            } else if (a->left->kind == Expr::EXPR_FIELD) {
                auto f = dynamic_pointer_cast<FieldExpr>(a->left);
                int raddr = (destReg + 1) <= 3 ? destReg + 1 : 3;
                genExpr(f->base, raddr);
                int offset = getFieldOffset(f->base->type, f->field);
                if (offset > 0) {
                    emit("    LDI R4, " + to_string(offset));
                    emit("    ADD R" + to_string(raddr) + ", R" + to_string(raddr) + ", R4");
                }
                emit("    STR [R" + to_string(raddr) + "], R0");
            }
            if (destReg != 0) {
                emit("    MOV R" + to_string(destReg) + ", R0");
            }
            break;
        }
        case Expr::EXPR_SIZEOF: {
            auto s = dynamic_pointer_cast<SizeofExpr>(e);
            int sz = 1;
            if (s->targetType) sz = typeSize(s->targetType);
            else if (s->targetExpr && s->targetExpr->type) sz = typeSize(s->targetExpr->type);
            emit("    LDI R" + to_string(destReg) + ", " + to_string(sz));
            break;
        }
        case Expr::EXPR_OFFSETOF: {
            auto o = dynamic_pointer_cast<OffsetofExpr>(e);
            int off = getFieldOffset(o->targetType, o->field);
            emit("    LDI R" + to_string(destReg) + ", " + to_string(off));
            break;
        }
        case Expr::EXPR_CAST: {
            auto c = dynamic_pointer_cast<CastExpr>(e);
            genExpr(c->operand, destReg);
            // Casts are no-ops at the machine level; type system already validated
            break;
        }
        case Expr::EXPR_FIELD: {
            auto f = dynamic_pointer_cast<FieldExpr>(e);
            genExpr(f->base, destReg);
            int offset = getFieldOffset(f->base->type, f->field);
            if (offset > 0) {
                emit("    LDI R4, " + to_string(offset));
                emit("    ADD R" + to_string(destReg) + ", R" + to_string(destReg) + ", R4");
            }
            // Load field value unless it's a struct/union/array type (which we want the address of)
            bool shouldLoad = true;
            if (f->type) {
                auto tk = f->type->kind;
                if (tk == TYPE_STRUCT || tk == TYPE_UNION || tk == TYPE_ARRAY) shouldLoad = false;
            }
            if (shouldLoad) {
                emit("    LDR R" + to_string(destReg) + ", [R" + to_string(destReg) + "]");
            }
            break;
        }
    }
}

void CodeGenerator::genProgram(shared_ptr<Program> prog) {
    // First pass: emit .equ for consts and collect data
    vector<shared_ptr<GlobalConstDecl>> consts;
    for (auto &d : prog->decls) {
        if (d->kind == Decl::DECL_CONST) {
            auto cd = dynamic_pointer_cast<GlobalConstDecl>(d);
            Int128 val = evaluateConstExpr(cd->value);
            emit("    " + cd->name + ": EQU " + to_string((long long)val));
            constValues[cd->name] = (long long)val;
            consts.push_back(cd);
        } else if (d->kind == Decl::DECL_VAR) {
            auto vd = dynamic_pointer_cast<GlobalVarDecl>(d);
            emit("    .data");
            emit(vd->name + ":");
            if (vd->init && vd->init->kind == Expr::EXPR_NUMBER) {
                auto n = dynamic_pointer_cast<NumberExpr>(vd->init);
                emit("    DT " + to_string((long long)n->value));
            } else {
                emit("    DT 0");
            }
            emit("    .text");
        } else if (d->kind == Decl::DECL_IMPORT) {
            auto id = dynamic_pointer_cast<ImportDecl>(d);
            if (!id->module) continue;
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
            if (generatedFiles.find(absPath) != generatedFiles.end()) continue;
            if (id->module && generatedModules.find(id->module.get()) != generatedModules.end()) continue;
            generatedFiles.insert(absPath);
            if (id->module) generatedModules.insert(id->module.get());
            genProgram(id->module);
        }
    }

    for (auto &d : prog->decls) {
        if (d->kind == Decl::DECL_FUNC) {
            genFuncDecl(dynamic_pointer_cast<FuncDecl>(d));
        } else if (d->kind == Decl::DECL_SYSCALL) {
            auto sd = dynamic_pointer_cast<SyscallDecl>(d);
            userSyscalls.push_back("    .syscall " + sd->name + ", " + to_string(sd->cmdId) + ", " + to_string(sd->argCount));
        }
    }
}

string CodeGenerator::generate(shared_ptr<Program> prog) {
    return generate(prog, "");
}

string CodeGenerator::generate(shared_ptr<Program> prog, const string &bp) {
    out.str("");
    labelCounter = 0;
    stringCounter = 0;
    stringLabels.clear();
    generatedFiles.clear();
    generatedModules.clear();
    userSyscalls.clear();
    constValues.clear();
    basePath = bp;

    emit("    .global _start");
    emit("_start:");
    emit("    CALL main");
    emit("    HALT");

    genProgram(prog);

    emit("    .syscall putchar, 14, 1");
    emit("    .syscall putint, 15, 1");
    emit("    .syscall getchar, 16, 0");
    emit("    .syscall getint, 17, 0");
    emit("    .syscall alloc, 12, 1");
    emit("    .syscall free, 13, 1");
    emit("    .syscall exit, 18, 1");
    emit("    .syscall putstr, 19, 2");
    emit("    .syscall sleep, 20, 1");
    for (auto &s : userSyscalls) emit(s);

    if (!stringLabels.empty()) {
        emit("    .data");
        for (auto &sl : stringLabels) emit(sl);
    }

    return out.str();
}

#include "codegen.hpp"
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
using namespace std;

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
    auto it = localOffsets.find(name);
    if (it != localOffsets.end()) {
        emit("    LDI R4, " + to_string(it->second));
        emit("    ADD R4, R6, R4");
        emit("    LDR R" + to_string(reg) + ", [R4]");
    } else {
        emit("    ; unknown var " + name);
    }
}

void CodeGenerator::storeVar(const string &name, int reg) {
    auto it = localOffsets.find(name);
    if (it != localOffsets.end()) {
        emit("    LDI R4, " + to_string(it->second));
        emit("    ADD R4, R6, R4");
        emit("    STR [R4], R" + to_string(reg));
    } else {
        emit("    ; unknown var " + name);
    }
}

void CodeGenerator::genFuncDecl(shared_ptr<FuncDecl> d) {
    localOffsets.clear();
    localSize = 0;
    curFuncRetLabel = "." + d->name + "_ret";

    // parameter offsets: FP+2, FP+3, ...
    for (size_t i = 0; i < d->params.size(); i++) {
        localOffsets[d->params[i].name] = (int)(i + 2);
    }

    // collect local variable offsets
    collectLocals(d->body);

    emit("    .global " + d->name);
    emit(d->name + ":");
    emit("    PUSH R6");
    emit("    MOV R6, SP");

    // save register args to stack slots
    for (size_t i = 0; i < d->params.size() && i < 4; i++) {
        int off = localOffsets[d->params[i].name];
        emit("    LDI R4, " + to_string(off));
        emit("    ADD R4, R6, R4");
        emit("    STR [R4], R" + to_string((int)i));
    }

    // allocate locals
    if (localSize > 0) {
        emit("    LDI R4, " + to_string(localSize));
        emit("    SUB SP, SP, R4");
    }

    genStmt(d->body);

    emit(curFuncRetLabel + ":");
    emit("    MOV SP, R6");
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
    }
}

void CodeGenerator::genCondJump(shared_ptr<Expr> e, const string &falseLabel) {
    // Evaluate condition, jump to falseLabel if false (== 0)
    genExpr(e, 0);
    emit("    LDI R4, 0");
    emit("    CMP R0, R4");
    emit("    JE " + falseLabel);
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
            stringLabels.push_back(lbl + ":\n    DB \"" + str->value + "\"");
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
            // special handling for logical operators
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
                if (r2 > 3) r2 = 3; // fallback, should not happen for simple cases
                genExpr(b->left, destReg);
                genExpr(b->right, r2);
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
                    // comparison operators: generate 0 or 1
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

            // Special handling for putint / putchar
            if (name == "putint" || name == "putchar") {
                if (!c->args.empty()) {
                    genExpr(c->args[0], 0);
                }
                string addr = (name == "putint") ? "0xFFF2" : "0xFFF0";
                emit("    STR [" + addr + "], R0");
                break;
            }

            // Save args to stack
            int n = (int)c->args.size();
            for (auto &arg : c->args) {
                genExpr(arg, 0);
                emit("    PUSH R0");
            }
            // Load from stack to R0-R3
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
            emit("    ADD R" + to_string(destReg) + ", R" + to_string(destReg) + ", R" + to_string(r2));
            emit("    LDR R" + to_string(destReg) + ", [R" + to_string(destReg) + "]");
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
                emit("    ADD R" + to_string(raddr) + ", R" + to_string(raddr) + ", R" + to_string(ridx));
                emit("    STR [R" + to_string(raddr) + "], R0");
            }
            if (destReg != 0) {
                emit("    MOV R" + to_string(destReg) + ", R0");
            }
            break;
        }
    }
}

void CodeGenerator::genProgram(shared_ptr<Program> prog) {
    // Collect global vars/strings (simplified)
    for (auto &d : prog->decls) {
        if (d->kind == Decl::DECL_VAR) {
            auto vd = dynamic_pointer_cast<GlobalVarDecl>(d);
            emit("    .data");
            emit(vd->name + ":");
            emit("    DT 0");
            emit("    .text");
        } else if (d->kind == Decl::DECL_CONST) {
            auto cd = dynamic_pointer_cast<GlobalConstDecl>(d);
            // global consts are inlined, skip
        } else if (d->kind == Decl::DECL_IMPORT) {
            auto id = dynamic_pointer_cast<ImportDecl>(d);
            if (!id->module) continue;
            fs::path p = id->path;
            if (!basePath.empty() && p.is_relative()) {
                p = fs::path(basePath) / p;
            }
            string absPath = fs::weakly_canonical(p).string();
            if (generatedFiles.find(absPath) != generatedFiles.end()) continue;
            generatedFiles.insert(absPath);
            genProgram(id->module);
        }
    }

    for (auto &d : prog->decls) {
        if (d->kind == Decl::DECL_FUNC) {
            genFuncDecl(dynamic_pointer_cast<FuncDecl>(d));
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
    basePath = bp;

    // Generate entry point
    emit("    .global _start");
    emit("_start:");
    emit("    CALL main");
    emit("    HALT");

    genProgram(prog);

    // Append string literals
    if (!stringLabels.empty()) {
        emit("    .data");
        for (auto &sl : stringLabels) emit(sl);
    }

    return out.str();
}

#include "test_utils.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>

using namespace std;

static shared_ptr<Program> parse(const string &src, string &err) {
    ofstream("tmp.jvl") << src;
    Lexer lex; lex.tokenize("tmp.jvl");
    FrontParser par;
    bool ok = par.parse(lex.getTokens());
    remove("tmp.jvl");
    err = par.getError();
    return ok ? par.getProgram() : nullptr;
}

static bool parse_ok(const string &src, string &err) {
    return parse(src, err) != nullptr;
}

int test_parser_main() {
    // --- Original tests ---
    test_header("parser_empty");
    {
        string err;
        auto prog = parse("", err);
        TEST_ASSERT(prog != nullptr, "parse empty");
        TEST_ASSERT(prog->decls.empty(), "empty decls");
    }
    test_passed("parser_empty");

    test_header("parser_func");
    {
        string err;
        auto prog = parse("func add(a: int, b: int): int { return a + b; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        TEST_ASSERT_EQ(prog->decls.size(), 1, "one decl");
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        TEST_ASSERT(fd != nullptr, "func decl");
        TEST_ASSERT(fd->name == "add", "name");
        TEST_ASSERT_EQ(fd->params.size(), 2, "params");
    }
    test_passed("parser_func");

    test_header("parser_var");
    {
        string err;
        auto prog = parse("func main(): int { var x: int = 5; return x; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto body = fd->body;
        TEST_ASSERT_EQ(body->stmts.size(), 2, "two stmts");
        auto vs = dynamic_pointer_cast<VarStmt>(body->stmts[0]);
        TEST_ASSERT(vs != nullptr, "var stmt");
        TEST_ASSERT(vs->name == "x", "x");
    }
    test_passed("parser_var");

    test_header("parser_expr_precedence");
    {
        string err;
        auto prog = parse("func main(): int { return 1 + 2 * 3; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto ret = dynamic_pointer_cast<ReturnStmt>(fd->body->stmts[0]);
        auto bin = dynamic_pointer_cast<BinaryExpr>(ret->value);
        TEST_ASSERT(bin != nullptr, "binary");
        TEST_ASSERT(bin->op == "+", "+ at top");
    }
    test_passed("parser_expr_precedence");

    test_header("parser_import");
    {
        string err;
        auto prog = parse("import \"foo.jvl\"; func main() {}", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        TEST_ASSERT_EQ(prog->decls.size(), 2, "two decls");
        auto imp = dynamic_pointer_cast<ImportDecl>(prog->decls[0]);
        TEST_ASSERT(imp != nullptr, "import decl");
        TEST_ASSERT(imp->path == "foo.jvl", "path");
    }
    test_passed("parser_import");

    // --- New comprehensive tests ---

    test_header("parser_all_types");
    {
        string err;
        auto prog = parse(
            "func f1(a: int): int { return a; }\n"
            "func f2(a: char): char { return a; }\n"
            "func f3(a: bool): bool { return a; }\n"
            "func f4(a: ptr<int>): ptr<int> { return a; }\n"
            "func f5(a: array<int>): array<int> { return a; }\n"
            "func f6(): void { }\n", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        TEST_ASSERT_EQ(prog->decls.size(), 6, "six decls");
    }
    test_passed("parser_all_types");

    test_header("parser_ptr_nested");
    {
        string err;
        auto prog = parse("func main(): int { var p: ptr<ptr<int> > = alloc(1); return 0; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto vs = dynamic_pointer_cast<VarStmt>(fd->body->stmts[0]);
        TEST_ASSERT(vs->varType->kind == TYPE_PTR, "ptr");
        TEST_ASSERT(vs->varType->sub->kind == TYPE_PTR, "ptr-ptr");
        TEST_ASSERT(vs->varType->sub->sub->kind == TYPE_INT, "ptr-ptr-int");
    }
    test_passed("parser_ptr_nested");

    test_header("parser_const");
    {
        string err;
        auto prog = parse("func main(): int { const MAX = 100; return MAX; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto cs = dynamic_pointer_cast<ConstStmt>(fd->body->stmts[0]);
        TEST_ASSERT(cs != nullptr, "const stmt");
        TEST_ASSERT(cs->name == "MAX", "MAX");
    }
    test_passed("parser_const");

    test_header("parser_if");
    {
        string err;
        auto prog = parse("func main(): int { if (true) { return 1; } else { return 0; } }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto ifs = dynamic_pointer_cast<IfStmt>(fd->body->stmts[0]);
        TEST_ASSERT(ifs != nullptr, "if stmt");
        TEST_ASSERT(ifs->elseBranch != nullptr, "else branch");
    }
    test_passed("parser_if");

    test_header("parser_while");
    {
        string err;
        auto prog = parse("func main(): int { while (true) { break; } return 0; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto ws = dynamic_pointer_cast<WhileStmt>(fd->body->stmts[0]);
        TEST_ASSERT(ws != nullptr, "while stmt");
    }
    test_passed("parser_while");

    test_header("parser_for");
    {
        string err;
        auto prog = parse("func main(): int { for (var i = 0; i < 10; i = i + 1) { putint(i); } return 0; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto fs = dynamic_pointer_cast<ForStmt>(fd->body->stmts[0]);
        TEST_ASSERT(fs != nullptr, "for stmt");
        TEST_ASSERT(fs->init != nullptr, "init");
        TEST_ASSERT(fs->cond != nullptr, "cond");
        TEST_ASSERT(fs->step != nullptr, "step");
    }
    test_passed("parser_for");

    test_header("parser_all_operators");
    {
        string err;
        auto prog = parse(
            "func main(): int {\n"
            "    var a = 1 + 2 - 3 * 4 / 5 % 6;\n"
            "    var b = 1 == 2 != 3 < 4 > 5 <= 6 >= 7;\n"
            "    var c = 1 && 2 || 3;\n"
            "    var d = 1 & 2 | 3 ^ 4 << 5 >> 6;\n"
            "    var e = -1 + !2 + ~3;\n"
            "    return 0;\n"
            "}\n", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
    }
    test_passed("parser_all_operators");

    test_header("parser_borrow");
    {
        string err;
        auto prog = parse("func main(): int { var x = 5; var p = &x; var q = &mut x; return 0; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[0]);
        auto be = dynamic_pointer_cast<BorrowExpr>(dynamic_pointer_cast<VarStmt>(fd->body->stmts[1])->init);
        TEST_ASSERT(be != nullptr, "borrow");
        TEST_ASSERT(!be->mutableBorrow, "immutable");
        auto be2 = dynamic_pointer_cast<BorrowExpr>(dynamic_pointer_cast<VarStmt>(fd->body->stmts[2])->init);
        TEST_ASSERT(be2->mutableBorrow, "mutable");
    }
    test_passed("parser_borrow");

    test_header("parser_index");
    {
        string err;
        auto prog = parse("func main(): int { var p: ptr<int> = alloc(3); p[0] = 1; return p[0]; }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
    }
    test_passed("parser_index");

    test_header("parser_call_args");
    {
        string err;
        auto prog = parse("func add(a: int, b: int, c: int): int { return a + b + c; }\n"
                          "func main(): int { return add(1, 2, 3); }", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
        auto fd = dynamic_pointer_cast<FuncDecl>(prog->decls[1]);
        auto ret = dynamic_pointer_cast<ReturnStmt>(fd->body->stmts[0]);
        auto call = dynamic_pointer_cast<CallExpr>(ret->value);
        TEST_ASSERT_EQ(call->args.size(), 3, "3 args");
    }
    test_passed("parser_call_args");

    test_header("parser_string_char_bool");
    {
        string err;
        auto prog = parse(
            "func main(): int {\n"
            "    var s = \"hello\";\n"
            "    var c = 'a';\n"
            "    var b = true;\n"
            "    var b2 = false;\n"
            "    return 0;\n"
            "}\n", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
    }
    test_passed("parser_string_char_bool");

    test_header("parser_nested_if");
    {
        string err;
        auto prog = parse(
            "func main(): int {\n"
            "    if (true) {\n"
            "        if (false) { return 1; }\n"
            "    }\n"
            "    return 0;\n"
            "}\n", err);
        TEST_ASSERT(prog != nullptr, err.c_str());
    }
    test_passed("parser_nested_if");

    test_header("parser_error_missing_semi");
    {
        string err;
        TEST_ASSERT(!parse_ok("func main(): int { var x = 5 return 0 }", err), "should fail");
        TEST_ASSERT(!err.empty(), "has error");
    }
    test_passed("parser_error_missing_semi");

    test_header("parser_error_missing_paren");
    {
        string err;
        TEST_ASSERT(!parse_ok("func main(): int { if true { return 0; } }", err), "should fail");
        TEST_ASSERT(!err.empty(), "has error");
    }
    test_passed("parser_error_missing_paren");

    return 0;
}

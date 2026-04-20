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

int test_parser_main() {
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

    return 0;
}

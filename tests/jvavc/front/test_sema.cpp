#include "test_utils.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include <fstream>

using namespace std;

static bool sema_ok(const string &src, string &err) {
    ofstream("tmp.jvl") << src;
    Lexer lex; lex.tokenize("tmp.jvl");
    FrontParser par; par.parse(lex.getTokens());
    Sema sema;
    bool ok = sema.analyze(par.getProgram());
    err = sema.getError();
    remove("tmp.jvl");
    return ok;
}

int test_sema_main() {
    test_header("sema_undefined");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { return x; }", err), "should fail");
        TEST_ASSERT(err.find("Undefined") != string::npos, "undefined msg");
    }
    test_passed("sema_undefined");

    test_header("sema_redeclaration");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = 1; var x = 2; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("Redeclaration") != string::npos, "redecl msg");
    }
    test_passed("sema_redeclaration");

    test_header("sema_builtin");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { putint(42); return 0; }", err), err.c_str());
    }
    test_passed("sema_builtin");

    test_header("sema_func_call");
    {
        string err;
        TEST_ASSERT(sema_ok("func add(a: int, b: int): int { return a + b; }\nfunc main(): int { return add(1, 2); }", err), err.c_str());
    }
    test_passed("sema_func_call");

    return 0;
}

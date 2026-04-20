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

static bool sema_has(const string &src, const string &substr) {
    string err;
    sema_ok(src, err);
    return err.find(substr) != string::npos;
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

    // ---- MimiWorld ownership tests ----

    test_header("sema_uninitialized");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x: int; return x; }", err), "should fail");
        TEST_ASSERT(err.find("uninitialized") != string::npos || err.find("Uninitialized") != string::npos, "uninit msg");
    }
    test_passed("sema_uninitialized");

    test_header("sema_move_copy");
    {
        // int is Copy; assignment does NOT move
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var x = 5; var y = x; return x + y; }", err), err.c_str());
    }
    test_passed("sema_move_copy");

    test_header("sema_borrow_basic");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var x = 5; var p = &x; return 0; }", err), err.c_str());
    }
    test_passed("sema_borrow_basic");

    test_header("sema_mut_borrow");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var x = 5; var p = &mut x; return 0; }", err), err.c_str());
    }
    test_passed("sema_mut_borrow");

    test_header("sema_return_type");
    {
        string err;
        TEST_ASSERT(!sema_ok("func foo(): void { return 1; }", err), "should fail");
        TEST_ASSERT(err.find("void") != string::npos, "void return msg");
    }
    test_passed("sema_return_type");

    test_header("sema_func_as_value");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = main; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("function") != string::npos, "func as value msg");
    }
    test_passed("sema_func_as_value");

    return 0;
}

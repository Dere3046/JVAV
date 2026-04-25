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
    // --- Original tests (updated for new error messages) ---
    test_header("sema_undefined");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { return x; }", err), "should fail");
        TEST_ASSERT(err.find("cannot find value") != string::npos || err.find("Undefined") != string::npos, "undefined msg");
    }
    test_passed("sema_undefined");

    test_header("sema_redeclaration");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = 1; var x = 2; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("defined multiple times") != string::npos || err.find("Redeclaration") != string::npos, "redecl msg");
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

    test_header("sema_alloc");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var p = alloc(1); p[0] = 1; free(p); return 0; }", err), err.c_str());
    }
    test_passed("sema_alloc");

    test_header("sema_alloc_move");
    {
        // ptr<int> is non-Copy; after free(p), p is moved and cannot be used
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var p: ptr<int> = alloc(1); free(p); p[0] = 1; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("moved") != string::npos || err.find("Moved") != string::npos, "move msg after free");
    }
    test_passed("sema_alloc_move");

    // --- New comprehensive tests ---

    test_header("sema_type_inference");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var x = 5; var y = true; var z = 'a'; return x; }", err), err.c_str());
    }
    test_passed("sema_type_inference");

    test_header("sema_ptr_type");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var p: ptr<int> = alloc(1); return 0; }", err), err.c_str());
    }
    test_passed("sema_ptr_type");

    test_header("sema_array_type");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var a: array<int> = alloc(3); return 0; }", err), err.c_str());
    }
    test_passed("sema_array_type");

    test_header("sema_scope_shadowing");
    {
        string err;
        // Inner scope can shadow outer
        TEST_ASSERT(sema_ok("func main(): int { var x = 1; { var x = 2; putint(x); } return x; }", err), err.c_str());
    }
    test_passed("sema_scope_shadowing");

    test_header("sema_borrow_conflict_immut_after_mut");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = 5; var p = &mut x; var q = &x; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("borrow") != string::npos || err.find("Borrow") != string::npos, "borrow conflict");
    }
    test_passed("sema_borrow_conflict_immut_after_mut");

    test_header("sema_borrow_conflict_mut_after_immut");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = 5; var p = &x; var q = &mut x; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("borrow") != string::npos || err.find("Borrow") != string::npos, "borrow conflict");
    }
    test_passed("sema_borrow_conflict_mut_after_immut");

    test_header("sema_double_mut_borrow");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var x = 5; var p = &mut x; var q = &mut x; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("mutably borrowed") != string::npos || err.find("borrow") != string::npos, "double mut borrow");
    }
    test_passed("sema_double_mut_borrow");

    test_header("sema_use_after_move");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var p: ptr<int> = alloc(1); var q = p; p[0] = 1; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("moved") != string::npos || err.find("Moved") != string::npos, "use after move");
    }
    test_passed("sema_use_after_move");

    test_header("sema_uninit_ptr");
    {
        string err;
        TEST_ASSERT(!sema_ok("func main(): int { var p: ptr<int>; p[0] = 1; return 0; }", err), "should fail");
        TEST_ASSERT(err.find("uninitialized") != string::npos || err.find("Uninitialized") != string::npos, "uninit ptr");
    }
    test_passed("sema_uninit_ptr");

    test_header("sema_void_param");
    {
        string err;
        // void parameter should be rejected (parser might catch it, but sema should too)
        // This may be a parser error instead; just check it fails
        TEST_ASSERT(!sema_ok("func foo(x: void): int { return 0; }", err), "should fail");
    }
    test_passed("sema_void_param");

    test_header("sema_missing_return");
    {
        string err;
        TEST_ASSERT(!sema_ok("func foo(): int { }", err), "should fail");
        TEST_ASSERT(err.find("return") != string::npos || err.find("missing") != string::npos, "missing return");
    }
    test_passed("sema_missing_return");

    test_header("sema_correct_return_void");
    {
        string err;
        TEST_ASSERT(sema_ok("func foo(): void { return; }", err), err.c_str());
    }
    test_passed("sema_correct_return_void");

    test_header("sema_unused_warning");
    {
        string err;
        // Unused variables may produce warnings but not errors
        TEST_ASSERT(sema_ok("func main(): int { var x = 5; return 0; }", err), err.c_str());
    }
    test_passed("sema_unused_warning");

    test_header("sema_if_else_types");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func main(): int {\n"
            "    var x = 0;\n"
            "    if (true) { x = 1; } else { x = 2; }\n"
            "    return x;\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_if_else_types");

    test_header("sema_while_loop");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func main(): int {\n"
            "    var i = 0;\n"
            "    while (i < 10) { i = i + 1; }\n"
            "    return i;\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_while_loop");

    test_header("sema_for_loop");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func main(): int {\n"
            "    var sum = 0;\n"
            "    for (var i = 0; i < 5; i = i + 1) {\n"
            "        sum = sum + i;\n"
            "    }\n"
            "    return sum;\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_for_loop");

    test_header("sema_borrow_function_param");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func get(x: &int): int { return x[0]; }\n"
            "func main(): int {\n"
            "    var a = 42;\n"
            "    return get(&a);\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_borrow_function_param");

    test_header("sema_mut_borrow_function_param");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func set(x: &mut int): void { x[0] = 99; }\n"
            "func main(): int {\n"
            "    var a = 42;\n"
            "    set(&mut a);\n"
            "    return a;\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_mut_borrow_function_param");

    test_header("sema_char_type");
    {
        string err;
        TEST_ASSERT(sema_ok("func main(): int { var c: char = 'A'; putchar(c); return 0; }", err), err.c_str());
    }
    test_passed("sema_char_type");

    test_header("sema_bool_type");
    {
        string err;
        TEST_ASSERT(sema_ok(
            "func main(): int {\n"
            "    var b: bool = true;\n"
            "    if (b) { return 1; } else { return 0; }\n"
            "}\n", err), err.c_str());
    }
    test_passed("sema_bool_type");

    return 0;
}

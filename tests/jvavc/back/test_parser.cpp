#include "test_utils.hpp"
#include "parser.hpp"
#include <fstream>

using namespace std;

static bool parse_ok(const string &asmSrc) {
    ofstream("tmp.jvav") << asmSrc;
    Parser p;
    bool ok = p.parse("tmp.jvav");
    remove("tmp.jvav");
    return ok;
}

int test_parser_main() {
    test_header("parse_arithmetic");
    TEST_ASSERT(parse_ok("ADD R0, R1, R2\nSUB R3, R4, R5\n"), "basic arithmetic");
    test_passed("parse_arithmetic");

    test_header("parse_immediate");
    TEST_ASSERT(parse_ok("LDI R0, 123\nLDI R1, -1\nLDI R2, 0xFFF0\n"), "immediate values");
    test_passed("parse_immediate");

    test_header("parse_labels");
    TEST_ASSERT(parse_ok("loop: ADD R0, R0, 1\nJMP loop\n"), "label reference");
    test_passed("parse_labels");

    test_header("parse_data");
    TEST_ASSERT(parse_ok("msg: DB 'H','e','l','l','o'\nHALT\n"), "data section");
    test_passed("parse_data");

    test_header("parse_errors");
    {
        ofstream("tmp_err.jvav") << "BAD_OPCODE R0\n";
        Parser p;
        bool ok = p.parse("tmp_err.jvav");
        remove("tmp_err.jvav");
        TEST_ASSERT(!ok, "should fail on bad mnemonic");
    }
    test_passed("parse_errors");

    return 0;
}

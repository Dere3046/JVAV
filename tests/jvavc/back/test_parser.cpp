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

    // ------------------------------------------------------------------
    // NEW TESTS
    // ------------------------------------------------------------------

    test_header("parse_all_mnemonics");
    TEST_ASSERT(parse_ok(
        "HALT\n"
        "MOV R0, R1\n"
        "LDR R0, [R1]\n"
        "STR [R0], R1\n"
        "ADD R0, R1, R2\n"
        "SUB R0, R1, R2\n"
        "MUL R0, R1, R2\n"
        "DIV R0, R1, R2\n"
        "CMP R0, R1\n"
        "JMP loop\n"
        "JZ loop\n"
        "JNZ loop\n"
        "JE loop\n"
        "JNE loop\n"
        "JL loop\n"
        "JG loop\n"
        "JLE loop\n"
        "JGE loop\n"
        "PUSH R0\n"
        "POP R0\n"
        "CALL loop\n"
        "RET\n"
        "LDI R0, 1\n"
        "loop:\n"
    ), "all mnemonics should parse");
    test_passed("parse_all_mnemonics");

    test_header("parse_all_registers");
    TEST_ASSERT(parse_ok(
        "MOV R0, R1\n"
        "MOV R2, R3\n"
        "MOV R4, R5\n"
        "MOV R6, R7\n"
        "MOV PC, SP\n"
        "MOV FLAGS, R0\n"
    ), "all registers should parse");
    test_passed("parse_all_registers");

    test_header("parse_ldr_str_indirect");
    TEST_ASSERT(parse_ok("LDR R0, [R1]\nSTR [R0], R1\n"), "register indirect");
    TEST_ASSERT(parse_ok("LDR R0, [0x100]\nSTR [0x200], R1\n"), "immediate address");
    test_passed("parse_ldr_str_indirect");

    test_header("parse_equ_colon");
    {
        Parser p;
        vector<string> lines = {"VAL: EQU 42", "LDI R0, VAL"};
        TEST_ASSERT(p.parseLines(lines), "EQU with colon should parse");
        TEST_ASSERT_EQ((long long)p.getEquSymbols().at("VAL"), 42, "EQU value");
    }
    test_passed("parse_equ_colon");

    test_header("parse_equ_no_colon");
    {
        Parser p;
        vector<string> lines = {"VAL EQU 42", "LDI R0, VAL"};
        TEST_ASSERT(p.parseLines(lines), "EQU without colon should parse");
        TEST_ASSERT_EQ((long long)p.getEquSymbols().at("VAL"), 42, "EQU value");
    }
    test_passed("parse_equ_no_colon");

    test_header("parse_equ_hex");
    {
        Parser p;
        vector<string> lines = {"VAL: EQU 0xFF", "LDI R0, VAL"};
        TEST_ASSERT(p.parseLines(lines), "hex EQU should parse");
        TEST_ASSERT_EQ((long long)p.getEquSymbols().at("VAL"), 255, "EQU hex value");
    }
    test_passed("parse_equ_hex");

    test_header("parse_global");
    {
        Parser p;
        vector<string> lines = {".global _start, foo", ".global bar"};
        TEST_ASSERT(p.parseLines(lines), ".global should parse");
        TEST_ASSERT(p.getGlobalSymbols().count("_start") > 0, "_start global");
        TEST_ASSERT(p.getGlobalSymbols().count("foo") > 0, "foo global");
        TEST_ASSERT(p.getGlobalSymbols().count("bar") > 0, "bar global");
    }
    test_passed("parse_global");

    test_header("parse_extern");
    {
        Parser p;
        vector<string> lines = {".extern foo, bar", ".extern baz"};
        TEST_ASSERT(p.parseLines(lines), ".extern should parse");
        TEST_ASSERT(p.getExternSymbols().count("foo") > 0, "foo extern");
        TEST_ASSERT(p.getExternSymbols().count("bar") > 0, "bar extern");
        TEST_ASSERT(p.getExternSymbols().count("baz") > 0, "baz extern");
    }
    test_passed("parse_extern");

    test_header("parse_db");
    {
        Parser p;
        vector<string> lines = {"msg: DB 1, 2, 3", "DB 'A','B'"};
        TEST_ASSERT(p.parseLines(lines), "DB should parse");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems.size(), 3, "DB items count");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems[0].width, 1, "DB width");
    }
    test_passed("parse_db");

    test_header("parse_dw");
    {
        Parser p;
        vector<string> lines = {"vals: DW 100, 200"};
        TEST_ASSERT(p.parseLines(lines), "DW should parse");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems.size(), 2, "DW items count");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems[0].width, 2, "DW width");
    }
    test_passed("parse_dw");

    test_header("parse_dt");
    {
        Parser p;
        vector<string> lines = {"big: DT 999"};
        TEST_ASSERT(p.parseLines(lines), "DT should parse");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems.size(), 1, "DT items count");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems[0].width, 16, "DT width");
    }
    test_passed("parse_dt");

    test_header("parse_db_string");
    {
        Parser p;
        vector<string> lines = {"msg: DB \"Hello\""};
        TEST_ASSERT(p.parseLines(lines), "DB string should parse");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems.size(), 5, "DB string chars");
        TEST_ASSERT_EQ((long long)p.getInstructions()[0].dataItems[0].value, 'H', "first char");
    }
    test_passed("parse_db_string");

    test_header("parse_include");
    {
        ofstream("inc_test.jvav") << "VAL EQU 10\n";
        ofstream("main_test.jvav") << "#include \"inc_test.jvav\"\nLDI R0, VAL\n";
        Parser p;
        TEST_ASSERT(p.parse("main_test.jvav"), "include should parse");
        TEST_ASSERT_EQ((long long)p.getEquSymbols().at("VAL"), 10, "included EQU value");
        remove("inc_test.jvav");
        remove("main_test.jvav");
    }
    test_passed("parse_include");

    test_header("parse_comments");
    {
        Parser p;
        vector<string> lines = {
            "; comment at start",
            "HALT ; inline comment",
            "// C++ style comment",
            "MOV R0, R1 ; move",
            ""
        };
        TEST_ASSERT(p.parseLines(lines), "comments should parse");
        TEST_ASSERT_EQ((int)p.getInstructions().size(), 2, "only instructions, no comments");
    }
    test_passed("parse_comments");

    test_header("parse_empty_lines");
    {
        Parser p;
        vector<string> lines = {"", "   ", "\t", "HALT", "", "MOV R0, R1"};
        TEST_ASSERT(p.parseLines(lines), "empty lines should parse");
        TEST_ASSERT_EQ((int)p.getInstructions().size(), 2, "two instructions");
    }
    test_passed("parse_empty_lines");

    test_header("parse_label_only_line");
    {
        Parser p;
        vector<string> lines = {"start:", "HALT"};
        TEST_ASSERT(p.parseLines(lines), "label-only line should parse");
        TEST_ASSERT_EQ((int)p.getLabels().at("start"), 0, "label address");
    }
    test_passed("parse_label_only_line");

    test_header("parse_negative_imm");
    {
        Parser p;
        vector<string> lines = {"LDI R0, -5", "LDI R1, -0x10"};
        TEST_ASSERT(p.parseLines(lines), "negative immediate should parse");
    }
    test_passed("parse_negative_imm");

    test_header("parse_parseLines");
    {
        Parser p;
        vector<string> lines = {"LDI R0, 1", "ADD R0, R0, 1"};
        TEST_ASSERT(p.parseLines(lines), "parseLines should work");
        TEST_ASSERT_EQ((int)p.getInstructions().size(), 2, "two instructions");
    }
    test_passed("parse_parseLines");

    test_header("parse_addExternalEqu");
    {
        Parser p;
        p.addExternalEqu("EXT", 123);
        vector<string> lines = {"LDI R0, EXT"};
        TEST_ASSERT(p.parseLines(lines), "external EQU should parse");
        TEST_ASSERT(p.getEquSymbols().count("EXT") > 0, "external EQU in map");
        TEST_ASSERT(p.getInstructions()[0].ops[1].type == OP_IMM, "external EQU resolved to IMM");
    }
    test_passed("parse_addExternalEqu");

    test_header("parse_duplicate_label");
    {
        Parser p;
        vector<string> lines = {"label: HALT", "label: MOV R0, R1"};
        TEST_ASSERT(!p.parseLines(lines), "duplicate label should fail");
    }
    test_passed("parse_duplicate_label");

    test_header("parse_undefined_label_ref");
    {
        Parser p;
        vector<string> lines = {"JMP undefined_label"};
        TEST_ASSERT(p.parseLines(lines), "undefined label reference should parse fine");
        TEST_ASSERT(p.getInstructions()[0].ops[0].label == string("undefined_label"), "label stored");
    }
    test_passed("parse_undefined_label_ref");

    test_header("parse_data_mixed");
    {
        Parser p;
        vector<string> lines = {"mix: DB 1, 'A', \"hi\""};
        TEST_ASSERT(p.parseLines(lines), "mixed data should parse");
        TEST_ASSERT_EQ((int)p.getInstructions()[0].dataItems.size(), 4, "mixed items count");
        TEST_ASSERT_EQ((long long)p.getInstructions()[0].dataItems[1].value, 'A', "char item");
    }
    test_passed("parse_data_mixed");

    test_header("parse_sections");
    TEST_ASSERT(parse_ok(".data\n.text\nHALT\n"), "section directives should parse");
    test_passed("parse_sections");

    return 0;
}

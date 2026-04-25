#include "test_utils.hpp"
#include "parser.hpp"
#include "encoder.hpp"
#include <fstream>

using namespace std;

static vector<Int128> compile(const string &asmSrc) {
    ofstream("tmp.jvav") << asmSrc;
    Parser p;
    p.parse("tmp.jvav");
    Encoder e;
    vector<Int128> bc;
    e.encode(p.getInstructions(), p.getLabels(), bc);
    remove("tmp.jvav");
    return bc;
}

int test_encoder_main() {
    test_header("encode_halt");
    auto bc = compile("HALT\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x00, "opcode");
    test_passed("encode_halt");

    test_header("encode_arithmetic");
    bc = compile("ADD R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x04, "ADD opcode");
    test_passed("encode_arithmetic");

    test_header("encode_ldi");
    bc = compile("LDI R0, 0x1234\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    auto op = (uint8_t)(long long)bc[0];
    TEST_ASSERT_EQ((int)op, 0x10, "LDI opcode");
    auto dst = (uint8_t)(long long)(bc[0] >> 8);
    TEST_ASSERT_EQ((int)dst, 0, "dst R0");
    test_passed("encode_ldi");

    test_header("encode_label_jump");
    bc = compile("loop: ADD R0, R0, 1\nJMP loop\n");
    TEST_ASSERT_EQ(bc.size(), 4, "size");
    op = (uint8_t)(long long)bc[0];
    TEST_ASSERT_EQ((int)op, 0x10, "first LDI");
    op = (uint8_t)(long long)bc.back();
    TEST_ASSERT_EQ((int)op, 0x09, "JMP opcode");
    test_passed("encode_label_jump");

    test_header("encode_data");
    bc = compile("msg: DB 'H','e'\nHALT\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size (2 data + 1 halt)");
    TEST_ASSERT_EQ((long long)(bc[0] & 0xFF), 'H', "first char");
    TEST_ASSERT_EQ((long long)(bc[1] & 0xFF), 'e', "second char");
    op = (uint8_t)(long long)bc[2];
    TEST_ASSERT_EQ((int)op, 0x00, "HALT after data");
    test_passed("encode_data");

    // ------------------------------------------------------------------
    // NEW TESTS
    // ------------------------------------------------------------------

    test_header("encode_mov_reg");
    bc = compile("MOV R0, R1\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x01, "MOV opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 8), 0, "dst R0");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 1, "src1 R1");
    test_passed("encode_mov_reg");

    test_header("encode_mov_imm");
    bc = compile("MOV R0, 42\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x01, "MOV opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 0x80, "src1 imm flag");
    test_passed("encode_mov_imm");

    test_header("encode_ldr_reg_indirect");
    bc = compile("LDR R0, [R1]\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x02, "LDR opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 8), 0, "dst R0");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 1, "src1 R1");
    test_passed("encode_ldr_reg_indirect");

    test_header("encode_str_reg_indirect");
    bc = compile("STR [R0], R1\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x03, "STR opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 8), 0, "dst R0");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 1, "src1 R1");
    test_passed("encode_str_reg_indirect");

    test_header("encode_ldr_imm");
    bc = compile("LDR R0, [0x100]\n");
    TEST_ASSERT_EQ(bc.size(), 2, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x10, "LDI temp");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x02, "LDR opcode");
    test_passed("encode_ldr_imm");

    test_header("encode_str_imm");
    bc = compile("STR [0x100], R0\n");
    TEST_ASSERT_EQ(bc.size(), 2, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x10, "LDI temp");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x03, "STR opcode");
    test_passed("encode_str_imm");

    test_header("encode_ldr_label");
    bc = compile("data: DB 1\nLDR R0, [data]\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x10, "LDI temp with label addr");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x02, "LDR opcode");
    test_passed("encode_ldr_label");

    test_header("encode_str_label");
    bc = compile("data: DB 0\nSTR [data], R0\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x10, "LDI temp with label addr");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x03, "STR opcode");
    test_passed("encode_str_label");

    test_header("encode_sub");
    bc = compile("SUB R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x05, "SUB opcode");
    test_passed("encode_sub");

    test_header("encode_mul");
    bc = compile("MUL R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x06, "MUL opcode");
    test_passed("encode_mul");

    test_header("encode_div");
    bc = compile("DIV R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x07, "DIV opcode");
    test_passed("encode_div");

    test_header("encode_arith_imm_one");
    bc = compile("ADD R0, R1, 5\n");
    TEST_ASSERT_EQ(bc.size(), 2, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x10, "LDI temp");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x04, "ADD opcode");
    test_passed("encode_arith_imm_one");

    test_header("encode_arith_imm_both");
    bc = compile("ADD R0, 10, 20\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x10, "LDI temp1");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x10, "LDI temp2");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x04, "ADD opcode");
    test_passed("encode_arith_imm_both");

    test_header("encode_cmp");
    bc = compile("CMP R0, R1\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x08, "CMP opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 0, "src1 R0");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 24), 1, "src2 R1");
    test_passed("encode_cmp");

    test_header("encode_jz");
    bc = compile("lab: HALT\nJZ lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x0A, "JZ opcode");
    test_passed("encode_jz");

    test_header("encode_jnz");
    bc = compile("lab: HALT\nJNZ lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x0B, "JNZ opcode");
    test_passed("encode_jnz");

    test_header("encode_je");
    bc = compile("lab: HALT\nJE lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x11, "JE opcode");
    test_passed("encode_je");

    test_header("encode_jne");
    bc = compile("lab: HALT\nJNE lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x12, "JNE opcode");
    test_passed("encode_jne");

    test_header("encode_jl");
    bc = compile("lab: HALT\nJL lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x13, "JL opcode");
    test_passed("encode_jl");

    test_header("encode_jg");
    bc = compile("lab: HALT\nJG lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x14, "JG opcode");
    test_passed("encode_jg");

    test_header("encode_jle");
    bc = compile("lab: HALT\nJLE lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x15, "JLE opcode");
    test_passed("encode_jle");

    test_header("encode_jge");
    bc = compile("lab: HALT\nJGE lab\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x16, "JGE opcode");
    test_passed("encode_jge");

    test_header("encode_push");
    bc = compile("PUSH R0\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x0C, "PUSH opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 0, "src1 R0");
    test_passed("encode_push");

    test_header("encode_pop");
    bc = compile("POP R0\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x0D, "POP opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 8), 0, "dst R0");
    test_passed("encode_pop");

    test_header("encode_call");
    bc = compile("func: RET\nCALL func\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[2], 0x0E, "CALL opcode");
    test_passed("encode_call");

    test_header("encode_ret");
    bc = compile("RET\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x0F, "RET opcode");
    test_passed("encode_ret");

    test_header("encode_ldi_label");
    bc = compile("lab: HALT\nLDI R0, lab\n");
    TEST_ASSERT_EQ(bc.size(), 2, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[1], 0x10, "LDI opcode");
    test_passed("encode_ldi_label");

    test_header("encode_dw");
    bc = compile("vals: DW 100, 200\nHALT\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size");
    TEST_ASSERT_EQ((long long)(bc[0] & 0xFF), 100, "first word");
    TEST_ASSERT_EQ((long long)(bc[1] & 0xFF), 200, "second word");
    test_passed("encode_dw");

    test_header("encode_dt");
    bc = compile("big: DT 0x1234\nHALT\n");
    TEST_ASSERT_EQ(bc.size(), 2, "size");
    TEST_ASSERT_EQ((long long)(bc[0] & 0xFF), 0x34, "DT low byte");
    test_passed("encode_dt");

    test_header("encode_all_registers");
    bc = compile(
        "MOV R0, R0\n"
        "MOV R1, R1\n"
        "MOV R2, R2\n"
        "MOV R3, R3\n"
        "MOV R4, R4\n"
        "MOV R5, R5\n"
        "MOV R6, R6\n"
        "MOV R7, R7\n"
        "MOV PC, PC\n"
        "MOV SP, SP\n"
        "MOV FLAGS, FLAGS\n"
    );
    TEST_ASSERT_EQ(bc.size(), 11, "size");
    for (int i = 0; i < 11; i++) {
        int expected_dst = (i < 8) ? i : (i == 8) ? 8 : (i == 9) ? 9 : 10;
        int actual_dst = (int)(uint8_t)(long long)(bc[i] >> 8);
        TEST_ASSERT_EQ(actual_dst, expected_dst, "register index dst");
        int expected_src = expected_dst;
        int actual_src = (int)(uint8_t)(long long)(bc[i] >> 16);
        TEST_ASSERT_EQ(actual_src, expected_src, "register index src");
    }
    test_passed("encode_all_registers");

    test_header("encode_mod");
    bc = compile("MOD R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x17, "MOD opcode");
    test_passed("encode_mod");

    test_header("encode_and");
    bc = compile("AND R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x18, "AND opcode");
    test_passed("encode_and");

    test_header("encode_or");
    bc = compile("OR R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x19, "OR opcode");
    test_passed("encode_or");

    test_header("encode_xor");
    bc = compile("XOR R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x1A, "XOR opcode");
    test_passed("encode_xor");

    test_header("encode_shl");
    bc = compile("SHL R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x1B, "SHL opcode");
    test_passed("encode_shl");

    test_header("encode_shr");
    bc = compile("SHR R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x1C, "SHR opcode");
    test_passed("encode_shr");

    test_header("encode_not");
    bc = compile("NOT R0, R1\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)bc[0], 0x1D, "NOT opcode");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 8), 0, "dst R0");
    TEST_ASSERT_EQ((int)(uint8_t)(long long)(bc[0] >> 16), 1, "src1 R1");
    test_passed("encode_not");

    return 0;
}

#include "test_utils.hpp"
#include "parser.hpp"
#include "encoder.hpp"
#include <fstream>

using namespace std;

static vector<__int128> compile(const string &asmSrc) {
    ofstream("tmp.jvav") << asmSrc;
    Parser p;
    p.parse("tmp.jvav");
    Encoder e;
    vector<__int128> bc;
    e.encode(p.getInstructions(), p.getLabels(), bc);
    remove("tmp.jvav");
    return bc;
}

int test_encoder_main() {
    test_header("encode_halt");
    auto bc = compile("HALT\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)bc[0], 0x00, "opcode");
    test_passed("encode_halt");

    test_header("encode_arithmetic");
    bc = compile("ADD R0, R1, R2\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    TEST_ASSERT_EQ((int)(uint8_t)bc[0], 0x04, "ADD opcode");
    test_passed("encode_arithmetic");

    test_header("encode_ldi");
    bc = compile("LDI R0, 0x1234\n");
    TEST_ASSERT_EQ(bc.size(), 1, "size");
    auto op = (uint8_t)bc[0];
    TEST_ASSERT_EQ((int)op, 0x10, "LDI opcode");
    auto dst = (uint8_t)(bc[0] >> 8);
    TEST_ASSERT_EQ((int)dst, 0, "dst R0");
    test_passed("encode_ldi");

    test_header("encode_label_jump");
    bc = compile("loop: ADD R0, R0, 1\nJMP loop\n");
    TEST_ASSERT_EQ(bc.size(), 4, "size");
    op = (uint8_t)bc[0];
    TEST_ASSERT_EQ((int)op, 0x10, "first LDI");
    op = (uint8_t)bc.back();
    TEST_ASSERT_EQ((int)op, 0x09, "JMP opcode");
    test_passed("encode_label_jump");

    test_header("encode_data");
    bc = compile("msg: DB 'H','e'\nHALT\n");
    TEST_ASSERT_EQ(bc.size(), 3, "size (2 data + 1 halt)");
    TEST_ASSERT_EQ((long long)(bc[0] & 0xFF), 'H', "first char");
    TEST_ASSERT_EQ((long long)(bc[1] & 0xFF), 'e', "second char");
    op = (uint8_t)bc[2];
    TEST_ASSERT_EQ((int)op, 0x00, "HALT after data");
    test_passed("encode_data");

    return 0;
}

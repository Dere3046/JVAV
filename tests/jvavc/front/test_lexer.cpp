#include "test_utils.hpp"
#include "lexer.hpp"
#include <fstream>

using namespace std;

static bool writeAndLex(const string &src, vector<Token> &out) {
    ofstream("tmp.jvl") << src;
    Lexer lex;
    bool ok = lex.tokenize("tmp.jvl");
    out = lex.getTokens();
    remove("tmp.jvl");
    return ok;
}

int test_lexer_main() {
    test_header("lexer_keywords");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("func var if import", toks), "lex failed");
        TEST_ASSERT(toks.size() >= 5, "too few tokens");
        TEST_ASSERT(toks[0].type == TOK_KW_FUNC, "func");
        TEST_ASSERT(toks[1].type == TOK_KW_VAR, "var");
        TEST_ASSERT(toks[2].type == TOK_KW_IF, "if");
        TEST_ASSERT(toks[3].type == TOK_KW_IMPORT, "import");
    }
    test_passed("lexer_keywords");

    test_header("lexer_numbers");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("42 0xFF -7", toks), "lex failed");
        TEST_ASSERT(toks[0].type == TOK_NUMBER, "number");
        TEST_ASSERT_EQ((long long)toks[0].value, 42, "42");
        TEST_ASSERT(toks[1].type == TOK_NUMBER, "0xFF");
        TEST_ASSERT_EQ((long long)toks[1].value, 255, "0xFF");
        TEST_ASSERT(toks[2].type == TOK_NUMBER, "-7");
        TEST_ASSERT_EQ((long long)toks[2].value, -7, "-7");
    }
    test_passed("lexer_numbers");

    test_header("lexer_identifiers");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("foo _bar123", toks), "lex failed");
        TEST_ASSERT(toks[0].type == TOK_IDENT, "ident");
        TEST_ASSERT(toks[0].text == "foo", "foo");
        TEST_ASSERT(toks[1].type == TOK_IDENT, "ident2");
        TEST_ASSERT(toks[1].text == "_bar123", "_bar123");
    }
    test_passed("lexer_identifiers");

    test_header("lexer_symbols");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("+ - * / == != <= >= && ||", toks), "lex failed");
        TEST_ASSERT(toks[0].type == TOK_PLUS, "+");
        TEST_ASSERT(toks[1].type == TOK_MINUS, "-");
        TEST_ASSERT(toks[2].type == TOK_STAR, "*");
        TEST_ASSERT(toks[3].type == TOK_SLASH, "/");
        TEST_ASSERT(toks[4].type == TOK_EQ, "==");
        TEST_ASSERT(toks[5].type == TOK_NE, "!=");
        TEST_ASSERT(toks[6].type == TOK_LE, "<=");
        TEST_ASSERT(toks[7].type == TOK_GE, ">=");
        TEST_ASSERT(toks[8].type == TOK_AND, "&&");
        TEST_ASSERT(toks[9].type == TOK_OR, "||");
    }
    test_passed("lexer_symbols");

    return 0;
}

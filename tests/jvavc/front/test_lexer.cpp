#include "test_utils.hpp"
#include "lexer.hpp"
#include <fstream>

using namespace std;

static bool writeAndLex(const string &src, vector<Token> &out, string *err = nullptr) {
    ofstream("tmp.jvl") << src;
    Lexer lex;
    bool ok = lex.tokenize("tmp.jvl");
    out = lex.getTokens();
    if (err) *err = lex.getError();
    remove("tmp.jvl");
    return ok;
}

int test_lexer_main() {
    // --- Original tests ---
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

    // --- New comprehensive tests ---

    test_header("lexer_all_keywords");
    {
        vector<Token> toks;
        string src = "func var const if else while for return int char bool void ptr array true false import mut";
        TEST_ASSERT(writeAndLex(src, toks), "lex failed");
        TEST_ASSERT_EQ((int)toks.size(), 19, "token count"); // 18 keywords + EOF
        TEST_ASSERT(toks[0].type == TOK_KW_FUNC, "func");
        TEST_ASSERT(toks[1].type == TOK_KW_VAR, "var");
        TEST_ASSERT(toks[2].type == TOK_KW_CONST, "const");
        TEST_ASSERT(toks[3].type == TOK_KW_IF, "if");
        TEST_ASSERT(toks[4].type == TOK_KW_ELSE, "else");
        TEST_ASSERT(toks[5].type == TOK_KW_WHILE, "while");
        TEST_ASSERT(toks[6].type == TOK_KW_FOR, "for");
        TEST_ASSERT(toks[7].type == TOK_KW_RETURN, "return");
        TEST_ASSERT(toks[8].type == TOK_KW_INT, "int");
        TEST_ASSERT(toks[9].type == TOK_KW_CHAR, "char");
        TEST_ASSERT(toks[10].type == TOK_KW_BOOL, "bool");
        TEST_ASSERT(toks[11].type == TOK_KW_VOID, "void");
        TEST_ASSERT(toks[12].type == TOK_KW_PTR, "ptr");
        TEST_ASSERT(toks[13].type == TOK_KW_ARRAY, "array");
        TEST_ASSERT(toks[14].type == TOK_KW_TRUE, "true");
        TEST_ASSERT(toks[15].type == TOK_KW_FALSE, "false");
        TEST_ASSERT(toks[16].type == TOK_KW_IMPORT, "import");
        TEST_ASSERT(toks[17].type == TOK_KW_MUT, "mut");
    }
    test_passed("lexer_all_keywords");

    test_header("lexer_all_symbols");
    {
        vector<Token> toks;
        string src = "+ - * / % = == != < > <= >= && || & | ^ ~ ! ( ) [ ] { } , ; :";
        TEST_ASSERT(writeAndLex(src, toks), "lex failed");
        TEST_ASSERT_EQ((int)toks.size(), 29, "token count"); // 28 symbols + EOF
        TEST_ASSERT(toks[0].type == TOK_PLUS, "+");
        TEST_ASSERT(toks[1].type == TOK_MINUS, "-");
        TEST_ASSERT(toks[2].type == TOK_STAR, "*");
        TEST_ASSERT(toks[3].type == TOK_SLASH, "/");
        TEST_ASSERT(toks[4].type == TOK_PERCENT, "%");
        TEST_ASSERT(toks[5].type == TOK_ASSIGN, "=");
        TEST_ASSERT(toks[6].type == TOK_EQ, "==");
        TEST_ASSERT(toks[7].type == TOK_NE, "!=");
        TEST_ASSERT(toks[8].type == TOK_LT, "<");
        TEST_ASSERT(toks[9].type == TOK_GT, ">");
        TEST_ASSERT(toks[10].type == TOK_LE, "<=");
        TEST_ASSERT(toks[11].type == TOK_GE, ">=");
        TEST_ASSERT(toks[12].type == TOK_AND, "&&");
        TEST_ASSERT(toks[13].type == TOK_OR, "||");
        TEST_ASSERT(toks[14].type == TOK_BITAND, "&");
        TEST_ASSERT(toks[15].type == TOK_BITOR, "|");
        TEST_ASSERT(toks[16].type == TOK_BITXOR, "^");
        TEST_ASSERT(toks[17].type == TOK_BITNOT, "~");
        TEST_ASSERT(toks[18].type == TOK_NOT, "!");
        TEST_ASSERT(toks[19].type == TOK_LPAREN, "(");
        TEST_ASSERT(toks[20].type == TOK_RPAREN, ")");
        TEST_ASSERT(toks[21].type == TOK_LBRACKET, "[");
        TEST_ASSERT(toks[22].type == TOK_RBRACKET, "]");
        TEST_ASSERT(toks[23].type == TOK_LBRACE, "{");
        TEST_ASSERT(toks[24].type == TOK_RBRACE, "}");
        TEST_ASSERT(toks[25].type == TOK_COMMA, ",");
        TEST_ASSERT(toks[26].type == TOK_SEMI, ";");
        TEST_ASSERT(toks[27].type == TOK_COLON, ":");
    }
    test_passed("lexer_all_symbols");

    test_header("lexer_strings");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("\"hello\"", toks), "lex failed");
        TEST_ASSERT_EQ((int)toks.size(), 2, "count");
        TEST_ASSERT(toks[0].type == TOK_STRING, "string token");
        TEST_ASSERT(toks[0].text == "hello", "hello");
    }
    test_passed("lexer_strings");

    test_header("lexer_string_escapes");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("\"hello\\nworld\\t!\"", toks), "lex failed");
        TEST_ASSERT(toks[0].type == TOK_STRING, "string");
        TEST_ASSERT(toks[0].text == "hello\nworld\t!", "escaped");
    }
    test_passed("lexer_string_escapes");

    test_header("lexer_chars");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("'a' '\\n' '\\t'", toks), "lex failed");
        TEST_ASSERT(toks[0].type == TOK_CHAR, "char");
        TEST_ASSERT_EQ((long long)toks[0].value, 'a', "a");
        TEST_ASSERT(toks[1].type == TOK_CHAR, "char2");
        TEST_ASSERT_EQ((long long)toks[1].value, '\n', "newline");
        TEST_ASSERT(toks[2].type == TOK_CHAR, "char3");
        TEST_ASSERT_EQ((long long)toks[2].value, '\t', "tab");
    }
    test_passed("lexer_chars");

    test_header("lexer_hex_numbers");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("0x0 0xFF 0xabcdef 0xABC", toks), "lex failed");
        TEST_ASSERT_EQ((long long)toks[0].value, 0, "0");
        TEST_ASSERT_EQ((long long)toks[1].value, 255, "0xFF");
        TEST_ASSERT_EQ((long long)toks[2].value, 0xABCDEF, "0xabcdef");
        TEST_ASSERT_EQ((long long)toks[3].value, 0xABC, "0xABC");
    }
    test_passed("lexer_hex_numbers");

    test_header("lexer_comments");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("// line comment\n42 /* block */ 99", toks), "lex failed");
        TEST_ASSERT_EQ((int)toks.size(), 3, "count");
        TEST_ASSERT_EQ((long long)toks[0].value, 42, "42");
        TEST_ASSERT_EQ((long long)toks[1].value, 99, "99");
    }
    test_passed("lexer_comments");

    test_header("lexer_line_col");
    {
        vector<Token> toks;
        TEST_ASSERT(writeAndLex("func\nvar\n  if", toks), "lex failed");
        TEST_ASSERT_EQ(toks[0].line, 1, "line1");
        TEST_ASSERT_EQ(toks[1].line, 2, "line2");
        TEST_ASSERT_EQ(toks[2].line, 3, "line3");
        TEST_ASSERT_EQ(toks[2].col, 3, "col3");
    }
    test_passed("lexer_line_col");

    test_header("lexer_crlf_compat");
    {
        // Test CRLF line endings are normalized
        ofstream f("tmp_crlf.jvl", ios::binary);
        f << "func\r\nvar\r\nif\r\n";
        f.close();
        Lexer lex;
        bool ok = lex.tokenize("tmp_crlf.jvl");
        vector<Token> toks = lex.getTokens();
        remove("tmp_crlf.jvl");
        TEST_ASSERT(ok, "crlf lex failed");
        TEST_ASSERT(toks.size() >= 4, "token count");
        TEST_ASSERT(toks[0].type == TOK_KW_FUNC, "func");
        TEST_ASSERT(toks[1].type == TOK_KW_VAR, "var");
        TEST_ASSERT(toks[2].type == TOK_KW_IF, "if");
        TEST_ASSERT_EQ(toks[2].line, 3, "line3 after crlf");
    }
    test_passed("lexer_crlf_compat");

    test_header("lexer_error_unterminated_string");
    {
        vector<Token> toks;
        string err;
        TEST_ASSERT(!writeAndLex("\"hello", toks, &err), "should fail");
        TEST_ASSERT(err.find("unterminated") != string::npos, "unterminated msg");
    }
    test_passed("lexer_error_unterminated_string");

    test_header("lexer_error_unknown_char");
    {
        vector<Token> toks;
        string err;
        TEST_ASSERT(!writeAndLex("@", toks, &err), "should fail");
        TEST_ASSERT(err.find("unknown character") != string::npos, "unknown char msg");
    }
    test_passed("lexer_error_unknown_char");

    return 0;
}

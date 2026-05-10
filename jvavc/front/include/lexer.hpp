#ifndef LEXER_HPP
#define LEXER_HPP
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include "int128.hpp"

enum TokenType {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,
    // Keywords
    TOK_KW_FUNC, TOK_KW_VAR, TOK_KW_CONST,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_FOR, TOK_KW_RETURN, TOK_KW_DO,
    TOK_KW_INT, TOK_KW_CHAR, TOK_KW_BOOL, TOK_KW_VOID,
    TOK_KW_PTR, TOK_KW_ARRAY,
    TOK_KW_TRUE, TOK_KW_FALSE,
    TOK_KW_IMPORT,
    TOK_KW_SYSCALL,
    TOK_KW_MUT,
    TOK_KW_STRUCT, TOK_KW_UNION,
    TOK_KW_BYTE, TOK_KW_UINT,
    TOK_KW_SIZEOF, TOK_KW_OFFSETOF,
    TOK_KW_VOLATILE, TOK_KW_ASM, TOK_KW_BREAK, TOK_KW_CONTINUE,
    TOK_KW_SWITCH, TOK_KW_CASE, TOK_KW_DEFAULT, TOK_KW_ENUM, TOK_KW_TYPEDEF,
    // Symbols
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR,
    TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_BITNOT, TOK_SHL, TOK_SHR,
    TOK_NOT,
    TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN, TOK_SLASH_ASSIGN, TOK_PERCENT_ASSIGN,
    TOK_AND_ASSIGN, TOK_OR_ASSIGN, TOK_XOR_ASSIGN, TOK_SHL_ASSIGN, TOK_SHR_ASSIGN,
    TOK_INC, TOK_DEC,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMI, TOK_COLON, TOK_QUESTION,
    TOK_DOT, TOK_ARROW,
};

struct Token {
    TokenType type;
    std::string text;
    Int128 value;   // for NUMBER
    int line;
    int col;
};

class Lexer {
public:
    bool tokenize(const std::string &filename);
    const std::vector<Token>& getTokens() const { return tokens; }
    const std::string& getError() const { return error; }
    int getErrorLine() const { return errorLine; }
    int getErrorCol() const { return errorCol; }
    const std::string& getFilename() const { return filename; }
    const std::string& getSource() const { return src; }
private:
    std::string filename;
    std::string src;
    std::vector<Token> tokens;
    std::string error;
    int errorLine = 0;
    int errorCol = 0;
    size_t pos;
    int line;
    int col;

    void skipWhitespace();
    void skipComment();
    bool readString();
    bool readChar();
    bool readNumber();
    bool readIdentOrKeyword();
    bool readSymbol();
    void emit(TokenType t, const std::string &txt, Int128 val = 0);
    void setError(const std::string &msg);
    bool isIdentStart(char c);
    bool isIdentChar(char c);
};

/* ---------- Preprocessor / Macro System ---------- */

struct Macro {
    std::vector<std::string> params;   // empty = object macro
    std::string body;
};

class Preprocessor {
public:
    bool preprocess(const std::string &input, std::string &output,
                    std::string &error, int &errorLine);
private:
    std::map<std::string, Macro> macros;

    struct CondState {
        bool enabled;   // is this block currently emitting?
        bool matched;   // has any branch in this #if been taken?
    };
    std::vector<CondState> condStack;

    bool processLine(const std::string &line, std::string &out,
                     std::string &error, int lineNum);
    bool processDirective(const std::string &directive, std::string &out,
                          std::string &error, int lineNum);
    bool procDefine(const std::string &rest, std::string &error);
    bool procUndef (const std::string &rest, std::string &error);
    bool procIfdef (const std::string &rest, std::string &error);
    bool procIfndef(const std::string &rest, std::string &error);
    bool procIf    (const std::string &rest, std::string &error, int line);
    bool procElif  (const std::string &rest, std::string &error, int line);
    bool procElse  (std::string &error);
    bool procEndif (std::string &error);

    bool condEnabled() const;
    bool evalExpr(const std::string &expr, bool &result, std::string &error);

    std::string expandMacros(const std::string &text);
    std::string expandRecursive(const std::string &text,
                                std::set<std::string> &expanding);
    std::string expandFuncMacro(const Macro &m, const std::string &argsRaw);

    static std::string trim(const std::string &s);
    static std::vector<std::string> splitArgs(const std::string &args);
};

#endif

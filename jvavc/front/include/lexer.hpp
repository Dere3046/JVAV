#ifndef LEXER_HPP
#define LEXER_HPP
#include <string>
#include <vector>
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
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_FOR, TOK_KW_RETURN,
    TOK_KW_INT, TOK_KW_CHAR, TOK_KW_BOOL, TOK_KW_VOID,
    TOK_KW_PTR, TOK_KW_ARRAY,
    TOK_KW_TRUE, TOK_KW_FALSE,
    TOK_KW_IMPORT,
    TOK_KW_SYSCALL,
    TOK_KW_MUT,
    // Symbols
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR,
    TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_BITNOT, TOK_SHL, TOK_SHR,
    TOK_NOT,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMI, TOK_COLON,
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

#endif

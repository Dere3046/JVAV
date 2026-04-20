#include "lexer.hpp"
#include <fstream>
#include <cctype>
#include <map>

using namespace std;

static const map<string, TokenType> keywords = {
    {"func", TOK_KW_FUNC}, {"var", TOK_KW_VAR}, {"const", TOK_KW_CONST},
    {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE}, {"while", TOK_KW_WHILE},
    {"for", TOK_KW_FOR}, {"return", TOK_KW_RETURN},
    {"int", TOK_KW_INT}, {"char", TOK_KW_CHAR}, {"bool", TOK_KW_BOOL},
    {"void", TOK_KW_VOID}, {"ptr", TOK_KW_PTR}, {"array", TOK_KW_ARRAY},
    {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
    {"import", TOK_KW_IMPORT},
    {"mut", TOK_KW_MUT},
};

bool Lexer::tokenize(const string &filename) {
    ifstream f(filename, ios::binary);
    if (!f) { error = "Cannot open file: " + filename; return false; }
    src = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    f.close();
    pos = 0;
    line = 1;
    col = 1;
    tokens.clear();

    while (pos < src.size()) {
        skipWhitespace();
        if (pos >= src.size()) break;
        skipComment();
        if (pos >= src.size()) break;

        char c = src[pos];
        if (c == '"') { if (!readString()) return false; }
        else if (c == '\'') { if (!readChar()) return false; }
        else if (isdigit(c) || (c == '-' && pos + 1 < src.size() && isdigit(src[pos + 1]) &&
                 (pos == 0 || isspace((unsigned char)src[pos-1]) || src[pos-1] == '(' || src[pos-1] == '[' || src[pos-1] == '{' ||
                  src[pos-1] == ';' || src[pos-1] == ',' || src[pos-1] == '='))) { if (!readNumber()) return false; }
        else if (isIdentStart(c)) { if (!readIdentOrKeyword()) return false; }
        else { if (!readSymbol()) return false; }
    }
    emit(TOK_EOF, "", 0);
    return true;
}

void Lexer::skipWhitespace() {
    while (pos < src.size() && isspace((unsigned char)src[pos])) {
        if (src[pos] == '\n') { line++; col = 1; }
        else { col++; }
        pos++;
    }
}

void Lexer::skipComment() {
    if (pos + 1 < src.size() && src[pos] == '/' && src[pos + 1] == '/') {
        pos += 2; col += 2;
        while (pos < src.size() && src[pos] != '\n') { pos++; col++; }
    }
    if (pos + 1 < src.size() && src[pos] == '/' && src[pos + 1] == '*') {
        pos += 2; col += 2;
        while (pos + 1 < src.size() && !(src[pos] == '*' && src[pos + 1] == '/')) {
            if (src[pos] == '\n') { line++; col = 1; }
            else { col++; }
            pos++;
        }
        if (pos + 1 < src.size()) { pos += 2; col += 2; }
    }
}

bool Lexer::readString() {
    pos++; col++;
    string s;
    while (pos < src.size() && src[pos] != '"') {
        if (src[pos] == '\\') {
            pos++; col++;
            if (pos >= src.size()) { error = "Unterminated string"; return false; }
            char c = src[pos];
            if (c == 'n') s += '\n';
            else if (c == 't') s += '\t';
            else if (c == 'r') s += '\r';
            else if (c == '\\') s += '\\';
            else if (c == '"') s += '"';
            else s += c;
        } else {
            s += src[pos];
        }
        pos++; col++;
    }
    if (pos >= src.size()) { error = "Unterminated string"; return false; }
    pos++; col++;
    emit(TOK_STRING, s, 0);
    return true;
}

bool Lexer::readChar() {
    pos++; col++;
    if (pos >= src.size()) { error = "Unterminated char"; return false; }
    char c = src[pos];
    if (c == '\\') {
        pos++; col++;
        if (pos >= src.size()) { error = "Unterminated char"; return false; }
        char esc = src[pos];
        if (esc == 'n') c = '\n';
        else if (esc == 't') c = '\t';
        else if (esc == 'r') c = '\r';
        else if (esc == '\\') c = '\\';
        else if (esc == '\'') c = '\'';
        else c = esc;
    }
    pos++; col++;
    if (pos >= src.size() || src[pos] != '\'') { error = "Expected ' after char"; return false; }
    pos++; col++;
    emit(TOK_CHAR, string(1, c), (unsigned char)c);
    return true;
}

bool Lexer::readNumber() {
    size_t start = pos;
    bool neg = false;
    if (src[pos] == '-') { neg = true; pos++; col++; }
    int base = 10;
    if (pos + 1 < src.size() && src[pos] == '0' && (src[pos+1] == 'x' || src[pos+1] == 'X')) {
        pos += 2; col += 2;
        base = 16;
    }
    __int128 val = 0;
    while (pos < src.size() && ((base==10 && isdigit(src[pos])) || (base==16 && isxdigit(src[pos])))) {
        int digit = (src[pos] <= '9') ? (src[pos]-'0') : (tolower(src[pos])-'a'+10);
        val = val * base + digit;
        pos++; col++;
    }
    if (neg) val = -val;
    emit(TOK_NUMBER, src.substr(start, pos-start), val);
    return true;
}

bool Lexer::readIdentOrKeyword() {
    size_t start = pos;
    while (pos < src.size() && isIdentChar(src[pos])) { pos++; col++; }
    string text = src.substr(start, pos-start);
    auto it = keywords.find(text);
    if (it != keywords.end()) emit(it->second, text, 0);
    else emit(TOK_IDENT, text, 0);
    return true;
}

bool Lexer::readSymbol() {
    char c = src[pos];
    char c2 = (pos+1 < src.size()) ? src[pos+1] : 0;
    
    switch (c) {
        case '+': pos++; col++; emit(TOK_PLUS, "+", 0); return true;
        case '-': pos++; col++; emit(TOK_MINUS, "-", 0); return true;
        case '*': pos++; col++; emit(TOK_STAR, "*", 0); return true;
        case '/': 
            if (c2 == '/' || c2 == '*') { skipComment(); return true; }
            pos++; col++; emit(TOK_SLASH, "/", 0); return true;
        case '%': pos++; col++; emit(TOK_PERCENT, "%", 0); return true;
        case '=': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_EQ, "==", 0); return true; }
            pos++; col++; emit(TOK_ASSIGN, "=", 0); return true;
        case '!':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_NE, "!=", 0); return true; }
            pos++; col++; emit(TOK_NOT, "!", 0); return true;
        case '<':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_LE, "<=", 0); return true; }
            if (c2 == '<') { pos+=2; col+=2; emit(TOK_SHL, "<<", 0); return true; }
            pos++; col++; emit(TOK_LT, "<", 0); return true;
        case '>':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_GE, ">=", 0); return true; }
            if (c2 == '>') { pos+=2; col+=2; emit(TOK_SHR, ">>", 0); return true; }
            pos++; col++; emit(TOK_GT, ">", 0); return true;
        case '&':
            if (c2 == '&') { pos+=2; col+=2; emit(TOK_AND, "&&", 0); return true; }
            pos++; col++; emit(TOK_BITAND, "&", 0); return true;
        case '|':
            if (c2 == '|') { pos+=2; col+=2; emit(TOK_OR, "||", 0); return true; }
            pos++; col++; emit(TOK_BITOR, "|", 0); return true;
        case '^': pos++; col++; emit(TOK_BITXOR, "^", 0); return true;
        case '~': pos++; col++; emit(TOK_BITNOT, "~", 0); return true;
        case '(': pos++; col++; emit(TOK_LPAREN, "(", 0); return true;
        case ')': pos++; col++; emit(TOK_RPAREN, ")", 0); return true;
        case '[': pos++; col++; emit(TOK_LBRACKET, "[", 0); return true;
        case ']': pos++; col++; emit(TOK_RBRACKET, "]", 0); return true;
        case '{': pos++; col++; emit(TOK_LBRACE, "{", 0); return true;
        case '}': pos++; col++; emit(TOK_RBRACE, "}", 0); return true;
        case ',': pos++; col++; emit(TOK_COMMA, ",", 0); return true;
        case ';': pos++; col++; emit(TOK_SEMI, ";", 0); return true;
        case ':': pos++; col++; emit(TOK_COLON, ":", 0); return true;
        default:
            error = "Unknown character '" + string(1, c) + "' at line " + to_string(line);
            return false;
    }
}

void Lexer::emit(TokenType t, const std::string &txt, __int128 val) {
    Token tok; tok.type = t; tok.text = txt; tok.value = val; tok.line = line; tok.col = col;
    tokens.push_back(tok);
}

bool Lexer::isIdentStart(char c) {
    return isalpha(c) || c == '_';
}

bool Lexer::isIdentChar(char c) {
    return isalnum(c) || c == '_';
}

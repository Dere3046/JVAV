#include "lexer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <map>

using namespace std;

static const map<string, TokenType> keywords = {
    {"func", TOK_KW_FUNC}, {"var", TOK_KW_VAR}, {"const", TOK_KW_CONST},
    {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE}, {"while", TOK_KW_WHILE},
    {"for", TOK_KW_FOR}, {"return", TOK_KW_RETURN}, {"do", TOK_KW_DO},
    {"int", TOK_KW_INT}, {"char", TOK_KW_CHAR}, {"bool", TOK_KW_BOOL},
    {"void", TOK_KW_VOID}, {"ptr", TOK_KW_PTR}, {"array", TOK_KW_ARRAY},
    {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
    {"import", TOK_KW_IMPORT},
    {"syscall", TOK_KW_SYSCALL},
    {"mut", TOK_KW_MUT},
    {"struct", TOK_KW_STRUCT}, {"union", TOK_KW_UNION},
    {"byte", TOK_KW_BYTE}, {"uint", TOK_KW_UINT},
    {"sizeof", TOK_KW_SIZEOF}, {"offsetof", TOK_KW_OFFSETOF},
    {"volatile", TOK_KW_VOLATILE}, {"asm", TOK_KW_ASM},
    {"break", TOK_KW_BREAK}, {"continue", TOK_KW_CONTINUE},
    {"switch", TOK_KW_SWITCH}, {"case", TOK_KW_CASE}, {"default", TOK_KW_DEFAULT},
    {"enum", TOK_KW_ENUM},
    {"typedef", TOK_KW_TYPEDEF},
};

bool Lexer::tokenize(const string &filename) {
    ifstream f(filename, ios::binary);
    if (!f) { error = "Cannot open file: " + filename; errorLine = 0; errorCol = 0; return false; }
    this->filename = filename;
    string raw = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    f.close();

    Preprocessor pp;
    string preprocessed;
    int ppLine = 0;
    if (!pp.preprocess(raw, preprocessed, error, ppLine)) {
        errorLine = ppLine;
        errorCol = 1;
        return false;
    }

    src = preprocessed;
    pos = 0;
    line = 1;
    col = 1;
    tokens.clear();

    while (pos < src.size()) {
        skipWhitespace();
        if (pos >= src.size()) break;
        skipComment();
        if (pos >= src.size()) break;
        skipWhitespace();
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
            if (pos >= src.size()) { error = "unterminated string literal"; errorLine = line; errorCol = col; return false; }
            char c = src[pos];
            if (c == 'n') s += '\n';
            else if (c == 't') s += '\t';
            else if (c == 'r') s += '\r';
            else if (c == '\\') s += '\\';
            else if (c == '"') s += '"';
            else if (c == 'x' && pos + 2 < src.size() && isxdigit((unsigned char)src[pos+1]) && isxdigit((unsigned char)src[pos+2])) {
                int val = 0;
                for (int i = 1; i <= 2; i++) {
                    char ch = src[pos + i];
                    int digit = isdigit((unsigned char)ch) ? (ch - '0') : (tolower((unsigned char)ch) - 'a' + 10);
                    val = val * 16 + digit;
                }
                s += (char)val;
                pos += 2; col += 2;
            }
            else s += c;
        } else {
            s += src[pos];
        }
        pos++; col++;
    }
    if (pos >= src.size()) { error = "unterminated string literal"; errorLine = line; errorCol = col; return false; }
    pos++; col++;
    emit(TOK_STRING, s, 0);
    return true;
}

bool Lexer::readChar() {
    pos++; col++;
    if (pos >= src.size()) { error = "unterminated character literal"; errorLine = line; errorCol = col; return false; }
    char c = src[pos];
    if (c == '\\') {
        pos++; col++;
        if (pos >= src.size()) { error = "unterminated character literal"; errorLine = line; errorCol = col; return false; }
        char esc = src[pos];
        if (esc == 'n') c = '\n';
        else if (esc == 't') c = '\t';
        else if (esc == 'r') c = '\r';
        else if (esc == '\\') c = '\\';
        else if (esc == '\'') c = '\'';
        else if (esc == 'x' && pos + 2 < src.size() && isxdigit((unsigned char)src[pos+1]) && isxdigit((unsigned char)src[pos+2])) {
            int val = 0;
            for (int i = 1; i <= 2; i++) {
                char ch = src[pos + i];
                int digit = isdigit((unsigned char)ch) ? (ch - '0') : (tolower((unsigned char)ch) - 'a' + 10);
                val = val * 16 + digit;
            }
            c = (char)val;
            pos += 2; col += 2;
        }
        else c = esc;
    }
    pos++; col++;
    if (pos >= src.size() || src[pos] != '\'') { error = "Expected ' after char"; errorLine = line; errorCol = col; return false; }
    pos++; col++;
    emit(TOK_CHAR, string(1, c), (unsigned char)c);
    return true;
}

bool Lexer::readNumber() {
    size_t start = pos;
    int startCol = col;
    bool neg = false;
    if (src[pos] == '-') { neg = true; pos++; col++; }
    int base = 10;
    if (pos + 1 < src.size() && src[pos] == '0' && (src[pos+1] == 'x' || src[pos+1] == 'X')) {
        pos += 2; col += 2;
        base = 16;
    } else if (pos + 1 < src.size() && src[pos] == '0' && (src[pos+1] == 'b' || src[pos+1] == 'B')) {
        pos += 2; col += 2;
        base = 2;
    }
    Int128 val = 0;
    while (pos < src.size() && ((base==10 && isdigit(src[pos])) || (base==16 && isxdigit(src[pos])) || (base==2 && (src[pos]=='0' || src[pos]=='1')))) {
        int digit;
        if (base == 2) {
            digit = src[pos] - '0';
        } else {
            digit = (src[pos] <= '9') ? (src[pos]-'0') : (tolower(src[pos])-'a'+10);
        }
        val = val * base + digit;
        pos++; col++;
    }
    if (neg) val = -val;
    int savedCol = col; col = startCol;
    emit(TOK_NUMBER, src.substr(start, pos-start), val);
    col = savedCol;
    return true;
}

bool Lexer::readIdentOrKeyword() {
    size_t start = pos;
    int startCol = col;
    while (pos < src.size() && isIdentChar(src[pos])) { pos++; col++; }
    string text = src.substr(start, pos-start);
    auto it = keywords.find(text);
    int savedCol = col; col = startCol;
    if (it != keywords.end()) emit(it->second, text, 0);
    else emit(TOK_IDENT, text, 0);
    col = savedCol;
    return true;
}

bool Lexer::readSymbol() {
    char c = src[pos];
    char c2 = (pos+1 < src.size()) ? src[pos+1] : 0;
    
    switch (c) {
        case '+': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_PLUS_ASSIGN, "+=", 0); return true; }
            if (c2 == '+') { pos+=2; col+=2; emit(TOK_INC, "++", 0); return true; }
            pos++; col++; emit(TOK_PLUS, "+", 0); return true;
        case '-': 
            if (c2 == '>') { pos+=2; col+=2; emit(TOK_ARROW, "->", 0); return true; }
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_MINUS_ASSIGN, "-=", 0); return true; }
            if (c2 == '-') { pos+=2; col+=2; emit(TOK_DEC, "--", 0); return true; }
            pos++; col++; emit(TOK_MINUS, "-", 0); return true;
        case '*': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_STAR_ASSIGN, "*=", 0); return true; }
            pos++; col++; emit(TOK_STAR, "*", 0); return true;
        case '/': 
            if (c2 == '/' || c2 == '*') { skipComment(); return true; }
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_SLASH_ASSIGN, "/=", 0); return true; }
            pos++; col++; emit(TOK_SLASH, "/", 0); return true;
        case '%': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_PERCENT_ASSIGN, "%=", 0); return true; }
            pos++; col++; emit(TOK_PERCENT, "%", 0); return true;
        case '=': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_EQ, "==", 0); return true; }
            pos++; col++; emit(TOK_ASSIGN, "=", 0); return true;
        case '!':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_NE, "!=", 0); return true; }
            pos++; col++; emit(TOK_NOT, "!", 0); return true;
        case '<':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_LE, "<=", 0); return true; }
            if (c2 == '<') {
                if (pos+2 < src.size() && src[pos+2] == '=') { pos+=3; col+=3; emit(TOK_SHL_ASSIGN, "<<=", 0); return true; }
                pos+=2; col+=2; emit(TOK_SHL, "<<", 0); return true;
            }
            pos++; col++; emit(TOK_LT, "<", 0); return true;
        case '>':
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_GE, ">=", 0); return true; }
            if (c2 == '>') {
                if (pos+2 < src.size() && src[pos+2] == '=') { pos+=3; col+=3; emit(TOK_SHR_ASSIGN, ">>=", 0); return true; }
                pos+=2; col+=2; emit(TOK_SHR, ">>", 0); return true;
            }
            pos++; col++; emit(TOK_GT, ">", 0); return true;
        case '&':
            if (c2 == '&') { pos+=2; col+=2; emit(TOK_AND, "&&", 0); return true; }
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_AND_ASSIGN, "&=", 0); return true; }
            pos++; col++; emit(TOK_BITAND, "&", 0); return true;
        case '|':
            if (c2 == '|') { pos+=2; col+=2; emit(TOK_OR, "||", 0); return true; }
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_OR_ASSIGN, "|=", 0); return true; }
            pos++; col++; emit(TOK_BITOR, "|", 0); return true;
        case '^': 
            if (c2 == '=') { pos+=2; col+=2; emit(TOK_XOR_ASSIGN, "^=", 0); return true; }
            pos++; col++; emit(TOK_BITXOR, "^", 0); return true;
        case '~': pos++; col++; emit(TOK_BITNOT, "~", 0); return true;
        case '(': pos++; col++; emit(TOK_LPAREN, "(", 0); return true;
        case ')': pos++; col++; emit(TOK_RPAREN, ")", 0); return true;
        case '[': pos++; col++; emit(TOK_LBRACKET, "[", 0); return true;
        case ']': pos++; col++; emit(TOK_RBRACKET, "]", 0); return true;
        case '{': pos++; col++; emit(TOK_LBRACE, "{", 0); return true;
        case '}': pos++; col++; emit(TOK_RBRACE, "}", 0); return true;
        case '.': pos++; col++; emit(TOK_DOT, ".", 0); return true;
        case ',': pos++; col++; emit(TOK_COMMA, ",", 0); return true;
        case ';': pos++; col++; emit(TOK_SEMI, ";", 0); return true;
        case ':': pos++; col++; emit(TOK_COLON, ":", 0); return true;
        case '?': pos++; col++; emit(TOK_QUESTION, "?", 0); return true;
        default:
            error = "unknown character `" + string(1, c) + "`"; errorLine = line; errorCol = col;
            return false;
    }
}

void Lexer::emit(TokenType t, const std::string &txt, Int128 val) {
    Token tok; tok.type = t; tok.text = txt; tok.value = val; tok.line = line; tok.col = col;
    tokens.push_back(tok);
}

bool Lexer::isIdentStart(char c) {
    return isalpha(c) || c == '_';
}

bool Lexer::isIdentChar(char c) {
    return isalnum(c) || c == '_';
}

/* ================================================================
   Preprocessor / Macro System
   ================================================================ */

static string ppTrim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool Preprocessor::preprocess(const string &input, string &output,
                              string &error, int &errorLine) {
    condStack.clear();
    macros.clear();
    istringstream in(input);
    string line;
    int lineNum = 0;
    output.clear();

    vector<string> lines;
    while (getline(in, line)) lines.push_back(line);
    for (size_t i = 0; i < lines.size(); i++) {
        line = lines[i];
        bool cr = !line.empty() && line.back() == '\r';
        if (cr) line.pop_back();

        string processed;
        if (!processLine(line, processed, error, (int)i + 1)) {
            errorLine = (int)i + 1;
            return false;
        }
        output += processed;
        if (i + 1 < lines.size()) output += '\n';
    }

    if (!condStack.empty()) {
        error = "unterminated conditional directive";
        errorLine = lineNum;
        return false;
    }
    return true;
}

bool Preprocessor::processLine(const string &line, string &out,
                               string &error, int lineNum) {
    size_t i = 0;
    while (i < line.size() && isspace((unsigned char)line[i])) i++;
    if (i < line.size() && line[i] == '#') {
        return processDirective(line.substr(i + 1), out, error, lineNum);
    }
    if (!condEnabled()) {
        out = "";
        return true;
    }
    out = expandMacros(line);
    return true;
}

bool Preprocessor::processDirective(const string &directive, string &out,
                                    string &error, int lineNum) {
    out = "";
    size_t i = 0;
    while (i < directive.size() && isspace((unsigned char)directive[i])) i++;
    size_t start = i;
    while (i < directive.size() && (isalpha((unsigned char)directive[i]) || directive[i] == '_')) i++;
    string cmd = directive.substr(start, i - start);
    string rest = ppTrim(directive.substr(i));

    if (cmd == "ifdef")  return procIfdef(rest, error);
    if (cmd == "ifndef") return procIfndef(rest, error);
    if (cmd == "if")     return procIf(rest, error, lineNum);
    if (cmd == "elif")   return procElif(rest, error, lineNum);
    if (cmd == "else")   return procElse(error);
    if (cmd == "endif")  return procEndif(error);
    // In disabled conditional blocks, only process conditional directives
    if (!condEnabled()) return true;
    if (cmd == "define") return procDefine(rest, error);
    if (cmd == "undef")  return procUndef(rest, error);
    if (cmd == "error")  { error = "#error: " + rest; return false; }
    // Unknown directive: ignore in enabled blocks
    return true;
}

bool Preprocessor::procDefine(const string &rest, string &error) {
    size_t i = 0;
    while (i < rest.size() && isspace((unsigned char)rest[i])) i++;
    if (i >= rest.size() || !(isalpha((unsigned char)rest[i]) || rest[i] == '_')) {
        error = "#define: invalid macro name"; return false;
    }
    size_t nameStart = i;
    while (i < rest.size() && (isalnum((unsigned char)rest[i]) || rest[i] == '_')) i++;
    string name = rest.substr(nameStart, i - nameStart);

    Macro macro;
    if (i < rest.size() && rest[i] == '(') {
        i++; // skip (
        while (i < rest.size() && rest[i] != ')') {
            while (i < rest.size() && isspace((unsigned char)rest[i])) i++;
            if (i >= rest.size() || rest[i] == ')') break;
            size_t ps = i;
            while (i < rest.size() && (isalnum((unsigned char)rest[i]) || rest[i] == '_')) i++;
            macro.params.push_back(rest.substr(ps, i - ps));
            while (i < rest.size() && isspace((unsigned char)rest[i])) i++;
            if (i < rest.size() && rest[i] == ',') { i++; }
        }
        if (i >= rest.size() || rest[i] != ')') { error = "#define: missing )"; return false; }
        i++; // skip )
    }
    while (i < rest.size() && isspace((unsigned char)rest[i])) i++;
    macro.body = rest.substr(i);
    macros[name] = macro;
    return true;
}

bool Preprocessor::procUndef(const string &rest, string &error) {
    string name = ppTrim(rest);
    macros.erase(name);
    return true;
}

bool Preprocessor::procIfdef(const string &rest, string &error) {
    string name = ppTrim(rest);
    bool e = macros.find(name) != macros.end();
    condStack.push_back({e, e});
    return true;
}

bool Preprocessor::procIfndef(const string &rest, string &error) {
    string name = ppTrim(rest);
    bool e = macros.find(name) == macros.end();
    condStack.push_back({e, e});
    return true;
}

bool Preprocessor::procIf(const string &rest, string &error, int line) {
    bool r = false;
    if (!evalExpr(rest, r, error)) return false;
    condStack.push_back({r, r});
    return true;
}

bool Preprocessor::procElif(const string &rest, string &error, int line) {
    if (condStack.empty()) { error = "#elif without #if"; return false; }
    CondState &st = condStack.back();
    if (st.matched) {
        st.enabled = false;
        return true;
    }
    bool r = false;
    if (!evalExpr(rest, r, error)) return false;
    st.enabled = r;
    if (r) st.matched = true;
    return true;
}

bool Preprocessor::procElse(string &error) {
    if (condStack.empty()) { error = "#else without #if"; return false; }
    CondState &st = condStack.back();
    st.enabled = !st.matched;
    return true;
}

bool Preprocessor::procEndif(string &error) {
    if (condStack.empty()) { error = "#endif without #if"; return false; }
    condStack.pop_back();
    return true;
}

bool Preprocessor::condEnabled() const {
    for (const auto &s : condStack) if (!s.enabled) return false;
    return true;
}

/* ---------- Macro expansion ---------- */

string Preprocessor::expandMacros(const string &text) {
    set<string> expanding;
    return expandRecursive(text, expanding);
}

string Preprocessor::expandRecursive(const string &text, set<string> &expanding) {
    string result;
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        // Skip string literals
        if (c == '"') {
            result += c; i++;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.size()) { result += text[i]; i++; }
                result += text[i]; i++;
            }
            if (i < text.size()) { result += text[i]; i++; }
            continue;
        }
        // Skip char literals
        if (c == '\'') {
            result += c; i++;
            while (i < text.size() && text[i] != '\'') {
                if (text[i] == '\\' && i + 1 < text.size()) { result += text[i]; i++; }
                result += text[i]; i++;
            }
            if (i < text.size()) { result += text[i]; i++; }
            continue;
        }
        // Skip line comments
        if (c == '/' && i + 1 < text.size() && text[i+1] == '/') {
            result += text.substr(i);
            break;
        }
        // Skip block comments
        if (c == '/' && i + 1 < text.size() && text[i+1] == '*') {
            size_t end = text.find("*/", i + 2);
            if (end == string::npos) { result += text.substr(i); break; }
            result += text.substr(i, end - i + 2);
            i = end + 2;
            continue;
        }
        // Identifier
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < text.size() && (isalnum((unsigned char)text[i]) || text[i] == '_')) i++;
            string name = text.substr(start, i - start);
            auto it = macros.find(name);
            if (it != macros.end() && expanding.find(name) == expanding.end()) {
                const Macro &m = it->second;
                if (m.params.empty()) {
                    expanding.insert(name);
                    string expanded = expandRecursive(m.body, expanding);
                    expanding.erase(name);
                    result += expanded;
                } else {
                    // Function macro: must be followed by '('
                    size_t j = i;
                    while (j < text.size() && isspace((unsigned char)text[j])) j++;
                    if (j < text.size() && text[j] == '(') {
                        string args;
                        int depth = 0;
                        j++; // skip (
                        while (j < text.size()) {
                            if (text[j] == '(') depth++;
                            if (text[j] == ')') { if (depth == 0) break; depth--; }
                            args += text[j];
                            j++;
                        }
                        if (j < text.size() && text[j] == ')') j++; // skip )
                        i = j;
                        expanding.insert(name);
                        string expanded = expandRecursive(expandFuncMacro(m, args), expanding);
                        expanding.erase(name);
                        result += expanded;
                    } else {
                        result += name;
                    }
                }
                continue;
            }
            result += name;
            continue;
        }
        result += c;
        i++;
    }
    return result;
}

string Preprocessor::expandFuncMacro(const Macro &m, const string &argsRaw) {
    vector<string> args = splitArgs(argsRaw);
    string result;
    size_t i = 0;
    while (i < m.body.size()) {
        if (isalpha((unsigned char)m.body[i]) || m.body[i] == '_') {
            size_t s = i;
            while (i < m.body.size() && (isalnum((unsigned char)m.body[i]) || m.body[i] == '_')) i++;
            string name = m.body.substr(s, i - s);
            auto it = find(m.params.begin(), m.params.end(), name);
            if (it != m.params.end()) {
                size_t idx = it - m.params.begin();
                if (idx < args.size()) result += args[idx];
            } else {
                result += name;
            }
        } else {
            result += m.body[i];
            i++;
        }
    }
    return result;
}

vector<string> Preprocessor::splitArgs(const string &args) {
    vector<string> list;
    string cur;
    int depth = 0;
    for (char c : args) {
        if (c == '(') { depth++; cur += c; }
        else if (c == ')') { depth--; cur += c; }
        else if (c == ',' && depth == 0) {
            list.push_back(ppTrim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    string t = ppTrim(cur);
    if (!t.empty() || !list.empty()) list.push_back(t);
    return list;
}

string Preprocessor::trim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/* ---------- #if expression evaluator ---------- */

struct ExprTok {
    enum Type { END, NUM, IDENT, LPAREN, RPAREN,
                AND, OR, NOT, BITAND, BITOR, BITXOR, BITNOT,
                SHL, SHR, EQ, NE, LT, GT, LE, GE,
                PLUS, MINUS, STAR, SLASH, PERCENT } type;
    int64_t value;
};

static vector<ExprTok> tokenizeIfExpr(const string &expr) {
    vector<ExprTok> toks;
    size_t i = 0;
    while (i < expr.size()) {
        while (i < expr.size() && isspace((unsigned char)expr[i])) i++;
        if (i >= expr.size()) break;
        char c = expr[i];
        // two-char operators
        if (i + 1 < expr.size()) {
            string two = expr.substr(i, 2);
            if (two == "&&") { toks.push_back({ExprTok::AND}); i += 2; continue; }
            if (two == "||") { toks.push_back({ExprTok::OR}); i += 2; continue; }
            if (two == "==") { toks.push_back({ExprTok::EQ}); i += 2; continue; }
            if (two == "!=") { toks.push_back({ExprTok::NE}); i += 2; continue; }
            if (two == "<=") { toks.push_back({ExprTok::LE}); i += 2; continue; }
            if (two == ">=") { toks.push_back({ExprTok::GE}); i += 2; continue; }
            if (two == "<<") { toks.push_back({ExprTok::SHL}); i += 2; continue; }
            if (two == ">>") { toks.push_back({ExprTok::SHR}); i += 2; continue; }
        }
        if (c == '!') { toks.push_back({ExprTok::NOT}); i++; continue; }
        if (c == '&') { toks.push_back({ExprTok::BITAND}); i++; continue; }
        if (c == '|') { toks.push_back({ExprTok::BITOR}); i++; continue; }
        if (c == '^') { toks.push_back({ExprTok::BITXOR}); i++; continue; }
        if (c == '~') { toks.push_back({ExprTok::BITNOT}); i++; continue; }
        if (c == '<') { toks.push_back({ExprTok::LT}); i++; continue; }
        if (c == '>') { toks.push_back({ExprTok::GT}); i++; continue; }
        if (c == '+') { toks.push_back({ExprTok::PLUS}); i++; continue; }
        if (c == '-') { toks.push_back({ExprTok::MINUS}); i++; continue; }
        if (c == '*') { toks.push_back({ExprTok::STAR}); i++; continue; }
        if (c == '/') { toks.push_back({ExprTok::SLASH}); i++; continue; }
        if (c == '%') { toks.push_back({ExprTok::PERCENT}); i++; continue; }
        if (c == '(') { toks.push_back({ExprTok::LPAREN}); i++; continue; }
        if (c == ')') { toks.push_back({ExprTok::RPAREN}); i++; continue; }
        // number
        if (isdigit(c) || (c == '-' && i + 1 < expr.size() && isdigit(expr[i+1]))) {
            bool neg = false;
            if (expr[i] == '-') { neg = true; i++; }
            int64_t v = 0;
            int base = 10;
            if (i + 1 < expr.size() && expr[i] == '0' && (expr[i+1] == 'x' || expr[i+1] == 'X')) {
                i += 2; base = 16;
            } else if (i + 1 < expr.size() && expr[i] == '0' && (expr[i+1] == 'b' || expr[i+1] == 'B')) {
                i += 2; base = 2;
            }
            while (i < expr.size() && ((base == 10 && isdigit(expr[i])) || (base == 16 && isxdigit(expr[i])) || (base == 2 && (expr[i]=='0' || expr[i]=='1')))) {
                int d = (base == 2) ? (expr[i]-'0') : (isdigit(expr[i]) ? expr[i]-'0' : tolower(expr[i])-'a'+10);
                v = v * base + d; i++;
            }
            toks.push_back({ExprTok::NUM, neg ? -v : v});
            continue;
        }
        // identifier
        if (isalpha((unsigned char)c) || c == '_') {
            size_t s = i;
            while (i < expr.size() && (isalnum((unsigned char)expr[i]) || expr[i] == '_')) i++;
            string name = expr.substr(s, i - s);
            if (name == "defined") {
                toks.push_back({ExprTok::IDENT, 0});
            } else {
                // undefined identifier in #if evaluates to 0
                toks.push_back({ExprTok::NUM, 0});
            }
            continue;
        }
        i++; // skip unknown
    }
    toks.push_back({ExprTok::END});
    return toks;
}

struct ExprParser {
    const vector<ExprTok> &toks;
    size_t pos = 0;
    const map<string, Macro> *macros = nullptr;

    ExprParser(const vector<ExprTok> &t, const map<string, Macro> *m) : toks(t), macros(m) {}

    int64_t parse() { return parseOr(); }

    int64_t parseOr() {
        int64_t left = parseAnd();
        while (match(ExprTok::OR)) {
            int64_t right = parseAnd();
            left = (left || right) ? 1 : 0;
        }
        return left;
    }
    int64_t parseAnd() {
        int64_t left = parseBitOr();
        while (match(ExprTok::AND)) {
            int64_t right = parseBitOr();
            left = (left && right) ? 1 : 0;
        }
        return left;
    }
    int64_t parseBitOr() {
        int64_t left = parseBitXor();
        while (match(ExprTok::BITOR)) { left |= parseBitXor(); }
        return left;
    }
    int64_t parseBitXor() {
        int64_t left = parseBitAnd();
        while (match(ExprTok::BITXOR)) { left ^= parseBitAnd(); }
        return left;
    }
    int64_t parseBitAnd() {
        int64_t left = parseEq();
        while (match(ExprTok::BITAND)) { left &= parseEq(); }
        return left;
    }
    int64_t parseEq() {
        int64_t left = parseRel();
        while (true) {
            if (match(ExprTok::EQ)) { left = (left == parseRel()) ? 1 : 0; }
            else if (match(ExprTok::NE)) { left = (left != parseRel()) ? 1 : 0; }
            else break;
        }
        return left;
    }
    int64_t parseRel() {
        int64_t left = parseShift();
        while (true) {
            if (match(ExprTok::LT)) { left = (left < parseShift()) ? 1 : 0; }
            else if (match(ExprTok::GT)) { left = (left > parseShift()) ? 1 : 0; }
            else if (match(ExprTok::LE)) { left = (left <= parseShift()) ? 1 : 0; }
            else if (match(ExprTok::GE)) { left = (left >= parseShift()) ? 1 : 0; }
            else break;
        }
        return left;
    }
    int64_t parseShift() {
        int64_t left = parseAdd();
        while (true) {
            if (match(ExprTok::SHL)) { left <<= parseAdd(); }
            else if (match(ExprTok::SHR)) { left >>= parseAdd(); }
            else break;
        }
        return left;
    }
    int64_t parseAdd() {
        int64_t left = parseMul();
        while (true) {
            if (match(ExprTok::PLUS)) { left += parseMul(); }
            else if (match(ExprTok::MINUS)) { left -= parseMul(); }
            else break;
        }
        return left;
    }
    int64_t parseMul() {
        int64_t left = parseUnary();
        while (true) {
            if (match(ExprTok::STAR)) { left *= parseUnary(); }
            else if (match(ExprTok::SLASH)) { int64_t r = parseUnary(); left = r ? left / r : 0; }
            else if (match(ExprTok::PERCENT)) { int64_t r = parseUnary(); left = r ? left % r : 0; }
            else break;
        }
        return left;
    }
    int64_t parseUnary() {
        if (match(ExprTok::NOT)) return parseUnary() ? 0 : 1;
        if (match(ExprTok::BITNOT)) return ~parseUnary();
        if (match(ExprTok::MINUS)) return -parseUnary();
        return parsePrimary();
    }
    int64_t parsePrimary() {
        if (match(ExprTok::NUM)) return toks[pos-1].value;
        if (match(ExprTok::IDENT)) {
            // "defined" operator
            bool paren = match(ExprTok::LPAREN);
            if (pos < toks.size() && toks[pos].type == ExprTok::NUM) {
                // The tokenizer turns identifiers after "defined" into NUM(0)
                // Actually we need to handle this differently.
                // Let me re-design: tokenizeIfExpr should keep identifier names
                // But for simplicity, if we see "defined", we parse the next token as name.
                // Since unknown identifiers become NUM(0), let's change the tokenizer.
            }
            return 0;
        }
        if (match(ExprTok::LPAREN)) {
            int64_t v = parseOr();
            match(ExprTok::RPAREN); // consume if present
            return v;
        }
        return 0;
    }
    bool match(ExprTok::Type t) {
        if (pos < toks.size() && toks[pos].type == t) { pos++; return true; }
        return false;
    }
};

// Since the tokenizer turns unknown identifiers into NUM(0), "defined(X)" won't work
// properly. We need a custom tokenizer that preserves identifiers for the "defined"
// operator. Let me override the tokenizer for expressions.

static vector<ExprTok> tokenizeIfExpr2(const string &expr) {
    vector<ExprTok> toks;
    size_t i = 0;
    while (i < expr.size()) {
        while (i < expr.size() && isspace((unsigned char)expr[i])) i++;
        if (i >= expr.size()) break;
        char c = expr[i];
        if (i + 1 < expr.size()) {
            string two = expr.substr(i, 2);
            if (two == "&&") { toks.push_back({ExprTok::AND}); i += 2; continue; }
            if (two == "||") { toks.push_back({ExprTok::OR}); i += 2; continue; }
            if (two == "==") { toks.push_back({ExprTok::EQ}); i += 2; continue; }
            if (two == "!=") { toks.push_back({ExprTok::NE}); i += 2; continue; }
            if (two == "<=") { toks.push_back({ExprTok::LE}); i += 2; continue; }
            if (two == ">=") { toks.push_back({ExprTok::GE}); i += 2; continue; }
            if (two == "<<") { toks.push_back({ExprTok::SHL}); i += 2; continue; }
            if (two == ">>") { toks.push_back({ExprTok::SHR}); i += 2; continue; }
        }
        if (c == '!') { toks.push_back({ExprTok::NOT}); i++; continue; }
        if (c == '&') { toks.push_back({ExprTok::BITAND}); i++; continue; }
        if (c == '|') { toks.push_back({ExprTok::BITOR}); i++; continue; }
        if (c == '^') { toks.push_back({ExprTok::BITXOR}); i++; continue; }
        if (c == '~') { toks.push_back({ExprTok::BITNOT}); i++; continue; }
        if (c == '<') { toks.push_back({ExprTok::LT}); i++; continue; }
        if (c == '>') { toks.push_back({ExprTok::GT}); i++; continue; }
        if (c == '+') { toks.push_back({ExprTok::PLUS}); i++; continue; }
        if (c == '-') { toks.push_back({ExprTok::MINUS}); i++; continue; }
        if (c == '*') { toks.push_back({ExprTok::STAR}); i++; continue; }
        if (c == '/') { toks.push_back({ExprTok::SLASH}); i++; continue; }
        if (c == '%') { toks.push_back({ExprTok::PERCENT}); i++; continue; }
        if (c == '(') { toks.push_back({ExprTok::LPAREN}); i++; continue; }
        if (c == ')') { toks.push_back({ExprTok::RPAREN}); i++; continue; }
        if (isdigit(c)) {
            int64_t v = 0;
            int base = 10;
            if (i + 1 < expr.size() && expr[i] == '0' && (expr[i+1] == 'x' || expr[i+1] == 'X')) {
                i += 2; base = 16;
            }
            while (i < expr.size() && ((base == 10 && isdigit(expr[i])) || (base == 16 && isxdigit(expr[i])))) {
                int d = isdigit(expr[i]) ? expr[i]-'0' : tolower(expr[i])-'a'+10;
                v = v * base + d; i++;
            }
            toks.push_back({ExprTok::NUM, v});
            continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            size_t s = i;
            while (i < expr.size() && (isalnum((unsigned char)expr[i]) || expr[i] == '_')) i++;
            string name = expr.substr(s, i - s);
            if (name == "defined") {
                toks.push_back({ExprTok::IDENT, 0});
            } else {
                // Unknown identifier: preserve as IDENT so we can check defined() properly
                toks.push_back({ExprTok::IDENT, 0});
                // We'll need to store the name somehow... but ExprTok doesn't have a string field.
            }
            continue;
        }
        i++;
    }
    toks.push_back({ExprTok::END});
    return toks;
}

// Since ExprTok doesn't store identifier names, I'll modify the approach:
// Pre-expand macros in the expression text first, then evaluate as pure arithmetic.
// For "defined(NAME)", replace it with 1 or 0 before tokenization.

bool Preprocessor::evalExpr(const string &expr, bool &result, string &error) {
    // Replace "defined(NAME)" and "defined NAME" with 1 or 0
    string e = expr;
    size_t pos = 0;
    while (true) {
        pos = e.find("defined", pos);
        if (pos == string::npos) break;
        size_t after = pos + 7;
        while (after < e.size() && isspace((unsigned char)e[after])) after++;
        bool paren = false;
        if (after < e.size() && e[after] == '(') { paren = true; after++; }
        while (after < e.size() && isspace((unsigned char)e[after])) after++;
        size_t nameStart = after;
        while (after < e.size() && (isalnum((unsigned char)e[after]) || e[after] == '_')) after++;
        string name = e.substr(nameStart, after - nameStart);
        while (after < e.size() && isspace((unsigned char)e[after])) after++;
        if (paren && after < e.size() && e[after] == ')') after++;

        bool defined = macros.find(name) != macros.end();
        string replacement = defined ? "1" : "0";
        e.replace(pos, after - pos, replacement);
        pos += replacement.size();
    }

    // Now expand any remaining macros
    e = expandMacros(e);

    // Tokenize and evaluate
    vector<ExprTok> toks;
    size_t i = 0;
    while (i < e.size()) {
        while (i < e.size() && isspace((unsigned char)e[i])) i++;
        if (i >= e.size()) break;
        char c = e[i];
        if (i + 1 < e.size()) {
            string two = e.substr(i, 2);
            if (two == "&&") { toks.push_back({ExprTok::AND}); i += 2; continue; }
            if (two == "||") { toks.push_back({ExprTok::OR}); i += 2; continue; }
            if (two == "==") { toks.push_back({ExprTok::EQ}); i += 2; continue; }
            if (two == "!=") { toks.push_back({ExprTok::NE}); i += 2; continue; }
            if (two == "<=") { toks.push_back({ExprTok::LE}); i += 2; continue; }
            if (two == ">=") { toks.push_back({ExprTok::GE}); i += 2; continue; }
            if (two == "<<") { toks.push_back({ExprTok::SHL}); i += 2; continue; }
            if (two == ">>") { toks.push_back({ExprTok::SHR}); i += 2; continue; }
        }
        if (c == '!') { toks.push_back({ExprTok::NOT}); i++; continue; }
        if (c == '&') { toks.push_back({ExprTok::BITAND}); i++; continue; }
        if (c == '|') { toks.push_back({ExprTok::BITOR}); i++; continue; }
        if (c == '^') { toks.push_back({ExprTok::BITXOR}); i++; continue; }
        if (c == '~') { toks.push_back({ExprTok::BITNOT}); i++; continue; }
        if (c == '<') { toks.push_back({ExprTok::LT}); i++; continue; }
        if (c == '>') { toks.push_back({ExprTok::GT}); i++; continue; }
        if (c == '+') { toks.push_back({ExprTok::PLUS}); i++; continue; }
        if (c == '-') { toks.push_back({ExprTok::MINUS}); i++; continue; }
        if (c == '*') { toks.push_back({ExprTok::STAR}); i++; continue; }
        if (c == '/') { toks.push_back({ExprTok::SLASH}); i++; continue; }
        if (c == '%') { toks.push_back({ExprTok::PERCENT}); i++; continue; }
        if (c == '(') { toks.push_back({ExprTok::LPAREN}); i++; continue; }
        if (c == ')') { toks.push_back({ExprTok::RPAREN}); i++; continue; }
        if (isdigit(c) || (c == '-' && i + 1 < e.size() && isdigit(e[i+1]))) {
            bool neg = false;
            if (e[i] == '-') { neg = true; i++; }
            int64_t v = 0;
            int base = 10;
            if (i + 1 < e.size() && e[i] == '0' && (e[i+1] == 'x' || e[i+1] == 'X')) {
                i += 2; base = 16;
            }
            while (i < e.size() && ((base == 10 && isdigit(e[i])) || (base == 16 && isxdigit(e[i])))) {
                int d = isdigit(e[i]) ? e[i]-'0' : tolower(e[i])-'a'+10;
                v = v * base + d; i++;
            }
            toks.push_back({ExprTok::NUM, neg ? -v : v});
            continue;
        }
        // Unknown token in expression - skip
        i++;
    }
    toks.push_back({ExprTok::END});

    ExprParser parser(toks, &macros);
    int64_t val = parser.parse();
    result = (val != 0);
    return true;
}


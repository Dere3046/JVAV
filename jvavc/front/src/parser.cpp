#include "parser.hpp"
#include <iostream>
using namespace std;

#define CURRENT (peek(0))

const Token& FrontParser::peek(int ahead) const {
    static Token eofTok{TOK_EOF, "", 0, -1, -1};
    if (pos + ahead < tokens.size()) return tokens[pos + ahead];
    return eofTok;
}

const Token& FrontParser::advance() {
    if (pos < tokens.size()) return tokens[pos++];
    return peek(0);
}

FrontParser::ParserState FrontParser::saveState() {
    return {pos, error, errorLine, errorCol};
}

void FrontParser::restoreState(const ParserState &s) {
    pos = s.pos;
    error = s.error;
    errorLine = s.errorLine;
    errorCol = s.errorCol;
}

bool FrontParser::expect(TokenType t) {
    if (peek().type == t) { advance(); return true; }
    const char* names[] = {
        "EOF", "identifier", "number", "string", "char",
        "func", "var", "const", "if", "else", "while", "for", "return", "do",
        "int", "char", "bool", "void", "ptr", "array",
        "true", "false", "import", "syscall", "mut",
        "struct", "union", "byte", "uint", "sizeof", "offsetof",
        "volatile", "asm", "break", "continue",
        "switch", "case", "default", "enum", "typedef",
        "+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=",
        "&&", "||", "&", "|", "^", "~", "<<", ">>", "!",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=",
        "++", "--",
        "(", ")", "[", "]", "{", "}", ",", ";", ":", "?", ".", "->"
    };
    const char* got = (t >= 0 && t <= TOK_ARROW) ? names[t] : "?";
    errorLine = CURRENT.line;
    errorCol = CURRENT.col;
    string msg = "expected `" + string(got) + "`, but found `" + CURRENT.text + "`";
    error = msg;
    reportError(msg);
    return false;
}

bool FrontParser::match(TokenType t) {
    if (peek().type == t) { advance(); return true; }
    return false;
}

bool FrontParser::check(TokenType t) const {
    return peek().type == t;
}

bool FrontParser::isTypeToken() {
    TokenType t = peek().type;
    return t == TOK_KW_INT || t == TOK_KW_CHAR || t == TOK_KW_BOOL || t == TOK_KW_VOID
        || t == TOK_KW_PTR || t == TOK_KW_ARRAY
        || t == TOK_KW_BYTE || t == TOK_KW_UINT
        || t == TOK_KW_STRUCT || t == TOK_KW_UNION
        || t == TOK_KW_VOLATILE;
}

string Type::toString() const {
    string vol = isVolatile ? "volatile " : "";
    switch (kind) {
        case TYPE_INT: return vol + "int";
        case TYPE_CHAR: return vol + "char";
        case TYPE_BOOL: return vol + "bool";
        case TYPE_VOID: return vol + "void";
        case TYPE_BYTE: return vol + "byte";
        case TYPE_UINT: return vol + "uint";
        case TYPE_PTR: return vol + "ptr<" + (sub ? sub->toString() : "?") + ">";
        case TYPE_ARRAY: {
            string s = vol + (sub ? sub->toString() : "?");
            s += "[" + to_string(arraySize) + "]";
            return s;
        }
        case TYPE_STRUCT: return vol + "struct " + structName;
        case TYPE_UNION: return vol + "union " + structName;
    }
    return "?";
}

shared_ptr<Type> FrontParser::parseType() {
    bool vol = false;
    if (match(TOK_KW_VOLATILE)) vol = true;

    auto t = make_shared<Type>();
    t->isVolatile = vol;

    if (match(TOK_KW_INT)) t->kind = TYPE_INT;
    else if (match(TOK_KW_CHAR)) t->kind = TYPE_CHAR;
    else if (match(TOK_KW_BOOL)) t->kind = TYPE_BOOL;
    else if (match(TOK_KW_VOID)) t->kind = TYPE_VOID;
    else if (match(TOK_KW_BYTE)) t->kind = TYPE_BYTE;
    else if (match(TOK_KW_UINT)) t->kind = TYPE_UINT;
    else if (match(TOK_KW_PTR)) {
        t->kind = TYPE_PTR;
        if (!expect(TOK_LT)) return nullptr;
        t->sub = parseType();
        if (!t->sub) return nullptr;
        if (!expect(TOK_GT)) return nullptr;
    }
    else if (match(TOK_KW_ARRAY)) {
        t->kind = TYPE_ARRAY;
        if (!expect(TOK_LT)) return nullptr;
        t->sub = parseType();
        if (!t->sub) return nullptr;
        if (!expect(TOK_GT)) return nullptr;
    }
    else if (match(TOK_KW_STRUCT)) {
        if (!expect(TOK_IDENT)) return nullptr;
        t->kind = TYPE_STRUCT;
        t->structName = peek(-1).text;
    }
    else if (match(TOK_KW_UNION)) {
        if (!expect(TOK_IDENT)) return nullptr;
        t->kind = TYPE_UNION;
        t->structName = peek(-1).text;
    }
    else if (check(TOK_IDENT)) {
        // Could be a typedef name; resolve in sema
        t->kind = TYPE_STRUCT;
        t->structName = CURRENT.text;
        advance();
    }
    else {
        errorLine = CURRENT.line;
        errorCol = CURRENT.col;
        error = "expected a type, but found `" + CURRENT.text + "`";
        return nullptr;
    }

    // Parse array suffixes [N] with C semantics: int[8][4] = array of 8 arrays of 4 ints
    vector<int> dims;
    while (match(TOK_LBRACKET)) {
        if (!expect(TOK_NUMBER)) {
            error = "expected array size";
            return nullptr;
        }
        dims.push_back((int)(long long)peek(-1).value);
        if (!expect(TOK_RBRACKET)) return nullptr;
    }
    for (auto it = dims.rbegin(); it != dims.rend(); ++it) {
        auto arr = make_shared<Type>();
        arr->kind = TYPE_ARRAY;
        arr->sub = t;
        arr->arraySize = *it;
        t = arr;
    }
    return t;
}

// ---- Expressions (precedence climbing) ----

shared_ptr<Expr> FrontParser::parseExpr(bool allowAssign) {
    if (allowAssign) return parseAssign();
    return parseTernary();
}

shared_ptr<Expr> FrontParser::parseAssign() {
    auto left = parseTernary();
    if (!left) return nullptr;
    string op;
    if (match(TOK_ASSIGN)) op = "=";
    else if (match(TOK_PLUS_ASSIGN)) op = "+=";
    else if (match(TOK_MINUS_ASSIGN)) op = "-=";
    else if (match(TOK_STAR_ASSIGN)) op = "*=";
    else if (match(TOK_SLASH_ASSIGN)) op = "/=";
    else if (match(TOK_PERCENT_ASSIGN)) op = "%=";
    else if (match(TOK_AND_ASSIGN)) op = "&=";
    else if (match(TOK_OR_ASSIGN)) op = "|=";
    else if (match(TOK_XOR_ASSIGN)) op = "^=";
    else if (match(TOK_SHL_ASSIGN)) op = "<<=";
    else if (match(TOK_SHR_ASSIGN)) op = ">>=";
    if (!op.empty()) {
        auto right = parseAssign();
        if (!right) return nullptr;
        if (op == "=") {
            return make_shared<AssignExpr>(left, right, CURRENT.line);
        }
        // Compound assignment: desugar to left = left op right
        string binOp = op.substr(0, op.size() - 1);
        auto bin = make_shared<BinaryExpr>(binOp, left, right, CURRENT.line);
        return make_shared<AssignExpr>(left, bin, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseTernary() {
    auto cond = parseOr();
    if (!cond) return nullptr;
    if (match(TOK_QUESTION)) {
        auto thenExpr = parseExpr();
        if (!thenExpr) return nullptr;
        if (!expect(TOK_COLON)) return nullptr;
        auto elseExpr = parseTernary();
        if (!elseExpr) return nullptr;
        return make_shared<TernaryExpr>(cond, thenExpr, elseExpr, CURRENT.line);
    }
    return cond;
}

shared_ptr<Expr> FrontParser::parseOr() {
    auto left = parseAnd();
    if (!left) return nullptr;
    while (match(TOK_OR)) {
        auto right = parseAnd();
        if (!right) return nullptr;
        left = make_shared<BinaryExpr>("||", left, right, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseAnd() {
    auto left = parseBitOr();
    if (!left) return nullptr;
    while (match(TOK_AND)) {
        auto right = parseBitOr();
        if (!right) return nullptr;
        left = make_shared<BinaryExpr>("&&", left, right, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseBitOr() {
    auto left = parseBitXor();
    if (!left) return nullptr;
    while (match(TOK_BITOR)) {
        auto right = parseBitXor();
        if (!right) return nullptr;
        left = make_shared<BinaryExpr>("|", left, right, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseBitXor() {
    auto left = parseBitAnd();
    if (!left) return nullptr;
    while (match(TOK_BITXOR)) {
        auto right = parseBitAnd();
        if (!right) return nullptr;
        left = make_shared<BinaryExpr>("^", left, right, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseBitAnd() {
    auto left = parseEquality();
    if (!left) return nullptr;
    while (match(TOK_BITAND)) {
        auto right = parseEquality();
        if (!right) return nullptr;
        left = make_shared<BinaryExpr>("&", left, right, CURRENT.line);
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseEquality() {
    auto left = parseRelational();
    if (!left) return nullptr;
    while (true) {
        if (match(TOK_EQ)) {
            auto right = parseRelational();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("==", left, right, CURRENT.line);
        } else if (match(TOK_NE)) {
            auto right = parseRelational();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("!=", left, right, CURRENT.line);
        } else break;
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseRelational() {
    auto left = parseShift();
    if (!left) return nullptr;
    while (true) {
        if (match(TOK_LT)) {
            auto right = parseShift();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("<", left, right, CURRENT.line);
        } else if (match(TOK_GT)) {
            auto right = parseShift();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>(">", left, right, CURRENT.line);
        } else if (match(TOK_LE)) {
            auto right = parseShift();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("<=", left, right, CURRENT.line);
        } else if (match(TOK_GE)) {
            auto right = parseShift();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>(">=", left, right, CURRENT.line);
        } else break;
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseShift() {
    auto left = parseAdditive();
    if (!left) return nullptr;
    while (true) {
        if (match(TOK_SHL)) {
            auto right = parseAdditive();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("<<", left, right, CURRENT.line);
        } else if (match(TOK_SHR)) {
            auto right = parseAdditive();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>(">>", left, right, CURRENT.line);
        } else break;
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseAdditive() {
    auto left = parseMultiplicative();
    if (!left) return nullptr;
    while (true) {
        if (match(TOK_PLUS)) {
            auto right = parseMultiplicative();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("+", left, right, CURRENT.line);
        } else if (match(TOK_MINUS)) {
            auto right = parseMultiplicative();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("-", left, right, CURRENT.line);
        } else break;
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseMultiplicative() {
    auto left = parseUnary();
    if (!left) return nullptr;
    while (true) {
        if (match(TOK_STAR)) {
            auto right = parseUnary();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("*", left, right, CURRENT.line);
        } else if (match(TOK_SLASH)) {
            auto right = parseUnary();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("/", left, right, CURRENT.line);
        } else if (match(TOK_PERCENT)) {
            auto right = parseUnary();
            if (!right) return nullptr;
            left = make_shared<BinaryExpr>("%", left, right, CURRENT.line);
        } else break;
    }
    return left;
}

shared_ptr<Expr> FrontParser::parseUnary() {
    if (match(TOK_MINUS)) {
        auto e = parseUnary();
        if (!e) return nullptr;
        return make_shared<UnaryExpr>("-", e, CURRENT.line);
    }
    if (match(TOK_NOT)) {
        auto e = parseUnary();
        if (!e) return nullptr;
        return make_shared<UnaryExpr>("!", e, CURRENT.line);
    }
    if (match(TOK_BITNOT)) {
        auto e = parseUnary();
        if (!e) return nullptr;
        return make_shared<UnaryExpr>("~", e, CURRENT.line);
    }
    if (match(TOK_INC)) {
        auto e = parseUnary();
        if (!e) return nullptr;
        auto one = make_shared<NumberExpr>(Int128(1), e->line);
        auto add = make_shared<BinaryExpr>("+", e, one, e->line);
        return make_shared<AssignExpr>(e, add, e->line);
    }
    if (match(TOK_DEC)) {
        auto e = parseUnary();
        if (!e) return nullptr;
        auto one = make_shared<NumberExpr>(Int128(1), e->line);
        auto sub = make_shared<BinaryExpr>("-", e, one, e->line);
        return make_shared<AssignExpr>(e, sub, e->line);
    }
    if (match(TOK_BITAND)) {
        bool mut_ = false;
        if (match(TOK_KW_MUT)) mut_ = true;
        auto e = parseUnary();
        if (!e) return nullptr;
        return make_shared<BorrowExpr>(e, mut_, CURRENT.line);
    }
    if (match(TOK_KW_SIZEOF)) {
        if (match(TOK_LPAREN)) {
            if (isTypeToken()) {
                auto t = parseType();
                if (!t) return nullptr;
                if (!expect(TOK_RPAREN)) return nullptr;
                return make_shared<SizeofExpr>(t, nullptr, CURRENT.line);
            } else {
                auto e = parseExpr();
                if (!e) return nullptr;
                if (!expect(TOK_RPAREN)) return nullptr;
                return make_shared<SizeofExpr>(nullptr, e, CURRENT.line);
            }
        } else {
            auto e = parseUnary();
            if (!e) return nullptr;
            return make_shared<SizeofExpr>(nullptr, e, CURRENT.line);
        }
    }
    if (match(TOK_KW_OFFSETOF)) {
        if (!expect(TOK_LPAREN)) return nullptr;
        auto t = parseType();
        if (!t) return nullptr;
        if (!expect(TOK_COMMA)) return nullptr;
        if (!expect(TOK_IDENT)) return nullptr;
        string field = peek(-1).text;
        if (!expect(TOK_RPAREN)) return nullptr;
        return make_shared<OffsetofExpr>(t, field, CURRENT.line);
    }
    return parsePostfix();
}

shared_ptr<Expr> FrontParser::parsePostfix() {
    auto expr = parsePrimary();
    if (!expr) return nullptr;
    while (true) {
        if (match(TOK_LPAREN)) {
            vector<shared_ptr<Expr>> args;
            if (!check(TOK_RPAREN)) {
                do {
                    auto arg = parseExpr();
                    if (!arg) return nullptr;
                    args.push_back(arg);
                } while (match(TOK_COMMA));
            }
            if (!expect(TOK_RPAREN)) return nullptr;
            expr = make_shared<CallExpr>(expr, args, CURRENT.line);
        } else if (match(TOK_LBRACKET)) {
            auto idx = parseExpr();
            if (!idx) return nullptr;
            if (!expect(TOK_RBRACKET)) return nullptr;
            expr = make_shared<IndexExpr>(expr, idx, CURRENT.line);
        } else if (match(TOK_ARROW)) {
            if (!expect(TOK_IDENT)) return nullptr;
            expr = make_shared<FieldExpr>(expr, peek(-1).text, true, CURRENT.line);
        } else if (match(TOK_DOT)) {
            if (!expect(TOK_IDENT)) return nullptr;
            expr = make_shared<FieldExpr>(expr, peek(-1).text, false, CURRENT.line);
        } else break;
    }
    return expr;
}

shared_ptr<Expr> FrontParser::parsePrimary() {
    if (match(TOK_NUMBER)) {
        return make_shared<NumberExpr>(peek(-1).value, peek(-1).line, peek(-1).col);
    }
    if (match(TOK_STRING)) {
        return make_shared<StringExpr>(peek(-1).text, peek(-1).line, peek(-1).col);
    }
    if (match(TOK_CHAR)) {
        return make_shared<CharExpr>((char)(long long)peek(-1).value, peek(-1).line, peek(-1).col);
    }
    if (match(TOK_KW_TRUE)) {
        return make_shared<BoolExpr>(true, peek(-1).line, peek(-1).col);
    }
    if (match(TOK_KW_FALSE)) {
        return make_shared<BoolExpr>(false, peek(-1).line, peek(-1).col);
    }
    if (match(TOK_IDENT)) {
        return make_shared<IdentExpr>(peek(-1).text, peek(-1).line, peek(-1).col);
    }
    if (check(TOK_LBRACE)) {
        int line = CURRENT.line;
        advance(); // consume '{'
        // Check if it's a struct literal: { field: value, ... }
        if (!check(TOK_RBRACE) && check(TOK_IDENT)) {
            auto state = saveState();
            advance(); // consume ident
            if (check(TOK_COLON)) {
                restoreState(state);
                // Struct literal
                vector<pair<string, shared_ptr<Expr>>> fields;
                while (true) {
                    if (!expect(TOK_IDENT)) return nullptr;
                    string fname = peek(-1).text;
                    if (!expect(TOK_COLON)) return nullptr;
                    auto val = parseExpr();
                    if (!val) return nullptr;
                    fields.push_back({fname, val});
                    if (check(TOK_RBRACE)) break;
                    if (!expect(TOK_COMMA)) return nullptr;
                }
                if (!expect(TOK_RBRACE)) return nullptr;
                return make_shared<StructLiteralExpr>("", fields, line);
            }
            restoreState(state);
        }
        // Array literal
        vector<shared_ptr<Expr>> elements;
        if (!check(TOK_RBRACE)) {
            while (true) {
                auto e = parseExpr();
                if (!e) return nullptr;
                elements.push_back(e);
                if (check(TOK_RBRACE)) break;
                if (!expect(TOK_COMMA)) return nullptr;
            }
        }
        if (!expect(TOK_RBRACE)) return nullptr;
        return make_shared<ArrayLiteralExpr>(elements, line);
    }
    if (check(TOK_LPAREN)) {
        // Check for cast: (type) expr
        auto state = saveState();
        advance(); // consume '('
        auto t = parseType();
        if (t && match(TOK_RPAREN)) {
            auto e = parseUnary();
            if (e) return make_shared<CastExpr>(t, e, CURRENT.line);
        }
        restoreState(state);
        // Grouped expression
        advance(); // consume '('
        auto e = parseExpr();
        if (!e) return nullptr;
        if (!expect(TOK_RPAREN)) return nullptr;
        return e;
    }
    errorLine = CURRENT.line;
    errorCol = CURRENT.col;
    error = "unexpected token `" + CURRENT.text + "` in expression";
    return nullptr;
}

// ---- Statements ----

shared_ptr<Stmt> FrontParser::parseStmt() {
    if (check(TOK_LBRACE)) return parseBlock();
    if (check(TOK_KW_VAR)) return parseVarDecl();
    if (check(TOK_KW_CONST)) return parseConstDecl();
    if (check(TOK_KW_IF)) return parseIfStmt();
    if (check(TOK_KW_WHILE)) return parseWhileStmt();
    if (check(TOK_KW_DO)) return parseDoWhileStmt();
    if (check(TOK_KW_FOR)) return parseForStmt();
    if (check(TOK_KW_RETURN)) return parseReturnStmt();
    if (check(TOK_KW_ASM)) return parseAsmStmt();
    if (check(TOK_KW_BREAK)) return parseBreakStmt();
    if (check(TOK_KW_CONTINUE)) return parseContinueStmt();
    if (check(TOK_KW_SWITCH)) return parseSwitchStmt();
    auto e = parseExpr();
    if (!e) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<ExprStmt>(e, e->line);
}

shared_ptr<BlockStmt> FrontParser::parseBlock() {
    int line = CURRENT.line;
    int col = CURRENT.col;
    if (!expect(TOK_LBRACE)) return nullptr;
    auto block = make_shared<BlockStmt>(line, col);
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        auto s = parseStmt();
        if (!s) {
            if (panicMode) {
                synchronize();
            } else {
                return nullptr;
            }
        } else {
            block->stmts.push_back(s);
        }
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    return block;
}

shared_ptr<Stmt> FrontParser::parseVarDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_VAR)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    shared_ptr<Type> t = nullptr;
    if (match(TOK_COLON)) {
        t = parseType();
        if (!t) return nullptr;
    }
    shared_ptr<Expr> init = nullptr;
    if (match(TOK_ASSIGN)) {
        init = parseExpr();
        if (!init) return nullptr;
    } else {
        errorLine = CURRENT.line;
        errorCol = CURRENT.col;
        error = "variable declaration requires an initializer";
        reportError(error);
        return nullptr;
    }
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<VarStmt>(name, t, init, line);
}

shared_ptr<Stmt> FrontParser::parseConstDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_CONST)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_ASSIGN)) return nullptr;
    auto val = parseExpr();
    if (!val) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<ConstStmt>(name, val, line);
}

shared_ptr<Stmt> FrontParser::parseIfStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_IF)) return nullptr;
    if (!expect(TOK_LPAREN)) return nullptr;
    auto cond = parseExpr();
    if (!cond) return nullptr;
    if (!expect(TOK_RPAREN)) return nullptr;
    auto thenB = parseStmt();
    if (!thenB) return nullptr;
    shared_ptr<Stmt> elseB = nullptr;
    if (match(TOK_KW_ELSE)) {
        elseB = parseStmt();
        if (!elseB) return nullptr;
    }
    return make_shared<IfStmt>(cond, thenB, elseB, line);
}

shared_ptr<Stmt> FrontParser::parseWhileStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_WHILE)) return nullptr;
    if (!expect(TOK_LPAREN)) return nullptr;
    auto cond = parseExpr();
    if (!cond) return nullptr;
    if (!expect(TOK_RPAREN)) return nullptr;
    auto body = parseStmt();
    if (!body) return nullptr;
    return make_shared<WhileStmt>(cond, body, line);
}

shared_ptr<Stmt> FrontParser::parseForStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_FOR)) return nullptr;
    if (!expect(TOK_LPAREN)) return nullptr;
    shared_ptr<Stmt> init = nullptr;
    if (!check(TOK_SEMI)) {
        if (check(TOK_KW_VAR)) init = parseVarDecl();
        else {
            auto e = parseExpr();
            if (!e) return nullptr;
            init = make_shared<ExprStmt>(e, e->line);
            if (!expect(TOK_SEMI)) return nullptr;
        }
    } else {
        expect(TOK_SEMI);
    }
    shared_ptr<Expr> cond = nullptr;
    if (!check(TOK_SEMI)) {
        cond = parseExpr();
        if (!cond) return nullptr;
    }
    if (!expect(TOK_SEMI)) return nullptr;
    shared_ptr<Expr> step = nullptr;
    if (!check(TOK_RPAREN)) {
        step = parseExpr();
        if (!step) return nullptr;
    }
    if (!expect(TOK_RPAREN)) return nullptr;
    auto body = parseStmt();
    if (!body) return nullptr;
    return make_shared<ForStmt>(init, cond, step, body, line);
}

shared_ptr<Stmt> FrontParser::parseReturnStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_RETURN)) return nullptr;
    shared_ptr<Expr> val = nullptr;
    if (!check(TOK_SEMI)) {
        val = parseExpr();
        if (!val) return nullptr;
    }
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<ReturnStmt>(val, line);
}

shared_ptr<Stmt> FrontParser::parseAsmStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_ASM)) return nullptr;
    if (!expect(TOK_LBRACE)) return nullptr;
    vector<string> instructions;
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (match(TOK_STRING)) {
            instructions.push_back(peek(-1).text);
        } else {
            errorLine = CURRENT.line;
            errorCol = CURRENT.col;
            error = "expected string literal in asm block";
            return nullptr;
        }
        if (match(TOK_SEMI)) {
            // optional semicolon after each instruction
        }
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    expect(TOK_SEMI); // optional trailing semicolon
    return make_shared<InlineAsmStmt>(instructions, line);
}

shared_ptr<Stmt> FrontParser::parseBreakStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_BREAK)) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<BreakStmt>(line);
}

shared_ptr<Stmt> FrontParser::parseContinueStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_CONTINUE)) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<ContinueStmt>(line);
}

shared_ptr<Stmt> FrontParser::parseSwitchStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_SWITCH)) return nullptr;
    if (!expect(TOK_LPAREN)) return nullptr;
    auto e = parseExpr();
    if (!e) return nullptr;
    if (!expect(TOK_RPAREN)) return nullptr;
    if (!expect(TOK_LBRACE)) return nullptr;
    vector<CaseClause> cases;
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (match(TOK_KW_CASE)) {
            auto val = parseExpr();
            if (!val) return nullptr;
            if (!expect(TOK_COLON)) return nullptr;
            auto body = parseStmt();
            if (!body) return nullptr;
            cases.push_back(CaseClause(val, body));
        } else if (match(TOK_KW_DEFAULT)) {
            if (!expect(TOK_COLON)) return nullptr;
            auto body = parseStmt();
            if (!body) return nullptr;
            cases.push_back(CaseClause(nullptr, body));
        } else {
            errorLine = CURRENT.line;
            errorCol = CURRENT.col;
            error = "expected `case` or `default` in switch statement";
            return nullptr;
        }
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    return make_shared<SwitchStmt>(e, cases, line);
}

shared_ptr<Stmt> FrontParser::parseDoWhileStmt() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_DO)) return nullptr;
    auto body = parseStmt();
    if (!body) return nullptr;
    if (!expect(TOK_KW_WHILE)) return nullptr;
    if (!expect(TOK_LPAREN)) return nullptr;
    auto cond = parseExpr();
    if (!cond) return nullptr;
    if (!expect(TOK_RPAREN)) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<DoWhileStmt>(cond, body, line);
}

// ---- Declarations ----

shared_ptr<Decl> FrontParser::parseDecl() {
    if (check(TOK_KW_IMPORT)) {
        int line = CURRENT.line;
        advance();
        if (!expect(TOK_STRING)) return nullptr;
        if (!expect(TOK_SEMI)) return nullptr;
        return make_shared<ImportDecl>(peek(-2).text, line);
    }
    if (check(TOK_KW_SYSCALL)) return parseSyscallDecl();
    if (check(TOK_KW_STRUCT)) return parseStructDecl();
    if (check(TOK_KW_UNION)) return parseUnionDecl();
    if (check(TOK_KW_ENUM)) return parseEnumDecl();
    if (check(TOK_KW_TYPEDEF)) return parseTypedefDecl();
    if (check(TOK_KW_FUNC)) return parseFuncDecl();
    if (check(TOK_KW_VAR)) {
        auto s = parseVarDecl();
        if (!s) return nullptr;
        auto vs = dynamic_pointer_cast<VarStmt>(s);
        return make_shared<GlobalVarDecl>(vs->name, vs->varType, vs->init, vs->line);
    }
    if (check(TOK_KW_CONST)) {
        auto s = parseConstDecl();
        if (!s) return nullptr;
        auto cs = dynamic_pointer_cast<ConstStmt>(s);
        return make_shared<GlobalConstDecl>(cs->name, cs->value, cs->line);
    }
    errorLine = CURRENT.line;
    errorCol = CURRENT.col;
    error = "expected a declaration, but found `" + CURRENT.text + "`";
    return nullptr;
}

shared_ptr<SyscallDecl> FrontParser::parseSyscallDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_SYSCALL)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_COMMA)) return nullptr;
    if (!expect(TOK_NUMBER)) return nullptr;
    int cmdId = (int)(long long)peek(-1).value;
    if (!expect(TOK_COMMA)) return nullptr;
    if (!expect(TOK_NUMBER)) return nullptr;
    int argCount = (int)(long long)peek(-1).value;
    if (!expect(TOK_SEMI)) return nullptr;
    if (argCount < 0 || argCount > 3) {
        errorLine = line;
        errorCol = CURRENT.col;
        error = "syscall arg_count must be 0..3";
        return nullptr;
    }
    return make_shared<SyscallDecl>(name, cmdId, argCount, line);
}

shared_ptr<FuncDecl> FrontParser::parseFuncDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_FUNC)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_LPAREN)) return nullptr;
    vector<Param> params;
    if (!check(TOK_RPAREN)) {
        do {
            if (!expect(TOK_IDENT)) return nullptr;
            string pname = peek(-1).text;
            shared_ptr<Type> ptype = nullptr;
            if (!expect(TOK_COLON)) return nullptr;
            ptype = parseType();
            if (!ptype) return nullptr;
            params.push_back({pname, ptype});
        } while (match(TOK_COMMA));
    }
    if (!expect(TOK_RPAREN)) return nullptr;
    shared_ptr<Type> retType = nullptr;
    if (!expect(TOK_COLON)) return nullptr;
    retType = parseType();
    if (!retType) return nullptr;
    auto body = parseBlock();
    if (!body) return nullptr;
    return make_shared<FuncDecl>(name, retType, params, body, line);
}

shared_ptr<StructDecl> FrontParser::parseStructDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_STRUCT)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_LBRACE)) return nullptr;
    vector<StructField> fields;
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (!expect(TOK_IDENT)) return nullptr;
        string fname = peek(-1).text;
        if (!expect(TOK_COLON)) return nullptr;
        auto ftype = parseType();
        if (!ftype) return nullptr;
        fields.push_back({fname, ftype});
        if (!expect(TOK_SEMI)) return nullptr;
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    expect(TOK_SEMI);
    return make_shared<StructDecl>(name, fields, line);
}

shared_ptr<EnumDecl> FrontParser::parseEnumDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_ENUM)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_LBRACE)) return nullptr;
    vector<pair<string, shared_ptr<Expr>>> members;
    int nextValue = 0;
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (!expect(TOK_IDENT)) return nullptr;
        string mname = peek(-1).text;
        shared_ptr<Expr> mval = nullptr;
        if (check(TOK_ASSIGN)) {
            advance();
            mval = parseExpr();
            if (!mval) return nullptr;
        }
        members.push_back({mname, mval});
        if (check(TOK_COMMA)) advance();
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    expect(TOK_SEMI);
    return make_shared<EnumDecl>(name, members, line);
}

shared_ptr<UnionDecl> FrontParser::parseUnionDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_UNION)) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_LBRACE)) return nullptr;
    vector<StructField> fields;
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (!expect(TOK_IDENT)) return nullptr;
        string fname = peek(-1).text;
        if (!expect(TOK_COLON)) return nullptr;
        auto ftype = parseType();
        if (!ftype) return nullptr;
        fields.push_back({fname, ftype});
        if (!expect(TOK_SEMI)) return nullptr;
    }
    if (!expect(TOK_RBRACE)) return nullptr;
    expect(TOK_SEMI);
    return make_shared<UnionDecl>(name, fields, line);
}

shared_ptr<TypedefDecl> FrontParser::parseTypedefDecl() {
    int line = CURRENT.line;
    if (!expect(TOK_KW_TYPEDEF)) return nullptr;
    auto t = parseType();
    if (!t) return nullptr;
    if (!expect(TOK_IDENT)) return nullptr;
    string name = peek(-1).text;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<TypedefDecl>(name, t, line);
}

void FrontParser::reportError(const string &msg) {
    errors.push_back(msg);
    if (error.empty()) {
        error = msg;
        errorLine = CURRENT.line;
        errorCol = CURRENT.col;
    }
    panicMode = true;
}

bool FrontParser::atDeclStart() const {
    return check(TOK_KW_FUNC) || check(TOK_KW_VAR) || check(TOK_KW_CONST) ||
           check(TOK_KW_STRUCT) || check(TOK_KW_UNION) || check(TOK_KW_ENUM) ||
           check(TOK_KW_TYPEDEF) || check(TOK_KW_IMPORT) || check(TOK_KW_SYSCALL);
}

bool FrontParser::atStmtStart() const {
    return check(TOK_LBRACE) || check(TOK_KW_VAR) || check(TOK_KW_CONST) ||
           check(TOK_KW_IF) || check(TOK_KW_WHILE) || check(TOK_KW_DO) ||
           check(TOK_KW_FOR) || check(TOK_KW_RETURN) || check(TOK_KW_ASM) ||
           check(TOK_KW_BREAK) || check(TOK_KW_CONTINUE) || check(TOK_KW_SWITCH) ||
           check(TOK_IDENT) || check(TOK_NUMBER) || check(TOK_STRING) ||
           check(TOK_CHAR) || check(TOK_LPAREN) || check(TOK_LBRACE) ||
           check(TOK_KW_TRUE) || check(TOK_KW_FALSE) || check(TOK_MINUS) ||
           check(TOK_NOT) || check(TOK_BITNOT) || check(TOK_KW_SIZEOF);
}

void FrontParser::synchronize() {
    panicMode = false;
    while (!check(TOK_EOF)) {
        if (check(TOK_SEMI)) { advance(); return; }
        if (check(TOK_RBRACE)) return;
        if (atDeclStart()) return;
        if (atStmtStart()) return;
        advance();
    }
}

bool FrontParser::parse(const vector<Token> &toks) {
    tokens = toks;
    pos = 0;
    error.clear();
    errors.clear();
    panicMode = false;
    program = make_shared<Program>();
    while (!check(TOK_EOF)) {
        auto d = parseDecl();
        if (!d) {
            if (panicMode) {
                synchronize();
            } else {
                return false;
            }
        } else {
            program->decls.push_back(d);
        }
    }
    return errors.empty();
}

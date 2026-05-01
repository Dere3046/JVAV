#include "parser.hpp"
using namespace std;

#define CURRENT (peek(0))

const Token& FrontParser::peek(int ahead) {
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
        "func", "var", "const", "if", "else", "while", "for", "return",
        "int", "char", "bool", "void", "ptr", "array",
        "true", "false", "import", "syscall", "mut",
        "struct", "union", "byte", "uint", "sizeof", "offsetof",
        "volatile", "asm",
        "+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=",
        "&&", "||", "&", "|", "^", "~", "<<", ">>", "!",
        "(", ")", "[", "]", "{", "}", ",", ";", ":", ".", "->"
    };
    const char* got = (t >= 0 && t <= TOK_ARROW) ? names[t] : "?";
    errorLine = CURRENT.line;
    errorCol = CURRENT.col;
    error = "expected `" + string(got) + "`, but found `" + CURRENT.text + "`";
    return false;
}

bool FrontParser::match(TokenType t) {
    if (peek().type == t) { advance(); return true; }
    return false;
}

bool FrontParser::check(TokenType t) {
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

shared_ptr<Expr> FrontParser::parseExpr() {
    return parseAssign();
}

shared_ptr<Expr> FrontParser::parseAssign() {
    auto left = parseOr();
    if (!left) return nullptr;
    if (match(TOK_ASSIGN)) {
        auto right = parseAssign();
        if (!right) return nullptr;
        return make_shared<AssignExpr>(left, right, CURRENT.line);
    }
    return left;
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
        return make_shared<NumberExpr>(peek(-1).value, peek(-1).line);
    }
    if (match(TOK_STRING)) {
        return make_shared<StringExpr>(peek(-1).text, peek(-1).line);
    }
    if (match(TOK_CHAR)) {
        return make_shared<CharExpr>((char)(long long)peek(-1).value, peek(-1).line);
    }
    if (match(TOK_KW_TRUE)) {
        return make_shared<BoolExpr>(true, peek(-1).line);
    }
    if (match(TOK_KW_FALSE)) {
        return make_shared<BoolExpr>(false, peek(-1).line);
    }
    if (match(TOK_IDENT)) {
        return make_shared<IdentExpr>(peek(-1).text, peek(-1).line);
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
    if (check(TOK_KW_FOR)) return parseForStmt();
    if (check(TOK_KW_RETURN)) return parseReturnStmt();
    if (check(TOK_KW_ASM)) return parseAsmStmt();
    auto e = parseExpr();
    if (!e) return nullptr;
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<ExprStmt>(e, e->line);
}

shared_ptr<BlockStmt> FrontParser::parseBlock() {
    int line = CURRENT.line;
    if (!expect(TOK_LBRACE)) return nullptr;
    auto block = make_shared<BlockStmt>(line);
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        auto s = parseStmt();
        if (!s) return nullptr;
        block->stmts.push_back(s);
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
    if (!expect(TOK_SEMI)) return nullptr;
    return make_shared<InlineAsmStmt>(instructions, line);
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
            if (match(TOK_COLON)) {
                ptype = parseType();
                if (!ptype) return nullptr;
            }
            params.push_back({pname, ptype});
        } while (match(TOK_COMMA));
    }
    if (!expect(TOK_RPAREN)) return nullptr;
    shared_ptr<Type> retType = nullptr;
    if (match(TOK_COLON)) {
        retType = parseType();
        if (!retType) return nullptr;
    }
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

bool FrontParser::parse(const vector<Token> &toks) {
    tokens = toks;
    pos = 0;
    error.clear();
    program = make_shared<Program>();
    while (!check(TOK_EOF)) {
        auto d = parseDecl();
        if (!d) return false;
        program->decls.push_back(d);
    }
    return true;
}

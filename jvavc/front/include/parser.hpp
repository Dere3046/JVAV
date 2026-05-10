#ifndef PARSER_HPP
#define PARSER_HPP
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <string>

class FrontParser {
public:
    bool parse(const std::vector<Token> &toks);
    std::shared_ptr<Program> getProgram() { return program; }
    const std::string& getError() const { return error; }
    const std::vector<std::string>& getErrors() const { return errors; }
    int getErrorLine() const { return errorLine; }
    int getErrorCol() const { return errorCol; }
private:
    std::vector<Token> tokens;
    size_t pos;
    std::string error;
    std::vector<std::string> errors;
    int errorLine = 0;
    int errorCol = 0;
    bool panicMode = false;
    std::shared_ptr<Program> program;

    const Token& peek(int ahead = 0) const;
    const Token& advance();
    bool expect(TokenType t);
    bool match(TokenType t);
    bool check(TokenType t) const;
    bool isTypeToken();

    std::shared_ptr<Type> parseType();
    std::shared_ptr<Expr> parseExpr(bool allowAssign = true);
    std::shared_ptr<Expr> parseAssign();
    std::shared_ptr<Expr> parseTernary();
    std::shared_ptr<Expr> parseOr();
    std::shared_ptr<Expr> parseAnd();
    std::shared_ptr<Expr> parseBitOr();
    std::shared_ptr<Expr> parseBitXor();
    std::shared_ptr<Expr> parseBitAnd();
    std::shared_ptr<Expr> parseEquality();
    std::shared_ptr<Expr> parseRelational();
    std::shared_ptr<Expr> parseShift();
    std::shared_ptr<Expr> parseAdditive();
    std::shared_ptr<Expr> parseMultiplicative();
    std::shared_ptr<Expr> parseUnary();
    std::shared_ptr<Expr> parsePostfix();
    std::shared_ptr<Expr> parsePrimary();

    std::shared_ptr<Stmt> parseStmt();
    std::shared_ptr<BlockStmt> parseBlock();
    std::shared_ptr<Stmt> parseVarDecl();
    std::shared_ptr<Stmt> parseConstDecl();
    std::shared_ptr<Stmt> parseIfStmt();
    std::shared_ptr<Stmt> parseWhileStmt();
    std::shared_ptr<Stmt> parseDoWhileStmt();
    std::shared_ptr<Stmt> parseForStmt();
    std::shared_ptr<Stmt> parseReturnStmt();
    std::shared_ptr<Stmt> parseBreakStmt();
    std::shared_ptr<Stmt> parseContinueStmt();
    std::shared_ptr<Stmt> parseSwitchStmt();

    std::shared_ptr<Decl> parseDecl();
    std::shared_ptr<SyscallDecl> parseSyscallDecl();
    std::shared_ptr<FuncDecl> parseFuncDecl();
    std::shared_ptr<StructDecl> parseStructDecl();
    std::shared_ptr<UnionDecl> parseUnionDecl();
    std::shared_ptr<EnumDecl> parseEnumDecl();
    std::shared_ptr<TypedefDecl> parseTypedefDecl();
    std::shared_ptr<Stmt> parseAsmStmt();

    struct ParserState {
        size_t pos;
        std::string error;
        int errorLine, errorCol;
    };
    ParserState saveState();
    void restoreState(const ParserState &s);
    void synchronize();
    void reportError(const std::string &msg);
    bool atDeclStart() const;
    bool atStmtStart() const;
};

#endif

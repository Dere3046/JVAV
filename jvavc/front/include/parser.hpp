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
    int getErrorLine() const { return errorLine; }
    int getErrorCol() const { return errorCol; }
private:
    std::vector<Token> tokens;
    size_t pos;
    std::string error;
    int errorLine = 0;
    int errorCol = 0;
    std::shared_ptr<Program> program;

    const Token& peek(int ahead = 0);
    const Token& advance();
    bool expect(TokenType t);
    bool match(TokenType t);
    bool check(TokenType t);
    bool isTypeToken();

    std::shared_ptr<Type> parseType();
    std::shared_ptr<Expr> parseExpr();
    std::shared_ptr<Expr> parseAssign();
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
    std::shared_ptr<Stmt> parseForStmt();
    std::shared_ptr<Stmt> parseReturnStmt();

    std::shared_ptr<Decl> parseDecl();
    std::shared_ptr<SyscallDecl> parseSyscallDecl();
    std::shared_ptr<FuncDecl> parseFuncDecl();
};

#endif

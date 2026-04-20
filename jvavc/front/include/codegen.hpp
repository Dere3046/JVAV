#ifndef CODEGEN_HPP
#define CODEGEN_HPP
#include "ast.hpp"
#include <string>
#include <sstream>
#include <map>
#include <set>

class CodeGenerator {
public:
    std::string generate(std::shared_ptr<Program> prog);
    std::string generate(std::shared_ptr<Program> prog, const std::string &basePath);
    void setBasePath(const std::string &path) { basePath = path; }
private:
    std::stringstream out;
    int labelCounter = 0;
    std::map<std::string, int> localOffsets;
    int localSize = 0;
    std::string curFuncRetLabel;
    std::vector<std::string> stringLabels;
    int stringCounter = 0;
    std::string basePath;
    std::set<std::string> generatedFiles;

    std::string nextLabel(const std::string &prefix);
    void emit(const std::string &s);

    void collectLocals(std::shared_ptr<Stmt> s);
    void genFuncDecl(std::shared_ptr<FuncDecl> d);
    void genStmt(std::shared_ptr<Stmt> s);
    void genExpr(std::shared_ptr<Expr> e, int destReg);
    void loadVar(const std::string &name, int reg);
    void storeVar(const std::string &name, int reg);
    void genCondJump(std::shared_ptr<Expr> e, const std::string &falseLabel);
    void genProgram(std::shared_ptr<Program> prog);
};

#endif

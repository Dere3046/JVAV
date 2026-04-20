#ifndef SEMA_HPP
#define SEMA_HPP
#include "ast.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

struct Symbol {
    enum Kind { SYM_VAR, SYM_FUNC, SYM_CONST };
    Kind kind;
    std::shared_ptr<Type> type;
    int scopeLevel;
    bool isGlobal;
};

class Sema {
public:
    bool analyze(std::shared_ptr<Program> prog);
    bool analyze(std::shared_ptr<Program> prog, const std::string &basePath);
    const std::string& getError() const { return error; }
    void setBasePath(const std::string &path) { basePath = path; }
private:
    std::string error;
    std::string basePath;
    int scopeLevel;
    std::vector<std::map<std::string, Symbol>> scopes;
    std::set<std::string> importedFiles;

    void enterScope();
    void exitScope();
    bool declare(const std::string &name, Symbol::Kind k, std::shared_ptr<Type> type, int level, bool global);
    Symbol* lookup(const std::string &name);
    bool checkDecl(std::shared_ptr<Decl> d);
    bool checkStmt(std::shared_ptr<Stmt> s);
    bool checkExpr(std::shared_ptr<Expr> e);
};

#endif

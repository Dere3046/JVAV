#ifndef SEMA_HPP
#define SEMA_HPP
#include "ast.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

enum SemaLevel { SEM_ERROR, SEM_WARNING };

struct SemaError {
    SemaLevel level;
    std::string msg;
    std::string file;
    int line;
    int col = 1;
    std::string hint;     // suggested fix
};

struct Symbol {
    enum Kind { SYM_VAR, SYM_FUNC, SYM_CONST };
    Kind kind;
    std::shared_ptr<Type> type;
    int scopeLevel;
    bool isGlobal;
    bool isCopy;          // true for int/char/bool
    bool initialized;     // has been assigned
    bool moved;           // value has been moved out
    int borrowCount;      // number of immutable borrows
    bool mutBorrowed;     // has a mutable borrow
    bool used;            // has been read/used
};

struct VarState {
    bool initialized = false;
    bool moved = false;
    int borrowCount = 0;
    bool mutBorrowed = false;
    bool used = false;
};

class Sema {
public:
    bool analyze(std::shared_ptr<Program> prog);
    bool analyze(std::shared_ptr<Program> prog, const std::string &basePath);
    const std::string& getError() const { return firstError; }
    const std::vector<SemaError>& getErrors() const { return errors; }
    bool hasErrors() const;
    void printErrors(std::ostream &os) const;
    void setBasePath(const std::string &path) { basePath = path; }
    void setImportPaths(const std::vector<std::string>& paths) { importPaths = paths; }
    void setCurrentFile(const std::string &f) { currentFile = f; }
private:
    std::string firstError;
    std::vector<SemaError> errors;
    std::string basePath;
    std::vector<std::string> importPaths;
    std::string currentFile;
    int scopeLevel;
    std::vector<std::map<std::string, Symbol>> scopes;
    std::set<std::string> importedFiles;
    std::shared_ptr<Type> curRetType;   // return type of current function
    bool inLoop;                         // are we inside a loop?

    void report(SemaLevel level, const std::string &msg, int line, int col = 1, const std::string &hint = "");
    void enterScope();
    void exitScope();
    bool declare(const std::string &name, Symbol::Kind k, std::shared_ptr<Type> type, int level, bool global);
    Symbol* lookup(const std::string &name);

    // Ownership helpers (MimiWorld)
    static bool isCopyType(std::shared_ptr<Type> t);
    void markInitialized(const std::string &name);
    void markMoved(const std::string &name, int line);
    void markUsed(const std::string &name);
    bool checkUsable(const std::string &name, int line);
    bool checkBorrow(const std::string &name, bool mut_, int line);

    // Checkers
    bool checkDecl(std::shared_ptr<Decl> d);
    bool checkStmt(std::shared_ptr<Stmt> s);
    bool checkExpr(std::shared_ptr<Expr> e);
    bool checkAssignTarget(std::shared_ptr<Expr> e);

    bool typeEqual(std::shared_ptr<Type> a, std::shared_ptr<Type> b);
    bool typeCompatible(std::shared_ptr<Type> dst, std::shared_ptr<Type> src);
    std::string typeStr(std::shared_ptr<Type> t);

    // Import handling
    bool processImport(std::shared_ptr<ImportDecl> id);

    // Unused variable check at scope exit
    void checkUnusedInScope(int level);
};

#endif

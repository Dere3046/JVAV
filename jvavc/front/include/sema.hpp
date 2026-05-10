#ifndef SEMA_HPP
#define SEMA_HPP
#include "ast.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

enum SemaLevel { SEM_ERROR, SEM_WARNING };

enum ErrorCode {
    E_REDECLARATION = 1,
    E_UNKNOWN_TYPE = 2,
    E_MOVE_FROM_BORROWED = 3,
    E_MOVE_FROM_MUT_BORROWED = 4,
    E_UNINITIALIZED = 5,
    E_USE_AFTER_MOVE = 6,
    E_USE_WHILE_MUT_BORROWED = 7,
    E_BORROW_UNINIT = 8,
    E_BORROW_MOVED = 9,
    E_MUT_BORROW_CONFLICT = 10,
    E_MUT_BORROW_DOUBLE = 11,
    E_BORROW_WHILE_MUT = 12,
    W_UNUSED_VAR = 13,
    E_VOID_PARAM = 14,
    E_MISSING_RETURN = 15,
    E_VOID_FIELD = 16,
    E_IMPORT_NOT_FOUND = 17,
    E_IMPORT_PARSE = 18,
    E_IMPORT_SEMA = 19,
    E_RETURN_IN_VOID = 20,
    E_MISSING_RETURN_VALUE = 21,
    E_BREAK_OUTSIDE_LOOP = 22,
    E_CONTINUE_OUTSIDE_LOOP = 23,
    E_UNDEFINED_VAR = 24,
    E_FUNC_AS_VALUE = 25,
    E_INVALID_STRING_ARITH = 26,
    E_INVALID_NUMERIC_OP = 27,
    E_INVALID_BITWISE_OP = 28,
    E_INVALID_LOGICAL_OP = 29,
    E_INVALID_STRING_CMP = 30,
    E_INVALID_UNARY_OP = 31,
    E_INVALID_LOGICAL_NOT = 32,
    E_UNDEFINED_FUNC = 33,
    E_NOT_A_FUNCTION = 34,
    E_BORROW_TARGET = 35,
    E_UNKNOWN_STRUCT_TYPE = 36,
    E_UNKNOWN_UNION_TYPE = 37,
    E_STRUCT_NO_FIELD = 38,
    E_UNION_NO_FIELD = 39,
    E_OFFSEOF_TYPE = 40,
    E_INVALID_ASSIGN_TARGET = 41,
    E_FIELD_UNKNOWN_TYPE = 42,
    E_FIELD_NOT_STRUCT = 43,
    E_FIELD_NOT_FOUND = 44,
    E_GENERIC = 99
};

struct SemaError {
    SemaLevel level;
    int code;
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
    bool borrowsArgs;     // function borrows (does not move) non-copy arguments
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
    std::shared_ptr<Program> program;   // current program being analyzed
    std::shared_ptr<Type> curRetType;   // return type of current function
    bool inLoop;                         // are we inside a loop?
    bool inSwitch;                       // are we inside a switch?
    bool allowAssign;                    // is assignment allowed in current context?
    std::map<std::string, std::shared_ptr<Type>> typedefs;
    std::map<std::string, std::vector<std::shared_ptr<Type>>> builtinParams;

    void report(SemaLevel level, const std::string &msg, int line, int col = 1, const std::string &hint = "", int code = E_GENERIC);
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
    bool typeCompatible(std::shared_ptr<Type> dst, std::shared_ptr<Type> src, bool isLiteral = false);
    std::string typeStr(std::shared_ptr<Type> t);
    std::shared_ptr<Type> resolveTypedef(std::shared_ptr<Type> t);
    bool validateType(std::shared_ptr<Type> t, int line);

    // Import handling
    bool processImport(std::shared_ptr<ImportDecl> id);

    // Unused variable check at scope exit
    void checkUnusedInScope(int level);
};

#endif

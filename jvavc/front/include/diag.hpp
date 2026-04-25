#ifndef DIAG_HPP
#define DIAG_HPP
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>

enum DiagLevel {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
    DIAG_HELP
};

struct DiagLabel {
    int line;
    int col_start;
    int col_end;
    std::string message;
};

struct Diagnostic {
    std::string code;          // e.g., "E0001"
    std::string message;
    DiagLevel level;
    std::string file;
    int line;
    int col;
    std::vector<DiagLabel> labels;
    std::vector<std::pair<DiagLevel, std::string>> notes;
};

class DiagEngine {
public:
    void emit(const Diagnostic &d);
    void emitError(const std::string &file, int line, int col,
                   const std::string &code, const std::string &msg);
    void emitError(const std::string &file, int line, int col,
                   const std::string &code, const std::string &msg,
                   const std::string &hint);
    bool hasErrors() const { return errorCount > 0; }
    int getErrorCount() const { return errorCount; }
    int getWarningCount() const { return warningCount; }
    void printAll(std::ostream &os) const;
    void clear();

    static std::string levelPrefix(DiagLevel level);
    static std::string levelColor(DiagLevel level);

private:
    std::vector<Diagnostic> diagnostics;
    int errorCount = 0;
    int warningCount = 0;

    static std::string getLine(const std::string &file, int line);
    static void printSnippet(std::ostream &os, const Diagnostic &d);
};

#endif

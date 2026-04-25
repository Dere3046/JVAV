#include "diag.hpp"
#include <iomanip>
#include <algorithm>

using namespace std;

static bool isTerminal() {
#ifdef _WIN32
    return false; // simplified
#else
    return isatty(fileno(stderr));
#endif
}

string DiagEngine::levelPrefix(DiagLevel level) {
    switch (level) {
        case DIAG_ERROR:   return "error";
        case DIAG_WARNING: return "warning";
        case DIAG_NOTE:    return "note";
        case DIAG_HELP:    return "help";
    }
    return "?";
}

string DiagEngine::levelColor(DiagLevel level) {
    if (!isTerminal()) return "";
    switch (level) {
        case DIAG_ERROR:   return "\033[1;31m";
        case DIAG_WARNING: return "\033[1;33m";
        case DIAG_NOTE:    return "\033[1;34m";
        case DIAG_HELP:    return "\033[1;32m";
    }
    return "";
}

string DiagEngine::getLine(const string &filename, int targetLine) {
    ifstream f(filename);
    if (!f) return "";
    string line;
    int n = 1;
    while (getline(f, line)) {
        if (n == targetLine) return line;
        ++n;
    }
    return "";
}

void DiagEngine::emit(const Diagnostic &d) {
    diagnostics.push_back(d);
    if (d.level == DIAG_ERROR) ++errorCount;
    else if (d.level == DIAG_WARNING) ++warningCount;
}

void DiagEngine::emitError(const string &file, int line, int col,
                           const string &code, const string &msg) {
    Diagnostic d;
    d.file = file;
    d.line = line;
    d.col = col;
    d.code = code;
    d.message = msg;
    d.level = DIAG_ERROR;
    emit(d);
}

void DiagEngine::emitError(const string &file, int line, int col,
                           const string &code, const string &msg,
                           const string &hint) {
    Diagnostic d;
    d.file = file;
    d.line = line;
    d.col = col;
    d.code = code;
    d.message = msg;
    d.level = DIAG_ERROR;
    if (!hint.empty()) {
        d.notes.push_back({DIAG_HELP, hint});
    }
    emit(d);
}

void DiagEngine::printSnippet(ostream &os, const Diagnostic &d) {
    string lineText = getLine(d.file, d.line);
    if (lineText.empty() && d.line > 0) {
        lineText = "<unable to read source>";
    }

    string reset = isTerminal() ? "\033[0m" : "";
    string errColor = levelColor(DIAG_ERROR);
    string noteColor = levelColor(DIAG_NOTE);
    string helpColor = levelColor(DIAG_HELP);

    // Header
    os << errColor << levelPrefix(d.level) << reset;
    if (!d.code.empty()) os << "[" << d.code << "]";
    os << ": " << d.message << "\n";

    // Location
    os << " --> " << d.file << ":" << d.line << ":" << d.col << "\n";

    // Source line
    int lineNumWidth = max(4, (int)to_string(d.line).length());
    os << string(lineNumWidth + 1, ' ') << "|\n";
    os << " " << setw(lineNumWidth) << d.line << " | " << lineText << "\n";

    // Underline
    os << string(lineNumWidth + 1, ' ') << "| ";
    int underlineStart = max(0, d.col - 1);
    int underlineLen = 1;
    if (!d.labels.empty() && d.labels[0].col_end > d.labels[0].col_start) {
        underlineStart = max(0, d.labels[0].col_start - 1);
        underlineLen = d.labels[0].col_end - d.labels[0].col_start;
    }
    for (int i = 0; i < underlineStart; ++i) os << " ";
    os << errColor;
    for (int i = 0; i < underlineLen; ++i) os << "^";
    os << reset;
    if (!d.labels.empty() && !d.labels[0].message.empty()) {
        os << " " << helpColor << d.labels[0].message << reset;
    }
    os << "\n";

    // Notes / help
    for (const auto &note : d.notes) {
        os << "   = " << levelColor(note.first) << levelPrefix(note.first) << reset
           << ": " << note.second << "\n";
    }
}

void DiagEngine::printAll(ostream &os) const {
    for (const auto &d : diagnostics) {
        printSnippet(os, d);
        os << "\n";
    }
    if (errorCount > 0 || warningCount > 0) {
        os << "aborted due to " << errorCount << " error(s) and "
           << warningCount << " warning(s)\n";
    }
}

void DiagEngine::clear() {
    diagnostics.clear();
    errorCount = 0;
    warningCount = 0;
}

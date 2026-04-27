#ifdef _WIN32
#include <process.h>
#endif

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <vector>
namespace fs = std::filesystem;
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "codegen.hpp"
using namespace std;

#ifdef _WIN32
static int run_cmd(const string &cmd) {
    vector<string> args;
    size_t i = 0;
    while (i < cmd.size()) {
        while (i < cmd.size() && isspace((unsigned char)cmd[i])) ++i;
        if (i >= cmd.size()) break;
        string arg;
        if (cmd[i] == '"') {
            ++i;
            while (i < cmd.size() && cmd[i] != '"') {
                arg += cmd[i];
                ++i;
            }
            if (i < cmd.size() && cmd[i] == '"') ++i;
        } else {
            while (i < cmd.size() && !isspace((unsigned char)cmd[i])) {
                arg += cmd[i];
                ++i;
            }
        }
        args.push_back(arg);
    }
    if (args.empty()) return -1;
    vector<const char*> argv;
    for (auto &a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    intptr_t ret = _spawnvp(_P_WAIT, argv[0], argv.data());
    return (ret == -1) ? -1 : (int)ret;
}
#else
static int run_cmd(const string &cmd) {
    return system(cmd.c_str());
}
#endif

static fs::path get_executable_dir(char *argv0) {
    try {
        return fs::canonical(fs::path(argv0)).parent_path();
    } catch (...) {
        return fs::current_path();
    }
}

// Split source text into lines; return vector of (line_number, content)
static vector<pair<int, string>> splitLines(const string &srcText) {
    vector<pair<int, string>> lines;
    int curLine = 1;
    size_t start = 0;
    for (size_t i = 0; i <= srcText.size(); ++i) {
        if (i == srcText.size() || srcText[i] == '\n') {
            string content = srcText.substr(start, i - start);
            if (!content.empty() && content.back() == '\r') content.pop_back();
            lines.push_back({curLine, content});
            start = i + 1;
            ++curLine;
        }
    }
    return lines;
}

static void printSourceSnippet(ostream &os, const vector<pair<int, string>> &lines,
                                int targetLine, int col, int contextLines = 1) {
    if (lines.empty() || targetLine <= 0) return;
    int targetIdx = -1;
    for (int i = 0; i < (int)lines.size(); ++i) {
        if (lines[i].first == targetLine) { targetIdx = i; break; }
    }
    if (targetIdx == -1) return; // line not found

    int ctxStart = max(0, targetIdx - contextLines);
    int ctxEnd = min((int)lines.size() - 1, targetIdx + contextLines);
    int maxLineNum = lines[ctxEnd].first;
    int w = (int)to_string(maxLineNum).length();

    os << string(w + 1, ' ') << "|\n";
    for (int i = ctxStart; i <= ctxEnd; ++i) {
        os << " " << setw(w) << lines[i].first << " | " << lines[i].second << "\n";
        if (i == targetIdx) {
            os << string(w + 1, ' ') << "| ";
            for (int j = 1; j < col; ++j) os << " ";
            os << "^\n";
        }
    }
}

static void printSourceLine(ostream &os, const string &srcText, int line, int col) {
    auto lines = splitLines(srcText);
    printSourceSnippet(os, lines, line, col, 1);
}

static void printLexError(const Lexer &lexer) {
    cerr << "error[E0100]: " << lexer.getError() << "\n";
    if (lexer.getErrorLine() > 0) {
        cerr << " --> " << lexer.getFilename() << ":" << lexer.getErrorLine()
             << ":" << lexer.getErrorCol() << "\n";
        printSourceLine(cerr, lexer.getSource(), lexer.getErrorLine(), lexer.getErrorCol());
    }
}

static void printParseError(const FrontParser &parser, const Lexer &lexer) {
    cerr << "error[E0200]: " << parser.getError() << "\n";
    if (parser.getErrorLine() > 0) {
        cerr << " --> " << lexer.getFilename() << ":" << parser.getErrorLine()
             << ":" << parser.getErrorCol() << "\n";
        printSourceLine(cerr, lexer.getSource(), parser.getErrorLine(), parser.getErrorCol());
    }
}

static int compile_jvl(const string &src, const string &out, const vector<string> &importPaths) {
    Lexer lexer;
    if (!lexer.tokenize(src)) {
        printLexError(lexer);
        return 1;
    }

    FrontParser parser;
    if (!parser.parse(lexer.getTokens())) {
        printParseError(parser, lexer);
        return 1;
    }

    Sema sema;
    string basePath = fs::path(src).parent_path().string();
    sema.setImportPaths(importPaths);
    sema.setCurrentFile(src);
    if (!sema.analyze(parser.getProgram(), basePath)) {
        sema.printErrors(cerr);
        return 1;
    } else if (!sema.getErrors().empty()) {
        sema.printErrors(cout);
    }

    CodeGenerator codegen;
    codegen.setImportPaths(importPaths);
    string asmText = codegen.generate(parser.getProgram(), basePath);

    ofstream f(out);
    if (!f) {
        cerr << "error[E0300]: cannot write output file `" << out << "`\n";
        return 1;
    }
    f << asmText;
    return 0;
}

static vector<string> find_std_import_paths(const fs::path &exeDir) {
    vector<string> paths;
    vector<fs::path> candidates = {
        exeDir,                   // bin/     → check bin/std/
        exeDir / "..",            // ../      → check ../std/ (installed layout)
        exeDir / ".." / ".."      // ../../   → check ../../std/ (dev layout)
    };
    for (auto &cand : candidates) {
        try {
            fs::path base = fs::weakly_canonical(cand);
            fs::path stdDir = base / "std";
            if (fs::exists(stdDir) && fs::is_directory(stdDir)) {
                paths.push_back(base.string());
                break;
            }
        } catch (...) {}
    }
    return paths;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    system("chcp 65001 >nul");
#endif
    bool runMode = false;
    string src;
    string out;
    string legacyOut;   // for: jvlc source.jvl output.jvav

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--run" || arg == "-r") {
            runMode = true;
        } else if (arg == "-o" && i + 1 < argc) {
            out = argv[++i];
        } else if (arg == "-v" || arg == "--version") {
            cout << "jvlc " << JVAV_VERSION << "\n";
            return 0;
        } else if (arg[0] != '-') {
            if (src.empty()) {
                src = arg;
            } else if (legacyOut.empty() && !runMode) {
                legacyOut = arg;     // old-style second positional argument = output
            } else {
                cerr << "Usage: " << argv[0] << " [-v|--version] [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
                cerr << "  Default: compile .jvl to .jvav\n";
                cerr << "  --run: compile .jvl -> .jvav -> .bin and execute with JVM\n";
                return 1;
            }
        } else {
            cerr << "Usage: " << argv[0] << " [-v|--version] [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
            cerr << "  Default: compile .jvl to .jvav\n";
            cerr << "  --run: compile .jvl -> .jvav -> .bin and execute with JVM\n";
            return 1;
        }
    }

    if (src.empty()) {
        cerr << "Usage: " << argv[0] << " [-v|--version] [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
        cerr << "  Default: compile .jvl to .jvav\n";
        cerr << "  --run: compile .jvl -> .jvav -> .bin and execute with JVM\n";
        return 1;
    }

    fs::path srcPath(src);
    string stem = srcPath.stem().string();

    if (!runMode) {
        // Normal compile mode
        string jvavOut = out.empty() ? (legacyOut.empty() ? "a.jvav" : legacyOut) : out;
        fs::path exeDir = get_executable_dir(argv[0]);
        vector<string> importPaths = find_std_import_paths(exeDir);
        int ret = compile_jvl(src, jvavOut, importPaths);
        if (ret == 0) {
            cout << "Compiled to " << jvavOut << "\n";
        }
        return ret;
    }

    // Run mode: .jvl -> .jvav -> .bin -> execute
    string jvavFile = out.empty() ? (stem + ".jvav") : (fs::path(out).stem().string() + ".jvav");
    string binFile = out.empty() ? (stem + ".bin") : out;

    fs::path exeDir = get_executable_dir(argv[0]);
    vector<string> importPaths = find_std_import_paths(exeDir);
    int ret = compile_jvl(src, jvavFile, importPaths);
    if (ret != 0) return ret;

    // Find backend assembler and JVM
    string jvavcExe = exeDir.string();
    string jvmExe = exeDir.string();
#ifdef _WIN32
    jvavcExe += "\\jvavc.exe";
    jvmExe += "\\jvm.exe";
#else
    jvavcExe += "/jvavc";
    jvmExe += "/jvm";
#endif

    // Fallback to PATH if not found alongside jvlc
    if (!fs::exists(jvavcExe)) jvavcExe = "jvavc";
    if (!fs::exists(jvmExe)) jvmExe = "jvm";

    ret = run_cmd("\"" + jvavcExe + "\" \"" + jvavFile + "\" \"" + binFile + "\"");
    if (ret != 0) {
        cerr << "Backend assembly failed\n";
        return ret;
    }

    ret = run_cmd("\"" + jvmExe + "\" \"" + binFile + "\"");
    if (ret != 0) {
        cerr << "JVM execution failed\n";
        return ret;
    }

    // Clean up intermediate files unless -o was specified
    if (out.empty()) {
        fs::remove(jvavFile);
        fs::remove(binFile);
    }

    return 0;
}

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <iomanip>
namespace fs = std::filesystem;
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "codegen.hpp"
using namespace std;

static int run_cmd(const string &cmd) {
    return system(cmd.c_str());
}

static fs::path get_executable_dir(char *argv0) {
    try {
        return fs::canonical(fs::path(argv0)).parent_path();
    } catch (...) {
        return fs::current_path();
    }
}

static void printLexError(const Lexer &lexer) {
    cerr << "error[E0100]: " << lexer.getError() << "\n";
    if (lexer.getErrorLine() > 0) {
        cerr << " --> " << lexer.getFilename() << ":" << lexer.getErrorLine()
             << ":" << lexer.getErrorCol() << "\n";
        // Try to print source line
        const string &srcText = lexer.getSource();
        int line = 1;
        size_t start = 0;
        for (size_t i = 0; i < srcText.size(); ++i) {
            if (line == lexer.getErrorLine()) {
                start = i;
                while (i < srcText.size() && srcText[i] != '\n' && srcText[i] != '\r') ++i;
                string srcLine = srcText.substr(start, i - start);
                cerr << "  " << setw(4) << line << " | " << srcLine << "\n";
                cerr << "      | ";
                for (int j = 1; j < lexer.getErrorCol(); ++j) cerr << " ";
                cerr << "^\n";
                break;
            }
            if (srcText[i] == '\n') { ++line; }
        }
    }
}

static void printParseError(const FrontParser &parser, const Lexer &/*lexer*/) {
    cerr << "error[E0200]: " << parser.getError() << "\n";
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

int main(int argc, char *argv[]) {
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
        } else if (arg[0] != '-') {
            if (src.empty()) {
                src = arg;
            } else if (legacyOut.empty() && !runMode) {
                legacyOut = arg;     // old-style second positional argument = output
            } else {
                cerr << "Usage: " << argv[0] << " [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
                cerr << "  Default: compile .jvl to .jvav\n";
                cerr << "  --run: compile .jvl -> .jvav -> .bin and execute with JVM\n";
                return 1;
            }
        } else {
            cerr << "Usage: " << argv[0] << " [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
            cerr << "  Default: compile .jvl to .jvav\n";
            cerr << "  --run: compile .jvl -> .jvav -> .bin and execute with JVM\n";
            return 1;
        }
    }

    if (src.empty()) {
        cerr << "Usage: " << argv[0] << " [--run|-r] [-o output] <source.jvl> [output.jvav]\n";
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
        fs::path stdDir = exeDir / ".." / "..";
        vector<string> importPaths;
        try {
            importPaths.push_back(fs::weakly_canonical(stdDir).string());
        } catch (...) {}
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
    fs::path stdDir = exeDir / ".." / "..";
    vector<string> importPaths;
    try {
        importPaths.push_back(fs::weakly_canonical(stdDir).string());
    } catch (...) {}
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

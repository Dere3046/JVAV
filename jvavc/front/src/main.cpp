#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "codegen.hpp"
using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <source.jvl> [output.jvav]\n";
        return 1;
    }
    string src = argv[1];
    string out = (argc >= 3) ? argv[2] : "a.jvav";

    Lexer lexer;
    if (!lexer.tokenize(src)) {
        cerr << "Lex error: " << lexer.getError() << "\n";
        return 1;
    }

    FrontParser parser;
    if (!parser.parse(lexer.getTokens())) {
        cerr << "Parse error: " << parser.getError() << "\n";
        return 1;
    }

    Sema sema;
    string basePath = fs::path(src).parent_path().string();
    if (!sema.analyze(parser.getProgram(), basePath)) {
        sema.printErrors(cerr);
        return 1;
    } else if (!sema.getErrors().empty()) {
        sema.printErrors(cout);
    }

    CodeGenerator codegen;
    string asmText = codegen.generate(parser.getProgram(), basePath);

    ofstream f(out);
    if (!f) { cerr << "Cannot write to " << out << "\n"; return 1; }
    f << asmText;
    cout << "Compiled to " << out << "\n";
    return 0;
}

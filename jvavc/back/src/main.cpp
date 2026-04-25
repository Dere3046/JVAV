#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "parser.hpp"
#include "encoder.hpp"
#include "linker.hpp"
using namespace std;

static void printUsage(const char *prog) {
    cerr << "Usage:\n"
         << "  " << prog << " <source.jvav> [output.bin]       (single file)\n"
         << "  " << prog << " <file1.jvav> [file2.jvav ...] -o <output.bin>\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    vector<string> inputs;
    string output;
    bool hasDashO = false;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-o") {
            hasDashO = true;
            if (i + 1 < argc) {
                output = argv[++i];
            } else {
                cerr << "Error: -o requires an output filename\n";
                return 1;
            }
        } else {
            inputs.push_back(arg);
        }
    }

    if (inputs.empty()) { printUsage(argv[0]); return 1; }

    if (!hasDashO) {
        if (inputs.size() == 1) {
            output = "a.out";
        } else if (inputs.size() == 2) {
            // Legacy mode: jvavc input output
            output = inputs.back();
            inputs.pop_back();
        } else {
            output = "a.out";
        }
    }

    if (inputs.size() == 1) {
        // Single-file mode
        Parser p;
        if (!p.parse(inputs[0])) { cerr << "Parse error: " << p.getError() << "\n"; return 1; }
        Encoder e;
        vector<Int128> bc;
        if (!e.encode(p.getInstructions(), p.getLabels(), bc)) {
            cerr << "Encode error: " << e.getError() << "\n";
            return 1;
        }
        ofstream f(output, ios::binary);
        f.write((char*)bc.data(), bc.size()*sizeof(Int128));
        cout << "Compiled to " << output << " (" << bc.size() << " units)\n";
    } else {
        // Multi-file linker mode
        Linker linker;
        for (auto &in : inputs) {
            if (!linker.addFile(in)) {
                cerr << "Link error: " << linker.getError() << "\n";
                return 1;
            }
        }
        if (!linker.link(output)) {
            cerr << "Link error: " << linker.getError() << "\n";
            return 1;
        }
        cout << "Linked to " << output << "\n";
    }
    return 0;
}

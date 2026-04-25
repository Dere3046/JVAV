#include "linker.hpp"
#include <fstream>
#include <cctype>
using namespace std;

static string trim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool Linker::extractEqu(const vector<string> &lines, map<string, Int128> &out) {
    for (const string &line : lines) {
        string l = trim(line);
        if (l.empty() || l.substr(0,2)=="//" || l[0]==';') continue;
        
        // Format: LABEL: EQU value
        size_t colon = l.find(':');
        if (colon != string::npos) {
            string rest = trim(l.substr(colon+1));
            if (!rest.empty() && rest.substr(0,3)=="EQU") {
                string name = trim(l.substr(0, colon));
                string valstr = trim(rest.substr(3));
                if (!name.empty()) {
                    try {
                        Int128 v;
                        if (valstr.substr(0,2)=="0x") v = stoull(valstr.substr(2),nullptr,16);
                        else v = stoll(valstr);
                        out[name] = v;
                    } catch(...) {}
                }
                continue;
            }
        }
        
        // Format: LABEL EQU value
        size_t equPos = l.find("EQU");
        if (equPos != string::npos) {
            string name = trim(l.substr(0, equPos));
            string valstr = trim(l.substr(equPos + 3));
            if (!name.empty() && !valstr.empty()) {
                try {
                    Int128 v;
                    if (valstr.substr(0,2)=="0x") v = stoull(valstr.substr(2),nullptr,16);
                    else v = stoll(valstr);
                    out[name] = v;
                } catch(...) {}
            }
        }
    }
    return true;
}

bool Linker::addFile(const string &filename) {
    FileUnit unit;
    unit.filename = filename;
    if (!unit.parser.preprocessIncludes(filename, unit.lines)) {
        error = unit.parser.getError();
        return false;
    }
    extractEqu(unit.lines, globalEqu);
    units.push_back(std::move(unit));
    return true;
}

bool Linker::link(const string &output) {
    // Auto-reorder: move entry-point file (_start or main) to front
    auto isEntry = [](const FileUnit &u) -> bool {
        for (const string &line : u.lines) {
            string l = trim(line);
            if (l.empty() || l[0]==';' || l.substr(0,2)=="//") continue;
            if (l.find("_start:") != string::npos) return true;
            if (l.find("main:") != string::npos) return true;
            if (l.substr(0,14)==".global _start") return true;
            if (l.substr(0,12)==".global main") return true;
        }
        return false;
    };
    for (size_t i = 0; i < units.size(); i++) {
        if (isEntry(units[i])) {
            if (i > 0) {
                auto entry = std::move(units[i]);
                units.erase(units.begin() + i);
                units.insert(units.begin(), std::move(entry));
            }
            break;
        }
    }

    vector<Instruction> mergedInstructions;
    map<string, int> mergedLabels;
    set<string> allGlobals;
    set<string> allExterns;

    int baseAddr = 0;
    for (auto &unit : units) {
        unit.parser.clear();
        for (auto &kv : globalEqu) {
            unit.parser.addExternalEqu(kv.first, kv.second);
        }
        unit.parser.setBaseAddr(baseAddr);
        if (!unit.parser.parseLines(unit.lines)) {
            error = unit.filename + ": " + unit.parser.getError();
            return false;
        }
        
        // Merge instructions
        const auto &instrs = unit.parser.getInstructions();
        mergedInstructions.insert(mergedInstructions.end(), instrs.begin(), instrs.end());
        
        // Merge labels (with conflict detection)
        for (auto &kv : unit.parser.getLabels()) {
            if (mergedLabels.find(kv.first) != mergedLabels.end()) {
                error = "Duplicate label: " + kv.first + " in " + unit.filename;
                return false;
            }
            mergedLabels[kv.first] = kv.second;
        }
        
        // Collect globals and externs
        for (auto &s : unit.parser.getGlobalSymbols()) allGlobals.insert(s);
        for (auto &s : unit.parser.getExternSymbols()) allExterns.insert(s);
        
        // Update base address for next file
        for (auto &instr : instrs) {
            baseAddr += Parser::getInstructionSize(instr);
        }
    }
    
    // Check extern symbols
    for (auto &sym : allExterns) {
        if (mergedLabels.find(sym) == mergedLabels.end()) {
            error = "Undefined external symbol: " + sym;
            return false;
        }
    }
    
    // Encode
    Encoder e;
    vector<Int128> bc;
    if (!e.encode(mergedInstructions, mergedLabels, bc)) {
        error = e.getError();
        return false;
    }
    
    ofstream f(output, ios::binary);
    if (!f) { error = "Cannot write to " + output; return false; }
    f.write((char*)bc.data(), bc.size()*sizeof(Int128));
    return true;
}

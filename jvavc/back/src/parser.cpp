#include "parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <cctype>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
using namespace std;

static const set<string> validMnemonics = {
    "HALT", "MOV", "LDR", "STR", "ADD", "SUB", "MUL", "DIV",
    "CMP", "JMP", "JZ", "JNZ", "JE", "JNE", "JL", "JG", "JLE", "JGE",
    "PUSH", "POP", "CALL", "RET", "LDI"
};

static string trim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static vector<string> split(const string &s, char delim) {
    vector<string> tks; stringstream ss(s); string it;
    while (getline(ss, it, delim)) { string t = trim(it); if (!t.empty()) tks.push_back(t); }
    return tks;
}

int Parser::getRegister(const string &s) {
    if (s.size()==2 && s[0]=='R' && isdigit(s[1])) return s[1]-'0';
    if (s=="PC") return 8;
    if (s=="SP") return 9;
    if (s=="FLAGS") return 10;
    return -1;
}

void Parser::addExternalEqu(const std::string &name, __int128 value) {
    equSymbols[name] = value;
}

void Parser::setBaseAddr(int addr) {
    baseAddr = addr;
}

void Parser::clear() {
    instructions.clear();
    labels.clear();
    equSymbols.clear();
    includedFiles.clear();
    globalSymbols.clear();
    externSymbols.clear();
    error.clear();
    baseAddr = 0;
}

bool Parser::resolveEqu(const std::string &name, __int128 &out) {
    auto it = equSymbols.find(name);
    if (it == equSymbols.end()) return false;
    out = it->second;
    return true;
}

bool Parser::parseOperand(const string &tok, Operand &op) {
    string t = tok;
    if (t.front()=='[' && t.back()==']') {
        string inner = trim(t.substr(1, t.size()-2));
        int r = getRegister(inner);
        if (r >= 0 && r <= 7) {
            op.type = OP_MEM; op.reg = r; return true;
        }
        if (isdigit(inner[0]) || (inner[0]=='-'&&isdigit(inner[1])) || (inner.size()>2&&inner.substr(0,2)=="0x")) {
            op.type = OP_MEM; op.reg = -1;
            try {
                if (inner.substr(0,2)=="0x") op.imm = stoull(inner.substr(2),nullptr,16);
                else op.imm = stoll(inner);
            } catch(...) { error="Bad mem addr: "+inner; return false; }
            return true;
        }
        __int128 equVal;
        if (resolveEqu(inner, equVal)) {
            op.type = OP_MEM; op.reg = -1; op.imm = equVal; return true;
        }
        op.type = OP_MEM; op.reg = -1; op.label = inner; return true;
    }
    if (t.front()=='\'' && t.back()=='\'') {
        if (t.size()==3) { op.type=OP_IMM; op.imm=(unsigned char)t[1]; return true; }
        error="Bad char: "+t; return false;
    }
    if (isdigit(t[0])||(t[0]=='-'&&isdigit(t[1]))||(t.size()>2&&t.substr(0,2)=="0x")) {
        op.type=OP_IMM;
        try {
            if (t.substr(0,2)=="0x") op.imm = stoull(t.substr(2),nullptr,16);
            else op.imm = stoll(t);
        }
        catch(...) { error="Bad imm: "+t; return false; }
        return true;
    }
    int r = getRegister(t);
    if (r>=0&&r<=10) { op.type=OP_REG; op.reg=r; return true; }
    __int128 equVal;
    if (resolveEqu(t, equVal)) {
        op.type = OP_IMM; op.imm = equVal; return true;
    }
    op.type=OP_IMM; op.label=t; op.imm=0; return true;
}

static bool parseDataItems(const string &args, vector<DataItem> &items, int width) {
    if (args.empty()) return true;
    if (args.find(',') == string::npos) {
        string p = trim(args);
        if (p.empty()) return true;
        if (p.front()=='"' && p.back()=='"') {
            string s = p.substr(1, p.size()-2);
            for (char c : s) items.push_back({(unsigned char)c, width});
        } else if (p.front()== '\'' && p.back()=='\'') {
            if (p.size()!=3) return false;
            items.push_back({(unsigned char)p[1], width});
        } else {
            try {
                __int128 v;
                if (p.substr(0,2)=="0x") v = stoull(p.substr(2),nullptr,16);
                else v = stoll(p);
                items.push_back({v, width});
            } catch(...) { return false; }
        }
        return true;
    }
    for (const string &p : split(args, ',')) {
        if (p.front()=='"' && p.back()=='"') {
            string s = p.substr(1, p.size()-2);
            for (char c : s) items.push_back({(unsigned char)c, width});
        } else if (p.front()=='\'' && p.back()=='\'') {
            if (p.size()!=3) return false;
            items.push_back({(unsigned char)p[1], width});
        } else {
            try {
                __int128 v;
                if (p.substr(0,2)=="0x") v = stoull(p.substr(2),nullptr,16);
                else v = stoll(p);
                items.push_back({v, width});
            } catch(...) { return false; }
        }
    }
    return true;
}

bool Parser::parseLine(const string &line, int lineNum) {
    string l = trim(line);
    if (l.empty() || l.substr(0,2)=="//" || l[0]==';') return true;
    if (l.substr(0,5)==".data" || l.substr(0,5)==".text") return true;
    if (l.substr(0,7)==".global") {
        string rest = trim(l.substr(7));
        for (const string &s : split(rest, ',')) {
            string sym = trim(s);
            if (!sym.empty()) globalSymbols.insert(sym);
        }
        return true;
    }
    if (l.substr(0,7)==".extern") {
        string rest = trim(l.substr(7));
        for (const string &s : split(rest, ',')) {
            string sym = trim(s);
            if (!sym.empty()) externSymbols.insert(sym);
        }
        return true;
    }
    if (l.substr(0,8)=="#include") {
        string rest = trim(l.substr(8));
        if (rest.size()>=2 && ((rest.front()=='"'&&rest.back()=='"') || (rest.front()=='<'&&rest.back()=='>'))) {
            string incfile = rest.substr(1, rest.size()-2);
            Instruction instr; instr.lineNum = lineNum;
            instr.mnemonic = "#INCLUDE";
            instructions.push_back(instr);
            return true;
        }
    }
    size_t colon = l.find(':');
    string lbl, rest = l;
    if (colon != string::npos) {
        lbl = trim(l.substr(0, colon));
        rest = trim(l.substr(colon+1));
    }
    // EQU with colon: "LABEL: EQU value"
    if (!rest.empty() && rest.substr(0,3)=="EQU") {
        string valstr = trim(rest.substr(3));
        try {
            __int128 v;
            if (valstr.substr(0,2)=="0x") v = stoull(valstr.substr(2),nullptr,16);
            else v = stoll(valstr);
            equSymbols[lbl] = v;
        } catch(...) { error="Bad EQU value at line "+to_string(lineNum); return false; }
        return true;
    }
    // EQU without colon: "LABEL EQU value"
    if (colon == string::npos) {
        size_t equPos = l.find("EQU");
        if (equPos != string::npos) {
            string left = trim(l.substr(0, equPos));
            string right = trim(l.substr(equPos + 3));
            if (!left.empty() && !right.empty()) {
                try {
                    __int128 v;
                    if (right.substr(0,2)=="0x") v = stoull(right.substr(2),nullptr,16);
                    else v = stoll(right);
                    equSymbols[left] = v;
                } catch(...) { error="Bad EQU value at line "+to_string(lineNum); return false; }
                return true;
            }
        }
    }
    Instruction instr;
    instr.lineNum = lineNum;
    instr.label = lbl;
    instr.isPseudo = false;
    if (!rest.empty() && (rest.substr(0,2)=="DB" || rest.substr(0,2)=="DW" || rest.substr(0,2)=="DT")) {
        string args = trim(rest.substr(2));
        int width = (rest[1]=='B')?1:(rest[1]=='W')?2:16;
        if (!parseDataItems(args, instr.dataItems, width)) {
            error="Bad data items at line "+to_string(lineNum); return false;
        }
        instr.isPseudo = true;
        instructions.push_back(instr);
        return true;
    }
    if (rest.empty()) {
        if (!lbl.empty()) instructions.push_back(instr);
        return true;
    }
    size_t sp = rest.find_first_of(" \t");
    string mnem = (sp==string::npos)?rest:rest.substr(0,sp);
    transform(mnem.begin(), mnem.end(), mnem.begin(), ::toupper);
    if (validMnemonics.find(mnem)==validMnemonics.end()) {
        error="Unknown mnemonic '"+mnem+"' at line "+to_string(lineNum)+": "+line;
        return false;
    }
    instr.mnemonic = mnem;
    string oppart = (sp==string::npos)?"":trim(rest.substr(sp));
    for (const string &os : split(oppart, ',')) {
        Operand op;
        if (!parseOperand(os, op)) {
            error += " at line "+to_string(lineNum)+": "+line; return false;
        }
        instr.ops.push_back(op);
    }
    instructions.push_back(instr);
    return true;
}

int Parser::getInstructionSize(const Instruction& instr) {
    if (instr.isPseudo) return (int)instr.dataItems.size();
    if (instr.mnemonic.empty() || instr.mnemonic == "#INCLUDE") return 0;
    if ((instr.mnemonic=="JMP"||instr.mnemonic=="JZ"||instr.mnemonic=="JNZ"||
         instr.mnemonic=="JE"||instr.mnemonic=="JNE"||instr.mnemonic=="JL"||
         instr.mnemonic=="JG"||instr.mnemonic=="JLE"||instr.mnemonic=="JGE"||
         instr.mnemonic=="CALL") &&
        instr.ops.size()==1 && !instr.ops[0].label.empty())
        return 2;
    if ((instr.mnemonic=="ADD"||instr.mnemonic=="SUB"||instr.mnemonic=="MUL"||instr.mnemonic=="DIV") && instr.ops.size()==3) {
        int extra = 0;
        if (instr.ops[1].type == OP_IMM) extra++;
        if (instr.ops[2].type == OP_IMM) extra++;
        return 1 + extra;
    }
    // LDR/STR with [imm] or [label] memory operand needs LDI expansion
    if ((instr.mnemonic=="LDR"||instr.mnemonic=="STR") && instr.ops.size()==2) {
        int memIdx = (instr.mnemonic=="LDR") ? 1 : 0;
        if (instr.ops[memIdx].type == OP_MEM && instr.ops[memIdx].reg < 0) {
            return 2;
        }
    }
    return 1;
}

void Parser::computeAddresses() {
    int addr = baseAddr;
    for (auto &instr : instructions) {
        if (!instr.label.empty()) {
            if (labels.find(instr.label) != labels.end()) {
                error = "Duplicate label: " + instr.label;
                return;
            }
            labels[instr.label] = addr;
        }
        addr += getInstructionSize(instr);
    }
}

bool Parser::preprocessFile(const std::string &filename, const std::string &basePathStr, std::vector<std::string> &outLines) {
    fs::path basePath = fs::path(basePathStr);
    fs::path cp = fs::path(filename);
    string canon = fs::canonical(cp).string();
    if (includedFiles.find(canon) != includedFiles.end()) return true;
    includedFiles.insert(canon);
    ifstream file(filename);
    if (!file) { error = "Cannot open file: " + filename; return false; }
    string line;
    int lineNum = 0;
    while (getline(file, line)) {
        lineNum++;
        string l = trim(line);
        if (l.substr(0,8)=="#include") {
            string rest = trim(l.substr(8));
            if (rest.size()>=2 && ((rest.front()=='"'&&rest.back()=='"') || (rest.front()=='<'&&rest.back()=='>'))) {
                string incfile = rest.substr(1, rest.size()-2);
                fs::path incPath = basePath / incfile;
                if (!fs::exists(incPath)) {
                    incPath = cp.parent_path() / incfile;
                }
                if (!fs::exists(incPath)) {
                    error = "Include file not found: " + incfile + " at line " + to_string(lineNum) + " in " + filename;
                    return false;
                }
                if (!preprocessFile(incPath.string(), basePath.string(), outLines)) return false;
                continue;
            }
        }
        outLines.push_back(line);
    }
    return true;
}

bool Parser::preprocessIncludes(const std::string &filename, std::vector<std::string> &outLines) {
    fs::path basePath = fs::path(filename).parent_path();
    if (basePath.empty()) basePath = fs::current_path();
    return preprocessFile(filename, basePath.string(), outLines);
}

bool Parser::parseLines(const vector<string> &lines) {
    instructions.clear();
    labels.clear();
    // Keep equSymbols (may contain external EQU injected by linker)
    includedFiles.clear();
    globalSymbols.clear();
    externSymbols.clear();
    int lineNum = 0;
    for (const string &line : lines) {
        lineNum++;
        if (!parseLine(line, lineNum)) return false;
    }
    computeAddresses();
    return error.empty();
}

bool Parser::parse(const string &filename) {
    vector<string> lines;
    if (!preprocessIncludes(filename, lines)) return false;
    return parseLines(lines);
}

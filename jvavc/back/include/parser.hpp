#ifndef PARSER_HPP
#define PARSER_HPP
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "int128.hpp"
#include <set>

enum OperandType { OP_NONE, OP_REG, OP_IMM, OP_MEM };

struct Operand {
    OperandType type = OP_NONE;
    int reg = -1;
    int64_t _pad = 0;
    Int128 imm = 0;
    std::string label;
};

struct DataItem {
    Int128 value;
    int width;
};

struct Instruction {
    std::string mnemonic;
    std::vector<Operand> ops;
    std::string label;
    int lineNum;
    bool isPseudo;
    std::vector<DataItem> dataItems;
};

class Parser {
public:
    bool parse(const std::string &filename);
    bool parseLines(const std::vector<std::string> &lines);
    bool preprocessIncludes(const std::string &filename, std::vector<std::string> &outLines);

    const std::string& getError() const { return error; }
    const std::vector<Instruction>& getInstructions() const { return instructions; }
    const std::map<std::string, int>& getLabels() const { return labels; }
    const std::map<std::string, Int128>& getEquSymbols() const { return equSymbols; }
    const std::set<std::string>& getGlobalSymbols() const { return globalSymbols; }
    const std::set<std::string>& getExternSymbols() const { return externSymbols; }

    void addExternalEqu(const std::string &name, Int128 value);
    void setBaseAddr(int addr);
    void clear();

    static int getInstructionSize(const Instruction& instr);

private:
    std::vector<Instruction> instructions;
    std::map<std::string, int> labels;
    std::map<std::string, Int128> equSymbols;
    std::set<std::string> includedFiles;
    std::set<std::string> globalSymbols;
    std::set<std::string> externSymbols;
    std::string error;
    int baseAddr = 0;

    int getRegister(const std::string &s);
    bool resolveEqu(const std::string &name, Int128 &out);
    bool parseOperand(const std::string &tok, Operand &op);
    bool parseLine(const std::string &line, int lineNum);
    void computeAddresses();
    bool preprocessFile(const std::string &filename, const std::string &basePath, std::vector<std::string> &outLines);
};

#endif

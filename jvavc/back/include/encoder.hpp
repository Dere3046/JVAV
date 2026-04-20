#ifndef ENCODER_HPP
#define ENCODER_HPP
#include "parser.hpp"
#include <vector>
#include <map>
#include <string>

class Encoder {
public:
    bool encode(const std::vector<Instruction>& instructions,
                const std::map<std::string, int>& labels,
                std::vector<__int128>& bytecode);
    const std::string& getError() const { return error; }
private:
    std::string error;
    const std::map<std::string, int>* labelMap;
    std::vector<__int128>* output;

    void emit(__int128 word);
    bool encodeData(const Instruction& instr);
    bool encodeInstruction(const Instruction& instr);
};

#endif

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <set>
#include "parser.hpp"
#include "encoder.hpp"
using namespace std;

static int opCode(const string& mnem) {
    if (mnem=="HALT") return 0x00;
    if (mnem=="MOV")  return 0x01;
    if (mnem=="LDR")  return 0x02;
    if (mnem=="STR")  return 0x03;
    if (mnem=="ADD")  return 0x04;
    if (mnem=="SUB")  return 0x05;
    if (mnem=="MUL")  return 0x06;
    if (mnem=="DIV")  return 0x07;
    if (mnem=="CMP")  return 0x08;
    if (mnem=="JMP")  return 0x09;
    if (mnem=="JZ")   return 0x0A;
    if (mnem=="JNZ")  return 0x0B;
    if (mnem=="JE")   return 0x11;
    if (mnem=="JNE")  return 0x12;
    if (mnem=="JL")   return 0x13;
    if (mnem=="JG")   return 0x14;
    if (mnem=="JLE")  return 0x15;
    if (mnem=="JGE")  return 0x16;
    if (mnem=="PUSH") return 0x0C;
    if (mnem=="POP")  return 0x0D;
    if (mnem=="CALL") return 0x0E;
    if (mnem=="RET")  return 0x0F;
    if (mnem=="LDI")  return 0x10;
    return -1;
}

static __int128 buildInstr(uint8_t op, uint8_t dst, uint8_t src1, uint8_t src2, __int128 imm) {
    uint64_t lo = (uint64_t)imm;
    uint32_t hi = (uint32_t)(imm >> 64);
    return ((__int128)hi<<96)|((__int128)lo<<32)|((__int128)src2<<24)
          |((__int128)src1<<16)|((__int128)dst<<8)|op;
}

static int allocTempReg(const set<int>& used) {
    for (int i=7;i>=0;i--) if (used.find(i)==used.end()) return i;
    return -1;
}

void Encoder::emit(__int128 word) {
    output->push_back(word);
}

bool Encoder::encodeData(const Instruction& instr) {
    for (auto& item : instr.dataItems) emit(item.value);
    return true;
}

bool Encoder::encodeInstruction(const Instruction& instr) {
    int op = opCode(instr.mnemonic);
    if (op < 0) { error="Unknown opcode"; return false; }
    const auto& ops = instr.ops;

    // Expand label jumps (JMP/JZ/JNZ/CALL label)
    if ((op==0x09||op==0x0A||op==0x0B||op==0x0E||
         op==0x11||op==0x12||op==0x13||op==0x14||op==0x15||op==0x16) && ops.size()==1 && !ops[0].label.empty()) {
        auto it = labelMap->find(ops[0].label);
        if (it==labelMap->end()) { error="Undefined label: "+ops[0].label; return false; }
        int addr = it->second;
        int tr = allocTempReg({});
        if (tr<0) { error="No free register for jump"; return false; }
        emit(buildInstr(0x10, tr, 0x80, 0, addr));
        emit(buildInstr(op, tr, 0, 0, 0));
        return true;
    }

    // Expand memory operands [label] or [imm] for LDR/STR
    if ((op==0x02||op==0x03) && ops.size()==2) {
        int memIdx = -1, regIdx = -1;
        if (op==0x02 && ops[1].type==OP_MEM) { memIdx=1; regIdx=0; }
        else if (op==0x03 && ops[0].type==OP_MEM) { memIdx=0; regIdx=1; }
        if (memIdx >= 0) {
            // Direct register indirect: [Rs] — no expansion needed
            if (ops[memIdx].reg >= 0) {
                if (op==0x02) {
                    emit(buildInstr(0x02, (uint8_t)ops[regIdx].reg, (uint8_t)ops[memIdx].reg, 0, 0));
                } else {
                    emit(buildInstr(0x03, (uint8_t)ops[memIdx].reg, (uint8_t)ops[regIdx].reg, 0, 0));
                }
                return true;
            }
            __int128 addr = ops[memIdx].imm;
            if (!ops[memIdx].label.empty()) {
                auto it = labelMap->find(ops[memIdx].label);
                if (it==labelMap->end()) { error="Undefined label: "+ops[memIdx].label; return false; }
                addr = it->second;
            }
            int tr = allocTempReg({});
            if (tr<0) { error="No free register for mem address expansion"; return false; }
            emit(buildInstr(0x10, tr, 0x80, 0, addr));
            if (op==0x02) {
                emit(buildInstr(0x02, (uint8_t)ops[regIdx].reg, tr, 0, 0));
            } else {
                emit(buildInstr(0x03, tr, (uint8_t)ops[regIdx].reg, 0, 0));
            }
            return true;
        }
    }

    uint8_t dst=0, src1=0, src2=0;
    __int128 imm=0;

    if (op==0x00||op==0x0F) {
        // HALT / RET
    } else if (op==0x01||op==0x10) {
        // MOV / LDI
        if (ops.size()!=2) { error=instr.mnemonic+" requires 2 operands"; return false; }
        dst = ops[0].reg;
        if (ops[1].type==OP_REG) src1 = ops[1].reg;
        else {
            src1 = 0x80;
            imm = ops[1].imm;
            if (!ops[1].label.empty()) {
                auto it = labelMap->find(ops[1].label);
                if (it==labelMap->end()) { error="Undefined label: "+ops[1].label; return false; }
                imm = it->second;
            }
        }
    } else if (op==0x02) {
        if (ops.size()!=2) { error="LDR requires 2 operands"; return false; }
        dst = ops[0].reg; src1 = ops[1].reg;
    } else if (op==0x03) {
        if (ops.size()!=2) { error="STR requires 2 operands"; return false; }
        dst = ops[0].reg; src1 = ops[1].reg;
    } else if (op==0x08) {
        if (ops.size()!=2) { error="CMP requires 2 operands"; return false; }
        src1 = ops[0].reg; src2 = ops[1].reg;
    } else if (op==0x0C) {
        if (ops.size()!=1) { error="PUSH requires 1 operand"; return false; }
        src1 = ops[0].reg;
    } else if (op==0x0D) {
        if (ops.size()!=1) { error="POP requires 1 operand"; return false; }
        dst = ops[0].reg;
    } else if (op==0x09||op==0x0A||op==0x0B||op==0x0E) {
        if (ops.size()!=1||ops[0].type!=OP_REG) { error=instr.mnemonic+" requires register operand"; return false; }
        dst = ops[0].reg;
    } else if (op==0x04||op==0x05||op==0x06||op==0x07) {
        // ADD / SUB / MUL / DIV with possible immediate expansion
        if (ops.size()!=3) { error=instr.mnemonic+" requires 3 operands"; return false; }
        uint8_t dstReg = ops[0].reg;
        set<int> used;
        used.insert(dstReg);
        
        // Build expanded operands
        struct EOp { uint8_t type; uint8_t reg; __int128 imm; };
        EOp eops[3];
        for (int i=0;i<3;i++) {
            eops[i].type = (uint8_t)ops[i].type;
            eops[i].reg = ops[i].reg;
            eops[i].imm = ops[i].imm;
        }
        
        for (int idx=1; idx<3; idx++) {
            if (ops[idx].type==OP_IMM) {
                __int128 val = ops[idx].imm;
                if (!ops[idx].label.empty()) {
                    auto it = labelMap->find(ops[idx].label);
                    if (it==labelMap->end()) { error="Undefined label: "+ops[idx].label; return false; }
                    val = it->second;
                }
                int tr = allocTempReg(used);
                if (tr<0) { error="No free register for immediate"; return false; }
                emit(buildInstr(0x10, tr, 0x80, 0, val));
                eops[idx].type = OP_REG;
                eops[idx].reg = tr;
                used.insert(tr);
            }
        }
        
        dst = dstReg;
        src1 = eops[1].reg;
        src2 = eops[2].reg;
    } else {
        error="Unhandled opcode"; return false;
    }

    emit(buildInstr(op, dst, src1, src2, imm));
    return true;
}

bool Encoder::encode(const vector<Instruction>& instructions,
                     const map<string,int>& labels,
                     vector<__int128>& bytecode) {
    labelMap = &labels;
    output = &bytecode;
    bytecode.clear();
    for (const auto& instr : instructions) {
        if (instr.isPseudo) {
            if (!encodeData(instr)) return false;
            continue;
        }
        if (instr.mnemonic.empty() || instr.mnemonic == "#INCLUDE") continue;
        if (!encodeInstruction(instr)) {
            error += " at line " + to_string(instr.lineNum);
            return false;
        }
    }
    return true;
}

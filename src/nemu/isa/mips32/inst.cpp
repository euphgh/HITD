/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <nemu/cpu/cpu.hpp>
#include "easylogging++.h"
#include "local-include/reg.hpp"
#include <nemu/cpu/ifetch.hpp>
#include "isa-def.hpp"
#include "utils.hpp"
#include <csignal>


enum {
  TYPE_I, // signed extended imm
  TYPE_U, // zero extended imm
  TYPE_B, // branch instr
  TYPE_J, // only J and JAL
  TYPE_S, // only need src1(R(rs)) and rd
  TYPE_R, // three registers instr include JR and JALR
  TYPE_N, // none
};

#define src1R() do { *src1 = R(rs); } while (0)
#define src2R() do { *src2 = R(rt); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 15, 0), 16); } while(0)
#define immU() do { *imm = BITS(i, 15, 0); } while(0)
#define immB() do { *imm = SEXT(BITS(i, 15, 0), 16)<<2;} while(0)
#define immJ() do { *imm = BITS(i, 25, 0) << 2; } while(0)

#ifdef CONFIG_FTRACE
void mips32_CPU_state::check_link(int rs){
    uint8_t opcode = BITS(inst_state.inst,31,26);
    //NOTE:jal instr must link to 31
    if (opcode==0x3) SET_CALL(inst_state.flag);
    if (opcode==0x0) {
        uint8_t special = BITS(inst_state.inst,5,0);
        //NOTE:jalr instr may link to 31, must to 31 at func and perf
        if (special==0x9) SET_CALL(inst_state.flag);
        //NOTE:jr 31 instr
        if (special==0x8 && rs==31) SET_RET(inst_state.flag);
    }
}
#endif /* CONFIG_FTRACE */
void mips32_CPU_state::decode_operand(int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = inst_state.inst;
  int rt = BITS(i, 20, 16);
  int rs = BITS(i, 25, 21);
  *rd = (type == TYPE_U || type == TYPE_I) ? rt : BITS(i, 15, 11);
  switch (type) {
    case TYPE_J: 
        immJ();
        src1R();
        IFDEF(CONFIG_FTRACE,check_link(rs));
        break;
    case TYPE_I: src1R();immI(); break;
    case TYPE_U: src1R();immU(); break;
    case TYPE_R: src1R();src2R(); break;
    case TYPE_B: 
        src1R();
        immB(); 
        break;
  }
}

#define __INST_MULT__(is_signed) \
    MUXONE(is_signed, int64_t, uint64_t) a = (MUXONE(is_signed,signed,uint64_t))src1; \
    MUXONE(is_signed, int64_t, uint64_t) b = (MUXONE(is_signed,signed,uint64_t))src2; \
    MUXONE(is_signed, int64_t, uint64_t) res = a*b;\
    arch_state.hi = BITS(res,63,32); \
    arch_state.lo = BITS(res,31,0);

int mips32_CPU_state::decode_exec() {
  int rd = 0;
  inst_state.skip = false;
  word_t src1 = 0, src2 = 0, imm = 0;

#define INSTPAT_INST(s) ((inst_state).inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
    decode_operand(&rd, &src1, &src2, &imm, concat(TYPE_, type)); \
    __VA_ARGS__ ; \
}
#define EXPT(...) try { __VA_ARGS__; } catch (int e) {}
  INSTPAT_START();

  //R type 
  //      opcode  rs      rt      rd  
  //      6       5       5       5
  //      [31:26] [25:21] [20:16] [15:11] [10:6] [5:0]
  INSTPAT("000000 ?????   ?????   ?????   00000  100000", add    , R, EXPT(inst_add(rd, src1, src2)));
  INSTPAT("000000 ?????   ?????   ?????   00000  100001", addu   , R, Rw(rd, src1 + src2));
  INSTPAT("000000 ?????   ?????   ?????   00000  100010", sub    , R, EXPT(inst_add(rd, src1, (~src2)+1)));
  INSTPAT("000000 ?????   ?????   ?????   00000  100011", subu   , R, Rw(rd, src1 - src2));

  INSTPAT("000000 ?????   ?????   ?????   00000  100100", and    , R, Rw(rd, src1 & src2));
  INSTPAT("000000 ?????   ?????   ?????   00000  100101", or     , R, Rw(rd, src1 | src2));
  INSTPAT("000000 ?????   ?????   ?????   00000  100110", xor    , R, Rw(rd, src1 ^ src2));
  INSTPAT("000000 ?????   ?????   ?????   00000  100111", nor    , R, Rw(rd, ~(src1 | src2)));

  INSTPAT("000000 ?????   ?????   ?????   00000  101010", slt    , R, Rw(rd, (signed)src1 < (signed)src2));
  INSTPAT("000000 ?????   ?????   ?????   00000  101011", sltu   , R, Rw(rd, src1 < src2));
  INSTPAT("000000 00000   ?????   ?????   ?????  000000", sll    , R, Rw(rd, src2 << BITS(inst_state.inst, 10, 6)));
  INSTPAT("000000 00000   ?????   ?????   ?????  000010", srl    , R, Rw(rd, (unsigned)src2 >> BITS(inst_state.inst, 10, 6)));
  INSTPAT("000000 00000   ?????   ?????   ?????  000011", sra    , R, Rw(rd, (signed)src2 >> BITS(inst_state.inst, 10, 6)));
  INSTPAT("000000 ?????   ?????   ?????   00000  000100", sllv   , R, Rw(rd, src2 << BITS(src1, 4, 0)));
  INSTPAT("000000 ?????   ?????   ?????   00000  000110", srlv   , R, Rw(rd, (unsigned)src2 >> BITS(src1, 4, 0)));
  INSTPAT("000000 ?????   ?????   ?????   00000  000111", srav   , R, Rw(rd, (signed)src2 >> BITS(src1, 4, 0)));

  INSTPAT("000000 ?????   ?????   00000   00000  011000", mult   , R, hilo_valid=true;__INST_MULT__(1));
  INSTPAT("000000 ?????   ?????   00000   00000  011001", multu  , R, hilo_valid=true;__INST_MULT__(0));
  INSTPAT("000000 ?????   ?????   00000   00000  011010", div    , R, hilo_valid=true;arch_state.lo = (signed)src1/(signed)src2; arch_state.hi = (signed)src1%(signed)src2);
  INSTPAT("000000 ?????   ?????   00000   00000  011011", divu   , R, hilo_valid=true;arch_state.lo = src1/src2; arch_state.hi = src1%src2;);
  INSTPAT("000000 00000   00000   ?????   00000  010000", mfhi   , R, Rw(rd, arch_state.hi));
  INSTPAT("000000 00000   00000   ?????   00000  010010", mflo   , R, Rw(rd, arch_state.lo));
  
  //I type 
  //      opcode  rs      rt      imm
  //      6       5       5       16
  //      [31:26] [25:21] [20:16] [15:0]
  INSTPAT("001000 ?????   ?????   ????? ????? ??????", addi   , I, EXPT(inst_add(rd, src1, imm)));
  INSTPAT("001001 ?????   ?????   ????? ????? ??????", addui  , I, Rw(rd, src1 + imm));
  INSTPAT("001010 ?????   ?????   ????? ????? ??????", slti   , I, Rw(rd, (signed)src1 < (signed)imm));
  INSTPAT("001011 ?????   ?????   ????? ????? ??????", sltiu  , I, Rw(rd, (unsigned)src1 < (unsigned)imm));

  INSTPAT("100000 ?????   ?????   ????? ????? ??????", lb     , I, EXPT(Rw(rd, SEXT(Mr(src1 + imm, 1),8))));
  INSTPAT("100010 ?????   ?????   ????? ????? ??????", lwl    , I, EXPT(Rw(rd, inst_lwl(src1 + imm, R(rd)))));
  INSTPAT("100100 ?????   ?????   ????? ????? ??????", lbu    , I, EXPT(Rw(rd, BITS(Mr(src1 + imm, 1),7,0)))); 
  INSTPAT("100110 ?????   ?????   ????? ????? ??????", lwr    , I, EXPT(Rw(rd, inst_lwr(src1 + imm, R(rd)))));
  INSTPAT("100001 ?????   ?????   ????? ????? ??????", lh     , I, EXPT(Rw(rd, SEXT(Mr(align_check(src1+imm, 0x1, EC_AdEL), 2),16))));
  INSTPAT("100011 ?????   ?????   ????? ????? ??????", lw     , I, EXPT(Rw(rd, Mr(align_check(src1+imm, 0x3, EC_AdEL), 4)))); 
  INSTPAT("100101 ?????   ?????   ????? ????? ??????", lhu    , I, EXPT(Rw(rd, BITS(Mr(align_check(src1+imm, 0x1, EC_AdEL), 2),15,0))));

  INSTPAT("101000 ?????   ?????   ????? ????? ??????", sb     , I, EXPT(Mw(src1 + imm, 0x11, R(rd))));
  INSTPAT("101001 ?????   ?????   ????? ????? ??????", sh     , I, EXPT(Mw(align_check(src1+imm, 0x1, EC_AdES), 0x32, R(rd))));
  INSTPAT("101010 ?????   ?????   ????? ????? ??????", swl    , I, EXPT(inst_swl(src1 + imm, R(rd))));
  INSTPAT("101011 ?????   ?????   ????? ????? ??????", sw     , I, EXPT(Mw(align_check(src1+imm, 0x3, EC_AdES), 0xf4, R(rd))));
  INSTPAT("101110 ?????   ?????   ????? ????? ??????", swr    , I, EXPT(inst_swr(src1 + imm, R(rd))));
  
  //U type 
  //      opcode  rs      rt      imm
  //      6       5       5       16
  //      [31:26] [25:21] [20:16] [15:0]
  INSTPAT("001100 ?????   ?????   ????? ????? ??????", addi   , U, Rw(rd, imm & src1));
  INSTPAT("001101 ?????   ?????   ????? ????? ??????", ori    , U, Rw(rd, imm | src1));
  INSTPAT("001110 ?????   ?????   ????? ????? ??????", xori   , U, Rw(rd, imm ^ src1));
  INSTPAT("001111 00000   ?????   ????? ????? ??????", lui    , U, Rw(rd, imm << 16)); 
  INSTPAT("000000 ?????   00000   00000 00000 010001", mthi   , U, hilo_valid=true;arch_state.hi = src1);
  INSTPAT("000000 ?????   00000   00000 00000 010011", mtlo   , U, hilo_valid=true;arch_state.lo = src1);
  INSTPAT("010000 00000   ?????   ????? 00000 000???", mfc0   , U, inst_mfc0(imm, rd)); 
  INSTPAT("010000 00100   ?????   ????? 00000 000???", mtc0   , U, inst_mtc0(imm, rd));

  INSTPAT("000000 ?????   ?????   ????? ????? 001101", break  , N, EXPT(isa_raise_intr(EC_Bp,inst_state.pc)));
  INSTPAT("000000 ?????   ?????   ????? ????? 001100", syscall, N, EXPT(isa_raise_intr(EC_Sys,inst_state.pc)));
  INSTPAT("010000 10000   00000   00000 00000 011000", eret   , N, inst_eret());/*}}}*/

  //B type 
  //      opcode  rs      rt      imm
  //      6       5       5       16
  //      [31:26] [25:21] [20:16] [15:0]
  INSTPAT("000100 ?????   ?????   ????? ????? ??????", beq    , B, inst_branch(src1==R(BITS(inst_state.inst,20,16)), imm, inst_state.snpc));
  INSTPAT("000101 ?????   ?????   ????? ????? ??????", bne    , B, inst_branch(src1!=R(BITS(inst_state.inst,20,16)), imm, inst_state.snpc));
  INSTPAT("000001 ?????   00000   ????? ????? ??????", bltz   , B, inst_branch((signed)src1 <  0, imm, inst_state.snpc));
  INSTPAT("000001 ?????   10000   ????? ????? ??????", bltzal , B, inst_branch((signed)src1 <  0, imm, inst_state.snpc);Rw(31, inst_state.snpc + 4);log_pt->info("bltzal"));
  INSTPAT("000001 ?????   00001   ????? ????? ??????", bgez   , B, inst_branch((signed)src1 >= 0, imm, inst_state.snpc));
  INSTPAT("000001 ?????   10001   ????? ????? ??????", bgezal , B, inst_branch((signed)src1 >= 0, imm, inst_state.snpc);Rw(31, inst_state.snpc + 4);log_pt->info("bgezal"));
  INSTPAT("000111 ?????   00000   ????? ????? ??????", bgtz   , B, inst_branch((signed)src1 >  0, imm, inst_state.snpc));
  INSTPAT("000110 ?????   00000   ????? ????? ??????", blez   , B, inst_branch((signed)src1 <= 0, imm, inst_state.snpc));
  
  //J type 
  //      opcode  rs      0       rd 
  //      6       5       5       16
  //      [31:26] [25:21] [20:16] [15:11]
  INSTPAT("000010 ?????   ?????   ????? ????? ??????", j      , J, inst_jump((BITS(inst_state.pc, 31, 28)<<28) | imm));
  INSTPAT("000011 ?????   ?????   ????? ????? ??????", jal    , J, inst_jump((BITS(inst_state.pc, 31, 28)<<28) | imm); Rw(31, inst_state.snpc+4));
  INSTPAT("000000 ?????   00000   00000 00000 001000", jr     , J, inst_jump(src1));
  INSTPAT("000000 ?????   00000   ????? 00000 001001", jalr   , J, inst_jump(src1); Rw(rd, inst_state.snpc + 4));
  
#ifdef CONFIG_MIPS_RLS1
  // for uboot
  INSTPAT("011100 ?????   ?????   ????? 00000 000010", mul    , R, hilo_valid=false; Rw(rd, ((signed)src1 * (signed)src2)));
  INSTPAT("000000 ?????   ?????   ????? 00000 001011", movn   , R, Rw(rd, (src2!=0) ? src1 : R(rd)));
  INSTPAT("000000 ?????   ?????   ????? 00000 001010", movz   , R, Rw(rd, (src2==0) ? src1 : R(rd)));
  INSTPAT("101111 ?????   ?????   ????? ????? ??????", cache  , N, );
  INSTPAT("000000 00000   00000   00000 ????? 001111", sync   , N, );
  INSTPAT("000000 ?????   00000   00000 1???? 001000", jrhb   , J, inst_jump(src1));
  // for linux
  INSTPAT("010000 10000   00000   00000 00000 001000", tlbp   , N, tlbp());
  INSTPAT("010000 10000   00000   00000 00000 000001", tlbr   , N, tlbr());
  INSTPAT("010000 10000   00000   00000 00000 000010", tlbwi  , N, EXPT(tlbwi())); //TODO: machine exception
  INSTPAT("010000 10000   00000   00000 00000 000110", tlbwr  , N, EXPT(tlbwr())); //TODO: machine exception
  INSTPAT("110000 ?????   ?????   ????? ????? ??????", ll     , I, EXPT(Rw(rd, Mr(align_check(src1+imm, 0x3, EC_AdEL), 4)));arch_state.llbit=1);
  INSTPAT("111000 ?????   ?????   ????? ????? ??????", sc     , I, EXPT(inst_sc(rd, src1+imm)));
  INSTPAT("000000 ?????   ?????   ????? ????? 110110", tne    , R, EXPT(if (src1!=src2) isa_raise_intr(EC_Tr)));
  INSTPAT("011100 ?????   ?????   ????? 00000 100000", clz    , R, inst_clz(src1, rd));
  INSTPAT("110011 ?????   ?????   ????? ????? ??????", pref   , N, );
  INSTPAT("011100 ?????   ?????   00000 00000 000000", madd   , R, inst_madd(src1, src2));
#endif /* CONFIG_MIPS_RLS1 */
  INSTPAT("011??? ?????   ?????   ????? ?????  ??????", ri_011 , N, EXPT(isa_raise_intr(EC_RI, inst_state.pc)));
  INSTPAT("0101?? ?????   ?????   ????? ?????  ??????", ri_bl  , N, EXPT(isa_raise_intr(EC_RI, inst_state.pc)));
  // only func test need RI else is CpU
  INSTPAT("010001 01110   11111   00000 00011  100000", ri_ft  , N, EXPT(isa_raise_intr(MUXDEF(CONFIG_MIPS_RLS1,EC_CpU,EC_RI), inst_state.pc)));
  INSTPAT("010??? ?????   ?????   ????? ?????  ??????", ri_cop , N, EXPT(isa_raise_intr(EC_RI, inst_state.pc)));
  INSTPAT("100111 ?????   ?????   ????? ?????  ??????", ri_47  , N, EXPT(isa_raise_intr(EC_RI, inst_state.pc)));
  INSTPAT("10110? ?????   ?????   ????? ?????  ??????", ri_101 , N, EXPT(isa_raise_intr(EC_RI, inst_state.pc)));

  // INSTPAT("011100 ????? ????? ????? ????? 111111", sdbbp  , N, NEMUTRAP(inst_state.pc, R(2))); // R(2) is $v0;
  INSTPAT("?????? ????? ????? ????? ????? ??????", inv    , N, INV(inst_state.inst));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0
  inst_state.wdata = R(inst_state.wnum);
  return 0;
}

int mips32_CPU_state::isa_exec_once(bool has_int) {
    // TIMED_FUNC(isa_exec_once);
    inst_state.wnum = 0;
    inst_state.flag = 0;
    have_dised = false;

    inst_state.snpc = inst_state.pc = arch_state.pc;
    word_t this_pc = inst_state.snpc;
    inst_state.is_delay_slot = next_is_delay_slot;
    next_is_delay_slot = false;
    // if (this_pc==0x80100ad0) raise(SIGTRAP);
    try {
        inst_state.inst = vaddr_ifetch(align_check(inst_state.snpc,0x3,EC_AdEL), 4);
        inst_state.snpc += 4;
        inst_state.dnpc = inst_state.is_delay_slot ? delay_slot_npc : inst_state.snpc;
        if (has_int) (isa_raise_intr(EC_Int, arch_state.pc));
        decode_exec();
    } catch (int e) {}

    //TODO:check pc finish conditions
    // if (this_pc==0x9fc13178)
    //     analysis = !analysis;
    IFDEF(CONFIG_TEST_FUNC, if (this_pc==0xbfc00100) nemu_state.state = NEMU_END);
    IFDEF(CONFIG_TEST_PERF, if (this_pc==0xbfc00100) nemu_state.state = NEMU_END);
    arch_state.pc = inst_state.dnpc;
    extern uint32_t log_pc;
    log_pc = this_pc;
    return 0;
}

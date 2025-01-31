#ifndef __MYTRACE_H__
#define __MYTRACE_H__

#include <common.hpp>
#include "testbench/difftest/struct.hpp"
#include "easylogging++.h"

//NOTE:buf length must greater than 8*5 = 40
void hex_display(int len, word_t data, char* buf);

void read_mtrace(wen_t info, paddr_t addr, word_t value);
void write_mtrace(wen_t info, paddr_t addr, word_t value);

#include <map>
#include <stack>
#include <string>
void load_elf_info(const char* filename, 
        std::vector<std::tuple<word_t, word_t, std::string>>& func_list);
class ftracer {
    std::map<word_t, std::tuple<word_t, std::string>> start_addr_map;
    std::stack<word_t> fstack;
    el::Logger* log_pt;
    std::string unknow;
    const std::string& search(word_t addr);
    public:
    bool end_of(word_t addr, word_t& end_addr);
    ftracer(std::string elf_name, el::Logger* input_logger, word_t _start_pc);
    void push(word_t call_at, word_t call_to);
    bool pop(word_t ret_at, word_t ret_to);
    void clean_call_stack();
    std::string call_stack_info(word_t pc);
};
#endif

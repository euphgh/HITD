#include <string>
#include "Vmycpu_top__Dpi.h"
#include "testbench/difftest/api.hpp"
#include "testbench/dpic.hpp"

void dut_get_state(diff_state *mycpu){
    for (uint8_t i = 0; i < 32; i++) {
        mycpu->gpr[i] = dpi_regfile(i);
    }
    mycpu->pc = dpi_retirePC();
}

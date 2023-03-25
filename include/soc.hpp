#include "debug.hpp"
#include "paddr/paddr_interface.hpp"
#include "testbench/sim_state.hpp"
#include <map>
#include <utility>
#ifndef __SOC_HPP__
#define __SOC_HPP__

class dual_soc {/*{{{*/
    public:
        enum soc_who  { DUT = 0, REF = 1};
        dual_soc();
        inline PaddrTop* get_dut_soc(){ return ptop[DUT]; }
        inline PaddrTop* get_ref_soc(){ return ptop[REF]; }

        void tick();
        void set_switch(uint8_t value);
    private:
        PaddrTop*       ptop[2];
        PaddrConfreg*   pcfreg[2];
        Puart8250*      puart[2];
        bool has_confreg;
        void create_basic_soc(int test_code);
        void create_boot_soc(int test_code);
        void create_kernel_soc(int test_code);
        void check_uart_tx();
        void check_confreg_tx();
};/*}}}*/

class single_soc {/*{{{*/
    public:
        single_soc();
        inline PaddrTop* get_single_soc(){ return ptop; }
        inline void tick() { if(pcfreg) pcfreg->tick(); }
        inline void set_switch(uint8_t value) { if(pcfreg) pcfreg->set_switch(value); } 

    private:
        PaddrTop* ptop;
        PaddrConfreg *pcfreg;
        Puart8250 *puart;
        void create_basic_soc(int test_code);
        void create_boot_soc(int test_code);
        void create_kernel_soc(int test_code);
};/*}}}*/

#endif // !__SOC_HPP__

/*
 * microsim360 - 2050 Display roller information.
 *
 * Copyright 2022, Richard Cornwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdint.h>
#include "model2050.h"
#include "cpu.h"
#include "device.h"
#include "xlat.h"
#include "rollers2050.h"


uint64_t
add_parity(uint32_t data)
{
    uint64_t  result;

   result = ((uint64_t)odd_parity[data&0xff]) |
                         ((uint64_t)data & 0xff);
   result |= ((uint64_t)odd_parity[(data>>8)&0xff] << 9) |
                         (((uint64_t)data & 0xff00) << 9);
   result |= ((uint64_t)odd_parity[(data>>16)&0xff] << 18) |
                         (((uint64_t)data & 0xff0000) << 18);
   result |= ((uint64_t)odd_parity[(data>>24)&0xff] << 27) |
                         (((uint64_t)data & 0xff000000) << 27);
   return result;
}

uint64_t
add_parity_n_bit(uint32_t data, uint32_t mask)
{
    uint64_t result = data;
    if (odd_parity[data & mask] != 0) {
        result |= (uint64_t)(mask + 1);
    }
    return result;
}

uint64_t
roller_1(int position)
{
    uint64_t bits = position;

    switch(position) {
    case 0:
            if ((cpu_2050.CHCTL & 1) != 0) {   /* SIO */
                bits |= 1llu << 35;
            }
            if ((cpu_2050.CHCTL & 4) != 0) {   /* TIO */
                bits |= 1llu << 34;
            }
            if ((cpu_2050.CHCTL & 2) != 0) {   /* HIO */
                bits |= 1llu << 33;
            }
            if ((cpu_2050.CHCTL & 8) != 0) {   /* TCH */
                bits |= 1llu << 32;
            }
            if ((cpu_2050.CH & 4) != 0) {      /* CH */
                bits |= 1llu << 31;
            }
            if ((cpu_2050.CH & 2) != 0) {
                bits |= 1llu << 30;
            }
            if ((cpu_2050.CH & 1) != 0) {
                bits |= 1llu << 29;
            }
            /* Instruction reply - 7,8,9,10 */
            /* reply 6 */
            if ((cpu_2050.BCHI) != 0) {       /* BCHI */
                bits |= 1llu << 23;
            }
            /* Proceed on Intrrupt 4 */
            /* Time Out 3 */
            /* Time Check 2 */
            if ((cpu_2050.CHCTL & 1) != 0) {  /* Foul */
                bits |= 1llu << 24;
            }
            /* Blank */
            break;
    case 1:
            /* Buffer 1,2, 3 */
            bits = ((uint64_t)cpu_2050.IOSTAT[3]) << 18;
            bits |= ((uint64_t)ros_2050[cpu_2050.ROAR].row2 & 0xf) << 15;
            /* First cycle check */
            break;
    case 2:
            bits = add_parity_n_bit(cpu_2050.BFR1, 0xff) << 27;
            bits |= add_parity_n_bit(cpu_2050.BFR2, 0xff) << 18;
            /* Logs, gate status, reset */
            break;
    case 3:
            if ((cpu_2050.MPX_TAGS & CHAN_SEL_OUT) != 0) {
                bits |= 1llu << 35;
            }
            if ((cpu_2050.MPX_TI & CHAN_SEL_OUT) != 0) {
                bits |= 1llu << 34;
            }
            if ((cpu_2050.MPX_TI & CHAN_OPR_IN) != 0) {
                bits |= 1llu << 33;
            }
            if ((cpu_2050.MPX_TI & CHAN_SEL_OUT) != 0) {
                bits |= 1llu << 32;
            }
            if ((cpu_2050.MPX_TAGS & CHAN_SUP_OUT) != 0) {
                bits |= 1llu << 31;
            }
            if ((cpu_2050.MPX_TI & CHAN_REQ_IN) != 0) {
                bits |= 1llu << 30;
            }
            if ((cpu_2050.MPX_TAGS & CHAN_SRV_OUT) != 0) {
                bits |= 1llu << 29;
            }
            if ((cpu_2050.MPX_TAGS & CHAN_ADR_OUT) != 0) {
                bits |= 1llu << 28;
            }
            if ((cpu_2050.MPX_TAGS & CHAN_CMD_OUT) != 0) {
                bits |= 1llu << 27;
            }
            if ((cpu_2050.MPX_TI & CHAN_SRV_IN) != 0) {
                bits |= 1llu << 26;
            }
            if ((cpu_2050.MPX_TI & CHAN_ADR_IN) != 0) {
                bits |= 1llu << 25;
            }
            if ((cpu_2050.MPX_TI & CHAN_STA_IN) != 0) {
                bits |= 1llu << 24;
            }
            bits |= add_parity_n_bit(cpu_2050.BUS_OUT[3], 0xff) << 14;
            if ((cpu_2050.FLAGS_REG[3] & CHAN_PROG) != 0) {
                bits |= 1llu << 13;
            }
            if ((cpu_2050.FLAGS_REG[3] & CHAN_PROT) != 0) {
                bits |= 1llu << 12;
            }
            break;
    case 4:
            bits = ((uint64_t)ros_2050[cpu_2050.ROAR].ce) << 32;
            bits |= ((uint64_t)cpu_2050.ROUTINE[3]) << 27;
            /* Priority 2, 3, PCI */
            /* CC, DTC, UCW */
            bits |= ((uint64_t)cpu_2050.IBFULL) << 20;
            bits |= ((uint64_t)cpu_2050.polling[3]) << 19;
            /* Burst mode */
            bits |= ((uint64_t)cpu_2050.IOSTAT[3]) << 14;
            /* Data xfer control */
            /* cc reset control */
            break;
    case 5:
            break;
    case 6:
            break;
    case 7:
            break;
    }
    return bits;
}

uint64_t
roller_2(int position)
{
    uint64_t bits = position;

    switch(position) {
    case 0:
        bits = add_parity(cpu_2050.B_REG[cpu_2050.SEL_CHAN_SEL]);
        break;
    case 1:
        bits = add_parity(cpu_2050.C_REG[cpu_2050.SEL_CHAN_SEL]);
        break;
    case  2:
        bits = ((uint64_t)cpu_2050.GP_REG[cpu_2050.SEL_CHAN_SEL]) << 25;
        /* Count interlock */
        /* end record 1, 2, read interlock */
        /* B AC */
        /* LS ENABLE */
        bits |= ((uint64_t)cpu_2050.LS_FULL[cpu_2050.SEL_CHAN_SEL]) << 18;
        bits |= ((uint64_t)cpu_2050.B_FULL[cpu_2050.SEL_CHAN_SEL]) << 17;
        bits |= ((uint64_t)cpu_2050.C_FULL[cpu_2050.SEL_CHAN_SEL]) << 16;
        /* Read bkwd, op, ready */
        /* Write op, ready, if */
        /* Channel checks */
        break;
    case  3:
        bits = ((uint64_t)cpu_2050.CHPOS[cpu_2050.SEL_CHAN_SEL]) << 27;
        /* Cycle counter phase step */
        bits |= ((uint64_t)cpu_2050.CHCLK[cpu_2050.SEL_CHAN_SEL]) << 21;
        /* Clock step */
        /* LS REQ */
//        bits |= ((uint64_t)cpu_2050.PCI_REQ[cpu_2050.SEL_CHAN_SEL]) << 21;
        if ((cpu_2050.FLAGS_REG[cpu_2050.SEL_CHAN_SEL] & CHAN_PCI_FLAG) != 0) {
            bits |= 1llu << 18;
        }
        /* Priority */
        bits |= ((uint64_t)cpu_2050.CHREQ[cpu_2050.SEL_CHAN_SEL]) << 9;
        bits |= ((uint64_t)cpu_2050.IOSTAT[cpu_2050.SEL_CHAN_SEL]) << 4;
        /* Common chan, LS, Pri 1, Pri 2-3, PCI, INH RTE */
        break;
    case 4:
        /* POS REG TRG */
        /* INH RD STORE */
        /* A CLOCK */
        /* SP */
        /* INSN SCAN */
        /* CHAN IN USE */
            bits = ((uint64_t)cpu_2050.polling[cpu_2050.SEL_CHAN_SEL]) << 25;
        /* Poll irpt end */
        /* Insn inh */
        /* BC Ready */
        /* UA to BUS = 0 */
        /* U Sel adr out */
        /* Compare ==, != */
        bits = ((uint64_t)cpu_2050.GP_REG[cpu_2050.SEL_CHAN_SEL]) << 25;
        bits |= ((uint64_t)cpu_2050.FLAGS_REG[cpu_2050.SEL_CHAN_SEL] & 0xf8) << 12;
        /* Fin */
        /* First word */
        break;
    case 5:
        break;
    }
    return bits;
}

uint64_t
roller_3(int position)
{
    uint64_t bits;

    switch(position) {
    case 0:
        bits = add_parity(cpu_2050.L_REG);
        break;
    case 1:
        bits = add_parity(cpu_2050.R_REG);
        break;
    case  2:
        bits = add_parity(cpu_2050.M_REG);
        break;
    case  3:
        bits = add_parity(cpu_2050.H_REG);
        break;
    case 4:
        bits = add_parity(cpu_2050.SAR_REG) << 9;
        bits |= ((uint64_t)cpu_2050.BI_REG) << 4;
        bits |= (uint64_t)cpu_2050.BS_REG;
        break;
    case 5:
        bits = ((uint64_t)ros_2050[cpu_2050.ROAR].row3) << 9;
        bits |= ((uint64_t)ros_2050[cpu_2050.ROAR].row4) << 2;
        break;
    case 6:
        bits = ((uint64_t)(cpu_2050.break_in != 0)) << 35;
        bits |= ((uint64_t)cpu_2050.last_cycle) << 33;
    }
    return bits;
}

uint64_t
roller_4(int position)
{
    uint64_t bits = position;

    switch(position) {
    case 0:
        bits = ((uint64_t)ros_2050[cpu_2050.ROAR].row1) << 4;
        break;
    case 1:
        bits = ((uint64_t)ros_2050[cpu_2050.ROAR].row2) << 12;
        bits |= ((uint64_t)cpu_2050.mvfnc) << 7;
        bits |= ((uint64_t)cpu_2050.io_mvfnc) << 4;
        break;
   case  2:
        bits = ((uint64_t)cpu_2050.SYLS1) << 35;
        bits |= ((uint64_t)cpu_2050.REFETCH) << 34;
        bits |= ((uint64_t)cpu_2050.NROAR) << 18;
        bits |= ((uint64_t)cpu_2050.ILC) << 10;
        bits |= ((uint64_t)cpu_2050.CC) << 8;
        bits |= ((uint64_t)cpu_2050.PMASK) << 4;
        break;
    case 3:
        bits = ((uint64_t)cpu_2050.io_mode) << 35;
        bits |= add_parity_n_bit(cpu_2050.IO_REG, 0x3) << 32;
        bits |= ((uint64_t)timer_event) << 31;
        /* Cons irpt */
        bits |= add_parity_n_bit(cpu_2050.LB_REG, 0x3) << 27;
        bits |= add_parity_n_bit(cpu_2050.MB_REG, 0x3) << 24;
        bits |= add_parity_n_bit(cpu_2050.F_REG, 0xf) << 19;
        bits |= ((uint64_t)cpu_2050.Q_REG) << 18;
        bits |= ((uint64_t)cpu_2050.ED_STAT) << 15;
        bits |= ((uint64_t)cpu_2050.S_REG) << 9;
        bits |= ((uint64_t)cpu_2050.LSGNS) << 8;
        bits |= ((uint64_t)cpu_2050.RSGNS) << 8;
        /* carry */
        bits |= ((uint64_t)cpu_2050.CAR) << 7;
        /* rtl */
        bits |= ((uint64_t)cpu_2050.mem_state);
        break;
    case 4:
        bits = ((uint64_t)cpu_2050.LS) << 29;
        /* FN */
        /* LS */
        bits |= add_parity_n_bit(cpu_2050.J_REG, 0xf) << 22;
        bits |= add_parity_n_bit(cpu_2050.MD_REG, 0x7) << 18;
        bits |= ((uint64_t)cpu_2050.G1NEG) << 15;
        bits |= add_parity_n_bit(cpu_2050.G_REG, 0xf0) << 6;
        bits |= ((uint64_t)cpu_2050.G2NEG) << 9;
        bits |= add_parity_n_bit(cpu_2050.G_REG, 0x0f) << 4;
        break;
    case 5:
        /* Addr parity */
        /* carry */
        /* counters */
        /* Mover */
        /* LSAR */
        bits |= add_parity_n_bit(cpu_2050.LSA, 0x7) << 18;
        break;
    case 6:
        bits = ((uint64_t)cpu_2050.ROAR) << 18;
        break;
    case 7:
        bits = ((uint64_t)cpu_2050.PROAR) << 18;
        break;
    }
    return bits;
}

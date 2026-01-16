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
    }
    return bits;
}

uint64_t
roller_2(int position)
{
    uint64_t bits = position;

    switch(position) {
    case 0:
//        bits = cpu_2050.SAR_REG;
        break;
    case 1:
 //       bits = cpu_2050.BS_REG;
        break;
    case  2:
  //      bits = add_parity(cpu_2050.M_REG);
        break;
    case  3:
   //     bits = add_parity(cpu_2050.H_REG);
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
        bits |= ((uint64_t)cpu_2050.NROAR) << 20;
 // b & a bits
        bits |= ((uint64_t)cpu_2050.ILC) << 10;
        bits |= ((uint64_t)cpu_2050.CC) << 8;
        bits |= ((uint64_t)cpu_2050.PMASK) << 4;
        break;
    }
    return bits;
}
#if 0
    SET_INDICATOR8(&roller[0].disp[1].ind[0], &cpu_2050.CHCTL, 0, 0);
    SET_INDICATOR8(&roller[0].disp[1].ind[1], &cpu_2050.CHCTL, 2, 0);
    SET_INDICATOR8(&roller[0].disp[1].ind[2], &cpu_2050.break_in, 3, 0);
    SET_INDICATOR8(&roller[0].disp[1].ind[3], &cpu_2050.io_mode, 0, 0);
    SET_INDICATOR8(&roller[0].disp[1].ind[5], &cpu_2050.first_cycle, 0, 0);
    roller[1].rollers = roll;
    roller[1].pos.x = 150;
    roller[1].pos.y = 200;
    roller[1].sel = 0;
    roller[1].ystart = 8 * 25;
    roller[2].rollers = roll;
    roller[2].pos.x = 150;
    roller[2].pos.y = 300;
    roller[2].sel = 0;
    roller[2].ystart = 16 * 25;
    roller[3].rollers = roll;
    roller[3].pos.x = 150;
    roller[3].pos.y = 400;
    roller[3].sel = 0;
    roller[3].ystart = 24 * 25;
    SET_INDICATOR8(&roller[3].disp[2].ind[0], &cpu_2050.SYLS1, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[1], &cpu_2050.REFETCH, 0, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[6], &cpu_2050.NROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[7], &cpu_2050.NROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[8], &cpu_2050.NROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[9], &cpu_2050.NROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[10], &cpu_2050.NROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[11], &cpu_2050.NROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[12], &cpu_2050.NROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[13], &cpu_2050.NROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[14], &cpu_2050.NROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[15], &cpu_2050.NROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[16], &cpu_2050.NROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[17], &cpu_2050.NROAR, 0, 0);
    /* Bits 18-23 External interrupt */
    SET_INDICATOR8(&roller[3].disp[2].ind[24], &cpu_2050.ILC, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[25], &cpu_2050.ILC, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[26], &cpu_2050.CC, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[27], &cpu_2050.CC, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[28], &cpu_2050.PMASK, 3, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[29], &cpu_2050.PMASK, 2, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[30], &cpu_2050.PMASK, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[31], &cpu_2050.PMASK, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[0], &cpu_2050.io_mode, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[1], &cpu_2050.IO_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[2], &cpu_2050.IO_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[3], &cpu_2050.IO_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[4], NULL, 0, 0);   /* Timer Irpt */
    SET_INDICATOR8(&roller[3].disp[3].ind[5], NULL, 0, 0);   /* Cons Irpt */
    SET_INDICATOR8(&roller[3].disp[3].ind[6], &cpu_2050.LB_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[7], &cpu_2050.LB_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[8], &cpu_2050.LB_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[9], &cpu_2050.MB_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[10], &cpu_2050.MB_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[11], &cpu_2050.MB_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[12], &cpu_2050.F_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[3].ind[13], &cpu_2050.F_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[14], &cpu_2050.F_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[15], &cpu_2050.F_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[16], &cpu_2050.F_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[17], &cpu_2050.Q_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[18], &cpu_2050.ED_STAT, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[19], &cpu_2050.ED_STAT, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[20], &cpu_2050.S_REG, 7, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[21], &cpu_2050.S_REG, 6, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[22], &cpu_2050.S_REG, 5, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[23], &cpu_2050.S_REG, 4, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[24], &cpu_2050.S_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[25], &cpu_2050.S_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[26], &cpu_2050.S_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[27], &cpu_2050.S_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[28], &cpu_2050.LSGNS, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[29], &cpu_2050.RSGNS, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[30], &cpu_2050.CAR, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[31], NULL, 0, 0);  /* RTL */
    SET_INDICATOR8(&roller[3].disp[3].ind[32], &cpu_2050.mem_state, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[33], &cpu_2050.mem_state, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[34], &cpu_2050.mem_state, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[35], &cpu_2050.mem_state, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[0], &cpu_2050.LSA, 0, 0x3f);
    SET_INDICATOR8(&roller[3].disp[4].ind[1], &cpu_2050.LSA, 5, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[2], &cpu_2050.LSA, 4, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[3], &cpu_2050.LSA, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[4], &cpu_2050.LSA, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[5], &cpu_2050.LSA, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[6], &cpu_2050.LSA, 0, 0);
    SET_INDICATOR16(&roller[3].disp[4].ind[7], &cpu_2050.FN, 1, 0);
    SET_INDICATOR16(&roller[3].disp[4].ind[8], &cpu_2050.FN, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[9], &cpu_2050.J_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[4].ind[10], &cpu_2050.J_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[11], &cpu_2050.J_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[12], &cpu_2050.J_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[13], &cpu_2050.J_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[14], &cpu_2050.MD_REG, 0, 0x7);
    SET_INDICATOR8(&roller[3].disp[4].ind[15], &cpu_2050.MD_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[16], &cpu_2050.MD_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[17], &cpu_2050.MD_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[18], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[19], &cpu_2050.G1NEG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[20], &cpu_2050.G_REG, 0, 0xf0);
    SET_INDICATOR8(&roller[3].disp[4].ind[21], &cpu_2050.G_REG, 7, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[22], &cpu_2050.G_REG, 6, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[23], &cpu_2050.G_REG, 5, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[24], &cpu_2050.G_REG, 4, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[25], &cpu_2050.G2NEG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[26], &cpu_2050.G_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[4].ind[27], &cpu_2050.G_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[28], &cpu_2050.G_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[29], &cpu_2050.G_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[30], &cpu_2050.G_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[31], NULL, 0, 0);  /* RTL */
    SET_INDICATOR8(&roller[3].disp[4].ind[32], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[33], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[34], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[35], NULL, 0, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[6], &cpu_2050.ROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[7], &cpu_2050.ROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[8], &cpu_2050.ROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[9], &cpu_2050.ROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[10], &cpu_2050.ROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[11], &cpu_2050.ROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[12], &cpu_2050.ROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[13], &cpu_2050.ROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[14], &cpu_2050.ROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[15], &cpu_2050.ROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[16], &cpu_2050.ROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[17], &cpu_2050.ROAR, 0, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[6], &cpu_2050.PROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[7], &cpu_2050.PROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[8], &cpu_2050.PROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[9], &cpu_2050.PROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[10], &cpu_2050.PROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[11], &cpu_2050.PROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[12], &cpu_2050.PROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[13], &cpu_2050.PROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[14], &cpu_2050.PROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[15], &cpu_2050.PROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[16], &cpu_2050.PROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[17], &cpu_2050.PROAR, 0, 0);
    roller_ptr = 4;
    j = 150 + 35;
    for (i = 0; i < 36; i++) {
        SET_INDICATOR32(&roller[2].disp[0].ind[i], &cpu_2050.L_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[1].ind[i], &cpu_2050.R_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[2].ind[i], &cpu_2050.M_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[3].ind[i], &cpu_2050.H_REG, roller_shift[i], roller_mask[i]);
        if (i < 27) {
            SET_INDICATOR32(&roller[2].disp[4].ind[i], &cpu_2050.SAR_REG, roller_shift[i+8], roller_mask[i+8]);
        } else if (i > 28 && i < 32) {
            SET_INDICATOR8(&roller[2].disp[4].ind[i], &cpu_2050.BS_REG, 3 - (i - 29), 0);
        }
        if (i < 28) {  /* CE - UX */
            SET_INDICATOR32(&roller[2].disp[5].ind[i], &cpu_2050.ros_row3, 28 - i, 0);
        } else if (i < 34) {  /* SS */
            SET_INDICATOR32(&roller[2].disp[5].ind[i], &cpu_2050.ros_row4, 32 - i, 0);
        }
        if (i < 30) { /* LU - ZN, skip 1, TR-SF */
            SET_INDICATOR32(&roller[3].disp[0].ind[i], &cpu_2050.ros_row1, 32 - i, 0);
        }
        if (i < 28) {  /* IV - UR */
            SET_INDICATOR32(&roller[3].disp[1].ind[i], &cpu_2050.ros_row2, 30 - i, 0);
        }
        if (i > 28 && i < 30) {
            SET_INDICATOR8(&roller[3].disp[1].ind[i], &cpu_2050.mvfnc, 30 - i, 0);
        }
        if (i > 30 && i < 31) {
            SET_INDICATOR8(&roller[3].disp[1].ind[i], &cpu_2050.io_mvfnc, 30 - i, 0);
        }
        j += lamp_offset[i];
    //    sprintf(buffer, "%d", i);
     //   ADD_LABEL(j, (h2 * 20), f1_wd*2, buffer, c1, cc);
    }
#endif

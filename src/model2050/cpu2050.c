/*
 * microsim360 - Model 2050 CPU.
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

#include "device.h"
#include "model2050.h"

uint32_t    BS_MASK[16] = {
    0x00000000,  /* 0 */
    0x000000FF,  /* 1 */
    0x0000FF00,  /* 2 */
    0x0000FFFF,  /* 3 */
    0x00FF0000,  /* 4 */
    0x00FF00FF,  /* 5 */
    0x00FFFF00,  /* 6 */
    0x00FFFFFF,  /* 7 */
    0xFF000000,  /* 8 */
    0xFF0000FF,  /* 9 */
    0xFF00FF00,  /* a */
    0xFF00FFFF,  /* b */
    0xFFFF0000,  /* c */
    0xFFFF00FF,  /* d */
    0xFFFFFF00,  /* e */
    0xFFFFFFFF   /* f */
};

#define IVD  0x140   /* Invalid digit address */
#define STPR 0x142   /* Storage protection */
#define IVAD 0x1c0   /* Invalid oprnd address */
#define SPEC 0x1c2   /* Specification voilation */

struct CPU_2050 cpu_2050;

void
cycle_2050()
{
    uint16_t         roar;
    struct ROS_2050  *sal;
    int              a_bit;
    int              b_bit;
    int              carry_in;
    int              carry_out;
    int              init_mem;          /* Start Main memory cycle */
    uint8_t          g_update;
    uint8_t          s_update;
    uint8_t          f_update;
    uint8_t          q_update;
    uint32_t         l_update;
    uint32_t         r_update;
    uint32_t         m_update;
    uint32_t         carries;
    uint32_t         t1, t2, t3;
    int              t;

    /* Handle front paenl switch */
    if (CHECK_RST) {
    }

    if (INTR) {
    }

    if (START) {
    }

    if (STOP) {
    }

    if (LOAD) {
        printf("Load\n");
        load_mode = 1;
        SYS_RST = 1;
    }

    if (SET_IC) {
        printf("Set IC\n");
        cpu_2050.OPPANEL = 0x2;
        SET_IC = 0;
    }

    if (SYS_RST) {
        cpu_2050.WX = 0x242;
        if (load_mode)
             cpu_2050.WX = 0x240;
    }

    if (DISPLAY) {
        cpu_2050.OPPANEL = 0x8;
        DISPLAY = 0;
    }

    if (STORE) {
        cpu_2050.OPPANEL = 0x9;
        STORE = 0;
    }

    sal = &ros_2050[cpu_2050.WX];
    cpu_2050.ros_row1 = sal->row1;
    cpu_2050.ros_row2 = sal->row2;
    cpu_2050.ros_row3 = sal->row3;
    cpu_2050.ros_row4 = sal->row4;
    g_update = cpu_2050.G_REG;
    s_update = cpu_2050.S_REG;
    l_update = cpu_2050.L_REG;
    r_update = cpu_2050.R_REG;
    m_update = cpu_2050.M_REG;
    f_update = cpu_2050.F_REG;
    q_update = cpu_2050.Q_REG;
    init_mem = 0;


    /* Set up L and R inputs */
    switch (sal->ry) {
    case 0: /*  0  */
            cpu_2050.right_bus = 0;
            break;
    case 1: /*  R  */
            cpu_2050.right_bus = cpu_2050.R_REG;
            break;
    case 2: /*  M  */
            cpu_2050.right_bus = cpu_2050.M_REG;
            break;
    case 3: /*  M23 */
            cpu_2050.right_bus = cpu_2050.M_REG & 0xffff;
            break;
    case 4: /*  H  */
            cpu_2050.right_bus = cpu_2050.H_REG;
            break;
    case 5: /*  SEMT  */
            cpu_2050.right_bus = 0;
            break;
    case 6: /*  Unknown  */
            cpu_2050.right_bus = 0;
            break;
    case 7: /*  Unknown  */
            cpu_2050.right_bus = 0;
            break;
    }

    switch (sal->lx) {
    case 0: /*  0  */
            cpu_2050.left_bus = 0;
            break;
    case 1: /*  L  */
            cpu_2050.left_bus = cpu_2050.L_REG;
            break;
    case 2: /*  SGN  */
            cpu_2050.left_bus = 0x80000000;
            break;
    case 3: /*  E  CE bit < 1 */
            cpu_2050.left_bus = sal->ce << 1;
            break;
    case 4: /*  LRL  */
            cpu_2050.left_bus = (cpu_2050.L_REG & 0xffff) << 16;
            break;
    case 5: /*  LWA  */
            cpu_2050.left_bus = cpu_2050.L_REG | 3;
            cpu_2050.right_bus = cpu_2050.right_bus | 3;
            break;
    case 6: /*  4  */
            cpu_2050.left_bus = 4;
            break;
    case 7: /*  64C  */
            cpu_2050.left_bus = 0;
            break;
    }

    if (sal->ss == 42) {
       cpu_2050.left_bus |= BS_MASK[cpu_2050.BS_REG];
    }

    if (!sal->tc)
        cpu_2050.left_bus ^= 0xffffffff;

    if (cpu_2050.io_mode) {
        /* Set up Mover left and right inputs */
        switch (sal->lu) {
        case 0:  /* 0 */
                cpu_2050.u_bus = 0;
                break;
        case 1:  /* MD,F */
                cpu_2050.u_bus = (cpu_2050.MD_REG << 4) | cpu_2050.F_REG;
                break;
        case 2:  /* R3 */
                cpu_2050.u_bus = cpu_2050.R_REG & 0xff;
                break;
        case 3:  /* BIB->U */
                /* Gate multiplexer input to LU bus */
                cpu_2050.u_bus = 0;
                break;
        case 4:  /* LU0 */
                cpu_2050.u_bus = (cpu_2050.L_REG >> 24) & 0xff;
                break;
        case 5:  /* LU1 */
                cpu_2050.u_bus = (cpu_2050.L_REG >> 16) & 0xff;
                break;
        case 6:  /* LU2 */
                cpu_2050.u_bus = (cpu_2050.L_REG >> 8) & 0xff;
                break;
        case 7:  /* LU3 */
                cpu_2050.u_bus = (cpu_2050.L_REG ) & 0xff;
                break;
        }

        /* Gate data to V inputs */
        switch (sal->mv) {
        case 0:   /* 0 */
                cpu_2050.v_bus = 0;
                break;
        case 1:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        case 2:   /* BIB->V Gate multiplexer buffer in to V buss */
                cpu_2050.v_bus = 0;
                break;
        case 3:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        }
    } else {
        /* Set up Mover left and right inputs */
        switch (sal->lu) {
        case 0:  /* 0 */
                cpu_2050.u_bus = 0;
                break;
        case 1:  /* MD,F */
                cpu_2050.u_bus = (cpu_2050.MD_REG << 4) | cpu_2050.F_REG;
                break;
        case 2:  /* R3 */
                cpu_2050.u_bus = cpu_2050.R_REG & 0xff;
                break;
        case 3:  /* DCI->U Gate direct input to lu bus */
                cpu_2050.u_bus = 0;
                break;
        case 4:  /* XTR  Gate external interrupt register to lu bus */
                cpu_2050.u_bus = 0;
                break;
        case 5:  /* PSW4 ILC,CC, Progmask */
                cpu_2050.u_bus = (cpu_2050.ILC << 6) | (cpu_2050.CC << 4) |
                                  (cpu_2050.PM);
                break;
        case 6:  /* LMB */
                cpu_2050.u_bus = (cpu_2050.L_REG >> (cpu_2050.MB_REG * 8)) & 0xff;
                break;
        case 7:  /* LLB */
                cpu_2050.u_bus = (cpu_2050.L_REG >> (cpu_2050.LB_REG * 8)) & 0xff;
                break;
        }

        /* Gate data to V inputs */
        switch (sal->mv) {
        case 0:   /* 0 */
                cpu_2050.v_bus = 0;
                break;
        case 1:   /* MLB */
                cpu_2050.v_bus = (cpu_2050.M_REG >> (cpu_2050.LB_REG * 8)) & 0xff;
                break;
        case 2:   /* MMB */
                cpu_2050.v_bus = (cpu_2050.M_REG >> (cpu_2050.MB_REG * 8)) & 0xff;
                break;
        case 3:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        }
    }


    /* Do mover function left */
    switch (sal->ul) {
    case 0:  /* E->WR */
            cpu_2050.w_bus |= sal->ce;
            if (sal->wm == 12) {
                /* Fix based on ASCII flag setting */
            }
            break;
    case 1:  /* U */
            cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
            break;
    case 2:  /* V */
            cpu_2050.w_bus |= cpu_2050.v_bus & 0x0f;
            break;
    case 3:  /* ? */
            switch (cpu_2050.mvfnc) {
            case 0: /* Cross */
                    cpu_2050.w_bus = (cpu_2050.u_bus & 0xf0) >> 4;
                    break;
            case 1: /* Or */
                    cpu_2050.w_bus = (cpu_2050.v_bus | cpu_2050.u_bus) & 0x0f;
                    break;
            case 2: /* And  */
                    cpu_2050.w_bus = (cpu_2050.v_bus & cpu_2050.u_bus) & 0x0f;
                    break;
            case 3: /* Xor */
                    cpu_2050.w_bus = (cpu_2050.v_bus ^ cpu_2050.u_bus) & 0x0f;
                    break;
            case 4: /* char */
                    cpu_2050.w_bus = cpu_2050.u_bus & 0x0f;
                    break;
            case 5: /* Numeric */
                    cpu_2050.w_bus = cpu_2050.v_bus & 0x0f;
                    break;
            case 6: /* Zone */
                    cpu_2050.w_bus = cpu_2050.u_bus & 0x0f;
                    break;
            case 7:  /* Unknown */
                    break;
            }
            break;
    }

    /* Do mover function right */
    switch (sal->ur) {
    case 0:  /* E->WL */
            cpu_2050.w_bus = (sal->ce << 4);
            if (sal->wm == 12) {
                /* Fix based on ASCII flag setting */
            }
            break;
    case 1:  /* U */
            cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
            break;
    case 2:  /* V */
            cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
            break;
    case 3:  /* ? */
            switch (cpu_2050.mvfnc) {
            case 0: /* Cross */
                    cpu_2050.w_bus = (cpu_2050.u_bus & 0x0f) << 4;
                    break;
            case 1: /* Or */
                    cpu_2050.w_bus = (cpu_2050.v_bus | cpu_2050.u_bus) & 0xf0;
                    break;
            case 2: /* And  */
                    cpu_2050.w_bus = (cpu_2050.v_bus & cpu_2050.u_bus) & 0xf0;
                    break;
            case 3: /* Xor */
                    cpu_2050.w_bus = (cpu_2050.v_bus ^ cpu_2050.u_bus) & 0xf0;
                    break;
            case 4: /* char */
                    cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                    break;
            case 5: /* Numeric */
                    cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                    break;
            case 6: /* Zone */
                    cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
                    break;
            case 7:  /* Unknown */
                    break;
            }
            break;
    }

    /* Instruction address register */
    switch (sal->iv) {
    case 0:  /* Nop */
            break;
    case 1:  /* WL->IVD, trap if mover output bits 0-3 greater 9 */
            if ((cpu_2050.w_bus & 0xf0) < 0x90) {
                roar = IVD;
            }
            break;
    case 2:  /* WR->IVD, trap if mover output bits 4-7 greater 9 */
            if ((cpu_2050.w_bus & 0x0f) < 0x09) {
                roar = IVD;
            }
            break;
    case 3:  /* W->IVD, trap if mover output bits 0-3 or 4-7 greater 9 */
            if ((cpu_2050.w_bus & 0xf0) < 0x90 || (cpu_2050.w_bus & 0x0f) < 0x09) {
                roar = IVD;
            }
            break;
    case 4:  /* IA/4->A,IA, initiate storage read, inhibit invalid address, set flag */
            if (cpu_2050.mem_state != W1) {
                goto channel;
            }
            if (cpu_2050.IA_REG & 1) {
                cpu_2050.IVA = 1;
            } else {
                cpu_2050.IA_REG += 4;
                cpu_2050.SAR_REG = cpu_2050.IA_REG;
                init_mem = 1;
            }
            break;
    case 5:  /* IA+2/4 based on ILC if ILC == 0 or 1, IAR+= 2, ILC = 2 or 3, IAR+=4 */
            if (cpu_2050.ILC < 2)
                cpu_2050.IA_REG += 2;
            else
                cpu_2050.IA_REG += 4;
            break;
    case 6:  /* IA+2 */
            cpu_2050.IA_REG += 2;
            break;
    case 7:  /* IA+0/2->A */
            if (cpu_2050.mem_state != W1) {
                goto channel;
            }
            if (sal->zn == 1) {
               if (cpu_2050.REFETCH == 0 && (cpu_2050.IA_REG & 0x3) != 0)
                   break;
            }
            if (cpu_2050.REFETCH) {
                cpu_2050.SAR_REG = cpu_2050.IA_REG;
            } else {
                cpu_2050.SAR_REG = (cpu_2050.IA_REG + 2) & ~0x3;
            }
            init_mem = 1;
            break;
    }


    /* Compute carry into adder */
    g_update = cpu_2050.G_REG;
    switch (sal->dg) {
    case 0: /*  */
            carry_in = 0;
            break;
    case 1: /* CSTAT->ADDER */
            carry_in = cpu_2050.CSTAT_REG;
            break;
    case 2: /* HOT1->ADDER */
            carry_in = 1;
            break;
    case 3: /* G1-1  */
            if ((cpu_2050.G_REG & 0xf0) == 0) {
                cpu_2050.G1NEG = 1;
            } else {
                cpu_2050.G1NEG = 0;
                g_update = cpu_2050.G_REG - 0x10;
            } 
            carry_in = 0;
            break;
    case 4: /* HOT1,G-1 */
            if (cpu_2050.G_REG == 0) {
                cpu_2050.G1NEG = 1;
            } else {
                cpu_2050.G1NEG = 0;
                g_update = cpu_2050.G_REG - 1;
            } 
            carry_in = 1;
            break;
    case 5: /* G2-1 */
            if ((cpu_2050.G_REG & 0x0f) == 0) {
                cpu_2050.G2NEG = 1;
            } else {
                cpu_2050.G2NEG = 0;
                g_update = ((cpu_2050.G_REG & 0x0f) - 0x1) | (cpu_2050.G_REG & 0xf0);
            } 
            carry_in = 0;
            break;
    case 6: /* G-1 */
            if (cpu_2050.G_REG == 0) {
                cpu_2050.G1NEG = 1;
            } else {
                cpu_2050.G1NEG = 0;
                g_update = cpu_2050.G_REG - 1;
            } 
            carry_in = 0;
            break;
    case 7: /* G1,2-1 */
            if ((cpu_2050.G_REG & 0xf0) == 0) {
                cpu_2050.G1NEG = 1;
            } else {
                cpu_2050.G1NEG = 0;
                g_update = cpu_2050.G_REG - 0x10;
            } 
            if ((cpu_2050.G_REG & 0x0f) == 0) {
                cpu_2050.G2NEG = 1;
            } else {
                cpu_2050.G2NEG = 0;
                g_update = ((cpu_2050.G_REG & 0x0f) - 0x1) | (g_update & 0xf0);
            } 
            carry_in = 0;
            break;
    }

    switch (sal->ad) {
    case 0:  /* Nop */
    case 1:  /* Default */
    case 3:  /* Unknown */
    case 13:
    case 14:
    case 15:
            carry_in = 0;
            break;
    case 2:  /* BCF0 */
            if (cpu_2050.F_REG == 0)
                carry_in = 1;
            break;
    case 4:  /* BC0 */
    case 5: /* BC^C */
    case 6: /* BC1B */
    case 7: /* BC8 */
    case 8: /* DHL */
    case 11: /* DHH */
            break;
    case 9: /* DC0 */
    case 10: /* DDC0 */
    case 12: /* DCBS */
            carry_in = (cpu_2050.S_REG & BIT1) != 0;
            break;
    }


    /* Do adder, update output */
    cpu_2050.alu_out = cpu_2050.left_bus + cpu_2050.right_bus + carry_in;
    carries = (cpu_2050.left_bus & cpu_2050.right_bus) | 
                 ((cpu_2050.left_bus ^ cpu_2050.right_bus) & ~cpu_2050.alu_out);
    carry_out = 0;
   
    switch (sal->ad) {
    case 0:  /* Nop */
    case 1:  /* Default */
    case 2:  /* BCF0 */
    case 3:  /* Unknown */
            break;
    case 4:  /* BC0 */ /* Set carry based out sum out */
            carry_out = ((carries & 0x80000000) != 0);
            break;
    case 5: /* BC^C */  /* Set carry based on xor of carry from 0 and 1 */
            carry_out = ((carries ^ (carries << 1)) & 0x80000000) != 0;
            break;
    case 6: /* BC1B */ /* Set carry out from position 1 */
                       /* Block carry between 8 and 7 */
    case 7: /* BC8 */  /* Same */
            if (sal->al == 23) {
                t1 = (cpu_2050.left_bus & 0xff000000) + (cpu_2050.right_bus & 0xff000000) + 0x010000000;
            } else {
                t1 = (cpu_2050.left_bus & 0xff000000) + (cpu_2050.right_bus & 0xff000000);
            }
            t2 = (cpu_2050.left_bus & 0x00ffffff) + (cpu_2050.right_bus & 0x00ffffff) + carry_in;
            cpu_2050.alu_out =  (t1 & 0xff000000) + (t2 & 0x00ffffff);
            carries = (cpu_2050.left_bus & cpu_2050.right_bus) | ((cpu_2050.left_bus ^ cpu_2050.right_bus) & ~t1);
            carry_out = (carries & 0x80000000) != 0;
            break;

    case 8: /* DHL */ /* If out bit 2 is set, set L right one digit to 6. Left 
                         digit of L set to 6 if aux carry set */
            t1 = 0x22222222 & cpu_2050.alu_out;
            l_update = (t1 >> 4) || (t1 >> 5);
            if (cpu_2050.AUX_REG)
                l_update |= 0x60000000;
            break;
    case 9: /* DC0 */ /* Set L to 6 if carry out of sum between digits */
            t1 = 0x88888888 & ~carries;
            l_update = ((t1 >> 2) | (t1 >> 3));
            if ((carries & 0x800000000) != 0) {
                s_update |= BIT1;
            } else {
                s_update &= ~BIT1;
            }
            break;
    case 10: /* DDC0 */ /* Set L to 6 if sum out is greater then 6. */
            t1 = (cpu_2050.alu_out << 1) & (cpu_2050.alu_out << 2) & 0x88888888;
            t1 |= (cpu_2050.alu_out & 0x88888888);
            l_update = ((t1 >> 1) | (t1 >> 2));
            if ((t3 & 0x20000000) != 0) {
                s_update |= BIT1;
            } else {
                s_update &= ~BIT1;
            }
            break;
    case 11: /* DHH */ /* Same as DC0, Aux carry set to right most digit bit 2 */
            t1 = 0x22222222 & cpu_2050.alu_out;
            l_update = (t1 >> 4) || (t1 >> 5);
            cpu_2050.AUX_REG = (t1 & 0x2) != 0;
            break;
    case 12: /* DCBS */
            t1 = 0x88888888 & ~carries;
            l_update = ((t1 >> 2) | (t1 >> 3));
            t1 = 0x80000000;
            for (t2 = 0x8; (cpu_2050.BS_REG & t2) == 0 && t2 != 0; t2 >>= 1 ) {
                t1 >>= 8;
            }
            s_update &= ~BIT1;
            if (carries & t1) {
               s_update |= BIT1;
            }
            break;
    case 13:
            break;
    case 14:
            break;
    case 15:
            break;
    }
    cpu_2050.alu_out &= 0xffffffff;

    /* Do adder shifter */
    switch (sal->al) {
    case 0: /* Normal */
            cpu_2050.aob_latch = cpu_2050.alu_out;
            break;
    case 1: /* Q->SR1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | (cpu_2050.Q_REG << 31);
            f_update = ((cpu_2050.alu_out & 1) << 3) | (cpu_2050.F_REG >> 1);
            break;
    case 2: /* L0->-S4-> */
            t1 = ((cpu_2050.S_REG & BIT4) == 0) ? 0x80000000 : 0;
            cpu_2050.aob_latch = t1 | (cpu_2050.L_REG & 0x7f000000) | 
                           (cpu_2050.alu_out & 0xffffff);
            break;
    case 3: /* +SGN-> */
            cpu_2050.aob_latch = cpu_2050.alu_out & 0x7fffffff;
            break;
    case 4: /* -SGN-> */
            cpu_2050.aob_latch = cpu_2050.alu_out | 0x80000000;
            break;
    case 5: /* L0,S4-> */
            t1 = ((cpu_2050.S_REG & BIT4) != 0) ? 0x80000000 : 0;
            cpu_2050.aob_latch = t1 | (cpu_2050.L_REG & 0x7f000000) | 
                           (cpu_2050.alu_out & 0xffffff);
            break;
    case 6: /* IA->H */
            cpu_2050.H_REG = (cpu_2050.H_REG & 0xff000000) | (cpu_2050.IA_REG & 0xffffff);
            cpu_2050.aob_latch = cpu_2050.alu_out;
            break;
    case 7: /* Q->SL->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            f_update = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            f_update ^= 0x1;
            break;
    case 8: /* Q->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            f_update = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            break;
    case 9: /* F->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.F_REG);
            f_update = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            break;
    case 10: /* SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1);
            q_update = ((cpu_2050.alu_out & 0x80000000) != 0);
            break;
    case 11: /* Q->SL1 */
            cpu_2050.aob_latch = cpu_2050.alu_out << 1 | (cpu_2050.Q_REG);
            break;
    case 12: /* SR1->F */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            f_update = ((cpu_2050.alu_out & 1) << 3) | (cpu_2050.F_REG >> 1);
            break;
    case 13: /* SR1->Q */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            q_update = cpu_2050.alu_out & 1;
            break;
    case 14: /* Q->SR1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | ((cpu_2050.Q_REG) << 31);
            q_update = (cpu_2050.alu_out & 1);
            break;
    case 15: /* F->SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | ((cpu_2050.F_REG >> 3) & 1);
            f_update = (cpu_2050.F_REG << 1);
            q_update = ((cpu_2050.alu_out & 0x80000000) != 0);
            break;
    case 16: /* SL4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 4);
            f_update = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 17: /* F->SL4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 4) | (cpu_2050.F_REG);
            f_update = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 18: /* FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) | (cpu_2050.alu_out & 0xff000000);
            break;
    case 19: /* F->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) | (cpu_2050.alu_out & 0xff000000) |
                                     (cpu_2050.F_REG);
            break;
    case 20: /* SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            f_update = (cpu_2050.alu_out & 0xf);
            break;
    case 21: /* F->SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) | (cpu_2050.F_REG << 28);
            f_update = (cpu_2050.alu_out & 0xf);
            break;
    case 22: /* FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out >> 4) & 0xffffff) | (cpu_2050.alu_out & 0xff000000);
            f_update = (cpu_2050.alu_out & 0xf);
            break;
    case 23: /* 1->FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out >> 4) & 0x0fffff) | (cpu_2050.alu_out & 0xff000000) |
                                  0x100000;
            f_update = (cpu_2050.alu_out & 0xf);
            break;
    case 24: /* SR4->H */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            cpu_2050.H_REG = (cpu_2050.alu_out & 0xF0000000) | (cpu_2050.H_REG & 0xfffffff);
            r_update = ((cpu_2050.aob_latch & 0x0F000000) << 4) | (cpu_2050.R_REG & 0xfffffff);
            break;
    case 25: /* F->SR4 */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) | (cpu_2050.F_REG << 28);
            break;
    case 26: /* E->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) | (cpu_2050.alu_out & 0xff000000) 
                                  | (sal->ce & 0xf);
            break;
    case 27: /* F->SR1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | ((cpu_2050.F_REG & 0x1) << 31);
            q_update = cpu_2050.alu_out & 1;
            break;
    case 28: /* DKEY-> */
            cpu_2050.aob_latch = cpu_2050.DKEYS;
            break;
    case 29: /* CH, gate selector channels to latch  */
            break;
    case 30: /* D-> gate storage to latch, hold off it not ready */
            if (cpu_2050.mem_state != R2)
                 goto channel;
            cpu_2050.aob_latch = cpu_2050.SDR_REG;
            break;
    case 31: /* AKEY-> */
            cpu_2050.aob_latch = cpu_2050.AKEYS;
            break;
    }

    /* Preform State function */
    /* Depends on u_bus, w_bus, alu_out, aob_latch */
    switch (sal->ss) {
    case 0: /* NOP */
            break;
    case 1: /* undefined */
            break;
    case 2: /* undefined */
            break;
    case 3: /* D->CR*BS */
            /* Read main store, do test and set */
            if (cpu_2050.mem_state != R2)
                 goto channel;
            if (((cpu_2050.SDR_REG & BS_MASK[cpu_2050.BS_REG]) & 0x80808080) != 0)
                cpu_2050.CC = 1;
            else
                cpu_2050.CC = 0;
            break;
    case 4: /* E->SCHANCTL */
            break;
    case 5: /* L,RSGNS */
            /*  If U < 01010 force invalid digit trap, 
             *  if B or D turn on L sign, invert Rsign,
             *  else clear L sign */
            t = cpu_2050.u_bus & 0xf;
            if (t < 0xa) {
               /* Invalid trap */
            }  else if (t == 0xB || t == 0xD) {
               cpu_2050.LSGNS = 1;
               cpu_2050.RSGNS = !cpu_2050.RSGNS;
            }  else {
               cpu_2050.LSGNS = 0;
            }
            break;
    case 6: /* IVD/RSGNS */
            t = cpu_2050.u_bus & 0xf;
            /* If minus, clear R sign, input in U */
            if (t < 0xa) {
               /* Invalid trap */
            }  else if (t == 0xB || t == 0xD) {
               cpu_2050.RSGNS = !cpu_2050.RSGNS;
            }
            break;
    case 7: /* EDITSGN */
            t = cpu_2050.w_bus & 0xf;
            if (t > 0x9) {
               cpu_2050.RSGNS = 1;
               if (t != 0xB && t != 0xD) {
                  cpu_2050.LSGNS = 0;
               }
            } else {
               cpu_2050.RSGNS = 0;
            }
            break;
    case 8: /* E->S03 */
            s_update &= 0xf;
            s_update |= (sal->ce & 0xf) << 4;
            break;
    case 9: /* S03|E,1->LSGNS */
            cpu_2050.LSGNS = 1;
            s_update |= (sal->ce & 0xf) << 4;
            break;
    case 10: /* S03|E */
            s_update |= (sal->ce & 0xf) << 4;
            break;
    case 11: /* S03|E,0->BS */
            cpu_2050.BS_REG = 0;
            s_update |= (sal->ce & 0xf) << 4;
            break;
    case 12: /* X0,B0,1SYL */
            s_update &= ~(BIT0|BIT1);
            if ((cpu_2050.alu_out & 0x000f0000) == 0)
                s_update |= BIT0;
            if ((cpu_2050.alu_out & 0xf0000000) == 0)
                s_update |= BIT1;
            if ((cpu_2050.alu_out >> 28) <= 3) {
                cpu_2050.SYLS1 = 1;
            } else {
                cpu_2050.SYLS1 = 0;
            }
            break;
    case 13: /* FPZERO, set S0 if floating point 0. */
            if ((cpu_2050.aob_latch & 0xffffff) == 0 && cpu_2050.F_REG == 0 && (s_update & BIT3) != 0)
                s_update |= BIT0;
            else
                s_update &= ~BIT0;
            break;
    case 14: /* FPZER,E->FN*/
            break;
    case 15: /* B0,1SYL */
            s_update &= ~(BIT1);
            if ((cpu_2050.alu_out & 0xf0000000) == 0)
                s_update |= BIT1;
            if ((cpu_2050.alu_out >> 28) <= 3) {
                cpu_2050.SYLS1 = 1;
            } else {
                cpu_2050.SYLS1 = 0;
            }
            break;
    case 16: /* S03.!E */
            s_update &= ~((sal->ce & 0xf) << 4);
            break;
    case 17: /* (T=0)->S3 */
            s_update &= ~(BIT3);
            if (cpu_2050.alu_out == 0)
                s_update |= BIT3;
            break;
    case 18: /* E->BS,T30->S3 */
            cpu_2050.BS_REG = (sal->ce & 0xf) << 4;
            s_update &= ~(BIT3);
            if ((cpu_2050.alu_out & 02) != 0)
                s_update |= BIT3;
            break;
    case 19: /* E->BS */
            cpu_2050.BS_REG = (sal->ce & 0xf) << 4;
            break;
    case 20: /* 1->BS*MB */
            cpu_2050.BS_REG = 1 << ((3 - cpu_2050.MB_REG) + 4);
            break;
    case 21: /* DIRCTL->E */
            break;
    case 22: /* Undefined */
            break;
    case 23: /* Manual->STOP */
            break;
    case 24: /* E->S47 */
            s_update &= 0xf0;
            s_update |= (sal->ce & 0xf);
    case 25: /* S47|E */
            s_update |= (sal->ce & 0xf);
            break;
    case 26: /* S47&~E */
            s_update &= 0xf0 | ~(sal->ce & 0xf);
            break;
    case 27: /* S47,ED*FP */
            break;
    case 28: /* OPPANEL->S47 */
            break;
    case 29: /* CAR,(T!=0)->CR */
            break;
    case 30: /* KEY->F */
            break;
    case 31: /* F->KEY */
            break;
    case 32: /* 1->LSGNS */
            cpu_2050.LSGNS = 1;
            break;
    case 33: /* 0->LSGNS */
            cpu_2050.LSGNS = 0;
            break;
    case 34: /* 1->RSGNS */
            cpu_2050.RSGNS = 1;
            break;
    case 35: /* 0->RSGNS */
            cpu_2050.RSGNS = 0;
            break;
    case 36: /* L(0)->LSGNS */
            cpu_2050.LSGNS = (cpu_2050.L_REG & 0x80000000) != 0;
            break;
    case 37: /* R(0)->RSGNS */
            cpu_2050.RSGNS = (cpu_2050.R_REG & 0x80000000) != 0;
            break;
    case 38: /* E(13)->WFN */
            if (cpu_2050.io_mode) 
                cpu_2050.io_mvfnc = sal->ce & 07;
            else
                cpu_2050.mvfnc = sal->ce & 07;
            break;
    case 39: /* E(23)->FN */
            cpu_2050.FN = (sal->ce & 03) << 4;
            break;
    case 40: /* E(23)->CR */
            cpu_2050.CC = sal->ce & 03;
            break;
    case 41: /* SETCRALG */
             /* CC = 0 if aob_latch == 0, CC = 1 if aob_latch<0> == 1, CC=2 otherwise */
            if (cpu_2050.aob_latch == 0) {
                cpu_2050.CC = 0;
            } else if (cpu_2050.aob_latch & 0x80000000) {
                cpu_2050.CC = 1;
            } else {
                cpu_2050.CC = 2;
            }
            break;
    case 42: /* SETCRLOG */
             /* CC = 0 if aob_latch & BS == 0, CC = 1 if */
            if ((cpu_2050.aob_latch & BS_MASK[cpu_2050.BS_REG]) == 0) {
                cpu_2050.CC = 0;
            } else {
                if (carry_out)
                    cpu_2050.CC = 2;
                else
                    cpu_2050.CC = 1;
            }
            break;
    case 43: /* !S4,S4->CR */
            cpu_2050.CC = (cpu_2050.S_REG & BIT4) ? 1 : 2;
            break;
    case 44: /* S4,~S4->CR */
            cpu_2050.CC = (cpu_2050.S_REG & BIT4) ? 2 : 1;
            break;
    case 45: /* 1->REFETCH */
            cpu_2050.REFETCH = 1;
            break;
    case 46: /* SYNC->OPPANEL */
            break;
    case 47: /* SCAN*E,10 */
            break;
    case 48: /* 1>SUPROUT */
            break;
    case 49: /* MPX RESET */
            break;
    case 50: /* E(0)->IBFULL */
            break;
    case 51: /* undefined */
            break;
    case 52: /* E->CH */
            cpu_2050.CH = sal->ce << 2;
            break;
    case 53: /* undefined */
            break;
    case 54: /* 1->TIMERIRPT */
            break;
    case 55: /* T->PSW,IPL->T */
            l_update = (A_SW << 8) | (B_SW << 28) | (C_SW << 24);
            break;
    case 56: /* T->PSW T(12-15) to PSW */
            break;
    case 57: /* SCAN*E,00 */
            break;
    case 58: /* 1->IOMODE */
            cpu_2050.io_mode = 1;
            break;
    case 59: /* 0->IOMODE */
            cpu_2050.io_mode = 0;
            break;
    case 60: /* 1->SELOUT */
            break;
    case 61: /* 1->ADROUT */
            break;
    case 62: /* 1->COMOUT */
            break;
    case 63: /* 1->SERVOUT */
            break;
    }

    switch (sal->tr) {
    case 0:  /* T */
            break;
    case 1:  /* R */
            r_update = cpu_2050.aob_latch;
            break;
    case 2:  /* R0 */
            r_update = (cpu_2050.aob_latch & 0xff000000) | 
                             (cpu_2050.R_REG & 0x00ffffff);
            break;
    case 3:  /* M */
            cpu_2050.M_REG = cpu_2050.aob_latch;
            break;
    case 4:  /* D */
            /* Wait if memory state != W1 */
            if (cpu_2050.mem_state == R1 || cpu_2050.mem_state == R2)
                 goto channel;
            cpu_2050.SDR_REG = cpu_2050.aob_latch;
            break;
    case 5:  /* L0 */
            l_update = (cpu_2050.aob_latch & 0xff000000) | 
                             (cpu_2050.L_REG & 0x00ffffff);
            break;
    case 6:  /* R,A Initiate main storage read */
            /* Wait if memory state != W2 */
            r_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch;
            init_mem = 1;
            break;
    case 7:  /* L */
            l_update = cpu_2050.aob_latch;
            break;
    case 8:  /* HA->A */
             /* Generate hardwired address */
             /* Emit x000 write main storage address 80 */
             /* Emit x001 read main storage address 80 */
             /* Emit x010 or from main storage address 80 */
             /* Emit x011 undefined */
             /* Emit x100 write main storage address 84 set IA to 84 */
             /* Emit x101 read main storage address 84 set IA to 84 */
             /* Emit x110 undefined */
             /* Emit x111 undefined */
            break;
    case 9:  /* R,AN, initaite memory request, suppress invalid memory address trap */
            /* Wait if memory state != W2 */
            r_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch;
            init_mem = 1;
            break;
    case 10:  /* R,AW, initiate memory request, invalid address trap if not word boundry  */
            /* Wait if memory state != W2 */
            r_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch;
            init_mem = 1;
            break;
    case 11:  /* R,AD  memory request, invalid address trap if not double boundry */
            /* Wait if memory state != W2 */
            r_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch;
            init_mem = 1;
            break;
    case 12:  /* D->IAR interlock with storage timing rinmmg */
            /* Read */
            if (cpu_2050.mem_state != R2)
                 goto channel;
            cpu_2050.IA_REG = cpu_2050.SDR_REG;
            break;
    case 13:  /* SCAN->D */
            break;
    case 14:  /* R13 */
            r_update = (cpu_2050.R_REG & 0xff000000) | 
                             (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 15:  /* A initiate memory request */
            /* Wait if memory state != W2 */
            cpu_2050.SAR_REG = cpu_2050.aob_latch & 0xffffff;
            init_mem = 1;
            break;
    case 16:  /* L,A initiate memory request */
            /* Wait if memory state != W2 */
            l_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch & 0xffffff;
            init_mem = 1;
            break;
    case 17:  /* R,D  */
            if (cpu_2050.mem_state == R1 || cpu_2050.mem_state == R2)
                 goto channel;
            r_update = cpu_2050.aob_latch;
            cpu_2050.SDR_REG = cpu_2050.aob_latch;
            break;
    case 18:  /* Undefined  */
            break;
    case 19:  /* R,IO   */
            break;
    case 20:  /* H */
            cpu_2050.H_REG = cpu_2050.aob_latch;
            break;
    case 21:  /*  */
            cpu_2050.IA_REG = (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 22:  /* FOLD->D */
            break;
    case 23:  /* undefined */
            break;
    case 24:  /* L,M  */
            l_update = cpu_2050.aob_latch;
            cpu_2050.M_REG = cpu_2050.aob_latch;
            break;
    case 25:  /* MLJK */
            l_update = cpu_2050.aob_latch;
            cpu_2050.M_REG = cpu_2050.aob_latch;
            cpu_2050.J_REG = (cpu_2050.aob_latch >> 16) & 0xF;  /* 12-15 */
            cpu_2050.MD_REG = (cpu_2050.aob_latch >> 12) & 0xF;  /* 16-19 */
            cpu_2050.refresh = 0;
            s_update &= ~(BIT0|BIT1);
            if (cpu_2050.J_REG == 0)
                s_update |= BIT0;
            if (cpu_2050.MD_REG == 0)
                s_update |= BIT1;
            if ((cpu_2050.MD_REG & 0xc0) == 0)
                cpu_2050.osyl = 1;
            else
                cpu_2050.osyl = 0;
            cpu_2050.ilc = 0;
            if ((cpu_2050.aob_latch & 0x80000000) != 0) 
                cpu_2050.ILC = 1;
            if ((cpu_2050.aob_latch & 0xc0000000) == 0xc0000000) 
                cpu_2050.ILC = 2;
            break;
    case 26:  /* MHL */
            l_update = cpu_2050.aob_latch;
            cpu_2050.M_REG = (cpu_2050.aob_latch >> 16) & 0xffff;
            cpu_2050.MD_REG = (cpu_2050.aob_latch >> 28) & 0xF;  /* 0-3 */
            break;
    case 27:  /* MD */
            cpu_2050.MD_REG = (cpu_2050.aob_latch >> 20) & 0xF;  /* 8-11 */
            break;
    case 28:  /* M,SP gate adder to M, bits 8-11 to key */
            break;
    case 29:  /* D,BS gate D to SDR based on BS */
            if (cpu_2050.mem_state == R1 || cpu_2050.mem_state == R2)
                 goto channel;
            cpu_2050.SDR_REG = (cpu_2050.SDR_REG & ~BS_MASK[cpu_2050.BS_REG]) |
                           (cpu_2050.aob_latch & BS_MASK[cpu_2050.BS_REG]);
            break;
    case 30:  /* L13 */
            l_update = (cpu_2050.L_REG & 0xff000000) | 
                             (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 31:  /* IO  gate adder 29-31 to I/O reg */
            break;
    }

    /* Store Mover output */
    switch (sal->wm) {
    case 0: /* No change */
            break;
    case 1: /* W->MMB  */
            cpu_2050.M_REG &= ~(0xff << (3-cpu_2050.MB_REG));
            cpu_2050.M_REG |= (cpu_2050.w_bus << (3-cpu_2050.MB_REG));
            break;
    case 2: /* W67->MB  */
            cpu_2050.MB_REG = cpu_2050.w_bus & 03;
            break;
    case 3: /* W67->LB  */
            cpu_2050.LB_REG = cpu_2050.w_bus & 03;
            break;
    case 4: /* W27->PSW4, 2-7 -> PSW 34-39  */
            break;
    case 5: /* W->PSW0  */
            cpu_2050.MASK = cpu_2050.w_bus;
            break;
    case 6: /* WL->J  */
            cpu_2050.J_REG = (cpu_2050.w_bus & 0xf0) >> 4;
            break;
    case 7: /* W->CHCTL  */
            break;
    case 8: /* W,E->A(BUMP)  */
            break;
    case 9: /* WL->G1  */
            g_update &= 0xf;
            g_update |= (cpu_2050.w_bus & 0xf0);
            break;
    case 10: /* WR->G2  */
            g_update &= 0xf0;
            g_update |= (cpu_2050.w_bus & 0x0f);
            break;
    case 11: /* W->G  */
            g_update = cpu_2050.w_bus;
            break;
    case 12: /* W->MMB(E?)  */
            cpu_2050.M_REG &= ~(0xff << (3-cpu_2050.MB_REG));
            cpu_2050.M_REG |= (cpu_2050.w_bus << (3-cpu_2050.MB_REG));
            break;
    case 13: /* WR->MD */
            cpu_2050.MD_REG = (cpu_2050.w_bus & 0xf0) >> 4;
            break;
    case 14: /* WR->F */
            cpu_2050.F_REG = (cpu_2050.w_bus & 0x0f);
            break;
    case 15: /* W->MD,F  */
            cpu_2050.MD_REG = (cpu_2050.w_bus & 0xf0) >> 4;
            cpu_2050.F_REG = (cpu_2050.w_bus & 0x0f);
            break;
    }

    /* Update counters */
    switch (sal->up) {
    case 0: /* 0-> */
            if (sal->lb)
                cpu_2050.LB_REG = 0;
            if (sal->mb)
                cpu_2050.MB_REG = 0;
            if (sal->md)
                cpu_2050.MD_REG = 0;
            break;
    case 1: /* 3-> */
            if (sal->lb)
                cpu_2050.LB_REG = 3;
            if (sal->mb)
                cpu_2050.MB_REG = 3;
            if (sal->md)
                cpu_2050.MD_REG = 3;
            break;
    case 2: /* - */
            if (sal->lb)
                cpu_2050.LB_REG = (cpu_2050.LB_REG - 1) & 3;
            if (sal->mb)
                cpu_2050.MB_REG = (cpu_2050.MB_REG - 1) & 3;
            if (sal->md)
                cpu_2050.MD_REG = (cpu_2050.MD_REG - 1) & 3;
            break;
    case 3:
            if (sal->lb)
                cpu_2050.LB_REG = (cpu_2050.LB_REG + 1) & 3;
            if (sal->mb)
                cpu_2050.MB_REG = (cpu_2050.MB_REG + 1) & 3;
            if (sal->md)
                cpu_2050.MD_REG = (cpu_2050.MD_REG + 1) & 3;
            break;
    }

    if (cpu_2050.io_mode) {
        /* Set local storage address */
        switch (sal->ws) {
        case 0: /* 2,3  */
                cpu_2050.LSA = 0x2c;
                break;
        case 1: /* 2,3 */
                cpu_2050.LSA = 0x2d;
                break;
        case 2: /* 2,3 */
                cpu_2050.LSA = 0x2e;
                break;
        case 3: /* 2,3 */
                cpu_2050.LSA = 0x2f;
                break;
        case 4: /* 0,CH */
                cpu_2050.LSA = (cpu_2050.CH);
                break;
        case 5: /* 0,CH */
                cpu_2050.LSA = (cpu_2050.CH) | 1;
                break;
        case 6: /* 0,CH */
                cpu_2050.LSA = (cpu_2050.CH) | 2;
                break;
        case 7: /* 0,CH */
                cpu_2050.LSA = (cpu_2050.CH) | 3;
                break;
        }
    } else {
        /* Set local storage address */
        switch (sal->ws) {
        case 0: /* No access  */
                break;
        case 1: /* WS1->LSA */
                cpu_2050.LSA = 0x11;
                break;
        case 2: /* WS2->LSA */
                cpu_2050.LSA = 0x12;
                break;
        case 3: /* WS,E->LSA */
                cpu_2050.LSA = 0x10 | (sal->ce & 0xf);
                break;
        case 4: /* FN,J->LSA */
                if (sal->sf != 7)
                    cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.J_REG & 0xf);
                break;
        case 5: /* FN,J|1->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.J_REG & 0xf) | 1;
                break;
        case 6: /* FN,MD->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.MD_REG & 0xf);
                break;
        case 7: /* FN,MD|1->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.MD_REG & 0xf) | 1;
                break;
        }
    }


    /* Fetch/store local storage */
    switch (sal->sf) {
    case 0: /* R->LS */
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.R_REG;
            break;
    case 1: /* LS->L,R->LS */
            l_update = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.R_REG;
            break;
    case 2: /* LS->R->LS */
            r_update = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.R_REG;
            break;
    case 3: /* unknown */
            break;
    case 4: /* L->LS */
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.L_REG;
            break;
    case 5: /* LS->R,L->LS */
            r_update = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.L_REG;
            break;
    case 6: /* LS->L->LS */
            l_update = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.L_REG;
            break;
    case 7: /* No function */
            break;
    }


    /* Basic next address */
    roar = sal->zp;
    if (sal->zn != 0) {
        roar |= sal->sf << 2; 
    }

    b_bit = 0;
    switch (sal->ab) {
    case 0: /*  0  */
            a_bit = 0;
            break;
    case 1: /*  1  */
            a_bit = 1;
            break;
    case 2: /* S0 */
            a_bit = ((cpu_2050.S_REG & BIT0) != 0);
            break;
    case 3: /* S1 */
            a_bit = ((cpu_2050.S_REG & BIT1) != 0);
            break;
    case 4: /* S2 */
            a_bit = ((cpu_2050.S_REG & BIT2) != 0);
            break;
    case 5: /* S3 */
            a_bit = ((cpu_2050.S_REG & BIT3) != 0);
            break;
    case 6: /* S4 */
            a_bit = ((cpu_2050.S_REG & BIT4) != 0);
            break;
    case 7: /* S5 */
            a_bit = ((cpu_2050.S_REG & BIT5) != 0);
            break;
    case 8: /* S6 */
            a_bit = ((cpu_2050.S_REG & BIT6) != 0);
            break;
    case 9: /* S7 */
            a_bit = ((cpu_2050.S_REG & BIT7) != 0);
            break;
    case 10: /* CSTAT carry status */
            a_bit = (cpu_2050.CSTAT_REG != 0);
            break;
    case 11: /* Undefined */
            break;
    case 12: /* 1SYLS */
            a_bit = (cpu_2050.SYLS1);
            break;
    case 13: /* LSGNS  L sign status */
            a_bit = (cpu_2050.LSGNS);
            break;
    case 14: /* XSGNS  L sign ^ R Sign. */
            a_bit = (cpu_2050.LSGNS |= cpu_2050.RSGNS);
            break;
    case 15: /* Unknown */
            break;
    case 16: /* CRMD, mask condition codes */
            a_bit = (cpu_2050.MD_REG & (1 << 3 - cpu_2050.CC));
            break;
    case 17: /* W = 0 */
            a_bit = (cpu_2050.w_bus == 0);
            break;
    case 18: /* WL = 0 */
            a_bit = ((cpu_2050.w_bus & 0xf0) == 0);
            break;
    case 19: /* WR = 0 */
            a_bit = ((cpu_2050.w_bus & 0x0f) == 0);
            break;
    case 20: /* MD = FP, Valid FP register */
            a_bit = ((cpu_2050.MD_REG & 0x9) == 0);
            break;
    case 21: /* MB = 3 */
            a_bit = (cpu_2050.MB_REG == 3);
            break;
    case 22: /* MD3= 0 */
            a_bit = ((cpu_2050.MD_REG & 1) == 0);
            break;
    case 23: /* G1 == 0 */
            a_bit = ((cpu_2050.G_REG & 0xf0) == 0);
            break;
    case 24: /* G1NEG */
            a_bit = (cpu_2050.G1NEG != 0);
            break;
    case 25: /* G<4 */
            a_bit = (cpu_2050.G_REG < 4);
            break;
    case 26: /* G1MBZ */
            a_bit = ((cpu_2050.G_REG & 0xf0) == 0 && (cpu_2050.MB_REG == 0));
    case 27: /* IO Stat 0 to CPU */
            a_bit = (cpu_2050.IOSTAT & 4) != 0;
            break;
    case 28: /* IO Stat 2  */
            a_bit = (cpu_2050.IOSTAT & 2) != 0;
            break;
    case 29: /* R(31)  */
            a_bit = ((cpu_2050.R_REG & 1) != 0);
            break;
    case 30: /* F(2)  */
            a_bit = ((cpu_2050.F_REG & 2) != 0);
            break;
    case 31: /* L(0)  */
            a_bit = ((cpu_2050.L_REG & 0x80000000) != 0);
            break;
    case 32: /* F=0  */
            a_bit = (cpu_2050.F_REG == 0);
            break;
    case 33: /* UNORM,  T8-11 zero and not S0  */
            a_bit = ((cpu_2050.alu_out & 0x00f00000) != 0 && 
                        (cpu_2050.S_REG & BIT0) == 0);
            break;
    case 34: /* TZ*BZ */
            a_bit = ((cpu_2050.alu_out & BS_MASK[cpu_2050.BS_REG]) == 0);
            break;
    case 35: /* EDITPAT  */
            break;
    case 36: /* PROB, in problem state */
            a_bit = (cpu_2050.AMWP & 1) != 0;
            break;
    case 37: /* TIMUP, timer update signal  */
            break;
    case 38: /* Undefined */
            break;
    case 39: /*  GZ/MB3 */
            a_bit = (cpu_2050.G_REG == 0 || (cpu_2050.MB_REG == 3));
            break;
    case 40: /* Undefined  */
            break;
    case 41: /* LOG, log trigger  */
            break;
    case 42: /* STC=0, Scan test counter  */
            break;
    case 43: /* G2<=LB  */
            a_bit = ((cpu_2050.G_REG & 0xf) <= cpu_2050.LB_REG);
            break;
    case 44: /* Undefined  */
            break;
    case 45: /* D(7), test SDR bit 7   */
            a_bit = (cpu_2050.SDR_REG & 0x1000000) != 0;
            break;
    case 46: /* SCPS  */
            break;
    case 47: /* SCFS  */
            break;
    case 48: /* CROS storage protection violation  */
            break;
    case 49: /* W(67)-AB  */
            a_bit = (cpu_2050.w_bus & 02) != 0;
            b_bit = (cpu_2050.w_bus & 1) != 0;
            break;
    case 50: /* CROS T16-31 != 0  */
            break;
    case 51: /* CROS T5-7 == 0 && T16-31 != 0  */
            break;
    case 52: /* CROS  bus in bit 0  */
            break;
    case 53: /* CROS IB full  */
            break;
    case 54: /* CANG: (29-31) != 0, CA not good  */
            break;
    case 55: /* CHLOG, channel log   */
            break;
    case 56: /* I-FETCH,   */
            a_bit = ((cpu_2050.IA_REG & 0x2) == 0);
            if (!a_bit) {
               b_bit = cpu_2050.REFETCH;
            }
            break;
    case 57: /* IA(30)  */
            a_bit = ((cpu_2050.IA_REG & 0x2) != 0);
            break;
    case 58: /* EXP,CHIRPT  */
            break;
    case 59: /* CROS: Direct date hold sense branch  */
            break;
    case 60: /* PSS,  */
            break;
    case 61: /* IO Stat 4  */
            a_bit = ((cpu_2050.IOSTAT & 01) != 0);
            break;
    case 62: /*  Undefined   */
            break;
    case 63: /*  RX.S0     */
            a_bit = 0;
            if ((cpu_2050.S_REG & BIT0) != 0 && 
                 ((cpu_2050.M_REG & 0xc0000000) == 0x40000000))
                a_bit = 1;
            break;
    }

    switch (sal->bb) {
    case 0: /*  0  */
            b_bit |= 0;
            break;
    case 1: /*  1  */
            b_bit |= 1;
            break;
    case 2: /* S0 */
            b_bit |= ((cpu_2050.S_REG & BIT0) != 0);
            break;
    case 3: /* S1 */
            b_bit |= ((cpu_2050.S_REG & BIT1) != 0);
            break;
    case 4: /* S2 */
            b_bit |= ((cpu_2050.S_REG & BIT2) != 0);
            break;
    case 5: /* S3 */
            b_bit |= ((cpu_2050.S_REG & BIT3) != 0);
            break;
    case 6: /* S4 */
            b_bit |= ((cpu_2050.S_REG & BIT4) != 0);
            break;
    case 7: /* S5 */
            b_bit |= ((cpu_2050.S_REG & BIT5) != 0);
            break;
    case 8: /* S6 */
            b_bit |= ((cpu_2050.S_REG & BIT6) != 0);
            break;
    case 9: /* S7 */
            b_bit |= ((cpu_2050.S_REG & BIT7) != 0);
            break;
    case 10: /* RSGNS Right sign status */
            b_bit |= cpu_2050.RSGNS;
            break;
    case 11: /* HSCH HS  channel special branch */
            break;
    case 12: /* EXC  exception branch */
            break;
    case 13: /* WR=0 MVR LTH 4-7 eq zero */
            break;
    case 14: /* Unknown */
            break;
    case 15: /* T13=0 */
            b_bit |= ((cpu_2050.alu_out & 0x00ffffff) == 0);
            break;
    case 16: /* T(0) */
            b_bit |= ((cpu_2050.alu_out & 0x80000000) == 0);
            break;
    case 17: /* T = 0 */
            b_bit |= (cpu_2050.alu_out == 0);
            break;
    case 18: /* TZ*BS */
            t1  = ((cpu_2050.BS_REG & BIT0) ? 0xff000000: 0) |
                  ((cpu_2050.BS_REG & BIT1) ? 0x00ff0000: 0) |
                  ((cpu_2050.BS_REG & BIT2) ? 0x0000ff00: 0) |
                  ((cpu_2050.BS_REG & BIT3) ? 0x000000ff: 0);
            b_bit |= ((cpu_2050.alu_out & t1) == 0);
            break;
    case 19: /* W = 1 */
            b_bit = (cpu_2050.w_bus == 1);
            break;
    case 20: /* LB=0 */
            b_bit |= (cpu_2050.LB_REG == 0);
            break;
    case 21: /* LB=3 */
            b_bit |= (cpu_2050.LB_REG == 3);
            break;
    case 22: /* MD= 0 */
            b_bit |= (cpu_2050.MD_REG == 0);
            break;
    case 23: /* G2=0 */
            b_bit |= ((cpu_2050.G_REG & 0x0f) == 0);
            break;
    case 24: /* G2NEG */
            b_bit |= cpu_2050.G2NEG;
            break;
    case 25: /* G2LBZ */
            b_bit |= ((cpu_2050.G_REG & 0x0f) == 0 || (cpu_2050.LB_REG == 0));
            break;
    case 26: /* IO Stat 1 to CPU */
            b_bit |= (cpu_2050.IOSTAT & 4) != 0;
            break;
    case 27: /* MD/JI MD Odd gt 8 or J odd gt 8 */
            b_bit |= ((cpu_2050.MD_REG & 0x9) != 0 || (cpu_2050.J_REG & 0x9) != 0);
            break;
    case 28: /* IVA  */
            break;
    case 29: /* IO Stat 3 */
            b_bit |= (cpu_2050.IOSTAT & 1) != 0;
            break;
    case 30: /* (CAR) branch on carry latch  */
            b_bit |= carry_out;
            break;
    case 31: /* (Z00) looks at current T */
            b_bit |= (cpu_2050.alu_out & 0x80000000) != 0;
            break;
    }

    switch (sal->zn) {
    case 0: /* Use ZF function */
            switch (sal->zf) {
            case 0:  /* Undefined */
                     break;
            case 1:  /* Undefined */
                     break;
            case 2:  /* D->ROAR,SCAN */
                     break;
            case 3:  /* Undefined */
                     break;
            case 4:  /* Undefined */
                     break;
            case 5:  /* Undefined */
                     break;
            case 6:  /* M(03)->ROAR */
                     roar |= ((cpu_2050.M_REG >> 28) & 0xf) << 2;
                     break;
            case 7:  /* Undefined */
                     break;
            case 8:  /* M(47)->ROAR */
                     roar |= ((cpu_2050.M_REG >> 24) & 0xf) << 2;
                     break;
            case 9:  /* Undefined */
                     break;
            case 10:  /* F->ROAR */
                     roar |= (cpu_2050.F_REG & 0xf) << 2;
                     break;
            case 11:  /* Undefined */
                     break;
            case 12:  /* ED->ROAR exp diff */
                     break;
            case 13:  /* Undefined */
                     break;
            case 14:  /* RETURN->ROAR */
                     break;
            case 15:  /* Undefined */
                     break;
            }
            break;
    case 1: /* SMIF suppress memory instruction fetch */
            /* By order IV7 if refresh == 0 and IAR bit 30 == 1 */
            break;
    case 2: /* A&(B=0)->A */
            if (!b_bit) {
                a_bit = 1;
            }
            break;
    case 3: /* A&(B=1)->A */
            if (b_bit) {
                a_bit = 1;
            }
            break;
    case 4: /* Normal no change */
            break;
    case 5: /* Force Invalid Op trap address */
            break;
    case 6: /* B&(A=0)->B */
            if (!a_bit) {
                b_bit = 1;
            }
            break;
    case 7: /* B&(A=1)->B */
            if (a_bit) {
                b_bit = 1;
            }
            break;
    }

    roar |= (a_bit << 1)|b_bit;

    cpu_2050.L_REG = l_update;
    cpu_2050.R_REG = r_update;
    cpu_2050.G_REG = g_update;
    cpu_2050.S_REG = s_update;
channel:

    /* Main memory cycle */
    switch (cpu_2050.mem_state) {
    case R1:
             cpu_2050.mem_state = R2;
             cpu_2050.SDR_REG = cpu_2050.M[cpu_2050.SAR_REG >> 2];
             break;
    case R2:
             cpu_2050.mem_state = R3;
             break;
    case R3:
             cpu_2050.mem_state = W1;
             break;
    case W1:
             if (init_mem)
                cpu_2050.mem_state = R1;
             cpu_2050.M[cpu_2050.SAR_REG >> 2] = cpu_2050.SDR_REG;
             break;
    }
    ;
    
} 


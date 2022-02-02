/*
 * microsim360 - Model 2030 definitions.
 *
 * Copyright 2021, Richard Cornwell
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

#include <stdio.h>
#include <stdint.h>

#ifndef _MODEL50_H_
#define _MODEL50_H_

int SYS_RST;
int ROAR_RST;
int START;
int SET_IC;
int CHECK_RST;
int STOP;
int INT_TMR;
int STORE;
int DISPLAY;
int LAMP_TEST;
int POWER;
int INTR;
int LOAD;
int timer_event;

uint8_t  A_SW;
uint8_t  B_SW;
uint8_t  C_SW;
uint8_t  D_SW;
uint8_t  E_SW;
uint8_t  F_SW;
uint8_t  G_SW;
uint8_t  H_SW;
uint8_t  J_SW;

uint8_t  PROC_SW;
uint8_t  RATE_SW;
uint8_t  CHK_SW;
uint8_t  MATCH_SW;

uint16_t const odd_parity[256];

struct ROS_2050 {
    int     io;
    int     lu;
    int     mv;
    int     zp;
    int     zn;
    int     zf;
    int     tr;
    int     zr;
    int     ws;
    int     sf;
    int     iv;
    int     al;
    int     wm;
    int     up;
    int     md;
    int     lb;
    int     mb;
    int     dg;
    int     ul;
    int     ur;
    int     ce;
    int     lx;
    int     tc;
    int     ry;
    int     ad;
    int     ab;
    int     bb;
    int     ux;
    int     ss;
    int     extra;
    char    note[20];
} ros_2030[4096];


struct CPU_2050 {
int          count;
uint32_t    M[64 * 1024];
uint32_t    LS[64];
uint32_t    BUMP[1024];          /* Bump storage */

int         mem_max;             /* Maximum memory address - 1 */
int         io_mode;             /* CPU or I/O mode of operation */

uint32_t    right_bus;           /* Right alu input bus */
uint32_t    left_bus;            /* Left alu input bus */
uint32_t    alu_out;             /* Alu output bus */
uint32_t    aob_latch;           /* AOB output latch */

uint8_t     u_bus;               /* Mover u input */
uint8_t     v_bus;               /* Mover v input */
uint8_t     w_bus;               /* Mover w output */

int         mvfnc;               /* Mover function register */
int         io_mvfnc;            /* Mover I/O function register */

uint32_t    R_REG;               /* Right register */
uint32_t    L_REG;               /* Left register */
uint32_t    H_REG;               /* H register */
uint32_t    M_REG;               /* M instruction register */
uint32_t    IAR_REG;             /* Instruction pointer register */
uint32_t    SAR_REG;             /* Memory address register */
uint32_t    SDR_REG;             /* Memory data register */
uint8_t     F_REG;               /* F shift output register */
uint8_t     MD_REG;              /* 4 bit register */
uint8_t     MB_REG;              /* Memory byte selector */
uint8_t     LB_REG;              /* Memory byte selector */
uint8_t     G_REG;               /* G1 and G2 counter registers */
uint8_t     J_REG;               /* J register */
uint8_t     Q_REG;               /* Q register */
uint8_t     S_REG;               /* S status bits */
uint8_t     BS_REG;              /* Byte mask regiser */
uint8_t     LSGN;                /* Left sign flag */
uint8_t     RSGN;                /* Right sign flag */
uint8_t     SYL1;                /* One sylabal opcode */
uint8_t     MASK;                /* Interrupt mask register */
uint8_t     REFETCH;             /* Refetch flag */
uint8_t     LSA;                 /* Local storage address */
uint8_t     CH;                  /* Current channel select */
uint8_t     KEY;                 /* Storage key */
uint8_t     AMWP;                /* Flags */
uint8_t     CC;                  /* CC register */
uint8_t     ILC;                 /* ILC register */
uint8_t     PM;                  /* Program Mask */

int         CSTAT;               /* Carry status register */
int         CAR;                 /* Output carry status */
int         G1NEG;               /* G1 negative */
int         G2NEG;               /* G2 negative */

uint16_t    WX;                 /* ROAR address register. */
uint16_t    FWX;                /* Backup ROAR address. */
uint16_t    PWX;                /* Previous ROAR address. */

uint8_t     IOS;                /* I/O Status registers */
uint8_t     BRC;                /* Branch control register */
uint16_t    BIF1;               /* MPX buffer 1 */
uint16_t    BIF2;               /* MPX buffer 1 */
uint32_t    B_REG[4];           /* Select channel B regiser */
uint32_t    C_REG[4];           /* Select channel C regiser */
uint16_t    GR_REG[4];          /* Selector General register */
uint16_t    TAGS[4];            /* channel tags. */
uint16_t    TI[4];              /* Input tags. */

} cpu_2050; 

uint16_t    store;
uint16_t    allow_write;
uint16_t    match;
uint8_t     allow_man_operation;
uint8_t     wait;
uint8_t     test_mode;
uint8_t     clock_start_lch;

void  cycle();

#endif

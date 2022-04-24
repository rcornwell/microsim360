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

extern int SYS_RST;
extern int ROAR_RST;
extern int START;
extern int SET_IC;
extern int CHECK_RST;
extern int STOP;
extern int INT_TMR;
extern int STORE;
extern int DISPLAY;
extern int LAMP_TEST;
extern int POWER;
extern int INTR;
extern int LOAD;
extern int timer_event;

extern uint8_t  A_SW;
extern uint8_t  B_SW;
extern uint8_t  C_SW;
extern uint8_t  D_SW;
extern uint8_t  E_SW;
extern uint8_t  F_SW;
extern uint8_t  G_SW;
extern uint8_t  H_SW;
extern uint8_t  J_SW;

extern uint8_t  PROC_SW;
extern uint8_t  RATE_SW;
extern uint8_t  CHK_SW;
extern uint8_t  MATCH_SW;

extern uint16_t const odd_parity[256];
extern uint8_t     load_mode;


extern struct ROS_2050 {
    int      io;
    int      lu;
    int      mv;
    int      zp;
    int      zn;
    int      zf;
    int      tr;
    int      zr;
    int      ws;
    int      sf;
    int      iv;
    int      al;
    int      wm;
    int      up;
    int      md;
    int      mb;
    int      lb;
    int      dg;
    int      ul;
    int      ur;
    int      ce;
    int      lx;
    int      tc;
    int      ry;
    int      ad;
    int      ab;
    int      bb;
    int      ux;
    int      ss;
    int      extra;
    uint32_t row1;
    uint32_t row2;
    uint32_t row3;
    uint32_t row4;
    char    *note;
} ros_2050[4096];

#define R1  1                    /* Start of read cycle */
#define R2  2                    /* Data ready during this cycle */
#define R3  3                    /* Data can be modified in SDR */
#define W1  0                    /* Store SDR in memory */

/* To initiate Store read, state must be W2.
   Set address into SAR change state to R1. */
/* Data is avaliable at R2. Hold cycle if access during other cycle */
/* Data can be written to SDR during W1, hold if not W1. */
/* Hold in state W2 until new address written to SAR */

/* Start storage request when:
    iv = 4 or 7.
    tr = 4, 8, 9, 10, 11, 15, 16
*/
    
/* Read storage:
    tr = 12
    al = 30
 */
   
/* Modify storage when:
    tr = 4, 17,29
 */


extern struct CPU_2050 {
int          count;
uint32_t    M[64 * 1024];
uint32_t    LS[64];
uint32_t    BUMP[1024];          /* Bump storage */
uint8_t     mem_state;           /* Storage cycle state */


int         mem_max;             /* Maximum memory address - 1 */
int         io_mode;             /* CPU or I/O mode of operation */
uint32_t    ros_row1;            /* Current ROS data */
uint32_t    ros_row2;
uint32_t    ros_row3;
uint32_t    ros_row4;

uint32_t    right_bus;           /* Right alu input bus */
uint32_t    left_bus;            /* Left alu input bus */
uint32_t    alu_out;             /* Alu output bus */
uint32_t    aob_latch;           /* AOB output latch */

uint8_t     u_bus;               /* Mover u input */
uint8_t     v_bus;               /* Mover v input */
uint8_t     w_bus;               /* Mover w output */

int         mvfnc;               /* Mover function register */
int         io_mvfnc;            /* Mover I/O function register */
int         osyl;                /* Sylable */
int         refresh;             /* Refresh memory buffer */
int         ilc;                 /* Instruction length */

uint32_t    R_REG;               /* Right register */
uint32_t    L_REG;               /* Left register */
uint32_t    H_REG;               /* H register */
uint32_t    M_REG;               /* M instruction register */
uint32_t    IA_REG;              /* Instruction pointer register */
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
uint8_t     LSGNS;               /* Left sign flag */
uint8_t     RSGNS;               /* Right sign flag */
uint8_t     SYLS1;               /* One sylabal opcode */
uint8_t     MASK;                /* Interrupt mask register */
uint8_t     REFETCH;             /* Refetch flag */
uint8_t     LSA;                 /* Local storage address */
uint8_t     CH;                  /* Current channel select */
uint8_t     KEY;                 /* Storage key */
uint8_t     AMWP;                /* Flags */
uint8_t     CC;                  /* CC register */
uint8_t     ILC;                 /* ILC register */
uint8_t     PM;                  /* Program Mask */
uint16_t    FN;                  /* LSA Function address */
uint8_t     OPPANEL;             /* Front panel switch state */
uint8_t     IVA;                 /* Invalid address flag */
uint32_t    DKEYS;               /* Front panel data keys */
uint32_t    AKEYS;               /* Front panel address keys */

int         CSTAT_REG;           /* Carry status register */
int         AUX_REG;             /* Auxliary register */
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
uint32_t    IOSTAT;             /* I/O Status register */
uint16_t    GR_REG[4];          /* Selector General register */
uint16_t    TAGS[4];            /* channel tags. */
uint16_t    TI[4];              /* Input tags. */

} cpu_2050; 

extern uint16_t    store;
extern uint16_t    allow_write;
extern uint16_t    match;
extern uint8_t     allow_man_operation;
extern uint8_t     wait;
extern uint8_t     test_mode;
extern uint8_t     clock_start_lch;

void  cycle();

#endif

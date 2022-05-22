/*
 * microsim360 - Model 2065 definitions.
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

#include <stdio.h>
#include <stdint.h>

#ifndef _MODEL65_H_
#define _MODEL65_H_

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


extern struct ROS_2065 {
    int      MODE;
    int      IND;
    int      A;          /* Bits 06-09 Ingate to A,B,IC */
    int      B;          /* Bits 10-11 Ingate local store to S,T */
    int      C;          /* Bits 12-16 Register ingate to D,K,Q,S,T,PSW,N,G */
    int      D;          /* Bits 17-19 End ops and ingate serial adder to F */
    int      SD;         /* Bits 17-19 Scan control lines */
    int      E;          /* Bits 81,21-24 Increment/Decrement and Emit */
    int      F;          /* Bits 25-30 Misc control lines */
    int      SF;         /* Bits 25-30 Scan control lines */
    int      G;          /* Bits 31-35 Misc control lines and set IC */
    int      SG;         /* Bits 31-35 Scan control lines */
    int      H;          /* Bits 36-42 Local Store, FAA Regs, R/W control */
    int      J;          /* Bits 62-68 Conditional Branch ROSAR 11 */
    int      K;          /* Bits 57-61 Conditional Branch ROSAR 10 */
    int      L;          /* Bits 43-46 Memory request and mark settings */
    int      M;          /* Bits 69-73 Serial adder A side */
    int      N;          /* Bits 74-77 Serial adder B side */
    int      P;          /* Bits 78-80 Parallel adder */
    int      Q;          /* Bits 82-84 Hot ones to Parallel adder A side */
    int      R;          /* Bit 86 Outgate to serial adder inbus A side */
    int      T;          /* Bits 87-90 Outgates to Padder B side A,B,IC */
    int      U;          /* Bits 96,92-95 Outgates to padder A side from S,T,D */
    int      V;          /* Bits 97-99 E and Q register to parallel adder B side */
    int      W;          /* Bits 02-05 FAA and misc control */
    int      NX;         /* Bits 47-56 Next address */

    /* Bit -1 Parity 1-99.
       Bit 20 Parity 2-42
       Bit 85 Parity 43-68
       Bit 91 Parity 69-99
    */
    uint32_t row1;
    uint32_t row2;
    uint32_t row3;
    uint32_t row4;
    char    note[20];
    char    ec[20];
} ros_2065[4096];

#define STAA  BIT0
#define STAB  BIT1
#define STAC  BIT2
#define STAD  BIT3
#define STAE  BIT4
#define STAF  BIT5
#define STAG  BIT6
#define STAH  BIT7

extern struct CPU_2064 {
int          count;
uint64_t    M[64 * 1024];
uint32_t    LS[32];
uint64_t    BUMP[1024];          /* Bump storage */


int         mem_max;             /* Maximum memory address - 1 */
uint32_t    ros_row1;            /* Current ROS data */
uint32_t    ros_row2;
uint32_t    ros_row3;
uint32_t    ros_row4;

uint64_t    paa;                 /* Parallel adder A input 8-63 */
uint64_t    pab;                 /* Parallel adder B input 4-67 */
uint64_t    pal;                 /* Parallel adder Ouput Latches 4-67 */

uint8_t     siba;                /* Serial adder A input */
uint8_t     sibb;                /* Serial adder B input */
uint8_t     sba;                 /* Serial adder A final input */
uint8_t     sbb;                 /* Serial adder B final input */
uint8_t     sal;                 /* Serial adder output latches */

uint32_t    D_REG;               /* Data register */
uint32_t    S_REG;               /* S register */
uint32_t    T_REG;               /* T register */
uint32_t    SAR_REG;             /* Main memory address register */
uint64_t    Q_REG;               /* Instruction buffer */
uint16_t    R_REG;               /* Instruction register */
uint16_t    E_REG;               /* Exponent? */
uint32_t    A_REG;               /* A register */
uint32_t    B_REG;               /* B Register */
uint8_t     BX_REG;              /* Lower bit 64-67 of B */
uint8_t     STC_REG;             /* S extension */
uint32_t    IC_REG;              /* Instruction counter */
uint8_t     MARKS;               /* Marks mask */
uint8_t     F_REG;               /* F register */
uint8_t     G_REG;               /* G counter registers */
uint8_t     MASK;                /* Interrupt mask register */
uint8_t     KEY;                 /* Storage key */
uint8_t     AMWP;                /* Flags */
uint8_t     CC;                  /* CC register */
uint8_t     ILC;                 /* ILC register */
uint8_t     PM;                  /* Program Mask */
uint8_t     OPPANEL;             /* Front panel switch state */
uint8_t     IVA;                 /* Invalid address flag */

int         CSTAT_REG;           /* Carry status register */
int         STAT_REG;            /* Status flags */

uint16_t    ROAR;               /* ROAR address register. */

} cpu_2050;

extern uint16_t    store;
extern uint16_t    allow_write;
extern uint16_t    match;
extern uint8_t     allow_man_operation;
extern uint8_t     wait;
extern uint8_t     test_mode;
extern uint8_t     clock_start_lch;

void  cycle_2065();

#endif

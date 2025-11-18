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

#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "device.h"
#include "xlat.h"
#include "cpu.h"
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

#define MSIGN 0x80000000  /* Minus sign */

#define BIT0L 0x80000000  /* Bit position 0 */
#define BIT1L 0x40000000  /* Bit position 1 */

#define CPOS0 0x80000000  /* Carry from position 0 */
#define CPOS1 0x40000000  /* Carry from position 1 */
#define CPOS8 0x00800000  /* Carry from position 8 */

struct CPU_2050 cpu_2050;

static int         timer_update;         /* Flag that timer update triggered */
static uint32_t    SA;                   /* Address of last memory reference */
static uint8_t     stop_mode = 0;        /* Issue stop at MANUAL->STOP instruction */
static int         timer_irq = 0;        /* Timer Interrupt request */
static int         dtc_latch = 0;
static int         dtc1 = 0;             /* DTC1 option */
static int         dtc2 = 0;             /* DTC2 option */


static uint32_t tr_interloc = 0x3043bf50;

static    char     *lu_name[] = {  /* Left Mover input */
                  "0",
                  "MD,F>U",
                  "R3>U",    /* Byte 3 to U */
                  "DD>U",    /* Direct data to U */
                  "XTR>U",   /* XTR to U, reset XTR */
                  "PSW>U",   /* PSW (32-39) to U */
                  "LMB>U",   /* M to U per MB ctr */
                  "LLB>U",   /* L to U per LB ctr */
              };
static    char     *lu_io_name[] = {
                  "0",
                  "1",
                  "R3>U",    /* Byte 3 to U */
                  "BIB>U",   /* BIB to U */
                  "L0>U",    /* L<0-7> to U */
                  "L1>U",    /* L<8-15> to U */
                  "L2>U",    /* L<16-23> to U */
                  "L3>U",    /* L<24-31> to U */
              };
static    char     *mv_name[] = {  /* Right Mover input */
                  "0",
                  "MLB>V",
                  "MMB>V",
                  "3",       /* Not used */
             };
    char     *mv_io_name[] = {  /* Right Mover input */
                  "0>V",     /* Zero into V */
                  "1",       /* Not used */
                  "BIB>V",
                  "3",       /* Not used */
             };

    /* ZP = ROAR bits 0-5 */
static    char     *zn_name[] = {      /* branch logic */
                  "0",            /* Use ZF */
                  "SMIF",         /* Suppress memory if off bounds and !refetch */
                  "A|(B=0)>A",    /* If ! B branch bit set A to 1 */
                  "A|(B=1)>A",    /* If B branch bit set A to 1 */
                  "",             /* No change */
                  "5",            /* Not used */
                  "B|(A=0)>B",    /* If !A branch bit set B to 1 */
                  "B|(A=1)>B",    /* If A branch bit set B to 1 */
              };
static    char     *zf_name[] = {       /* Special branch functions */
                  "0",
                  "1",              /* Used with SMIF? */
                  "D>ROAR,SCAN",    /* Scan SDR to ROAR */
                  "3",
                  "4",
                  "5",
                  "M(03)>>ROAR",    /* M<0-3> to ROAR */
                  "7",
                  "M(47)>>ROAR",    /* M<4-7> to ROAR */
                  "9",
                  "F>ROAR",       /* F to ROAR */
                  "b",
                  "ED>ROAR",       /* Expf to ROAR */
                  "d",
                  "RETURN>ROAR",   /* Return from micro irq */
                  "f",
              };
static    char     *tr_name[] = {
                  "T",
                  "R",
                  "R0",
                  "M",
                  "D",
                  "L0",
                  "R,A",
                  "L",
                  "HA>A",
                  "R,AN",
                  "R,AW",
                  "R,AD",
                  "D>IAR",
                  "SCAN>D",
                  "R13",
                  "A",
                  "L,A",
                  "R,D",
                  "12",
                  "R,IO",
                  "H",
                  "IA",
                  "FOLD",
                  "17",
                  "M,L",
                  "MLJK",   /* T(12-15)>J T(16-19)>MD, (X=0)>S0,(B=0)>S1, ILC, */
                  "MHL",    /* T(0-3)>MD, T(0-15)>M, BUS(16-31)>M (B=0)>S1 */
                  "MD",
                  "M,SP",
                  "D*BS",
                  "L13",
                  "J",
              };
static    char     *cssa_name[] = {  /* Local Storage Address decode */
                  "2,3,0>LSA",
                  "2,3,1>LSA",
                  "2,3,2>LSA",
                  "2,3,3>LSA",
                  "0,CH,0>LSA",
                  "0,CH,1>LSA",
                  "0,CH,2>LSA",
                  "0,CH,3>LSA",
              };
static    char     *ws_name[] = {   /* Local storage address decode */
                  "0",
                  "WS1>LSA",  /* Select WS1 from local storage */
                  "WS2>LSA",  /* Select WS2 from local storage */
                  "WS,E>LSA",
                  "FN,J>LSA",   /* (R2) > LSA */
                  "FN,J|1>LSA",
                  "FN,MD>LSA",
                  "FN,MD|1>LSA",
              };
static    char     *sf_name[] = {    /* Local Storage controlles */
                  "R>LS",
                  "LS>L,R>LS",
                  "LS>R>LS",
                  "3",
                  "L>LS",
                  "LS>R,L>LS",
                  "LS>L>LS",
                  "7",
              };

static    char     *ct_name[] = { /* IV field under I/O control */
                  "0",           /* Nop */
                  "FIRSTCYCLE",  /* Set channel trap if not first breakout cycle */
                  "DTC1",        /* Send ingate to channel, latch branch control */
                  "DTC2",        /* Send outgate to channel. */
                  "IA 4,4,IAR",  /* Increment IAR by 4, start read cycle */
                  "5",
                  "6",
                  "7",
             };
static    char     *iv_name[] = {
                  "0",
                  "WL>IVD",
                  "WR>IVD",
                  "W>IVD",
                  "IA/4>A,IA",
                  "IA_2/4",
                  "IA+2",
                  "IA+0/2>A",
              };
static    char     *al_name[] = {  /* Shift control */
                  "00",
                  "Q>SR1>F",
                  "L0,!S4>T",
                  "+SGN>T",
                  "-SGN>T",
                  "L0,S4>T",
                  "IA>H",
                  "Q>SL1>-F",
                  "Q>SL1>F",
                  "F>SL1>F",
                  "SL1>Q",
                  "Q>SL1",
                  "SR1>F",
                  "SR1>Q",
                  "Q>SR1>Q",
                  "F>SL1>Q",
                  "SL4>F",
                  "F>SL4>F",
                  "FPSL4",
                  "F>FPSL4",
                  "SR4>F",
                  "F>SR4>F",
                  "FPSR4>F",
                  "1>FPSR4>F",
                  "SR4>H",
                  "F>SR4",
                  "E>FPSL4",
                  "F>SR1>Q",
                  "DKEY>F",
                  "CH>",
                  "D",
                  "AKEY>",
             };
static    char     *wl_name[] = {  /* Mover destination under I/O WL */
                  "0",         /* 0 */
                  "0",       /* 1 */ /* W to M index by MB */
                  "W>L0",    /* 2 */ /* W bits 6-7 to MB */
                  "W>L0",    /* 3 */ /* W bits 6-7 to LB */
                  "W>L1",  /* 4 */ /* W bits 2-7 to PSW 34-39 */
                  "W>L1",    /* 5 */ /* W to PSW 0-7 SSM */
                  "W>L2",      /* 6 */
                  "W>L2",   /* 7 */
                  "W>L3",   /* 8 */ /* W,E(32) select bump sector address */
                  "W>L3",     /* 9 */
                  "W,E>A(BMP)",     /* a */
                  "W,E>A(BMP)",       /* b */
                  "W,E>A(BMP)S",  /* c */
                  "W,E>A(BMP)S",     /* d */
                  "e",      /* e */
                  "f",    /* f */
              };

static    char     *wm_name[] = {    /* Mover destination */
                  "0",         /* 0 */
                  "W>MMB",     /* 1 */ /* W to M index by MB */
                  "W67>MB",    /* 2 */ /* W bits 6-7 to MB */
                  "W67>LB",    /* 3 */ /* W bits 6-7 to LB */
                  "W27>PSW4",  /* 4 */ /* W bits 2-7 to PSW 34-39 */
                  "W>PSW0",    /* 5 */ /* W to PSW 0-7 SSM */
                  "WL>J",      /* 6 */
                  "W>CHCTL",   /* 7 */
                  "W,E>A(BMP)", /* 8 */ /* W,E(32) select bump sector address */
                  "WL>G1",     /* 9 */
                  "WL>G2",     /* a */
                  "W>G",       /* b */
                  "W>MMB(E)",  /* c */
                  "WL>MD",     /* d */
                  "WR>F",      /* e */
                  "W>MD,F",    /* f */
              };

static    char     *ms_name[] = {  /* UP and MD field under I/O */
                  "",
                  "BIB(03)->IOS",
                  "BIB(47)->IOS",
                  "BIB03->IOS*E",
                  "BIB47->IOS*E",
                  "IOS|E",     /* Turn I/O Stats off per emit */
                  "IOS.~E",     /* Turn I/O Stats off per Emit */
                  "BIB4.ERR>IOS"      /* Set I/O stats on CC */
             };
static    char     *md_name[] = {
                  "MD=0",
                  "MD=3",
                  "MD-",
                  "MD+",
              };
static    char     *lb_name[] = {
                  "LB=0",
                  "LB=3",
                  "LB-",
                  "LB+",
              };
static    char     *mb_name[] = {
                  "MB=0",
                  "MB=3",
                  "MB-",
                  "MB+",
              };
static    char     *cg_name[] = { /* LB and MB field under I/O */
                 "",
                 "CH>BI",
                 "1>PRI",
                 "1>LCY",
             };

static    char     *mg_name[] = {  /* DG field under I/O */
                  "BFR2->BIB",
                  "CHPOSTEST",
                  "BFR2->BUSO", /* BFR1->BIB */
                  "BFR1->BIB",  /* BIB->V? */
                  "BOB->BFR1",  /* BFR1->BIB */
                  "BOB->BFR2",
                  "BUSI->BFR1",
                  "BUSI->BFR2"
             };

static    char     *dg_name[] = {
                  "0",
                  "CSTAT>ADDR",   /* Carry to addr */
                  "HOT1>ADDR",    /* 1 to Carry */
                  "G1-1",         /* if G1 == 0 G1Neg = 1, else G1Neg = 0 */
                  "HOT1,G-1",     /* 1 to carry */
                  "G2-1",
                  "G-1",
                  "G1,2-1",
              };
static    char     *ul_name[] = {  /* Mover left input */
                  "E>WL",
                  "U>WL",
                  "V>WL",
                  "?>WL",
             };
static    char     *ur_name[] = {  /* Mover right input */
                  "E>WR",
                  "U>WR",
                  "V>WR",
                  "?>WR",
             };
static    char     *lx_name[] = {  /* Left side of adder input */
                  "0",   /* Adder input 0 */
                  "L",   /* L reg */
                  "SGN", /* Sign bit */
                  "E",   /* Emit * 2 */
                  "LRL", /* Left to Right L */
                  "LWA", /* Lreg | 3, R | 3 */
                  "4",   /* Value 4 */
                  "64C", /* 64 complement for excess 64 correct */
              };
static    char     *lx_io_name[] = {  /* Left side of adder input */
                  "0",   /* Adder input 0 */
                  "L",   /* L reg */
                  "SGN", /* Sign bit */
                  "E",   /* Emit * 2 */
                  "LRL", /* Left to Right L */
                  "LWA", /* Lreg | 3, R | 3 */
                  "IOC",   /* Value 4 */
                  "IO", /* 64 complement for excess 64 correct */
              };
static    char     *ry_name[] = {  /* Right side input */
                  "0",
                  "R",
                  "M",
                  "M23",  /* Bytes 2 and 3 of M */
                  "H",
                  "SEMT",
                  "6",
                  "7",
              };
static    char     *ad_name[] = { /* Adder function */
                  "0",
                  "+",
                  "BCF0",   /* set carry if F=0 */
                  "3",
                  "BC0",    /* Carry from position 0 */
                  "BC^C",   /* Carry test for overflow */
                  "BC1B",
                  "BC8",    /* Carry set from position 8 */
                  "DHL",    /* Decimal half correct Low */
                  "DC0",    /* Decimal correct */
                  "DDC0",
                  "DHH",    /* Decimal halt correct High */
                  "DCBS",
                  "d",
                  "e",
                  "f",
              };
static   char     *cy_name[] = { /* I/O mode selector channel adder latch */
                  "0",        /* nop */
                  "1",        /* undefined */
                  "2",        /* Undefined */
                  "CCW2TEST", /* Test CCW2 for validity */
                  "CATEST",   /* Address check */
                  "UATEST",   /* Compare address in */
                  "LSWDTEST", /* Test for last word */
                  "7",        /* Undefined */
             };
static    char     *ab_name[] = {
                  "0",
                  "1",
                  "S0",
                  "S1",
                  "S2",
                  "S3",
                  "S4",
                  "S5",
                  "S6",
                  "S7",
                  "Carry",
                  "0b",
                  "1SYLS",   /* One Syllable op stat */
                  "LSGNS",   /* L sig stat */
                  "XSGNS",   /* LSS EOR RSS */
                  "0f",
                  "CRMD",  /* MD Bit 0 & Not Cond Reg Pos2 & Not cond Reg Pos 3 */
                  "W=0",
                  "WL=0",
                  "WR=0",  /* MVR LTH 4-7 eq zero */
                  "MD=FP",    /* Branch if MD<0>==0 and MD<3> == 0 */
                  "MB=3",  /* Branch if MB=3 */
                  "MD3=0",
                  "G1=0",
                  "G1NEG",
                  "G<4",
                  "G1MBZ",
                  "IOS0",
                  "IOS2",
                  "R(31)",   /* R Reg Pos 31 */
                  "F(2)",    /* F Reg Pos 2 */
                  "L(0)",    /* L Reg Pos 0 */
                  "F=0",
                  "UNORM",
                  "TZ*BS",
                  "EDITPAT",
                  "PROB",    /* In problem state */
                  "TIMUP",
                  "26",
                  "GZ/MB3",
                  "28",
                  "LOG",
                  "STC=0",
                  "G2<=LB",
                  "2c",
                  "D(7)",
                  "SCPS",
                  "SCFS",
                  "CROS",
                  "W(67)-AB",
                  "Z23!0",
                  "CCW20K",   /* T(5-7) = 0 && T(16-31) == 0 */
                  "MXBIO",    /* Test bit 0 if Buffer input */
                  "IBFULL",
                  "CANG",
                  "CHLOG",
                  "I-FETCH",   /* I Fetch Stat FCN A, 00 Odd-no ref, 10 even, 01 odd- ref, 11=? */
                  "IA(30)",
                  "EXT,CHIRPT",
                  "3b",
                  "PSS",
                  "IOS4",
                  "3e",
                  "RX.S0",
              };
static    char     *bb_name[] = {
                  "0",
                  "1",
                  "S0",
                  "S1",
                  "S2",
                  "S3",
                  "S4",
                  "S5",
                  "S6",
                  "S7",
                  "RSGN",
                  "HSCH",     /* HS Channel Specail branch */
                  "EXC",    /* Exception branch */
                  "WR=0",     /* execption branch */
                  "0e",     /* Mover Lth 4-7 eq zero */
                  "T13=0",     /* T 8-31 zero reg */
                  "T(0)",     /* T Pos 0 to reg */
                  "T=0",     /* T 0-31 zero reg */
                  "TZ*BS",     /* T Zero per Byte stats */
                  "W=1",     /* MVR LTH eq One */
                  "LB=0",     /* BAL eq 0 */
                  "LB=3",     /* BAL Bit 0 & Bit 1 */
                  "MD=0",     /* MD eq zero */
                  "G2=0",     /* Gz eq zero */
                  "G2NEG",     /* G2 less than zero */
                  "G2LBZ",     /* G2 eq zero or BAL eq Zero */
                  "IOS1",     /* I-O Stat 1 to CPU */
                  "MD/JI",     /* MD Odd Gt 8 or J Odd gt 8 */
                  "IVA",     /* Br invalid addr */
                  "IOS3",     /* I-O Stat 3 */
                  "(CAR)",     /* BR Immeded on Carry LTH */
                  "(Z00)",
              };
static    char     *ss_name[] = {
                  "00",
                  "01",
                  "02",
                  "D->CR*BS",
                  "E->SCHANCTL",
                  "L,RSGNS",
                  "IVD/RSGNS",
                  "EDITSGN",
                  "E->S03",
                  "S03|E,1->LSGN",
                  "S03|E",
                  "S03|E,0->BS",
                  "X0,B0,1SYL",
                  "FPZERO",
                  "FPZER,E->FN",
                  "B0,1SYL",
                  "S03.!E",
                  "(T=0)->S3",
                  "E->BS,T30->S3",  /* T(30) to S3 */
                  "E->BS",
                  "1->BS*MB",
                  "DIRCTL*E",
                  "16",
                  "MANUAL>STOP",
                  "E>S47",
                  "S47|E",
                  "S47&~E",
                  "S47,ED*FP",
                  "OPPANEL->S47",
                  "CAR,(T!=0)->CR",
                  "KEY->F",
                  "F->KEY",
                  "1->LSGNS",
                  "0->LSGNS",
                  "1->RSGNS",
                  "0->RSGNS",
                  "L(0)->LSGNS",
                  "R(0)->RSGNS",
                  "E(13)>WFN",
                  "E(23)->LSFN",   /* Set FN to E, 00-  01-   10-  11- Gen Reg */
                  "E(23)->CR",
                  "SETCRALG",
                  "SETCRLOG",
                  "!S4,S4->CR",
                  "S4,!S4->CR",
                  "1->REFETCH",
                  "SYNC->OPPANEL",
                  "SCAN*E,10",
                  "1->SUPOUT",
                  "31",
                  "E(0)->IBFULL",
                  "33",
                  "E->CH",
                  "35",
                  "1->TIMERIRPT",
                  "T->PSW,IPL->T",
                  "T->PSW T(12-15)",
                  "SCAN*E,00",
                  "1->IOMODE",
                  "3b",
                  "1->SELOUT",
                  "1->ADROUT",
                  "1->COMOUT",
                  "1->SERVOUT",
              };

void
cycle_2050()
{
    struct ROS_2050  *sal;
    int              a_bit = 0;
    int              b_bit = 0;
    int              carry_in;
    int              carry_out = 0;
    int              rest_roar = 0;
    uint8_t          s_update;
    uint8_t          md_old;
    uint32_t         l_update;
    uint32_t         carries = 0;
    uint32_t         t1, t2;
    int              t;
    int              i;
    struct _device  *dev;
    int              exc;

    dtc1 = dtc2 = 0;
    if (RATE_SW != 1 || ADR_CMP != 0 || ROS_CMP != 0 || INST_REP != 0 ||
        INT_TMR != 0 || SAR_CMP != 0) {
        test_mode = 1;
    } else {
        test_mode = 0;
    }

    /* Handle interval timer */
    if (timer_event) {
        timer_event = 0;
        if (!INT_TMR && allow_man_operation == 0) {
           log_trace("Update timer\n");
           timer_update = 1;
        }
    }

    if (RATE_SW == 2 && START) {
        allow_man_operation = 1;
    }

    cpu_2050.OPPANEL = 0;
    exc = allow_man_operation | wait;
    if (stop_mode == 0) {
        if (timer_update || ((INTR | timer_irq) && (cpu_2050.MASK & BIT7) != 0))
            exc = 1;
        if ((cpu_2050.BCHI & cpu_2050.MASK) != 0)
            exc = 1;
        if (ADR_CMP != 0)
            exc = 1;
    }

    /* Handle front panel switch */
    if (DISPLAY | STORE) {
        switch(E_SW) {
        case 0:  cpu_2050.OPPANEL |= 0xc; break; /* Local store */
        case 1:  cpu_2050.OPPANEL |= 0x8; break; /* Main store */
        case 2:  cpu_2050.OPPANEL |= 0xa; break; /* Protect tags */
        case 3:  cpu_2050.OPPANEL |= 0xe; break; /* MPX bump store */
        }
    }

    /* Convert panel switches to microcode functions */
    if ((STORE | SET_IC) && allow_man_operation) {
        cpu_2050.OPPANEL |= 0x1;
    }

    if (SET_IC && allow_man_operation) {
        cpu_2050.OPPANEL |= 0x2;
    }

    if (INST_REP == 1) {
        cpu_2050.OPPANEL |= 0x3;
    }

    if (SAR_CMP && cpu_2050.SAR_REG == cpu_2050.AKEYS) {
        allow_man_operation = 1;
    }

    if (START) {
        stop_mode = 0;
        allow_man_operation = 0;
        if (RATE_SW == 0 && (DISPLAY & SET_IC) == 0 && wait == 0)
            cpu_2050.OPPANEL |= 1;
    }

    if (STOP) {
        STOP = 0;
        allow_man_operation = 1;
    }

    if (LOAD) {
        printf("Load\n");
        load_mode = 1;
        stop_mode = 0;
        allow_man_operation = 0;
        for (i = 0; i < 4; i++) {
            cpu_2050.TAGS[i] = 0;
            cpu_2050.polling[i] = 1;
            cpu_2050.ROUTINE[i] = 0;
            cpu_2050.CHREQ[i] = 0;
            cpu_2050.CHPOS[i] = 0;
        }
        cpu_2050.ROAR = 0x240;
        cpu_2050.init_mem = 0;
        cpu_2050.init_bump_mem = 0;
        cpu_2050.bump_mem = 0;
        cpu_2050.mem_state = 0;
        LOAD = 0;
        INTR = 0;
        wait = 0;
        timer_irq = 0;
        timer_update = 0;
    }

    if (SEL_ENTER && CHN_MODE != 0) {
        cpu_2050.OPPANEL |= 0x6;
    }

    /* Reset system */
    if (SYS_RST) {
        cpu_2050.ROAR = 0x242;
        allow_man_operation = 1;
        for (i = 0; i < 4; i++) {
            cpu_2050.TAGS[i] = 0;
            cpu_2050.polling[i] = 1;
            cpu_2050.ROUTINE[i] = 0;
            cpu_2050.CHREQ[i] = 0;
            cpu_2050.CHPOS[i] = 0;
            cpu_2050.C1[i] = 0;
            cpu_2050.C2[i] = 0;
            cpu_2050.C3[i] = 0;
            cpu_2050.C4[i] = 0;
            cpu_2050.D1[i] = 0;
            cpu_2050.D2[i] = 0;
        }
        cpu_2050.KEY = 0;
        cpu_2050.CC = 0;
        cpu_2050.MASK = 0;
        cpu_2050.PMASK = 0;
        cpu_2050.AMWP = 0;
        cpu_2050.BCHI = 0;
//        cpu_2050.IBFULL = 0;
        cpu_2050.S_REG = 0;
        cpu_2050.init_mem = 0;
        cpu_2050.init_bump_mem = 0;
        cpu_2050.bump_mem = 0;
        cpu_2050.break_in = 0;
        cpu_2050.break_out = 0;
        cpu_2050.first_cycle = 0;
        cpu_2050.last_cycle = 0;
        SYS_RST = 0;
        INTR = 0;
        wait = 0;
        timer_irq = 0;
        timer_update = 0;
    }

    /* Handle Reset ROAR function */
    if (ROAR_RST) {
        cpu_2050.ROAR = 0x2c2;
        allow_man_operation = 1;
        cpu_2050.init_mem = 0;
        cpu_2050.init_bump_mem = 0;
        cpu_2050.bump_mem = 0;
        ROAR_RST = 0;
    }
    log_trace("OPPanel %x\n", cpu_2050.OPPANEL);

    sal = &ros_2050[cpu_2050.ROAR];
    cpu_2050.ros_row1 = sal->row1;
    cpu_2050.ros_row2 = sal->row2;
    cpu_2050.ros_row3 = sal->row3;
    cpu_2050.ros_row4 = sal->row4;

    /* Stop running microsteps if ROAR matches */
    if (ROS_CMP == 1 && cpu_2050.ROAR == (cpu_2050.DKEYS & 0xfff)) {
       goto channel;
    }

    /* Process on cycle if in single cycle mode */
    if (RATE_SW == 2) {
       if (START == 0)
           goto channel;
       START = 0;
    }

    /* Main memory cycle */
    switch (cpu_2050.mem_state) {
    case R1:
             cpu_2050.mem_state = R2;
             log_mem("mem cycle read %06X %08x %d\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG, sal->tr);
             /* Check if this cycle will access the SDR register, wait */
             if (tr_interloc & (1 << sal->tr))
                 goto channel;
             if (sal->al == 30 || sal->ss == 3 || sal->iv == 4 || sal->iv == 7)
                 goto channel;
             /* If we are initiating a memory cycle, or updating SDR, wait */
             if (cpu_2050.init_mem | cpu_2050.init_bump_mem | cpu_2050.update_d)
                 goto channel;
             break;
    case R2:
             cpu_2050.mem_state = R3;
             if (!cpu_2050.update_d || sal->tr == 29) {
                 if (cpu_2050.bump_mem)
                     cpu_2050.SDR_REG = cpu_2050.BUMP[SA >> 2];
                 else
                     cpu_2050.SDR_REG = M[SA >> 2];
             }
             log_mem("mem cycle read 2 %06X %08x i=%d ib=%d b=%d\n", cpu_2050.SAR_REG,
                   cpu_2050.SDR_REG, cpu_2050.init_mem, cpu_2050.init_bump_mem, cpu_2050.bump_mem);
             /* Check if this cycle will access the SDR register, wait */
             if (tr_interloc & (1 << sal->tr))
                 goto channel;
             if (sal->al == 30 || sal->ss == 3 || sal->iv == 4 || sal->iv == 7)
                 goto channel;
             /* If we are initiating a memory cycle, or updating SDR, wait */
             if (cpu_2050.init_mem | cpu_2050.init_bump_mem | cpu_2050.update_d)
                 goto channel;
             break;
    case R3:
             log_mem("mem cycle read 3 %06X %08x i=%d ib=%d b=%d\n", cpu_2050.SAR_REG,
                   cpu_2050.SDR_REG, cpu_2050.init_mem, cpu_2050.init_bump_mem, cpu_2050.bump_mem);
             cpu_2050.mem_state = W1;
             /* If we are initiating a memory cycle, wait */
             if (cpu_2050.init_mem | cpu_2050.init_bump_mem)
                 goto channel;
             if (cpu_2050.update_d || sal->tr == 29) {
                 if (cpu_2050.bump_mem)
                     cpu_2050.BUMP[SA >> 2] = cpu_2050.SDR_REG;
                 else
                     M[SA >> 2] = cpu_2050.SDR_REG;
             }
             break;
    case 0:  /* Memory cycle at system reset */
             cpu_2050.SDR_REG = M[SA >> 2];
             cpu_2050.mem_state = W1;
    case W1:
             /* Fall through */
             if (cpu_2050.update_d) {
                 cpu_2050.update_d = 0;
                 if (cpu_2050.bump_mem) {
                     cpu_2050.BUMP[SA >> 2] = cpu_2050.SDR_REG;
                 } else {
                    M[SA >> 2] = cpu_2050.SDR_REG;
                 }
             }
             SA = cpu_2050.SAR_REG & 0x3ffff;

             log_mem("mem write  %06X %08x\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG);

             if (cpu_2050.init_mem | cpu_2050.init_bump_mem) {
                  log_mem("mem init  %06X %08x %08x\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG,
                            mem_max);
                 cpu_2050.bump_mem = cpu_2050.init_bump_mem;
                 cpu_2050.init_mem = cpu_2050.init_bump_mem = 0;
                 cpu_2050.IVA = 0;
                 if (cpu_2050.SAR_REG > mem_max) {
                     cpu_2050.ROAR = IVAD;
                     cpu_2050.IVA = 1;
                     goto channel;
                 }
                 cpu_2050.mem_state = R1;
                 if (sal->al == 30 || sal->ss == 3 || sal->iv == 4 || sal->iv == 7 ||
                     sal->tr == 29)
                     goto channel;
             }

             break;
    }

    if ((log_level & LOG_ITRACE) != 0 && (cpu_2050.ROAR == 0x188)) {
        uint8_t   mem[6];
        int       i;
        for (i = 0; i < 6; i++) {
            uint32_t pc = i + cpu_2050.IA_REG;
            uint32_t mm = M[pc >> 2];
            mem[i] = (mm >> (8 * (3 - (pc & 3)))) & 0xff;
        }

        print_inst(mem);
        log_itrace_c(" IC=%06x CC=%x", cpu_2050.IA_REG, cpu_2050.CC);
        log_itrace("\n");

        log_itrace_s(" ");
        for (i = 0; i < 16; i++) {
            log_itrace_c(" GR%02d = %08x", i, cpu_2050.LS[0x30 + i]);
            if ((i & 0x3) == 0x3) {
                 log_itrace_s(" ");
            }
        }
    }

    /* Disassemble micro instruction */
    if ((log_level & LOG_MICRO) != 0) {
        char       dis_buffer[1024];
        char       t_buffer[100];
        char       *p;

        sprintf(dis_buffer, "%s %03X: E=%0X ", sal->note, cpu_2050.ROAR, sal->ce);
        if (sal->ry != 0 || sal->lx == 0) {
            if (sal->al == 0x1e) {
                strcat(dis_buffer, al_name[sal->al]);
            } else {
                strcat(dis_buffer, ry_name[sal->ry]);
            }
        }
        if (sal->lx != 0 || sal->tc == 0) {
           if (sal->io == 1) {
               if (sal->ry != 0 && sal->tc == 1) {
                  strcat(dis_buffer, "+");
               }
           }
           if (sal->tc == 0) {
              strcat(dis_buffer, "-");
           } else {
              strcat(dis_buffer, "+");
           }
           strcat(dis_buffer, (sal->io) ? lx_io_name[sal->lx] : lx_name[sal->lx]);
        }
        strcat(dis_buffer, ">");
        p = tr_name[sal->tr];
        if (sal->io && sal->tr == 28) {
            p = "D*BI";
        }
        if (sal->io && sal->tr == 31) {
            p = "IO";
        }
        strcat(dis_buffer, p);
        strcat(dis_buffer, " ");
        if (sal->ad >= 2) {
            strcat(dis_buffer, ad_name[sal->ad]);
            strcat(dis_buffer, " ");
        }
        if (sal->al != 0 && sal->al != 0x1e) {
            strcat(dis_buffer, al_name[sal->al]);
            strcat(dis_buffer, " ");
        }
        if (sal->lu != 0) {
            if (sal->io) {
                strcat(dis_buffer, lu_io_name[sal->lu]);
            } else {
                strcat(dis_buffer, lu_name[sal->lu]);
            }
            strcat(dis_buffer, " ");
        }
        if (sal->mv != 0) {
            strcat(dis_buffer, (sal->io) ? mv_io_name[sal->mv] : mv_name[sal->mv]);
        }
        strcat(dis_buffer, " ");
        if (sal->ur == sal->ul) {
            switch (sal->ur) {
            case 0:  strcat(dis_buffer, "E>W"); break;
            case 1:  strcat(dis_buffer, "U>W"); break;
            case 2:  strcat(dis_buffer, "V>W"); break;
            case 3:  strcat(dis_buffer, "?>W"); break;
            }
        } else {
            strcat(dis_buffer, ul_name[sal->ul]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, ur_name[sal->ur]);
        }
        strcat(dis_buffer, " ");
        strcat(dis_buffer, (sal->io) ? wl_name[sal->wm] : wm_name[sal->wm]);
        strcat(dis_buffer, " ");
        if (sal->sf != 7) {
            strcat(dis_buffer, (sal->io) ? cssa_name[sal->ws]: ws_name[sal->ws]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, sf_name[sal->sf]);
        }

        if (sal->io == 1) {
            strcat(dis_buffer, " ");
            strcat(dis_buffer, ms_name[(sal->up<<1)|sal->md]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, cg_name[(sal->lb<<1) |sal->mb]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, ct_name[sal->iv]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, mg_name[sal->dg]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, cy_name[sal->ad>>1]);
            if (cpu_2050.chpostest) {
                strcat(dis_buffer, " ");
                /* Process MS field */
                if (sal->md & 1)
                    strcat(dis_buffer, "C0 ");
                if (sal->up & 1)
                    strcat(dis_buffer, "C1 ");
                if (sal->up & 2)
                    strcat(dis_buffer, "C2 ");
                if (sal->lu & 1)
                    strcat(dis_buffer, "UA ");
                if (sal->lu & 2)
                    strcat(dis_buffer, "CCW2 ");
                if (sal->lu & 4)
                    strcat(dis_buffer, "CCW1 ");
                if (sal->mv & 2)
                    strcat(dis_buffer, "RD ");
                if (sal->ul & 2)
                    strcat(dis_buffer, "WR ");
                if (sal->ul & 1)
                    strcat(dis_buffer, "END ");
                if (sal->ur & 2)
                    strcat(dis_buffer, "COMP ");
                if (sal->ur & 1)
                    strcat(dis_buffer, "INTR ");
            }
        } else {
            if (sal->md) {
              strcat(dis_buffer, " ");
              strcat(dis_buffer, md_name[sal->up]);
            }
            if (sal->lb) {
              strcat(dis_buffer, " ");
              strcat(dis_buffer, lb_name[sal->up]);
            }
            if (sal->mb) {
              strcat(dis_buffer, " ");
              strcat(dis_buffer, mb_name[sal->up]);
            }
            strcat(dis_buffer, " ");
            strcat(dis_buffer, iv_name[sal->iv]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, dg_name[sal->dg]);
        }
        if (sal->ss != 0) {
            strcat(dis_buffer, " ");
            strcat(dis_buffer, ss_name[sal->ss]);
        }
        strcat(dis_buffer, " ");
        strcat(dis_buffer, ab_name[sal->ab]);
        strcat(dis_buffer, " ");
        strcat(dis_buffer, bb_name[sal->bb]);
        strcat(dis_buffer, " ");

        if (sal->zn != 0) {
           sprintf(t_buffer, "%03X %s", sal->zp | (sal->zf << 2), zn_name[sal->zn]);
        } else {
           sprintf(t_buffer, "%03X %s", sal->zp, zf_name[sal->zf]);
        }
        strcat(dis_buffer, t_buffer);
        log_micro("%s\n", dis_buffer);
    }

    /* Make sure control functions are at right spot */
    if (cpu_2050.chpostest) {
        uint16_t  postest = 0;
        /* Process MS field */
        if (sal->lu & 1)
            postest |= 0x100; /* UA Fetch */
        if (sal->lu & 2)
            postest |= BIT1;  /* CCW2 */
        if (sal->lu & 4) {
            postest |= BIT0;  /* CCW1 */
        }
        if (sal->mv & 2)
            postest |= BIT3;  /* RD Store */
        if (sal->ul & 2)
            postest |= BIT4;  /* WR Fetch */
        if (sal->ul & 1)
            postest |= BIT5;  /* END update */
        if (sal->ur & 2)
            postest |= BIT6;  /* Comp */
        if (sal->ur & 1)
            postest |= BIT7;  /* Intrp */
        if (cpu_2050.CHPOS[cpu_2050.CH] != postest) {
            log_micro("Position does not match %02x %02x\n",
                cpu_2050.CHPOS[cpu_2050.CH], postest);
        }
        cpu_2050.chpostest = 0;
    }

    /* Basic next address */
    cpu_2050.NROAR = sal->zp;

    /* Set bit 1 of next addressed based on conditions */
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
            a_bit = (cpu_2050.CAR != 0);
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
            a_bit = (cpu_2050.LSGNS ^ cpu_2050.RSGNS);
            break;
    case 15: /* Unknown */
            break;
    case 16: /* CRMD, mask condition codes */
            a_bit = (cpu_2050.MD_REG & (8 >> cpu_2050.CC)) != 0;
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
            a_bit = !(cpu_2050.MD_REG & 1);
            break;
    case 23: /* G1 == 0 */
            a_bit = ((cpu_2050.G_REG & 0xf0) == 0);
            break;
    case 24: /* G1NEG */
            a_bit = (cpu_2050.G1NEG != 0);
            break;
    case 25: /* G<4 */
            a_bit = (cpu_2050.G_REG < 4) | (cpu_2050.G1NEG != 0);
            break;
    case 26: /* G1MBZ */
            a_bit = ((cpu_2050.G_REG & 0xf0) == 0) | (cpu_2050.MB_REG == 0);
            break;
    case 27: /* IO Stat 0 to CPU */
            a_bit = (cpu_2050.IOSTAT[cpu_2050.CH]  & BIT0) != 0;
            break;
    case 28: /* IO Stat 2  */
            a_bit = (cpu_2050.IOSTAT[cpu_2050.CH]  & BIT2) != 0;
            break;
    case 29: /* R(31)  */
            a_bit = ((cpu_2050.R_REG & 1) != 0);
            break;
    case 30: /* F(2)  */
            a_bit = ((cpu_2050.F_REG & 2) != 0);
            break;
    case 31: /* L(0)  */
            a_bit = ((cpu_2050.L_REG & MSIGN) != 0);
            break;
    case 32: /* F=0  */
            a_bit = (cpu_2050.F_REG == 0);
            break;
    case 33: /* UNORM,  T8-11 zero and not S0  */
            a_bit = ((cpu_2050.aob_latch & 0x00f00000) == 0 &&
                        (cpu_2050.S_REG & BIT0) == 0);
            break;
    case 34: /* TZ*BS */
            a_bit = ((cpu_2050.alu_out & BS_MASK[cpu_2050.BS_REG]) == 0);
            break;
    case 35: /* EDITPAT  */
            a_bit = ((cpu_2050.ED_STAT & 0x2) != 0);
            b_bit = ((cpu_2050.ED_STAT & 0x1) != 0);
            break;
    case 36: /* PROB, in problem state */
            a_bit = (cpu_2050.AMWP & 1) != 0;
            break;
    case 37: /* TIMUP, timer update signal  */
            if (timer_update)
               a_bit = 1;
            timer_update = 0;
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
    case 50: /* Z23!=0 T16-31 != 0  */
            a_bit = (cpu_2050.alu_out & 0xFFFF) != 0;
            break;
    case 51: /* CCW2OK T5-7 == 0 && T16-31 != 0  */
            a_bit = ((cpu_2050.alu_out & 0xFFFF) == 0) |
                     ((cpu_2050.alu_out & 0x07000000) != 0);
            break;
    case 52: /* MXBIO  bus in bit 0  */
            a_bit = (cpu_2050.BUS_IN[0] & 0x80) != 0;
            break;
    case 53: /* IBFULL IB full  */
            a_bit = cpu_2050.IBFULL;
            break;
    case 54: /* CANG: (29-31) != 0, CA not good  */
            a_bit = ((cpu_2050.alu_out & 0x7) != 0) || cpu_2050.IVA;
            break;
    case 55: /* CHLOG, channel log   */
            break;
    case 56: /* I-FETCH,   */
            if (exc) {
               a_bit = 1;
               b_bit = 1;
            } else {
               a_bit = ((cpu_2050.IA_REG & 0x2) == 0);
               if (!a_bit) {
                  b_bit = cpu_2050.REFETCH;
               }
            }
            break;
    case 57: /* IA(30)  */
            a_bit = ((cpu_2050.IA_REG & 0x2) != 0);
            break;
    case 58: /* EXT,CHIRPT  */
            if (timer_update) {   /* Timer update, give it priority */
               b_bit = 1;
               a_bit = 1;
            }
            if ((cpu_2050.BCHI & cpu_2050.MASK) != 0) {
               b_bit = 1;
            }
            if ((cpu_2050.MASK & BIT7) != 0 && (INTR | timer_irq)) {
               a_bit = 1;
            }
            break;
    case 59: /* CROS: Direct date hold sense branch  */
            break;
    case 60: /* PSS,  */
            break;
    case 61: /* IO Stat 4  */
            a_bit = ((cpu_2050.IOSTAT[cpu_2050.CH]  & BIT4) != 0);
            break;
    case 62: /*  Undefined   */
            break;
    case 63: /*  RX.S0     */
            a_bit = 0;
            if ((cpu_2050.S_REG & BIT0) != 0 &&
                 ((cpu_2050.M_REG & 0xc0000000) == BIT1L))
                a_bit = 1;
            break;
    }

    /* Set bit 0 of next addressed based on conditions */
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
            /* Set to one if High speed channel or 256 sub channel option */
            break;
    case 12: /* EXC  exception branch */
            b_bit |= exc;
            break;
    case 13: /* WR=0 MVR LTH 4-7 eq zero */
            b_bit |= ((cpu_2050.w_bus & 0xf) == 0);
            break;
    case 14: /* Unknown */
            break;
    case 15: /* T13=0 bits 8-31 == 0 */
            b_bit |= ((cpu_2050.aob_latch & 0x00ffffff) == 0);
            break;
    case 16: /* T(0) */
            b_bit |= ((cpu_2050.aob_latch & MSIGN) != 0);
            break;
    case 17: /* T = 0 */
            b_bit |= (cpu_2050.aob_latch == 0);
            break;
    case 18: /* TZ*BS */
            b_bit |= ((cpu_2050.aob_latch & BS_MASK[cpu_2050.BS_REG]) == 0);
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
            b_bit |= (cpu_2050.IOSTAT[cpu_2050.CH]  & BIT1) != 0;
            break;
    case 27: /* MD/JI MD Odd gt 8 or J odd gt 8 */
            b_bit |= ((cpu_2050.MD_REG & 0x9) != 0 || (cpu_2050.J_REG & 0x9) != 0);
            break;
    case 28: /* IVA  */
            b_bit = cpu_2050.IVA;
            break;
    case 29: /* IO Stat 3 */
            b_bit |= (cpu_2050.IOSTAT[cpu_2050.CH]  & BIT3) != 0;
            break;
    case 30: /* (CAR) branch on carry latch  */
            break;
    case 31: /* (Z00) looks at current T */
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
                     cpu_2050.NROAR = ((cpu_2050.SDR_REG >> 1) & 0xfff);
                     cpu_2050.io_mode = ((cpu_2050.SDR_REG & BIT7) != 0);
                     break;
            case 3:  /* Undefined */
                     break;
            case 4:  /* Undefined */
                     break;
            case 5:  /* Undefined */
                     break;
            case 6:  /* M(03)->ROAR */
                     cpu_2050.NROAR |= ((cpu_2050.M_REG >> 28) & 0xf) << 2;
                     break;
            case 7:  /* Undefined */
                     break;
            case 8:  /* M(47)->ROAR */
                     cpu_2050.NROAR |= ((cpu_2050.M_REG >> 24) & 0xf) << 2;
                     break;
            case 9:  /* Undefined */
                     break;
            case 10:  /* F->ROAR */
                     cpu_2050.NROAR |= (cpu_2050.F_REG & 0xf) << 2;
                     break;
            case 11:  /* Undefined */
                     break;
            case 12:  /* ED->ROAR exp diff */
                     cpu_2050.NROAR |= (cpu_2050.ED_REG & 0xf) << 2;
                     break;
            case 13:  /* Undefined */
                     break;
            case 14:  /* RETURN->ROAR */
                     rest_roar = 1;
                     break;
            case 15:  /* Undefined */
                     break;
            }
            break;
    case 1: /* SMIF suppress memory instruction fetch */
            /* By order IV7 if refetch == 0 and IAR bit 30 == 1 */
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 2: /* A|(B=0)->A */
            a_bit |= !b_bit;
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 3: /* A|(B=1)->A */
            a_bit |= b_bit;
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 4: /* Normal no change */
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 5: /* Force Invalid Op trap address */
            /* Not used. */
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 6: /* B|(A=0)->B */
            b_bit |= !a_bit;
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    case 7: /* B|(A=1)->B */
            b_bit |= a_bit;
            cpu_2050.NROAR |= sal->zf << 2;
            break;
    }

    /* Save next ROAR address. */
    cpu_2050.NROAR |= (a_bit << 1)|b_bit;


    /* On dead cycle, save state, and ignore rest of cycle */
    if (cpu_2050.dead_cycle) {
        cpu_2050.PROAR = cpu_2050.ROAR;
        cpu_2050.BROAR = cpu_2050.NROAR;
        cpu_2050.ROAR = cpu_2050.ROUTINE[cpu_2050.CH];
        cpu_2050.LS[0x2c] = cpu_2050.R_REG;  /* Save R register for later */
        cpu_2050.MPX_LST = 0;
        cpu_2050.dead_cycle = 0;
        cpu_2050.first_cycle = 1;
        cpu_2050.io_mode = 1;
        log_trace("Transfer %02x %d\n", cpu_2050.ROAR, cpu_2050.CH);
        goto channel;
    }

    /* On restore cycle reset the ROAR to the previous and run with last next state */
    if (cpu_2050.rest_cycle) {
        cpu_2050.io_mode = 0;
        cpu_2050.NROAR = cpu_2050.BROAR;
        log_trace("S REG %02x\n", cpu_2050.S_REG);
        cpu_2050.break_out = 0;
    }

    /* Some registers can't be changed during operation. Save these.
       Any updates to these registers should be to the update variable.
       At end of cycle, these values will update the register. */
    s_update = cpu_2050.S_REG;
    l_update = cpu_2050.L_REG;
    md_old = cpu_2050.MD_REG;

    /* Set up L and R inputs */
    switch (sal->lx) {
    case 0: /*  0  */
            cpu_2050.left_bus = 0;
            break;
    case 1: /*  L  */
            cpu_2050.left_bus = cpu_2050.L_REG;
            break;
    case 2: /*  SGN  */
            cpu_2050.left_bus = MSIGN;
            break;
    case 3: /*  E  CE bit < 1 */
            cpu_2050.left_bus = sal->ce << 1;
            break;
    case 4: /*  LRL  */
            cpu_2050.left_bus = (cpu_2050.L_REG & 0xffff) << 16;
            break;
    case 5: /*  LWA  */
            cpu_2050.left_bus = cpu_2050.L_REG | 3;
            break;
    case 6: /* Complement of IOC register */
            if (cpu_2050.io_mode) /* IOC */
                cpu_2050.left_bus = 0x3 ^ (cpu_2050.IO_REG & 3);
            else /*  4  */
                cpu_2050.left_bus = 4;
            break;
    case 7: /*  64C  */
            /* IO Reg in I/O mode */
            if (cpu_2050.io_mode)
               cpu_2050.left_bus = cpu_2050.IO_REG & 3;
            else
               cpu_2050.left_bus = 0xc0000000;
            break;
    }

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

    if (sal->lx == 5) { /*  LWA  */
       cpu_2050.right_bus |= 3;
    }

    if (sal->ss == 42) {
       cpu_2050.left_bus &= BS_MASK[cpu_2050.BS_REG];
    }

    if (!sal->tc)
        cpu_2050.left_bus ^= 0xffffffff;

    /* I/O Mode on MPX channel */
    if (cpu_2050.io_mode) {
        uint8_t   bib = 0xff;
        /* MG field in I/O Mode */

        if (cpu_2050.CH == 0x3) {
            switch (sal->dg) {
            case 0: /* BFR2>BIB.
                       Gate mpx channel buffer 2 to buffer in bus.  */
                    bib = cpu_2050.BFR2;
                    break;
            case 1: /* CHPOSTEST.
                       Initiate selector channel position test */
                    cpu_2050.chpostest = 1;
                    break;
            case 2: /* BFR2>BUSO.
                       Gate MPX channel buffer 1 to buffer in bus,
                       Gate MPX channel buffer 2 to bus out (I/O interface) */
                    bib = cpu_2050.BFR1;
                    break;
            case 3: /* BFR1>BIB.
                       Gate MPX channel buffer 1 to buffer in bus. */
                    bib = cpu_2050.BFR1;
                    break;
            case 4: /* BOB>BFR1.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then Gate buffer out bus to channel buffer 1. */
                    bib = cpu_2050.BFR1;
                    break;
            case 5: /* BOB>BFR2.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then Gate buffer out bus to channel buffer 2. */
                    bib = cpu_2050.BFR1;
                    break;
            case 6: /* BUSI>BFR1.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then bus in (I/O interface) to channel buffer 1 */
                    bib = cpu_2050.BFR1;
                    break;
            case 7: /* BUSI>BFR2.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then bus in (I/O interface) to channel buffer 2 */
                    bib = cpu_2050.BFR1;
                    break;
            }
        } else {
            switch (sal->dg) {
            case 0: /* BFR2>BIB.
                       Gate mpx channel buffer 2 to buffer in bus.  */
                    break;
            case 1: /* CHPOSTEST.
                       Initiate selector channel position test */
                    cpu_2050.chpostest = 1;
                    break;
            case 2: /* BFR2>BUSO.
                       Gate MPX channel buffer 1 to buffer in bus,
                       Gate MPX channel buffer 2 to bus out (I/O interface) */
                    break;
            case 3: /* BFR1>BIB.
                       Gate MPX channel buffer 1 to buffer in bus. */
                    if (cpu_2050.CHPOS[cpu_2050.CH] == BIT7) {  /* .IRPT */
                         bib = (cpu_2050.CH + 1) & 3;
                    } else {
                         bib = 0;
                    }
                    break;
            case 4: /* BOB>BFR1.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then Gate buffer out bus to channel buffer 1. */
                    break;
            case 5: /* BOB>BFR2.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then Gate buffer out bus to channel buffer 2. */
                    break;
            case 6: /* BUSI>BFR1.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then bus in (I/O interface) to channel buffer 1 */
                    break;
            case 7: /* BUSI>BFR2.
                       Gate MPX channel buffer 1 to buffer in bus.
                       Then bus in (I/O interface) to channel buffer 2 */
                    bib = cpu_2050.BFR1;
                    break;
            }
        }

        log_trace("dg=%d BFR2=%02x bib = %02x CH=%x\n", sal->dg, cpu_2050.BFR2, bib, cpu_2050.CH);
        /* Process MS field */
        switch ((sal->up << 1) | sal->md) {
        case 0:  /* Nop */
                break;
        case 1:  /* BIB(03)>IOS */
                cpu_2050.IOSTAT[cpu_2050.CH]  = (bib) & 0xf0;
                break;
        case 2:  /* BIB(47)>IOS */
                cpu_2050.IOSTAT[cpu_2050.CH]  = (bib & 0xf) << 4;
                break;
        case 3:  /* BIB03>IOS*E */
                cpu_2050.IOSTAT[cpu_2050.CH]  &= ~(sal->ce << 4);
                cpu_2050.IOSTAT[cpu_2050.CH]  |= (bib & 0xf0) & (sal->ce << 4);
                break;
        case 4:  /* BIB47>IOS*E */
                cpu_2050.IOSTAT[cpu_2050.CH]  &= ~(sal->ce << 4);
                cpu_2050.IOSTAT[cpu_2050.CH]  |= (bib & sal->ce & 0xf) << 4;
                break;
        case 5:  /* IOS|E */
                cpu_2050.IOSTAT[cpu_2050.CH]  |= sal->ce << 4;
                break;
        case 6:  /* IOS.~E */
                cpu_2050.IOSTAT[cpu_2050.CH]  &= ~(sal->ce << 4);
                break;
        case 7:  /* BIB4.ERR>IOS */
                 /* IOS1 < BUSIN(0|2|3|6|7) or BIT(1&!5).
                    Busin:
                    Bit 0  Attn    Bit 6 Unit Chk
                    Bit 2  Chn end Bit 7 Unit exp
                    Bit 3  Busy    Bit 1 & !Bit5 SMS & !Device end.
                 */
                cpu_2050.IOSTAT[cpu_2050.CH] &= ~(BIT0|BIT1);
                if (cpu_2050.BUS_IN[0] & (BIT0|BIT2|BIT3|BIT6|BIT7))
                    cpu_2050.IOSTAT[cpu_2050.CH]  |= BIT1;
                if ((cpu_2050.BUS_IN[0] & SNS_SMS) != 0 && (cpu_2050.BUS_IN[0] & SNS_DEVEND) == 0)
                    cpu_2050.IOSTAT[cpu_2050.CH]  |= BIT1;
                if ((cpu_2050.BUS_IN[0] & SNS_CHNEND) != 0)
                    cpu_2050.IOSTAT[cpu_2050.CH]  |= BIT0;
                break;
        }

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
                cpu_2050.u_bus = bib;
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
                cpu_2050.v_bus = bib;
                break;
        case 3:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        }
    /* Handle selector channels */
    } else if (cpu_2050.io_mode && cpu_2050.CH != 0x3) {
        /* MG field in I/O Mode */
        switch (sal->dg) {
        case 0: /* BFR2>BIB.
                   Gate mpx channel buffer 2 to buffer in bus.  */
                break;
        case 1: /* CHPOSTEST.
                   Initiate selector channel position test */
                cpu_2050.chpostest = 1;
                break;
        case 2: /* BFR2>BUSO.
                   Gate MPX channel buffer 1 to buffer in bus,
                   Gate MPX channel buffer 2 to bus out (I/O interface) */
                break;
        case 3: /* BFR1>BIB.
                   Gate MPX channel buffer 1 to buffer in bus. */
                break;
        case 4: /* BOB>BFR1.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then Gate buffer out bus to channel buffer 1. */
                break;
        case 5: /* BOB>BFR2.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then Gate buffer out bus to channel buffer 2. */
                break;
        case 6: /* BUSI>BFR1.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then bus in (I/O interface) to channel buffer 1 */
                break;
        case 7: /* BUSI>BFR2.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then bus in (I/O interface) to channel buffer 2 */
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
                if (timer_irq) {
                    cpu_2050.u_bus = BIT0;
                    timer_irq = 0;
                } else if (INTR) {
                    cpu_2050.u_bus = BIT1;
                    INTR = 0;
                }
                break;
        case 5:  /* PSW4 ILC,CC, Progmask */
                cpu_2050.u_bus = (cpu_2050.ILC << 6) | (cpu_2050.CC << 4) |
                                  (cpu_2050.PMASK);
                break;
        case 6:  /* LMB */
                cpu_2050.u_bus = (cpu_2050.L_REG >>
                                     ((3 - cpu_2050.MB_REG) * 8)) & 0xff;
                break;
        case 7:  /* LLB */
                cpu_2050.u_bus = (cpu_2050.L_REG >>
                                     ((3 - cpu_2050.LB_REG) * 8)) & 0xff;
                break;
        }

        /* Gate data to V inputs */
        switch (sal->mv) {
        case 0:   /* 0 */
                cpu_2050.v_bus = 0;
                break;
        case 1:   /* MLB */
                cpu_2050.v_bus = (cpu_2050.M_REG >>
                                     ((3 - cpu_2050.LB_REG) * 8)) & 0xff;
                break;
        case 2:   /* MMB */
                cpu_2050.v_bus = (cpu_2050.M_REG >>
                                     ((3 - cpu_2050.MB_REG) * 8)) & 0xff;
                break;
        case 3:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        }

        /* Update ED stats */
        if ((cpu_2050.v_bus & 0xfc) == 0x20) {
            cpu_2050.ED_STAT = (cpu_2050.v_bus & 0x3);
        } else {
            cpu_2050.ED_STAT = 0x3;
        }
    }

    /* Do mover function left */
    switch (sal->ul) {
    case 0:  /* E>WL */
            cpu_2050.w_bus = (sal->ce << 4);
            break;
    case 1:  /* U */
            cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
            break;
    case 2:  /* V */
            cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
            break;
    case 3:  /* ? */
            if (cpu_2050.io_mode) {
                switch (cpu_2050.io_mvfnc) {
                case 0: /* Cross */
                        cpu_2050.w_bus = (cpu_2050.u_bus & 0x0f) << 4;
                        break;
                case 1: /* Straight */
                        cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                        break;
                case 2: /* And  */
                        cpu_2050.w_bus = (cpu_2050.v_bus & cpu_2050.u_bus) & 0xf0;
                        break;
                case 3: /* Left */
                        cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
                        break;
                case 4: /* Or */
                        cpu_2050.w_bus = (cpu_2050.v_bus | cpu_2050.u_bus) & 0xf0;
                        break;
                case 5: /* Right */
                        cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                        break;
                case 6: /* Xor */
                        cpu_2050.w_bus = (cpu_2050.v_bus ^ cpu_2050.u_bus) & 0xf0;
                        break;
                case 7:  /* Unknown */
                        cpu_2050.w_bus = 0;
                        break;
                }
            } else {
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
                case 5: /* Zone */
                        cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                        break;
                case 6: /* Numeric */
                        cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
                        break;
                case 7:  /* Unknown */
                        cpu_2050.w_bus = 0;
                        break;
                }
            }
            break;
    }

    /* Do mover function right */
    switch (sal->ur) {
    case 0:  /* E->WR */
            cpu_2050.w_bus |= sal->ce;
            break;
    case 1:  /* U */
            cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
            break;
    case 2:  /* V */
            cpu_2050.w_bus |= cpu_2050.v_bus & 0x0f;
            break;
    case 3:  /* ? */
            if (cpu_2050.io_mode) {
                switch (cpu_2050.io_mvfnc) {
                case 0: /* Cross */
                        cpu_2050.w_bus |= (cpu_2050.u_bus & 0xf0) >> 4;
                        break;
                case 1: /* Straight */
                        cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
                        break;
                case 2: /* And  */
                        cpu_2050.w_bus |= (cpu_2050.v_bus & cpu_2050.u_bus) & 0x0f;
                        break;
                case 3: /* Left */
                        cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
                        break;
                case 4: /* Or */
                        cpu_2050.w_bus |= (cpu_2050.v_bus | cpu_2050.u_bus) & 0x0f;
                        break;
                case 5: /* Right */
                        cpu_2050.w_bus |= cpu_2050.v_bus & 0x0f;
                        break;
                case 6: /* Xor */
                        cpu_2050.w_bus |= (cpu_2050.v_bus ^ cpu_2050.u_bus) & 0x0f;
                        break;
                case 7:  /* Unknown */
                        break;
                }
            } else {
                switch (cpu_2050.mvfnc) {
                case 0: /* Cross */
                        cpu_2050.w_bus |= (cpu_2050.u_bus & 0xf0) >> 4;
                        break;
                case 1: /* Or */
                        cpu_2050.w_bus |= (cpu_2050.v_bus | cpu_2050.u_bus) & 0x0f;
                        break;
                case 2: /* And  */
                        cpu_2050.w_bus |= (cpu_2050.v_bus & cpu_2050.u_bus) & 0x0f;
                        break;
                case 3: /* Xor */
                        cpu_2050.w_bus |= (cpu_2050.v_bus ^ cpu_2050.u_bus) & 0x0f;
                        break;
                case 4: /* char */
                        cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
                        break;
                case 5: /* Zone */
                        cpu_2050.w_bus |= cpu_2050.v_bus & 0x0f;
                        break;
                case 6: /* Numeric */
                        cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
                        break;
                case 7:  /* Unknown */
                        break;
                }
            }
            break;
    }

    carry_in = 0;
    if (cpu_2050.io_mode) {
         /* Channel timing */
         switch (sal->iv) {  /* CT */
         case 0:  /* Nop */
                 break;
         case 1:  /* FIRSTCYLE, if not first cycle, channel check */
                 log_trace("First cycle %d\n", cpu_2050.first_cycle);
                 break;
         case 2:  /* DTC1 send ingate timing to channel */
                 /* For RST0, Clock 0. */
                 /* For RST0, Clock 0. set LW, flags, ER. */
                 /* For ENDU. Clock 0. IOS4 = Er/chain, IOS1 = 0 STS, CH = C0 */
                 /* For ENDU. Clock 0. IOS4 = Er/chain, IOS2,
                                     IOS3  IOS1 = 0 STS, CH = C0 */
                 /* For IRPT. Clock 0. IOS0 = Status Only, IOS3=Poll proceed. */
                 /* For IRPT. Clock 1, ? */
                 if (cpu_2050.CH == 0x3) {
                     if (dtc_latch == 0) {
                         cpu_2050.BRC = sal->ce;   /* Latch branch control to E */
                         dtc_latch = 1;
                         log_trace("DTC1 %x RTN=%02x\n", sal->ce, cpu_2050.ROUTINE[cpu_2050.CH]);
                     }
                 }
                 dtc1 = 1;
                 break;
         case 3:  /* DTC2 send outgate timing to channel */
                 /* For CCW1 IOS1 = SMS */
                 /* For CCW2, Clock 0 IOS2 = TIC, IOS1 = FW/BKWD */
                 if (cpu_2050.CH != 0x3) {
                    /* For CCW1, Clock 0 R0 > B0. (UA) */
                    /* For CCW1, Clock 1 B0 > C0, L0 > B0. */
                    /* For CCW2, Clock 0. ? */
                    /* For CCW2, Clock 0. R3 > Byte select */
                    /* For CCW2, Clock 0. L0 > flags. */
                    /* For CCW2, Clock 2. set LW, flags, ER. start select out */
                    /* For WFCH, Clock 0. IOS3 == First word */
                    /* For WFCH, Clock 0. set LW, flags, ER. IOS2 = Data to CH */
                    /* For WFCH, Clock 0. aob > B */
                    /* For RST0, Clock 0.
                             IOS2 = Skip, IOS1 = Fwd/Bck IOS3 = Data In LS */
                    /* UAFT, Clock 0, C0 > L0. */
                    /* COMP, Clock 0. UATEST */
                 }
                 dtc2 = 1;
                 break;
         case 4:  /* IA/4->A,IA, initiate storage read,
                       inhibit invalid address, set flag */
                 if (cpu_2050.IA_REG & 1) {
                     cpu_2050.IVA = 1;
                 } else {
                     cpu_2050.IA_REG += 4;
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                     if (cpu_2050.SAR_REG > mem_max) {
                         cpu_2050.IVA = 1;
                     } else {
                         cpu_2050.init_mem = 1;
                     }
                 }
                 break;
         case 5:  /* Undefined */
                 break;
         case 6:  /* Undefined */
                 break;
         case 7:  /* Undefined */
                 break;
         }
         /* Compute carry into adder */
         carry_in = (sal->wm & 1);
    } else {
        switch (sal->ad) {
        case 0:  /* Nop */
        case 1:  /* Default */
        case 3:  /* Unknown */
        case 4:  /* BC0 */
        case 5:  /* BC^C */
        case 6:  /* BC1B */
        case 7:  /* BC8 */
        case 8:  /* DHL */
        case 11: /* DHH */
        case 13:
        case 14:
        case 15:
                carry_in = 0;
                break;
        case 2:  /* BCF0 */
                carry_in = (cpu_2050.F_REG == 0);
                break;
        case 9: /* DC0 */
        case 10: /* DDC0 */
        case 12: /* DCBS */
                carry_in = (cpu_2050.S_REG & BIT1) != 0;     /* <-- */
                break;
        }
         /* Instruction address register */
         switch (sal->iv) {
         case 0:  /* Nop */
                 break;
         case 1:  /* WL->IVD, trap if mover output bits 0-3 greater 9 */
                 if ((cpu_2050.w_bus & 0xf0) > 0x90) {
                     cpu_2050.NROAR = IVD;
                 }
                 break;
         case 2:  /* WR->IVD, trap if mover output bits 4-7 greater 9 */
                 if ((cpu_2050.w_bus & 0x0f) > 0x09) {
                     cpu_2050.NROAR = IVD;
                 }
                 break;
         case 3:  /* W->IVD, trap if mover output bits 0-3 or 4-7 greater 9 */
                 if ((cpu_2050.w_bus & 0xf0) > 0x90 ||
                              (cpu_2050.w_bus & 0x0f) > 0x09) {
                     cpu_2050.NROAR = IVD;
                 }
                 break;
         case 4:  /* IA/4->A,IA, initiate storage read,
                             inhibit invalid address, set flag */
                 if (cpu_2050.IA_REG & 1) {
                     cpu_2050.IVA = 1;
                 } else {
                     cpu_2050.IA_REG += 4;
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                     if (cpu_2050.SAR_REG > mem_max) {
                         cpu_2050.IVA = 1;
                     } else {
                         cpu_2050.init_mem = 1;
                     }
                 }
                 break;
         case 5:  /* IA+2/4 based on ILC if ILC == 0 or 1,
                             IAR+= 2, ILC = 2 or 3, IAR+=4 */
                 if ((cpu_2050.ILC & 02) == 0)
                     cpu_2050.IA_REG += 2;
                 else
                     cpu_2050.IA_REG += 4;
                 break;
         case 6:  /* IA+2 */
                 cpu_2050.IA_REG += 2;
                 break;
         case 7:  /* IA+0/2->A */
                 if (sal->zn == 1) {  /* SMIF */
                    if (cpu_2050.REFETCH == 0 && (cpu_2050.IA_REG & 0x2) != 0)
                        break;
                 }
                 if (cpu_2050.REFETCH) {
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                 } else {
                     cpu_2050.SAR_REG = (cpu_2050.IA_REG + 2);
                 }
                 if (cpu_2050.SAR_REG > mem_max) {
                     cpu_2050.IVA = 1;
                 } else {
                     cpu_2050.init_mem = 1;
                 }
                 break;
         }

         /* Compute carry into adder */
         switch (sal->dg) {
         case 0: /*  */
                 break;
         case 1: /* CSTAT->ADDER */
                 carry_in = cpu_2050.CAR;
                 break;
         case 2: /* HOT1->ADDER */
                 carry_in = 1;
                 break;
         case 3: /* G1-1  */
                 cpu_2050.G1NEG |= ((cpu_2050.G_REG & 0xf0) == 0);
                 cpu_2050.G_REG = (cpu_2050.G_REG & 0x0f) |
                                       ((cpu_2050.G_REG - 0x10) & 0xf0);
                 break;
         case 4: /* HOT1,G-1 */
                 cpu_2050.G1NEG |= (cpu_2050.G_REG == 0);
                 cpu_2050.G2NEG |= ((cpu_2050.G_REG & 0xf) == 0);
                 cpu_2050.G_REG = cpu_2050.G_REG - 1;
                 carry_in = 1;
                 break;
         case 5: /* G2-1 */
                 cpu_2050.G2NEG |= ((cpu_2050.G_REG & 0x0f) == 0);
                 cpu_2050.G_REG = (cpu_2050.G_REG & 0xf0) |
                            ((cpu_2050.G_REG - 0x01) & 0x0f);
                 break;
         case 6: /* G-1 */
                 cpu_2050.G2NEG |= ((cpu_2050.G_REG & 0x0f) == 0);
                 cpu_2050.G1NEG |= (cpu_2050.G_REG == 0);
                 cpu_2050.G_REG = cpu_2050.G_REG - 1;
                 break;
         case 7: /* G1,2-1 */
                 cpu_2050.G1NEG |= ((cpu_2050.G_REG & 0xf0) == 0);
                 cpu_2050.G2NEG |= ((cpu_2050.G_REG & 0x0f) == 0);
                 t = ((cpu_2050.G_REG & 0xf) - 1) & 0xf;
                 cpu_2050.G_REG = (t) | ((cpu_2050.G_REG - 0x10) & 0xf0);
                 break;
         }
     }

     /* Do adder, update output */
     cpu_2050.alu_out = cpu_2050.left_bus + cpu_2050.right_bus + carry_in;
     carries = (cpu_2050.left_bus & cpu_2050.right_bus) |
                  ((cpu_2050.left_bus ^ cpu_2050.right_bus) & ~cpu_2050.alu_out);
     carry_out = ((carries & MSIGN) != 0);

     if (!cpu_2050.io_mode) {
        switch (sal->ad) {
        case 0:  /* Nop */
        case 1:  /* Default */
        case 2:  /* BCF0 */
        case 3:  /* Unknown */
                break;
        case 4:  /* BC0 */ /* Set carry based out sum out */
                carry_out = ((carries & CPOS0) != 0);
                cpu_2050.CAR = carry_out;
                break;
        case 5: /* BC^C */  /* Set carry based on xor of carry from 0 and 1 */
                carry_out = ((carries ^ (carries << 1)) & CPOS0) != 0;
                break;
        case 6: /* BC1B */ /* Set carry out from position 1 */
                           /* Block carry between 8 and 7 */
                           /* When AL == 23 (1>FPSR4>F) force hot carry to
                               position 7 */
                if (sal->al == 23) {
                    t1 = (cpu_2050.left_bus & 0xff000000) +
                                (cpu_2050.right_bus & 0xff000000) + 0x01000000;
                } else {
                    t1 = (cpu_2050.left_bus & 0xff000000) +
                                (cpu_2050.right_bus & 0xff000000);
                }
                log_trace("t1=%08x %08x\n", t1, cpu_2050.alu_out);
                cpu_2050.alu_out =  (t1 & 0xff000000) |
                                (cpu_2050.alu_out & 0x00ffffff);
                carries = (cpu_2050.left_bus & cpu_2050.right_bus) |
                             ((cpu_2050.left_bus ^ cpu_2050.right_bus) &
                                   ~cpu_2050.alu_out);
                carry_out = (carries & CPOS1) != 0;
                cpu_2050.CAR = carry_out;
                break;
        case 7: /* BC8 */
                t1 = (cpu_2050.left_bus & 0xff000000) +
                                       (cpu_2050.right_bus & 0xff000000);
                cpu_2050.alu_out =  (t1 & 0xff000000) |
                                        (cpu_2050.alu_out & 0x00ffffff);
                carries = (cpu_2050.left_bus & cpu_2050.right_bus) |
                             ((cpu_2050.left_bus ^ cpu_2050.right_bus) &
                                         ~cpu_2050.alu_out);
                carry_out = (carries & 0x00800000) != 0;
                cpu_2050.CAR = carry_out;
                break;

        case 8: /* DHL */ /* If out bit 2 is set, set L right one digit to 6.
                             Left digit of L set to 6 if aux carry set */
                t1 = 0x22222222 & cpu_2050.alu_out;
                l_update = (t1 >> 3) | (t1 >> 4);
                if (cpu_2050.AUX_REG)
                    l_update |= 0x60000000;
                break;
        case 9: /* DC0 */
                    /* Set L to 6 if carry out of sum between digits */
                t1 = 0x88888888 & ~carries;
                l_update = ((t1 >> 1) | (t1 >> 2));
                if ((carries & CPOS0) != 0) {
                    s_update |= BIT1;     /* <-- */
                } else {
                    s_update &= ~BIT1;     /* <-- */
                }
                break;
        case 10: /* DDC0 */
                     /* Set L to 6 if sum out is 5 or greater. */
                l_update = 0;
                for (t = 0; t < 31; t+= 4) {
                    if (((cpu_2050.alu_out >> t) & 0xf) >= 5)
                       l_update |= 6 << t;
                }
                log_trace("t=%08x %08x\n", l_update, carries);
                if ((carries & CPOS0) != 0) {
                    s_update |= BIT1;     /* <-- */
                } else {
                    s_update &= ~BIT1;     /* <-- */
                }
                break;
        case 11: /* DHH */
                     /* Same as DC0, Aux carry set to right most digit bit 2 */
                t1 = 0x22222222 & cpu_2050.alu_out;
                l_update = (t1 >> 3) | (t1 >> 4);
                cpu_2050.AUX_REG = (t1 & 0x2) != 0;
                break;
        case 12: /* DCBS */
                t1 = 0x88888888 & ~carries;
                l_update = ((t1 >> 1) | (t1 >> 2));
                /* Find left most digit which has BS on */
                s_update &= ~BIT1;     /* <-- */
                if ((cpu_2050.BS_REG & 0x8) != 0) {
                    if ((carries & CPOS0) != 0)
                        s_update |= BIT1;     /* <-- */
                } else if ((cpu_2050.BS_REG & 0x4) != 0) {
                    if ((carries & 0x00800000) != 0)
                        s_update |= BIT1;     /* <-- */
                } else if ((cpu_2050.BS_REG & 0x2) != 0) {
                    if ((carries & 0x00008000) != 0)
                        s_update |= BIT1;     /* <-- */
                } else if ((cpu_2050.BS_REG & 0x1) != 0) {
                    if ((carries & 0x00000080) != 0)
                        s_update |= BIT1;     /* <-- */
                }
                break;
        case 13:
                break;
        case 14:
                break;
        case 15:
                break;
        }
    }

    /* Update next Roar if BB (CAR) or (ZOO) are used */
    if (sal->bb == 30 || sal->bb == 31) {
         if (sal->bb == 30) {  /* (CAR) branch on carry latch  */
            b_bit |= carry_out;
         } else  {             /* (ZOO) branch on T bit 0 */
            b_bit |= (cpu_2050.alu_out & CPOS0) != 0;
         }
         switch (sal->zn) {
         case 0: /* Use ZF function */
                 break;
         case 1: /* SMIF suppress memory instruction fetch */
                 /* By order IV7 if refetch == 0 and IAR bit 30 == 1 */
                 break;
         case 2: /* A|(B=0)->A */
                 a_bit |= !b_bit;
                 break;
         case 3: /* A|(B=1)->A */
                 a_bit |= b_bit;
                 break;
         case 4: /* Normal no change */
                 break;
         case 5: /* Force Invalid Op trap address */
                 break;
         case 6: /* B|(A=0)->B */
                 b_bit |= !a_bit;
                 break;
         case 7: /* B|(A=1)->B */
                 b_bit |= a_bit;
                 break;
         }
         cpu_2050.NROAR &= ~3;
         cpu_2050.NROAR |= (a_bit << 1)|b_bit;
    }


    /* Do adder shifter */
    switch (sal->al) {
    case 0: /* Normal */
            cpu_2050.aob_latch = cpu_2050.alu_out;
            break;
    case 1: /* Q->SR1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1);
            if (cpu_2050.Q_REG)
                cpu_2050.aob_latch |= BIT0L;
            cpu_2050.F_REG = ((cpu_2050.alu_out & 1) << 3) |
                       (cpu_2050.F_REG >> 1);
            break;
    case 2: /* L0,-S4-> */
            t1 = ((cpu_2050.S_REG & BIT4) == 0) ? MSIGN : 0;     /* <-- */
            cpu_2050.aob_latch = t1 | (cpu_2050.L_REG & 0x7f000000) |
                           (cpu_2050.alu_out & 0xffffff);
            break;
    case 3: /* +SGN-> */
            cpu_2050.aob_latch = cpu_2050.alu_out & 0x7fffffff;
            break;
    case 4: /* -SGN-> */
            cpu_2050.aob_latch = cpu_2050.alu_out | MSIGN;
            break;
    case 5: /* L0,S4-> */
            t1 = ((cpu_2050.S_REG & BIT4) != 0) ? MSIGN : 0;     /* <-- */
            cpu_2050.aob_latch = t1 | (cpu_2050.L_REG & 0x7f000000) |
                           (cpu_2050.alu_out & 0xffffff);
            break;
    case 6: /* IA->H */
            cpu_2050.H_REG = (cpu_2050.H_REG & 0xff000000) |
                     (cpu_2050.IA_REG & 0xffffff);
            cpu_2050.aob_latch = cpu_2050.alu_out;
            break;
    case 7: /* Q->SL->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            cpu_2050.F_REG = (((cpu_2050.alu_out >>31) & 1) |
                     (cpu_2050.F_REG << 1)) & 0xf;
            cpu_2050.F_REG ^= 0x1;
            break;
    case 8: /* Q->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            cpu_2050.F_REG = (((cpu_2050.alu_out >>31) & 1) |
                     (cpu_2050.F_REG << 1)) & 0xf;
            break;
    case 9: /* F->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) |
                     ((cpu_2050.F_REG & 8) != 0);
            cpu_2050.F_REG = (((cpu_2050.alu_out & BIT0L) != 0) |
                                       (cpu_2050.F_REG << 1)) & 0xf;
            break;
    case 10: /* SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1);
            cpu_2050.Q_REG = ((cpu_2050.alu_out & BIT0L) != 0);
            break;
    case 11: /* Q->SL1 */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            break;
    case 12: /* SR1->F */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            cpu_2050.F_REG = ((cpu_2050.alu_out & 1) << 3) |
                     (cpu_2050.F_REG >> 1);
            break;
    case 13: /* SR1->Q */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            cpu_2050.Q_REG = cpu_2050.alu_out & 1;
            break;
    case 14: /* Q->SR1->Q */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            if (cpu_2050.Q_REG)
                cpu_2050.aob_latch |= BIT0L;
            cpu_2050.Q_REG = (cpu_2050.alu_out & 1);
            break;
    case 15: /* F->SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) |
                     ((cpu_2050.F_REG >> 3) & 1);
            cpu_2050.F_REG = (cpu_2050.F_REG << 1);
            cpu_2050.Q_REG = ((cpu_2050.alu_out & BIT0L) != 0);
            break;
    case 16: /* SL4->F */
            cpu_2050.aob_latch = cpu_2050.alu_out << 4;
            cpu_2050.F_REG = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 17: /* F->SL4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 4) |
                     (cpu_2050.F_REG & 0xf);
            cpu_2050.F_REG = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 18: /* FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                   (cpu_2050.alu_out & 0xff000000);
            break;
    case 19: /* F->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                   (cpu_2050.alu_out & 0xff000000) |
                                    (cpu_2050.F_REG & 0xf);
            break;
    case 20: /* SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 21: /* F->SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) |
                     ((cpu_2050.F_REG & 0xf) << 28);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 22: /* FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out & 0x00ffffff) >> 4);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 23: /* 1->FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out & 0xffffff) >> 4) |
                                   (cpu_2050.alu_out & 0xff000000) | 0x100000;
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 24: /* SR4->H */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            cpu_2050.H_REG = ((cpu_2050.alu_out & 0xF) << 28) |
                                 (cpu_2050.H_REG & 0x0fffffff);
            cpu_2050.R_REG = ((cpu_2050.alu_out &   0x0F000000) << 4) |
                                  (cpu_2050.R_REG & 0x0fffffff);
            break;
    case 25: /* F->SR4 */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) |
                                 (((uint32_t)cpu_2050.F_REG & 0xf) << 28);
            break;
    case 26: /* E->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                  (cpu_2050.alu_out & 0xff000000) |
                                 (sal->ce & 0xf);
            break;
    case 27: /* F->SR1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) |
                                 (((uint32_t)cpu_2050.F_REG & 0x1) << 31);
            cpu_2050.Q_REG = (cpu_2050.alu_out & 1);
            break;
    case 28: /* DKEY-> */
            cpu_2050.aob_latch = cpu_2050.DKEYS;
            if (allow_man_operation && SEL_ENTER) {
                    uint16_t   data = (cpu_2050.DKEYS >> 24) & 0xff;
                    int        ch = (cpu_2050.CH + 1) & 3;
                    if (cpu_2050.AKEYS & 1)
                        cpu_2050.TAGS[ch] = CHAN_OPR_IN;
                    if (cpu_2050.AKEYS & 2)
                        cpu_2050.TAGS[ch] = CHAN_SEL_IN;
                    if (cpu_2050.AKEYS & 4)
                        cpu_2050.TAGS[ch] = CHAN_SRV_IN;
                    if (cpu_2050.AKEYS & 8)
                        cpu_2050.TAGS[ch] = CHAN_STA_IN;
                    if (cpu_2050.AKEYS & 0x10)
                        cpu_2050.TAGS[ch] = CHAN_ADR_IN;
                    if (cpu_2050.AKEYS & 0x20)
                        cpu_2050.TAGS[ch] = CHAN_REQ_IN;
                    cpu_2050.BUS_IN[ch] = data | odd_parity[data];
            }
            break;
    case 29: /* CH, gate selector channels to latch  */
                /* CHPOS
                 *    BIT 0 - .STIO/.CCW1
                 *    BIT 2 - Unit Select.
                 *    BIT 3 - .RST0
                 *    BIT 4 - .WFCH
                 *    BIT 5 - .ENDU
                 *    BIT 6 - .COMP
                 *    BIT 7 - .IRPT
                 *    100   - .UAFT
                 */
            log_trace("CH gate:  %03x\n", cpu_2050.CHPOS[cpu_2050.CH]);
            if (cpu_2050.CHPOS[cpu_2050.CH] == BIT5) {   /* .ENDU */
                if (!cpu_2050.C3[cpu_2050.CH]) {
                 cpu_2050.aob_latch = (cpu_2050.GP_REG[cpu_2050.CH] >> 5) & 3;
                if (cpu_2050.OP_REG[cpu_2050.CH] == 0x6) {
                    cpu_2050.aob_latch ^= 3;
                }
                }
            } else if (cpu_2050.CHPOS[cpu_2050.CH] == BIT7) {  /* .IRPT */
                switch(cpu_2050.CHCLK[cpu_2050.CH]) {
                case 1:
                    if (cpu_2050.C3[cpu_2050.CH]) {
                    cpu_2050.aob_latch = 0;
                    } else {
                    cpu_2050.aob_latch = cpu_2050.C_REG[cpu_2050.CH] & 0xff000000;
                    }
                    cpu_2050.aob_latch |= ((uint32_t)cpu_2050.FLAGS_REG[cpu_2050.CH] & 0xff) << 16;
                    break;
                case 2:
                    cpu_2050.aob_latch = cpu_2050.B_REG[cpu_2050.CH] & 0xff000000;
                    break;
                }
            } else if (cpu_2050.CHPOS[cpu_2050.CH] == 0x200) {
                cpu_2050.aob_latch = cpu_2050.B_REG[cpu_2050.CH];
                cpu_2050.LS_FULL[cpu_2050.CH] = 1;
                cpu_2050.B_FULL[cpu_2050.CH] = 0;
                cpu_2050.IOSTAT[cpu_2050.CH] |= BIT3;
            } else if (cpu_2050.CHPOS[cpu_2050.CH] == 0x400) {
                cpu_2050.B_REG[cpu_2050.CH] = cpu_2050.aob_latch;
                cpu_2050.LS_FULL[cpu_2050.CH] = 0;
                cpu_2050.B_FULL[cpu_2050.CH] = 1;
                cpu_2050.IOSTAT[cpu_2050.CH] |= BIT3;
            }
            break;
    case 30: /* D-> gate storage to latch, hold off it not ready */
            cpu_2050.aob_latch = cpu_2050.SDR_REG;
            break;
    case 31: /* AKEY-> */
            cpu_2050.aob_latch = cpu_2050.AKEYS;
            break;
    }

    /* Preform State function */
    /* These functions have to be done before registers are modified */
    switch (sal->ss) {
    case 3: /* D->CR*BS */
            /* Read main store, do test and set */
            cpu_2050.CC = (((cpu_2050.SDR_REG & BS_MASK[cpu_2050.BS_REG]) &
                                    0x80808080) != 0);
            break;
    case 36: /* L(0)->LSGNS */
            cpu_2050.LSGNS = (cpu_2050.L_REG & MSIGN) != 0;
            break;
    case 37: /* R(0)->RSGNS */
            cpu_2050.RSGNS = (cpu_2050.R_REG & MSIGN) != 0;
            break;
    }

    /* Save aob_latch to register */
    switch (sal->tr) {
    case 0:  /* T */
            break;
    case 1:  /* R */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            if (sal->al == 24) { /* SR4->H */
                cpu_2050.R_REG = ((cpu_2050.aob_latch & 0x0F000000) << 4) |
                                  (cpu_2050.aob_latch & 0x0fffffff);
            }
            break;
    case 2:  /* R0 */
            cpu_2050.R_REG = (cpu_2050.aob_latch & 0xff000000) |
                             (cpu_2050.R_REG & 0x00ffffff);
            break;
    case 3:  /* M */
            cpu_2050.M_REG = cpu_2050.aob_latch;
            break;
    case 4:  /* D */
            /* Check storage protection */
            if (cpu_2050.KEY != 0 &&
                   cpu_2050.KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                cpu_2050.NROAR = STPR;
                break;
            }
            cpu_2050.SDR_REG = cpu_2050.aob_latch;
            cpu_2050.update_d = 1;
            break;
    case 5:  /* L0 */
            l_update = (cpu_2050.aob_latch & 0xff000000) |
                             (cpu_2050.L_REG & 0x00ffffff);
            break;
    case 6:  /* R,A Initiate main storage read */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            cpu_2050.init_mem = 1;
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
            switch(sal->ce & 07) {
            case 0:  cpu_2050.SAR_REG = 0x80;
                     cpu_2050.init_mem = 1;
                     cpu_2050.update_d = 1;
                     break;
            case 1:  cpu_2050.SAR_REG = 0x80;
                     cpu_2050.init_mem = 1;
                     break;
            case 2:  cpu_2050.SAR_REG = 0x80;
                     cpu_2050.init_mem = 1;
                     break;
            case 4:  cpu_2050.SAR_REG = 0x84;
                     cpu_2050.IA_REG = 0x84;
                     cpu_2050.init_mem = 1;
                     cpu_2050.update_d = 1;
                     break;
            case 5:  cpu_2050.SAR_REG = 0x84;
                     cpu_2050.IA_REG = 0x84;
                     cpu_2050.init_mem = 1;
                     break;
            case 3:
            case 6:
            case 7:
                     break;
            }
            break;
    case 9:  /* R,AN, initiate memory request,
                            suppress invalid memory address trap */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            if (cpu_2050.SAR_REG > mem_max) {
                cpu_2050.IVA = 1;
            } else {
                cpu_2050.init_mem = 1;
            }
            break;
    case 10:  /* R,AW, initiate memory request,
                           invalid address trap if not word boundry  */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            if ((cpu_2050.SAR_REG & 0x3) != 0)
                cpu_2050.NROAR = SPEC;
            else
                cpu_2050.init_mem = 1;
            break;
    case 11:  /* R,AD  memory request,
                            invalid address trap if not double boundry */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            if ((cpu_2050.SAR_REG & 0x7) != 0)
                cpu_2050.NROAR = SPEC;
            else
                cpu_2050.init_mem = 1;
            break;
    case 12:  /* D->IAR interlock with storage timing rinmmg */
            /* Read */
            cpu_2050.IA_REG = cpu_2050.SDR_REG;
            break;
    case 13:  /* SCAN->D */
            /* Check storage protection */
            if (cpu_2050.KEY != 0 &&
                     cpu_2050.KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                cpu_2050.NROAR = STPR;
                break;
            }
            cpu_2050.SDR_REG = 0;
            cpu_2050.update_d = 1;
            break;
    case 14:  /* R13 */
            cpu_2050.R_REG = (cpu_2050.R_REG & 0xff000000) |
                             (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 15:  /* A initiate memory request */
            cpu_2050.SAR_REG = cpu_2050.aob_latch & 0xffffff;
            cpu_2050.init_mem = 1;
            break;
    case 16:  /* L,A initiate memory request */
            l_update = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = cpu_2050.aob_latch & 0xffffff;
            if (cpu_2050.io_mode) {
                cpu_2050.IO_KEY = (cpu_2050.aob_latch >> 28) & 0xf;
            }
            cpu_2050.init_mem = 1;
            break;
    case 17:  /* R,D  */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            /* Check storage protection */
            if (cpu_2050.io_mode) {
                if (cpu_2050.bump_mem == 0 && cpu_2050.IO_KEY != 0 &&
                     cpu_2050.IO_KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                     cpu_2050.FLAGS_REG[cpu_2050.CH] |= CHAN_PROT;
                     cpu_2050.IF_STOP[cpu_2050.CH] = 1;
                     break;
                }
            } else {
                if (cpu_2050.KEY != 0 &&
                       cpu_2050.KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                    cpu_2050.NROAR = STPR;
                    break;
                }
            }
            cpu_2050.SDR_REG = cpu_2050.aob_latch;
            cpu_2050.update_d = 1;
            break;
    case 18:  /* Undefined  */
            break;
    case 19:  /* R,IO   */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.IO_REG = cpu_2050.aob_latch & 0x3;
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
            cpu_2050.REFETCH = 0;
            s_update &= ~(BIT0|BIT1);     /* <-- */
            if (cpu_2050.J_REG == 0)
                s_update |= BIT0;     /* <-- */
            if (cpu_2050.MD_REG == 0)
                s_update |= BIT1;     /* <-- */
            cpu_2050.SYLS1 = ((cpu_2050.MD_REG & 0xc) == 0);
            cpu_2050.ILC = 1;
            cpu_2050.ILC += ((cpu_2050.aob_latch & 0xc0000000) != 0);
            cpu_2050.ILC += ((cpu_2050.aob_latch & 0xc0000000) == 0xc0000000);
            break;
    case 26:  /* MHL */
            cpu_2050.M_REG = (cpu_2050.M_REG & 0xffff0000) |
                       ((cpu_2050.aob_latch >> 16) & 0xffff);
            cpu_2050.MD_REG = (cpu_2050.aob_latch >> 28) & 0xF;  /* 0-3 */
            l_update = cpu_2050.aob_latch;
            break;
    case 27:  /* MD */
            cpu_2050.MD_REG = (cpu_2050.aob_latch >> 20) & 0xF;  /* 8-11 */
            break;
    case 28:  /* M,SP gate adder to M, bits 8-11 to key */
              /* D*BI under I/O mode */
            if (cpu_2050.io_mode) {
                if (cpu_2050.bump_mem == 0 && cpu_2050.IO_KEY != 0 &&
                     cpu_2050.IO_KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                     cpu_2050.FLAGS_REG[cpu_2050.CH] |= CHAN_PROT;
                     cpu_2050.IF_STOP[cpu_2050.CH] = 1;
                     break;
                }
                cpu_2050.SDR_REG =
                              (cpu_2050.SDR_REG & ~BS_MASK[cpu_2050.BI_REG]) |
                              (cpu_2050.aob_latch & BS_MASK[cpu_2050.BI_REG]);
                cpu_2050.update_d = 1;
            } else {
                cpu_2050.M_REG = cpu_2050.aob_latch;
                cpu_2050.KEY = (cpu_2050.aob_latch >> 20) & 0xf;
            }
            break;
    case 29:  /* D,BS gate D to SDR based on BS */
            /* Check storage protection */
            if (cpu_2050.KEY != 0 &&
                     cpu_2050.KEY != cpu_2050.MP[cpu_2050.SAR_REG >> 11]) {
                cpu_2050.NROAR = STPR;
                break;
            }
            cpu_2050.SDR_REG = (cpu_2050.SDR_REG & ~BS_MASK[cpu_2050.BS_REG]) |
                           (cpu_2050.aob_latch & BS_MASK[cpu_2050.BS_REG]);
            cpu_2050.update_d = 1;
            break;
    case 30:  /* L13 */
            l_update = (cpu_2050.L_REG & 0xff000000) |
                             (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 31:  /* J, gate adder bits 12-15 to J */
            if (cpu_2050.io_mode) {
                if (cpu_2050.OP_REG[cpu_2050.CH] == 0x6) {
                    cpu_2050.IO_REG = 3 ^ (cpu_2050.aob_latch & 0x3);
                } else {
                    cpu_2050.IO_REG = cpu_2050.aob_latch & 0x3;
                }
            } else {
                cpu_2050.J_REG = (cpu_2050.aob_latch >> 16) & 0xf;
            }
            break;
    }

    if (cpu_2050.io_mode) {
        /* CL in I/O mode */
        switch(sal->ad >> 1) {
        case 0:   /* Nop */
        case 1:   /* Undefined */
        case 2:   /* Undefined */
            break;
        case 3:   /* CCW2TEST
                     Send program check to select channel if.
                     SDR bits 5-7 are not zero or bits 16-31 are zero. */
            log_trace("CCW2Test\n");
            if ((cpu_2050.aob_latch & 0x07000000) != 0) {
                 cpu_2050.FLAGS_REG[cpu_2050.CH] |= CHAN_PROG; /* Program check */
            }
            if ((cpu_2050.aob_latch & 0x0000FFFF) == 0) {
                 cpu_2050.FLAGS_REG[cpu_2050.CH] |= CHAN_PROG; /* Program check */
            }
            break;
        case 4:   /* CATEST
                     Send program check to select channel if.
                     Adder_output 29-31 bits not zero (adder check) */
            log_trace("CATest\n");
            if ((cpu_2050.aob_latch & 0x7) != 0) {
                 cpu_2050.FLAGS_REG[cpu_2050.CH] |= CHAN_PROG; /* Program check */
            }
            break;
        case 5:   /* UATEST
                     set bit 8 of adder-latch-to-channel if adder ouput
                     bits 0-7 are zero. Set bit 8 to 0 if not.
                     Other bits set to zero.
                        Unit address test */

            log_trace("UATest\n");
            if ((cpu_2050.aob_latch & 0xff000000) == 0) {
                cpu_2050.aob_latch |= 0x00800000;
            } else {
                cpu_2050.aob_latch &= 0xff7fffff;
            }
            break;
        case 6:   /* LSWDTEST
                     gate adder-latch bits 15-31 to channel bits 15-31.
                     send last 1 word if 15-31 >0 and <5
                     send last 2 word if 15-31 >4 and <9
                     send last 3 word if 15-31 >8 and <13.
                       Last word test. */
            log_trace("Test last word\n");
            if (cpu_2050.CH == 0x3) {
                if ((cpu_2050.aob_latch & 0xffff) == 0x0001) {
                    log_trace("Set last word\n");
                    cpu_2050.MPX_LST = 1;
                } else {
                    log_trace("clear last word\n");
                    cpu_2050.MPX_LST = 0;
                }
            } else {
                int    byte = cpu_2050.aob_latch & 0xf;
                static uint8_t last_back[] = { BIT5, BIT5, BIT5, BIT5,
                                               BIT6, BIT6, BIT6, BIT6,
                                               BIT7, BIT7, BIT7, BIT7,
                                               0,    0,    0,    0};
                static uint8_t last_forw[] = { BIT5, BIT5, BIT5, BIT5,
                                               BIT5, BIT6, BIT6, BIT6,
                                               BIT6, BIT7, BIT7, BIT7,
                                               BIT7, 0,    0,    0,
                                               0,    0,    0,    0 };
                if (cpu_2050.OP_REG[cpu_2050.CH] == 1 &&
                         ((cpu_2050.aob_latch & 0x10000) != 0 || (cpu_2050.aob_latch & 0xffff) == 0)) {
                       cpu_2050.GP_REG[cpu_2050.CH] |= (byte & 03) << 3;
                       cpu_2050.EOR[cpu_2050.CH] = 1;
                       break;
                }
                if ((cpu_2050.aob_latch & 0xfff0) != 0/* || cpu_2050.C3[cpu_2050.CH]*/) {
                     break;
                }
                       cpu_2050.GP_REG[cpu_2050.CH] &= ~(0x18);
                switch(cpu_2050.OP_REG[cpu_2050.CH]) {
                case 0x1:   /* Write */
                     //  if (byte < 4) {
//                       cpu_2050.GP_REG[cpu_2050.CH] |= BIT7;
//                       byte = 3 - ((cpu_2050.GP_REG[cpu_2050.CH] >> 5) & 0x3);
                       cpu_2050.GP_REG[cpu_2050.CH] |= (byte & 03) << 3;
                      // }
                     //  cpu_2050.GP_REG[cpu_2050.CH] |= last_forw[byte];
                     //  if (last_forw[byte] & BIT5) {
                     //      cpu_2050.EOR[cpu_2050.CH] = 1;
                     //  }
                       break;
                case 0x6: /* Read backward */
                       cpu_2050.GP_REG[cpu_2050.CH] &= ~(0x18);
                       cpu_2050.GP_REG[cpu_2050.CH] |= (byte & 3) << 3;
                       cpu_2050.GP_REG[cpu_2050.CH] |= last_back[byte];
                       break;
                case 0x4:  /* Read forward */
                       cpu_2050.GP_REG[cpu_2050.CH] &= ~(0x18);
                       cpu_2050.GP_REG[cpu_2050.CH] |= (byte & 3) << 3;
                       cpu_2050.GP_REG[cpu_2050.CH] |= last_forw[byte];
                       break;
                   }
               }
            break;
        case 7:   /* Undefined */
            break;
        }
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
            /* Done before */
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
               cpu_2050.NROAR = IVD;
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
               cpu_2050.NROAR = IVD;
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
            s_update &= 0xf;     /* <-- */
            s_update |= (sal->ce & 0xf) << 4;     /* <-- */
            break;
    case 9: /* S03|E,1->LSGNS */
            cpu_2050.LSGNS = 1;
            s_update |= (sal->ce & 0xf) << 4;     /* <-- */
            break;
    case 10: /* S03|E */
            s_update |= (sal->ce & 0xf) << 4;     /* <-- */
            break;
    case 11: /* S03|E,0->BS */
            cpu_2050.BS_REG = 0;
            s_update |= (sal->ce & 0xf) << 4;     /* <-- */
            break;
    case 12: /* X0,B0,1SYL */
            s_update &= ~(BIT0|BIT1);     /* <-- */
            if ((cpu_2050.aob_latch & 0x000f0000) == 0)
                s_update |= BIT0;     /* <-- */
            if ((cpu_2050.aob_latch & 0xf0000000) == 0)
                s_update |= BIT1;     /* <-- */
            if ((cpu_2050.aob_latch >> 28) <= 3) {
                cpu_2050.SYLS1 = 1;
            } else {
                cpu_2050.SYLS1 = 0;
            }
            break;
    case 13: /* FPZERO, set S0 if floating point 0. */
            if ((cpu_2050.aob_latch & 0x00ffffff) == 0 &&
                   cpu_2050.F_REG == 0 && (s_update & BIT3) != 0)     /* <-- */
                s_update |= BIT0;     /* <-- */
            else
                s_update &= ~BIT0;     /* <-- */
            break;
    case 14: /* FPZER,E->FN*/
            if ((cpu_2050.aob_latch & 0x00ffffff) == 0 &&
                   cpu_2050.F_REG == 0 && (s_update & BIT3) != 0)     /* <-- */
                s_update |= BIT0;     /* <-- */
            else
                s_update &= ~BIT0;     /* <-- */
            cpu_2050.LSA &= 0xf;
            cpu_2050.LSA |= (sal->ce & 0x3) << 4;
            break;
    case 15: /* B0,1SYL */
            s_update &= ~(BIT1);     /* <-- */
            if ((cpu_2050.aob_latch & 0xf0000000) == 0)
                s_update |= BIT1;     /* <-- */
            if ((cpu_2050.aob_latch & 0x0000c000) == 0) {
                cpu_2050.SYLS1 = 1;
            } else {
                cpu_2050.SYLS1 = 0;
            }
            break;
    case 16: /* S03.!E */
            /* Hack, if restoring and in S3 wait loop, ignore this update */
            s_update &= ~((sal->ce & 0xf) << 4);     /* <-- */
            break;
    case 17: /* (T=0)->S3 */
            s_update &= ~(BIT3);     /* <-- */
            if (cpu_2050.aob_latch == 0)
                s_update |= BIT3;     /* <-- */
            break;
    case 18: /* E->BS,T30->S3 */
            cpu_2050.BS_REG = (sal->ce & 0xf);
            s_update &= ~(BIT3);     /* <-- */
            if ((cpu_2050.aob_latch & 02) != 0)
                s_update |= BIT3;     /* <-- */
            break;
    case 19: /* E->BS */
            cpu_2050.BS_REG = (sal->ce & 0xf);
            break;
    case 20: /* 1->BS*MB */
            cpu_2050.BS_REG |= 8 >> cpu_2050.MB_REG;
            break;
    case 21: /* DIRCTL->E */
            break;
    case 22: /* Undefined */
            break;
    case 23: /* MANUAL->STOP */
            stop_mode |= allow_man_operation;
            break;
    case 24: /* E->S47 */
            s_update &= 0xf0;     /* <-- */
            s_update |= (sal->ce & 0xf);     /* <-- */
            break;
    case 25: /* S47|E */
            s_update |= (sal->ce & 0xf);     /* <-- */
            break;
    case 26: /* S47&~E */
            s_update &= 0xf0 | ~(sal->ce & 0xf);     /* <-- */
            break;
    case 27: /* S47,ED*FP */
            s_update &= 0xf0;     /* <-- */
            t = (cpu_2050.alu_out >> 24) & 0xff;
            t1 = (carries & CPOS1) != 0;
            /* Compute ED value */
            /* Carry from bit 1. and sum 1-4 != 0
               or no carry from bit 1 and sum 4-4 = 1111 */
            if ((t1 != 0 && (t & 0x78) != 0) || (t1 == 0 && (t & 0x78) == 0x78))
                cpu_2050.ED_REG = 0x8 | (t & 07);
            else
                cpu_2050.ED_REG = t & 07;
            t = (carries & CPOS0) != 0;
            t1 = (cpu_2050.right_bus & CPOS0) != 0;
            t2 = ((cpu_2050.left_bus & CPOS0) != 0);
            /* S0 = add
               S1 = sub
               S2 = unorm
               S3 = comp sign when S0 and S1 zero (Multiply or divide).
            */
            /* Stat 4 turned on:
                   If S0 or S1 and right adder input bit 0 is 1, and carry out bit 1
                   If S0 or S1 and carry out bit 1 and either adder left input 0 or S1.
                      But not both. (Add type result minus)
                   If S0 and S1 == 0 and left adder input bit 0 != right adder input
                     bit 0 */
            if (((s_update & (BIT0)) != 0 && t1 == 0 && t2 == 0 && carry_out) ||     /* <-- */
                ((s_update & (BIT0)) != 0 && t1 == 0 && t2 != 0 && !t && carry_out) ||     /* <-- */
                ((s_update & (BIT1)) != 0 && t1 == 0 && t2 != 0 && t && carry_out) ||     /* <-- */
                ((s_update & (BIT0|BIT1)) != 0 && t1 != 0 && t2 == 0 && !t) ||     /* <-- */
                ((s_update & (BIT0)) != 0 && t1 != 0 && t2 == 0 && t && carry_out) ||     /* <-- */
                ((s_update & (BIT0)) != 0 && t1 != 0 && t2 != 0 && !carry_out) ||     /* <-- */
                ((s_update & (BIT1)) != 0 && t1 != 0 && t2 != 0) ||     /* <-- */
                ((s_update & (BIT0|BIT1|BIT3)) == 0 && t1 != t2) ||     /* <-- */
                ((s_update & (BIT0|BIT1|BIT3)) == BIT3 && t1 == t2))     /* <-- */
                s_update |= BIT4;     /* <-- */
            /* Stat 5 turned on:
                   If right adder bit 0, left adder bit 0 and S1 have even number
                   of bits. (True add required). */
            if ((((s_update & BIT1) != 0) ^ t1 ^ t2) != 0)     /* <-- */
                s_update |= BIT5;     /* <-- */
            /* Stat 6 turned on:
                   If abs(ED) < 16. */
            if (t1 < 16 || (t1 & 0xf0) == 0xf0)
                s_update |= BIT6;     /* <-- */
            /* Stat 7 turned on:
                   If ED == 0 */
            if (cpu_2050.ED_REG == 0)
                s_update |= BIT7;     /* <-- */
            break;
    case 28: /* OPPANEL->S47 */
            s_update &= 0xf0;     /* <-- */
            s_update |= cpu_2050.OPPANEL;     /* <-- */
            break;
    case 29: /* CAR,(T!=0)->CR */
            cpu_2050.CC = carry_out << 1;
            cpu_2050.CC |= (cpu_2050.alu_out != 0);
            break;
    case 30: /* KEY->F */
            cpu_2050.F_REG = cpu_2050.MP[cpu_2050.SAR_REG >> 11];
            break;
    case 31: /* F->KEY */
            cpu_2050.MP[cpu_2050.SAR_REG >> 11] = cpu_2050.F_REG;
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
            /* Done before */
            break;
    case 37: /* R(0)->RSGNS */
            /* Done before */
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
             /* CC = 0 if aob_latch == 0,
                CC = 1 if aob_latch<0> == 1,
                CC = 2 otherwise */
            if (cpu_2050.aob_latch == 0) {
                cpu_2050.CC = 0;
            } else if (cpu_2050.aob_latch & MSIGN) {
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
                cpu_2050.CC = carry_out + 1;
            }
            break;
    case 43: /* !S4,S4->CR */
            cpu_2050.CC = (cpu_2050.S_REG & BIT4) ? 1 : 2;     /* <-- */
            break;
    case 44: /* S4,!S4->CR */
            cpu_2050.CC = (cpu_2050.S_REG & BIT4) ? 2 : 1;     /* <-- */
            break;
    case 45: /* 1->REFETCH */
            cpu_2050.REFETCH = 1;
            break;
    case 46: /* SYNC->OPPANEL */
            if (ADR_CMP == 1)
                allow_man_operation = 1;
            break;
    case 47: /* SCAN*E,10 */
            break;
    case 48: /* 1>SUPROUT */
            cpu_2050.TAGS[0] |= CHAN_SUP_OUT;
            break;
    case 49: /* MPX RESET */
            break;
    case 50: /* E(0)->IBFULL */
            if ((sal->ce & 8) != 0) {
                cpu_2050.IBFULL = 1;
                cpu_2050.BCHI |= BIT0;
            } else {
                cpu_2050.IBFULL = 0;
                cpu_2050.TAGS[0] &= ~CHAN_SUP_OUT;
            }
            break;
    case 51: /* undefined */
            break;
    case 52: /* E->CH */
            /* 0000   Selector channel interrupt routine in process, gen S3 immediate */
            /* 0001   Reset common channel */
            /* 0010   Gate status selector channel */
            /* 0011   Set interface register */
            /* 0100   Set scan channel latch */
            /* 0101   Set Log trg */
            /* 0110   Reset common/MPX channel after logout Used by C4 */
            /* 0111   Local store Write DTC */
            /* 1001   Issue proc with irpt */
            /* 1010   Selector channel end update test */
            /* 1011   Generate time out STOP + CTRL CHK */
            switch (sal->ce) {
            case 0:  /* Interrupt routine */
                   log_selchn("Interrupt\n");
                   if (cpu_2050.CH == 0x3) {
                       cpu_2050.polling[0] = 1;
                       if (cpu_2050.break_out) {
                           cpu_2050.inst_latch = 0;
                           cpu_2050.s3_set = 1;
                       }
                       cpu_2050.BCHI &= ~BIT0;
                       cpu_2050.ROUTINE[cpu_2050.CH] = 0;
                   } else {
                       cpu_2050.BCHI &= ~(BIT1 >> cpu_2050.CH);
                       if (cpu_2050.break_out) {
                           cpu_2050.inst_latch = 0;
                           cpu_2050.s3_set = 1;
                       }
                   }
                   s_update |= BIT3;     /* <-- */
                   break;
                   /* Fall through */
            case 9:  /* Issue proc with irpt */
                   log_selchn("Proceed interrupt\n");
                   if (cpu_2050.break_out) {
                       cpu_2050.s3_set = 1;
                   }
                   if (cpu_2050.CH == 0x3) {
                       cpu_2050.BCHI &= ~BIT0;
                       s_update |= BIT3;     /* <-- */
                   } else {
                       cpu_2050.BCHI &= ~(BIT1 >> cpu_2050.CH);
                       cpu_2050.CHREQ[cpu_2050.CH] |= BIT1;
                   }
                   break;

            case 1:  /* Reset common channel */
                   log_selchn("Reset channel\n");
                   break;

            case 2:  /* Gate status */
                   log_selchn("Gate status\n");
                   cpu_2050.aob_latch = cpu_2050.C_REG[cpu_2050.CH];
                   break;

            case 3: /* Set interface register */
                   log_selchn("Set interface register\n");
                   break;

            case 4: /* Set scan channel latch */
                   log_selchn("Set Scan channel latch\n");
                   break;

            case 5:  /* Set log trg */
                   log_selchn("Set Log trigger\n");
                   break;

            case 6:  /* Reset common/MPX after logout */
                   log_selchn("Reset MPX\n");
                   break;

            case 7:  /* Local store write DTC */
                   log_selchn("Write DTC\n");
                   break;

            case 10:  /* Selector channel update test */
                   log_selchn("Select Update\n");
                   break;

            case 11:  /* Generate timeout */
                   log_selchn("Generate timeout\n");
                   break;
            }
            break;
    case 53: /* undefined */
            break;
    case 54: /* 1->TIMERIRPT */
            timer_irq = 1;
            break;
    case 55: /* T->PSW,IPL->T */
            l_update = (A_SW << 8) | (B_SW << 28) | (C_SW << 24);
                                                         /* MG field */
            break;
    case 56: /* T->PSW T(12-15) to PSW */
            log_trace("PSW aob=%08x\n", cpu_2050.aob_latch);
            cpu_2050.AMWP = (cpu_2050.aob_latch >> 16) & 0xf;
            wait = ((cpu_2050.AMWP & 0x2) != 0);
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
            cpu_2050.TAGS[0] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
            break;
    case 61: /* 1->ADROUT */
            cpu_2050.TAGS[0] |= CHAN_ADR_OUT;
            break;
    case 62: /* 1->COMOUT */
            cpu_2050.TAGS[0] |= CHAN_CMD_OUT;
            break;
    case 63: /* 1->SERVOUT */
            cpu_2050.TAGS[0] |= CHAN_SRV_OUT;
            break;
    }

    /* set local storage address */
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
        case 4: /* 0,CH,0 */
                cpu_2050.LSA = (cpu_2050.CH << 2);
                break;
        case 5: /* 0,CH,1 */
                cpu_2050.LSA = (cpu_2050.CH << 2) | 1;
                break;
        case 6: /* 0,CH,2 */
                cpu_2050.LSA = (cpu_2050.CH << 2) | 2;
                break;
        case 7: /* 0,CH,3 */
                cpu_2050.LSA = (cpu_2050.CH << 2) | 3;
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
                cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.J_REG & 0xf);
                break;
        case 5: /* FN,J|1->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (cpu_2050.J_REG & 0xf) | 1;
                break;
        case 6: /* FN,MD->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (md_old & 0xf);
                break;
        case 7: /* FN,MD|1->LSA */
                cpu_2050.LSA = (cpu_2050.FN) | (md_old & 0xf) | 1;
                break;
        }
    }

    if (cpu_2050.io_mode) {
        switch (sal->wm) {
        case 0: /* Nop */
        case 1:
                break;
        case 2: /* W->L0 */
        case 3:
                l_update &= 0x00ffffff;
                l_update |= ((uint32_t)cpu_2050.w_bus) << 24;
                break;
        case 4: /* W->L1 */
        case 5:
                l_update &= 0xff00ffff;
                l_update |= ((uint32_t)cpu_2050.w_bus) << 16;
                break;
        case 6: /* W->L2 */
        case 7:
                l_update &= 0xffff00ff;
                l_update |= ((uint32_t)cpu_2050.w_bus) << 8;
                break;
        case 8: /* W->L3 */
        case 9:
                l_update &= 0xffffff00;
                l_update |= ((uint32_t)cpu_2050.w_bus) << 0;
                break;
        case 10: /* W,E>R(BMP) */
        case 11:
        case 12: /* W,E>R(BMP)S */
        case 13:
               /* 1111112222222222
                  4567890123456789
                  LLEE      LLLLLL
                  0123      234567
                */
                cpu_2050.SAR_REG = ((uint32_t)cpu_2050.w_bus & 0x7f) | ((sal->ce & 3) << 7) |
                                   (((uint32_t)cpu_2050.w_bus & 0x80) << 2);
                if (cpu_2050.io_mode && (cpu_2050.u_bus & 0x80) != 0) {
                    cpu_2050.SAR_REG &= 0x187;
                }
                cpu_2050.SAR_REG <<= 2;
                cpu_2050.init_bump_mem = 1;
                break;
        case 14: /* Nop */
        case 15:
                break;
        }

        /* MG field in I/O Mode */
        switch (sal->dg) {
        case 0: /* BFR2>BIB.
                   Gate mpx channel buffer 2 to buffer in bus.  */
                break;
        case 1: /* CHPOSTEST.
                   Initiate selector channel position test */
                cpu_2050.chpostest = 1;
                break;
        case 2: /* BFR2>BUS.
                   Gate MPX channel buffer 1 to buffer in bus,
                   Gate MPX channel buffer 2 to bus out (I/O interface) */
                cpu_2050.BUS_OUT[0] = cpu_2050.BFR2 | odd_parity[cpu_2050.BFR2];
                break;
        case 3: /* BFR1>BIB.
                   Gate MPX channel buffer 1 to buffer in bus. */
                break;
        case 4: /* BOB>BFR1.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then Gate buffer out bus to channel buffer 1. */
                cpu_2050.BFR1 = cpu_2050.w_bus;
//                cpu_2050.BUS_OUT[0] = cpu_2050.BFR1 | odd_parity[cpu_2050.BFR1];
                break;
        case 5: /* BOB>BFR2.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then Gate buffer out bus to channel buffer 2. */
                cpu_2050.BFR2 = cpu_2050.w_bus;
                /* Hardware sets lower 3 bits to 1 */
                if (cpu_2050.first_cycle && cpu_2050.ROUTINE[cpu_2050.CH] == 0xb4) {
                    cpu_2050.BFR2 |= 0x7;
                }
                if (cpu_2050.first_cycle && cpu_2050.ROUTINE[cpu_2050.CH] == 0xbc) {  /* RTB7 */
                    cpu_2050.BFR2 = cpu_2050.FLAGS_REG[cpu_2050.CH];
                }
                break;
        case 6: /* BUSI>BFR1.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then bus in (I/O interface) to channel buffer 1 */
                cpu_2050.BFR1 = cpu_2050.BUS_IN[0] & 0xff;
                cpu_2050.BUS_OUT[0] = 0x100;
                break;
        case 7: /* BUSI>BFR2.
                   Gate MPX channel buffer 1 to buffer in bus.
                   Then bus in (I/O interface) to channel buffer 2 */
               cpu_2050.BUS_OUT[0] = cpu_2050.BFR2 | odd_parity[cpu_2050.BFR2];
               cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
               break;
        }

        /* CG field in I/O mode */
        switch ((sal->lb << 1) | sal->mb) {
        case 0:  /* nop */
                break;
        case 1:  /* CH>BI
                    Selector channel byte gate, cause channel to set
                    BGC counter from byte counter and end register.
                    decode of BGC is gated to BS register. by order tr20.
                    When backward, output is crossed before gating to
                    byte states. */
                cpu_2050.BI_REG = cpu_2050.CLI_REG[cpu_2050.CH];
                break;
        case 2:  /* 1>PRI
                    Send end of channel routine signal to channel priority
                    circuits. Used in next to last cycle of routine */
                break;
        case 3:  /* 1>LCY
                    Send last cycle signal to channel routine.
                    Used in last cycle of routine */
                log_trace("Set Last Cycle\n");
                cpu_2050.last_cycle = 1;
                break;
        }

    } else {
        /* Store Mover output */
        switch (sal->wm) {
        case 0: /* No change */
                break;
        case 1: /* W->MMB  */
                if (sal->tr == 3) {
                    cpu_2050.M_REG &= 0xffffff00;
                    cpu_2050.M_REG |= cpu_2050.w_bus & 0xff;
                } else {
                    cpu_2050.M_REG &= ~(0xff << (8 * (3-cpu_2050.MB_REG)));
                    cpu_2050.M_REG |= (cpu_2050.w_bus <<
                                                   (8 * (3-cpu_2050.MB_REG)));
                }
                break;
        case 2: /* W67->MB  */
                cpu_2050.MB_REG = cpu_2050.w_bus & 03;
                break;
        case 3: /* W67->LB  */
                cpu_2050.LB_REG = cpu_2050.w_bus & 03;
                break;
        case 4: /* W27->PSW4, 2-7 -> PSW 34-39  */
                if (load_mode) {
                    timer_irq = 0;
                    timer_update = 0;
                }
                load_mode = 0;
                cpu_2050.CC = (cpu_2050.w_bus >> 4) & 0x3;
                cpu_2050.PMASK = cpu_2050.w_bus & 0xf;
                break;
        case 5: /* W->PSW0  */
                cpu_2050.MASK = cpu_2050.w_bus;
                break;
        case 6: /* WL->J  */
                cpu_2050.J_REG = (cpu_2050.w_bus & 0xf0) >> 4;
                break;
        case 7: /* W->CHCTL  */
                /* Start channel micro op */
                /*
                   00000001 SIO
                   00000010 Channel Count=0 no ST3
                   00000100 Emit Timout chk to Sel Chan.
                   00001000 TCH
                   00010000   Foul on SIO
                   00100000 Timeout.
                   10000000 Issue IRPT Test IO to MPX.
                */
                i = (cpu_2050.L_REG >> 8) & 0xf;
                cpu_2050.CHCTL = cpu_2050.w_bus;
                if (cpu_2050.CHCTL == 0x80) {
                    i = 0;
                }
                if (i > 3) {   /* Invalid channel */
                    cpu_2050.CC = 3;
                    s_update |= BIT3;     /* <-- */
                    cpu_2050.CHCTL = 0;
                    break;
                }
                if (cpu_2050.CHCTL == 0x08) { /* TCH */
                    if (i == 0) {
                        cpu_2050.CC = cpu_2050.IBFULL;
                        if (cpu_2050.ROUTINE[3] == 0x8c) { /* Burst mode */
                           cpu_2050.CC = 2;
                        }
                    } else {
                       if (cpu_2050.polling[i] == 0) {
                           cpu_2050.CC = 2;
                       } else {
                           cpu_2050.CC = 0;
                       }
                    }
                    s_update |= BIT3;     /* <-- */
                    cpu_2050.CHCTL = 0;
                    break;
                }
                if (i != 0) {
                    switch(cpu_2050.CHCTL) {
                    case 1: /* SIO */
                    if (cpu_2050.polling[i] == 0) {
                        cpu_2050.CC = 2;
                        s_update |= BIT3;     /* <-- */
                    } else {
                        cpu_2050.CHREQ[i-1] = BIT4;
                    cpu_2050.polling[i] = 0;
                    cpu_2050.inst_latch = 1;
                   }
                        break;
                  case 4: /* TIO */
                        if ((cpu_2050.BCHI & (1 << (i-1))) != 0) {
                             cpu_2050.CHREQ[i-1] = BIT1;
                    cpu_2050.inst_latch = 1;
       log_trace("Post TIO interupt\n");
                             break;
                        }
                    if (cpu_2050.polling[i] == 0 || cpu_2050.CHREQ[i-1] != 0 ||
                        cpu_2050.CHPOS[i-1] != 0) {
                        cpu_2050.CC = 2;
                        s_update |= BIT3;     /* <-- */
                        break;
                    }
                        cpu_2050.CHREQ[i-1] = BIT2;
                    cpu_2050.polling[i] = 0;
                    cpu_2050.inst_latch = 1;
                        break;
                  case 2: /* HIO */
                        if ((cpu_2050.BCHI & (1 << (i-1))) != 0) {
                             cpu_2050.CC = 0;
                        s_update |= BIT3;     /* <-- */
       log_trace("Post HIO interupt\n");
                             break;
                        }
                         if (cpu_2050.polling[i]) {
                        cpu_2050.CHREQ[i-1] = BIT2;
                    cpu_2050.inst_latch = 1;
                        } else {
                     cpu_2050.TAGS[i] |= CHAN_ADR_OUT;
                    s_update |= BIT3;     /* <-- */
                    cpu_2050.CHCTL = 0;
                     cpu_2050.CD[i-1] = 0;
                     cpu_2050.FLAGS_REG[i-1] &= 0xff;
                    cpu_2050.CC = 2;
       log_trace("HIO terminated\n");
                        }
                        break;
                    case 0x10: /* Foul on SIO */
       log_trace("Foul on SIO\n");
//                        cpu_2050.CC = 2;
                      cpu_2050.CHCTL = 0;
                    s_update |= BIT1|BIT2|BIT3;     /* <-- */
                    }
                } else {
                    if (cpu_2050.polling[0] == 1 && cpu_2050.ROUTINE[3] == 0) {
                        cpu_2050.ROUTINE[3] = 0xa0;
                        cpu_2050.break_in |= 8;
                        cpu_2050.inst_latch = 1;
                    } else {
                        cpu_2050.CC = 2;
                        s_update |= BIT3;     /* <-- */
                    }
                }
                break;
        case 8: /* W,E->A(BMP)  */
                cpu_2050.SAR_REG = ((uint32_t)cpu_2050.w_bus & 0x7f) | ((sal->ce & 3) << 7) |
                                   (((uint32_t)cpu_2050.w_bus & 0x80) << 2);
                if (cpu_2050.io_mode && (cpu_2050.u_bus & 0x80) != 0) {
                    cpu_2050.SAR_REG &= 0x187;
                }
                cpu_2050.SAR_REG <<= 2;
                cpu_2050.init_bump_mem = 1;
                break;
        case 9: /* WL->G1  */
                cpu_2050.G_REG &= 0xf;
                cpu_2050.G_REG |= (cpu_2050.w_bus & 0xf0);
                cpu_2050.G1NEG = 0;
                break;
        case 10: /* WR->G2  */
                cpu_2050.G_REG &= 0xf0;
                cpu_2050.G_REG |= (cpu_2050.w_bus & 0x0f);
                cpu_2050.G2NEG = 0;
                break;
        case 11: /* W->G  */
                cpu_2050.G_REG = cpu_2050.w_bus;
                cpu_2050.G1NEG = 0;
                cpu_2050.G2NEG = 0;
                break;
        case 12: /* W->MMB(E?)  */
                if ((cpu_2050.AMWP & 0x8) != 0) {
                    if ((cpu_2050.w_bus & 0xf) == 0xc)
                        cpu_2050.w_bus = (cpu_2050.w_bus & 0xf0) | 0xa;
                    if ((cpu_2050.w_bus & 0xf) == 0xd)
                        cpu_2050.w_bus = (cpu_2050.w_bus & 0xf0) | 0xb;
                    if ((cpu_2050.w_bus & 0xf0) == 0xf0)
                        cpu_2050.w_bus = (cpu_2050.w_bus & 0x0f) | 0x50;
                }
                cpu_2050.M_REG &= ~(0xff << (8 * (3-cpu_2050.MB_REG)));
                cpu_2050.M_REG |= (cpu_2050.w_bus << (8 * (3-cpu_2050.MB_REG)));
                break;
        case 13: /* WL->MD */
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
                    cpu_2050.MD_REG = (cpu_2050.MD_REG - 1) & 0xf;
                break;
        case 3:
                if (sal->lb)
                    cpu_2050.LB_REG = (cpu_2050.LB_REG + 1) & 3;
                if (sal->mb)
                    cpu_2050.MB_REG = (cpu_2050.MB_REG + 1) & 3;
                if (sal->md)
                    cpu_2050.MD_REG = (cpu_2050.MD_REG + 1) & 0xf;
                break;
        }
    }


    /* Fetch/store local storage */
    switch (sal->sf) {
    case 0: /* R>LS */
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.R_REG;
            break;
    case 1: /* LS>L,R->LS */
            l_update = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = cpu_2050.R_REG;
            break;
    case 2: /* LS>R>LS */
            cpu_2050.R_REG = cpu_2050.LS[cpu_2050.LSA];
            break;
    case 3: /* unknown */
            break;
    case 4: /* L>LS */
            cpu_2050.LS[cpu_2050.LSA] = l_update;
            break;
    case 5: /* LS>R,L>LS */
            cpu_2050.R_REG = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = l_update;
            break;
    case 6: /* LS>L>LS */
            l_update = cpu_2050.LS[cpu_2050.LSA];
            break;
    case 7: /* No function */
            break;
    }

    /* Update registers */
    cpu_2050.L_REG = l_update;
    cpu_2050.S_REG = s_update;

    if (ROS_REP != 0) {
       if (ROS_REP == 2)
          cpu_2050.io_mode = 1;
       else
          cpu_2050.io_mode = 0;
       goto channel;
    }

#if 0
    if (sal->zn == 0 && sal->zf == 14) {  /* Clear I/O mode on RETURN>ROAR */
       cpu_2050.io_mode = 0;
       cpu_2050.ROAR = cpu_2050.BROAR;
       cpu_2050.break_out = 0;
       cpu_2050.break_in = 0;
       cpu_2050.w_bus = cpu_2050.w_back;
       cpu_2050.alu_out = cpu_2050.alu_back;
       cpu_2050.aob_latch = cpu_2050.aob_back;
       cpu_2050.CAR = cpu_2050.CAR_back;
    } else {
#endif
       cpu_2050.ROAR = cpu_2050.NROAR;
//    }

    cpu_2050.first_cycle = 0;

    /* Handle break in cycle */
    if ((cpu_2050.init_mem | cpu_2050.init_bump_mem) == 0 &&
              cpu_2050.mem_state == W1 && cpu_2050.break_in) {
        dtc_latch = 0;
        log_trace("check breakout %x\n", cpu_2050.break_in);
        for (i = 0; i < 4; i++) {
            if (cpu_2050.break_in & (1 << i)) {
               cpu_2050.CH = i;
               cpu_2050.break_in &= ~(1 << i);
               cpu_2050.polling[i] = 0;
               break;
            }
        }
        if (!cpu_2050.break_out) {
            cpu_2050.dead_cycle = 1;
#if 0
            cpu_2050.BROAR = cpu_2050.ROAR;
            cpu_2050.ROAR = cpu_2050.ROUTINE[cpu_2050.CH];
            cpu_2050.LS[0x2c] = cpu_2050.R_REG;  /* Save R register for later */
            cpu_2050.w_back = cpu_2050.w_bus;    /* Save stuff need for next check */
            cpu_2050.alu_back = cpu_2050.alu_out;
            cpu_2050.aob_back = cpu_2050.aob_latch;
            cpu_2050.CAR_back = cpu_2050.CAR;
            cpu_2050.MPX_LST = 0;
#endif
            cpu_2050.break_out = 1;
            log_trace("start breakout %03x\n", cpu_2050.ROAR);
    //    } else {
     //       cpu_2050.ROAR = cpu_2050.ROUTINE[cpu_2050.CH];
      //      log_trace("Next break\n");
        }
    }

    if (cpu_2050.rest_cycle) {
        cpu_2050.rest_cycle = 0;
        if (cpu_2050.s3_set) {
           cpu_2050.S_REG |= BIT3;
           cpu_2050.s3_set = 0;
           log_trace("Set S3\n");
        }
    }
    if (rest_roar) {
            cpu_2050.ROAR = cpu_2050.PROAR;
            cpu_2050.rest_cycle = 1;
    }


channel:

    /* Check if break in cycle */
    if (cpu_2050.mem_state == W1) {
        log_trace("Routine %02x bi=%x bo=%x dtc1=%d dtc2=%d\n", cpu_2050.ROUTINE[3],
                  cpu_2050.break_in, cpu_2050.break_out, dtc1, dtc2);
    }

    /* Check if break in cycle */
    switch (cpu_2050.ROUTINE[3]) {
    case 0x00:   /* Idle */
                /*  A4 <- C-br, status in, CC latch off */
                /*  A5 <- C-br, status in, CC latch off */
                /*  B0 <- Any Ch 0 opcode, poll ctr no req, not test channel */
                /*  B5 <- C-br, not status in, not opin, UCW store trg, any ck off */
                /*  B5 <- A-ck any ck off */
                /*  B7 <- c-br, not status in, not op in, ucw store trg on, any ck on */
                /*  B7 <- a-ck, any ck on */
                /*  C0 <- Not inhibit I/O fix addr, PCI MPX request */
                /*  idle <- C-br, not op in, not status in, UCW store off */
                cpu_2050.TAGS[0] |= CHAN_OPR_OUT;
                if (cpu_2050.polling[3]) {
                    if ((cpu_2050.TAGS_IN[0] & CHAN_REQ_IN) != 0) {
                        cpu_2050.TAGS[0] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                        log_trace("Request in\n");
                    }
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                        log_trace("Select dev\n");
                        cpu_2050.polling[3] = 0;
                        cpu_2050.ROUTINE[3] = 0x80;  /* RTA0 */
                        cpu_2050.break_in |= 8;
                        cpu_2050.BI_REG = 0x3;
                    }
                }
                break;

    case 0x80:  /*  RTA0  Routine A0 - QV210 */
                log_trace("Routine A0\n");
                /*  Address in, poll control */
                /*  A1 <- (A0) BR0 */
                /*  A1 <- (A0) BR0 */
                /* Fetch count and update */
                /* When address in drops, clear command out */
                /* BRC always 0 */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 0) {
                    cpu_2050.ROUTINE[3] = 0x84;   /* RTA1 */
                    cpu_2050.break_in |= 8;
                    cpu_2050.IOSTAT[3] = cpu_2050.BFR2 << 4;
                    cpu_2050.FLAGS_REG[3] = 0;
                }
                break;

    case 0x84:  /*  RTA1  Routine A1 - QV220 */
                /*  A2 <- (A1) BR 1 (output), not status in */
                /*  A2 <- (A1) BR 2 (input), Fwd/bkwd, service in */
                /*  A3 <- (A1) BR 3, Stop, input skip, not service in */
                /*  A4 <- (A1) (BR 1), output, status in, not cc trg on */
                /*  A5 <- (A1) (BR 1), output, status in, CC trg */
                /*  BRC = 1 Output */
                /*  BRC = 2 Input */
                /*  BRC = 3 Stop or Skip */
                log_trace("Routine A1\n");
                /* Data address fetch */
                /* When address in drops, clear command out */
                print_tags("CPU", 0, cpu_2050.TAGS_IN[0], 0);

                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    /* Go grab end status */
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                        log_trace("Status in\n");
                        if ((cpu_2050.BFR2 & 0x40) != 0)  /* Command Chain */
                            cpu_2050.ROUTINE[3] = 0x94;  /* RTA5 */
                        else
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                        cpu_2050.break_in |= 8;
                        break;
                    }
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        if ((cpu_2050.IOSTAT[3] & 0x60) == 0) {
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                            if ((cpu_2050.IOSTAT[3]  & 0x70) == 0x10) { /* Backword */
                                cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                            } else if ((cpu_2050.IOSTAT[3] & 0x70) == 0x30) { /* Skip */
                                cpu_2050.BI_REG = 0;
                            } else {
                            cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                            }
                        }
                        if (cpu_2050.BRC == 1 && (cpu_2050.IOSTAT[3] & 0x70) == 0x60) {   /* Output */
                            cpu_2050.ROUTINE[3] = 0x88;  /* RTA2 */
                            cpu_2050.break_in |= 8;
                            break;
                        }
                        if (cpu_2050.BRC == 3 /*&&(cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0*/) {
                            cpu_2050.ROUTINE[3] = 0x8c;  /* RTA3 */
                            cpu_2050.break_in |= 8;
                            break;
                        }
                        /* Handle next byte */
//                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x88;  /* RTA2 */
                            cpu_2050.break_in |= 8;
 //                       }
                    }
                }
                break;
    case 0x88:  /*  RTA2  Routine A2 - QV230 */
                log_trace("Routine A2\n");
                /* A3 <- input, service in, not service out, (BR 3 or BR 7) */
                /* A3 <- input, (invalid address|prot check) */
                /* A3 <- input, BR 5, Count == 0 */
                /* A3 <- output, (BR3 | BR 7), (Count == 1 | invalid address */
                /* Go back to A2 and wait for state change */
                /* A2 <- BR 7 and input and service in, not service out */
                /* A2 <- BR 7 and output and count > 1 */
                /* BRC 3 Output */
                /* BRC 5 to count zero */
                /* BRC 7 count -1 not = 0 */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    cpu_2050.BUS_OUT[0] = cpu_2050.BFR2 | odd_parity[cpu_2050.BFR2];
                    if (cpu_2050.FLAGS_REG[cpu_2050.CH] != 0) {
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        cpu_2050.TAGS[0] |= CHAN_CMD_OUT;
                    }
                        cpu_2050.ROUTINE[3] = 0xbc;  /* RTB7 */
                        cpu_2050.break_in |= 8;
                        break;
                    }
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        cpu_2050.TAGS[0] |= CHAN_SRV_OUT;
                    }

                    if (cpu_2050.BRC == 3 && cpu_2050.MPX_LST) {  /* End of output */
                        cpu_2050.ROUTINE[3] = 0x9c;  /* RTA7 */
                        cpu_2050.break_in |= 8;
                        break;
                    }
                    if (cpu_2050.BRC == 5) {  /* End of input */
                        cpu_2050.ROUTINE[3] = 0x9c;  /* RTA7 */
                        cpu_2050.break_in |= 8;
                        break;
                    }
                    if ((cpu_2050.IOSTAT[3] & BIT0) != 0) {
                         cpu_2050.MPX_PCI = 1;
                     }
                    break;
                }
                if (cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) {
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                         log_trace("Status in\n");
                         cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                         cpu_2050.break_in |= 8;
                    }
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        log_trace("Service in\n");
                        if ((cpu_2050.IOSTAT[3] & 0x60) == 0x00) {   /* Input */
                             cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                             if ((cpu_2050.IOSTAT[3]  & 0x70) == 0x10) { /* Backword */
                                 switch(cpu_2050.BFR1 & 3) {
                                 case 0:  cpu_2050.BI_REG = 0x1; break;
                                 case 1:  cpu_2050.BI_REG = 0x8; break;
                                 case 2:  cpu_2050.BI_REG = 0x4; break;
                                 case 3:  cpu_2050.BI_REG = 0x2; break;
                                 }
                             } else if ((cpu_2050.IOSTAT[3] & 0x70) == 0x30) { /* Skip */
                                 cpu_2050.BI_REG = 0;
                             } else {
                                 cpu_2050.BI_REG = 0x8 >> ((cpu_2050.BFR1 + 1) & 3);
                            }
                        } else {
                                 cpu_2050.BI_REG = 0;
                        }
                        cpu_2050.ROUTINE[3] = 0x8c;  /* RTA3 */
                        cpu_2050.break_in |= 8;
                    }
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                        log_trace("Oper drop\n");
                        if ((cpu_2050.IOSTAT[3] & 0x70) == 0x60) {   /* output */
                             cpu_2050.ROUTINE[3] = 0x8c;  /* RTA3 */
                             cpu_2050.break_in |= 8;
                        } else {
                             cpu_2050.polling[0] = 1;
                             cpu_2050.ROUTINE[3] = 0x00;
                        }
                    }
                }
                break;
    case 0x02:  /*  RTC0  Routine C0 - QV260 */
    case 0x8c:  /*  RTA3  Routine A3 - QV240 */
                log_trace("Routine A3 %d\n", cpu_2050.MPX_LST);
                /*  A7 <- (A3) (BR 3 | BR 7) (output), decrement count = 1 */
                /*  A7 <- (A3) (BR 3 | BR 7) (output), inv addr */
                /*  A7 <- (A3) (BR 5) (input), decrement count == 0 */
                /*  A7 <- (A3) (BR 7) (output), decrement count == 1 */
                /*  A7 <- (A3) (BR 7) (output), invald addr, prot ck */
                /*  A3 <- (A3) BR 7, service in, not service out, input */
                /*  A3 <- (A3) BR 7, output, count > 1 */
             /* A6 <- IB Not full, BR5, active */
             /* A6 <- IB not full, BR6, not active. */
             /* A6 <- IB full, BR7 */
             if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                 log_trace("RTA3 Last\n");
                    if (cpu_2050.FLAGS_REG[cpu_2050.CH] != 0) {
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        cpu_2050.TAGS[0] |= CHAN_CMD_OUT;
                    }
                        cpu_2050.ROUTINE[3] = 0xbc;  /* RTB7 */
                        cpu_2050.break_in |= 8;
                        break;
                    }
                    if ((cpu_2050.IOSTAT[3] & BIT0) != 0) {
                         cpu_2050.MPX_PCI = 1;
                     }
                 cpu_2050.BUS_OUT[0] = cpu_2050.BFR2 | odd_parity[cpu_2050.BFR2];
                 if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                      if ((cpu_2050.IOSTAT[3] & BIT0) == 0) {
                          cpu_2050.TAGS[0] |= CHAN_SRV_OUT;
                      }
                 }
                 if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                     log_trace("Status in\n");
                        if ((cpu_2050.BFR2 & 0x40) != 0)  /* Command Chain & SLI */
                            cpu_2050.ROUTINE[3] = 0x94;  /* RTA5 */
                        else
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
//                     cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                     cpu_2050.break_in |= 8;
                 }
                 if ((cpu_2050.BRC == 7 && cpu_2050.MPX_LST) || cpu_2050.BRC == 5) {  /* End of output */
                     if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                         cpu_2050.ROUTINE[3] = 0xb4;  /* RTB5 */
                     } else {
                         cpu_2050.ROUTINE[3] = 0x9c;  /* RTA7 */
                     }
                     cpu_2050.break_in |= 8;
                 }
                 break;
             }
             if (cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) {
                 if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                     log_trace("Status in\n");
                     if ((cpu_2050.BFR1 & 0x40) != 0)   /* Command Chain & SLI */
                         cpu_2050.ROUTINE[3] = 0x94;  /* RTA5 */
                     else
                         cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                     cpu_2050.break_in |= 8;
                 }
                 if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                     if ((cpu_2050.IOSTAT[3] & 0x60) == 0x00) {   /* Input */
                          cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                          if ((cpu_2050.IOSTAT[3]  & 0x70) == 0x10) { /* Backword */
                         switch(cpu_2050.BFR1 & 3) {
                         case 0:  cpu_2050.BI_REG = 0x1; break;
                         case 1:  cpu_2050.BI_REG = 0x8; break;
                         case 2:  cpu_2050.BI_REG = 0x4; break;
                         case 3:  cpu_2050.BI_REG = 0x2; break;
                         }
                          } else if ((cpu_2050.IOSTAT[3] & 0x70) == 0x30) { /* Skip */
                              cpu_2050.BI_REG = 0;
                          } else {
                              cpu_2050.BI_REG = 0x8 >> ((cpu_2050.BFR1 + 1) & 3);
                         }
                        } else {
                                 cpu_2050.BI_REG = 0;
                        }
                     cpu_2050.ROUTINE[3] = 0x8c;  /* RTA3 */
                     cpu_2050.break_in |= 8;
                 }
                 if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                     cpu_2050.ROUTINE[3] = 0xb4;  /* RTB5 */
                     cpu_2050.break_in |= 8;
                 }
             }
             break;
    case 0x90:  /*  RTA4  Routine A4 - QV250 */
                log_trace("Routine A4\n");
                /*  A6 <- (A4) (BR 5), IB Not full, active  */
                /*  A6 <- (A4) (BR 6), IB Not full, not active */
                /*  A6 <- (A4) (BR 7), IB full */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 5 || cpu_2050.BRC == 6 || cpu_2050.BRC == 7) {
                        cpu_2050.ROUTINE[3] = 0x98;   /* RTA6 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 15) {      /* IBFull and active, save status and continue */
                        cpu_2050.ROUTINE[3] = 0xb4;   /* RTB5 */
                        cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0x94:  /*  RTA5  Routine A5 - QV350 */
                log_trace("Routine A5\n");
                /*  A6 <- (A5) (BR 5 | BR 7), not SILI Flag, count != 0 */
                /*  A6 <- (A5) (BR 5 | BR 7), ((not device end, SMS) | unit execp | unit check |
                                         busy | attn), SILI flag) | Count = 0) */
                /*  A6 <- (A5) (BR 5 | BR 7), (SILI flag, not ch end, dev end, seq ctr 5 not cc end rcved) | Count = 0) */
                /*  B5 <- (A5) (BR 3), ch end, not dev end */
                /*  D0 <- (A5) (BR 6), (Ch end, dev end)  | CC end rec */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                     cpu_2050.IOSTAT[3]  &= 0x70;
                     cpu_2050.IOSTAT[3] |= cpu_2050.BFR1 & 0x80;
                     if (cpu_2050.BRC == 3) {          /* Channel end no Device end */
                         cpu_2050.ROUTINE[3] = 0xb4;   /* RTB5 */
                         cpu_2050.break_in |= 8;
                     }
                     if (cpu_2050.BRC == 6) {          /* Device end recieved */
                         cpu_2050.ROUTINE[3] = 0xa2;   /* RTD0 */
                         cpu_2050.break_in |= 8;
                     }
                     if (cpu_2050.BRC == 7 || cpu_2050.BRC == 5) {          /* Interrupt prep */
                         cpu_2050.ROUTINE[3] = 0x98;   /* RTA6 */
                         cpu_2050.break_in |= 8;
                     }
                }
                break;

    case 0x98:  /*  RTA6  Routine A6 - QV820 */
                log_trace("Routine A6\n");
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 15) {
                    cpu_2050.ROUTINE[3] = 0xb4;   /* RTB5 */
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0x9c:  /*  RTA7  Routine A7 - QV270 */
                log_trace("Routine A7\n");
                /*  D0 <- (A7) (BR 6), CDA flag, Not inv addr, not prot ch */
                /*  A3 <- (A7) BR 7, service in, not service out, prot ck, inv address CDA */
                /*  A3 <- (A7) BR 7, service in, not service out, prot ck, inv address not CDA, input */
                if ((cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) && cpu_2050.BRC == 7) {
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                        cpu_2050.ROUTINE[3] = 0xb4;     /* RTB5 */
                        cpu_2050.break_in |= 8;
                    }
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                        if ((cpu_2050.BFR2 & 0x40) != 0)
                            cpu_2050.ROUTINE[3] = 0x94;  /* RTA5 */
                        else
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                        cpu_2050.break_in |= 8;
                        cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                    }
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                        cpu_2050.ROUTINE[3] = 0x8c;      /* RTA3 */
                        cpu_2050.break_in |= 8;
                        cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                    }
                }
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 6) {        /* Fetch next CCW */
                        cpu_2050.ROUTINE[3] = 0xa2;      /* RTD0 */
                        cpu_2050.break_in |= 8;
                }
                break;

    case 0xa0:  /*  RTB0  Routine B0 - QV410 */
                /*  Start of I/O operation */
                log_trace("Routine B0\n");
                /*  B1 <- (B0) (BR 0), start I/O */
                /*  C1 <- (B0) (BR 0), test I/O or int Test I/O */
                /*  C6 <- (B0) (BR 0), Halt I/O */
                /*  idle <- (B0) (BR 0), foul on SIO, Seq ctrl idle */
                /*  idle <- (B0) (BR 0), foul on SIO, Seq ctrl 5 busy */
                /*  idle <- (B0) (BR 6), inv bump addr */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 0) {
                        cpu_2050.polling[0] = 0;
                        if (cpu_2050.CHCTL == 0x01)  /* SIO */
                           cpu_2050.ROUTINE[3] = 0xa4;  /* RTB1 */
                        if (cpu_2050.CHCTL == 0x04 || cpu_2050.CHCTL == 0x80)  /* TIO */
                           cpu_2050.ROUTINE[3] = 0x86;  /* RTC1 */
                        if (cpu_2050.CHCTL == 0x02)  /* HIO */
                           cpu_2050.ROUTINE[3] = 0x9a;  /* RTC6 */
                        if (cpu_2050.CHCTL == 0x20)  /* Timeout  */
                           cpu_2050.ROUTINE[3] = 0x96;  /* RTC5 */
                        if (cpu_2050.CHCTL == 0x10) { /* Foul on SIO */
    cpu_2050.S_REG |= BIT1|BIT2;
//                        cpu_2050.CC = 2;
                            cpu_2050.CHCTL = 0;
                        cpu_2050.s3_set = 1;      /* Set stat 3. */
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.polling[0] = 1;
                            break;
                        }
                        if (cpu_2050.ROUTINE[3] != 0xa0) {
                            cpu_2050.break_in |= 8;
                        }
                    }
                    if (cpu_2050.BRC == 6) {
                        cpu_2050.s3_set = 1;      /* Set stat 3. */
                        cpu_2050.CC = 3;       /* CC = 3 */
                        cpu_2050.CHCTL = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.polling[0] = 1;
                    }
                }
                break;

    case 0xa4:  /*  RTB1  Routine B1 - QV420 */
                /*  Start I/O Unit select */
                log_trace("Routine B1\n");
                /*  B2 <- (B1) (BR 6), seq ctrl idl, op in, addr in */
                /*  C5 <- (B1) (BR 6), seq ctrl idle, opin, status in, not sel in */
                /*  D5 <- (B1) (BR 1), seq ctrl idle, prog ck */
                /*  D5 <- (B1) (BR 1), seq ctrl idel, invalid op */
                /*  idle <- (B1) (BR 2), seq crtl active */
                /*  idle <- (B1) (BR 6), seq ctrl idle, op in, sel in */
                if (cpu_2050.break_out) {
                    break;
                }
                if (cpu_2050.BRC == 6) {
                    /* Unit selected */
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_ADR_IN)) ==
                              (CHAN_OPR_IN|CHAN_ADR_IN)) {
                        cpu_2050.ROUTINE[3] = 0xa8; /* RTB2 */
                        cpu_2050.break_in |= 8;
                    }
                    /* Unit busy */
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_STA_IN)) == 0) {
                        cpu_2050.ROUTINE[3] = 0x96; /* RTC5 */
                        cpu_2050.break_in |= 8;
                    }
                }
                if (cpu_2050.BRC == 2) {
                    cpu_2050.s3_set = 1;     /* Set stat 3. */
                    cpu_2050.CC = 2;      /* Set CC=2, busy */
                    cpu_2050.CHCTL = 0;
                    cpu_2050.ROUTINE[3] = 0x00;
                    cpu_2050.inst_latch = 0;
                    cpu_2050.polling[0] = 1;
                }
                if (cpu_2050.BRC == 1) { /* Program check */
                    cpu_2050.ROUTINE[3] = 0xb6; /* RTD5 */
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0xa8:  /*  RTB2  Routine B2 - QV430 */
                /* Compare unit address */
                log_trace("Routine B2\n");
                /*  B3 <- (B2) (BR 4), status in */
                /* Log out BR 1 */
                if (cpu_2050.break_out == 0 && cpu_2050.BRC == 4 && (cpu_2050.TAGS_IN[0] & (CHAN_STA_IN)) != 0) {
                    if ((cpu_2050.IOSTAT[3]  & BIT3) != 0 && (cpu_2050.BFR2 & 0x7) == 0) { /* Skip */
                        cpu_2050.BFR2 |= 0x03; /* Skip operator */
                    }
                    cpu_2050.ROUTINE[3] = 0xac; /* RTB3 */
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0xac:  /*  RTB3  Routine B3 - QV440 */
                log_trace("Routine B3\n");
                /*  A2 <- (B3) BR 7, Service in, input, status = 0 */
                /*  A2 <- (B3) BR 7, Status != 0, not input */
                /*  D0 <- (B3) (BR 6), CC flag, Ch end, Dev end */
                /*  D5 <- (B3) (BR 15), CC flag, status error */
                /*  D5 <- (B3) (BR 15), not cc flag, status != 0 */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 15) {    /* Initial status not zero */
                        cpu_2050.ROUTINE[3] = 0xb6;  /* RTD5 */
                        cpu_2050.break_in |= 8;
                        break;
                    }

                    /* Start I/O complete */
                    cpu_2050.CC = 0;
                    cpu_2050.CHCTL = 0;
                    cpu_2050.inst_latch = 0;
                    cpu_2050.s3_set = 1;

                    if (cpu_2050.BRC == 3) {        /* CC, no device end */
                            cpu_2050.ROUTINE[3] = 0xb4;  /* RTB5 */
                            cpu_2050.TAGS_IN[0] &= ~(CHAN_SUP_OUT);
                            cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 6) {        /* CC, device end, fetch next */
                            /* Copy PCI flag */
                            cpu_2050.IOSTAT[3] &= (BIT1|BIT2|BIT3|BIT4);
                            if (cpu_2050.BFR2 & BIT4) {
                                cpu_2050.IOSTAT[3] |= BIT0;
                            }
                            cpu_2050.ROUTINE[3] = 0xa2;  /* RTD0 */
                            cpu_2050.break_in |= 8;
                    }
                }
                if (dtc1 && cpu_2050.CH == 3 && cpu_2050.BRC == 7) {
                    if ((cpu_2050.IOSTAT[3] & 0x70) == 0x10) {
                         cpu_2050.BI_REG = 0x1 << (cpu_2050.BFR1 & 3);
                    } else {
                         cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                    }
                }

                if (cpu_2050.break_out == 0 && cpu_2050.BRC == 7) {       /* Good status, no CC */
                    /* If service in, go transfer some data */
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_SRV_IN)) != 0) {
                        if ((cpu_2050.IOSTAT[3] & 0x70) != 0x60) {   /* Not output */
                             cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                             if ((cpu_2050.IOSTAT[3]  & 0x70) == 0x10) { /* Backword */
                                 cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                             } else if ((cpu_2050.IOSTAT[3] & 0x70) == 0x30) { /* Skip */
                                 cpu_2050.BI_REG = 0;
                             } else {
                                 cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                            }
                        }
                        cpu_2050.ROUTINE[3] = 0x88; /* RTA2 */
                        cpu_2050.break_in |= 8;
                    }
                    /* If status in, go check it out */
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_STA_IN)) != 0) {
                        cpu_2050.ROUTINE[3] = 0x90; /* RTA4 */
                        cpu_2050.break_in |= 8;
                    }
                    /* If device drops oper in go save count */
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                        cpu_2050.ROUTINE[3] = 0xb4; /* RTB5 */
                        if ((cpu_2050.IOSTAT[3] & (BIT1)) == 0) { /* If input set to output */
                             cpu_2050.IOSTAT[3] &= BIT0;          /* Set I/O Stat to output */
                             cpu_2050.IOSTAT[3] |= BIT1|BIT2;
                        }
                        cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0xb4:  /*  RTB5  Routine B5 - QV460 */
                /* Store count in bump */
                log_trace("Routine B5\n");
                /*  B6 <- (B5) (BR 2) */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    cpu_2050.ROUTINE[3] = 0xb8; /* RTB6 */
//                    cpu_2050.BI_REG = 0x7;
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0xb8:  /*  RTB6  Routine B6 - QV470 */
                /* Store data address in Bump */
                log_trace("Routine B6\n");
                /*  idle <- (B6) not opt in, not status in */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_STA_IN)) == 0) {
                    cpu_2050.TAGS[0] &= ~CHAN_SUP_OUT;
                    cpu_2050.ROUTINE[3] = 0x0;   /* None */
                    cpu_2050.polling[0] = 1;
                    }
                    if ((cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                         log_trace("Status in\n");
                         cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                         cpu_2050.break_in |= 8;
                    }

                }
                break;

    case 0xbc:  /*  RTB7  Routine B7 - QV840 */
                log_trace("Routine B7\n");
                /*  B5 <- (B7) (BR 11) */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 11) {
                    cpu_2050.ROUTINE[3] = 0xb4; /* RTB5 */
//                    cpu_2050.IOSTAT[3] = BIT1|BIT2;
                    cpu_2050.FLAGS_REG[3] = 0;
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0x86:  /*  RTC1  Routine C1 - QV520 */
                log_trace("Routine C1\n");
                /*  C2 <- (C1) (BR 6), OP in, Addr in, (dev end| attn IB| idle) */
                /*  C5 <- (C1) (BR 6), not op in, not int test I/O, (dev end | attn IB |idle) */
                /*  C7 <- (C1) (BR 1), ch end queued */
                /*  C7 <- (C1) (BR 1), Ch end in IB, test I/O == IB */
                /*  idle <- (C1) (BR 2), (busy | ch end rec) */
                /*  idle <- (C1) (BR 2), ch end in IB, test I/o uo =1 IB uo */
                if (cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) {
                    if (cpu_2050.BRC == 6) {
                        if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_ADR_IN)) ==
                                  (CHAN_OPR_IN|CHAN_ADR_IN)) {
                            cpu_2050.ROUTINE[3] = 0x8a; /* RTC2 */
                            cpu_2050.break_in = 8;
                        }
                        if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_ADR_IN)) ==
                                  (CHAN_STA_IN)) {
                            cpu_2050.ROUTINE[3] = 0x8e; /* RTC3 */
                            cpu_2050.break_in = 8;
                        }
                    }
                }
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 2) {  /* IB UA != Inst UA */
                        cpu_2050.CC = 2;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.polling[0] = 1;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.s3_set = 1;      /* Set stat 3. */
                    }
                    if (cpu_2050.BRC == 1) {  /* Channel end queued. */
                        cpu_2050.ROUTINE[3] = 0x9e; /* RTC7 */
                        cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0x8a:  /*  RTC2  Routine C2 - QV540 */
                log_trace("Routine C2\n");
                /*  C3 <- (C2) (BR 2), addr in, status in */
                if (cpu_2050.break_in == 0 && (cpu_2050.break_out == 0 || cpu_2050.last_cycle)) {
                    if (cpu_2050.BRC == 2) {
                        if ((cpu_2050.TAGS_IN[0] & (CHAN_STA_IN)) != 0) {
                            cpu_2050.ROUTINE[3] = 0x8e; /* RTC3 */
                            cpu_2050.break_in |= 8;
                        }
                    }
                    if (cpu_2050.BRC == 1) {
                        /* Set channel log, inst ua != interface ua */
                    }
                }
                break;

    case 0x8e:  /*  RTC3  Routine C3 - QV550 */
                log_trace("Routine C3\n");
                /*  D5 <- (C3) (BR 3), (any busy|SMS) */
                /*  D5 <- (C3) (BR 3), !(any busy|SMS), (ch end|dev end queue) */
                /*  idle <- (C3) (BR 2), status = 0, (not any busy| not SMS|not ch end| dev end que */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 3) {  /* Channel end queued. */
                        cpu_2050.ROUTINE[3] = 0xb6; /* RTD5 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 2) {  /* IB UA != Inst UA */
                        cpu_2050.CC = 0;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.polling[0] = 1;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.s3_set = 1;      /* Set stat 3. */
                    }
                }
                break;

    case 0x92:  /*  RTC4  Routine C4 - QV850 */
                log_trace("Routine C4\n");
                break;

    case 0x96:  /*  RTC5  Routine C5 - QV810 */
                log_trace("Routine C5\n");
                /*  D5 <- (C5) (BR 3) */
                /* Check for DTC2 */
                if (dtc2 && cpu_2050.CH == 3 && dtc2) {
                    cpu_2050.TAGS[0] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                }
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 3) {
                    if (cpu_2050.CHCTL == 0x20) {
                        log_trace("C5 return\n");
                        cpu_2050.s3_set = 1;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.polling[0] = 1;
                    } else {
                        log_trace("C5 -> D5\n");
                        cpu_2050.ROUTINE[3] = 0xb6;  /* RTD5 */
                        cpu_2050.break_in = 8;
                    }
                }
                break;

    case 0x9a:  /*  RTC6  Routine C6 - QV620 */
                log_trace("Routine C6\n");
                /*  D5 <- (C6) (BR 6), op in, addr in, wait not op in */
                /*  C5 <- (C6) (BR 6), Not op in, status in */
                /*  idle <- (C6) (BR 2), (chn end in IB | ch end queued */
                /*  idle <- (C6) (BR 6), sel in, not op in, not status in */
                if ((cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) && cpu_2050.BRC == 6) {
                     if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN|CHAN_STA_IN)) == CHAN_STA_IN) {
                         cpu_2050.ROUTINE[3] = 0x96;  /* RTC5 */
                         cpu_2050.break_in = 8;
                     }
                     if ((cpu_2050.TAGS_IN[0] & CHAN_ADR_IN) != 0) {
                         cpu_2050.TAGS[0] |= CHAN_ADR_OUT;   /* Raise ADR Out to halt device */
                         cpu_2050.ROUTINE[3] = 0xb6;  /* RTD5 */
                         cpu_2050.break_in = 8;
                     }
                     if ((cpu_2050.TAGS_IN[0] & CHAN_SEL_OUT) != 0) {
                        cpu_2050.s3_set = 1;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.polling[0] = 1;
                     }
                }
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 2) {
                        cpu_2050.s3_set = 1;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.polling[0] = 1;
                }
                break;

    case 0x9e:  /*  RTC7  Routine C7  - QV530 */
                log_trace("Routine C7\n");
                /*  idle <- (C7) (BR 2), ucw uo != test I/O */
                /*  D5 <- (C7) (BR 3) */
                /*  C2 <- (C7) (BR 6), Op in addr in */
                /*  C5 <- (C7) (BR 6), not op in, not int test I/O */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 6) {
                        cpu_2050.ROUTINE[3] = 0x96; /* RTC5 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 3) {  /* */
                        cpu_2050.ROUTINE[3] = 0xb6;   /* RTD5 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 2) {  /* IB UA != Inst UA */
                        cpu_2050.CC = 2;
                        cpu_2050.polling[0] = 1;
                        cpu_2050.ROUTINE[3] = 0x00;
                        cpu_2050.CHCTL = 0;
                        cpu_2050.inst_latch = 0;
                        cpu_2050.s3_set = 1;      /* Set stat 3. */
                    }
                }
                break;

    case 0xa2:  /*  RTD0  Routine D0 - QV360 */
                log_trace("Routine D0\n");
                /*  D1 <- (D0) (BR 3), not cda, no op in */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.BRC == 1) {
                    cpu_2050.ROUTINE[3] = 0xa6; /* RTD1 */
                    cpu_2050.break_in |= 8;
                }
                break;

    case 0xa6:  /*  RTD1  Routine D1 - QV310 */
                log_trace("Routine D1\n");
                /*  D7 <- (D1) (BR 3) */
                /*  D2 <- (D1) (BR 6), op in, addr in */
                /*  D4 <- (D1) (BR 2) */
                if (cpu_2050.last_cycle && cpu_2050.CH == 3) {
                    if (cpu_2050.BRC == 6) {
                        cpu_2050.ROUTINE[3] = 0xaa; /* RTD2 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 2) {
                        /* If TIC set IOS1 */
                        if ((cpu_2050.aob_latch & 0xff000000) == 0x08000000) {
                           cpu_2050.IOSTAT[3]  |= BIT1;
                        }
                        cpu_2050.ROUTINE[3] = 0xb2; /* RTD4 */
                        cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0xaa:  /*  RTD2  Routine D2 - QV330 */
                log_trace("Routine D2\n");
                /*  D3 <- (D2) (BR 2), status in */
                if ((cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) && cpu_2050.BRC == 2) {    /* Status check */
                    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                        cpu_2050.ROUTINE[3] = 0xae;  /* RTD3 */
                        cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0xae:  /*  RTD3  Routine D3 - QV340 */
                log_trace("Routine D3\n");
                /*  A2 <- (D3) BR 1, not prog ck, Status = 0, new CC flag, service in, not skip, input */
                /*  A2 <- (D3) BR 1, not prog ck, Status = 0, new CC flag, not input */
                /*  A2 <- (D3) BR 1, not prog ck, Status = 0, not new CC flag, not input */
                /*  A2 <- (D3) BR 1, not prog ck, Status = 0, new CC flag, service in, not skip, input */
                /*  A3 <- (D3) BR 3, not prog ck, status = 0, input, skip, service in, not new CC */
                /*  A3 <- (D3) BR 3, not prog ck, status = 0, input, skip, service in, new CC */
                /*  A6 <- (D3) (BR 5) IB Not full, prog ck */
                /*  A6 <- (D3) (BR 5) IB Not full, no prog ck, new cc flag, unit status = error  */
                /*  A6 <- (D3) (BR 7) IB full, prog ck  */
                /*  D0 <- (D3) (BR 6), not prog ck, new cc, status eq, not err, dev end */
                if (cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) {
                    if (cpu_2050.BRC == 1) {        /* Data */
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 &&
                            (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                        }
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 &&
                            (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                            cpu_2050.ROUTINE[3] = 0x88;  /* RTA2 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                        }
                        if ((cpu_2050.TAGS_IN[0] & CHAN_OPR_IN) == 0) {
                            cpu_2050.ROUTINE[3] = 0xb4; /* RTB5 */
                            if ((cpu_2050.IOSTAT[3]  & 0x60) == 0) /* If input set to output */
                                 cpu_2050.IOSTAT[cpu_2050.CH]  = 0x60;     /* Set I/O Stat to output */
                            cpu_2050.break_in |= 8;
                        }
                    }
                    if (cpu_2050.BRC == 3) {        /* Non-zero initial status */
                        if ((cpu_2050.TAGS_IN[0] & CHAN_OPR_IN) == 0) {
                            cpu_2050.ROUTINE[3] = 0xb4; /* RTB5 */
                            if ((cpu_2050.IOSTAT[cpu_2050.CH]  & 0x60) == 0) /* If input set to output */
                                 cpu_2050.IOSTAT[cpu_2050.CH]  = 0x60;     /* Set I/O Stat to output */
                            cpu_2050.break_in |= 8;
                        }
                    }
                    if (cpu_2050.BRC == 6) {        /* Fetch next CCW */
                            cpu_2050.ROUTINE[3] = 0xa2;  /* RTD0 */
                            cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 5) {        /* Fetch next CCW */
                            cpu_2050.ROUTINE[3] = 0x98;  /* RTA6 */
                            cpu_2050.break_in |= 8;
                    }
                }
                break;

    case 0xb2:  /*  RTD4  Routine D4 - QV320 */
                log_trace("Routine D4\n");
                /*  D0 <- (D4) (BR 1), M/op = TIC, Valid addr, Not prev TIC */
                /*  D2 <- (D4) (BR 6), CC, M/op invalid */
                /*  D2 <- (D4) (BR 6), M/op valid, not TIC */
                /*  D7 <- (D4) | (BR 3) | CDA), Prev TIC, Valid addr, M/op TIC */
                /*  D7 <- (D4) | (BR 3) | CDA), inv Valid addr, M/op TIC */
                /*  D7 <- (D4) | (BR 3) | CDA), M/op invalid */
                /*  D7 <- (D4) | (BR 3) | CDA), M/op valid, not TIC */
                if (cpu_2050.break_out == 0 || (cpu_2050.last_cycle && cpu_2050.CH == 3)) {
                    if (cpu_2050.BRC == 3) {
                        cpu_2050.ROUTINE[3] = 0xbe; /* RTD7 */
                        cpu_2050.break_in |= 8;
                    }
                    if (cpu_2050.BRC == 6) {
                        if ((cpu_2050.TAGS_IN[0] & CHAN_ADR_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0xaa; /* RTD2 */
                            cpu_2050.break_in |= 8;
                        }
                    }
                    if (cpu_2050.BRC == 1) {        /* Fetch next CCW */
                        if ((cpu_2050.IOSTAT[3]  & BIT3) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_ADR_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0xa2;  /* RTD0 */
                            cpu_2050.break_in |= 8;
                        }
                        if ((cpu_2050.IOSTAT[3]  & BIT3) != 0) {
                            cpu_2050.ROUTINE[3] = 0xa2;  /* RTD0 */
                            cpu_2050.break_in |= 8;
                        }
                    }
                }
                break;

    case 0xb6:  /*  RTD5  Routine D5 - QV830 */
                log_trace("Routine D5 %d %d\n", cpu_2050.first_cycle, cpu_2050.CH);
                /* A2 <- (BR 1 | BR 3), not input */
                /* A2 <- (BR 1 | BR 3), input, not skip, service in */
                /* A4 <- BR 7 and not service in, status in, not cc trg off */
                /* A5 <- BR 7 and not service in, status in, cc trg on */
                cpu_2050.TAGS[0] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                if (cpu_2050.last_cycle && cpu_2050.CH == 3 &&cpu_2050.BRC == 0) {
                    cpu_2050.ROUTINE[3] = 0xb8; /* RTB6 */
                    cpu_2050.break_in |= 8;
                }
                break;
    case 0xbe:  /*  RTD7  Routine D7 - QV370 */
                log_trace("Routine D7\n");
                /*  idle <- (D7) (BR 7), not ser in, not op in, not status in, UCW store off */
                /*  A2 <- (D7) (BR 1 | BR 3) not input */
                /*  A2 <- (D7) (BR 1 | BR 3) input, not skip, service in */
                /*  A3 <- (D7) BR 7, service in */
                /*  A3 <- (D7) (BR 1 | BR 3), service in, input, skip */
                /*  A4 <- (D7) (BR 7), not service in, status in, not cc tgr */
                /*  A5 <- (D7) (BR 7), not service in, status in, cc tgr on */
                if (cpu_2050.break_out == 0) {
                    if (cpu_2050.BRC == 7) {    /* Program check */
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                        }
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x8c;  /* RTA3 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                            cpu_2050.BI_REG = 0x8 >> ((cpu_2050.BFR1 + 1) & 3);
                        }
                    }
                    if (cpu_2050.BRC == 1 || cpu_2050.BRC == 3) {
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_STA_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x90;  /* RTA4 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                        }
                        if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) == 0 && (cpu_2050.TAGS_IN[0] & CHAN_SRV_IN) != 0) {
                            cpu_2050.ROUTINE[3] = 0x88;  /* RTA2 */
                            cpu_2050.break_in |= 8;
                            cpu_2050.BFR2 = cpu_2050.BUS_IN[0] & 0xff;
                            cpu_2050.BI_REG = 0x8 >> (cpu_2050.BFR1 & 3);
                        }
                        if ((cpu_2050.TAGS_IN[0] & (CHAN_OPR_IN)) == 0) {
                            if ((cpu_2050.IOSTAT[3] & 0x60) == 0) { /* If input set to output */
                                 cpu_2050.IOSTAT[3] = 0x60;     /* Set I/O Stat to output */
                            }
                            cpu_2050.ROUTINE[3] = 0xb4;  /* RTB5 */
                            cpu_2050.break_in |= 8;
                        }
                    }
                }
                break;
    }

    /* Process MPX channel operations */
    if (cpu_2050.IBFULL) {
        cpu_2050.TAGS[0] |= CHAN_SUP_OUT;
    }
    cpu_2050.TAGS_IN[0] &= IN_TAGS;
    cpu_2050.TAGS_IN[0] |= cpu_2050.TAGS[0];
    for (dev = chan[0]; dev != NULL; dev = dev->next) {
         dev->bus_func(dev, &cpu_2050.TAGS_IN[0], cpu_2050.BUS_OUT[0], &cpu_2050.BUS_IN[0]);
    }

    /* If we are polling and get Request in, enable select. */
    if (cpu_2050.polling[0] && cpu_2050.ROUTINE[3] == 0x00) {
        if ((cpu_2050.TAGS_IN[0] & CHAN_REQ_IN) != 0) {
            cpu_2050.TAGS[0] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
            log_trace("Request in\n");
        }
        if ((cpu_2050.TAGS_IN[0] & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
            log_trace("Device select\n");
            cpu_2050.polling[0] = 0;
            cpu_2050.ROUTINE[3] = 0x80;  /* RTA0 */
            cpu_2050.break_in |= 8;
            cpu_2050.BI_REG = 0x3;
        }
    }

    /* Drop suppress in when Select out rises */
    if ((cpu_2050.TAGS[0] & CHAN_SEL_OUT) != 0) {
        log_trace("Clear Suppress out\n");
        cpu_2050.TAGS[0] &= ~(CHAN_SUP_OUT);
    }

    /* If device is addresses and there is no response, terminate */
    if ((cpu_2050.TAGS[0] & (CHAN_SEL_OUT|CHAN_ADR_OUT)) == (CHAN_SEL_OUT|CHAN_ADR_OUT) &&
        (cpu_2050.TAGS_IN[0] & CHAN_SEL_IN) != 0) {
//        print_tags("busy", 0, cpu_2050.TAGS_IN[0], 0);
        cpu_2050.TAGS[0] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
        cpu_2050.CC = 3;
        cpu_2050.CHCTL = 0;
        cpu_2050.s3_set = 1;
        log_trace("cpu_2050.CC %d\n", cpu_2050.CC);
        cpu_2050.ROUTINE[3] = 0;
        cpu_2050.inst_latch = 0;
        cpu_2050.polling[0] = 1;
    }

    /* Drop select out when address in raise  */
    if ((cpu_2050.TAGS[0] & CHAN_SEL_OUT) != 0 && (cpu_2050.TAGS_IN[0] & (CHAN_ADR_IN)) != 0) {
        cpu_2050.TAGS[0] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
    }

    /* Drop address out when we get operation in */
    if ((cpu_2050.TAGS_IN[0] & (CHAN_ADR_OUT|CHAN_OPR_IN)) == (CHAN_ADR_OUT|CHAN_OPR_IN)) {
        cpu_2050.TAGS[0] &= ~CHAN_ADR_OUT;
    }

    /* Drop command out when address in drops */
    if ((cpu_2050.TAGS[0] & CHAN_CMD_OUT) != 0 && (cpu_2050.TAGS_IN[0] & (CHAN_ADR_IN)) == 0) {
        cpu_2050.TAGS[0] &= ~CHAN_CMD_OUT;
    }

    /* Drop service out when status in or service in drops  */
    if ((cpu_2050.TAGS[0] & CHAN_SRV_OUT) != 0 && (cpu_2050.TAGS_IN[0] & (CHAN_STA_IN|CHAN_SRV_IN)) == 0) {
        cpu_2050.TAGS[0] &= ~(CHAN_SRV_OUT|CHAN_ADR_OUT);
        /* If select out still up, drop it */
        if ((cpu_2050.TAGS[0] & CHAN_SEL_OUT) != 0) {
            cpu_2050.TAGS[0] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
        }
    }

    if (cpu_2050.last_cycle && cpu_2050.CH == 3 && cpu_2050.break_out && cpu_2050.MPX_PCI) {
        cpu_2050.MPX_PCI = 0;
        cpu_2050.ROAR = 02;
        log_trace("continue PCI %x\n", cpu_2050.break_in);
        cpu_2050.first_cycle = 1;
        cpu_2050.io_mode = 1;
        dtc_latch = 0;
        cpu_2050.last_cycle = 0;
        cpu_2050.ROUTINE[3] = 0x02;  /* RTC0 */
    }
    /* Handle continue break in cycle */
    if (cpu_2050.last_cycle && (cpu_2050.break_in & (1 << cpu_2050.CH)) != 0 && cpu_2050.break_out) {
        log_trace("continue breakout %x %d\n", cpu_2050.break_in, cpu_2050.MPX_PCI);
        cpu_2050.first_cycle = 1;
        cpu_2050.io_mode = 1;
        dtc_latch = 0;
        cpu_2050.polling[cpu_2050.CH] = 0;
        if (cpu_2050.CH == 3 && cpu_2050.MPX_PCI) {
            cpu_2050.MPX_PCI = 0;
            cpu_2050.ROAR = 02;
            cpu_2050.ROUTINE[3] = 0x02;  /*  RTC0 */
        } else {
            cpu_2050.ROAR = cpu_2050.ROUTINE[cpu_2050.CH];
            cpu_2050.break_in &= ~(1 << cpu_2050.CH);
        }
    }

#if 0
uint8_t     D1[4];              /* Channel flag D1 */  /* Not used? */
uint8_t     D2[4];              /* Channel flag D2 */  /* Indicates TICH */
uint8_t     C1[4];              /* Channel flag C1 */  /* Indicates C_FULL with CD */
uint8_t     C2[4];              /* Channel flag C2 */  /* Indicates Interface check during selection */
uint8_t     C3[4];              /* Channel flag C3 */  /* Indicates PCI or status only interrupt */
uint8_t     C4[4];              /* Channel flag C4 */  /* Not used */
uint8_t     CD[4];              /* Data chain flag */
uint8_t     IF_STOP[4];         /* Stop I/O after count exhausted */
uint16_t    CHCLK[4];           /* Channel clock register */
#endif

    /* Process Selector channels */
   if (cpu_2050.last_cycle || cpu_2050.break_out == 0) {
         for (i = 0; i < 3; i++) {
             int    mask = 1 << (i);
             if (cpu_2050.CHPOS[i] != 0 || cpu_2050.break_out || (cpu_2050.break_in & mask) != 0) {
                 continue;
             }
             /* CHPOS
              *    BIT 0 - .STIO/.CCW1
              *    BIT 2 - Unit Select.
              *    BIT 3 - .RST0
              *    BIT 4 - .WFCH
              *    BIT 5 - .ENDU
              *    BIT 6 - .COMP
              *    BIT 7 - .IRPT
              *    100   - .UAFT
              */

             switch(cpu_2050.CHREQ[i]) {
             case BIT0:   /* Empty write fetch */
                      if (cpu_2050.OP_REG[i] == 0x1) {
                          cpu_2050.ROUTINE[i] = 0x20;  /* .WFCH qv104 */
                          cpu_2050.CHPOS[i] = BIT4;
                      } else {
                          cpu_2050.ROUTINE[i] = 0x24;  /* .RST0 qv105 */
                          cpu_2050.CHPOS[i] = BIT3;
                          cpu_2050.IOSTAT[i] = 0;
                          if (cpu_2050.LS_FULL[i]) {
                              cpu_2050.IOSTAT[i] |= BIT3;
                          }
                          if ((cpu_2050.OP_REG[i] & 0x2) != 0) {
                              cpu_2050.IOSTAT[i] |= BIT1;
                          }
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_SKIP_FLAG << 8)) != 0) {
                              cpu_2050.IOSTAT[i] |= BIT2;
                          }
                      }
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;
             case BIT1:  /* Interrupt routine */
                      cpu_2050.ROUTINE[i] = 0x04;   /* .IRPT qv107 */
                      cpu_2050.CHPOS[i] = BIT7;
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;
             case BIT2:  /* UA Fetch */
                      cpu_2050.ROUTINE[i] = 0x08;   /* .UAFT qv110 */
                      cpu_2050.CHPOS[i] = 0x100;
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;
             case BIT3:  /* END Routine */
//                          if (cpu_2050.D1[i]) {
 //                         cpu_2050.ROUTINE[i] = 0x2c;   /* .TICH qv101 */
  //                        } else {
                      cpu_2050.ROUTINE[i] = 0x0c;    /* .ENDU  qv106 */
   //                   }
                      cpu_2050.CHPOS[i] = BIT5;
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;
             case BIT4:  /* SIO Routine */
                      if (cpu_2050.inst_latch && i == (((cpu_2050.L_REG >> 8) -1) & 0x3)) {
                          cpu_2050.ROUTINE[i] = 0x10;   /* .STIO qv100 */
                          cpu_2050.FLAGS_REG[i] = 0;
                      } else {
                          if (cpu_2050.D2[i]) {
                          cpu_2050.ROUTINE[i] = 0x2c;   /* .TICH qv101 */
                          } else {
                          cpu_2050.ROUTINE[i] = 0x28;   /* .CCW1 qv102 */
                          }
                      }
                      cpu_2050.CHPOS[i] = BIT0;
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;
             case BIT5:  /* Compare Routine */
                      cpu_2050.ROUTINE[i] = 0x14;    /* .COMP qv111 */
                      cpu_2050.CHPOS[i] = BIT6;
                      cpu_2050.CHCLK[i] = 0;
                      cpu_2050.break_in |= mask;
                      break;

             case BIT2|BIT4:  /* Unit select */
                      cpu_2050.CHPOS[i] = BIT2;
                      cpu_2050.CHCLK[i] = 0;
                      break;
             }
             cpu_2050.CHREQ[i] = 0;
        }
    }


    /* Flags register holds CCW flags in upper byte, check flags in lower byte */
    for (i = 0; i < 3; i++) {
        int   ch = i + 1;
               int   inst = cpu_2050.inst_latch & (cpu_2050.CH == i);
        cpu_2050.TAGS_IN[ch] &= IN_TAGS;
        cpu_2050.TAGS_IN[ch] |= cpu_2050.TAGS[ch];
        for (dev = chan[ch]; dev != NULL; dev = dev->next) {
             dev->bus_func(dev, &cpu_2050.TAGS_IN[ch], cpu_2050.BUS_OUT[ch], &cpu_2050.BUS_IN[ch]);
        }
        if (cpu_2050.CHPOS[i] != 0) {
            if (cpu_2050.CHPOS[i] == 0x200 && cpu_2050.last_cycle) {
                   cpu_2050.CHPOS[i] = 0;
            }
            if (cpu_2050.CHPOS[i] == 0x400 && cpu_2050.last_cycle) {
                   cpu_2050.CHCLK[i] = 0;
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.B_REG[i] = cpu_2050.aob_latch;
                   cpu_2050.LS_FULL[i] = 0;
                   cpu_2050.B_FULL[i] = 1;
            }
            /* UA Fetch */
            if (cpu_2050.CHPOS[i] == 0x100) {
               log_device("UA Fetch\n");
               if (dtc2) {
                   log_device("DTC2\n");
                   dtc2 = 0;
                   cpu_2050.C_REG[i] = cpu_2050.aob_latch;
                   cpu_2050.B_REG[i] = 0;
                   cpu_2050.CHREQ[i] = BIT2|BIT4;
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.TAGS[ch] |= CHAN_OPR_OUT;
               }
            }
            /* CCW1 */
            if (cpu_2050.CHPOS[i] == BIT0) {
               log_device("CCW1 D2=%d\n", cpu_2050.D2[i]);
               /* On entry IOS1 is SMS flag */
               /* D2 indicates last operation was TIC */
               if (dtc2) {
                   int   cmd;
                   log_selchn("DTC2 %x\n", cpu_2050.CHCLK[i]);
                   dtc2 = 0;
                   switch(cpu_2050.CHCLK[i]) {
                   case 0:
                           cpu_2050.CHCLK[i] = 1;
                           cpu_2050.TAGS[ch] |= CHAN_OPR_OUT;
                           /* Check for Data chaining */
                           cpu_2050.B_REG[i] = cpu_2050.aob_latch;
                           cpu_2050.C1[i] = 0;
                           /* Clear CCW flags, keep PCI, and check flags */
                           cpu_2050.FLAGS_REG[i] &= 0x08ff;   /* Copy PCI flag */
            //               cpu_2050.D2[i] = 0;
                           break;
                   case 1:
                           cpu_2050.CHCLK[i] = 2;
                           /* Check for Data chaining */
                           cpu_2050.C_REG[i] = cpu_2050.B_REG[i];
                           cpu_2050.B_REG[i] = cpu_2050.aob_latch;
                           cmd = (cpu_2050.aob_latch >> 24) & 0xf;
                           cpu_2050.IOSTAT[i] = 0;
#if 0
                           if (cpu_2050.D1[i]) {  /* From TIC, request CCW1 again */
                              cpu_2050.IOSTAT[i] = 0;
                              cpu_2050.D1[i] = 0;
                              cpu_2050.CHREQ[i] = BIT3;
//                              cpu_2050.ROUTINE[i] = 0x2c;  /* .TICH qv101 */
                   //           cpu_2050.CHPOS[i] = BIT1;
                              cpu_2050.CHCLK[i] = 0;
 //                            cpu_2050.break_in |= 1<<i;
                              cpu_2050.CHPOS[i] = 0;
                              break;
                           }
#endif

                           if (cpu_2050.CD[i] == 0) {
                               if (cmd == 0x0) {
                               /* Set program check */
                                   cpu_2050.FLAGS_REG[i] |= CHAN_PROG; /* Program check */
       //                            cpu_2050.ROUTINE[i] = 0x04;  /* .IRPT qv004 */
      //                             cpu_2050.IOSTAT[i] |= BIT0;
     //                              cpu_2050.CHREQ[i] = BIT1;
    //                               cpu_2050.C_REG[i] &= 0xffffff;
   //                                cpu_2050.C3[i] = 0;
  //                                 cpu_2050.CHPOS[i] = 0;
 //                                  break;
                                }
                                /* Stop channel */
//                               } else {
                           //        cpu_2050.ROUTINE[i] = 0x30;  /* .    CCW2 qv103 */
                            //       cpu_2050.CHPOS[i] = BIT1;
                             //      cpu_2050.CHCLK[i] = 0;
                              //     cpu_2050.break_in |= 1<<i;
                              //}
//                              cpu_2050.D1[i] = 0;
                           } else {
                               if (cmd != CMD_TIC) {
                                   cmd = 0;
                               }
                           }

                           cpu_2050.CCI_REG[i] = 0;
                           if (cmd == CMD_TIC) {
                           /* Check for TIC */
                           /* If instruction or D2 set, set program check */
                           if (inst) {
                                   cpu_2050.FLAGS_REG[i] |= CHAN_PROG; /* Program check */
                                   cpu_2050.CHPOS[i] = 0;
//                                   cpu_2050.C4[i] = 1;
                                   cpu_2050.C_REG[i] &= 0xffffff;
                                   cpu_2050.CHREQ[i] = BIT3;      /* Request end */
                                   cpu_2050.IOSTAT[i] = BIT0;
  //                         cpu_2050.ROUTINE[i] = 0x30;  /* .CCW2 qv103 */
   //                        cpu_2050.CHPOS[i] = BIT1;
    //                       cpu_2050.CHCLK[i] = 0;
     //                      cpu_2050.break_in |= 1<<i;
//                                   cpu_2050.IOSTAT[i] |= BIT2;
 //                                  cpu_2050.D1[i] = 1;
////                                   cpu_2050.C1[i] = 1;
// //                                  cpu_2050.CHREQ[i] = BIT3;
    //                               cpu_2050.ROUTINE[i] = 0x2c;  /* .TICH qv101 */
     //                              cpu_2050.CHPOS[i] = BIT5;
      //                             cpu_2050.CHCLK[i] = 2;
 //                                  cpu_2050.ROUTINE[i] = 0x04;  /* .IRPT qv004 */
  //                                cpu_2050.break_in |= 1<<i;
  //    //                             cpu_2050.CHPOS[i] = 0;
                               break;
                               }
                               if (cpu_2050.D2[i]) {
                                   cpu_2050.FLAGS_REG[i] |= CHAN_PROG; /* Program check */
                               cpu_2050.D2[i] = 0;
                               } else {

                               cpu_2050.IOSTAT[i] |= BIT2;
                               cpu_2050.D2[i] = 1;
                               }
                               break;
                           }
                           /* Check for Data chaining */
                           cpu_2050.D2[i] = 0;
                           if (cpu_2050.CD[i] == 0) {
                   log_selchn("Set OP %x\n", cpu_2050.GP_REG[i]);
                           cpu_2050.GP_REG[i] = (cmd & 1) | ((!(cmd & 1)) << 2);
                           if (cmd == CMD_RDBWD) {
                               cpu_2050.GP_REG[i] |= 0x2;
                               cpu_2050.IOSTAT[i] |= BIT1;
                           }

                               cpu_2050.OP_REG[i] = cpu_2050.GP_REG[i] & 07;
                           }
                           cpu_2050.GP_REG[i] &= ~(3 << 5);
                           cpu_2050.GP_REG[i] |= (cpu_2050.aob_latch & 03) << 5;
                           break;
                   case 2: /* Error on startup. */
                           cpu_2050.FLAGS_REG[i] |= CHAN_PROG;
                           cpu_2050.CHCLK[i] = 0;
                           cpu_2050.CHPOS[i] = 0;
                          cpu_2050.BCHI |= 1 << i;    /* Request interrupt */
                           break;
                   }
               }
               if (cpu_2050.last_cycle) {
                           cpu_2050.ROUTINE[i] = 0x30;  /* .CCW2 qv103 */
                           cpu_2050.CHPOS[i] = BIT1;
                           cpu_2050.CHCLK[i] = 0;
                           cpu_2050.break_in |= 1<<i;
               }
            }
            /* CCW2 */
            /* L = CA+8, R=CA, M=op,DA */
            /* 0 CA, 1 IRV, 2 Op DA, 3 ?? */
            if (cpu_2050.CHPOS[i] == BIT1) {
               log_device("CCW2 D2=%d\n", cpu_2050.D2[i]);
               /* IOSTAT 2 = TIC
                  IOSTAT 1 = BKWD */
               if (dtc2) {
                   log_selchn("DTC2 %x\n", cpu_2050.CHCLK[i]);
                   dtc2 = 0;
                   switch(cpu_2050.CHCLK[i]) {
                   case 0:
                           cpu_2050.CHCLK[i] = 1;
//                           if (cpu_2050.D1[i]) {
 //                              cpu_2050.CHREQ[i] = BIT3;    /* Request end */
  //                                 cpu_2050.C3[i] = 0;
   //                        }
                           break;
                   case 1:
                           cpu_2050.CHCLK[i] = 2;
//                           cpu_2050.D1[i] = 0;
                  //         if (cpu_2050.D2[i] == 0) {
                           /* Insert flags from new CCW */
                           cpu_2050.FLAGS_REG[i] &= 0xff | (CHAN_PCI_FLAG << 8);;
                           cpu_2050.FLAGS_REG[i] |= (cpu_2050.aob_latch >> 16) & 0xf800;
                   //        }
                           /* Check if PCI */
//                           if (cpu_2050.FLAGS_REG[i] & 0x800) {
 //                              cpu_2050.FLAGS_REG[i] |= BIT0;
  //                         }

                           cpu_2050.GP_REG[i] &= ~(BIT3|BIT4|BIT5|BIT6|BIT7);
                           break;
                   case 2:
                           cpu_2050.CHCLK[i] = 0;
                           cpu_2050.CHPOS[i] = 0;
                           cpu_2050.EOR[i] = 0;
                           if ((cpu_2050.FLAGS_REG[i] & 0x7f) != 0) { /* Any errors request interrupt */
                                   log_selchn("Channel Error\n");
                                   cpu_2050.CHREQ[i] = BIT3;      /* Request End */
                                   cpu_2050.IOSTAT[i] |= BIT4;
                               break;
                           }
                           if (cpu_2050.CD[i] == 0 || cpu_2050.D2[i]) {  /* Not data chaining, request TIC or Unit select */
                               cpu_2050.C_FULL[i] = 0;
                               if (cpu_2050.D2[i]) {  /* TIC */
                               cpu_2050.CHREQ[i] = BIT4;
                               } else {
                               cpu_2050.CHREQ[i] = BIT2|BIT4; /* Unit select */
                           cpu_2050.D2[i] = 0;
                               }
                           }

                           /* If writing less then 4 bytes, set GP bits. */
                           if (cpu_2050.OP_REG[i] == 1) {
                               int byte = (cpu_2050.aob_latch & 0xffff) + ((cpu_2050.GP_REG[i] >> 5) & 3);
                               if (byte < 4) {
                                   cpu_2050.GP_REG[i] &= 0x67;
                                   cpu_2050.GP_REG[i] |= (byte + 1) & 3;
//                                   cpu_2050.EOR[i] = 1;
                               }
                           /* Clear Last word flags, on Write */
                           cpu_2050.GP_REG[i] &= ~(BIT5|BIT6|BIT7);
                           }

                           /* Don't update CD flag on TIC command */
                           if (cpu_2050.D2[i] == 0) { /* Not TIC Command */
                           cpu_2050.CD[i] = 0;
                           /* Set Chain data flag */
                           if (cpu_2050.FLAGS_REG[i] & (CHAN_CD_FLAG << 8)) {
                               cpu_2050.CD[i] = 1;
                           }
                           }
                           cpu_2050.IF_STOP[i] = 0;
                           cpu_2050.TAGS[ch] |= CHAN_OPR_OUT;
                           cpu_2050.C1[i] = 0;
                           cpu_2050.EOR[i] = 0;
                           if ((cpu_2050.FLAGS_REG[i] & 0x80) != 0) { /* PCI Request? */
                                   log_selchn("Channel PCI\n");
                                   cpu_2050.CHREQ[i] = BIT3;      /* Request End */
                                   cpu_2050.IOSTAT[i] |= BIT4;
                               break;
                           }
                           break;
                   }
                }
            }
            /* Unit Select */
            if (cpu_2050.CHPOS[i] == BIT2) {
               log_device("Unit Sel %d\n", i);
               switch(cpu_2050.CHCLK[i]) {
               case 0:  /* Start selection processing */
//                      if (cpu_2050.inst_latch && cpu_2050.CHCTL == 2 && cpu_2050.polling[i] == 0) {
 //                         /* Device active and HIO, drop Select out */
  //                        log_selchn("HIO active\n");
   //                       cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT); /* Drop Select Out. */
    //                      cpu_2050.CHCLK[i] = 1;
     //                     cpu_2050.FLAGS_REG[i] &= 0xff;
      //                    break;
       //               }
                       /* Raise adddress out */
//                       if ((cpu_2050.TAGS[ch] & CHAN_ADR_OUT) == 0) {
                           cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                           cpu_2050.TAGS[ch] |= CHAN_ADR_OUT;
                           cpu_2050.BUS_OUT[ch] = (cpu_2050.C_REG[i] >> 24) & 0xff;
                           cpu_2050.BUS_OUT[ch] |= odd_parity[cpu_2050.BUS_OUT[ch]];
                           cpu_2050.CHCLK[i] = 1;
                           cpu_2050.polling[ch] = 0;
 //                      }
                       break;
                case 1:
                      /* If Halting, raise Address out */
        //              if (cpu_2050.inst_latch && cpu_2050.CHCTL == 2 && cpu_2050.polling[i] == 0) {
         //                 log_selchn("HIO active\n");
          //                cpu_2050.TAGS[ch] |= (CHAN_ADR_OUT); /* Raise address out */
           //               cpu_2050.CHCLK[i] = 2;
            //              break;
             //         }
                      /* Else start selection process */
                           cpu_2050.TAGS[ch] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                           cpu_2050.CHCLK[i] = 2;
                       break;
                case 2:
                      /* Halting, wait for device to stop */
  //                    if (cpu_2050.inst_latch && cpu_2050.CHCTL == 2 && cpu_2050.polling[i] == 0) {
   //                       log_selchn("Wait Operin drop\n");
    //                      if ((cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) == 0) {
     //                         cpu_2050.TAGS[ch] &= ~(CHAN_ADR_OUT); /* Drop address out */
      //                         cpu_2050.CHCLK[i] = 0;
       //                cpu_2050.CHPOS[i] = 0;
        //                      cpu_2050.polling[ch] = 1;  /* Back to polling */
         //                     cpu_2050.CC = 1;
          //                    cpu_2050.CHCTL = 0;
           //                   cpu_2050.inst_latch = 0;
            //                  cpu_2050.S_REG |= BIT3;
             //             }
              //            break;
               //       }
                       /* See if nothing responded */
                       if ((cpu_2050.TAGS_IN[ch] & CHAN_SEL_IN) != 0) {
                           log_device("No device\n");
                           if (inst && cpu_2050.CHCTL != 0) {
                               cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                               cpu_2050.BUS_OUT[ch] = 0x100;
                               cpu_2050.CC = 3;
                               cpu_2050.polling[ch] = 1;
                               cpu_2050.CHPOS[i] = 0;
                               cpu_2050.CHCLK[i] = 0;
                               cpu_2050.CHCTL = 0;
                               cpu_2050.inst_latch = 0;
                               cpu_2050.S_REG |= BIT3;
                               break;
                           }
                       }
                       /* Device raised, Status In, drop address out */
                       if ((cpu_2050.TAGS_IN[ch] & (CHAN_ADR_OUT|CHAN_STA_IN)) == (CHAN_ADR_OUT|CHAN_STA_IN)) {
                           log_device("Quick Busy\n");
                               cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT);
                               cpu_2050.CHPOS[i] = 0;
                               cpu_2050.CHCLK[i] = 0;
                              if (inst) {
                               cpu_2050.CC = 2;
                               cpu_2050.polling[ch] = 1;
                               cpu_2050.CHCTL = 0;
                               cpu_2050.inst_latch = 0;
                               cpu_2050.S_REG |= BIT3;
                              } else {
                               cpu_2050.CHREQ[i] = BIT3;   /* Request End processing */
                               cpu_2050.IF_STOP[i] = 1;
                               cpu_2050.IOSTAT[i] = BIT4;  /* Status only */
                              }
                               break;
                       }
                       /* Device raised, Oper In, drop address out and wait for address in */
                       if ((cpu_2050.TAGS_IN[ch] & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_OPR_IN)) {
                           log_device("Selected\n");
                           cpu_2050.TAGS[ch] &= ~(CHAN_ADR_OUT|CHAN_SUP_OUT);
                       }
                       cpu_2050.CHCLK[i] = 3;
                       /* Address in, compare, and continue along */
                       if ((cpu_2050.TAGS_IN[ch] & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                           uint16_t    adr = (cpu_2050.C_REG[i] >> 24);
                        if (((cpu_2050.BUS_IN[ch] ^ odd_parity[cpu_2050.BUS_IN[ch] & 0xff]) & 0x100) != 0) {
                            cpu_2050.FLAGS_REG[i] |= CHAN_INTER; /* Interface check */
                                   cpu_2050.C2[i] = 1;
                        }
                           log_device("Address Compare %03x %03x\n", cpu_2050.BUS_IN[ch], adr);
                           cpu_2050.BUS_OUT[ch] = 0x100;
                           if ((cpu_2050.BUS_IN[ch] & 0xff) == adr) {
                               cpu_2050.C2[i] = 0;
                           } else {
                               cpu_2050.FLAGS_REG[i] |= CHAN_INTER;  /* Interface check */
    //                           if (cpu_2050.inst_latch && cpu_2050.CHCTL == 4) {
                                   cpu_2050.C2[i] = 1;
     //                          }
                           }
                       }
                       break;
                case 3:
                       /* If we didn't find device, clear select out and wait for device to disappear */
                       if (cpu_2050.C2[i]) {
                           log_device("Done %d\n");
                              cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                               cpu_2050.CHPOS[i] = 0;
                               cpu_2050.CHCLK[i] = 0;
                              if (inst) {
                               cpu_2050.CC = 3;
                               cpu_2050.polling[ch] = 1;
                               cpu_2050.CHCTL = 0;
                               cpu_2050.inst_latch = 0;
                               cpu_2050.S_REG |= BIT3;
                              } else {
                               cpu_2050.IOSTAT[i] = BIT4;  /* Status only */
                          cpu_2050.BCHI |= 1 << i;    /* Request interrupt */
                              }
                           break;
                       }
                       /* TIO, command is 0 */
                       if (inst && cpu_2050.CHCTL == 4) {
                           cpu_2050.BUS_OUT[ch] = 0x100;
                           /* HIO */
                       } else if (inst && cpu_2050.CHCTL == 2) {
                           /* Wait for device to drop Address in */
                           if ((cpu_2050.TAGS_IN[ch] & CHAN_ADR_IN) != 0) {
                               /* Raise Address out and drop Select out */
                               log_device("Raise Address out\n");
                                   cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                               cpu_2050.TAGS[ch] |= CHAN_ADR_OUT;
                               cpu_2050.CHCLK[i] = 6; /* Go wait for Oper in to drop */
                            }
                             break;
                       } else {
                           /* Put command on bus */
                           cpu_2050.C_REG[i] = cpu_2050.B_REG[i];
                           cpu_2050.BUS_OUT[ch] = (cpu_2050.C_REG[i] >> 24) & 0xff;
                           cpu_2050.BUS_OUT[ch] |= odd_parity[cpu_2050.BUS_OUT[ch]];
                           log_device("Command out %02x %d\n", (cpu_2050.C_REG[i] >> 24) & 0xff, cpu_2050.C2[i]);
                       }
                       /* Raise command out, and wait for Status in */
                       if ((cpu_2050.TAGS_IN[ch] & CHAN_ADR_IN) != 0) {
                           log_device("Raise CMD\n");
                           cpu_2050.TAGS[ch] |= CHAN_CMD_OUT;
                       }
                       if ((cpu_2050.TAGS_IN[ch] & CHAN_ADR_IN) == 0) {
                           log_device("Drop CMD\n");
                           cpu_2050.TAGS[ch] &= ~CHAN_CMD_OUT;
                       }
                       if ((cpu_2050.TAGS_IN[ch] & CHAN_STA_IN) != 0) {
                       /* If command chain, request new CCW */
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
                                 /* Tell device this is chain */
                               cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
                           }
//                           cpu_2050.C3[i] = 1;
                           cpu_2050.CHCLK[i] = 4;
                           log_device("Status IN\n");
                       }
                       break;
               case 4:
                       /* Accept the status */
                       cpu_2050.C_REG[i] &= 0xffffff;
                       cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << 24;
                       cpu_2050.CHCLK[i] = 5;
                               cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                        if (((cpu_2050.BUS_IN[ch] ^ odd_parity[cpu_2050.BUS_IN[ch] & 0xff]) & 0x100) != 0) {
                            cpu_2050.FLAGS_REG[i] |= CHAN_INTER; /* Interface check */
                           cpu_2050.CHCLK[i] = 5;
                        }

                       break;
               case 5:
                       /* Wait for status in to drop */
                       if ((cpu_2050.TAGS_IN[ch] & CHAN_STA_IN) != 0) {
                           break;
                       }
                               cpu_2050.TAGS[ch] &= ~CHAN_SRV_OUT;
//                          if ((cpu_2050.FLAGS_REG[i] & 0x4000) != 0) {
 //                          cpu_2050.TAGS[ch] &= ~(CHAN_SRV_OUT|CHAN_HLD_OUT);
  //                       } else {
   //                        cpu_2050.TAGS[ch] &= ~(CHAN_SRV_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT);
    //                    }
                       cpu_2050.C_FULL[i] = 0;
                       cpu_2050.B_FULL[i] = 0;
                       cpu_2050.LS_FULL[i] = 0;
                       cpu_2050.CHPOS[i] = 0;
                       cpu_2050.CHCLK[i] = 0;
                       log_selchn("End initial %d %02x\n", cpu_2050.CHCTL, (cpu_2050.C_REG[i] >> 24) & 0xff);
//                       if (cpu_2050.inst_latch && cpu_2050.CHCTL == 1 && ((cpu_2050.C_REG[i] >> 24) & 0xff) != 0) {
 //                          log_selchn("Error\n");
  //                             cpu_2050.CHREQ[i] |= BIT1;
   //                            cpu_2050.CC = 1;
    //                       break;
     //                 }
                      /* TIO */
                      if (inst && cpu_2050.CHCTL == 4) {
                          log_selchn("initial TIO status\n");
                          cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                          cpu_2050.CHCLK[i] = 0;
                          cpu_2050.CHCTL = 0;
                      if ((cpu_2050.C_REG[i] & ((uint32_t)0xff << 24)) != 0) {
                              cpu_2050.CHREQ[i] = BIT3;   /* Request end processing */
                          cpu_2050.IOSTAT[i] = BIT4;  /* Status only interrupt */
                               cpu_2050.CC = 1;
                       } else {
                              //cpu_2050.IOSTAT[i] = BIT4;
                          cpu_2050.polling[ch] = 1;
                          cpu_2050.CC = 0;
                          cpu_2050.inst_latch = 0;
                          cpu_2050.S_REG |= BIT3;
                       }
                          break;
                      }
                      /* HIO */
//                      if (cpu_2050.inst_latch && cpu_2050.CHCTL == 2) {
 //                         log_selchn("initial HIO status\n");
  //                        cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
   //                       cpu_2050.TAGS[ch] |= (CHAN_ADR_OUT);
    //                      cpu_2050.polling[ch] = 1;
     //                     cpu_2050.CC = 1;
      //                    cpu_2050.CHCTL = 0;
       //                   cpu_2050.inst_latch = 0;
        //                  cpu_2050.S_REG |= BIT3;
         //                 break;
          //            }
                      /* SIO */
//                      if (cpu_2050.inst_latch && cpu_2050.CHCTL == 1) {
 //                          log_selchn("End SIO\n");
                      /* If any error bits, quit chain. */
  //                    if ((cpu_2050.C_REG[i] & 0xb3000000) != 0) {
   //                       cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
    //                           cpu_2050.CHREQ[i] = BIT3;   /* Request end processing */
     //                              cpu_2050.C3[i] = 0;
      //                         cpu_2050.IF_STOP[i] = 1;
       //                        cpu_2050.IOSTAT[i] = BIT4;
        //                       cpu_2050.CC = 1;
         //                 break;
          //            }
           //           }
                      /* Device or channel end, no chaining */
#if 0
                           if ((cpu_2050.C_REG[i] & ((SNS_DEVEND|SNS_CHNEND) << 24)) != 0 &&
                               (cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) == 0) {

                          cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                               cpu_2050.CHREQ[i] = BIT3;   /* Request End processing */
                                   cpu_2050.C3[i] = 0;
                               cpu_2050.IF_STOP[i] = 1;
                               cpu_2050.IOSTAT[i] = BIT4;
                               /* Set Suppress to prevent others from interrupting */
                               cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
//                               cpu_2050.CC = 1;
//                               cpu_2050.BCHI |= 1 << i;
 //                              cpu_2050.IF_STOP[i] = 1;
 //                              cpu_2050.polling[ch] = 0;
                           break;
                           }
#endif
                      log_selchn("initial status\n");
                      /* If any error bits, quit chain. */
                      if ((cpu_2050.C_REG[i] & ((uint32_t)0xb3 << 24)) != 0) {
                          cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                      if (inst) {
                           log_selchn("End SIO error\n");
                      /* If any error bits, quit chain. */
//                      if ((cpu_2050.C_REG[i] & 0xb3000000) != 0) {
                               cpu_2050.CHREQ[i] = BIT3;   /* Request end processing */
                                   cpu_2050.C3[i] = 0;
                               cpu_2050.CC = 1;
                        } else {
                          cpu_2050.BCHI |= 1 << i;    /* Request interrupt */
                        }
                               cpu_2050.CHCLK[i] = 0;
                          cpu_2050.IF_STOP[i] = 1;
                          cpu_2050.polling[ch] = 0;
//                          cpu_2050.IOSTAT[i] = BIT4;
                          break;
                      }
                      /* Device end */
                      if ((cpu_2050.C_REG[i] & (SNS_DEVEND << 24)) != 0) {
                          log_selchn("Device end\n");
                          cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
                              /* If command chain, request new CCW */
                              if ((cpu_2050.C_REG[i] & (SNS_SMS << 24)) != 0) {
                                 cpu_2050.IOSTAT[i] |= BIT1;
                              }
                              cpu_2050.CHREQ[i] = BIT4;    /* Request new CCW */
                              cpu_2050.IF_STOP[i] = 1;
                          } else {
                              /* If not command chain, request interrupt */
//                          if ((cpu_2050.FLAGS_REG[i] & 0x4000) != 0) {
 //                          cpu_2050.TAGS[ch] &= ~(CHAN_SRV_OUT|CHAN_HLD_OUT);
  //                       } else {
                               /* Set Suppress to prevent others from interrupting */
//                               cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
 //                       }
                                   cpu_2050.C3[i] = 0;
                              cpu_2050.CHREQ[i] = BIT3;   /* Request end processing */
                              //cpu_2050.IOSTAT[i] = BIT4;
                              cpu_2050.C1[i] = 1;
                              break;
                          }
                       /* Release CPU if done starting command */
                      if (inst) {
                          cpu_2050.CC = 0;
                          cpu_2050.CHCTL = 0;
                          cpu_2050.inst_latch = 0;
                          cpu_2050.S_REG |= BIT3;
                       }
                          break;
                      }
                      /* Channel end */
                      if ((cpu_2050.C_REG[i] & (SNS_CHNEND << 24)) != 0) {
                          log_selchn("Channel end\n");
                          /* If channel end and not chaining, process results */
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) == 0) {
                              cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
//                              if ((cpu_2050.C_REG[i] & (SNS_SMS << 24)) != 0) {
 //                                cpu_2050.IOSTAT[i] |= BIT1;
  //                            }
//                              cpu_2050.CHREQ[i] = BIT3;
 //                             cpu_2050.polling[ch] = 1;
   //                       } else {
                           /* Command chaining, put Suppress out to tell device */
//                           if ((cpu_2050.FLAGS_REG[i] & 0x4000) != 0) {
 //                              cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
  //                         }
                                   cpu_2050.C2[i] = 0;
                              cpu_2050.CHREQ[i] = BIT3;   /* Request end processing */
                              cpu_2050.IOSTAT[i] = BIT4;
                          break;
                          }
                      }
                      /* Otherwise continue with transfer */
                      log_selchn("Continue\n");
                      cpu_2050.TAGS[ch] &= ~(CHAN_SUP_OUT);
                          cpu_2050.polling[ch] = 0;
                      if (inst) {
                          cpu_2050.CC = 0;
                          cpu_2050.CHCTL = 0;
                          cpu_2050.inst_latch = 0;
                          cpu_2050.S_REG |= BIT3;
                          break;
                      }
                      break;
               case 6:  /* HIO */
                       /* Wait for Status in to drop */
                       if ((cpu_2050.TAGS_IN[ch] & (CHAN_OPR_IN)) != 0) {
                       cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                           break;
                       }
                       cpu_2050.CHPOS[i] = 0;
                       cpu_2050.CHCLK[i] = 0;
                          cpu_2050.TAGS[ch] &= ~(CHAN_ADR_OUT);
                          cpu_2050.polling[ch] = 1;
                          cpu_2050.inst_latch = 0;
                          cpu_2050.CC = 1;
                          cpu_2050.S_REG |= BIT3;
                       break;
                  }
            }
            /* Read store */
            if (cpu_2050.CHPOS[i] == BIT3) {
               log_device("Read store %d\n", cpu_2050.last_cycle);
               /* IOSTAT 3 = LS loaded
                  IOSTAT 2 = SKIP
                  IOSTAT 1 = BKWD */
               if (cpu_2050.last_cycle) {
                   log_trace("Last cycle\n");
                   cpu_2050.CHCLK[i] = 0;
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.LS_FULL[i] = 0;
                   cpu_2050.IOSTAT[i] &= ~BIT3;
                   if ((cpu_2050.FLAGS_REG[i] & (CHAN_SKIP_FLAG << 8)) != 0) {
                       cpu_2050.IOSTAT[i] |= BIT2;
                   }
                   if (cpu_2050.C1[i]) {
                      cpu_2050.CHREQ[i] = BIT4;
                      cpu_2050.C1[i] = 0;
                   }
               }
            }
            /* Write Fetch */
            if (cpu_2050.CHPOS[i] == BIT4) {
               log_device("Write fetch\n");
               if (dtc2) {
                   switch(cpu_2050.CHCLK[i]) {
                   case 0:
                        cpu_2050.CHCLK[i] = 1;
                            cpu_2050.EOR[i] = 0;
                        break;
                   case 1:
//                        if (//(cpu_2050.aob_latch & 0xffff) == 0 ||
 //                           (cpu_2050.aob_latch & 0x10000) != 0) {
  //                          cpu_2050.EOR[i] = 1;
   //                     }
                        cpu_2050.CHCLK[i] = 2;
                        break;
                   case 2:
                        if ( cpu_2050.B_FULL[i] == 0) {
                            cpu_2050.B_REG[cpu_2050.CH] = cpu_2050.aob_latch;
                            cpu_2050.B_FULL[cpu_2050.CH] = 1;
                        } else {
                            cpu_2050.LS_FULL[i] = 1;
                        }

                        break;
                   }
               }
               if (cpu_2050.last_cycle) {
                   log_trace("Last cycle\n");
                   cpu_2050.CHCLK[i] = 0;
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.IOSTAT[i] &= ~BIT3;
               }
            }
            /* End */
            if (cpu_2050.CHPOS[i] == BIT5) {
               log_device("End %d %d\n", cpu_2050.D1[i], cpu_2050.C3[i]);
               /* IOSTAT 4 = Error or comm chain
                  IOSTAT 3 = Not 1 reg full and not 3 regs full
                  IOSTAT 2 = Not 2 regs full and not 1 reg ful
                  IOSTAT 1 = WR CHAIN DATA AND NEW CCW NOT USED,
                  C3 indicates PCI */
               if (dtc1) {
                   log_device("DTC1\n");
                   dtc1 = 0;
                   switch(cpu_2050.CHCLK[i]) {
                   case 0:
                          cpu_2050.CHCLK[i] = 1;
//                          if ((cpu_2050.FLAGS_REG[i] & 0x00ff) != 0) {
 //                                 break;
  //                        }
//                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
 //               cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
  //                }
//                          cpu_2050.C4[i] = 0;
 //                         if ((cpu_2050.IOSTAT[i] & BIT4) != 0) {
  //                             cpu_2050.C4[i] = 1;
   //                       }
                          break;
                   case 1:
                          cpu_2050.CHCLK[i] = 0;
                          cpu_2050.IOSTAT[i] = 0;
                          if ((cpu_2050.TAGS_IN[ch] & CHAN_STA_IN) != 0) {
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
                cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
                  }
                              cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                          }
//                          if ((cpu_2050.FLAGS_REG[i] & 0x00ff) != 0) {
 //                                 cpu_2050.BCHI |= 1 << i;
 //                             if (inst) {
//                                  cpu_2050.IOSTAT[i] = BIT4;
  //                                cpu_2050.CHREQ[i] = BIT1;  /* Request interrupt */
   //                           } else {
    //                              cpu_2050.BCHI |= 1 << i;
     //                         }
      //                            break;
       //                   }
                          if (cpu_2050.CHREQ[i] == 0 && (cpu_2050.FLAGS_REG[i] & 0xff) != 0) {
                                  cpu_2050.BCHI |= 1 << i;
                                   break;
                         }
                          /* Chain data, status only interrupt */
//                          if (cpu_2050.C4[i]) {
 //                         if ((cpu_2050.FLAGS_REG[i] & (CHAN_CD_FLAG << 8)) != 0) {
  //                            cpu_2050.C4[i] = 0;
   //                            break;
    //                      }
                          /* No command chain */
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) == 0) {
                              cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                              /* Request interrupt */
                              if (inst) {
                                  cpu_2050.CHREQ[i] = BIT1;  /* Request interrupt */
 //                                 cpu_2050.IOSTAT[i] = BIT0;
                              } else {
                                  cpu_2050.BCHI |= 1 << i;
                              }
                          } else {
                             /* Command chain */
                          if ((cpu_2050.TAGS_IN[ch] & CHAN_STA_IN) != 0) {
                                   cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                              if ((cpu_2050.C_REG[i] & (SNS_DEVEND << 24)) != 0) {
                                   if ((cpu_2050.C_REG[i] & (SNS_SMS << 24)) != 0) {
                                      cpu_2050.IOSTAT[i] |= BIT1;
                                   }
                                   cpu_2050.CHREQ[i] = BIT4;  /* Request CCW1 */
                              }
                              if (inst) {
                                  cpu_2050.CHREQ[i] = BIT1;  /* Request interrupt */
  //                                cpu_2050.IOSTAT[i] = BIT0;
                              }
                           }
                          }

                          break;
                   }
                }
                if (cpu_2050.last_cycle) {
                              cpu_2050.C1[i] = 0;
                              cpu_2050.C2[i] = 0;
//                              cpu_2050.C3[i] = 0;
                    cpu_2050.CHPOS[i] = 0;
                }
            }
            /* Compare */
            if (cpu_2050.CHPOS[i] == BIT6) {
               log_device("Compare\n");
               if (cpu_2050.first_cycle) {
                   cpu_2050.L_REG &= 0x00ffffff;
                   cpu_2050.L_REG |= cpu_2050.C_REG[i] & 0xff000000;
               }
               if (dtc2) {
                   if ((cpu_2050.aob_latch & 0x00800000) != 0) {
                       cpu_2050.C1[i] = 1;
                   } else {
                       cpu_2050.C1[i] = 0;
                   }
               }
               if (cpu_2050.last_cycle) {
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.TAGS[ch] |= CHAN_CMD_OUT;
               }
            }
            /* Interrupt routine */
            if (cpu_2050.CHPOS[i] == BIT7) {
               /* IOS0, IOS3
                     0   0    Normal interrupt.
                     1   0    Status Only Interrupt.
                     1   1    Poll Proceed.
                */
               log_device("Intrp %d %d\n", cpu_2050.D1[i], cpu_2050.C3[i]);
               if (dtc1) {
                   log_device("DTC1\n");
                   dtc1 = 0;
                   switch(cpu_2050.CHCLK[i]) {
                   case 0:
                          cpu_2050.CHCLK[i] = 1;
                          break;
                   case 1:
                          cpu_2050.CHCLK[i] = 2;
                          break;
                   case 2:
                          cpu_2050.CHCLK[i] = 0;
                          break;
                   }
                }
               if (cpu_2050.last_cycle) {
                   if ((cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) == 0) {
                   cpu_2050.polling[ch] = 1;
                   }
                   if (inst) {
                       cpu_2050.CC = 1;
                       cpu_2050.CHCTL = 0;
                       cpu_2050.inst_latch = 0;
                   cpu_2050.S_REG |= BIT3;
                   }
                   cpu_2050.C3[i] = 0;
                   cpu_2050.FLAGS_REG[i] &= ~(0x08ff);
                   cpu_2050.CHPOS[i] = 0;
                   cpu_2050.BCHI &= ~(1 << i);
               }
            }
          } else if (cpu_2050.polling[ch]) {  /* Handle polling mode */
            if ((cpu_2050.TAGS_IN[ch] & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
                cpu_2050.TAGS[ch] &= ~CHAN_SRV_OUT;
            }
               if ((cpu_2050.TAGS_IN[ch] & (CHAN_ADR_OUT|CHAN_OPR_IN)) == (CHAN_ADR_OUT)) {
                cpu_2050.TAGS[ch] &= ~CHAN_ADR_OUT;
               }
               if ((cpu_2050.TAGS_IN[ch] & CHAN_REQ_IN) != 0) {
                   cpu_2050.TAGS[ch] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                   log_trace("Request in\n");
               }
               if ((cpu_2050.TAGS_IN[ch] & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                   log_trace("Device select\n");
                   cpu_2050.CHREQ[i] = BIT5; /* Compare */
                   cpu_2050.C_REG[i] &= 0x00ffffff;
                   cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << 24;
               }
               if ((cpu_2050.TAGS_IN[ch] & (CHAN_STA_IN|CHAN_OPR_IN)) == (CHAN_STA_IN|CHAN_OPR_IN)) {
                   log_selchn("Post status\n");
                          if ((cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
                cpu_2050.TAGS[ch] |= CHAN_SUP_OUT;
                  }
                cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                   cpu_2050.TAGS[ch] &= ~CHAN_CMD_OUT;
                   cpu_2050.B_REG[i] = cpu_2050.C_REG[i];
                   cpu_2050.C_REG[i] &= 0x00ffffff;
                   cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << 24;
                   if (cpu_2050.C1[i] && (cpu_2050.BUS_IN[ch] & SNS_CHNEND) != 0) { /* For this device */

                       cpu_2050.IOSTAT[i] = 0;
                    if (cpu_2050.OP_REG[i] == 0x6) {
                        switch((cpu_2050.GP_REG[i] >> 3) & 3) {
                        case 0: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 1: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 2: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 3: cpu_2050.IOSTAT[i] |= 0; break;
                        }
                    } else {
                        switch((cpu_2050.GP_REG[i] >> 3) & 3) {
                        case 0: cpu_2050.IOSTAT[i] |= 0; break;
                        case 1: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 2: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 3: cpu_2050.IOSTAT[i] |= BIT3; break;
                        }
                    }
                       if ((cpu_2050.BUS_IN[ch] & 0xb3) != 0) {
                           log_selchn("Error status\n");
                           cpu_2050.IOSTAT[i] |= BIT4;
                       }
                                   cpu_2050.C3[i] = 0;
                       cpu_2050.CHREQ[i] = BIT3;    /* Request END */
                   } else {  /* Not this device, request status interrupt */
                       cpu_2050.IOSTAT[i] = BIT0;
                          cpu_2050.BCHI |= 1 << i;
                   }
                   cpu_2050.C1[i] = 0;
                               cpu_2050.C2[i] = 0;
               }
               if ((cpu_2050.TAGS_IN[ch] & (CHAN_SRV_IN|CHAN_OPR_IN)) == (CHAN_SRV_IN|CHAN_OPR_IN)) {
                   cpu_2050.TAGS[ch] &= ~(CHAN_CMD_OUT);
                   cpu_2050.C_REG[i] &= 0x00ffffff;
                   cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << 24;
                   if (cpu_2050.C1[i]) { /* For this device */
                          cpu_2050.polling[ch] = 0;
                   } else {  /* Not this device, request status interrupt */
                       cpu_2050.IOSTAT[i] = BIT0;
                          cpu_2050.BCHI |= 1 << i;
                   }
                   cpu_2050.C1[i] = 0;
               }
               if ((cpu_2050.TAGS_IN[ch] & (CHAN_HLD_OUT|CHAN_OPR_IN)) == (CHAN_HLD_OUT)) {
                           log_selchn("Drop select\n");
                   cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                   cpu_2050.C1[i] = 0;
               }
          } else {  /* Handle automatic channel functions */
            if ((cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) != 0 && cpu_2050.C_FULL[i] != 0 && cpu_2050.B_FULL[i] == 0 &&
                         (cpu_2050.OP_REG[i] & 0x6) != 0) {
                /* Read, Read Backword */
                log_device("Xfer C to B\n");
                cpu_2050.B_REG[i] = cpu_2050.C_REG[i];
                cpu_2050.CBI_REG[i] = cpu_2050.CCI_REG[i];
                cpu_2050.B_FULL[i] = 1;
                cpu_2050.C_FULL[i] = 0;
                cpu_2050.CCI_REG[i] = 0;
            }

            if ((cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) != 0 && cpu_2050.C_FULL[i] == 0 && cpu_2050.B_FULL[i] != 0 &&
                         (cpu_2050.OP_REG[i] & 0x7) == 1) {
                /* Write */
                log_device("Xfer B to C\n");
                cpu_2050.C_REG[i] = cpu_2050.B_REG[i];
                cpu_2050.CCI_REG[i] = cpu_2050.CBI_REG[i];
                cpu_2050.B_FULL[i] = 0;
                cpu_2050.C_FULL[i] = 1;
                cpu_2050.CBI_REG[i] = 0;
            }
            if (cpu_2050.break_out == 0 && cpu_2050.CHREQ[i] == 0 && cpu_2050.CHPOS[i] == 0) {

            if ((cpu_2050.OP_REG[i] & 0x6) != 0 && cpu_2050.B_FULL[i] != 0 && cpu_2050.LS_FULL[i] == 0) {
                /* Read, Read Backword */
                log_device("Xfer B to LS\n");
                cpu_2050.ROUTINE[i] = 0x40;   /* .LSRD  qv109 */
                cpu_2050.CHPOS[i] = 0x200;
                cpu_2050.CLI_REG[i] = cpu_2050.CBI_REG[i];
                cpu_2050.break_in |= 1<<i;
            }

            if (cpu_2050.OP_REG[i] == 1 && cpu_2050.LS_FULL[i] == 1 && cpu_2050.B_FULL[i] == 0) {
                /* Write */
                log_device("Xfer LS to B\n");
                cpu_2050.ROUTINE[i] = 0x44;   /* .LSWR  qv108 */
                cpu_2050.CHPOS[i] = 0x400;
                cpu_2050.break_in |= 1<<i;
            }
            }

            if ((cpu_2050.TAGS_IN[ch] & (CHAN_OPR_IN)) == 0) {
                cpu_2050.TAGS[ch] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                cpu_2050.polling[ch] = 1;
            }

            if (cpu_2050.LS_FULL[i] != 0 && cpu_2050.CHREQ[i] == 0 && cpu_2050.CHPOS[i] == 0 &&
                         (cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) != 0 && (cpu_2050.OP_REG[i] & 0x6) != 0) {
                /* Read, Read Backword */
                log_device("Request save LS\n");
                cpu_2050.CHREQ[i] = BIT0;
            }

            if (cpu_2050.LS_FULL[i] == 0 && cpu_2050.CHREQ[i] == 0 && cpu_2050.CHPOS[i] == 0 &&
                        (cpu_2050.TAGS_IN[ch] & CHAN_OPR_IN) != 0 && cpu_2050.OP_REG[i] == 1 &&
                        cpu_2050.EOR[i] == 0) {
/*                   (cpu_2050.GP_REG[i] & (BIT5)) == 0) { */
                /* Write */
                log_device("Request load LS\n");
                cpu_2050.CHREQ[i] = BIT0;
            }

            if ((cpu_2050.TAGS_IN[ch] & (CHAN_OPR_IN|CHAN_STA_IN)) == (CHAN_OPR_IN|CHAN_STA_IN)) {
                if (cpu_2050.CCI_REG[i] != 0 && cpu_2050.OP_REG[i] != 1) {
                    cpu_2050.C_FULL[i] = 1;
                }
                if (cpu_2050.OP_REG[i] == 1) {
                    cpu_2050.C_FULL[i] = 0;
                    cpu_2050.B_FULL[i] = 0;
                    cpu_2050.LS_FULL[i] = 0;
                }
                if (cpu_2050.B_FULL[i] == 0 && cpu_2050.C_FULL[i] == 0 && cpu_2050.LS_FULL[i] == 0) {
                    log_selchn("Read GP=%02x %02x\n", cpu_2050.GP_REG[i], cpu_2050.BUS_IN[i] & 0xff);
                    cpu_2050.C_REG[i] &= 0x00ffffff;
                    cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << 24;
                    log_selchn("Request END\n");
                    cpu_2050.CHREQ[i] = BIT3;   /* Request End */
                                   cpu_2050.C3[i] = 0;  /* Full status interrupt */
                    cpu_2050.IOSTAT[i] = 0;
                    if (cpu_2050.OP_REG[i] == 0x6) {
                        switch((cpu_2050.GP_REG[i] >> 3) & 3) {
                        case 0: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 1: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 2: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 3: cpu_2050.IOSTAT[i] |= 0; break;
                        }
                    } else {
                        switch((cpu_2050.GP_REG[i] >> 3) & 3) {
                        case 0: cpu_2050.IOSTAT[i] |= 0; break;
                        case 1: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 2: cpu_2050.IOSTAT[i] |= BIT3; break;
                        case 3: cpu_2050.IOSTAT[i] |= BIT3; break;
                        }
                    }
                    if ((cpu_2050.BUS_IN[ch] & 0xb3) != 0 || (cpu_2050.FLAGS_REG[i] & (CHAN_CC_FLAG << 8)) != 0) {
                        log_selchn("Error status\n");
                        cpu_2050.IOSTAT[i] |= BIT4;
                    }
                }

               /* IOSTAT 4 = Error or comm chain
                  IOSTAT 3 = Not 1 reg full and not 3 regs full
                  IOSTAT 2 = Not 2 regs full and not 1 reg ful
                  IOSTAT 1 = WR CHAIN DATA AND NEW CCW NOT USED */
            }
         }

            if ((cpu_2050.TAGS_IN[ch] & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
                cpu_2050.TAGS[ch] &= ~CHAN_SRV_OUT;
            }

            if (cpu_2050.IF_STOP[i] != 0 && (cpu_2050.TAGS_IN[ch] & (CHAN_CMD_OUT|CHAN_SRV_IN)) == (CHAN_CMD_OUT)) {
               cpu_2050.TAGS[ch] &= ~(CHAN_CMD_OUT);
            }

            if ((cpu_2050.TAGS_IN[ch] & (CHAN_SRV_IN|CHAN_OPR_IN|CHAN_SRV_OUT)) == (CHAN_SRV_IN|CHAN_OPR_IN)) {
                log_device("Srv C=%d\n", cpu_2050.C_FULL[i]);
                if (cpu_2050.IF_STOP[i] != 0) {
                    cpu_2050.TAGS[ch] |= CHAN_CMD_OUT;
                    /* Check for Short Length record. */
                    if ((cpu_2050.FLAGS_REG[i] & (CHAN_SLI_FLAG << 8)) == 0) {
                       cpu_2050.FLAGS_REG[i] |= CHAN_LENGTH;
                    }
                } else if (cpu_2050.C1[i] == 0 && cpu_2050.IF_STOP[i] == 0) {
                switch(cpu_2050.OP_REG[i]) {
                case 4:     /* Read */
                        if (cpu_2050.C_FULL[i] == 0) {
                            int      byte = 3 - ((cpu_2050.GP_REG[i] >> 5) & 0x3);
                            int      bp = byte * 8;
                            uint32_t msk = 0xff << bp;
                            log_device("Read GP=%02x %02x M=%08x %d\n", cpu_2050.GP_REG[i], cpu_2050.BUS_IN[ch] & 0xff, msk, bp);
                            cpu_2050.C_REG[i] &= ~msk;
                            cpu_2050.CCI_REG[i] |= 0x1 << byte;
                            cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << bp;
                            cpu_2050.GP_REG[i] += 0x20;
                            cpu_2050.GP_REG[i] &= 0x7f;
                            cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                            /* Check if full */
                            if ((cpu_2050.GP_REG[i] & BIT5) == 0 && (cpu_2050.GP_REG[i] & 0x60) == 0) {
                                log_device("Set C Full %02x  %02x\n", cpu_2050.GP_REG[i],
                                          ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                 cpu_2050.C_FULL[i] = 1;
                            }
                            if ((cpu_2050.GP_REG[i] & BIT5) != 0 &&
                                (((cpu_2050.GP_REG[i] >> 2) ^ (cpu_2050.GP_REG[i])) & 0x18) == 0) {
                                log_device("Set C Full last %02x  %02x\n", cpu_2050.GP_REG[i],
                                          ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                cpu_2050.C_FULL[i] = 1;
                           }
                           if (cpu_2050.C_FULL[i]) {
                                if ((cpu_2050.GP_REG[i] & BIT5) != 0) { /* L1W */
                                     if (cpu_2050.CD[i]) {
                                         cpu_2050.C1[i] = 1;
                                     } else {
                                         cpu_2050.IF_STOP[i] = 1;
                                     }
                                     break;
                                }
                                if ((cpu_2050.GP_REG[i] & BIT6) != 0) { /* L2W */
                                   cpu_2050.GP_REG[i] |= BIT5;
                                   break;
                                }
                                if ((cpu_2050.GP_REG[i] & BIT7) != 0) { /* L3W */
                                   cpu_2050.GP_REG[i] |= BIT6;
                                   break;
                                }
                           }
                      }
                        break;
                case 6:     /* Read backward */
                        if (cpu_2050.C_FULL[i] == 0) {
                            int      byte = 3 - ((cpu_2050.GP_REG[i] >> 5) & 0x3);
                            int      bp = byte * 8;
                            uint32_t msk = 0xff << bp;
                            log_device("Rdbk GP=%02x %02x M=%08x %d\n", cpu_2050.GP_REG[i], cpu_2050.BUS_IN[ch] & 0xff, msk, bp);
                            cpu_2050.C_REG[i] &= ~msk;
                            cpu_2050.CCI_REG[i] |= 0x1 << byte;
                            cpu_2050.C_REG[i] |= ((uint32_t)cpu_2050.BUS_IN[ch] & 0xff) << bp;
                            cpu_2050.GP_REG[i] -= 0x20;
                            cpu_2050.GP_REG[i] &= 0x7f;
                            cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                            /* Check if full */
                            if ((cpu_2050.GP_REG[i] & BIT5) == 0 && (cpu_2050.GP_REG[i] & 0x60) == 0x60) {
                                log_device("Set C Full %02x  %02x\n", cpu_2050.GP_REG[i],
                                          ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                 cpu_2050.C_FULL[i] = 1;
                            }
                            if ((cpu_2050.GP_REG[i] & BIT5) != 0 &&
                                (((cpu_2050.GP_REG[i] >> 2) ^ (cpu_2050.GP_REG[i])) & 0x18) == 0) {
                                log_device("Set C Full last %02x  %02x\n", cpu_2050.GP_REG[i],
                                          ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                cpu_2050.C_FULL[i] = 1;
                           }
                           if (cpu_2050.C_FULL[i]) {
                                if ((cpu_2050.GP_REG[i] & BIT5) != 0) { /* L1W */
                                     if (cpu_2050.CD[i]) {
                                         cpu_2050.C1[i] = 1;
                                     } else {
                                         cpu_2050.IF_STOP[i] = 1;
                                     }
                                     break;
                                }
                                if ((cpu_2050.GP_REG[i] & BIT6) != 0) { /* L2W */
                                   cpu_2050.GP_REG[i] |= BIT5;
                                   break;
                                }
                                if ((cpu_2050.GP_REG[i] & BIT7) != 0) { /* L3W */
                                   cpu_2050.GP_REG[i] |= BIT6;
                                   break;
                                }
                           }
                        }
                        break;
                case 1:     /* Write */
                        if (cpu_2050.C_FULL[i] == 1) {
//                            int      end = cpu_2050.GP_REG[i]+ 0x8;
                            int      byte = 3 - ((cpu_2050.GP_REG[i] >> 5) & 0x3);
                            int      bp = byte * 8;
                            uint32_t msk = 0xff << bp;
                            log_device("Write GP=%02x M=%08x %d\n", cpu_2050.GP_REG[i], msk, bp);
                            cpu_2050.CCI_REG[i] |= 0x1 << byte;
                            cpu_2050.BUS_OUT[ch] = (uint8_t)((cpu_2050.C_REG[i] >> bp) & 0xff);
                            cpu_2050.BUS_OUT[ch] |= odd_parity[cpu_2050.BUS_OUT[ch]];
                            cpu_2050.GP_REG[i] += 0x20;
                            cpu_2050.GP_REG[i] &= 0x7f;
                            cpu_2050.TAGS[ch] |= CHAN_SRV_OUT;
                            /* Check if empty */
                            if (cpu_2050.EOR[i] && cpu_2050.B_FULL[i] == 0 && cpu_2050.LS_FULL[i] == 0) {
                                if ((((cpu_2050.GP_REG[i] >> 2) ^ (cpu_2050.GP_REG[i])) & 0x18) == 0) {
                                    log_device("Set C Empty last %02x  %02x\n", cpu_2050.GP_REG[i],
                                              ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                    cpu_2050.C_FULL[i] = 0;
                           /* Check if Empty and LS is empty and data chaining */
                                     if (cpu_2050.CD[i]) {
                      cpu_2050.CHREQ[i] = BIT4;
                                         cpu_2050.C1[i] = 1;
                                     } else {
                                         cpu_2050.IF_STOP[i] = 1;
                                     }
                               }
                            } else {
                             if ((cpu_2050.GP_REG[i] & 0x60) == 0) {
                                log_device("Set C Empty %02x  %02x\n", cpu_2050.GP_REG[i],
                                          ((cpu_2050.GP_REG[i] >> 2) ^ cpu_2050.GP_REG[i]) & 0x18);
                                 cpu_2050.C_FULL[i] = 0;
                            }
                         }
                        }
                        break;
                }
#if 0
                            if ((cpu_2050.FLAGS_REG[i] & (CHAN_PCI_FLAG << 8)) != 0 &&
                                  cpu_2050.CHREQ[i] == 0 && cpu_2050.C1[i] == 0) {
                               log_selchn("PCI\n");
                               cpu_2050.FLAGS_REG[i] |= CHAN_PCI;
                               cpu_2050.FLAGS_REG[i] &= ~(CHAN_PCI_FLAG << 8);
                                 cpu_2050.CHREQ[i] |= BIT3;  /* Request end */
                                   cpu_2050.C3[i] = 1;      /* Indicate PCI */
                            }
#endif
                            /* PCI */
                            if ((cpu_2050.FLAGS_REG[i] & (CHAN_PCI_FLAG << 8)) != 0) {
                               log_selchn("PCI\n");
                               cpu_2050.FLAGS_REG[i] |= CHAN_PCI;
                               cpu_2050.FLAGS_REG[i] &= ~(CHAN_PCI_FLAG << 8);
                                   cpu_2050.C3[i] = 1;      /* Indicate PCI */
                                 if (cpu_2050.CHREQ[i] == 0 && cpu_2050.C1[i] == 0) {
                                 cpu_2050.CHREQ[i] |= BIT3;  /* Request end */
                                 }
                            }
                }


        }
    }


    log_reg("u=%02x v=%02x w=%02x mf=%x l=%08x r=%08x alu=%08x aob=%08x c=%08x c0=%x\n",
              cpu_2050.u_bus, cpu_2050.v_bus, cpu_2050.w_bus,
              (cpu_2050.io_mode) ? cpu_2050.io_mvfnc: cpu_2050.mvfnc,
              cpu_2050.left_bus, cpu_2050.right_bus, cpu_2050.alu_out, cpu_2050.aob_latch,
              carries, carry_out);
    log_reg("L=%c%08x R=%c%08X H=%08X M=%08X IAR=%06X SAR=%06X SDR=%08X F=%X Q=%X\n",
              (cpu_2050.LSGNS) ? '-' : '+', cpu_2050.L_REG, (cpu_2050.RSGNS)? '-' : '+',
              cpu_2050.R_REG, cpu_2050.H_REG, cpu_2050.M_REG, cpu_2050.IA_REG, cpu_2050.SAR_REG,
              cpu_2050.SDR_REG, cpu_2050.F_REG, cpu_2050.Q_REG);
    log_reg("MD=%X MB=%X LB=%X S=%02X J=%X G1=%c%X,G2=%c%X LSA=%02X FN=%02X BS=%X ED=%X IBFULL=%X BCHI=%x inst=%d LS=%08x\n",
              cpu_2050.MD_REG, cpu_2050.MB_REG, cpu_2050.LB_REG, cpu_2050.S_REG, cpu_2050.J_REG,
              (cpu_2050.G1NEG) ? '-' : '+', (cpu_2050.G_REG >> 4), (cpu_2050.G2NEG) ? '-' : '+',
              (cpu_2050.G_REG & 0xf), cpu_2050.LSA, cpu_2050.FN, cpu_2050.BS_REG, cpu_2050.ED_REG,
              cpu_2050.IBFULL, cpu_2050.BCHI, cpu_2050.inst_latch, cpu_2050.LS[cpu_2050.LSA]);
    log_reg("ILC=%X CC=%X AMWP=%X PM=%X MASK=%02X KEY=%01x poll=%d R=%d 1S=%d brk=%x bo=%d BRC=%d IO=%x IO_KEY=%x\n",
              cpu_2050.ILC, cpu_2050.CC, cpu_2050.AMWP, cpu_2050.PMASK, cpu_2050.MASK,
              cpu_2050.KEY, cpu_2050.polling[0], cpu_2050.REFETCH, cpu_2050.SYLS1, cpu_2050.break_in,
              cpu_2050.break_out, cpu_2050.BRC, cpu_2050.IO_REG, cpu_2050.IO_KEY);
    if (cpu_2050.CH == 3) {
        log_mpxchn("CH=%x CTL=%02x IOS=%x BFR1=%02x BFR2=%02x BI=%x BUS_IN=%02x BUS_OUT=%02x BROAR=%03x RTN=%02X BK=%x FLAGS=%02x\n",
                 cpu_2050.CH, cpu_2050.CHCTL, cpu_2050.IOSTAT[cpu_2050.CH] , cpu_2050.BFR1, cpu_2050.BFR2,
                 cpu_2050.BI_REG, cpu_2050.BUS_IN[0] & 0xff, cpu_2050.BUS_OUT[0] & 0xff, cpu_2050.BROAR,
                 cpu_2050.ROUTINE[3], cpu_2050.break_in, cpu_2050.FLAGS_REG[3]);
    } else {
        int    ch = cpu_2050.CH + 1;
        log_selchn("CH=%x CTL=%02x IOS=%x REQ=%02x POS=%02x CLK=%x BI=%x BUS_IN=%02x BUS_OUT=%02x BROAR=%03x RTN=%02X BK=%x\n",
                 cpu_2050.CH, cpu_2050.CHCTL, cpu_2050.IOSTAT[cpu_2050.CH],
                 cpu_2050.CHREQ[cpu_2050.CH], cpu_2050.CHPOS[cpu_2050.CH], cpu_2050.CHCLK[cpu_2050.CH],
                 cpu_2050.BI_REG, cpu_2050.BUS_IN[ch] & 0xff, cpu_2050.BUS_OUT[ch] & 0xff, cpu_2050.BROAR,
                 cpu_2050.ROUTINE[cpu_2050.CH], cpu_2050.break_in);
    }

    for (i = 0; i < 1; i++) {
        log_selchn("%d: B=%08x full=%d C=%08x full=%d LS=%d OP=%02x GP=%02x POS=%x REQ=%x CBI=%x CCI=%x CLI=%x STOP=%x C1=%d\n",
            i, cpu_2050.B_REG[i], cpu_2050.B_FULL[i], cpu_2050.C_REG[i], cpu_2050.C_FULL[i], cpu_2050.LS_FULL[i],
            cpu_2050.OP_REG[i], cpu_2050.GP_REG[i], cpu_2050.CHPOS[i], cpu_2050.CHREQ[i], cpu_2050.CBI_REG[i], cpu_2050.CCI_REG[i],
            cpu_2050.CLI_REG[i], cpu_2050.IF_STOP[i], cpu_2050.C1[i]);
        log_selchn("%d: LS[0] = %08x LS[1] = %08x LS[2] = %08x LS[3] = %08x FLAGS=%03x CD=%d c=%d, e=%d poll=%d EOR=%d\n", i,
                  cpu_2050.LS[i<<2], cpu_2050.LS[(i << 2)|1], cpu_2050.LS[(i << 2)| 2], cpu_2050.LS[(i << 2)|3], cpu_2050.FLAGS_REG[i],
                  cpu_2050.CD[i], (cpu_2050.GP_REG[i] >> 5) & 3, (cpu_2050.GP_REG[i]>> 3) & 3, cpu_2050.polling[i+1], cpu_2050.EOR[i] );
    }
    /* Handle break in cycle */
    if (cpu_2050.last_cycle && (cpu_2050.break_in & (1 << cpu_2050.CH)) != 0) {
        dtc_latch = 0;
            cpu_2050.ROAR = cpu_2050.ROUTINE[cpu_2050.CH];
             log_trace("Tranfer to %03x\n", cpu_2050.ROAR);
               cpu_2050.break_in &= ~(1 << cpu_2050.CH);
    cpu_2050.first_cycle = 0;
    }
    cpu_2050.last_cycle = 0;


#if 0
142  STPR  Storage protection
149  DATA  Invalid Decimal data or sign
1c0  IVAD  Invalid Opnd address
1c2  SPEC  Addr Spec violation

240  IPL
242  System reset.
2c2  PSW restarton IPL-IN


Select
004  IRPT  Interrupt routine
008  UAFT  Unit Address Fetch routine
00c  ENDU  End Update routine
010  STIO  Start I/O
014  COMP  Compare routine.
020  WFCH  Write Fetch routine.
024  RST0  Read Store routine
028  CCW1  Load CCW1
02c  TICH  Transfer in channel
030  CCW2  Load CCW2
040  LSRD  Local Store read routine
044  LSWR  Local Store write routine

F = 0000  Start IO
F = 0100  Test IO
F = 0010  Halt IO
F = 1000  Test Chan


 A -> 080 + n<<2
 B -> 0A0 + n<<2
 C -> 082 + n<<2
 D -> 0A2 + n<<2

Selector:

    GP      12   Byte cnt
            34   End reg
            567  OP/LW reg
            890  flags

    12 = bits 30/31 AOB
    34 = bits 30/31 AOB
    567  bit  4-7   AOB
    8910 bits 0-3   AOB
    CD
    CC
    SLI
    SKIP
    PCI

         4567
    xxxx xx01  Write  output                  0110
    xxxx xx10  Read   input forward           0000
    xxxx xx11  Control output                 0110
    xxxx 0100  Sense  input forward           0000
    xxxx 1100  Read   input backard           0001
    xxxx 1000  TIC

         bit 4&!bit7 -> bit 7
         bit 7       -> bit 5,6
         0           -> bit 4

     0 input fwd
     1 input back
     3 skip
     4 end stat and not WLR
     5 end stat and WLR
     6 output
     7 stop

   Request register 2 & 4         -> Unit select.
                        4 & Pri 3 -> SIO routine
                    2     & Pri 3 -> UA Fetch
                    3     & Pri 3 -> END routine
                    1     & Pri 3 -> INTR routine
                       5 & Pri 3 -> Compare ok, INTR routine

                    0    & Pri 2 -> Empty Write fetch.

     0 Empty Write fetch    020
     1 Interrupt routine.   004
     2 UA Fetch             008
     3 End routine          00c
     4 SIO                  010/028/02c
     5 Compare              014

004  IRPT  Interrupt routine
008  UAFT  Unit Address Fetch routine
00c  ENDU  End Update routine
010  STIO  Start I/O
014  COMP  Compare routine.
020  WFCH  Write Fetch routine.
024  RST0  Read Store routine
028  CCW1  Load CCW1
02c  TICH  Transfer in channel
030  CCW2  Load CCW2
040  LSRD  Local Store read routine
044  LSWR  Local Store write routine

    SIO  qv100-j1    L=UA,00,CH,UA R=CAW, M=op,Addr CHPOSTEST
               g2    L= ""         R= "", M=""      DTC2       aob = L
               j3    L= CA+8       R= CA, M=""      CHPOSTEST
               g4    L=05,DA       R=CA   M=op,Addr DTC2       aob = op,da
    TICH qv101
    CCW1 qv102
               DTC1 if chain data latch off
                     B <- aob_latch.
               Step 0 A1.
               if chain data latch off
                     C <- B. Request CCW2
                     GP<0-4,5-7> = 0;
               DTC1 step 1 A0.
                    if chain data latch off
                        OP <- aob_latch.
                        B <- aob_latch.
                    else
                        if read bkwd
                           CH <- ~aob_latch & 3;GP<1,2> = 1;
                        else
                           CH <- aob_latch & 3; OP<1,2> = 1;
                    if OP == TIC {
                        Request TICH
                        if (instruct latch or D1) {
                             if (D1 not) {
                                 STAT2 <- 1;
                             } else
                                Prog check.
                        }
                    } else {
                        D1 <- 0.
                        GP<5-7> <- OP.
                    }

     IOS2 IOS1
       0  0   For input
       0  1   Back input
       1  x   Tic
    CCW2 qv103 j1      ""          R=CA+8   ""      CHPOSTEST
               g2    L=05,DA       R=05,DA M=op,addr
               c3    L=UA,00,CH,UA R=05,5FE, M=op,addr  DTC2
               e4    L=UA,0001     R=05,5FE, M=op,addr  DTC2 CCW2TEST
               a5    L=UA,02,0001  R=000002    M=op,addr   CHPOSTEST
               e6    L=05,DA       R=dev,02,0003 M=op,adr  DTC2 LSWDTEST

    ENDU qv106   IOS4,1   00=Normal 10=Error, 11/01 = WR Chain  IOS3 CA False

    ITRP qv107   IOS0,IOS3 00 Normal, 10 Status only, 11 Poll Proceed

Multiplex
080  RTA0  Routine A0 - adr in, poll control
084  RTA1  Routine A1 - a0, brctrl 0
088  RTA2  Routine A2 - a1, brctrl 1, output, not status in |
                        a1, brctrl 2, input, fwd/bkwd service_in |
                        b3, brctrl 7, service_in, input, status == 0, not skip |
                        b3, brctrl 7, service_in, input, status == 0, skip |
                        b3, brctrl 7, status != 0, not input |
                        d3, brctrl 1, not prog ck, status == 0, new cc flag, service in, not skip, input |
                        d3, brctrl 1, not prog ck, status == 0, new cc flag, not input |
                        d3, brctrl 1, not prog ck, status == 0, not new cc flag, not input |
                        d3, brctrl 1, not prog ck, status == 0, not new cc flag, input, not skip, service in |
                        d7, (brctrl 1 | brctrl 3), not input |
                        d7, (brctrl 1 | brctrl 3), input, not skip, service in
08c  RTA3  Routine A3   a1, brctrl 3, stop, input skip, not service in |
                        a2, service in, not service out, (bc3 | brctrl 7), input |
                        a2, service in, not service out, (bc3 | brctrl 7), output |
                        a3, brctrl 7, not service out, service in, input |
                        a3, brctrl 7, output, count > 1 |
                        a7, brctrl 7, (prot ck | inv addr), cda flag, service in, not service out |
                        a7, brctrl 7, not cda flag, ervice in, not service out, input |
                        a7, brctrl 7, not cda flag, service in, not service out, output |
                        d3, brctrl 3, not prot ck, status == 0, input, skip, service in, not new cc flag |
                        d3, brctrl 3, not prot ck, status == 0, input, skip, service in, new cc flag |
                        brctrl 7, d7, service in |
                        d7, service in, input, skip, (brctrl 1 | brctrl 3)
090  RTA4  Routine A4   a1, brctrl 1, output, status in, not cc trg on |
                        d7, brctrl 7, not service in, status in, not cc trg off |
                        c-br, status in, cc latch off
094  RTA5  Routine A5   a1, brctrl 1, output, status in, cc trg on |
                        d7, brctrl 7, not service in, status in, cc trg on |
                        c-br, status in, cc latch on
098  RTA6  Routine A6   active, brctrl 5, a4, ib not full | a4, ib not full, brctrl 6, not active |
                        a4, brctrl 7, ib full | not sili flag, count != 0, (brctrl 5|brctrl 7), a5 |
                        (brctrl 5|brctrl 7), a5, ((((status mod, not device end) | unit exctp | unit ck | busy | atten), sili flag) | count === 0)
                        (brctrl 5|brctrl 7), a5, count == 0 | (sili flag, not ch end, device end, seq ctrl 5, not cc end rcvd) |
                        d3, brctrl 5, ib not full, prog ck | d3, brctrl 5, ib not full, not prog ck, new cc flag, unit status==error |
                        not prog ck, new cc flag, unit status==error, d3, brctrl 7, ib full | d3, brctrl 7, ib full, prog ck
09c  RTA7  Routine A7   (inv adr | prot ck), a2, input | a2, input, brctrl 5, decr count == 0 |
                        a2, output, (brctrl 3|brctrl 7), decr count == 1 |
                        a2, output, (brctrl 3|brctrl 7), inv adr |
                        (inv adr | prot ck) | a3, input, brctrl 5, decr count == 0 |
                        a3, brctrl 7, output, decr count == 1 |
                        a3, brctrl 7, output, (inv adr | prot ck)

0a0  RTB0  Routine B0  - Any ch0 op, poll ctrl-no req-in, not test channel
0a4  RTB1  Routine B1  - b0, brctrl 0, start i/o
0a8  RTB2  Routine B2  - b1, brctrl 6, seq ctrls idle, op in, adr in
0ac  RTB3  Routine B3  - b2, brctrl 4, sta in

0b4  RTB5  Routine B5  - c-br, not sta in, not op in, ucw store trg on, any ck off | any ck on & a-ck | b7, brctrl 11 | a5, brctrl 3, ch end, no dev end
0b8  RTB6  Routine B6  - b5, brctrl 2 | d5, brctrl 0 i/o stat code = start
0bc  RTB7  Routine B7  - c-br, not sta in, not op in, ucw store trg on, any ck on | any ck on & a-ck

002  RTC0  Routine C0  - no inh i/o fixed adr, pci mpx req
086  RTC1  Routine C1  - b0, brctrl 0, (test i/o | int test i/o)
08a  RTC2  Routine C2  - c1, brctrl 6, op in, adr in, (dev end,attn in, idle) | c7, brctrl 6, op in, adr in
08e  RTC3  Routine C3  - c2, brctrl 2, adr in, sta in
092  RTC4  Routine C4
096  RTC5  Routine C5  - b1, brctrl 6, seq ctr idle, op in, sta in, no sel in | c1, brctrl 6, not op in, not int test i/o (dev end, attn, idle) |
                         c6, brctrl 6, no op in, sta in | c7, brctrl 6, not op in, not int test i/o
09a  RTC6  Routine C6  - b0, brctrl 0, halt i/o
09e  RTC7  Routine C7  - c1, brctrl 1, ch end | c1, brctrl 1, ch end, test i/o

0a2  RTD0  Routine D0  - a5, brctrl 6, (cc end recv | (ch end, dev end)) | a7, brctrl 6, cda flag, not inv adr, not prot ck |
                         b3, brctrl 6, cc flag, ch end, dev end | d4, brctrl 1, m/op==tic, val adr, not prev tic |
                         d3, brctrl 6, not prog ck, new cc flag, status equal, not error, dev end
0a6  RTD1  Routine D1  - cda, d0, brctrl 1 | d0, brctrl 1, not cda, not op in
0aa  RTD2  Routine D2  - d1, brctrl 6, op in, adr in | m/op invalid, d4, brctrl 6, cc | d4, brctrl6, cc, m/op valid, not tic
0ae  RTD3  Routine D3  - d2, brctrl 2, sta in
0b2  RTD4  Routine D4  - d1, brctrl 2
0b6  RTD5  Routine D5  - prog ck, b1, brctrl 1, seq ctrl idle | b1, brctrl 1, seq ctrl idle, (tic | inv op) |
                         cc flag, status err, b3, brctrl 15 | b3, brctrl 15, not cc flag, status != 0) |
                         (any busy | status mod), c3, brctrl 3 | c3, brctrl 3, !(any busy|status mod), (ch end | dev end queued) |
                         c5, brctrl 3 | c6, brctrl 6, op in, adr in, wait for not op in | c7, brctrl 3
0be  RTD7  Routine D7  - d1, brctrl 3 | prev tic, valid adr, m/op=tic, (d4 | brctrl3 | cda) |
                           (d4 | brctrl3 | cda), invalid adr, m/op=tic |
                           (d4 | brctrl3 | cda), m/op invalid |
                           (d4 | brctrl3 | cda), m/op valid, not tic

resume polling - seq ctrl idle, b0, brctrl 0, foul on st i/o |
                 b0, brctrl 0, foul on st i/o, seq brctrl 5 busy |
                 b0, brctrl 6, inv bump adr | b1, brctrl 2, seq ctrl is active |
                 b1, brctrl 6, seq ctrl idel, op in, sel in, b6, no op in, not status in |
                 (busy | ch end recv), c1, brctrl 2 | c1, brctrl 2, ch end in ib, test i/o ua != ib ua |
                 c3, brctrl 2, status == 0, (not any busy | not status mod), (not ch end, dev end queued) |
                 c6, brctrl 1, (ch end in IB| ch end queued) | c6, brctrl 6, sel in, not op in, not status in |
                 c7, brctrl 2, ucw ua != test i/o ua |
                 d5, not op in, not status in | d7, brctrl 7, not service in, not op in, not status in, ucw store tag off
                 not op in, not status in, ucw store tgr off, c-br

log    -   a0, brctrl 6, inv bump adr | b2, brctrl 1, ucw ua != inter ua |
           c1, brctrl 6, (ib = dev end|ib = atten | idle), not op in, intr test i/o |
           c2, brctrl 1, instr ua != inter ua | c7, brctrl 6, not op in, intr test i/o, (status in|sel in) |
           d1, brctrl 6, not op in, (status in | sel in)

Start channel by W->CHCTL opcode.
OPPanel

0000 nil
0001 Instr step, not start
0010 set ic
0011 repeat inst
010x addr sync
011x enter channel
1yyz store display
  yy 00 main storage
     01 protect tags
     10 local store
     11 mpx bump
  z  0 display 1 store

Multiplex
W->CHCTL branch B0
W->CHCTL branch STIO

  E->CH E=1001    Set GPS3

B0 sequencial controlles:

    xx00    Idle.
    xx10    Dev end or att in IB
    xxx1    Active

bfr2 =
     0 input fwd
     1 input back
     3 skip
     4 end stat and not WLR
     5 end stat and WLR
     6 output
     7 stop

Seq controls
     0  idle
     1  busy
     3  CC end read
     5  CC end in IB
     6  Dev end/atten in IB
     7  CC End Queued

Channel status
     0  PCI
     1  WLR
     2  Program Check
     3  Protect Check
     4  Channel Data Check
     5  Channel Control check
     6  Interface Control check
     7  Chaining check

F
    0000  Start I/O
    0001  Foul on Start I/O
    0100  Test I/O
    0010  Halt I/O
    1000  Test Channel
    0000  Proceed with interrupt.

    B0 brctl 0, startio -> B1

    B1 brtcl 6, seq ctrls idle, op in, adr in -> B2

    B2 brctl 4, status in -> B3

    C-br, not status in, not op in, ucw store trg on, any chk off -> B5
    A-ck, any chk off -> B5
    B7  brctl 11 -> B5
    A5  brctl 3, ch end, not dev end -> B5

    B5  brctl 2 -> B6
    D5  brctl 0, io/state code = start -> B6
    C-br Not status in, not op in, UCW store trg on, any check on -> B7
    C-br status in, cc latch off -> A4
    C-br status in, cc latch on -> A5
    C-br not status in, not op in, ucw store tgr on, any ck off -> B5
    C-br not service in, not op in, not status in, ucw store tgr off  -> polling
    A-ck any chk on -> B7
    A-ck Any ck off -> B5

    not inhibit I/O, PCI MPX request to ROAR -> C0
    Any ch0 op code, poll ctrl, no request in, not test channel -> B0
    poll control, addr in -> A0

    A0  brctl 0 -> A1
        brctl 6 -> Invalid address.
          Set COM OUT.
                       BFR1 = UA  BFR2 = FL OP
    A1  brctl 1, output, not status in -> A2
    A1  brctl 1, output, status in, not cc tgr on -> A4
    A1  brctl 1, output, status in, cc trg on -> A5
    A1  brctl 2, input, fwd/back, service in -> A2
    A1  brctl 3, stop, input skip, not service in -> A3
       A1  IOS0, PCI, IOS1,2,3 OP  BFR2 FL OP
         State A1 Service in -> A2
         State A1 IOS=3,7 -> A3
         State A1 Status In BFR1.1 == 0 -> A4
         State A1 Status In BFR1.1 == 1 -> A5

    A2  brctl 3, service in, not service out, input -> A3
    A2  brctl 3, service in, not service out, output -> A3
    A2  brctl 5, input, count == 0 -> A7
    A2  brctl 7, service in, not service out, input -> A3
    A2  brctl 7, service in, not service out, output -> A3
       A2  IOS0, PCI, IOS1,2,3 OP  BFR1 DAB, BFR2 Data byte
         brctl 5 -> A7
         Service in not service out -> A3
       IOS0 -> Set PCI request trigger.

    A3  brctl 5, input, count == 0 -> A7
    A3  brctl 7, not servie out, service in, input -> A3
    A3  brctl 7, output, count > 1 -> A3
       A3  IOS0, PCI, IOS1,2,3 OP  BFR1 DAB, BFR2 Data byte

           brctl 5 if count == 0.
    A4  brctl 5, IB not full, active  -> A6
    A4  brctl 6, IB not full, not active -> A6
    A4  brctl 7, IB full -> A6
       A4 IOS0, PCI, IOS1  IOS2 SILI IOS3 Active        BRF1 PCI OP, SEQ BFR2 Status
                        1     0       0   SILI Not WLR
                        1     1       1   Not SILI(WLR)

          Accept or stack status

    A5  brctl 3, chn end, not device end -> B5
    A5  brctl 5, not sili flag, count != 0 -> A6
    A5  brctl 5, (((not device end, status mod bit) | unit expc | unit chk | busy | attent), SILI flag) | count == 0 -> A5
    A5  brctl 5, Count == 0 | (SILI flag, not ch end, device end, seq ctl 5, not cc end rec) -> A5
    A5  brctl 6, (cc end rec | (ch end, dev end)) -> D0
    A5  brctl 7, not sili flag, count != 0 -> A6
    A5  brctl 7, (((not device end, status mod bit) | unit expc | unit chk | busy | attent), SILI flag) | count == 0 -> A5
    A5  brctl 7, Count == 0 | (SILI flag, not ch end, device end, seq ctl 5, not cc end rec) -> A5
           IOS0 PCI, IOS2 Stat mod, IOS2 0(CC) IOS3 0(not TIC)  BFR1 PCI,OP, FLAG BFR2 Status
           IOS0 0, IOS2 1, IOS2 0 IOS3 WLR  BFR1 PCI,OP, FLAG BFR2 Status
    A7  brctl 6, CDA flag, not inv add, not prot ck -> D0
    A7  brctl 7, service in, not service out, (prot chk|inv addr), cda flag -> A3
    A7  brctl 7, service in, not service out, not cda flag, input -> A3
    A7  brctl 7, service in, not service out, not cda flag, output -> A3
           CDA and not STO Chk
         IOS0 PCI, IOS1 0 no stat mod, IOS2 1 CDA, IOS3 0 no tic  BFR1 DAB, BFR2 FL STOP
           CDA and STO chk
         IOS0 PCI, IOS1,2,3 111 STOP BFR1 UA, BFR2 FL STOP
           NO CDA
         IOS0 PCI, ISO1,2,3 111 STOP BFR1 UA, BFR2 FL STOP
    B0  brctl 0, (test i/o| int test i/o) -> C1
    B0  brctl 0, halt i/o -> C6
    B0  brctl 0, start i/O  -> B1
    B0  brctl 0, foul on st i/o, seq ctl idle -> polling
    B0  brctl 0, foul on st i/o, seq ctl 5 busy -> polling
    B0  brctl 6, inv bump adr -> polling  CC=3, GS3
         IOS0,1,2,3 Seq cont, BFR1 Seq cntrl, BFR2 MOD OP
            F == 0000  -> B1
            F == 0100  -> C1
            F == 0010  -> C6
            F == 1000   Test chan

    B1  brctl 1, seq ctrl idle, prog ck -> D5
    B1  brctl 1, seq ctrl idle, (TIC | invalid op) -> D5
    B1  brctl 2, seq ctrl active -> polling  Set CC2, GSS3
    B1  brctl 6,  op in, sel in -> polling
    B1  brctl 6, seq ctrl idle, op in, address in -> B2
    B1  brctl 6, seq ctrl idle, op in, status in -> C5
          IOS0,1,2,3 = 00x1     BFR1 = 0, BFR2 = 0
          IOS0,1,2,3 = 00x0     BFR1 = OP, BFR2 = Mod OP
              Set address out.
              bctrl 1 -> D5
              bctrl 2 -> GPS3=1, CC=2  break out
              bctrl 6 & IVA -> polling.
              bctrl 6 & addr In -> B2     /* Select */
              bctrl 6 & status in -> C5   /* Device quick busy */

    B2  brctl 4, status in -> B3
          IOS0 CDA, IOS1 CC and NO CDA IOS2 SILI and NO CDA, IOS3 Skip, BFR1=10001000 BFR2=FL OP
        brctl 1, LOGOUT  UA != device.
            Set Command out.
            Brctl 4 & status in -> B3
    B3  brctl 3, CC=0 -> A2
    B3  brctl 6, CC flag, ch end, dev end -> D0
    B3  brctl 7, service in, input, status=0 -> A2
    B3  brctl 7, service in, input, status=0 -> A2
    B3  brctl 7, status == 0, not input -> A2
    B3  brctl 15, CC flag, status error -> D5
    B3  brctl 15, not CC flag, status != 0, (any busy | status mod) -> D5
        IOS0 PCI IOS1,2,3 OP  BFR1 0000,DAB BFR2 Flags OP
        IOS0 x   IOS1,2,3 100  BFR1 0000 0011 BFR2 FL OP
        IOS0 PCI, IOS1 Stat Mod IOS2,3 00  BFR1 Status, BFR2 FLAGS OP
            CC set Suppess out. or Service out.
    B5  brctl 2 -> B6
    B6           not op in, not status in, (busy | ch end rec) -> polling
    B7  brctl 11 -> B5
    C0  IOS0 0 NOPCI, IOS1,2,3 OP, BFR1 DAB, BFR2 Data
    C0   -> End
    C1  brctl 1, ch end queued -> C7
    C1  brctl 1, ch end in IB, test I/O ua = IB ua -> C7
    C1  brctl 2, (busy | ch end rec) -> polling
    C1  brctl 2, ch end in ib, test i/o ua != IB ua -> polling
    C1  brctl 6, op in, addr in, (device end | attn in IB | idle) -> C2
    C1  brctl 6, not op in, not int test i/o, (dev end | attn in ib | idle) -> C5
         IOS 1100   BFR1 (INST UA, BFR2 (INST UA ^ UCW UA
              set ADR out. Sel out if Idle.
         brctl 6 addr in, opr in -> C2
         brctl 1 -> C7
         brctl 2 -> CC=2 -> Polling  GPS3 = 1
    C2  brctl 2, addr in, status in -> C3
          Send Comm out.
        brctl 2 status in -> C3
        brctl 1, log out.
    C3  brctl 2, status == 0, (not any busy|not status mod | not ch end|device end queue) -> polling
    C3  brctl 3, (any busy|status mod bit) -> D5
    C3  brctl 3, !(any busy|status mod bit), (chn end | dev end queue) -> D5
           No Chan end
         IOS = 1000  BFR1 = US
           Chan end
         IOS = 1100  BFR1 = US
         Respond Sev out.
         brctl 3 -> D5
         brctl 2 -> CC=0 GPS3 = 1
    C5  brctl 3  -> D5
         Not from HALTIO
         IOS = 0100   BFR1 = (US)
         From Halt IO
         IOS = 0101   BFR1 = (US) BFR2 = 10110111
             DTC2 reset Select out.
    C6  brctl 6, op in, addr in, wait for not op in -> D5
    C6  brctl 1, (ch end in IB| ch end queued) -> polling
    C6  brctl 6, select in, not op in, not status in -> polling
        brctl 2 -> set CC0
         IOS = 0101  BFR1 = 00000111 BFR2 = 10110111
         Addr out.
         Sel out.
         BRctl 6 op in, addr -> D5

    C7  brctl 2, ucw ua != test i/o ua -> polling
    C7  brctl 3  -> D5
    C7  brctl 6, op in, addr in -> C2
    C7  brctl 6, not op in status in -> C5
    C7  brctl 6, not op in, not int test i/o -> C5
        IOS0=1 IOS1 =0 IOS2 = PCI IOS3 = 1, BFR1 INT CODE, BFR2 US
    D0  brctl 1, CDA -> D1
    D0  brctl 1, not CDA, not op in -> D1
        IOS0 PCI, IOS1 Status mod, IOS2 CC CDA, IOS3=0 (For tic tic), BFR1=CHS,RO BFR2=Unit status
    D1  brctl 1 -> D4
    D1  brctl 3 -> D7
    D1  brctl 6, op in, addr in -> D2
          IOS0 PCI, IOS1=1(Prog ck), IOS2 CC CDA, IOS3=prev tic BFR1-CH.S.R BFR2=UA
    D2  brctl 2, status in -> D3
          Not prog ck
          IOS0 Old PCI, IOS1 New CC, IOS2 0 Not prog ck, IOS3 New skip BFR1 FL OP BFR2 MOD OP
          Not valid CA
          TIC-TIC Cnt flag test
          IOS0 old PCI  IOS1 0       IOS2 1 prog ck,  IOS3 0   BFR1 0000PCI HALT, BFR2 UA
    D3  brctl 1, not prog ck, status == 0, new cc flag, input, not skip, service in -> A2
    D3  brctl 1, not prog ck, status == 0, new cc flag, not input -> A2
    D3  brctl 1, not prog ck, status == 0, not new cc flag, not input -> A2
    D3  brctl 1, not prog ck, status == 0, not new cc flag, input, not skip, service in -> A2
    D3  brctl 3, not prog ck, status = 0, input, skip, service in, not new cc flag -> A3
    D3  brctl 3, not prog ck, status = 0, input, skip, service in, new cc flag -> A3
    D3  brctl 5, ib not full, prog ck -> A6
    D3  brctl 5, ib not full, not prog ck, new cc flag, unit status = error -> A6
    D3  brctl 6, not prog ck, new cc flag, status equal, not error, device end -> D0
    D3  brctl 7, ib full, not prog ck, new cc flag, unit status = error -> A6
    D3  brctl 7, ib full, prog ck -> A6
         IOS0=PCI, IOS1=Status mod, IOS2=0(CC), IOS3=0(No tic), BFR1=1011,PCI,OP BFR2=status
    D4  brctl 1, M/op = TIC, valid addr, not prev tic -> D0
    D4|brctl 3|CDA, m/op invalid -> D7
    D4|brctl 3|CDA, m/op valid, not tic -> D7
    D4|brctl 3|CDA, m/op TIC, invalid addr -> D7
    D4|brctl 3|CDA, m/op TIC, valid addr -> D7
    D4  brctl 6, CC, M/op invalid -> D2
    D4  brctl 6, CC, M/op valid, not TIC -> D2
         IOS0 Old PCI, IOS1 Prog chk, IOS2 CC CDA IOS3 Prev Tic BFR1=(CH.S.R.PCI OP), BFR2 (M OP)
    D5  brctl 0, i/o stat code=start -> B6
         brctl 8 = startio
         Set GPS2 = 1.
         E->CH 0 set GPS3,
    D7  brctl 1, not input -> A2
    D7  brctl 1, input, not skip, service in -> A2
    D7  brctl 1, input, skip -> A3
    D7  brctl 3, input, skip -> A3
    D7  brctl 3, not input -> A2
    D7  brctl 3, input, not skip, service in -> A2
    D7  brctl 7, service in -> A3
    D7  brctl 7, not service in, status in, not cc tgr off -> A4
    D7  brctl 7, not service in, status in, cc trg on -> A5
    D7  brctl 7, not service in, not op in, not status in, ucw store tgr off  -> polling
         IOS0 OLD+NEWPCI IOS1,2,3 OP BFR1 =0000 DAB, BFR2 FL OP
            prog ck
         IOS0 PCI OLD/NEW, IOS1,2,3=1 STOP, BFR1 = UA, BFR2 = 0000 PCI 111







    A0 brctl 0 -> A1, IVA trap
    A1 brctl 1 output BFR2=06 Status in == 0 -> A2
       brctl 2 input BFR2=0|1 service in > A2
       brctl 3 stop, inputskip BFR2=3, no service in > A3
       brctl 1 output, BFR2=06 status in, not cc trg on -> A4
       brctl 1 output, BFR2=06 status in, not cc trg off -> A5

    A2 (brctl 7|BC3), (input|output), service in, not service out -> A3
       brctl 5, count=0, input -> A7
       input, (invalid addr | prot chk) -> A7
        brctl 3, brctl7, output, count=1 -> A7
        brctl 3, brctl7, output, invalid -> A7

Multiplex
080  RTA0  Routine A0 - adr in, poll control
084  RTA1  Routine A1 - a0, brctrl 0
088  RTA2  Routine A2 - a1, brctrl 1, output, not status in |
                        a1, brctrl 2, input, fwd/bkwd service_in |
                        b3, brctrl 7, service_in, input, status == 0, not skip |
                        b3, brctrl 7, service_in, input, status == 0, skip |
                        b3, brctrl 7, status != 0, not input |
                        d3, brctrl 1, not prog ck, status == 0, new cc flag, service in, not skip, input |
                        d3, brctrl 1, not prog ck, status == 0, new cc flag, not input |
                        d3, brctrl 1, not prog ck, status == 0, not new cc flag, not input |
                        d3, brctrl 1, not prog ck, status == 0, not new cc flag, input, not skip, service in |
                        d7, (brctrl 1 | brctrl 3), not input |
                        d7, (brctrl 1 | brctrl 3), input, not skip, service in
08c  RTA3  Routine A3   a1, brctrl 3, stop, input skip, not service in |
                        a2, service in, not service out, (bc3 | brctrl 7), input |
                        a2, service in, not service out, (bc3 | brctrl 7), output |
                        a3, brctrl 7, not service out, service in, input |
                        a3, brctrl 7, output, count > 1 |
                        a7, brctrl 7, (prot ck | inv addr), cda flag, service in, not service out |
                        a7, brctrl 7, not cda flag, ervice in, not service out, input |
                        a7, brctrl 7, not cda flag, service in, not service out, output |
                        d3, brctrl 3, not prot ck, status == 0, input, skip, service in, not new cc flag |
                        d3, brctrl 3, not prot ck, status == 0, input, skip, service in, new cc flag |
                        brctrl 7, d7, service in |
                        d7, service in, input, skip, (brctrl 1 | brctrl 3)
090  RTA4  Routine A4   a1, brctrl 1, output, status in, not cc trg on |
                        d7, brctrl 7, not service in, status in, not cc trg off |
                        c-br, status in, cc latch off
094  RTA5  Routine A5   a1, brctrl 1, output, status in, cc trg on |
                        d7, brctrl 7, not service in, status in, cc trg on |
                        c-br, status in, cc latch on
098  RTA6  Routine A6   active, brctrl 5, a4, ib not full | a4, ib not full, brctrl 6, not active |
                        a4, brctrl 7, ib full | not sili flag, count != 0, (brctrl 5|brctrl 7), a5 |
                        (brctrl 5|brctrl 7), a5, ((((status mod, not device end) | unit exctp | unit ck | busy | atten), sili flag) | count === 0)
                        (brctrl 5|brctrl 7), a5, count == 0 | (sili flag, not ch end, device end, seq ctrl 5, not cc end rcvd) |
                        d3, brctrl 5, ib not full, prog ck | d3, brctrl 5, ib not full, not prog ck, new cc flag, unit status==error |
                        not prog ck, new cc flag, unit status==error, d3, brctrl 7, ib full | d3, brctrl 7, ib full, prog ck
09c  RTA7  Routine A7   (inv adr | prot ck), a2, input | a2, input, brctrl 5, decr count == 0 |
                        a2, output, (brctrl 3|brctrl 7), decr count == 1 |
                        a2, output, (brctrl 3|brctrl 7), inv adr |
                        (inv adr | prot ck) | a3, input, brctrl 5, decr count == 0 |
                        a3, brctrl 7, output, decr count == 1 |
                        a3, brctrl 7, output, (inv adr | prot ck)

0a0  RTB0  Routine B0  - Any ch0 op, poll ctrl-no req-in, not test channel
0a4  RTB1  Routine B1  - b0, brctrl 0, start i/o
0a8  RTB2  Routine B2  - b1, brctrl 6, seq ctrls idle, op in, adr in
0ac  RTB3  Routine B3  - b2, brctrl 4, sta in

0b4  RTB5  Routine B5  - c-br, not sta in, not op in, ucw store trg on, any ck off | any ck on & a-ck | b7, brctrl 11 | a5, brctrl 3, ch end, no dev end
0b8  RTB6  Routine B6  - b5, brctrl 2 | d5, brctrl 0 i/o stat code = start
0bc  RTB7  Routine B7  - c-br, not sta in, not op in, ucw store trg on, any ck on | any ck on & a-ck

002  RTC0  Routine C0  - no inh i/o fixed adr, pci mpx req
086  RTC1  Routine C1  - b0, brctrl 0, (test i/o | int test i/o)
08a  RTC2  Routine C2  - c1, brctrl 6, op in, adr in, (dev end,attn in, idle) | c7, brctrl 6, op in, adr in
08e  RTC3  Routine C3  - c2, brctrl 2, adr in, sta in
092  RTC4  Routine C4
096  RTC5  Routine C5  - b1, brctrl 6, seq ctr idle, op in, sta in, no sel in | c1, brctrl 6, not op in, not int test i/o (dev end, attn, idle) |
                         c6, brctrl 6, no op in, sta in | c7, brctrl 6, not op in, not int test i/o
09a  RTC6  Routine C6  - b0, brctrl 0, halt i/o
09e  RTC7  Routine C7  - c1, brctrl 1, ch end | c1, brctrl 1, ch end, test i/o

0a2  RTD0  Routine D0  - a5, brctrl 6, (cc end recv | (ch end, dev end)) | a7, brctrl 6, cda flag, not inv adr, not prot ck |
                         b3, brctrl 6, cc flag, ch end, dev end | d4, brctrl 1, m/op==tic, val adr, not prev tic |
                         d3, brctrl 6, not prog ck, new cc flag, status equal, not error, dev end
0a6  RTD1  Routine D1  - cda, d0, brctrl 1 | d0, brctrl 1, not cda, not op in
0aa  RTD2  Routine D2  - d1, brctrl 6, op in, adr in | m/op invalid, d4, brctrl 6, cc | d4, brctrl6, cc, m/op valid, not tic
0ae  RTD3  Routine D3  - d2, brctrl 2, sta in
0b2  RTD4  Routine D4  - d1, brctrl 2
0b6  RTD5  Routine D5  - prog ck, b1, brctrl 1, seq ctrl idle | b1, brctrl 1, seq ctrl idle, (tic | inv op) |
                         cc flag, status err, b3, brctrl 15 | b3, brctrl 15, not cc flag, status != 0) |
                         (any busy | status mod), c3, brctrl 3 | c3, brctrl 3, !(any busy|status mod), (ch end | dev end queued) |
                         c5, brctrl 3 | c6, brctrl 6, op in, adr in, wait for not op in | c7, brctrl 3
0be  RTD7  Routine D7  - d1, brctrl 3 | prev tic, valid adr, m/op=tic, (d4 | brctrl3 | cda) |
                           (d4 | brctrl3 | cda), invalid adr, m/op=tic |
                           (d4 | brctrl3 | cda), m/op invalid |
                           (d4 | brctrl3 | cda), m/op valid, not tic

resume polling - seq ctrl idle, b0, brctrl 0, foul on st i/o |
                 b0, brctrl 0, foul on st i/o, seq brctrl 5 busy |
                 b0, brctrl 6, inv bump adr | b1, brctrl 2, seq ctrl is active |
                 b1, brctrl 6, seq ctrl idel, op in, sel in, b6, no op in, not status in |
                 (busy | ch end recv), c1, brctrl 2 | c1, brctrl 2, ch end in ib, test i/o ua != ib ua |
                 c3, brctrl 2, status == 0, (not any busy | not status mod), (not ch end, dev end queued) |
                 c6, brctrl 1, (ch end in IB| ch end queued) | c6, brctrl 6, sel in, not op in, not status in |
                 c7, brctrl 2, ucw ua != test i/o ua |
                 d5, not op in, not status in | d7, brctrl 7, not service in, not op in, not status in, ucw store tag off
                 not op in, not status in, ucw store tgr off, c-br

log    -   a0, brctrl 6, inv bump adr | b2, brctrl 1, ucw ua != inter ua |
           c1, brctrl 6, (ib = dev end|ib = atten | idle), not op in, intr test i/o |
           c2, brctrl 1, instr ua != inter ua | c7, brctrl 6, not op in, intr test i/o, (status in|sel in) |
           d1, brctrl 6, not op in, (status in | sel in)

#endif
}


/*
 * Run a 1us cycle time.
 */
void
step_2050()
{
    cycle_2050();
    cycle_2050();
}

struct _device *
model2050_init(void *render, uint16_t addr)
{
    return NULL;
}

/* Create a 2050 cpu system. */
int
model2050_create(struct _option *opt)
{
    extern  void setup_fp2050(void *rend);
    int     msize;

    if (title != NULL) {
        fprintf(stderr, "CPU already defined, can't support more then one\n");
        return 0;
    }
    title = "IBM360/50";
    setup_cpu = &setup_fp2050;
    step_cpu = &step_2050;

    if (opt->model != '\0') {
        msize = 2048 << (opt->model - 'A');
        if (msize < (64 * 1024) || msize > (512 * 1024)) {
            return 0;
        }
    } else {
        msize = 64 * 1024;
    }
    if ((M = (uint32_t *)calloc(msize/4, sizeof(uint32_t))) == NULL)
        return 0;
    mem_max = msize - 1;
    return 1;
}

DEV_LIST_STRUCT(2050, CPU_TYPE, CHAR_OPT|NUM_MOD);


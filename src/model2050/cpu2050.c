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

#include <string.h>

#include "logger.h"
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

static uint32_t tr_interloc = 0x2043bf50;

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
                  "MPX>U",   /* MPX to U */
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
                  "L>R,L>LS",
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
                  "1d",
                  "D",
                  "1f",
             };
static    char     *wl_name[] = {  /* Mover destination under I/O WL */
                  "0",         /* 0 */
                  "1",     /* 1 */ /* W to M index by MB */
                  "2",    /* 2 */ /* W bits 6-7 to MB */
                  "3",    /* 3 */ /* W bits 6-7 to LB */
                  "4",  /* 4 */ /* W bits 2-7 to PSW 34-39 */
                  "5",    /* 5 */ /* W to PSW 0-7 SSM */
                  "6",      /* 6 */
                  "7",   /* 7 */
                  "8",   /* 8 */ /* W,E(32) select bump sector address */
                  "9",     /* 9 */
                  "a",     /* a */
                  "b",       /* b */
                  "c",  /* c */
                  "d",     /* d */
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
                  "W,E>A(BUMP)",   /* 8 */ /* W,E(32) select bump sector address */
                  "WL>G1",     /* 9 */
                  "WL>G2",     /* a */
                  "W>G",       /* b */
                  "W>MMB(E)",  /* c */
                  "WL>MD",     /* d */
                  "WR>F",      /* e */
                  "W>MD>F",    /* f */
              };

static    char     *ms_name[] = {  /* UP and MD field under I/O */
                  "0",
                  "BFR(03)->IOS",
                  "BFR(47)->IOS",
                  "BIB(03)->IOS",
                  "BIB(47)->IOS",
                  "5",     /* Turn I/O Stats off per emit */
                  "6",     /* Turn I/O Stats off per Emit */
                  "7"      /* Set I/O stats on CC */
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
                 "0",
                 "1",
                 "2",
                 "3",
             };

static    char     *mg_name[] = {  /* DG field under I/O */
                  "BFR2->BIB",
                  "CHPOSTEST",
                  "BFR2->BOB",  /* BFR1->BIB */
                  "BFR2->BIB",  /* BIB->V? */
                  "BOB->BFR1",  /* BFR1->BIB */
                  "BOB->BFR2",
                  "BUSI->BFR1",
                  "BIB->BFR2"
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
                  "?",
             };
static    char     *ur_name[] = {  /* Mover right input */
                  "E>WR",
                  "U>WR",
                  "V>WR",
                  "?",
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
                  "(F=0)>ADDR",  /* Save carry */
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
                  "TZ*BZ",
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
                  "TZ*BS",    /* T Zero per Byte Stats */
                  "IBFULL",
                  "CANG",
                  "CHLOG",
                  "I-FETCH",   /* I Fetch Stat FCN A, 00 Odd-no ref, 10 even, 01 odd- ref, 11=? */
                  "IA(30)",
                  "EXP,CHIRPT",
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
                  "15",
                  "16",
                  "17",
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

static uint32_t    SA;

void
cycle_2050()
{
    uint16_t         next_roar;
    struct ROS_2050  *sal;
    int              a_bit;
    int              b_bit;
    int              carry_in;
    int              carry_out;
    uint8_t          s_update;
    uint32_t         l_update;
    uint32_t         carries;
    uint32_t         t1, t2;
    int              t;
    struct _device  *dev;

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
        cpu_2050.ROAR = 0x242;
        if (load_mode)
             cpu_2050.ROAR = 0x240;
        cpu_2050.init_mem = 0;
        cpu_2050.init_bump_mem = 0;
        cpu_2050.bump_mem = 0;
    }

    if (DISPLAY) {
        cpu_2050.OPPANEL = 0x8;
        DISPLAY = 0;
    }

    if (STORE) {
        cpu_2050.OPPANEL = 0x9;
        STORE = 0;
    }

    sal = &ros_2050[cpu_2050.ROAR];
    cpu_2050.ros_row1 = sal->row1;
    cpu_2050.ros_row2 = sal->row2;
    cpu_2050.ros_row3 = sal->row3;
    cpu_2050.ros_row4 = sal->row4;
    s_update = cpu_2050.S_REG;
    l_update = cpu_2050.L_REG;

    /* Main memory cycle */
    switch (cpu_2050.mem_state) {
    case R1:
             cpu_2050.mem_state = R2;
             /* Check if this cycle will access the SDR register, wait */
             if (tr_interloc & (1 << sal->tr))
                 goto channel;
             if (sal->al == 30 || sal->ss == 3 || sal->iv == 4 || sal->iv == 7)
                 goto channel;
             /* If we are initiating a memory cycle, or updating SDR, wait */
             if (cpu_2050.init_mem | cpu_2050.init_bump_mem | cpu_2050.update_d)
                 goto channel;
             log_info("mem cycle read %06X %08x\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG);
             break;
    case R2:
             cpu_2050.mem_state = R3;
             if (cpu_2050.bump_mem) 
                 cpu_2050.SDR_REG = cpu_2050.BUMP[cpu_2050.SAR_REG >> 2];
             else
                 cpu_2050.SDR_REG = cpu_2050.M[cpu_2050.SAR_REG >> 2];
             log_info("mem cycle read 2 %06X %08x i=%d ib=%d b=%d\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG, cpu_2050.init_mem, cpu_2050.init_bump_mem, cpu_2050.bump_mem);
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
             log_info("mem cycle read 3 %06X %08x i=%d ib=%d b=%d\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG, cpu_2050.init_mem, cpu_2050.init_bump_mem, cpu_2050.bump_mem);
             cpu_2050.mem_state = W1;
             /* If we are initiating a memory cycle, wait */
             if (cpu_2050.init_mem | cpu_2050.init_bump_mem)
                 goto channel;
             if (cpu_2050.update_d) {
                 if (cpu_2050.bump_mem) 
                     cpu_2050.BUMP[SA >> 2] = cpu_2050.SDR_REG;
                 else
                     cpu_2050.M[SA >> 2] = cpu_2050.SDR_REG;
             }
             break;
    case 0:  /* Memory cycle at system reset */
             cpu_2050.SDR_REG = cpu_2050.M[cpu_2050.SAR_REG >> 2];
             cpu_2050.mem_state = W1;
    case W1:
             /* Fall through */
             if (cpu_2050.update_d) {
                 if (cpu_2050.bump_mem) 
                     cpu_2050.BUMP[SA >> 2] = cpu_2050.SDR_REG;
                 else
                     cpu_2050.M[SA >> 2] = cpu_2050.SDR_REG;
                 cpu_2050.update_d = 0;
             }
             SA = cpu_2050.SAR_REG;

             log_info("mem write  %06X %08x\n", cpu_2050.SAR_REG, cpu_2050.SDR_REG);

             if (cpu_2050.init_mem | cpu_2050.init_bump_mem) {
                 cpu_2050.mem_state = R1;
                 cpu_2050.bump_mem = cpu_2050.init_bump_mem;
                 cpu_2050.init_mem = cpu_2050.init_bump_mem = 0;
                 if (sal->al == 30 || sal->ss == 3 || sal->iv == 4 || sal->iv == 7)
                     goto channel;
             } 
             
             break;
    }

    if ((log_level & LOG_ITRACE) != 0 && (cpu_2050.ROAR == 0x188)) {
        uint8_t   mem[6];
        int       i;
        for (i = 0; i < 6; i++) {
            uint32_t pc = i + cpu_2050.IA_REG;
            uint32_t mm = cpu_2050.M[pc >> 2];
            mem[i] = (mm >> (8 * (3 - (pc & 3)))) & 0xff;
        }
        
        print_inst(mem);
        log_itrace_c(" IC=%06x CC=%x", cpu_2050.IA_REG, cpu_2050.CC);
        log_itrace("\n");

        log_itrace_s(" ");
        for (i = 0; i < 16; i++) {
            log_itrace_c(" GR%02d = %08x", i, cpu_2050.LS[0x30 + i]);
            if ((i & 0x3) == 0x3) {
                 log_itrace("");
                 log_itrace_s(" ");
            }
        }
        log_itrace("");
    }


    /* Disassemble micro instruction */
    if ((log_level & LOG_MICRO) != 0) {
        char       dis_buffer[1024];
        char       t_buffer[100];

        sprintf(dis_buffer, "%s %03X: E=%0X [ry=%d lx=%d tc=%d tr=%d] ", sal->note, cpu_2050.ROAR, sal->ce,
                  sal->ry, sal->lx, sal->tc, sal->tr);
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
           strcat(dis_buffer, lx_name[sal->lx]);
        }
        strcat(dis_buffer, ">");
        strcat(dis_buffer, tr_name[sal->tr]);
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
            strcat(dis_buffer, ms_name[(sal->up<<1)|sal->mb]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, cg_name[(sal->lb<<1) |sal->mb]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, ct_name[sal->iv]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, mg_name[sal->dg]);
            strcat(dis_buffer, " ");
            strcat(dis_buffer, cy_name[sal->ad>>1]);
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
    /* Basic next address */
    next_roar = sal->zp;
    if (sal->zn != 0) {
        next_roar |= sal->zf << 2;
    }

    a_bit = 0;
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
            a_bit = (cpu_2050.LSGNS |= cpu_2050.RSGNS);
            break;
    case 15: /* Unknown */
            break;
    case 16: /* CRMD, mask condition codes */
            a_bit = cpu_2050.MD_REG & (1 << (3 - cpu_2050.CC));
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
            a_bit = ((cpu_2050.G_REG & 0xf0) == 0 || (cpu_2050.MB_REG == 0));
            break;
    case 27: /* IO Stat 0 to CPU */
            a_bit = (cpu_2050.IOSTAT & BIT0) != 0;
            break;
    case 28: /* IO Stat 2  */
            a_bit = (cpu_2050.IOSTAT & BIT2) != 0;
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
    case 50: /* Z23!=0 T16-31 != 0  */
            a_bit = (cpu_2050.alu_out & 0xFFFF) != 0;
            break;
    case 51: /* CCW2OK T5-7 == 0 && T16-31 != 0  */
            a_bit = ((cpu_2050.alu_out & 0xFFFF) == 0) |
                     ((cpu_2050.alu_out & 0x07000000) != 0);
            break;
    case 52: /* MXBIO  bus in bit 0  */
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
            a_bit = ((cpu_2050.IOSTAT & BIT4) != 0);
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
            /* Set to one if High speed channel or 256 sub channel option */
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
            t1  = ((cpu_2050.BS_REG & 8) ? 0xff000000: 0) |
                  ((cpu_2050.BS_REG & 4) ? 0x00ff0000: 0) |
                  ((cpu_2050.BS_REG & 2) ? 0x0000ff00: 0) |
                  ((cpu_2050.BS_REG & 1) ? 0x000000ff: 0);
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
            b_bit |= (cpu_2050.IOSTAT & BIT1) != 0;
            break;
    case 27: /* MD/JI MD Odd gt 8 or J odd gt 8 */
            b_bit |= ((cpu_2050.MD_REG & 0x9) != 0 || (cpu_2050.J_REG & 0x9) != 0);
            break;
    case 28: /* IVA  */
            break;
    case 29: /* IO Stat 3 */
            b_bit |= (cpu_2050.IOSTAT & BIT3) != 0;
            break;
    case 30: /* (CAR) branch on carry latch  */
//            b_bit |= cpu_2050.CAR;
            break;
    case 31: /* (Z00) looks at current T */
 //           b_bit |= (cpu_2050.alu_out & 0x80000000) != 0;
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
                     next_roar |= ((cpu_2050.M_REG >> 28) & 0xf) << 2;
                     break;
            case 7:  /* Undefined */
                     break;
            case 8:  /* M(47)->ROAR */
                     next_roar |= ((cpu_2050.M_REG >> 24) & 0xf) << 2;
                     break;
            case 9:  /* Undefined */
                     break;
            case 10:  /* F->ROAR */
                     next_roar |= (cpu_2050.F_REG & 0xf) << 2;
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
            /* By order IV7 if refetch == 0 and IAR bit 30 == 1 */
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

    next_roar |= (a_bit << 1)|b_bit;

#if 0
    /* Check if break in cycle */
    if (cpu_2050.mem_state == W1 && cpu_2050.in_break == 0) {
        if (cpu_2050.mpx_pollcntl) {
            if ((cpu_2050.MPX_TI & CHAN_REQ_IN) != 0) {
                cpu_2050.MPX_TAGS |= CHAN_SEL_OUT|CHAN_HLD_OUT;
            }
            if ((cpu_2050.MPX_TI & CHAN_ADR_IN) != 0) {
                routine = 0x80;  /* A0 */
                cpu_2050.mpx_pollcntrl = 0;
                in_break = 1;
                cpu_2050.firstcycle = 1;
            }
        } else if (cpu_2050.lastcycle) {
            switch (cpu_2050.routine) {
            case 0x00:   /* Idle */
            case 0x04:   /*  IRPT  Interrupt routine */
            case 0x08:   /*  UAFT  Unit Address Fetch routine */
            case 0x0c:   /*  ENDU  End Update routine */
            case 0x10:   /*  STIO  Start I/O  */
            case 0x14:   /*  COMP  Compare routine. */
            case 0x20:   /*  WFCH  Write Fetch routine. */
            case 0x24:   /*  RST0  Read Store routine */
            case 0x28:   /*  CCW1  Load CCW1 */
            case 0x2c:   /*  TICH  Transfer in channel */
            case 0x30:   /*  CCW2  Load CCW2 */
            case 0x40:   /*  LSRD  Local Store read routine */
            case 0x44:   /*  LSWR  Local Store write routine */

            case 0x80:  /*  RTA0  Routine A0 */
                        if (cpu_2050.brctl == 0) {
                            routine = 0x84;   /* A1 */
                        } else if (cpu_2050.brctl == 6) {
                            /* Invalid address */
                        }
                        break;
            case 0x84:  /*  RTA1  Routine A1 */
                        /* When address in drops, clear command out */
                        if ((cpu_2050.MPX_TI & (CHAN_CMD_OUT|CHAN_ADR_IN)) == CHAN_CMD_OUT) {
                            cpu_2050.MPX_TAGS &= ~CHAN_CMD_OUT;
                        }
                        if (cpu_2050.brctl == 1) {
                            if ((cpu_2050.MPX_TI & (CHAN_STA_IN)) != 0) {
                                if (cpu_2050.BFR2 & BIT1) {
                                   routine = 0x94;
                                } else {
                                   routine = 0x90;
                                }
                            } else {
                                routine = 0x88;
                            }
                        }
                        if (cpu_2050.brctl == 2 &&
                                 (cpu_2050.MPX_TI & CHAN_SRV_IN) != 0) {
                           routine = 0x88;
                        }
                        if (cpu_2050.brctl == 3 &&
                                 (cpu_2050.MPX_TI & CHAN_SRV_IN) == 0) {
                           routine = 0x8c;
                        }
                        break;
            case 0x88:  /*  RTA2  Routine A2 */
                        if (cpu_2050.brctl == 3) {
                           if ((cpu_2050.MPX_TI & (CHAN_SRV_IN|CHAN_SRV_OUT)) ==
                                (CHAN_SRV_IN)) {
                                routine = 0x8c;
                           } else
    A2  brctl 3, service in, not service out, input -> A3
    A2  brctl 3, service in, not service out, output -> A3
    A2  brctl 5, input, count == 0 -> A7
    A2  brctl 7, service in, not service out, input -> A3
    A2  brctl 7, service in, not service out, output -> A3
       A2  IOS0, PCI, IOS1,2,3 OP  BFR1 DAB, BFR2 Data byte
         brctl 5 -> A7
         Service in not service out -> A3
       IOS0 -> Set PCI request trigger.

            case 0x8c:  /*  RTA3  Routine A3 */
            case 0x90:  /*  RTA4  Routine A4 */
            case 0x94:  /*  RTA5  Routine A5 */
            case 0x98:  /*  RTA6  Routine A6 */
            case 0x9c:  /*  RTA7  Routine A7 */
            case 0xa0:  /*  RTB0  Routine B0 */
            case 0xa4:  /*  RTB1  Routine B1 */
            case 0xa8:  /*  RTB2  Routine B2 */
            case 0xac:  /*  RTB3  Routine B3 */
            case 0xb4:  /*  RTB5  Routine B5 */
            case 0xb8:  /*  RTB6  Routine B6 */
            case 0xbc:  /*  RTB7  Routine B7 */
            case 0x02:  /*  RTC0  Routine C0 */
            case 0x86:  /*  RTC1  Routine C1 */
            case 0x8a:  /*  RTC2  Routine C2 */
            case 0x8e:  /*  RTC3  Routine C3 */
            case 0x92:  /*  RTC4  Routine C4 */
            case 0x96:  /*  RTC5  Routine C5 */
            case 0x9a:  /*  RTC6  Routine C6 */
            case 0x9e:  /*  RTC7  Routine C7 */
            case 0xa2:  /*  RTD0  Routine D0 */
            case 0xa6:  /*  RTD1  Routine D1 */
            case 0xaa:  /*  RTD2  Routine D2 */
            case 0xae:  /*  RTD3  Routine D3 */
            case 0xb2:  /*  RTD4  Routine D4 */
            case 0xb6:  /*  RTD5  Routine D5 */
            case 0xbe:  /*  RTD7  Routine D7 */
            }
        }
    }
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

#endif    

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
                                  (cpu_2050.PMASK);
                break;
        case 6:  /* LMB */
                cpu_2050.u_bus = (cpu_2050.L_REG >> ((3 - cpu_2050.MB_REG) * 8)) & 0xff;
                break;
        case 7:  /* LLB */
                cpu_2050.u_bus = (cpu_2050.L_REG >> ((3 - cpu_2050.LB_REG) * 8)) & 0xff;
                break;
        }

        /* Gate data to V inputs */
        switch (sal->mv) {
        case 0:   /* 0 */
                cpu_2050.v_bus = 0;
                break;
        case 1:   /* MLB */
                cpu_2050.v_bus = (cpu_2050.M_REG >> ((3 - cpu_2050.LB_REG) * 8)) & 0xff;
                break;
        case 2:   /* MMB */
                cpu_2050.v_bus = (cpu_2050.M_REG >> ((3 - cpu_2050.MB_REG) * 8)) & 0xff;
                break;
        case 3:   /* Undefined */
                cpu_2050.v_bus = 0;
                break;
        }
    }


    /* Do mover function left */
    switch (sal->ul) {
    case 0:  /* E->WR */
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
                    cpu_2050.w_bus = cpu_2050.v_bus & 0xf0;
                    break;
            case 6: /* Zone */
                    cpu_2050.w_bus = cpu_2050.u_bus & 0xf0;
                    break;
            case 7:  /* Unknown */
                    cpu_2050.w_bus = 0;
                    break;
            }
            break;
    }

    /* Do mover function right */
    switch (sal->ur) {
    case 0:  /* E->WL */
            cpu_2050.w_bus |= (sal->ce);
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
            case 5: /* Numeric */
                    cpu_2050.w_bus |= cpu_2050.u_bus & 0x0f;
                    break;
            case 6: /* Zone */
                    cpu_2050.w_bus |= cpu_2050.v_bus & 0x0f;
                    break;
            case 7:  /* Unknown */
                    break;
            }
            break;
    }

    if (cpu_2050.io_mode) {
         /* Channel timing */
         switch (sal->iv) {  /* CT */
         case 0:  /* Nop */
                 break;
         case 1:  /* FIRSTCYLE, if not first cycle, channel check */
                 break;
         case 2:  /* DTC1 send ingate timing to channel */
                 break;
         case 3:  /* DTC2 send outgate timing to channel */
                 break;
         case 4:  /* IA/4->A,IA, initiate storage read, inhibit invalid address, set flag */
                 if (cpu_2050.IA_REG & 1) {
                     cpu_2050.IVA = 1;
                 } else {
                     cpu_2050.IA_REG += 4;
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                     cpu_2050.init_mem = 1;
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
    } else {
         /* Instruction address register */
         switch (sal->iv) {
         case 0:  /* Nop */
                 break;
         case 1:  /* WL->IVD, trap if mover output bits 0-3 greater 9 */
                 if ((cpu_2050.w_bus & 0xf0) < 0x90) {
                     next_roar = IVD;
                 }
                 break;
         case 2:  /* WR->IVD, trap if mover output bits 4-7 greater 9 */
                 if ((cpu_2050.w_bus & 0x0f) < 0x09) {
                     next_roar = IVD;
                 }
                 break;
         case 3:  /* W->IVD, trap if mover output bits 0-3 or 4-7 greater 9 */
                 if ((cpu_2050.w_bus & 0xf0) < 0x90 || (cpu_2050.w_bus & 0x0f) < 0x09) {
                     next_roar = IVD;
                 }
                 break;
         case 4:  /* IA/4->A,IA, initiate storage read, inhibit invalid address, set flag */
                 if (cpu_2050.IA_REG & 1) {
                     cpu_2050.IVA = 1;
                 } else {
                     cpu_2050.IA_REG += 4;
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                     cpu_2050.init_mem = 1;
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
                 if (sal->zn == 1) {  /* SMIF */
                    if (cpu_2050.REFETCH == 0 && (cpu_2050.IA_REG & 0x2) != 0)
                        break;
                 }
                 if (cpu_2050.REFETCH) {
                     cpu_2050.SAR_REG = cpu_2050.IA_REG;
                 } else {
                     cpu_2050.SAR_REG = (cpu_2050.IA_REG + 2);
                 }
                 cpu_2050.init_mem = 1;
                 break;
         }
         /* Compute carry into adder */
         carry_in = 0;
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
                 if ((cpu_2050.G_REG & 0xf0) == 0) {
                     cpu_2050.G1NEG = 1;
                 } else {
                     cpu_2050.G1NEG = 0;
                 }
                 cpu_2050.G_REG = (cpu_2050.G_REG & 0x0f) | ((cpu_2050.G_REG - 0x10) & 0xf0);
                 break;
         case 4: /* HOT1,G-1 */
                 if (cpu_2050.G_REG == 0) {
                     cpu_2050.G1NEG = 1;
                 } else {
                     cpu_2050.G1NEG = 0;
                 }
                 cpu_2050.G_REG = cpu_2050.G_REG - 1;
                 break;
         case 5: /* G2-1 */
                 if ((cpu_2050.G_REG & 0x0f) == 0) {
                     cpu_2050.G2NEG = 1;
                 } else {
                     cpu_2050.G2NEG = 0;
                 }
                 cpu_2050.G_REG = (cpu_2050.G_REG & 0x0f) | ((cpu_2050.G_REG - 0x10) & 0xf0);
                 break;
         case 6: /* G-1 */
                 if (cpu_2050.G_REG == 0) {
                     cpu_2050.G1NEG = 1;
                 } else {
                     cpu_2050.G1NEG = 0;
                 }
                 cpu_2050.G_REG = cpu_2050.G_REG - 1;
                 break;
         case 7: /* G1,2-1 */
                 if ((cpu_2050.G_REG & 0xf0) == 0) {
                     cpu_2050.G1NEG = 1;
                 } else {
                     cpu_2050.G1NEG = 0;
                 }
                 if ((cpu_2050.G_REG & 0x0f) == 0) {
                     cpu_2050.G2NEG = 1;
                 } else {
                     cpu_2050.G2NEG = 0;
                 }
                 cpu_2050.G_REG = ((cpu_2050.G_REG & 0x0f) - 0x1) | ((cpu_2050.G_REG - 0x10) & 0xf0);
                 break;
         }
     }


    if (cpu_2050.io_mode) {
        carry_in = (sal->wm & 1);
        /* Do adder, update output */
        cpu_2050.alu_out = cpu_2050.left_bus + cpu_2050.right_bus + carry_in;
        carries = (cpu_2050.left_bus & cpu_2050.right_bus) |
                     ((cpu_2050.left_bus ^ cpu_2050.right_bus) & ~cpu_2050.alu_out);
        carry_out = 0;

    } else {
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
                cpu_2050.CAR = carry_out;
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
                if ((carries & 0x800000000) != 0) {
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
    }

    if (sal->bb == 30 || sal->bb == 31) {
         if (sal->bb == 30) {  /* (CAR) branch on carry latch  */
            b_bit |= carry_out;
         } else  {             /* (ZOO) branch on T bit 0 */
            b_bit |= (cpu_2050.alu_out & 0x80000000) != 0;
         }
         if (sal->zn == 2) { /* A&(B=0)->A */
            if (!b_bit) {
                a_bit = 1;
            }
         }
         if (sal->zn == 3) { /* A&(B=1)->A */
            if (b_bit) {
                a_bit = 1;
            }
         }
         next_roar &= ~3;
         next_roar |= (a_bit << 1)|b_bit;
    }


    /* Do adder shifter */
    switch (sal->al) {
    case 0: /* Normal */
            cpu_2050.aob_latch = cpu_2050.alu_out;
            break;
    case 1: /* Q->SR1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | (cpu_2050.Q_REG << 31);
            cpu_2050.F_REG = ((cpu_2050.alu_out & 1) << 3) | (cpu_2050.F_REG >> 1);
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
            cpu_2050.F_REG = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            cpu_2050.F_REG ^= 0x1;
            break;
    case 8: /* Q->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.Q_REG);
            cpu_2050.F_REG = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            break;
    case 9: /* F->SL1->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | (cpu_2050.F_REG);
            cpu_2050.F_REG = ((cpu_2050.alu_out >>31) & 1) | (cpu_2050.F_REG << 1);
            break;
    case 10: /* SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1);
            cpu_2050.Q_REG = ((cpu_2050.alu_out & 0x80000000) != 0);
            break;
    case 11: /* Q->SL1 */
            cpu_2050.aob_latch = cpu_2050.alu_out << 1 | (cpu_2050.Q_REG);
            break;
    case 12: /* SR1->F */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            cpu_2050.F_REG = ((cpu_2050.alu_out & 1) << 3) | (cpu_2050.F_REG >> 1);
            break;
    case 13: /* SR1->Q */
            cpu_2050.aob_latch = cpu_2050.alu_out >> 1;
            cpu_2050.Q_REG = cpu_2050.alu_out & 1;
            break;
    case 14: /* Q->SR1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | ((cpu_2050.Q_REG) << 31);
            cpu_2050.Q_REG = (cpu_2050.alu_out & 1);
            break;
    case 15: /* F->SL1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 1) | ((cpu_2050.F_REG >> 3) & 1);
            cpu_2050.F_REG = (cpu_2050.F_REG << 1);
            cpu_2050.Q_REG = ((cpu_2050.alu_out & 0x80000000) != 0);
            break;
    case 16: /* SL4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 4);
            cpu_2050.F_REG = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 17: /* F->SL4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out << 4) | (cpu_2050.F_REG);
            cpu_2050.F_REG = ((cpu_2050.alu_out >> 28) & 0xf);
            break;
    case 18: /* FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                   (cpu_2050.alu_out & 0xff000000);
            break;
    case 19: /* F->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                   (cpu_2050.alu_out & 0xff000000) | (cpu_2050.F_REG);
            break;
    case 20: /* SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 21: /* F->SR4->F */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) | (cpu_2050.F_REG << 28);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 22: /* FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out >> 4) & 0xffffff) |
                                      (cpu_2050.alu_out & 0xff000000);
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 23: /* 1->FPSR4->F */
            cpu_2050.aob_latch = ((cpu_2050.alu_out >> 4) & 0x0fffff) |
                                      (cpu_2050.alu_out & 0xff000000) | 0x100000;
            cpu_2050.F_REG = (cpu_2050.alu_out & 0xf);
            break;
    case 24: /* SR4->H */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4);
            cpu_2050.H_REG = (cpu_2050.alu_out & 0xF0000000) | (cpu_2050.H_REG & 0xfffffff);
            cpu_2050.R_REG = ((cpu_2050.aob_latch & 0x0F000000) << 4) |
                                  (cpu_2050.R_REG & 0xfffffff);
            break;
    case 25: /* F->SR4 */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 4) | (cpu_2050.F_REG << 28);
            break;
    case 26: /* E->FPSL4 */
            cpu_2050.aob_latch = ((cpu_2050.alu_out << 4) & 0xfffff0) |
                                  (cpu_2050.alu_out & 0xff000000) | (sal->ce & 0xf);
            break;
    case 27: /* F->SR1->Q */
            cpu_2050.aob_latch = (cpu_2050.alu_out >> 1) | ((cpu_2050.F_REG & 0x1) << 31);
            cpu_2050.Q_REG = cpu_2050.alu_out & 1;
            break;
    case 28: /* DKEY-> */
            cpu_2050.aob_latch = cpu_2050.DKEYS;
            break;
    case 29: /* CH, gate selector channels to latch  */
            break;
    case 30: /* D-> gate storage to latch, hold off it not ready */
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
               next_roar = IVD;
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
               next_roar = IVD;
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
            if ((cpu_2050.aob_latch & 0xffffff) == 0 && cpu_2050.F_REG == 0 && (s_update & BIT3) != 0)
                s_update |= BIT0;
            else
                s_update &= ~BIT0;
            cpu_2050.FN = sal->ce & 0x3;
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
            if (cpu_2050.aob_latch == 0)
                s_update |= BIT3;
            break;
    case 18: /* E->BS,T30->S3 */
            cpu_2050.BS_REG = (sal->ce & 0xf);
            s_update &= ~(BIT3);
            if ((cpu_2050.aob_latch & 02) != 0)
                s_update |= BIT3;
            break;
    case 19: /* E->BS */
            cpu_2050.BS_REG = (sal->ce & 0xf);
            break;
    case 20: /* 1->BS*MB */
            cpu_2050.BS_REG = 1 << (3 - cpu_2050.MB_REG);
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
            cpu_2050.S_REG &= 0xf0;
            cpu_2050.S_REG |= cpu_2050.OPPANEL;
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
            cpu_2050.MPX_TAGS |= CHAN_SUP_OUT;
            break;
    case 49: /* MPX RESET */
            break;
    case 50: /* E(0)->IBFULL */
            cpu_2050.IBFULL = sal->ce & 1;
            break;
    case 51: /* undefined */
            break;
    case 52: /* E->CH */
            /* 1001   Issue proc with irpt */
            /* 0101   Set Log trg */
            /* 0001   Reset common channel */
            /* 0010   Gate status */
            cpu_2050.CH = sal->ce << 2;
            break;
    case 53: /* undefined */
            break;
    case 54: /* 1->TIMERIRPT */
            break;
    case 55: /* T->PSW,IPL->T */
            l_update = (A_SW << 8) | (B_SW << 28) | (C_SW << 24);   /* MG field */
            break;
    case 56: /* T->PSW T(12-15) to PSW */
            cpu_2050.AMWP = (cpu_2050.alu_out >> 16) & 0xf;
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
            cpu_2050.MPX_TAGS |= CHAN_SEL_OUT;
            break;
    case 61: /* 1->ADROUT */
            cpu_2050.MPX_TAGS |= CHAN_ADR_OUT;
            break;
    case 62: /* 1->COMOUT */
            cpu_2050.MPX_TAGS |= CHAN_CMD_OUT;
            break;
    case 63: /* 1->SERVOUT */
            cpu_2050.MPX_TAGS |= CHAN_SRV_OUT;
            break;
    }

    switch (sal->tr) {
    case 0:  /* T */
            break;
    case 1:  /* R */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            break;
    case 2:  /* R0 */
            cpu_2050.R_REG = (cpu_2050.aob_latch & 0xff000000) |
                             (cpu_2050.R_REG & 0x00ffffff);
            break;
    case 3:  /* M */
            cpu_2050.M_REG = cpu_2050.aob_latch;
            break;
    case 4:  /* D */
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
            break;
    case 9:  /* R,AN, initaite memory request, suppress invalid memory address trap */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            cpu_2050.init_mem = 1;
            break;
    case 10:  /* R,AW, initiate memory request, invalid address trap if not word boundry  */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            cpu_2050.init_mem = 1;
            break;
    case 11:  /* R,AD  memory request, invalid address trap if not double boundry */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SAR_REG = (cpu_2050.aob_latch & 0xffffff);
            cpu_2050.init_mem = 1;
            break;
    case 12:  /* D->IAR interlock with storage timing rinmmg */
            /* Read */
            cpu_2050.IA_REG = cpu_2050.SDR_REG;
            break;
    case 13:  /* SCAN->D */
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
            cpu_2050.init_mem = 1;
            break;
    case 17:  /* R,D  */
            cpu_2050.R_REG = cpu_2050.aob_latch;
            cpu_2050.SDR_REG = cpu_2050.aob_latch;
            cpu_2050.update_d = 1;
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
            cpu_2050.REFETCH = 0;
            s_update &= ~(BIT0|BIT1);
            if (cpu_2050.J_REG == 0)
                s_update |= BIT0;
            if (cpu_2050.MD_REG == 0)
                s_update |= BIT1;
            if ((cpu_2050.MD_REG & 0xc0) == 0)
                cpu_2050.SYLS1 = 1;
            else
                cpu_2050.SYLS1 = 0;
            cpu_2050.ILC = 1;
            if ((cpu_2050.aob_latch & 0x80000000) != 0)
                cpu_2050.ILC = 2;
            if ((cpu_2050.aob_latch & 0xc0000000) == 0xc0000000)
                cpu_2050.ILC = 3;
            break;
    case 26:  /* MHL */
            cpu_2050.M_REG = (cpu_2050.M_REG & 0xffff0000) | ((cpu_2050.aob_latch >> 16) & 0xffff);
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
            cpu_2050.update_d = 1;
            break;
    case 30:  /* L13 */
            l_update = (cpu_2050.L_REG & 0xff000000) |
                             (cpu_2050.aob_latch & 0x00ffffff);
            break;
    case 31:  /* IO  gate adder 29-31 to I/O reg */
            break;
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
        case 10: /* W.E>R(BUMP) */
        case 11:
        case 12: /* W.E>R(BUMP)S */
        case 13:
               /* 1111112222222222
                  4567890123456789
                  LLEE      LLLLLL
                  0123      234567
                */
                cpu_2050.SAR_REG = (((uint32_t)cpu_2050.w_bus)<<2) |
                         ((sal->ce & 3) << 10);
                cpu_2050.init_bump_mem = 1;
                break;
        case 14: /* Nop */
        case 15:
                break;
        }
    } else {
        /* Store Mover output */
        switch (sal->wm) {
        case 0: /* No change */
                break;
        case 1: /* W->MMB  */
                cpu_2050.M_REG &= ~(0xff << (8 * (3-cpu_2050.MB_REG)));
                cpu_2050.M_REG |= (cpu_2050.w_bus << (8 * (3-cpu_2050.MB_REG)));
                break;
        case 2: /* W67->MB  */
                cpu_2050.MB_REG = cpu_2050.w_bus & 03;
                break;
        case 3: /* W67->LB  */
                cpu_2050.LB_REG = cpu_2050.w_bus & 03;
                break;
        case 4: /* W27->PSW4, 2-7 -> PSW 34-39  */
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
                /* 0000 Issue IRPT Test IO.
                   0001 SIO
                   0010 Channel Count=0 no ST3
                   0100 Emit Timout chk to Sel Chan.
                   1000 TCH
                   00010000   Foul on SIO
                */
                break;
        case 8: /* W,E->A(BUMP)  */
                cpu_2050.SAR_REG = (((uint32_t)cpu_2050.w_bus)<<2) |
                         ((sal->ce & 3) << 10);
                cpu_2050.init_bump_mem = 1;
                break;
        case 9: /* WL->G1  */
                cpu_2050.G_REG &= 0xf;
                cpu_2050.G_REG |= (cpu_2050.w_bus & 0xf0);
                break;
        case 10: /* WR->G2  */
                cpu_2050.G_REG &= 0xf0;
                cpu_2050.G_REG |= (cpu_2050.w_bus & 0x0f);
                break;
        case 11: /* W->G  */
                cpu_2050.G_REG = cpu_2050.w_bus;
                break;
        case 12: /* W->MMB(E?)  */
                cpu_2050.M_REG &= ~(0xff << (8 * (3-cpu_2050.MB_REG)));
                cpu_2050.M_REG |= (cpu_2050.w_bus << (8 * (3-cpu_2050.MB_REG)));
                break;
        case 13: /* WR->MD */
                cpu_2050.MD_REG = (cpu_2050.w_bus & 0x0f);
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
      log_trace("set up=%d lb=%d mb=%d md=%d\n", sal->up, sal->lb, sal->mb, sal->md);
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
            cpu_2050.R_REG = cpu_2050.LS[cpu_2050.LSA];
            break;
    case 3: /* unknown */
            break;
    case 4: /* L->LS */
            cpu_2050.LS[cpu_2050.LSA] = l_update;
            break;
    case 5: /* LS->R,L->LS */
            cpu_2050.R_REG = cpu_2050.LS[cpu_2050.LSA];
            cpu_2050.LS[cpu_2050.LSA] = l_update;
            break;
    case 6: /* LS->L->LS */
            l_update = cpu_2050.LS[cpu_2050.LSA];
            break;
    case 7: /* No function */
            break;
    }



    cpu_2050.L_REG = l_update;
    cpu_2050.S_REG = s_update;
    cpu_2050.ROAR = next_roar;
channel:

    log_reg("u=%02x v=%02x w=%02x l=%08x r=%08x alu=%08x aob=%08x\n",
              cpu_2050.u_bus, cpu_2050.v_bus, cpu_2050.w_bus,
              cpu_2050.left_bus, cpu_2050.right_bus, cpu_2050.alu_out, cpu_2050.aob_latch);
    log_reg("L=%c%08x R=%c%08X H=%08X M=%08X IAR=%06X SAR=%06X SDR=%08X F=%X Q=%X\n",
               (cpu_2050.LSGNS) ? '-' : '+', cpu_2050.L_REG, (cpu_2050.RSGNS)? '-' : '+',cpu_2050.R_REG,
               cpu_2050.H_REG, cpu_2050.M_REG, cpu_2050.IA_REG, cpu_2050.SAR_REG, cpu_2050.SDR_REG,
               cpu_2050.F_REG, cpu_2050.Q_REG);
    log_reg("MD=%X MB=%X LB=%X S=%02X J=%X G1=%c%X,G2=%c%X LSA=%02X FN=%02X\n",
                cpu_2050.MD_REG, cpu_2050.MB_REG, cpu_2050.LB_REG, cpu_2050.S_REG, cpu_2050.J_REG,
               (cpu_2050.G1NEG) ? '-' : '+', (cpu_2050.G_REG >> 4), (cpu_2050.G2NEG) ? '-' : '+', (cpu_2050.G_REG & 0xf),
                cpu_2050.LSA, cpu_2050.FN);
    log_reg("ILC=%X CC=%X AMWP=%X PM=%X MASK=%02X\n",
                cpu_2050.ILC, cpu_2050.CC, cpu_2050.AMWP, cpu_2050.PMASK, cpu_2050.MASK);

    if (cpu_2050.count & 1)
       return;

    cpu_2050.MPX_TI &= IN_TAGS;
    cpu_2050.MPX_TAGS |= cpu_2050.MPX_TAGS;
    for (dev = chan[0]; dev != NULL; dev = dev->next) {
         dev->bus_func(dev, &cpu_2050.MPX_TI, cpu_2050.BFR1, &cpu_2050.BUS_IN);
    }

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


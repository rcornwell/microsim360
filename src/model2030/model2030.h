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

#ifndef _MODEL30_H_
#define _MODEL30_H_

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

extern struct ROS_2030 {
    uint8_t    CN;
    uint8_t    CH;
    uint8_t    CL;
    uint8_t    CM;
    uint8_t    CU;
    uint8_t    CA;   /* Low 4 CA, bit 5 AA */
    uint8_t    CB;
    uint8_t    CK;   /* Low 4 CK, bit 5 AK */
    uint8_t    CD;
    uint8_t    CF;
    uint8_t    CG;
    uint8_t    CV;
    uint8_t    CC;
    uint8_t    CS;   /* Low 4 CS, bit 5 AS */
    uint8_t    PK;
    uint32_t   row1;
    uint32_t   row2;
    uint32_t   row3;
    char        note[16];
} ros_2030[4096];


extern struct CPU_2030 {
int         count;
uint32_t    M[64 * 1024];       /* Main memory */
uint16_t    LS[4096];           /* Local storage and BUMP storage */
uint8_t     MP[256];            /* Protection storage. 4 bits */

int          mem_max;           /* Maximum memory address - 1 */
                                /* 8K = 0x1FFF, 16K = 0x3FFF, 32K = 0x7FF, 64k = 0xFFFF */

uint32_t    ros_row1;           /* Current ROS values */
uint32_t    ros_row2;
uint32_t    ros_row3;

uint16_t    Abus;               /* Holds the input to the A side of ALU. */
uint16_t    Bbus;               /* Holds the input to the B side of ALU. */
uint16_t    Alu_out;            /* Holds output of Alu. */
uint8_t     prev_carry;         /* Holds previous carry out. */


uint8_t     C_REG;
uint16_t    D_REG;              /* Temporary data register. */
uint8_t     F_REG;              /* External IRQ */
uint16_t    G_REG;              /* Status regiser */
uint16_t    H_REG;              /* Priority control */
uint16_t    L_REG;              /* Length of data. */
uint16_t    Q_REG;              /* Storage protection key. */
uint16_t    R_REG;              /* Memory buffer. */
uint8_t     S_REG;              /* Status register */
uint16_t    T_REG;              /* Partial address. */
uint16_t    MC_REG;             /* Machine check */
uint16_t    XX_REG;             /* X latches, high order bump address. */
uint8_t     MASK;               /* System mask register.. */
uint8_t     ASCII;              /* ASCII flag latch */

uint16_t    M_REG;              /* M register */
uint16_t    N_REG;              /* N register */
uint16_t    MN_REG;             /* Memory address register */
uint16_t    I_REG;
uint16_t    J_REG;
uint16_t    U_REG;
uint16_t    V_REG;
uint8_t     SA_REG;             /* Storage address register. */
uint8_t     STAT_REG;           /* Immediate stat register */

uint16_t    WX;                 /* ROAR address register. */
uint16_t    FWX;                /* Backup ROAR address. */
uint16_t    GWX;                /* Backup ROAR address. */
uint8_t     MPX_STAT;           /* Save status register */
uint8_t     SEL_STAT;           /* Save status register */

/* Virtual registers */
/* FA functions.
 *
 *  Bits independent.
 * K = 10 Set command start latch on.
 * K =  8 Set bus out register from R.
 * K =  4 Set Address out.
 * K =  2 Set command out line.
 * K =  1 Set service out
 */

/* FB functions.
 *
 *  zero bits ignored.
 * K = C set mpx channel interrupt
 * K = 6 Set mpx opn latch
 * K = A set suppress out latch
 * K = 3 Set system mask based on R.
 * K = 5 Set op out.
 * K = 9 Set XX based on S012
 */

uint16_t    O_REG;              /* Mux bus out and control register. */
uint16_t    FI;                 /* MPX bus in. */
uint16_t    MPX_TAGS;           /* MPX Tags out */
uint16_t    MPX_TI;             /* MPX Tags in */
uint16_t    FT;                 /* MPX tag in */

/* FT bits.
 *
 *   Bit 0  - Suppress out
 *   Bit 1  -
 *   Bit 2  - MPX Opn lch
 *   Bit 3  - Mpx Share Request.
 *   Bit 4  - Load Ind
 *   Bit 5  - Select In
 *   Bit 6  - Select Out
 *   Bit 7  - MPX Channel interrpt
 */
uint16_t    TI;                 /* 1050 bus in */
uint16_t    TE;                 /* 1050 bus out */
uint16_t    TT;                 /* 1050 tag in */
/* TT Bits.
 *
 * Bit 0 - Cancel
 * Bit 1 - Ready
 * Bit 2 - End
 * Bit 3 - 1050 Operational
 * Bit 4 - Home Start
 * Bit 5 - Intervention Required
 * Bit 6 - Attention
 * Bit 7 - Data Check
 */
uint16_t    TA;                 /* 1050 tag out */
/* TA Bits.
 *
 * Bit 0 - Home Rdr Start Lch
 * Bit 1 - Rdr On Lch
 * Bit 2 - Micro Share
 * Bit 3 - Proceed
 * Bit 4 - Audible Alarm
 * Bit 5 - CR
 * Bit 6 - Atten rst
 * Bit 7 - Rst Lch
 */
uint8_t     JI;                 /* Direct bus in and out */
uint8_t     JE;

int          ch_sel;             /* Select channel 1 or 2. */
int          ch_sav;             /* Save channel pointer for ROS request */

/* GA Functions
 *
 *  Bits independent.
 *  K = 1  Set service out.
 *  K = 2  Set command out.
 *  K = 4  Set Address out.
 *  K = 8  Set Bus out control.
 */

/* GB Functions
 *
 *  K = 0 set GE Bit 2.
 *  K = 1 set select channel based on PK.
 *  K = 2 reset operation out.
 *  K = 3 reset pci flag.
 *  K = 4 set Select channel interrupt.
 *  K = 5 Channel control check.
 *  K = 6 set GR to zero.
 *  K = 7 Cpu-stored.
 *  K = 8 set/reset count ready based on PK.
 *  K = 9 reset channel, PK=1 channel and poll.
 *  K = A set/reset suppress out. based on PK
 *  K = B set/reset poll control based on PK.
 *  K = C reset select out.
 *  K = D channel busy set.
 *  K = E set Halt-I/O latch.
 *  K = F interface control check
 */
uint16_t     GE[2];              /* Selector channel errors. */
/* GE Register.
 *
 *  Bit 0 - PCI
 *  Bit 1 - Incorrect length.
 *  Bit 2 - Prog Check
 *  Bit 3 - Prot Check
 *  Bit 4 - Chnl Data Check
 *  Bit 5 - Chnl Ctrl Check
 *  Bit 6 - Intrf Ctrl Check
 */
uint16_t     GF[2];              /* Selector channel flags. */
/* GF Register.
 *
 *  Bit 0 - CD -> GS4
 *  Bit 1 - CC
 *  Bit 2 - SLI
 *  Bit 3 - Skip
 *  Bit 4 - PIC GEo
 */
uint16_t     GG[2];                /* Selector channel  command register. Bits 4-7. */
/* GG Register.
 *
 *  0000    - Invalid
 *  0100    - Sense
 *  1000    - TIC
 *  1100    - Read Backwards
 *  xx01    - Write
 *  xx10    - Read
 *  xx11    - Control
 */

/* GH functions
 *
 *  K = 0 Select Channel 1 and 2 machine reset.
 *  K = 1 Set diag mode
 *  K = 2 Set diag tag control
 *  K = 12 Set select out.
 */
uint16_t    GI[2];                 /* Selector channel bus in */
uint16_t    GK[2];                 /* Selector channel protection */
uint16_t    GR[2];                 /* Select data in */
uint16_t    GO[2];                 /* Select bus output */
/* GS Bits
 *
 *  Bit 0 - GR Full
 *  Bit 1 - Chain det
 *  Bit 2 - Select Out
 *  Bit 3 - Intrp cond
 *  Bit 4 - CD
 *  Bit 5 - 1 = chan 1, 0 - chan 2
 *  Bit 6 - 0
 *  Bit 7 - 0.  Chain request.
 */

/* GT Tag bits
 *
 *  Bit 0 - Select in
 *  Bit 1 - Service in not Service Out
 *  Bit 2 - Poll Ctrl
 *  Bit 3 - Channel Busy
 *  Bit 4 - Addr in
 *  Bit 5 - Status in
 *  Bit 6 - Intr Lch
 *  Bit 7 - Op in
 */

/* GJ selector internal register, selected by CK.
 *
 *  K = 0
 *  K = 1  GD to Abus
 *  K = 2  GC to Abus
 *  K = 3  GK to Abus
 *  K = 4  GE to Abus
 *  K = 5
 *  K = 6
 *         Bit 0 - Sx1 Cnt Rdy & not zero
 *         Bit 1 - SLI   GE<2>
 *         Bit 2 - COM 7 Bit output
 *         Bit 3 - Cnt Rdy & zero
 *         Bit 4 - 0
 *         Bit 5 - CC   GE<1>
 *         Bit 6 - SXI Rd Bkwd
 *         Bit 7 - Skip  GE<3>
 *  K = 7
 *         Bit 0 - Input
 *         Bit 1 - Suppress Out
 *         Bit 2 - SX1 ROS Req
 *         Bit 3 - Addr Out
 *         Bit 4 - Comd Out
 *         Bit 5 - Serv out
 *         Bit 6 - Bus Out Ctrl
 *         Bit 7 - Op Out
 *  K = 8  GO to Abus
 *  K = 9
 *  K = a
 *  K = b
 *  K = c
 *  K = d
 *  K = e
 *  K = f
 */
uint16_t    GC[2];                /* Select channel count */
uint16_t    GD[2];                /* Select channel count */
uint16_t    GU[2];                /* Select channel address */
uint16_t    GV[2];                /* Select channel address */

uint16_t    GHY;                  /* Incrementer bus. */
uint16_t    GHZ;

uint16_t    SEL_TAGS[2];          /* Select channel tags. */
uint16_t    SEL_TI[2];            /* Input tags. */

struct _device  *console;         /* Console device */

} cpu_2030;

extern uint16_t    end_of_e_cycle;
extern uint16_t    store;
extern uint16_t    allow_write;
extern uint16_t    match;
extern uint16_t    t_request;
extern uint8_t     allow_man_operation;
extern uint8_t     wait;
extern uint8_t     test_mode;
extern uint8_t     clock_start_lch;
extern uint8_t     load_mode;

#define MAIN   1
#define LOCAL  2
#define MPX    4

void  cycle_2030();


/* Select channel I/O sequence.
 *
 *   K->GB of 0001 (PK) selects Channel 1 or 2. PK==1 for 2.
 *   K->GB of 1011,1    set poll.   (Raise Sel out).
 *   GT & 0x20 == 0.   Poll control on to S4.
 *   K->GB of 1001,0   reset channel.
 *   set command out& service out.
 *
 *   K->GA 1100,0     set addr out.
 *   K->GB 1100,0     reset select out.
 *   Wait for Addr in or Status in   Bit 4 = addr in, bit5 = status in
 *      Status in = busy.
 *   check Sel in (bit 0) clear.
 *   Compare address in with device.
 *   Count to GCD Address to GUV. Op to GG.
 *   K->GA 1010,0     command out, drop address out.
 *   Wait status in GT Bit 5.
 *   K->GB 1000,0     set count ready flag.
 *
 *
 *   Status != 0:
 *    K->GB   1010,0   reset suppress out. Stack status.
 *
 *  Interrupt: (8)
 *    H & 0x4
 *    GT & 1010  Check Address in or irq latch.
 *    GT & 1100,0000 Check select in and service in.
 *
 *    Address in, store address
 *    Load GE.
 *
 *
 *
 *
 *
 */
#endif

/*
 * microsim360 - Model 2050 definitions.
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
#include "conf.h"

#ifndef _MODEL50_H_
#define _MODEL50_H_

extern struct ROS_2050 {
    int      io;
    int      lu;
    int      mv;
    int      zp;
    int      zn;
    int      zf;
    int      tr;
    int      zr;
    int      ws;   /* CS, SA field in I/O */
    int      sf;
    int      iv;   /* CT field in I/O */
    int      al;
    int      wm;   /* WL, HC field in I/O */
    int      up;   /* MS field in I/O */
    int      md;   /* MS field in I/O */
    int      lb;   /* CG field in I/O */
    int      mb;   /* CG field in I/O */
    int      dg;   /* MG field in I/O */
    int      ul;
    int      ur;
    int      ce;
    int      lx;
    int      tc;
    int      ry;
    int      ad;   /* CL field in I/O */
    int      ab;
    int      bb;
    int      ux;
    int      ss;
    int      extra;
    uint32_t row1;
    uint32_t row2;
    uint32_t row3;
    uint32_t row4;
    char     note[20];
} ros_2050[4096];

#define R1  1                    /* Start of read cycle */
#define R2  2                    /* Data ready during this cycle */
#define R3  4                    /* Data can be modified in SDR */
#define W1  8                    /* Store SDR in memory */

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
int         count;
uint32_t    LS[64];             /* Local storage */
uint32_t    BUMP[1024];         /* Bump storage */
uint8_t     MP[256];            /* Storage protection flags */
uint8_t     mem_state;          /* Storage cycle state */


uint32_t    mem_max;            /* Maximum memory address - 1 */
uint8_t     io_mode;            /* CPU or I/O mode of operation */
uint32_t    ros_row1;           /* Current ROS data */
uint32_t    ros_row2;
uint32_t    ros_row3;
uint32_t    ros_row4;

uint32_t    right_bus;          /* Right alu input bus */
uint32_t    left_bus;           /* Left alu input bus */
uint32_t    alu_out;            /* Alu output bus */
uint32_t    aob_latch;          /* AOB output latch */
uint8_t     n_s_reg;            /* Next cycle S register */

uint8_t     u_bus;              /* Mover u input */
uint8_t     v_bus;              /* Mover v input */
uint8_t     w_bus;              /* Mover w output */

uint8_t     mvfnc;              /* Mover function register */
uint8_t     io_mvfnc;           /* Mover I/O function register */

uint32_t    R_REG;              /* Right register */
uint32_t    L_REG;              /* Left register */
uint32_t    H_REG;              /* H register */
uint32_t    M_REG;              /* M instruction register */
uint32_t    IA_REG;             /* Instruction pointer register */
uint32_t    SAR_REG;            /* Memory address register */
uint32_t    SA_REG;             /* Buffered memory address */
uint32_t    SDR_REG;            /* Memory data register */
uint8_t     F_REG;              /* F shift output register */
uint8_t     MD_REG;             /* 4 bit register */
uint8_t     MB_REG;             /* Memory byte selector */
uint8_t     LB_REG;             /* Memory byte selector */
uint8_t     ED_REG;             /* Exponent difference register */
uint8_t     G_REG;              /* G1 and G2 counter registers */
uint8_t     J_REG;              /* J register */
uint8_t     Q_REG;              /* Q register */
uint8_t     S_REG;              /* S status bits */
uint8_t     BS_REG;             /* Byte mask regiser */
uint8_t     BI_REG;             /* Byte mask in I/O mode */
uint8_t     IO_REG;             /* IO register. */
uint8_t     ED_STAT;            /* Edit status bits */
uint8_t     LSGNS;              /* Left sign flag */
uint8_t     RSGNS;              /* Right sign flag */
uint8_t     SYLS1;              /* One sylabal opcode */
uint8_t     MASK;               /* Interrupt mask register */
uint8_t     REFETCH;            /* Refetch flag */
uint8_t     LSA;                /* Local storage address */
uint8_t     KEY;                /* Storage key */
uint8_t     AMWP;               /* Flags */
uint8_t     CC;                 /* CC register */
uint8_t     ILC;                /* ILC register */
uint8_t     PMASK;              /* Program Mask */
uint16_t    FN;                 /* LSA Function address */
uint8_t     OPPANEL;            /* Front panel switch state */
uint8_t     IVA;                /* Invalid address flag */
uint32_t    DKEYS;              /* Front panel data keys */
uint32_t    AKEYS;              /* Front panel address keys */

int         AUX_REG;            /* Auxliary register */
uint8_t     CAR;                /* Output carry status */
uint8_t     G1NEG;              /* G1 negative */
uint8_t     G2NEG;              /* G2 negative */

uint8_t     init_mem;           /* Initial memory cycle */
uint8_t     init_bump_mem;      /* Initial bump memory cycle */
uint8_t     bump_mem;           /* Accessing bump storage */
uint8_t     update_d;           /* Update to D register */

uint16_t    ROAR;               /* ROAR address register. */
uint16_t    BROAR;              /* Backup ROAR address. */
uint16_t    PROAR;              /* Previous ROAR address. */
uint16_t    NROAR;              /* Next ROAR address. */
uint8_t     dead_cycle;         /* Dead cycle before interrupt */
uint8_t     rest_cycle;         /* Restore cycle */
uint8_t     s3_set;             /* Set S3 after restore */
uint8_t     break_in;           /* Break in cycle requested */
uint8_t     break_out;          /* In break out cycle */
uint8_t     CAR_back;           /* Backup carry output */
uint8_t     first_cycle;        /* First cycle of breakout */
uint8_t     last_cycle;         /* Last cycle of breakout */
uint8_t     chpostest;          /* Do CHPosTest */
uint8_t     CHCTL;              /* Channel control register */

uint8_t     CH;                 /* Current channel select */
uint8_t     BRC;                /* Branch control register */
uint16_t    BFR1;               /* MPX buffer 1 */
uint16_t    BFR2;               /* MPX buffer 1 */
uint16_t    BUS_IN[4];          /* Channel bus in */
uint16_t    BUS_OUT[4];         /* Channel bus out */
uint16_t    MPX_TAGS;           /* MPX tags */
uint16_t    MPX_TI;             /* MPX tags in */
uint8_t     IBFULL;             /* IB full */
uint8_t     polling[4];         /* Polling state */
uint8_t     MPX_PCI;            /* MPX PCI request */
uint8_t     MPX_LST;            /* Last word for MPX */
uint16_t    ROUTINE[4];         /* Current breaking routine */
uint32_t    IOSTAT[4];          /* I/O Status register */

/* GP Register.
 *
 *  Bit 0   Bitcount high.
 *  Bit 1   Bitcount low.
 *  Bit 2   Last count high.
 *  Bit 3   Last count low.
 *  Bit 4   1 more word.
 *  Bit 5   2 more words.
 *  Bit 6   3 more words.
 */
uint16_t    GP_REG[4];          /* Selector General register */
uint8_t     OP_REG[4];          /* Selector Operator register */
uint8_t     POS_REG[4];         /* Selector Byte position count */
uint16_t    FLAGS_REG[4];       /* Selector Flags register */
/* CHPOS Register.
 *  Bit 0   UA Fetch
 *  Bit 1   CCW1 Type
 *  Bit 2   CCS2 Type
 *  Bit 3   Unit Select
 *  Bit 4   RD Store
 *  Bit 5   WR Fetch
 *  Bit 6   End UP
 *  Bit 7   Compare.
 *  Bit 8   Irpt.
 */
uint8_t     inst_latch;
uint8_t     D1[4];              /* Channel flag D1 */
uint8_t     D2[4];              /* Channel flag D2 */
uint8_t     C1[4];              /* Channel flag C1 */
uint8_t     C2[4];              /* Channel flag C2 */
uint8_t     C3[4];              /* Channel flag C3 */
uint8_t     C4[4];              /* Channel flag C4 */
uint8_t     CD[4];              /* Data chain flag */
uint32_t    B_REG[4];           /* Select channel B regiser */
uint32_t    C_REG[4];           /* Select channel C regiser */
uint8_t     CBI_REG[4];         /* Modify bits for B */
uint8_t     CCI_REG[4];         /* Modify bits for C */
uint8_t     CLI_REG[4];         /* Modify bits for LS */
uint8_t     B_FULL[4];          /* B register full */
uint8_t     C_FULL[4];          /* C register full */
uint8_t     LS_FULL[4];         /* Local storage full */
uint8_t     IF_STOP[4];         /* Stop I/O after count exhausted */
uint16_t    TAGS[4];            /* Channel tags. */
uint16_t    TAGS_IN[4];         /* Channel tags in. */
uint16_t    CHPOS[4];           /* Channel position register */
uint16_t    CHREQ[4];           /* Channel position request register */
uint16_t    CHCLK[4];           /* Channel clock register */
uint8_t     BCHI;               /* Interrupt pending */

} cpu_2050;

void  cycle_2050();
void  step_2050();
struct _device *model2050_init(void *render, uint16_t addr);
int             model2050_create(struct _option *opt);

#endif

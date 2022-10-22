/*
 * microsim360 - Model 2841 definitions.
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

#ifndef _MODEL2841_H_
#define _MODEL2841_H_


extern struct ROS_2841 {
      int    CA;   /* A bus input, includes aa */
      int    CB;   /* B bus input */
      int    CK;   /* Constant */
      int    CL;   /* X7 input select */
      int    CH;   /* X6 input select */
      int    PA;   /* Parity of address */
      int    PS;   /* Parity of CA,CB,CK,CL,CA ALT, PA, CH */
      int    CN;   /* Next address */
      int    PN;   /* Next address parity */
      int    CD;   /* Destination register, includes cda */
      int    CV;   /* Invert B input */
      int    CC;   /* Alu function */
      int    CS;   /* Status */
      int    PC;   /* Parity of CD,CD Alternate, CV, CC, CS, BP */
      int    BP;   /* Bypass ALU */
      char   NOTE[20];
} ros_2841[4096];



#ifndef CROS2841
#include <stdint.h>
#include "device.h"
#include "dasd.h"

struct _2841_context {
    int         addr;               /* Device address */
    int         chan;               /* Channel address */
    int         selected;           /* Device currently selected */
    int         request;            /* Requesting CPU */
    int         opr_in;             /* Raise operation in */
    int         sense;              /* Current sense value */
    int         cmd;                /* Current command */
    int         status;             /* Current bus status */
    int         data;               /* Current byte to send/recieve */
    int         data_rdy;           /* Data is valid */
    int         data_end;           /* Data transfer over */
    int         addressed;          /* Last address out matched */
    int         tr_1;               /* Transfer 1 latch */
    int         tr_2;               /* Transfer 2 latch */
    int         srv_in;             /* Service in */
    int         srv_req;            /* Service request latch */
    int         svc_req;            /* Service received */
    int         steering;           /* Steering latch */
    int         tags;               /* Last bus output tags */
    int         index;              /* Index sensed */

    uint8_t     Abus;               /* Holds the input to the A side of ALU. */
    uint8_t     Bbus;               /* Holds the input to the B side of ALU. */
    uint8_t     Alu_out;            /* Holds output of Alu. */
    uint8_t     carry;              /* Holds previous carry out. */
    uint8_t     d_nzero;            /* D bus not zero */

    /* Serializer/deserializer feeds into this register.
       When read/write word set ST4. */
    uint8_t     DR_REG;             /* Data read register */

    uint8_t     ST_REG;             /* Status register */
/*

   Bit 1            Index pulse.
   Bit 4            Read operation, turned on when data sent to DR.
 */
    uint8_t     OP_REG;             /* Operation code register */
    uint8_t     DW_REG;             /* Data write register */
    uint8_t     UR_REG;             /* Unit address register */
    uint8_t     BX_REG;             /* Code check burst register */
    uint8_t     BY_REG;             /* Code check burst register */
    uint8_t     DH_REG;             /* Data length High register */
    uint8_t     DL_REG;             /* Data length High register */
    uint8_t     FR_REG;             /* Flag register */
    uint8_t     GL_REG;             /* Gap length */
    uint8_t     KL_REG;             /* Key length register */
    uint8_t     ER_REG;
/* ER Register.
   Bit 0            Set if error during/writing - op in resets.
   Bit 1            Follows Address out.
   Bit 2            Set if bus parity error - op in resets.
   Bit 3            Set during short busy.
   Bit 4            Parity error on ALU bus.
   Bit 7            Set on Halt I/O.
*/

   uint8_t      GP_REG;             /* General purpose register */

   uint8_t      SC_REG;             /* Drive attention flags */

   uint8_t      IG_REG;             /* Channel control regiser */
/* IG register.
   Bit 0            Write latch
   Bit 1            Operational in
   Bit 2            Read Latch
   Bit 3            Queued Latch
   Bit 4            Poll enable latch
   Bit 5            Status in
   Bit 6            Present Dev end.
   Bit 7            Address In.
  */
   uint16_t        bus_out;
   uint16_t        WX;                 /* ROAR address register. */

/* FT register.
 * Bit 0            Control
 * Bit 1            Set Cylinder
 * Bit 2            Set Head and Sign
 * Bit 3            Set difference
 * Bit 4            Head advance.
 * Bit 5            unused.
 * Bit 6            unused.
 * Bit 7            2311 select.
 */
   uint8_t         FT;                 /* File tag register */

/* FC register.     Control         Set Cylinder    Set Head   Set Diff
 * Bit 0            Write Gate      track 128       forward     diff 128
 * Bit 1            Read Gate       track 64                    diff 64
 * Bit 2            Seek start      track 32                    diff 32
 * Bit 3            Head reset      track 16                    diff 16
 * Bit 4            Erase Gate      track 8         head 8      diff 8
 * Bit 5            Select head     track 4         head 4      diff 4
 * Bit 6            Return 000      track 2         head 2      diff 2
 * Bit 7            Head adance     track 1         head 1      diff 1
 *                  FT0 & FT4
 */
   uint8_t         FC;                 /* File control register */

/* FS Register.
 * Bit 0            Selected ready
 * Bit 1            Selected online
 * Bit 2            Selected unsafe
 * Bit 3            unused.
 * Bit 4            unused.
 * Bit 5            Selected end of cyl
 * Bit 6            unused.
 * Bit 7            Selected seek incomp
 */
    int              unit_num;          /* Selected unit number */
    struct _dasd_t  *disk[8];           /* Disk drives */

};

void   step_2841(void *data);
void    model2841_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);
struct _device *model2841_init(void *render, uint16_t addr);
#endif
#endif

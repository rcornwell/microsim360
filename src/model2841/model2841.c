/*
 * microsim360 - Model 2841 microcode simulator.
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
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include "logger.h"
#include "device.h"
#include "xlat.h"
#include "model2841.h"

#define STATE_IDLE      0     /* Device in Idle state */
#define STATE_SEL       1     /* Device now selected */
#define STATE_CMD       2     /* Device awaiting command */
#define STATE_INIT_STAT 3     /* Sent init status */
#define STATE_OPR       4     /* Do operation */
#define STATE_OPR_REL   5     /* Operator but release */
#define STATE_REQ       6     /* Request the channel */
#define STATE_DATA_O    7     /* Data out to device */
#define STATE_DATA_I    8     /* Data in  to device */
#define STATE_DATA_END  9     /* Post end of channel usage */
#define STATE_END       10    /* Post ending status */
#define STATE_STACK     11    /* Channel polling */
#define STATE_STACK_SEL 12    /* Stack status select */
#define STATE_STACK_CMD 13    /* Stack command */
#define STATE_STACK_HLD 14    /* Stack hold */
#define STATE_WAIT      15    /* After data transfer wait motion */


                      /* 0    1     2      3    4     5     6     7  */
static char *ca_name[32] =
                      { "0",  "GL", "BY", "BX", "FR", "KL", "DL", "DH",
                       "OP", "GP", "UR", "DW", "DR", "ER", "IE", "IH",
                       "SW","STP", "12", "13", "14", "15", "16", "17",
                       "18", "19", "1A", "1B", "SC", "FS", "OA", "IS"};

static char *cb_name[4] = { "0", "BY", "CK", "DR" };

                      /* 0    1     2      3     4      5      6      7 */
static char *cl_name[16] =
                     {   "0", "1", "ST3", "ST5", "ST7", "D=0", "A>X", "TY1",
                      "SERVO", "SORSP", "SELTO", "OP1", "OP3", "OP5", "Index", "OP7" };

                     /* 0    1     2      3     4      5      6      7 */
static char *ch_name[16] =
                     {  "0", "1", "ST0", "OP6", "ST2", "ST4", "ST6", "TY0",
                      "CK>W", "Carry", "COMMD", "SUPPO", "", "OP0", "OP2", "OP4"};

                     /* 0    1     2     3     4     5     6     7 */
static char *cd_name[32] =
                      { "D", "GL", "BY", "BX", "FR", "KL", "DL", "DH",
                     /* 8    9     10    11    12    13    14    15 */
                      "OP", "GP", "UR", "DW", "DR", "FT", "FC", "IG",
                      "10", "11", "12", "13", "14", "15", "16", "17",
                      "18", "19", "1A", "1B", "1C", "1D", "1E", "1F"};

                     /* 0       1          2         3         4          5       6         7 */
static char *cs_name[16] =
                     { "",      "0->ST0", "1->ST0", "0->ST1", "1->ST1", "0->ST2", "DNST21", "0->ST3",
                     "1->ST3", "0->ST4", "0->ST5", "1->ST5", "0->ST6", "1->ST6", "0->ST7", "1->ST7" };

void
step_2841(void *data)
{
   struct _2841_context   *ctx = (struct _2841_context *)data;
   uint16_t   nextWX = ctx->WX;
   struct ROS_2841  *sal;
   int        carry_in;
   uint16_t   carries;
   int        addr3;
   int        i;
   char       buffer[1024];
   char       tbuf[20];

   /* Walk through all drives and update current position */
   ctx->SC_REG = 0;
   for (i = 0; i < 8; i++) {
        uint8_t    ix;
        if (ctx->disk[i] == NULL)
            continue;
        if ((ctx->UR_REG & (0x80 >> i)) != 0 && (ctx->FT & 0x81) == 0x81 && (ctx->FC & 0x04) != 0) {
            uint8_t   data, am;
            ix = 0;
            data = 0;
            if ((ctx->FC & 0x40) != 0) {
                if (dasd_read_byte(ctx->disk[i], &data, &am, &ix)) {
                    log_disk("Disk read %d %02x\n", i, data);
                    ctx->ST_REG |= BIT4;
                    ctx->DR_REG = data;
                }
            } else if ((ctx->FC & 0x80) != 0) {
                data = ctx->DR_REG;
                am = ctx->FC & 0x01;
                if (dasd_write_byte(ctx->disk[i], &data, &am, &ix)) {
                    log_disk("Disk write %d %02x\n", i, data);
                    ctx->ST_REG |= BIT4;
                }
            } else {
                dasd_step(ctx->disk[i], &ix);
            }
            /* Update index if index detected */
            if ((ctx->ST_REG & BIT1) != 0 && ix)
                ctx->index = 1;
        } else {
           /* If not selected, just keep in sync */
          log_disk("Disk stepper %d\n", i);
            dasd_step(ctx->disk[i], &ix);
        }
        /* Check if drive has attention signal */
        if (dasd_check_attn(ctx->disk[i])) {
            ctx->SC_REG |= 0x80 >> i;
          log_disk("Disk attn %d\n", i);
        }
   }

   sal = &ros_2841[ctx->WX];

   /* Disassemble micro instruction */
   if (log_level & LOG_MICRO) {
       sprintf(buffer, "%s %03X: %02X %s ", sal->NOTE, ctx->WX, sal->CN, ca_name[sal->CA]);

       switch (sal->CC) {
       case 0:
       case 1:
       case 4:
       case 5:
       case 6:  if (sal->CV == 0)
                    strcat(buffer, "+");
                break;
       case 2:  strcat(buffer, "&"); break;
       case 3:  strcat(buffer, "|"); break;
       case 7:  strcat(buffer, "^"); break;
       }
       if (sal->CV == 1)
           strcat(buffer, "-");

       if (sal->CB == 2) {
          sprintf(tbuf, "%02x", sal->CK);
          strcat(buffer, tbuf);
       } else {
          strcat(buffer, cb_name[sal->CB]);
       }
       switch (sal->CC) {
       case 5:
       case 1:  strcat(buffer, "+1");  break;
       case 6:  strcat(buffer, "+C"); break;
       }
       strcat(buffer, "->");
       strcat(buffer, cd_name[sal->CD]);
       if (sal->CC > 3 && sal->CC < 7)
           strcat(buffer, "C");
       if (sal->BP)
           strcat(buffer, " BYPASS");

       sprintf(tbuf, " %02x ", sal->CK);
       strcat(buffer, tbuf);
       strcat(buffer, cs_name[sal->CS]);
       sprintf(tbuf, " %02x ", sal->CN);
       if (sal->CH == 8) {
           sprintf(tbuf, " %x>W ", sal->CK & 0xf);
           strcat(buffer, tbuf);
       } else {
           strcat(buffer, " ");
           strcat(buffer, ch_name[sal->CH]);
           strcat(buffer, " ");
       }
       strcat(buffer, cl_name[sal->CL]);
       strcat(buffer, " ");
       addr3 = sal->CN;
       if (sal->CH == 8)
          addr3 |= (sal->CK & 0xf) << 8;
       else
          addr3 |= (ctx->WX & 0xf00);
       if (sal->CH < 2 || sal->CH == 8) {
          if (sal->CH == 1)
             addr3 |= 2;
          if (sal->CL < 2) {
             if (sal->CL == 1)
                addr3 |= 1;
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
          } else {
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
          }
       } else if (sal->CL < 2) {
             if (sal->CL == 1)
                 addr3 |= 1;
             if (sal->CH > 1) {
                 strcat(buffer, ros_2841[addr3].NOTE);
                 sprintf(tbuf, " %03x ", addr3);
                 strcat(buffer, tbuf);
                 addr3 |= 2;
                 strcat(buffer, ros_2841[addr3].NOTE);
                 sprintf(tbuf, " %03x ", addr3);
                 strcat(buffer, tbuf);
             } else {
                if (sal->CH == 1)
                   addr3 |= 2;
                if (sal->CL == 1)
                   addr3 |= 1;
                strcat(buffer, ros_2841[addr3].NOTE);
                sprintf(tbuf, " %03x", addr3);
                strcat(buffer, tbuf);
             }
       } else {
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 &= ~1;
             addr3 |= 2;
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2841[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
       }
       strcat(buffer, "\n");
       log_micro(buffer);
   }


   /* Base next address. */
   nextWX = (ctx->WX & 0xf00) | sal->CN;

   /* Decode the X6 bit */
   switch (sal->CH) {
   case 0: /* 0 */
           break;
   case 1: /* 1 */
           nextWX |= 0x2;
           break;
   case 2: /* ST0 */
           if ((ctx->ST_REG & BIT0) != 0)
               nextWX |= 0x2;
           break;
   case 3: /* OP6 */
           if ((ctx->OP_REG & BIT6) != 0)
               nextWX |= 0x2;
           break;
   case 4: /* ST2 */
           if ((ctx->ST_REG & BIT2) != 0)
              nextWX |= 0x2;
           break;
   case 5: /* ST4 */
           if ((ctx->ST_REG & BIT4) != 0)
              nextWX |= 0x2;
           break;
   case 6: /* ST6 */
           if ((ctx->ST_REG & BIT6) != 0)
              nextWX |= 0x2;
           break;
   case 7:  /* FILE */  /* 01 for 2311 */
           if (ctx->cur_disk == NULL)
               break;
           switch (ctx->cur_disk->type) {
           case 0: /* 2303 */
           default: /* Unknown */
                   nextWX |= 0x2;
                   break;
           case 1: /* 2311 */
           case 2: /* 2302 */
                   break;
           }
           break;
   case 8:  /* CK>W */
           nextWX = (nextWX & 0xff) | ((sal->CK & 0xf) << 8);
           break;
   case 9:  /* Carry */
           if (ctx->carry)
              nextWX |= 0x2;
           break;
   case 10: /* COMMO */
           if ((ctx->tags & CHAN_CMD_OUT) != 0 || (ctx->ER_REG & BIT7) != 0)
              nextWX |= 0x2;
           break;
   case 11: /* SUPPO */
           if ((ctx->tags & CHAN_SUP_OUT) != 0) // && (ctx->IG_REG & BIT5) != 0)
              nextWX |= 0x2;
           break;
   case 12: /* Unused */
           break;
   case 13: /* OP0 */
           if ((ctx->OP_REG & BIT0) != 0)
              nextWX |= 0x2;
           break;
   case 14: /* OP2 */
           if ((ctx->OP_REG & BIT2) != 0)
              nextWX |= 0x2;
           break;
   case 15: /* OP4 */
           if ((ctx->OP_REG & BIT4) != 0)
              nextWX |= 0x2;
           break;
   }

   /* Decode the X7 bit */
   switch (sal->CL) {
   case 0: /* 0 */
           break;
   case 1: /* 1 */
           nextWX |= 0x1;
           break;
   case 2: /* ST3 */
           if ((ctx->ST_REG & BIT3) != 0)
              nextWX |= 0x1;
           break;
   case 3: /* ST5 */
           if ((ctx->ST_REG & BIT5) != 0)
              nextWX |= 0x1;
           break;
   case 4: /* ST7 */
           if ((ctx->ST_REG & BIT7) != 0)
              nextWX |= 0x1;
           break;
   case 5: /* D==0 */
           if (ctx->d_nzero == 0)
              nextWX |= 0x1;
           break;
   case 6: /* A>X */
           break;
   case 7: /*  File */
           if (ctx->cur_disk == NULL)
                break;
           switch (ctx->cur_disk->type) {
           case 0: /* 2303 */
           case 2: /* 2302 */
                   break;
           case 1: /* 2311 */
           default: /* Unknown */
                   nextWX |= 0x1;
                   break;
           }
           break;
   case 8: /* SERVO */
           if ((ctx->tags & CHAN_SRV_OUT) != 0)
              nextWX |= 0x1;
           break;
   case 9: /* SORSP */
           if ((ctx->srv_in && (ctx->IG_REG & BIT2) != 0) ||
               (ctx->srv_req && (ctx->IG_REG & BIT2) != 0) ||
               (ctx->srv_in && (ctx->tags & CHAN_SRV_OUT) != 0))
              nextWX |= 0x1;
           break;
   case 10: /* SELTO */
           if (ctx->selected) //  || ctx->SC_REG != 0)
              nextWX |= 0x1;
//           if (ctx->SC_REG != 0 && ctx->opr_in == 0)
 //             ctx->request = 1;
           break;
   case 11: /* OP1 */
           if ((ctx->OP_REG & BIT1) != 0)
              nextWX |= 0x1;
           break;
   case 12: /* OP3 */
           if ((ctx->OP_REG & BIT3) != 0)
              nextWX |= 0x1;
           break;
   case 13: /* OP5 */
           if ((ctx->OP_REG & BIT5) != 0)
              nextWX |= 0x1;
           break;
   case 14: /* index */
           /* Check for index */
           if (ctx->index)
              nextWX |= 0x1;
           break;
   case 15: /* OP7 */
           if ((ctx->OP_REG & BIT7) != 0)
              nextWX |= 0x1;
           break;
   }

   /* Set B Bus input */
   switch (sal->CB) {
   case 0:       /* None */
          ctx->Bbus = 0;
          break;
   case 1:       /* BY */
          ctx->Bbus = ctx->BY_REG;
          break;
   case 2:       /* CK */
          ctx->Bbus = sal->CK;
          break;
   case 3:       /* DR */
          ctx->Bbus = ctx->DR_REG;
          break;
   }


   /* Gate register to A Bus */
   switch (sal->CA) {
   case 0x00:    /* none */
   case 0x12:
   case 0x13:
   case 0x14:
   case 0x15:
   case 0x16:
   case 0x17:
   case 0x18:
   case 0x19:
   case 0x1A:
   case 0x1B:
          ctx->Abus = 0;
          break;
   case 0x01:    /* GL */
          ctx->Abus = ctx->GL_REG;
          break;
   case 0x02:    /* BY */
          ctx->Abus = ctx->BY_REG;
          break;
   case 0x03:    /* BX */
          ctx->Abus = ctx->BX_REG;
          break;
   case 0x04:    /* FR */
          ctx->Abus = ctx->FR_REG;
          break;
   case 0x05:    /* KL */
          ctx->Abus = ctx->KL_REG;
          break;
   case 0x06:    /* DL */
          ctx->Abus = ctx->DL_REG;
          break;
   case 0x07:    /* DH */
          ctx->Abus = ctx->DH_REG;
          break;
   case 0x08:    /* OP */
          ctx->Abus = ctx->OP_REG;
          break;
   case 0x09:    /* GP */
          ctx->Abus = ctx->GP_REG;
          break;
   case 0x0A:    /* UR */
          ctx->Abus = ctx->UR_REG;
          break;
   case 0x0B:    /* DW */
          ctx->Abus = ctx->DW_REG;
          break;
   case 0x0C:    /* DR */
          ctx->Abus = ctx->DR_REG;
          /* Set transfer control 1 if read */
          if ((ctx->IG_REG & BIT2) != 0) {
              ctx->tr_1 = 1;
              log_trace("Set TR1\n");
          }
          break;
   case 0x0D:    /* ER */
          ctx->Abus = ctx->ER_REG;
          ctx->srv_in = 0;
          break;
   case 0x0E:    /* IE */
          /* Drive interface register */
          ctx->Abus = 0;
          if (ctx->cur_disk == NULL)
              break;
          switch (ctx->cur_disk->type) {
          case 0: /* 2303 */
                  if (ctx->FT & BIT6)
                      ctx->Abus = BIT6|BIT3;
                  break;
          case 1: /* 2311 */
                  if (ctx->FT & BIT7)
                      ctx->Abus = BIT7;
                  break;
          case 2: /* 2302 */
                  if (ctx->FT & BIT5)
                      ctx->Abus = BIT5;
                  break;
          default: /* Invalid type */
                  break;
          }
          break;
   case 0x0F:    /* IH */
          ctx->Abus = ctx->bus_out;
          ctx->tr_1 = 1;
          log_trace("Set TR1 read IH\n");
          break;
   case 0x10:    /* SW */
          /* Controller switches */
          ctx->Abus = 0;
          break;
   case 0x11:    /* Stop */
          break;
   case 0x1C:    /* SC */
          /* Drive attention register */
          ctx->Abus = ctx->SC_REG;
          break;
   case 0x1D:    /* FS */
          /* Drive status register */
          ctx->Abus = 0;
          if (ctx->cur_disk == NULL)
              break;
          ctx->Abus = dasd_gettags(ctx->cur_disk);
          break;
   case 0x1E:    /* OA */
          if (ctx->cur_disk == NULL)
              break;
          /* Drive old address register */
          ctx->Abus = dasd_cur_cyl(ctx->cur_disk);
          break;
   case 0x1F:    /* IS */
          /* Drive interface register */
          ctx->Abus = 0;
          if (ctx->cur_disk == NULL)
              break;
          ctx->Abus = dasd_gettags(ctx->cur_disk);
          if ((ctx->Abus & (BIT0|BIT1)) == (BIT0|BIT1))
              ctx->Abus = BIT2|BIT3;
          break;
   }

   if (sal->CL == 6) {
       nextWX = (nextWX & 0xf00) | (ctx->Abus);
   }

   /* Do Alu operation */
   carries = 0;
   if (sal->CV)
       ctx->Bbus = ~ctx->Bbus;

   /* Set carry in based on CC */
   switch (sal->CC) {
   case 6:
           carry_in = (ctx->ST_REG & BIT3) != 0;
           break;
   case 1:
   case 5:
           carry_in = 1;
           break;
   case 0:
   case 4:
   case 2:
   case 3:
   case 7:
           carry_in = 0;
           break;
   }

   /* Preform alu function */
   carries = 0;
   switch (sal->CC) {
   case 6:
   case 1:
   case 5:
   case 0:
   case 4:
           /* Compute final sum */
           ctx->Alu_out = ctx->Abus + ctx->Bbus + carry_in;
           /* Compute bit carries */
           carries = ((ctx->Abus & ctx->Bbus) | ((ctx->Abus ^ ctx->Bbus) & ~ctx->Alu_out));
           break;
   case 2:
           ctx->Alu_out = ctx->Abus & ctx->Bbus;
           break;
   case 3:
           ctx->Alu_out = ctx->Abus | ctx->Bbus;
           break;
   case 7:
           ctx->Alu_out = ctx->Abus ^ ctx->Bbus;
           break;
   }

   ctx->d_nzero = (ctx->Alu_out != 0);
   ctx->carry = (carries & 0x80) != 0;

   /* If bypass set result to A bus */
   if (sal->BP) {
      ctx->Alu_out = ctx->Abus;
   }

   /* Save results into destination */
   switch (sal->CD) {
   case 0:  /* D */
           break;
   case 1:  /* GL */
           ctx->GL_REG = ctx->Alu_out;
           break;
   case 2: /* BY */
           ctx->BY_REG = ctx->Alu_out;
           break;
   case 3: /* BX */
           ctx->BX_REG = ctx->Alu_out;
           break;
   case 4: /* FR */
           ctx->FR_REG = ctx->Alu_out;
           break;
   case 5: /* KL */
           ctx->KL_REG = ctx->Alu_out;
           break;
   case 6:  /* DL */
           ctx->DL_REG = ctx->Alu_out;
           break;
   case 7:  /* DH */
           ctx->DH_REG = ctx->Alu_out;
           break;
   case 8:  /* OP */
           ctx->OP_REG = ctx->Alu_out;
           break;
   case 9:  /* GP */
           ctx->GP_REG = ctx->Alu_out;
           break;
   case 10: /* UR */
           ctx->UR_REG = ctx->Alu_out;
           ctx->cur_disk = NULL;
           for (i = 0; i < 8; i++) {
               if ((ctx->UR_REG & (0x80 >> i)) != 0) {
                   ctx->unit_num = i;
                   ctx->cur_disk = ctx->disk[i];
                   break;
               }
           }
           break;
   case 11:  /* DW */
           ctx->DW_REG = ctx->Alu_out;
           break;
   case 12:  /* DR */
           ctx->DR_REG = ctx->Alu_out;
           break;
   case 13:  /* FT */
           /* Drive FT register */
           ctx->FT &= ~ctx->Alu_out;
           if (sal->CN & 4)
               ctx->FT |= ctx->Alu_out;
           if (ctx->cur_disk == NULL)
              break;
           dasd_settags(ctx->cur_disk, ctx->FT, ctx->FC);

           /* Check if enabling read/write gate */
           if (ctx->FT == 0x81 && (ctx->FC & 0xc0) != 0) {
              /* Adjust our position */
               dasd_update(ctx->cur_disk);
           }
           break;
   case 14:  /* FC */
           /* Drive FC register */
           ctx->FC &= ~ctx->Alu_out;
           if (sal->CN & 4)
               ctx->FC |= ctx->Alu_out;
           if (ctx->cur_disk == NULL)
              break;
           dasd_settags(ctx->cur_disk, ctx->FT, ctx->FC);
           break;
   case 15:  /* IG */
           ctx->IG_REG = ctx->Alu_out;
           if ((ctx->IG_REG & BIT0) != 0 && ctx->srv_in == 0) {
               ctx->svc_req = 1;
               log_trace("Raise svc request %d\n", ctx->svc_req);
           }
           break;
   }

   /* Set carry based on CC */
   switch (sal->CC) {
   case 0:
   case 1:
   case 2:
   case 3:
   case 7:
            break;
   case 4:
   case 5:
   case 6:
           if (ctx->carry)
               ctx->ST_REG |= BIT3;
           else
               ctx->ST_REG &= ~BIT3;
           break;
   }

   /* Update static flags */
   switch (sal->CS) {
   case 0x00:  /* Not used */
           break;
   case 0x01:  /* 0 > ST0 */
           ctx->ST_REG &= ~BIT0;
           break;
   case 0x02:  /* 1 > ST0 */
           ctx->ST_REG |= BIT0;
           break;
   case 0x03:  /* 0 > ST1 */
           ctx->ST_REG &= ~BIT1;
           ctx->index = 0;
           break;
   case 0x04:  /* 1 -> ST1 */
           ctx->ST_REG |= BIT1;
           break;
   case 0x05:  /* 0 > ST2 */
           ctx->ST_REG &= ~(BIT2);
           break;
   case 0x06:  /* DNST21, 1>ST2 if D != 0 */
           if (ctx->d_nzero)
               ctx->ST_REG |= (BIT2);
           break;
   case 0x07:  /* 0 > ST3 */
           ctx->ST_REG &= ~BIT3;
           break;
   case 0x08:  /* 1 > ST3 */
           ctx->ST_REG |= BIT3;
           break;
   case 0x09:  /* 0 > ST4 */
           ctx->ST_REG &= ~(BIT4);
           break;
   case 0x0A: /* 0 > ST5 */
           ctx->ST_REG &= ~(BIT5);
           break;
   case 0x0B: /* 1 > ST5 */
           ctx->ST_REG |= BIT5;
           break;
   case 0x0C: /* 0 > ST6 */
           ctx->ST_REG &= ~(BIT6);
           break;
   case 0x0D: /* 1 > ST6 */
           ctx->ST_REG |= BIT6;
           break;
   case 0x0E: /* 0 > ST7 */
           ctx->ST_REG &= ~(BIT7);
           break;
   case 0x0F: /* 1 > ST7 */
           ctx->ST_REG |= BIT7;
           break;
   }

   ctx->WX = nextWX;

   log_reg("OP=%02x DW=%02x UR=%02x BX=%02x BY=%02x DH=%02x DL=%02x FR=%02x GL=%02x SC=%02x WX=%03x %d\n",
           ctx->OP_REG, ctx->DW_REG, ctx->UR_REG, ctx->BX_REG, ctx->BY_REG,
           ctx->DH_REG, ctx->DL_REG, ctx->FR_REG, ctx->GL_REG, ctx->SC_REG, ctx->WX, ctx->selected);
   log_reg("KL=%02x ER=%02x GP=%02x IG=%02x DR=%02x ST=%02x FT=%02x FC=%02x A=%02x B=%02x > %02x %x\n",
           ctx->KL_REG, ctx->ER_REG, ctx->GP_REG, ctx->IG_REG, ctx->DR_REG,
           ctx->ST_REG, ctx->FT, ctx->FC, ctx->Abus, ctx->Bbus, ctx->Alu_out, ctx->carry);
}


void
model2841_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2841_context *ctx = (struct _2841_context *)unit->dev;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Disk", 0, *tags, bus_out);
        last_tags = *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (ctx->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        ctx->selected = 0;
        ctx->addressed = 0;
        ctx->WX = 0;
        return;
    }

    ctx->bus_out = bus_out & 0xff;
    ctx->tags = *tags;

    /* Check if requesting device */
//    if ((*tags & (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_SEL_OUT)) == 
//                 (CHAN_OPR_OUT|CHAN_SEL_OUT) && 
      if ((ctx->IG_REG & BIT3) != 0) {
        ctx->request = 1;
//        *tags |= CHAN_REQ_IN;
//        ctx->selected = 1;
    }

log_trace("IG_REG=%02x\n", ctx->IG_REG);
    if (ctx->IG_REG & BIT7) {
        *tags |= CHAN_ADR_IN;
       *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
        ctx->opr_in = 1;
        ctx->tr_1 = 0;
        ctx->addressed = 1;
    } else {
       *tags &= ~CHAN_ADR_IN;
    }

    if ((*tags & CHAN_ADR_OUT) != 0) {
        if ((bus_out & 0xf0) == ctx->addr) {
            /* Respond with busy if status in still raised */
            if ((ctx->IG_REG & BIT5) != 0) {
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                *tags |= CHAN_STA_IN;
                log_trace("Unit busy\n");
                ctx->ER_REG |= BIT3|BIT7;
            } else {
                ctx->addressed = 1;
                ctx->ER_REG |= BIT1;
                log_trace("Addressed\n");
            }
        } else {
            ctx->addressed = 0;
            log_trace("Not Addressed %03x\n", ctx->addr);
        }
        if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
            ctx->ER_REG |= BIT2;
            log_trace("Address parity error\n");
        }
    } else {
        ctx->ER_REG &= ~BIT1;
    }

    if ((ctx->IG_REG & BIT1) != 0) {
log_trace("Drop Op in\n");
        ctx->opr_in = 0;
        ctx->selected = 0;
        ctx->addressed = 0;
        ctx->IG_REG &= ~BIT1;
        *tags &= ~CHAN_OPR_IN;
        ctx->ER_REG &= ~BIT7;
    }

    if ((*tags & CHAN_SEL_OUT) != 0 && ctx->addressed) {
        ctx->selected = 1;
log_trace("Set selected\n");
    } else {
log_trace("Drop selected\n");
        ctx->selected = 0;
    }

    if (ctx->opr_in) {
       *tags |= CHAN_OPR_IN;
    }

    /* If request, enable request in */
    if (ctx->selected == 0) {
       if ((ctx->IG_REG & BIT6) != 0) {
           ctx->request = 1;
       }

       /* If Polling and attention pending, generate request in */
       if ((ctx->IG_REG & BIT4) != 0 && ctx->SC_REG != 0) {
           ctx->request = 1;
       }

       if (ctx->request) {
           *tags |= CHAN_REQ_IN;
       }

       if ((ctx->IG_REG & BIT5) != 0 && 
          (*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN|CHAN_SRV_OUT)) {
          *tags &= ~CHAN_STA_IN;
       }
    }


    if (ctx->request && (*tags & (CHAN_REQ_IN|CHAN_SUP_OUT|CHAN_SEL_OUT)) == (CHAN_REQ_IN|CHAN_SEL_OUT)) {
       *tags &= ~(CHAN_REQ_IN);
       ctx->request = 0;
       ctx->selected = 1;
    }

    /* Present end status */
    if (ctx->selected == 0 && (ctx->IG_REG & BIT5) != 0) {
        print_tags("Disk", 0, *tags, bus_out);
        /* Wait for channel to request a poll */
        if ((*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_REQ_IN)) == (CHAN_SEL_OUT|CHAN_REQ_IN)) {
            *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
            *tags |= (CHAN_OPR_IN|CHAN_ADR_IN);
            /* Send address */
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
            ctx->tr_1 = 0;
            ctx->tr_2 = 0;
            ctx->addressed = 1;
        }
#if 0
        if ((*tags & (CHAN_OPR_IN)) == (CHAN_OPR_IN)) {
            *tags |= (CHAN_STA_IN);
            /* Send address */
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
            ctx->tr_1 = 0;
            ctx->tr_2 = 0;
            ctx->selected = 1;
        }
#endif

#if 0
        /* Wait for channel to accept it with CMD out */
        if ((*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN))
                    == (CHAN_SEL_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
            *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN|CHAN_ADR_IN);
            *tags |= (CHAN_OPR_IN|CHAN_ADR_IN);
            ctx->tr_1 = 1;
        }

        /* Wait for CMD out to drop */
        if (ctx->tr_1 && (*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN)) ==
                   (CHAN_SEL_OUT|CHAN_OPR_IN)) {
            *tags &= ~(CHAN_SEL_OUT);
            *tags |= (CHAN_OPR_IN|CHAN_STA_IN);
            /* Place status on bus */
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
           ctx->tr_1 = 0;
           ctx->tr_2 = 1;
        }

        /* Wait for CMD out or Service out */
        if (ctx->tr_2 &&
            (*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_CMD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) ==
                     (CHAN_SEL_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
             /* stacked status */
            *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN|CHAN_OPR_IN);
        }

        /* Wait for CMD out or Service out */
        if (ctx->tr_2 &&
            (*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_CMD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) ==
                     (CHAN_SEL_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
             /* status accepted */
            *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN|CHAN_OPR_IN);
        }

        /* Wait for channel to be idle */
        if ((*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_OPR_IN)) == 0) {
            *tags |= CHAN_REQ_IN;
        }
#endif
    }


    if (ctx->selected) {
#if 0
         if ((*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN|CHAN_SRV_OUT) &&
                 (ctx->IG_REG & BIT5) == 0) {
log_trace("Drop selct\n");
            ctx->addressed = 0;
         }
#endif
         *tags &= ~(CHAN_SEL_OUT);

         if (ctx->IG_REG & BIT7) {
             *tags |= CHAN_ADR_IN;
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
             ctx->opr_in = 1;
             ctx->tr_1 = 0;
         } else {
             *tags &= ~CHAN_ADR_IN;
         }

         if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
             log_trace("Data parity error\n");
             ctx->ER_REG |= BIT2;
         } else {
             ctx->ER_REG &= ~BIT2;
         }

log_trace("TR1=%d TR2=%d SVC=%d SVI=%d\n", ctx->tr_1, ctx->tr_2, ctx->svc_req, ctx->srv_in);
         ctx->tr_2 = ctx->svc_req;
         if (ctx->srv_in) {
             ctx->svc_req = 0;
log_trace("Clear svc request\n");
         }

         if (((ctx->IG_REG & BIT2) != 0 && ctx->tr_1) ||
             ((ctx->IG_REG & BIT0) != 0 && ctx->srv_in == 0)) {
             ctx->svc_req = 1;
log_trace("Raise svc request %d\n", ctx->svc_req);
         }

         if (ctx->tr_2) {
             ctx->srv_in = 1;
             *tags |= CHAN_SRV_IN;
             *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
log_trace("Raise Service in\n");
         }

         if ((ctx->tr_1 && (ctx->IG_REG & BIT2) == 0) ||
             ((ctx->IG_REG & BIT2) != 0 && (*tags & CHAN_SRV_OUT) != 0 && ctx->tr_2 == 0)) {
             ctx->srv_in = 0;
             *tags &= ~CHAN_SRV_IN;
log_trace("Clear Service in\n");
         }
         ctx->tr_1 = 0;
#if 0
         if ((ctx->tr_1 && (ctx->IG_REG & BIT2) == 0) ||
             ((ctx->IG_REG & BIT2) != 0 && (*tags & CHAN_SRV_OUT) != 0 && ctx->tr_2 == 0)) {
log_trace("Clear srv_in\n");
             ctx->srv_in = 0;
         }

         if (ctx->srv_in) {
log_trace("clear srv_req, raise service in\n");
             ctx->srv_req = 0;
             *tags |= CHAN_SRV_IN;
             *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
             ctx->bus_out = bus_out & 0xff;
         } else {
log_trace("clear service in\n");
             *tags &= ~CHAN_SRV_IN;
         }

         if ((ctx->tr_1 && (ctx->IG_REG & BIT2) != 0) ||
             ((ctx->IG_REG & BIT0) != 0 && !ctx->srv_in)) {
log_trace("Raise serv request\n");
             ctx->srv_req = 1;
         }

         if (ctx->tr_2 && (*tags & CHAN_SRV_OUT) == 0) {
log_trace("Raise srv_in\n");
             ctx->srv_in = 1;
//             *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
 //            *tags |= CHAN_SRV_IN;
         }
#endif

         /* If status latch */
         if ((ctx->IG_REG & BIT5) != 0) {
             *tags |= CHAN_STA_IN;
             *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
             ctx->ER_REG &= ~BIT3;
         } else {
             *tags &= ~CHAN_STA_IN;
         }

//         if ((*tags & (CHAN_ADR_OUT|CHAN_OPR_IN)) == (CHAN_ADR_OUT|CHAN_OPR_IN)) {
 //            ctx->ER_REG |= BIT7;
  //       }

#if 0
         if (ctx->tr_2) {
            *tags |= CHAN_SRV_IN;
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
            ctx->bus_out = bus_out & 0xff;
            if ((*tags & CHAN_SRV_OUT) != 0) {
                ctx->tr_2 = 0;
            }
         }
#endif
    }
}

struct _device *
model2841_init(void *rend, uint16_t addr)
{
 //    SDL_Surface *text;
 //    SDL_Renderer *render = (SDL_Renderer *)rend;
     int     i;
     struct _device *dev2841;
     struct _2841_context *ctx;

     if ((dev2841 = (struct _device *)calloc(1, sizeof(struct _device))) == NULL)
         return NULL;

     if ((ctx = (struct _2841_context *)calloc(1, sizeof(struct _2841_context))) == NULL) {
         free (dev2841);
         return NULL;
     }

     dev2841->bus_func = &model2841_dev;
     dev2841->dev = (void *)ctx;
     dev2841->draw_model = (void *)NULL;
     dev2841->create_ctrl = (void *)NULL;
     dev2841->rect[0].x = 0;
     dev2841->rect[0].y = 0;
     dev2841->rect[0].w = 305;
     dev2841->rect[0].h = 142;
     dev2841->n_units = 1;
     dev2841->addr = addr;
     ctx->addr = addr;
     ctx->WX = 0;
     for (i = 0; i < 8; i++)
         ctx->disk[i] = NULL;
     add_chan(dev2841, addr);
     add_disk(&step_2841, (void *)ctx);
     return dev2841;
}

int
model2841_create(struct _option *opt)
{
     struct  _device *dev2841;
     struct  _2841_context *ctx;
     int              i;

     if ((dev2841 = (struct _device *)calloc(1, sizeof(struct _device))) == NULL)
         return 0;

     if ((ctx = (struct _2841_context *)calloc(1, sizeof(struct _2841_context))) == NULL) {
         free (dev2841);
         return 0;
     }

     dev2841->bus_func = &model2841_dev;
     dev2841->dev = (void *)ctx;
     dev2841->draw_model = (void *)&model2311_draw;
     dev2841->create_ctrl = (void *)&model2311_control;
     dev2841->n_units = 8;
     ctx->addr = opt->addr & 0xff;;
     ctx->chan = (opt->addr >> 8) & 0x7;
     ctx->WX = 0;
     for (i = 0; i < dev2841->n_units; i++) {
         ctx->disk[i] = NULL;
         dev2841->rect[i].x = 0;
         dev2841->rect[i].y = 0;
         dev2841->rect[i].w = 0;
         dev2841->rect[i].h = 0;
     }
     add_chan(dev2841, opt->addr);
     add_disk(&step_2841, (void *)ctx);
     return 1;
}

int
model2311_create(struct _option *opt)
{
     struct  _device *dev2841;
     struct  _2841_context *ctx;
     struct _option   opts;
     int              i;
     char            *file;
     int              fmt;
     char            *vol;
     int              t;

     dev2841 = find_chan(opt->addr, 0xf8);
     if (dev2841 == NULL) {
         fprintf(stderr, "Device not found %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     i = opt->addr & 0x7;
     ctx = (struct _2841_context *)dev2841->dev;
     if (ctx->disk[i] != NULL) {
         fprintf(stderr, "Duplicate device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     ctx->disk[i] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
     if (ctx->disk[i] == NULL) {
         fprintf(stderr, "Unable to create device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     if (dasd_settype(ctx->disk[i], "2311") == 0) {
         fprintf(stderr, "Unknown type %s %03x\n", opt->opt, opt->addr);
         free(ctx->disk[i]);
         ctx->disk[i] = NULL;
         return 0;
     }
     file = NULL;
     vol = NULL;
     fmt = 0;
     while (get_option(&opts)) {
         if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
             file = strdup(opts.string);
         } else if (strcmp(opts.opt, "FORMAT") == 0) {
             fmt = 1;
         } else if (strcmp(opts.opt, "VOLID") == 0) {
             vol = strdup(opts.string);
         } else {
             fprintf(stderr, "Invalid option %s to 2415 Unit\n", opts.opt);
             free(ctx->disk[i]);
             ctx->disk[i] = NULL;
             return 0;
         }
     }
     dev2841->rect[i].x = 0;
     dev2841->rect[i].y = 0;
     dev2841->rect[i].w = 180;
     dev2841->rect[i].h = 100;
     if (vol != NULL) {
         dasd_setvolid(ctx->disk[i], vol);
         free(vol);
     }
     if (file != NULL) {
         if (dasd_attach(ctx->disk[i], file, fmt) == 0) {
             log_warn("Unable to open file %s\n", file);
         }
         free(file);
     }
     return 1;
}

int
model2302_create(struct _option *opt)
{
     struct  _device *dev2841;
     struct  _2841_context *ctx;
     int              i;

     dev2841 = find_chan(opt->addr, 0xf8);
     if (dev2841 == NULL) {
         fprintf(stderr, "Device not found %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     i = opt->addr & 0x7;
     ctx = (struct _2841_context *)dev2841->dev;
     if (ctx->disk[i] != NULL) {
         fprintf(stderr, "Duplicate device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     ctx->disk[i] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
     if (ctx->disk[i] != NULL) {
         fprintf(stderr, "Unable to create device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     if (dasd_settype(ctx->disk[i], "2302") == 0) {
         fprintf(stderr, "Unknown type %s %03x\n", opt->opt, opt->addr);
         free(ctx->disk[i]);
         ctx->disk[i] = NULL;
         return 0;
     }
     return 1;
}


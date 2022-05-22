/*
 * microsim360 - Model 2844 microcode simulator.
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
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include "logger.h"
#include "device.h"
#include "xlat.h"
#include "model2844.h"

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
                      { "0",  "GL", "BY", "IH", "FR", "KL", "DL", "DH",
                       "OP", "GP", "SP", "DW", "WH", "WL", "SW", "BC",
                       "STP","SC", "FS", "BX", "DR", "ER", "IE", "OA",
                       "CX", "IS", "UR", "SL", "AH", "AL", "BH", "BL"};

static char *cb_name[4] = { "0", "BY", "CK", "DR" };

                      /* 0    1     2      3     4      5      6      7 */
static char *cl_name[16] =
                     {   "0", "1", "ST3", "ST5", "ST7", "D=0", "A>X", "INLIN",
                      "SERVO", "SORSP", "SELTO", "OP1", "OP3", "OP5", "Index", "OP7" };

                     /* 0    1     2      3     4      5      6      7 */
static char *ch_name[16] =
                     {  "0", "1", "ST0", "OP6", "ST2", "ST4", "ST6", "BUF",
                      "CK>W", "Carry", "COMMD", "SUPPO", "ADCPR", "OP0", "OP2", "OP4"};

                     /* 0    1     2     3     4     5     6     7 */
static char *cd_name[32] =
                      { "D", "GL", "BY", "03", "FR", "KL", "DL", "DH",
                     /* 8    9     10    11    12    13    14    15 */
                      "OP", "GP", "UR", "DW", "DR", "FT", "FC", "IG",
                      "10", "BT", "WH", "WL", "AH", "AL", "BH", "BL",
                      "CX", "BX", "SP", "SW", "IE", "1D", "1E", "1F"};

                     /* 0       1          2         3         4          5       6         7 */
static char *cs_name[16] =
                     { "",      "0->ST4", "1->ST1", "0->ST1", "0->ST0", "1->ST0", "0->ST5", "1->ST5",
                     "0->ST2", "DNST21", "0->ST3", "1->ST3", "0->ST6", "1->ST6", "0->ST7", "1->ST7" };

void
step_2844(struct _2844_context *ctx)
{
   uint16_t   nextWX = ctx->WX;
   struct ROS_2844  *sal;
   int        carry_in;
   uint16_t   carries;
   int        addr3;
   int        i;
   char       buffer[1024];
   char       tbuf[20];

   sal = &ros_2844[nextWX];

   /* Disassemble micro instruction */
   if (log_level & LOG_MICRO) {
       sprintf(buffer, "%s %03X: %02X %s ", sal->NOTE, nextWX, sal->CN, ca_name[sal->CA]);
      
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
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
          } else {
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
          }
       } else if (sal->CL < 2) {
             if (sal->CH > 1) {
                 strcat(buffer, ros_2844[addr3].NOTE);
                 sprintf(tbuf, " %03x ", addr3);
                 strcat(buffer, tbuf);
                 addr3 |= 2;
                 strcat(buffer, ros_2844[addr3].NOTE);
                 sprintf(tbuf, " %03x ", addr3);
                 strcat(buffer, tbuf);
             } else {
                if (sal->CH == 1)
                   addr3 |= 2;
                if (sal->CL == 1)
                   addr3 |= 1;
                strcat(buffer, ros_2844[addr3].NOTE);
                sprintf(tbuf, " %03x", addr3);
                strcat(buffer, tbuf);
             }
       } else {
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 &= ~1;
             addr3 |= 2;
             strcat(buffer, ros_2844[addr3].NOTE);
             sprintf(tbuf, " %03x ", addr3);
             strcat(buffer, tbuf);
             addr3 |= 1;
             strcat(buffer, ros_2844[addr3].NOTE);
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
   case 7:  /* BUF */
              nextWX |= 0x2;
           break;
   case 8:  /* CK>W */
           nextWX = (nextWX & 0xff) | ((sal->CK & 0xf) << 8);
           break;
   case 9:  /* Carry */
           if (ctx->carry)
              nextWX |= 0x2;
           break;
   case 10: /* COMMO */
           if ((ctx->tags & CHAN_CMD_OUT) != 0)
              nextWX |= 0x2;
           break;
   case 11: /* SUPPO */
           if ((ctx->tags & CHAN_SUP_OUT) != 0)
              nextWX |= 0x2;
           break;
   case 12: /* ADCPR */
              nextWX |= 0x2;
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
           nextWX = (nextWX & 0xf00) | (ctx->Abus);
           break;
   case 7: /* INLIN */
              nextWX |= 0x1;
           break;
   case 8: /* SERVO */
           if ((ctx->tags & CHAN_SRV_OUT) != 0)
              nextWX |= 0x1;
           break;
   case 9: /* SORSP */
           if (ctx->tr_1 == 0 && ctx->tr_2 == 0 && (ctx->IG_REG & 0x20) != 0)  /* System not taken byte */
              nextWX |= 0x1;
           break;
   case 10: /* SELTO */
           if (ctx->selected)
              nextWX |= 0x1;
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
              nextWX |= 0x1;
           break;
   case 15: /* OP7 */
           if ((ctx->OP_REG & BIT7) != 0)
              nextWX |= 0x1;
           break;
   }

   ctx->WX = nextWX;

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
          ctx->Abus = 0;
          break;
   case 0x01:    /* GL */
          ctx->Abus = ctx->GL_REG;
          break;
   case 0x02:    /* BY */
          ctx->Abus = ctx->BY_REG;
          break;
   case 0x03:    /* IH */
          ctx->Abus = ctx->bus_out;
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
   case 0x0A:    /* SP */
          ctx->Abus = ctx->SP_REG;
          break;
   case 0x0B:    /* DW */
          ctx->Abus = ctx->DW_REG;
          break;
   case 0x0C:    /* WH */
          ctx->Abus = ctx->WH_REG;
          break;
   case 0x0D:    /* WL */
          ctx->Abus = ctx->WL_REG;
          break;
   case 0x0E:    /* SW */
          /* Drive interface register */
          break;
   case 0x0F:    /* BC */
          ctx->Abus = ctx->BC_REG;
          break;
   case 0x10:    /* STOP */
          /* Controller switches */
          ctx->Abus = 0;
          break;
   case 0x11:    /* SC */   /* Gated attention registers */
          ctx->Abus = 0;
          break;
   case 0x12:    /* FS */   /* File status */
          ctx->Abus = 0;
          break;
   case 0x13:    /* BX */
          ctx->Abus = ctx->BX_REG;
          break;
   case 0x14:    /* DR */
          ctx->Abus = ctx->DR_REG;
          break;
   case 0x15:    /* ER */
          ctx->Abus = ctx->ER_REG;
          break;
   case 0x16:    /* IE */
          ctx->Abus = ctx->IE;
          break;
   case 0x17:    /* OA */   /* Old address */
          ctx->Abus = 0;
          break;
   case 0x18:    /* CX */
          ctx->Abus = ctx->CX_REG;
          break;
   case 0x19:    /* IS */  /* Status */
          ctx->Abus = ctx->addr;
          break;
   case 0x1A:    /* UR */
          ctx->Abus = ctx->UR_REG;
          break;
   case 0x1B:    /* SL */   /* Selected drive */
          ctx->Abus = ctx->UR_REG;
          break;
   case 0x1C:    /* AH */ 
//          ctx->Abus = ctx->AH_REG;
          break;
   case 0x1D:    /* AL */
 //         ctx->Abus = ctx->AL_REG;
          break;
   case 0x1E:    /* BH */
  //        ctx->Abus = ctx->BH_REG;
          break;
   case 0x1F:    /* BL */
   //       ctx->Abus = ctx->BL_REG;
          break;
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
   case 3: /* none */
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
           break;
   case 11:  /* DW */
           ctx->DW_REG = ctx->Alu_out;
           ctx->tr_1 = 1;
           break;
   case 12:  /* DR */
           ctx->DR_REG = ctx->Alu_out;
           break;
   case 13:  /* FT */
           /* Drive FT register */
           ctx->FT &= ~ctx->Alu_out;
           if (sal->CN & 4)
               ctx->FT |= ctx->Alu_out;
           break;
   case 14:  /* FC */
           /* Drive FC register */
           ctx->FC &= ~ctx->Alu_out;
           if (sal->CN & 4)
               ctx->FC |= ctx->Alu_out;
           break;
   case 15:  /* IG */
           ctx->IG_REG = ctx->Alu_out;
           break;
   case 0x10:    /* Nop */
           break;
   case 0x11:    /* BT */
//           ctx->BT_REG = ctx->Alu_out;
           break;
   case 0x12:    /* WH */
     //      ctx->WH_REG = ctx->Alu_out;
           break;
   case 0x13:    /* WL */
    //       ctx->WL_REG = ctx->Alu_out;
           break;
   case 0x14:    /* AH */
//           ctx->AH_REG = ctx->Alu_out;
           break;
   case 0x15:    /* AL */
 //          ctx->AL_REG = ctx->Alu_out;
           break;
   case 0x16:    /* BH */
  //         ctx->BH_REG = ctx->Alu_out;
           break;
   case 0x17:    /* BL */
   //        ctx->BL_REG = ctx->Alu_out;
           break;
   case 0x18:    /* CX */
           ctx->BL_REG = ctx->Alu_out;
           break;
   case 0x19:    /* BX */
           ctx->BL_REG = ctx->Alu_out;
           break;
   case 0x1A:    /* SP */
           ctx->SP_REG = ctx->Alu_out;
           break;
   case 0x1B:    /* SW */
//           ctx->SW_REG = ctx->Alu_out;
           break;
   case 0x1C:    /* IE */ 
           ctx->IE = ctx->Alu_out & 0x1f;
           break;
   case 0x1D:    /* none */
           break;
   case 0x1E:    /* none */
           break;
   case 0x1F:    /* none */
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
   case 6:
   case 5:
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
           ctx->ST_REG &= ~BIT0;
           break;
   case 0x03:  /* 0 > ST1 */
           ctx->ST_REG &= ~BIT1;
           break;
   case 0x04:  /* 1 -> ST1 */
           ctx->ST_REG |= BIT1;
           break;
   case 0x05:  /* 0 > ST2 */
           ctx->ST_REG &= ~(BIT2);
           break;
   case 0x06:  /* DNST21, 1>ST2 if D != 0 */
           if (ctx->d_nzero)
               ctx->ST_REG &= ~(BIT2);
           break;
   case 0x07:  /* 0 > ST3 */
           ctx->ST_REG |= BIT3;
           break;
   case 0x08:  /* 1 > ST3 */
           ctx->ST_REG &= ~(BIT3);
           break;
   case 0x09:  /* 0 > ST4 */
           ctx->ST_REG |= BIT4;
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

   log_reg("OP=%02x DW=%02x UR=%02x BX=%02x BY=%02x DH=%02x DL=%02x FR=%02x GL=%02x\n",
           ctx->OP_REG, ctx->DW_REG, ctx->UR_REG, ctx->BX_REG, ctx->BY_REG,
           ctx->DH_REG, ctx->DL_REG, ctx->FR_REG, ctx->GL_REG);
   log_reg("KL=%02x ER=%02x GP=%02x IG=%02x DR=%02x ST=%02x FT=%02x FC=%02x A=%02x B=%02x > %02x\n",
           ctx->KL_REG, ctx->ER_REG, ctx->GP_REG, ctx->IG_REG, ctx->DR_REG,
           ctx->ST_REG, ctx->FT, ctx->FC, ctx->Abus, ctx->Bbus, ctx->Alu_out);
}


void
model2844_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
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
        ctx->WX = 0;
        return;
    }

    ctx->bus_out = bus_out & 0xff;
    ctx->tags = *tags;

    if ((*tags & CHAN_ADR_OUT) != 0) {
        if ((bus_out & 0xf0) == ctx->addr ||
              ((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) == 0) {
            ctx->addressed = 1;
        } else {
            ctx->addressed = 0;
        }
        ctx->ER_REG |= BIT1;
    } else {
        ctx->ER_REG &= ~BIT1;
    }

    if ((ctx->IG_REG & BIT1) != 0) {
log_trace("Drop Op in\n");
        ctx->opr_in = 0;
        ctx->IG_REG &= ~BIT1;
    }
  
    if ((*tags & CHAN_SEL_OUT) != 0 && ctx->addressed) {
        ctx->selected = 1; 
    } else {
        ctx->selected = 0;
    }

    if (ctx->opr_in) {
        *tags |= CHAN_OPR_IN;
        *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
    }

    /* Present end status */
    if (ctx->selected == 0 && (ctx->IG_REG & BIT5) != 0) {
        /* Wait for channel to request a poll */
        if ((*tags & (CHAN_SEL_OUT|CHAN_ADR_OUT|CHAN_REQ_IN)) == (CHAN_SEL_OUT|CHAN_REQ_IN)) {
            *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
            *tags |= (CHAN_OPR_IN|CHAN_ADR_IN);
            /* Send address */
            ctx->tr_1 = 0;
            ctx->tr_2 = 0;
        }
        
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
    }


    if (ctx->selected) {
         if ((*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN|CHAN_SRV_OUT) &&
                 (ctx->IG_REG & BIT5) == 0) {
log_trace("Drop selct\n");
            ctx->addressed = 0;
         }
         *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN|CHAN_STA_IN|CHAN_SRV_IN);

         if (ctx->IG_REG & BIT7) {
             *tags |= CHAN_ADR_IN;
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
             ctx->opr_in = 1;
         }
         if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
             ctx->ER_REG |= BIT2;
         } else {
             ctx->ER_REG &= ~BIT2;
         }

         /* If read latch */
         if ((ctx->IG_REG & BIT2) != 0 && ctx->tr_1 && (*tags & CHAN_SRV_OUT) == 0) {
             ctx->tr_1 = 0;
             ctx->tr_2 = 1;
         }

         /* If write latch */
         if ((ctx->IG_REG & BIT0) != 0 && ctx->tr_2 == 0 && (*tags & CHAN_SRV_OUT) == 0) {
             ctx->tr_1 = 0;
             ctx->tr_2 = 1;
         }

         /* If status latch */
         if ((ctx->IG_REG & BIT5) != 0) {
             *tags |= CHAN_STA_IN;
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
             ctx->tr_1 = 0;
             ctx->tr_2 = 0;
         }

         if (ctx->tr_2) {
            *tags |= CHAN_SRV_IN;
            *bus_in = ctx->DW_REG | odd_parity[ctx->DW_REG];
            ctx->bus_out = bus_out & 0xff;
            if ((*tags & CHAN_SRV_OUT) != 0) {
                ctx->tr_2 = 0;
            }
         }
    }
}

struct _device *
model2844_init(void *rend, uint16_t addr)
{
 //    SDL_Surface *text;
 //    SDL_Renderer *render = (SDL_Renderer *)rend;
     struct _device *dev2844;
     struct _2844_context *disk;

     if ((dev2844 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;

     if ((disk = calloc(1, sizeof(struct _2844_context))) == NULL)
         return NULL;

     dev2844->bus_func = &model2844_dev;
     dev2844->dev = (void *)disk;
     dev2844->draw_model = (void *)NULL;
     dev2844->create_ctrl = (void *)NULL;
     dev2844->rect[0].x = 0;
     dev2844->rect[0].y = 0;
     dev2844->rect[0].w = 305;
     dev2844->rect[0].h = 142;
     dev2844->n_units = 1;
     dev2844->addr = addr;
     add_chan(dev2844, addr);
     return dev2844;
}


/*
 * microsim360 - Model 1403 printer
 *
 * Copyright 2026, Richard Cornwell
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "logger.h"
#include "device.h"
#include "config.h"
#include "event.h"
#include "xlat.h"
#include "model2821/model2821.h"
#include "model1403.h"

/*
 *  Commands.
 *
 *                 01234567
 *  Write & Space  0LLLL001       L = 0000 to 0011
 *  Space Immedate 0LLLL011       L = 0000 to 0011
 *  Write & Skip   1CCCC001       C = 0001 to 1100
 *  Skip Immediate 1CCCC011       C = 0001 to 1100
 *  Sense          00000100
 */

DEV_LIST_STRUCT(1403, DEV_TYPE, 0);

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, stop key pressed */
                              /* No paper */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT5  /* Character send not on print train */
#define SENSE_CHAN9     BIT7  /* Channel 9 skipped over */



static const uint16_t cctape_legacy[] = {
/* 1      2      3      4      5      6      7      8      9     10       lines  */
0x800, 0x000, 0x000, 0x000, 0x000, 0x000, 0x400, 0x000, 0x000, 0x000, /*  1 - 10 */
0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x000, 0x000, 0x100, 0x000, /* 11 - 20 */
0x000, 0x000, 0x000, 0x000, 0x080, 0x000, 0x000, 0x000, 0x000, 0x000, /* 21 - 30 */
0x040, 0x000, 0x000, 0x000, 0x000, 0x000, 0x020, 0x000, 0x000, 0x000, /* 31 - 40 */
0x000, 0x000, 0x010, 0x000, 0x000, 0x000, 0x000, 0x000, 0x004, 0x000, /* 41 - 50 */
0x000, 0x000, 0x000, 0x000, 0x002, 0x000, 0x000, 0x000, 0x000, 0x000, /* 51 - 60 */
0x001, 0x000, 0x008, 0x000, 0x000, 0x000, };                          /* 61 - 66 */
/*
    PROGRAMMMING NOTE:  the below cctape value SHOULD match
                        the same corresponding fcb value!
*/
static const uint16_t cctape_std1[] = {
/* 1      2      3      4      5      6      7      8      9     10       lines  */
0x800, 0x000, 0x000, 0x000, 0x000, 0x000, 0x400, 0x000, 0x000, 0x000, /*  1 - 10 */
0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x000, 0x000, 0x100, 0x000, /* 11 - 20 */
0x000, 0x000, 0x000, 0x000, 0x080, 0x000, 0x000, 0x000, 0x000, 0x000, /* 21 - 30 */
0x040, 0x000, 0x000, 0x000, 0x000, 0x000, 0x020, 0x000, 0x000, 0x000, /* 31 - 40 */
0x000, 0x000, 0x010, 0x000, 0x000, 0x000, 0x000, 0x000, 0x008, 0x000, /* 41 - 50 */
0x000, 0x000, 0x000, 0x000, 0x004, 0x000, 0x000, 0x000, 0x000, 0x000, /* 51 - 60 */
0x002, 0x000, 0x001, 0x000, 0x000, 0x000, };                          /* 61 - 66 */

const static uint8_t ebcdic_to_out[256] = {
/*      0     1     2     3    4     5      6   7 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 0x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,      /* 1x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 2x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 3x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 4x */
    /*           [     .     <     (     +     |   */
    0x00, 0x00, 0x40, 0x30, 0x40, 0x40, 0x2b, 0x40,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 5x */
    /*           !     $     *     )     ;     ^   */
    0x00, 0x00, 0x40, 0x2f, 0x33, 0x40, 0x40, 0x40,
    /* -  / */
    0x2c, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 6x */
    /*                 ,     %     _     >     ?   */
    0x00, 0x00, 0x00, 0x2e, 0x32, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 7x */
    /*     `     :     #     @     \     =     "   */
    0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    /*     a     b     c     d     e     f     g   */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,      /* 8x */
    /* h  i */
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*     j     k     l     m     n     o     p    */
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,      /* 9x */
    /* q  r */
    0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*     ~     s     t     u     v     w     x   */
    0x00, 0x2c, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,      /* Ax */
    /* y  z */
    0x19, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* Bx */
    /*{    A     B     C     D     E     F     G   */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,      /* 8x */
    /* H  I */
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*}    J     K     L     M     N     O     P    */
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,      /* 9x */
    /* Q   R */
    0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* \         S     T     U     V     W     X   */
    0x40, 0x00, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,      /* Ax */
    /* Y   Z */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*  0     1     2     3     4     5     6     7    */
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,      /* Fx */
    /* 8  9 */
    0x29, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char *model1403_type_label[3] = { "LEGACY", "STD1", NULL};
const uint16_t *model1403_fcb[3] = {cctape_legacy, cctape_std1, NULL};

/*
 * Feed one line or to channel 1.
 */
void
model1403_feed(struct _1403_context *lpr, int num)
{
    int r;
    int i;
    uint16_t mask;

    if (num == 2) {
        mask = 0x1000 >> 1;  /* Mask which channel to stop at */
        r = 0;     /* What row we should be on */
        for (i = lpr->row + 1; (lpr->fcb[i] & mask) == 0 && lpr->row != i; i++) {
             r++;
             if (i > lpr->lpp) {
                 fwrite("\r\n\f", 1, 3, lpr->file);
                 memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
                 memset(&lpr->output[14][0], 0, 120);
                 r = 0;
             }
        }
    } else {
        r = 1;
    }

    while (r-- > 0) {
       fwrite("\r\n", 1, 2, lpr->file);
       memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
       memset(&lpr->output[14][0], 0, 120);
       lpr->row++;
       if (lpr->row > lpr->lpp) {
           log_device("printer skip %d > %d\n", lpr->row, lpr->lpp);
           fwrite("\f", 1, 1, lpr->file);
           lpr->row = 0;
       }
    }
}

/*
 * Print out a line and return number of lines being skipped
 */
static int
print_line(struct _2821_dev_context *ctx)
{

    struct _1403_context *lpr = (struct _1403_context *)ctx->ctx;
    char                out[150];       /* Temp conversion buffer */
    int                 i;
    int                 l = (ctx->cmd >> 3) & 0x1f;
    int                 f = 1;
    int                 r = 0;          /* Rows skipped */
    int                 time = 0;       /* Amount of time it took to print this line */
    uint16_t            mask;

    lpr->ch9 = lpr->ch12 = 0;
    /* Dump buffer if full */
    if ((ctx->cmd & 0x7) == 01) {

        /* Try to convert to text */
        memset(out, ' ', sizeof(out));

        /* Scan each column */
        for (i = 0; i < ctx->bptr; i++) {
           int         ch = ctx->buffer[i];

           if (i < 120)
               lpr->output[14][i] = ebcdic_to_out[ch];
           ch = ebcdic_to_ascii[ch];
           if (!isprint(ch))
              ch = '.';
           out[i] = ch;
        }

        /* Trim trailing spaces */
        for (--i; i > 0 && out[i] == ' '; i--) ;
        out[++i] = '\0';

        /* Print out buffer */
        fwrite(&out, 1, i, lpr->file);
        log_device( " Printer: %s\n", out);
        memset(ctx->buffer, 0x40, sizeof(ctx->buffer));
        time = 11;
    }
    fflush(lpr->file);

    f = 1;  /* Indicate we need to output a new line */
    if (l < 4) {
        while(l != 0) {
            fwrite("\r\n", 1, 2, lpr->file);
            memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
            memset(&lpr->output[14][0], 0, 120);
            r++;
            f = 0;
            log_device( " Printer fcb: %04x\n", lpr->fcb[lpr->row]);
            if ((lpr->fcb[lpr->row] & (0x1000 >> 9)) != 0)
                lpr->ch9 = 1;
            if ((lpr->fcb[lpr->row] & (0x1000 >> 12)) != 0) {
        log_device( " Printer chan 12\n");
                lpr->ch12 = 1;
            }
            lpr->row++;
            if (lpr->row > lpr->lpp)
                break;
            l--;
        }
        if (lpr->row > lpr->lpp) {
           memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
           memset(&lpr->output[14][0], 0, 120);
           if (f)
               fwrite("\r\n", 1, 2, lpr->file);
           fwrite("\f", 1, 1, lpr->file);
           lpr->row = 0;
        }
        time += 20 + (5 * r);
        return time;
    }

    mask = 0x1000 >> (l & 0xf);  /* Mask which channel to stop at */
    f = 0;     /* Indicate if we started new form */
    l = 0;     /* Total lines skipped */
    r = 0;     /* What row we should be on */
    for (i = lpr->row + 1; (lpr->fcb[i] & mask) == 0 && lpr->row != i; i++) {
         l++;
         r++;
         if (i > lpr->lpp) {
             log_device("printer skip2 %d > %d\n", i, lpr->lpp);
             fwrite("\r\n\f", 1, 3, lpr->file);
             memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
             memset(&lpr->output[14][0], 0, 120);
             f = 1;
             r = 0;
         }
    }

    /* If we passed over form, clear row */
    if (f) {
       lpr->row = 0;
    }

    if (lpr->fcb[i] & mask) {
        while (r-- > 0) {
           fwrite("\r\n", 1, 2, lpr->file);
           memcpy(&lpr->output[0][0], &lpr->output[1][0], 14 * 120);
           memset(&lpr->output[14][0], 0, 120);
           lpr->row++;
           if (lpr->row > lpr->lpp) {
               log_device("printer skip %d > %d\n", lpr->row, lpr->lpp);
               fwrite("\f", 1, 1, lpr->file);
               lpr->row = 0;
           }
        }
    }
    time += 20 + (5 * l);
    return l;
}

static void
done_callback(struct _device *unit, void *arg, int iarg)
{
    struct _2821_dev_context *ctx = (struct _2821_dev_context *)unit->dev;
    struct _1403_context *lpr = (struct _1403_context *)ctx->ctx;
    log_device("1403: %03x finish\n",unit->addr);
    ctx->request = 1;
    ctx->busy = 0;
    ctx->cmd_done = 1;
    if (lpr->ch9 && (ctx->cmd & 0x3) == 0x1) {
       ctx->sense |= SENSE_CHAN9;
       ctx->status |= SNS_UNITCHK;
    }
    if (lpr->ch12 && (ctx->cmd & 0x3) == 0x1) {
       log_device( " Printer chan 12\n");
       ctx->status |= SNS_UNITEXP;
    }
    ctx->status |= SNS_DEVEND;
}

static void
xfer_done(struct _2821_dev_context *ctx)
{
    add_event(ctx->device, done_callback, 2000 * print_line(ctx), NULL, 0);
    ctx->status |= SNS_CHNEND;
}

static void
halt_device(struct _2821_dev_context *ctx)
{
    xfer_done(ctx);
}

/* Decode command to device */
static void
device_cmd(struct _2821_dev_context *ctx, uint16_t bus_out)
{
    struct _1403_context *lpr = (struct _1403_context *)ctx->ctx;
    uint16_t cmd = bus_out & 0xff;

    log_device("1403: printer command %02x\n", bus_out);
    ctx->status = 0;

    switch (cmd & 07) {
    case 0: /* Test I/O */
           /* Check if device not ready */
           if (lpr->ready == 0 || lpr->file == 0) {
               ctx->sense |= SENSE_INTERV;
               ctx->status |= SNS_UNITCHK;
               return;
           }
           if (ctx->sense != 0) {
               ctx->status |= SNS_UNITCHK;
           }
           return;

    case 1: /* Write */
           ctx->cmd = cmd;
           ctx->cmd_done = 0;

           ctx->sense &= SENSE_INTERV;
           /* Check if skip invalid */
           if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
               ctx->sense |= SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }
           /* Check if space invalid */
           if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
               ctx->sense |= SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }

           /* Check if device not ready */
           if (lpr->ready == 0 || lpr->file == 0) {
               ctx->sense |= SENSE_INTERV;
               ctx->status |= SNS_UNITCHK;
               return;
           }

           /* Check if device not ready */
           if ((ctx->sense & SENSE_INTERV) != 0) {
               ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
               ctx->cmd_done = 1;
               break;
           }
           lpr->ch9 = 0;
           lpr->ch12 = 0;
           ctx->bptr = 0;
           ctx->busy = 1;
           break;

    case 3: /* Feed */
           ctx->cmd = cmd;
           ctx->cmd_done = 1;
           /* Check for NOP */
           ctx->sense &= SENSE_INTERV;
           if (cmd == 0x03) {
    log_device("printer nop\n");
               ctx->cmd = 0;
               ctx->status = SNS_CHNEND|SNS_DEVEND;
               break;
           }

           /* Check if skip invalid */
           if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               break;
           }
           /* Check if space invalid */
           if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               break;
           }

           /* Check if device not ready */
           if (lpr->ready == 0 || lpr->file == 0) {
               ctx->sense |= SENSE_INTERV;
               ctx->status |= SNS_UNITCHK;
               return;
           }

           /* Check if device not ready */
           if ((ctx->sense & SENSE_INTERV) != 0) {
               ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
               ctx->cmd_done = 1;
               break;
           }

           lpr->ch9 = 0;
           lpr->ch12 = 0;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           xfer_done(ctx);
           break;

    case 4:  /* Sense */
           if (cmd != 0x4) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }
           ctx->cmd = cmd;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           log_device("1403: Printer sense %02x\n", ctx->sense);
           break;

    default:
           ctx->sense = SENSE_CMDREJ;     /* Invalid command */
           ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
           break;
    }
}

struct _device *
model1403_init(uint16_t addr, uint16_t addr_2821)
{
     struct _device           *dev1403;
     struct _device           *unit;
     struct _2821_dev_context *ctx2821;

     if ((unit = find_chan_name("2821", addr_2821)) == NULL) {
         printf("Unable to find a 2821 device\n");
         return NULL;
     }

     if ((dev1403 = calloc(1, sizeof(struct _device))) == NULL) {
         return NULL;
     }

     if ((ctx2821 = calloc(1, sizeof(struct _2821_dev_context))) == NULL) {
         free(dev1403);
         return NULL;
     }

     dev1403->dev = (void *)ctx2821;
     dev1403->draw_model = (void *)&model1403_draw;
     dev1403->create_ctrl = (void *)&model1403_control;
     dev1403->init_device = (void *)&model1403_init_graphics;
     dev1403->type_name = "1403";
     dev1403->rect[0].x = 0;
     dev1403->rect[0].y = 0;
     dev1403->rect[0].w = 280;
     dev1403->rect[0].h = 200;
     dev1403->n_units = 1;
     dev1403->addr = addr;
     ctx2821->addr = addr;
     ctx2821->device = dev1403;
     ctx2821->blen = 120;
     ctx2821->mpx_count = 4;
     ctx2821->sense = 0;
     ctx2821->device_cmd = &device_cmd;
     ctx2821->device_xfer_done = &xfer_done;
     ctx2821->device_halt = &halt_device;
     add_chan(dev1403, dev1403->addr);
     switch(model2821_register(unit, ctx2821, DEVICE_2821_PRINTER)) {
     case DEVICE_2821_OK:
               log_info("%03x registered to 2821 device %03x\n", addr, addr_2821);
               break;
     default:
               log_error("2821 %03x already has too many printers\n", addr_2821);
               del_chan(dev1403, dev1403->addr);
               free(ctx2821);
               free(dev1403);
               return NULL;
     }
     return dev1403;
}


int
model1403_create(struct _option *opt)
{
     struct _device           *dev1403;
     struct _1403_context     *lpr;
     struct _2821_dev_context *ctx2821;
     struct _option            opts;
     int                       addr2821 = -1;


     if ((lpr = calloc(1, sizeof(struct _1403_context))) == NULL) {
         return 0;
     }
     lpr->file_name = NULL;
     lpr->form = 1;
     lpr->fcb_num = 0;
     lpr->fcb = model1403_fcb[lpr->fcb_num];
     lpr->lpp = 66;

     /* Parse options given on definition */
     while (get_option(&opts)) {
           if (strcmp(opts.opt, "START") == 0 && lpr->file != NULL) {
               lpr->ready = 1;
           } else if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
               if (lpr->file != NULL) {
                   fclose(lpr->file);
                   lpr->form = 1;
               }
               free(lpr->file_name);
               lpr->file_name = NULL;
               lpr->file = fopen(opts.string, "a");
               if (lpr->file != NULL) {
                  lpr->row = 0;
                  if ((lpr->file_name = (char *)malloc(strlen(opts.string)+1)) == NULL) {
                      fclose(lpr->file);
                      free(lpr);
                      return 0;
                  }
                  strcpy(lpr->file_name, opts.string);
               }
           } else if (strcmp(opts.opt, "FCB") == 0 && opts.flags == 1) {
               lpr->fcb_num = get_index(&opts, model1403_type_label);
               if (lpr->fcb_num < 0) {
                   log_error("%03x Invalid FCB Name %s\n", opt->addr, opt->string);
                   free(lpr);
                   return 0;
               }
               lpr->fcb = model1403_fcb[lpr->fcb_num];
           } else if (strcmp(opts.opt, "2821") == 0 && opts.flags == 1) {
               if (get_integer(&opts, &addr2821) != 1) {
                   free(lpr);
                   return 0;
               }
           } else {
               log_error("Invalid option %s to 1403\n", opts.opt);
               free(lpr);
               return 0;
           }
     }

     /* Check if we have 2821 device */
     if (addr2821 < 0) {
         log_error("%03x missing 2821 address\n", opt->addr);
         free(lpr);
         return 0;
     }

     /* Create device */
     addr2821 = (opt->addr & 0xf00) | (addr2821 & 0xff);

     /* Hook up lpr option */
     dev1403 = model1403_init(opt->addr, (uint16_t)addr2821);
     if (dev1403 == NULL) {
         free(lpr);
         return 0;
     }
     ctx2821 = (struct _2821_dev_context *)dev1403->dev;
     ctx2821->ctx = (void *)lpr;
     return 1;
}





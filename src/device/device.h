/*
 * microsim360 - Generic device header.
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

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>

#define BIT0    0x80
#define BIT1    0x40
#define BIT2    0x20
#define BIT3    0x10
#define BIT4    0x08
#define BIT5    0x04
#define BIT6    0x02
#define BIT7    0x01

/* Channel status byte flags */
#define     SNS_ATTN      0x80                /* Unit attention */
#define     SNS_SMS       0x40                /* Status modifier */
#define     SNS_CTLEND    0x20                /* Control unit end */
#define     SNS_BSY       0x10                /* Unit Busy */
#define     SNS_CHNEND    0x08                /* Channel end */
#define     SNS_DEVEND    0x04                /* Device end */
#define     SNS_UNITCHK   0x02                /* Unit check */
#define     SNS_UNITEXP   0x01                /* Unit exception */

/* Command masks */
#define     CMD_MASK      0xf                 /* Type mask */
#define     CMD_CHAN      0x0                 /* Channel command */
#define     CMD_WRITE     0x1                 /* Write command */
#define     CMD_READ      0x2                 /* Read command */
#define     CMD_CTL       0x3                 /* Control command */
#define     CMD_SENSE     0x4                 /* Sense channel command */
#define     CMD_TIC       0x8                 /* Transfer in channel */
#define     CMD_RDBWD     0xc                 /* Read backward */


/* Channel tag controls. */
#define CHAN_SEL_OUT    0x8000        /* Channel select out signal */
#define CHAN_ADR_OUT    0x4000        /* Address out */
#define CHAN_CMD_OUT    0x2000        /* Command out */
#define CHAN_SRV_OUT    0x1000        /* Service out */
#define CHAN_SUP_OUT    0x0800        /* Suppress out */
#define CHAN_HLD_OUT    0x0400        /* Hold out */
#define CHAN_OPR_OUT    0x0200        /* Operation output */
#define CHAN_OPR_IN     0x0080        /* Operation in from device */
#define CHAN_ADR_IN     0x0040        /* Address input from device */
#define CHAN_STA_IN     0x0020        /* Status in from device */
#define CHAN_SRV_IN     0x0010        /* Service in from device */
#define CHAN_REQ_IN     0x0008        /* Request in from device */
#define CHAN_SEL_IN     0x8000        /* Becomes select in at end of chain */

#define OUT_TAGS        0xfe00        /* Outbound tags */
#define IN_TAGS         0x00ff        /* Inbound tags */

/* Device structure. One per controller */
struct _device {
    void      (*bus_func)(struct _device *dev,
                          uint16_t *tags,
                          uint16_t bus_out,
                          uint16_t *bus_in);
    void      (*draw_model)(struct _device *unit, void *render);
    struct _popup *(*create_ctrl)(struct _device *unit, int hd, int wd, int u);
    void       *dev;               /* Pointer to device function */
    int         n_units;           /* Number of units */
    uint16_t    addr;              /* Device address and channel */
    struct _rect {
        int     x;                 /* X offset for device image */
        int     y;                 /* Y offset for device image */
        int     w;                 /* Width of device image */
        int     h;                 /* Height of image */
    }           rect[8];
    struct _device *next;          /* Next device in chain */
};

/* Disk controller microcode steps */
struct _disk {
    void      (*step)(void *data);   /* Pointer to microstep routine */
    void             *disk;          /* Pointer to per disk data */
    struct _disk     *next;          /* Next disk in list */
};

struct _unit {
    char *name;
};

struct _control {
    char *name;
    int   type;
    int   opts;
    int  (*create)(char *line);
    struct _device *(*init)(void *render, uint16_t addr);
    unsigned int    magic;
};


#define HEAD_TYPE    0
#define CPU_TYPE     1
#define DEVICE_TYPE  2
#define CTRL_TYPE    3
#define UNIT_TYPE    4

#define CHAR_OPT     1
#define NUM_MOD      2
#define NUM_OPT      4

#define DEV_LIST_MAGIC   0xdeadbeef

#if defined(_MSC_VER)
#pragma segment(".devlist$u")
#define DEV_LIST_SECTION __declspec(allocate(".devlist$u")) __declspec(align(1))
#elif defined(__APPLE__)
#define DEV_LIST_SECTION __attribute__ ((used, section ("__DATA, .devlist"), aligned(1)))
#else
#define DEV_LIST_SECTION __attribute__ ((used, section (".devlist"), aligned(1)))
#endif


#define DEV_LIST_STRUCT(mod, type, opts) \
    static struct _control DEV_LIST_SECTION model_##mod## DEV_LIST_SECTION = { \
        #mod, type, opts, model##mod##_create, model##mod##_init, DEV_LIST_MAGIC, \
    }


extern struct _device *chan[6];         /* Channels */
extern struct _disk   *disk;            /* Disk controller that need to be run */

void print_tags(char *name, int state, uint16_t tags, uint16_t bus_out);

void print_inst(uint8_t *val);

void add_chan(struct _device *dev, uint16_t addr);

void del_chan(struct _device *dev, uint16_t addr);

void add_disk(void (*fnc)(void *), void *drive);

void step_disk();

/* Return pointer to list of devices */
struct _control *dev_list();


/******************************************************************
 *
 *    Configuration file format.
 *
 *  CPU=model C30, D30, E30, F30, E50...
 *
 *
 *  CNTL=2415-4,180,TRANS,CONV
 *  UNIT=2415,180
 *  UNIT=2415,181
 *  UNIT=2415,182
 *  UNIT=2415,183
 *  
 *  CNTL=2821-1,000
 *  UNIT=2540R,00C
 *  UNIT=2540P,00D
 *  UNIT=1403,00E
 *
 *  CNTL=2841,190
 *  UNIT=2311,190
 *  UNIT=2311,191
 *  UNIT=2311,192
 *  UNIT=2311,193
 *  UNIT=2311,194
 *  UNIT=2311,195
 *  UNIT=2311,196
 *  UNIT=2311,197
 */


/*
 *    Bus-Out   Bus-In     Tags                                       Function
 *    dev        ~        Addr-out                                    Initial select
 *    dev        ~        Sel-out|Addr-out|Hold-out
 *    dev        dev      Sel-out|Addr-out|Hold-out|Opr-in|Addr-in    Device ack.
 *    dev        status   Sel-out|Addr-out|Hold-out|Status-in         Device busy.
 *    cmd        dev      ?Sel-out|Cmd-out|Hold-out?|Opr-in|Addr-in   Device Command
 *    cmd        status   ?Sel-out|Cmd-out|Hold-out?|Opr-in|Status-in Initial status.
 *    ~          ~        ?Sel-out|Hold-out?|Opr-in                   Device working.
 *    ~          ~                                                    Device Disco.
 *
 *    ~          ~        Req-in                                      Device request service.
 *    ~          ~        Req-in|Sel-out                              Channel asking for device.
 *    ~          Dev      Sel-out|Addr-in|Opr-in                      Device address.
 *    cmd        dev      Sel-out?|Cmd-out|Addr-in|Opr-in             Accept command.
 *    ~          ~        Opr-in|Serv-in                              Request byte. (Write/Control)
 *    data       ~        Opr-in|Serv-in|Serv-Out                     Data byte.
 *    ~          ~        Opr-in                                      Data accept.
 *    ~          data     Opr-in|Serv-in                              Send Byte (Read/Sense)
 *    ~          data     Opr-in|Serv-in|Serv-out                     Data accept.
 *    ~          ~        Opr-in                                         "
 *    ~          status   Opr-in|Status-in                            No more data.
 *    ~          status   Opr-in|Status-in|Serv-out                   Status accepted.
 *    ~          status   Opr-in|Status-in|Cmd-out                    Status Stack.
 *    data       data     Opr-in|Serv-in|Cmd-out                      Data accept, no more data.
 *    ~          status   Opr-in|Serv-in|Serv-out|Supr-out            Status accepted, command chain.
 *    ~          ~        !Opr-out|Supr-out|Opr-in                    Device reset.
 *    ~          ~        !Opr-out                                    Reset all.
 */

 /*
  *   Bus-out        Tag.
  *   device addr    Addr-out
  *   command        Cmd-out
  *   data           Serv-out & Write/Control.
  *
  *   Bus-in         Tag.
  *   device addr    Addr-in
  *   status         Status-in
  *   data           Serv-in & Read/Sense.
  *
  *   Opr-out        off reset.
  *   Req-in         Device had data for channel. Do not raise if Supr-out and Status.
  */   
#endif

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
#include "conf.h"

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

/* CCW flags */
#define     CHAN_CD_FLAG   BIT0        /* Chain data */
#define     CHAN_CC_FLAG   BIT1        /* Chain command */
#define     CHAN_SLI_FLAG  BIT2        /* Supress length error */
#define     CHAN_SKIP_FLAG BIT3        /* Don't transfer on read */
#define     CHAN_PCI_FLAG  BIT4        /* Issue PCI interrupt after first transfer */

/* Channel check flags */
#define     CHAN_PCI       BIT0        /* PCI interrupt. */
#define     CHAN_LENGTH    BIT1        /* Incorrect length */
#define     CHAN_PROG      BIT2        /* Program check error */
#define     CHAN_PROT      BIT3        /* Protection error */
#define     CHAN_DATA      BIT4        /* Data check */
#define     CHAN_CTRL      BIT5        /* Channel controller check */
#define     CHAN_INTER     BIT6        /* Channel interface check */
#define     CHAN_CHAIN     BIT7        /* Chaining check */


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


typedef enum {
        STATE_IDLE,           /* Device in Idle state */
        STATE_BUSY,           /* Device responding to busy status */
        STATE_INIT_SEL,       /* Device responding to selection */
        STATE_COMMAND,        /* Device responding to command out */
        STATE_STATUS,         /* Device presenting status */
        STATE_STATUS_ACCEPT,  /* Device waiting for status to be accepted */
        STATE_STATUS_WAIT,
        STATE_END_STATUS,     /* Device presenting ending status */
        STATE_END_ACCEPT,     /* Device waiting for ending status to be accepted */
        STATE_WAIT_DEVEND,    /* Hold bus on selector channel waiting for device end */
        STATE_OPR,            /* Wait for something to do */
        STATE_DATA_1,         /* Request data transfer */
        STATE_DATA_2,         /* Wait for service out */
} device_state_t;



/* Device structure. One per controller */
typedef struct _device {
    void      (*bus_func)(struct _device *dev,
                          uint16_t *tags,
                          uint16_t bus_out,
                          uint16_t *bus_in);
    void      (*draw_model)(struct _device *unit, void *render, int u);
    void      *(*create_ctrl)(struct _device *unit, int u);
    void      (*init_device)(struct _device *unit, void *render);
    void      (*close_device)(struct _device *unit);
    void       *dev;               /* Pointer to device context */
    char       *type_name;         /* Name of device */
    int         n_units;           /* Number of units */
    uint16_t    addr;              /* Device address and channel */
    uint16_t    mask;              /* Device address mask */
    struct _rect {
        int     x;                 /* X offset for device image */
        int     y;                 /* Y offset for device image */
        int     w;                 /* Width of device image */
        int     h;                 /* Height of image */
    }           rect[8];
    uint8_t     request;           /* Request pending */
    uint8_t     stacked;           /* Stacked status */
    uint8_t     selected;          /* Device selected */
    struct _device *next;          /* Next device in chain */
} device_t;

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
    char      *name;
    int        type;
    int        opts;
    int       (*create)(struct _option *opt);
    device_t *(*init)(void *render, uint16_t addr);
    unsigned int    magic;
};

extern device_t       *chan[6];         /* Channels */
extern struct _disk   *disk;            /* Disk controller that need to be run */

void print_tags(char *name, int state, uint16_t tags, uint16_t bus_out);

void print_inst(uint8_t *val);

void add_chan(device_t *dev, uint16_t addr);

struct _device *find_chan(uint16_t addr, uint16_t mask);

void del_chan(device_t *dev, uint16_t addr);

void add_disk(void (*fnc)(void *), void *drive);

void del_disk(void *drive);

void step_disk();

void system_init(void *render);

void system_shutdown();

extern char *title;

extern void (*setup_cpu)(void *rend);

extern void (*step_cpu)();


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

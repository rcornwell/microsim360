/*
 * microsim360 - Model 2415 header.
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

/*
 *  Commands.
 *
 *            01234567
 *  Write     00000001
 *  Read      00000010
 *  Sense     00000100
 *  Readback  00001100
 *
 *  Control   00CCC111   Tape motion control
 *              000      Rewind
 *              001      Rewind and unload
 *              010      Erase Gap
 *              011      Write tape mark.
 *              100      Backspace block
 *              101      Backspace file.
 *              110      Forward space block.
 *              111      Forward space file.
 *  Mode      DDMMM011   7 track
 *                       den, odd, even, conv, noconv, trans, notrans
 *              000          NOP
 *              001          Reserved.
 *              010       y   y          y                     y
 *              011          9 track only
 *              100       y         y             y            y
 *              101       y         y             y      y
 *              110       y   y                   y            y
 *              111       y   y                   y      y
 *
 *            00          200bpi
 *            01          556bpi
 *            10          800bpi
 *            11          9 track mode. Models 4-6
 *
 *  Mode      11NNN011    9 track.
 *              000       1600 bpi
 *              001       800 bpi
 */


#ifndef _MODEL2415_H_
#define _MODEL2415_H_
#include <stdint.h>
#include "device.h"

#define FRAME_DELAY     33    /* 34 us per frame delay */
#define REWIND_DELAY    1000 /* 20 ms */
#define REW_FRAME       3840  /* Frames per 20ms */
#define START_DELAY     100
#define STOP_DELAY      (10 * FRAME_DELAY)

#define MT_ODD          0x01  /* Odd parity */
#define MT_TRANS        0x02  /* Translation turned on ignored 9 track  */
#define MT_CONV         0x04  /* Data converter on ignored 9 track  */

struct _2415_context {
    device_state_t         state;             /* Current channel state */
    int                    addr;         /* Device address */
    int                    chan;
    int                    selected;     /* Device currently selected */
    int                    request;      /* Request pending */
    int                    addressed;    /* Device addressed */
    int                    stacked;      /* Device has stacked status */
    int                    sense[6];     /* Current sense value */
    int                    sense_cnt;    /* Sense counter */
    int                    busy;         /* Return control unit done */
    int                    chaining;     /* Command chaining in effect */
    int                    cmd;          /* Current command */
    int                    cmd_done;     /* Current read/write finished */
    int                    status;       /* Current bus status */
    uint8_t                data;         /* Current byte to send/recieve */
    uint16_t               cbuffer;      /* Conversion buffer */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* No more data to send/recieve */
    int                    data_end_post;/* No more data to send/recieve */
    int                    ctrl_busy;    /* Control unit posted short busy */
    int                    delay;        /* Delay time till operation done */
    int                    nunits;       /* Number of units */
    int                    cur_unit;     /* Currently selected unit */
    int                    stat_unit;    /* Unit having pending sense data */
    int                    stk_unit;     /* Unit that has stacked status */
    int                    t_scan;       /* Tape scanner */
    int                    rew_flags;    /* Units doing rewind */
    int                    run_flags;    /* Units doing unload */
    int                    rew_delay;    /* Delay until processing rewinding units */
    int                    rdy_flags;    /* Unit has become ready */
    int                    mrk_flags;    /* Last record was mark */
    struct _tape_buffer   *tape[6];      /* Tape units */
    int                    supply_color[6]; /* Color of supply reel color */
    int                    takeup_color[6]; /* Color of takeup reel color */
    int                    supply_label[6]; /* Color of supply label color */
    int                    takeup_label[6]; /* Color of takeup  label color */
    int                    track_7;      /* Support 7 track drives */
    int                    cc;           /* Character counter for 7 track tapes */
    int                    mode7;        /* Tape mode for 7 track tapes */
    int                    mode9;        /* Tape mode for 9 track tapes */
};

void model2415_rewind_callback(struct _device *unit, void *arg, int u);

struct _popup * model2415_control(struct _device *unit, int hd, int wd, int u);

void model2415_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);

void model2415_draw(struct _device *unit, void *rend);

void model2415_init(struct _device *unit, void *rend);
#endif

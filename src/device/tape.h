/*
 * microsim360 - Generic tape interface.
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

#ifndef _TAPE_H_
#define _TAPE_H_
#include "config.h"
#include <stdint.h>

#define TYPE_TAP    0
#define TYPE_E11    1
#define TYPE_P7B    2
#define TAPE_FMT    3                  /* Mask for tape format */

#define WRITE_RING  0x004
#define DEN_MASK    0x008
#define DEN_1600    0x008
#define DEN_800     0x000
#define TAPE_EOT    0x010               /* Mark that we are at end of tape */
#define TAPE_BOT    0x020               /* Mark that we are at load point */
#define TAPE_MARK   0x040               /* We read a tape mark */
#define TRACK9      0x080               /* 9 track tape drive */
#define SELECTED    0x100               /* Drive selected */
#define ONLINE      0x200               /* Drive online */

#define FUNC_READ   1
#define FUNC_WRITE  2
#define FUNC_REW    3
#define FUNC_RDBACK 4
#define FUNC_MARK   5
#define FUNC_V      12
#define FUNC_M      7

#define IRG_LEN     1200               /* Number of frames for a IRQ */
#define IRG_MASK    0x80               /* P7B inter record marker */
#define BCD_TM      0xf                /* IRG tape mark */

struct _tape_buffer {
     char         *file_name;           /* File name attached to */
     int           fd;                  /* Open file descriptor. */
     int           format;              /* Tape format, and flags */
     off_t         pos;                 /* Position in file of start of buffer */
     long          pos_frame;           /* Frame position from beginning of tape */
     int           pos_buff;            /* Position within buffer */
     int           len_buff;            /* Length of buffer */
     uint32_t      lrecl;               /* Logical record length of current record */
     uint32_t      orecl;               /* Original record length */
     long          srec;                /* Start of record offset for TAP and E11 format */
     int           dirty;               /* Buffer dirty */
     uint8_t       buffer[32*1024];     /* Buffer of current record */
};

extern struct _tape_image {
     int           x;                   /* X position */
     int           y;                   /* Y position */
     int           start;               /* Start position tape in frames */
     int           length;              /* Length of current rotation in frames */
     int           radius;              /* Current radius of reel */
} tape_position[1300];

extern int tape_image_pos[37];

/*
 * Return true if tape at load point.
 */

int tape_at_loadpt(struct _tape_buffer *tape);

/*
 * Determine if a tape is attached to drive.
 */
int tape_ready(struct _tape_buffer *tape);

/*
 * Determine if a tape can be written
 */
int tape_ring(struct _tape_buffer *tape);

/*
 * Determine if a 9 track or 7 track tape
 */
int tape_9_track(struct _tape_buffer *tape);

/*
 * Select tape drive.
 */
void tape_select(struct _tape_buffer *tape);

/*
 * Un-Select tape drive.
 */
void tape_unselect(struct _tape_buffer *tape);

/*
 * Is-selected tape drive.
 */
int tape_is_selected(struct _tape_buffer *tape);

/*
 * Attach a tape to buffer:
 *
 *    Tape is buffer for this drive.
 *    file_name is name to attach.
 *    ring indicates that there is a write ring installed.
 *    den  is the density, 0 = 800, 1 = 1600.
 */
int tape_attach(struct _tape_buffer *tape, char *file_name, int type, int ring, int den);

/*
 * Detach tape.
 *
 *   If buffer is dirty, flush to file before closing.
 *   set fd to -1, and free file_name.
 */
void tape_detach(struct _tape_buffer *tape);

/*
 * Start write.
 *
 * Return:
 *
 *    -1 if file write error.
 *     0
 *     1 successfull
 *     2 write protection.
 */

int tape_write_start(struct _tape_buffer *tape);

/*
 * Write tape mark.
 *
 * Return:
 *
 *    -1 if file write error.
 *     0
 *     1 successfull
 *     2 write protection.
 */

int tape_write_mark(struct _tape_buffer *tape);

/*
 * Start read forward.
 *
 * Return -1 if file read error.
 *         0 if at end of tape.
 *         1 if another record.
 *         2 if read tape mark.
 */

int tape_read_forw(struct _tape_buffer *tape);

/*
 * Start read backward.
 */

int
tape_read_back(struct _tape_buffer *tape);

/*
 * Read one frame from tape.
 *
 * Return -1 if file read error.
 *         0 end of record.
 *         1 read frame.
 */

int tape_read_frame(struct _tape_buffer *tape, uint8_t *data);

/*
 * Write one frame to tape.
 */

int tape_write_frame(struct _tape_buffer *tape, uint8_t data);

/*
 * Finish a record.
 */

int tape_finish_rec(struct _tape_buffer *tape);

/*
 * Rewind tape number of frames.
 */

int tape_start_rewind(struct _tape_buffer *tape);

/*
 * Rewind tape number of frames.
 */

int tape_rewind_frames(struct _tape_buffer *tape, int frames);

/*
 * Find the supply reel tape position.
 */

struct _tape_image *tape_supply_image(struct _tape_buffer *tape, int *rotate);

/*
 * Find the takup reel tape position.
 */

struct _tape_image *tape_takeup_image(struct _tape_buffer *tape, int *rotate);

/*
 * Initialize the tape rotation structures.
 */
void tape_init();

extern int max_tape_length;
extern int max_tape_pos;

#if 0
                  data rate          Speed     IRG   Start 7tm  9tm/800 9tm/1600    Rewind
2401   Model 1/4  30,000/60,000      37.5      .75   320ms 104.4 100.0   101.2       3.0m  160ips
2401         2/5  60,000/120,000     75.0      .75    64ms  52.2  50.5    50.0       1.4m  320ips
2401   Model 3/6  90,000/180,000    112.5      .75    48ms  34.8  33.5    33.7       1.0m  480ips
2415   Model 1-6  15,000/30,000      18.75     .75   204ms 205   243     205         4.0m  120ips
          1-3 800
          4-6 1600/800

   Rewind disco  2400 30ms
   Rewind disco  2415 30us
                  us              irg
2401  Model 1     33.3  - 800     16.0 ms
2401  Model 2     16.6  - 800      8.0 ms
2401  Model 3     11.1  - 800      5.3 ms
2401  Model 4     16.7  - 1600    16.0 ms
2401  Model 5      8.3  - 1600     8.0 ms
2401  Model 6      5.6  - 1600     5.3 ms
2415  9track 33.3/66.6 us          32ms/40ms (7track)

   Length: 1600
           28800 inch
        46080000 frames

    Tape begin to Load Point 10 ft.   100 inch
    Tape EOT to end of tape 15 ft.   180 inch
    31.5 frames per mm 800
    63.0 frames per mm 1600

   Rewind speed

   Inter take up 32.2 inch.  5.125 inch

   Outer tape up 62.9 inch.  10.5 inch

   About 2687 rotations. 0.002in or  .0508 mm per revolution.
#endif

#endif

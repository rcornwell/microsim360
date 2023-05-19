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

#include "config.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include "logger.h"
#include "tape.h"
#include "xlat.h"

struct _tape_image tape_position[1300];

int max_tape_length;
int max_tape_pos;

/*
 * Check it tape at load point.
 */
int
tape_at_loadpt(struct _tape_buffer *tape)
{
    return (tape->format & TAPE_BOT) != 0;
}

/*
 * Determine if a tape is attached to drive.
 */
int
tape_ready(struct _tape_buffer *tape)
{
    return (tape->file_name != NULL && (tape->format & ONLINE) != 0);
}

/*
 * Determine if a tape can be written
 */
int
tape_ring(struct _tape_buffer *tape)
{
    return ((tape->format & WRITE_RING) != 0);
}

/*
 * Determine if a 9 track or 7 track tape
 */
int
tape_9_track(struct _tape_buffer *tape)
{
    return ((tape->format & TRACK9) != 0);
}

/*
 * Select tape drive.
 */
void
tape_select(struct _tape_buffer *tape)
{
    tape->format |= SELECTED;
}

/*
 * Un-Select tape drive.
 */
void
tape_unselect(struct _tape_buffer *tape)
{
    tape->format &= ~SELECTED;
}

/*
 * Select tape drive.
 */
int
tape_is_selected(struct _tape_buffer *tape)
{
    if ((tape->format & SELECTED) != 0)
        return 1;
    return 0;
}

/*
 * Attach a tape to buffer:
 *
 *    Tape is buffer for this drive.
 *    file_name is name to attach.
 *    ring indicates that there is a write ring installed.
 *    den  is the density, 0 = 800, 1 = 1600.
 */
int
tape_attach(struct _tape_buffer *tape, char *file_name, int type, int ring, int den)
{
      int    flags;
      tape->format = type | (tape->format & TRACK9);
      if (ring) {
           flags = O_RDWR|O_CREAT;
           tape->format |= WRITE_RING;
      } else {
           flags = O_RDONLY;
      }
      if (den)
           tape->format |= DEN_1600;
      tape->pos = 0;
      tape->pos_frame = 0;
      tape->pos_buff = 0;
      tape->len_buff = 0;
      tape->lrecl = 0;
      tape->srec = 0;
      tape->dirty = 0;
      tape->format |= TAPE_BOT;
      if ((tape->fd = open(file_name, flags, 0660)) < 0)
          return 0;
      tape->file_name = strdup(file_name);
      return 1;
}

/*
 * Detach tape.
 *
 *   If buffer is dirty, flush to file before closing.
 *   set fd to -1, and free file_name.
 */
void
tape_detach(struct _tape_buffer *tape)
{
    int  r;
    /* If buffer is dirty flush it to the file */
    if (tape->dirty) {
        lseek(tape->fd, tape->pos, SEEK_SET);
        r = write(tape->fd, tape->buffer, tape->len_buff);
        if (r != tape->len_buff) {
            log_error("Tape write failed %s %d\n", tape->file_name, r);
        }
        tape->dirty = 0;
    }
    close(tape->fd);
    tape->fd = -1;
    tape->format &= ~ONLINE;
    free(tape->file_name);
    tape->file_name = NULL;
}

/*
 * read the next character.
 *
 * Return -1 if file read error.
 *        0 if at end of file.
 *        1 if character read.
 */
static int
tape_read_byte(struct _tape_buffer *tape, uint8_t *data)
{
     if (tape->file_name == NULL)
        return -1;
     /* Check if at end of buffer */
     if (tape->pos_buff >= tape->len_buff || tape->len_buff == 0) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
                 log_error("Tape write failed %s %d\n", tape->file_name, r);
                 return -1;
             }
             tape->pos += tape->len_buff;
             tape->len_buff = 0;
             tape->dirty = 0;
         }
         /* Advance tape by size of buffer */
         tape->pos += tape->len_buff;
         lseek(tape->fd, tape->pos, SEEK_SET);
         tape->len_buff = read(tape->fd, tape->buffer, sizeof(tape->buffer));
         tape->pos_buff = 0;
         if (tape->len_buff <= 0) {
             tape->format |= TAPE_EOT;
             log_tape("Tape EOT\n");
             return 0;
         }
         log_tape("Tape buffer fill: %d\n", tape->len_buff);
     }
     *data = tape->buffer[tape->pos_buff++];
     log_tape("Tape read byte c=%02x %d %ld %d %d\n", *data, tape->lrecl, tape->pos, tape->pos_buff, tape->len_buff);
     return 1;
}

/*
 * Peek at the next character but don't move pointer.
 *
 * Return -1 if file read error.
 *        0 if at end of file.
 *        1 if character read.
 */
static int
tape_peek_byte(struct _tape_buffer *tape, uint8_t *data)
{
     if (tape->file_name == NULL)
        return -1;
     /* Check if at end of buffer */
     /* Check if at end of buffer */
     if (tape->pos_buff >= tape->len_buff || tape->len_buff == 0) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
                 log_error("Tape write failed %s %d\n", tape->file_name, r);
                 return -1;
             }
             tape->pos += tape->len_buff;
             tape->len_buff = 0;
             tape->dirty = 0;
         }
         /* Advance tape by size of buffer */
         tape->pos += tape->len_buff;
         lseek(tape->fd, tape->pos, SEEK_SET);
         tape->len_buff = read(tape->fd, tape->buffer, sizeof(tape->buffer));
         tape->pos_buff = 0;
         if (tape->len_buff <= 0) {
             log_tape("Tape EOT\n");
             return 0;
         }
         log_tape("Tape buffer fill: %d\n", tape->len_buff);
     }
     *data = tape->buffer[tape->pos_buff];
     log_tape("Tape peek byte c=%02x %d %ld %d %d\n", *data, tape->lrecl, tape->pos, tape->pos_buff, tape->len_buff);
     return 1;
}

/*
 * write character to tape.
 *
 * Return -1 if file read error.
 *        1 if character written.
 */
static int
tape_write_byte(struct _tape_buffer *tape, uint8_t data)
{
     if (tape->file_name == NULL)
        return -1;
     /* Check if at end of buffer */
     if (tape->pos_buff >= sizeof(tape->buffer)) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
                 log_error("Tape write failed %s %d\n", tape->file_name, r);
                 return -1;
             }
             tape->pos += tape->len_buff;
             tape->dirty = 0;
         }
         tape->len_buff = 0;
         tape->pos_buff = 0;
     }
     tape->buffer[tape->pos_buff++] = data;
     tape->dirty = 1;
     if (tape->pos_buff > tape->len_buff)
        tape->len_buff = tape->pos_buff;
     log_tape("Write byte: (c=%02x p=%ld bp=%d bl=%d\n", data, tape->pos, tape->pos_buff, tape->len_buff);
     return 1;
}

/*
 * read the previous character.
 *
 * Return -1 if file read error.
 *        0 if at beginning of file.
 *        1 if character read.
 */
static int
tape_readbk_byte(struct _tape_buffer *tape, uint8_t *data)
{
     if (tape->file_name == NULL)
        return -1;
     /* Check if at end of buffer */
     if (tape->pos_buff == 0 || tape->len_buff == 0) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
                 log_error("Tape write failed %s %d\n", tape->file_name, r);
                 return -1;
             }
             tape->dirty = 0;
         }
         if (tape->format & TAPE_BOT) {
            return 0;
         }
         if (tape->pos == 0) {
            *data = tape->buffer[tape->pos_buff];
            tape->format |= TAPE_BOT;
            tape->pos_buff = 0;
            tape->len_buff = 0;
            return 0;
         } else {
             int       opos = -1;

             /* Backup current buffer first position */
             if (tape->pos < sizeof(tape->buffer)) {
                 opos = tape->pos;
                 tape->pos = 0;
             } else {
                 tape->pos -= sizeof(tape->buffer);
             }
             lseek(tape->fd, tape->pos, SEEK_SET);
             tape->len_buff = read(tape->fd, tape->buffer, sizeof(tape->buffer));
             if (opos == -1) {
                 tape->pos_buff = tape->len_buff;
             } else {
                 tape->pos_buff = opos;
             }
         }
         tape->format &= ~TAPE_EOT;
     }
     *data = tape->buffer[--tape->pos_buff];
     log_tape("Tape readbk byte c=%02x %d %ld %d %d\n", *data, tape->lrecl, tape->pos, tape->pos_buff, tape->len_buff);
     return 1;
}


/*
 * update a previous byte.
 *
 */
static int
tape_write_prev(struct _tape_buffer *tape, uint8_t data)
{
     long   pos = tape->srec - tape->pos;

     if (tape->file_name == NULL)
        return -1;
     /* Check if within buffer */
     log_tape("Write prev %ld %ld %02x\n", tape->srec, pos, data);
     if (pos >= 0 && pos < sizeof(tape->buffer)) {
         tape->buffer[pos] = data;
         if (pos > tape->len_buff) {
             tape->len_buff = pos;
         }
         tape->dirty = 1;
     } else {
         /* Point file at it and write one byte */
         lseek(tape->fd, tape->srec, SEEK_SET);
         if (write(tape->fd, &data, 1) != 1) {
             return -1;
         }
     }
     tape->srec++;
     return 1;
}


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

int
tape_write_start(struct _tape_buffer *tape)
{
     uint8_t      temp;
     int           r = 1;
     int           i;

     if (tape->file_name == NULL)
        return -1;
     if ((tape->format & WRITE_RING) == 0)
         return 2;
     tape->format &= ~(TAPE_BOT|TAPE_MARK);
     /* Save start of record, to update later */
     tape->srec = tape->pos + tape->pos_buff;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:
                   /* Write dummy record length */
                   temp = 0;
                   for (i = 0; i < 4; i++) {
                       r = tape_write_byte(tape, temp);
                       if (r != 1)
                           return r;
                   }
                   break;

     case TYPE_P7B:
                   break;
     }
     tape->lrecl = 0;
     tape->orecl = 0;
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_WRITE << FUNC_V;
     return 1;
}

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

int
tape_write_mark(struct _tape_buffer *tape)
{
     uint8_t      temp;
     int           r;
     int           i;

     if (tape->file_name == NULL)
        return -1;
     if ((tape->format & WRITE_RING) == 0)
         return 2;
     tape->format &= ~(TAPE_BOT|TAPE_MARK);
     /* Save start of record, to update later */
     tape->srec = tape->pos + tape->pos_buff;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:
                   /* Write dummy record length */
                   temp = 0;
                   for (i = 0; i < 4; i++) {
                       r = tape_write_byte(tape, temp);
                       if (r != 1)
                           return r;
                   }
                   break;

     case TYPE_P7B:
                   temp = (IRG_MASK|BCD_TM);
                   r = tape_write_byte(tape, temp);
                   if (r != 1)
                       return r;
                   break;
     }
     tape->lrecl = 0;
     tape->orecl = 0;
     tape->pos_frame += 1200;  /* Add in IRG based on 1600BPI tape */
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_MARK << FUNC_V;
     return 1;
}

/*
 * Start read forward.
 *
 * Return -1 if file read error.
 *         0 if at end of tape.
 *         1 if another record.
 *         2 if tape mark.
 */

int
tape_read_forw(struct _tape_buffer *tape)
{
     uint8_t      lrecl[4];
     int           r;
     int           i;
     int           j;
     int           k;

     if (tape->file_name == NULL)
        return -1;
     log_tape("tape_read_forw %04x %s\n", tape->format, tape->file_name);
     tape->format &= ~(TAPE_BOT|TAPE_MARK);
     if (tape->format & TAPE_EOT)
        return 0;
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_READ << FUNC_V;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:

                   tape->srec = tape->pos + tape->pos_buff;
                   /* Read in 4 byte logical record length */
                   for (i = 0; i < 4; i++) {
                       r = tape_read_byte(tape, &lrecl[i]);
                       if (r != 1)
                           return r;
                   }
                   tape->lrecl = lrecl[0] | ((uint32_t)lrecl[1] << 8) |
                                ((uint32_t)lrecl[2] << 16) | ((uint32_t)lrecl[3] << 24);
                   if (tape->lrecl == 0xffffffff) {
                       tape->format |= TAPE_EOT;
                       /* If we hit EOM marker, back up so if there is
                          a write, we will erase it */
                       for (i = 0; i < 4; i++) {
                           r = tape_readbk_byte(tape, &lrecl[i]);
                           if (r != 1)
                               return r;
                       }
                       return 0;
                   }
                   if (tape->lrecl == 0) {
                       tape->pos_frame += IRG_LEN;
                       tape->format |= TAPE_MARK;
                       log_tape("Tape mark\n");
                       return 2;
                   }
                   j = tape->lrecl;
                   if (j > tape->len_buff)
                       j = tape->len_buff;
                   k = 0;
                   while (j > 0 && (tape->pos_buff + k + 16) < sizeof(tape->buffer)) {
                       log_tape_s("data ");
                       for(i = 0; i < j && i < 16; i++)
                           log_tape_c ("%02x ", tape->buffer[tape->pos_buff + i + k]);
                       log_tape_c(" ");
                       for(i = 0; i < j && i < 16; i++) {
                           uint8_t ch = ebcdic_to_ascii[tape->buffer[tape->pos_buff + i + k]];
                           log_tape_c ("%c", isprint(ch) ? ch : '.');
                       }
                       j -= 16;
                       k += 16;
                   }

                   tape->orecl = tape->lrecl;
                   tape->lrecl = 0;
                   log_tape("Tape read forward: %d %d\n", tape->orecl, tape->pos_buff);
                   break;

     case TYPE_P7B:
                   /* Peek at current character. */
                   /* To see if it is a tape mark */
                   tape->srec = tape->pos + tape->pos_buff;
                   r = tape_peek_byte(tape, &lrecl[0]);
                   tape->lrecl = 2;
                   if (r != 1)
                       return r;
                   /* If tape mark, move over it */
                   if (lrecl[0] == (IRG_MASK|BCD_TM)) {
                       r = tape_read_byte(tape, &lrecl[0]);
                       if (r < 0)
                           return r;
                       tape->pos_frame += IRG_LEN;
                       tape->format |= TAPE_MARK;
                       log_tape("Tape mark %d\n", r);
                       return (r == 0) ? 0 : 2;
                   }
                   tape->lrecl = 0;  /* Flag at beginning of record */
                   break;
     }
     return 1;
}

/*
 * Start read backward.
 */

int
tape_read_back(struct _tape_buffer *tape)
{
     uint8_t       lrecl[4];
     int           r;
     int           i;

     if (tape->file_name == NULL)
        return -1;
     tape->format &= ~(TAPE_EOT|TAPE_MARK);
     if (tape->format & TAPE_BOT)
        return 0;
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_RDBACK << FUNC_V;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:

                   tape->srec = tape->pos + tape->pos_buff;
                   /* Read in 4 byte logical record length */
                   for (i = 3; i >= 0; i--) {
                       r = tape_readbk_byte(tape, &lrecl[i]);
                       if (r != 1)
                           return r;
                   }
                   tape->lrecl = lrecl[0] | ((uint32_t)lrecl[1] << 8) |
                                ((uint32_t)lrecl[2] << 16) | ((uint32_t)lrecl[3] << 24);
                   if (tape->lrecl == 0xffffffff) {
                       return 0;
                   }
                   /* If simH style tape, make sure even number of bytes per record */
                   if ((tape->format & TAPE_FMT) == TYPE_TAP && tape->lrecl & 1) {
                        uint8_t    temp;
                        r = tape_readbk_byte(tape, &temp);
                   }
                   if (tape->lrecl == 0) {
                       tape->pos_frame -= IRG_LEN;
                       tape->format |= TAPE_MARK;
                       log_tape("Tape mark\n");
                       return 2;
                   }
                   tape->orecl = tape->lrecl;
                   log_tape("Tape read backward: %d %d\n", tape->orecl, tape->pos_buff);
                   break;

     case TYPE_P7B:
                   /* Peek at previous character */
                   tape->srec = tape->pos + tape->pos_buff;
                   r = tape_readbk_byte(tape, &lrecl[0]);
                   if (r != 1)
                       return r;
                   tape->lrecl = 0;  /* Flag at beginning of record */
                   if (lrecl[0] == (IRG_MASK|BCD_TM)) {
                       tape->srec = tape->pos + tape->pos_buff;
                       tape->pos_frame -= IRG_LEN;
                       tape->lrecl = 2;
                       tape->format |= TAPE_MARK;
                       return 2;
                   } else {
                       /* If not mark, read back over skipped character */
                       r = tape_read_byte(tape, &lrecl[0]);
                   }
                   break;
     }
     return 1;
}

/*
 * Read one frame from tape.
 *
 * Return -1 if file read error.
 *         0 end of record.
 *         1 read frame.
 *         2 if read tape mark.
 */

int
tape_read_frame(struct _tape_buffer *tape, uint8_t *data)
{
     int r = -1;
     int l = ((tape->format & DEN_MASK) == DEN_800) ? 2 : 1;
     log_tape("tape_read_frame %04x %s\n", tape->format, (tape->file_name == NULL) ? "" : tape->file_name);

     if (tape->file_name == NULL)
        return -1;
     if (tape->format & TAPE_MARK)
         return 2;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_READ:
                       if (tape->lrecl >= tape->orecl)
                           return 0;
                       r = tape_read_byte(tape, data);
                       tape->lrecl++;
                       log_tape("Tape read frame: %d %d, %d\n", r, tape->lrecl, tape->orecl);
                       break;
                   case FUNC_WRITE:
                       break;
                   case FUNC_RDBACK:
                       if (tape->lrecl == 0)
                           return 0;
                       r = tape_readbk_byte(tape, data);
                       log_tape("Tape read bk frame: %d %d, %d\n", r, tape->lrecl, tape->orecl);
                       l = -l;
                       tape->lrecl--;
                       break;
                  }
                  break;
     case TYPE_P7B:
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_READ:
                       if (tape->lrecl == 2)
                           return 0;
                       r = tape_read_byte(tape, data);
                       if (tape->lrecl == 1 && (*data & IRG_MASK) != 0) {
                       r = tape_readbk_byte(tape, data);
                           tape->lrecl = 2;
                           return 0;
                       }
                       *data &= ~IRG_MASK;
                       tape->lrecl = 1;
                       break;
                   case FUNC_WRITE:
                       break;
                   case FUNC_RDBACK:
                       if (tape->lrecl == 2)
                           return 0;
                       r = tape_readbk_byte(tape, data);
                       if ((*data & IRG_MASK) != 0) {
                           tape->lrecl = 2;
                       } else {
                           tape->lrecl = 1;
                       }
                       *data &= ~IRG_MASK;
                       l = -l;
                       break;
                   }
                   break;
     }
     tape->pos_frame += l;
     return r;
}

/*
 * Write one frame to tape.
 */

int
tape_write_frame(struct _tape_buffer *tape, uint8_t data)
{
     log_tape("tape_write_frame %02x %d\n", data, tape->lrecl);
     if (tape->file_name == NULL)
        return -1;
     if ((tape->format & TAPE_FMT) == TYPE_P7B && tape->lrecl == 0) {
          data |= IRG_MASK;
     }
     tape->lrecl++;
     tape->pos_frame += ((tape->format & DEN_MASK) == DEN_800) ? 2 : 1;
     return tape_write_byte(tape, data);
}

/*
 * Finish a record.
 *
 *  Return:
 *    -1 if not opened.
 *     0 if at end of media.
 *     1 if ok.
 *     2 if at mark.
 */

int
tape_finish_rec(struct _tape_buffer *tape)
{
     uint8_t      lrecl[4];
     int           r;
     int           i, k, j, l;

     if (tape->file_name == NULL)
        return -1;
     log_tape("tape finish %04x %08x\n", tape->format, tape->lrecl);
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:
                   log_tape("tape finish rec e11/tap %d\n", (tape->format >> FUNC_V) & FUNC_M);
                   /* Nothing more to do if we either read or wrote a tape mark */
                   if (tape->format & TAPE_MARK) {
                       tape->format &= ~(FUNC_M << FUNC_V);
                       tape->format &= ~TAPE_MARK;
                       return 1;
                   }
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_READ:
                        log_tape(" Tape read end lrecl=%d\n", tape->lrecl);
                        /* Make sure we are at end of record. */
                        while (tape->lrecl < tape->orecl) {
                             r = tape_read_frame(tape, &lrecl[0]);
                        }
                        /* If simH style tape, make sure even number of bytes per record */
                        if ((tape->format & TAPE_FMT) == TYPE_TAP && tape->orecl & 1) {
                             r = tape_read_byte(tape, &lrecl[0]);
                        }
                        /* Read in 4 byte trailing record length logical record length */
                        for (i = 0; i < 4; i++) {
                            r = tape_read_byte(tape, &lrecl[i]);
                            if (r != 1)
                                return r;
                        }
                        tape->lrecl = lrecl[0] | ((uint32_t)lrecl[1] << 8) |
                                     ((uint32_t)lrecl[2] << 16) | ((uint32_t)lrecl[3] << 24);
                        if (tape->lrecl != tape->orecl) {
                            log_tape(" Tape read error lrecl != lrecl\n");
                        }
                        break;
                   case FUNC_WRITE:
                        /* If simH style tape, make sure even number of bytes per record */
                        lrecl[0] = 0;
                        if ((tape->format & TAPE_FMT) == TYPE_TAP && tape->lrecl & 1) {
                            (void)tape_write_byte(tape, lrecl[0]);
                        }
                        j = tape->srec - tape->pos;
                        if (j >= 0 && j < sizeof(tape->buffer)) {
                            l = tape->lrecl;
                            if (l > tape->len_buff)
                                l = tape->len_buff;
                            k = 0;
                            while (j > 0 && (j + k + 16) < sizeof(tape->buffer) && l > 0) {
                                log_tape_s("data ");
                                for(i = 0; i < l && i < 16; i++)
                                    log_tape_c ("%02x ", tape->buffer[j + i + k]);
                                log_tape_c(" ");
                                for(i = 0; i < l && i < 16; i++) {
                                    uint8_t ch = ebcdic_to_ascii[tape->buffer[j + i + k]];
                                    log_tape_c ("%c", isprint(ch) ? ch : '.');
                                }
                                l -= 16;
                                k += 16;
                            }
                        }

                        lrecl[0] = tape->lrecl & 0xff;
                        lrecl[1] = (tape->lrecl >> 8) & 0xff;
                        lrecl[2] = (tape->lrecl >> 16) & 0xff;
                        lrecl[3] = (tape->lrecl >> 24) & 0xff;
                        /* Write 4 byte leading record */
                        for (i = 0; i < 4; i++) {
                            r = tape_write_prev(tape, lrecl[i]);
                            if (r != 1)
                                return r;
                        }
                        /* Write 4 byte trailing record */
                        for (i = 0; i < 4; i++) {
                            r = tape_write_byte(tape, lrecl[i]);
                            if (r != 1)
                                return r;
                        }
                        break;
                   case FUNC_MARK:
                   case FUNC_REW:
                        break;
                   case FUNC_RDBACK:
                        /* Make sure we are at end of record. */
                        log_tape(" Tape read bk lrecl=%d\n", tape->lrecl);
                        while (tape->lrecl > 0) {
                             r = tape_read_frame(tape, &lrecl[0]);
                             log_tape(" Tape read bk lrecl=%d\n", tape->lrecl);
                        }
                        /* Read in 4 byte trailing record length logical record length */
                        for (i = 0; i < 4; i++) {
                            r = tape_readbk_byte(tape, &lrecl[3 - i]);
                            if (r != 1)
                                return r;
                        }
                        tape->lrecl = lrecl[0] | ((uint32_t)lrecl[1] << 8) |
                                     ((uint32_t)lrecl[2] << 16) | ((uint32_t)lrecl[3] << 24);
                        log_tape(" Tape read bk lrecl=%d %d length\n", tape->lrecl, tape->orecl);
                        if (tape->lrecl != tape->orecl) {
                            log_tape(" Tape read error lrecl != lrecl\n");
                        }
                        break;
                   }
                   break;
     case TYPE_P7B:
                   log_tape("tape finish rec p7b %d\n", (tape->format >> FUNC_V) & FUNC_M);
                   if (tape->format & TAPE_MARK) {
                       tape->format &= ~TAPE_MARK;
                       break;
                   }
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_WRITE:
                        break;
                   case FUNC_MARK:
                   case FUNC_REW:
                        break;
                   case FUNC_READ:
                        /* If not at end of record, read until IRG */
                        while (tape->lrecl != 2) {
                            r = tape_read_frame(tape, &lrecl[0]);
                            if (r < 0)
                               return r;
                        }
                        break;
                   case FUNC_RDBACK:
                        /* If not at end of record, read until IRG */
                        while (tape->lrecl != 2) {
                            r = tape_read_frame(tape, &lrecl[0]);
                            if (r < 0)
                               return r;
                        }
                        break;
                   }
                   break;
     }
     tape->format &= ~(FUNC_M << FUNC_V);
     return 1;
}


int
tape_start_rewind(struct _tape_buffer *tape)
{
    /* If buffer is dirty flush it to the file */
    if (tape->dirty) {
        int     r;
        lseek(tape->fd, tape->pos, SEEK_SET);
        r = write(tape->fd, tape->buffer, tape->len_buff);
        if (r != tape->len_buff) {
            return -1;
        }
        tape->dirty = 0;
    }
    tape->pos = 0;
    tape->pos_buff = 0;
    tape->len_buff = 0;
    return 1;
}

/*
 * Rewind tape number of frames.
 */

int
tape_rewind_frames(struct _tape_buffer *tape, int frames)
{
    log_tape("Rewind %ld %d\n", tape->pos_frame, frames);
     if (tape->file_name == NULL)
        return -1;
     if (tape->pos_frame < frames) {
         tape->pos_frame = 0;
         tape->format |= TAPE_BOT;
         log_tape("Rewind done\n");
         return 0;
     }
     tape->pos_frame -= frames;
     log_tape("Rewinding %ld\n", tape->pos_frame);
     return 1;
}

/*
 * Find the supply reel tape position.
 */

struct _tape_image *
tape_supply_image(struct _tape_buffer *tape, int *rotate)
{
    int top = max_tape_pos - 1;
    int pos;
    int len;
    int index;

    pos = max_tape_length - tape->pos_frame;
    for (index = top; index > 0; index--) {
        if (pos >= tape_position[index].start && pos <= tape_position[index+1].start)
           break;
    }
    len = pos - tape_position[index].start;
    *rotate = (int)(35.0f * (((float)len) / ((float)tape_position[index].length)));
    return &tape_position[index];
}

/*
 * Find the takup reel tape position.
 */

struct _tape_image *
tape_takeup_image(struct _tape_buffer *tape, int *rotate)
{
    int top = max_tape_pos - 1;
    int pos;
    int len;
    int index;

    if (tape->file_name == NULL)
       return &tape_position[0];
    pos = tape->pos_frame;
    for (index = 0; index < top; index++) {
        if (pos >= tape_position[index].start && pos <= tape_position[index+1].start)
           break;
    }
    len = pos - tape_position[index].start;
    *rotate = (int)(36.0f * (((float)len) / ((float)tape_position[index].length)));
    return &tape_position[index];
}

void
tape_init()
{
    float     length, radius, cir;
    int       i;
    int       fpi;
    int       f;
    int       xind, yind;
    int       xpos;
    int       ypos;
    int       step;
    int       last;

    length = 0.0f;
    f = 0;
    i = 0;
    xind = 0;
    yind = 1;
    xpos = 0;
    ypos = 75;
    step = 0;
    last = 0;
    fpi = (int)(M_PI * 5.125f * 1600.0f);

    for(radius = 5.125f; length < (2400.0f * 12.0f); radius += 0.003f) {
        cir = M_PI * radius;
        fpi = (int)(cir * 1600.0f);
        length += cir;
        xpos = 75 * xind;
        ypos = 75 * yind;

        tape_position[i].x = xpos;
        tape_position[i].y = ypos;
        tape_position[i].start = f;
        tape_position[i].length = fpi;
        tape_position[i].radius = (int)(radius * 3.1f);

        f += fpi;
        i++;
        step++;
        if (step > 32) {
           step = 0;
           yind ++;
           if (yind > 15 && xind < 2) {
              yind = 0;
              xind++;
           }
           if (xind == 2 && yind > 7) {
              yind = 7;
           }
        }
    }

    tape_position[i].x = xpos;
    tape_position[i].y = ypos;
    tape_position[i].start = f;
    tape_position[i].length = fpi;
    tape_position[i].radius = (int)(radius * 3.1f);

    last = i;
    i++;
    while (i < 1300) {
        tape_position[i].x = 0;
        tape_position[i].y = 0;
        tape_position[i].start = 0;
        tape_position[i].length = 0;
        tape_position[i].radius = 0;
        i++;
    }
    max_tape_length = f;
    max_tape_pos = last;
}

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

   Rewind speed  2415   3840 frames per 20ms



#endif

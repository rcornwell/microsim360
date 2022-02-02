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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>
#include "tape.h"

#include "tape_position.h"

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
    /* If buffer is dirty flush it to the file */
    if (tape->dirty) {
        lseek(tape->fd, tape->pos, SEEK_SET);
        (void)write(tape->fd, tape->buffer, tape->len_buff);
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
             printf("Tape EOT\n");
             return -1;
         }
         printf("Tape buffer fill: %d\n", tape->len_buff);
     }
     *data = tape->buffer[tape->pos_buff++];
     printf("Tape read byte c=%02x %d %ld %d %d\n", *data, tape->lrecl, tape->pos, tape->pos_buff, tape->len_buff);
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
     if (tape->pos_buff >= tape->len_buff || tape->len_buff > 0) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
                 return -1;
             }
             tape->pos += tape->len_buff;
             tape->len_buff = 0;
             tape->dirty = 0;
         }
         tape->pos += tape->len_buff;
         lseek(tape->fd, tape->pos, SEEK_SET);
         tape->len_buff = read(tape->fd, tape->buffer, sizeof(tape->buffer));
         tape->pos_buff = 0;
         if (tape->len_buff <= 0) {
             tape->format |= TAPE_EOT;
             return 0;
         }
         printf("Tape buffer fill: %d\n", tape->len_buff);
     }
     *data = tape->buffer[tape->pos_buff];
     printf("Tape peek: (c=%02x p=%ld bp=%d bl=%d\n", *data, tape->pos, tape->pos_buff, tape->len_buff);
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
     if (tape->pos_buff > sizeof(tape->buffer)) {
         /* If buffer is dirty flush it to the file */
         if (tape->dirty) {
             int     r;
             lseek(tape->fd, tape->pos, SEEK_SET);
             r = write(tape->fd, tape->buffer, tape->len_buff);
             if (r != tape->len_buff) {
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
     printf("Write byte: (c=%02x p=%ld bp=%d bl=%d\n", data, tape->pos, tape->pos_buff, tape->len_buff);
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
     if (tape->pos_buff == 0) {
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
         if (tape->pos != 0) {
             /* Backup current buffer first position */
             tape->pos_buff = tape->pos;
             if (tape->pos < tape->len_buff)
                 tape->pos = 0;
             else 
                 tape->pos -= tape->len_buff;
             tape->pos_buff -= tape->pos;
             lseek(tape->fd, tape->pos, SEEK_SET);
             tape->len_buff = read(tape->fd, tape->buffer, sizeof(tape->buffer));
             tape->pos_buff = tape->len_buff;
         }
         tape->format &= ~TAPE_EOT;
         if (tape->pos == 0 && tape->pos_buff == 0) {
            tape->format |= TAPE_BOT;
            return 0;
         }
     }
     *data = tape->buffer[--tape->pos_buff];
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
printf("Write prev %ld %ld %02x ", tape->srec, pos, data);
     if (pos >= 0 && pos < sizeof(tape->buffer)) {
         tape->buffer[pos] = data;
printf("buffered ");
         if (pos > tape->len_buff) {
             tape->len_buff = pos;
         }
         tape->dirty = 1;
     } else {
         /* Point file at it and write one byte */
printf("direct ");
         lseek(tape->fd, tape->srec, SEEK_SET);
         if (write(tape->fd, &data, 1) != 1) {
             return -1;
         }
     }
printf("\n");
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
     tape->format |= TAPE_MARK;  /* Indicate we wrote tape mark */
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

     if (tape->file_name == NULL)
        return -1;
     tape->format &= ~(TAPE_BOT|TAPE_MARK);
     if (tape->format & TAPE_EOT)
        return 0;
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
printf("\nTape mark\n");
                       return 2;
                   }
                   j = tape->lrecl;
                   if (j > 100)
                       j = 100;
                   for(i = 0; i < j; i++)
                       printf ("%02x ", tape->buffer[tape->pos_buff + i]);

                   tape->orecl = tape->lrecl;
                   tape->lrecl = 0; 
     printf("\nTape read forward: %d\n", tape->orecl);
                   break;
          
     case TYPE_P7B:
                   /* Peek at current character. */
                   /* To see if it is a tape mark */
                   tape->srec = tape->pos + tape->pos_buff;
                   tape->lrecl = 2;  /* Flag at end of record */
                   r = tape_peek_byte(tape, &lrecl[0]);
                   if (r != 1)
                       return r;
                   if (lrecl[0] == (IRG_MASK|BCD_TM)) {
                       tape->pos_frame += IRG_LEN;
                       tape->format |= TAPE_MARK;
                       return 2;
                   }
                   tape->lrecl = 0;  /* Flag at beginning of record */
                   break;
     } 
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_READ << FUNC_V;
     return 1;
}

/*
 * Start read backward.
 */

int
tape_read_back(struct _tape_buffer *tape)
{
     uint8_t      lrecl[4];
     int           r;
     int           i;
     int           j;

     if (tape->file_name == NULL)
        return -1;
     tape->format &= ~(TAPE_EOT|TAPE_MARK);
     if (tape->format & TAPE_BOT)
        return 0;
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
                   if ((tape->format & TAPE_FMT) == TYPE_TAP && tape->orecl & 1) {
                        r = tape_readbk_byte(tape, &lrecl[0]);
                   }
                   if (tape->lrecl == 0) {
                       tape->pos_frame -= IRG_LEN;
                       tape->format |= TAPE_MARK;
printf("\nTape mark\n");
                       return 2;
                   }
                   tape->orecl = tape->lrecl;
                   j = tape->lrecl;
                   if (j > 100)
                       j = 100;
                   for(i = 0; i < j; i++)
                       printf ("%02x ", tape->buffer[tape->pos_buff + i]);

     printf("\nTape read backward: %d\n", tape->orecl);
                   tape->orecl = tape->lrecl;
                   break;
          
     case TYPE_P7B:
                   /* Peek at current character. */
                   r = tape_readbk_byte(tape, &lrecl[0]);
                   if (r != 1)
                       return r;
                   tape->srec = tape->pos + tape->pos_buff;
                   tape->lrecl = 0;  /* Flag at beginning of record */
                   if (lrecl[0] == (IRG_MASK|BCD_TM)) {
                       tape->lrecl = 2;
                       tape->format |= TAPE_MARK;
                       return 1;
                   }
                   /* Was not a tape mark, undo read backward */
                   (void)tape_read_byte(tape, &lrecl[0]);
                   break;
     } 
     tape->format &= ~(FUNC_M << FUNC_V);
     tape->format |= FUNC_RDBACK << FUNC_V;
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
     printf("tape_read_frame\n");
     
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
                       printf("Tape read frame: %d %d, %d\n", r, tape->lrecl, tape->orecl);
                       break;
                   case FUNC_WRITE:
                       break;
                   case FUNC_RDBACK:
                       if (tape->lrecl == 0)
                           return 0;
                       r = tape_readbk_byte(tape, data);
                       printf("Tape read bk frame: %d %d, %d\n", r, tape->lrecl, tape->orecl);
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
                       if ((*data & IRG_MASK) != 0) {
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
                       tape->lrecl = 1;
                       if ((*data & IRG_MASK) != 0) {
                           tape->lrecl = 2;
                       }
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
     printf("tape_write_frame %02x %d\n", data, tape->lrecl);
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
     int           i;

     if (tape->file_name == NULL)
        return -1;
     if (tape->lrecl == 0xffffffff)
         return 0;
     /* Nothing more to do if we either read or wrote a tape mark */
     if (tape->format & TAPE_MARK)
         return 2;
     switch(tape->format & TAPE_FMT) {
     case TYPE_TAP:
     case TYPE_E11:
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_READ:
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
                            printf(" Tape read error lrecl != lrecl\n");
                        }
                        break;
                   case FUNC_WRITE:
                        /* If simH style tape, make sure even number of bytes per record */
                        lrecl[0] = 0;
                        if ((tape->format & TAPE_FMT) == TYPE_TAP && tape->lrecl & 1) {
                            (void)tape_write_byte(tape, lrecl[0]);
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
                        while (tape->lrecl > 0) {
                             r = tape_read_frame(tape, &lrecl[0]);
                        }
                        /* Read in 4 byte trailing record length logical record length */
                        for (i = 3; i >= 0; i--) {
                            r = tape_readbk_byte(tape, &lrecl[i]);
                            if (r != 1)
                                return r;
                        }
                        tape->lrecl = lrecl[0] | ((uint32_t)lrecl[1] << 8) |
                                     ((uint32_t)lrecl[2] << 16) | ((uint32_t)lrecl[3] << 24);
                        if (tape->lrecl != tape->orecl) {
                            printf(" Tape read error lrecl != lrecl\n");
                        }
                        break;
                   }
                   break;
     case TYPE_P7B:
                   switch ((tape->format >> FUNC_V) & FUNC_M) {
                   case FUNC_WRITE:
                        break;
                   case FUNC_MARK:
                   case FUNC_REW:
                        break;
                   case FUNC_READ:
                   case FUNC_RDBACK:
                        /* If not at end of record, read until IRG */
                        while (tape->lrecl != 2) {
                            r = tape_read_frame(tape, &lrecl[0]);
                        }
                        break;
                   }
                   break;
     } 
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
    printf("Rewind %ld %d\n", tape->pos_frame, frames);
     if (tape->file_name == NULL)
        return -1;
     if (tape->pos_frame < frames) {
         tape->pos_frame = 0;
         tape->format |= TAPE_BOT;
         printf("Rewind done\n");
         return 0;
     }
     tape->pos_frame -= frames;
     printf("Rewinding %ld\n", tape->pos_frame);
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
    for (index = top; index >= 0; index--) {
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
    *rotate = (int)(35.0f * (((float)len) / ((float)tape_position[index].length)));
    return &tape_position[index];
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
 


#define     MT_is_loading_tape  1
#define     MT_is_rewinding     2
#define     MT_is_using_tape    3


int PARAM_Reel_Diameter  =   267;  // reel diameter in mm
int PARAM_RPM            =  1140;  // reel forward/backwards motor revolutions per minute
int PARAM_VacCol_h_Low   = 15000;  // upper vacuum colum sensor (inches x 1000) triggers reel load medium on colum 
int PARAM_VacCol_h_Hi    = 40000;  // lower vacuum colum sensor (inches x 1000) triggers reel take medium from colum 
int PARAM_RWSpeed        =    75;  // tape head operates at 75 inches per sec
int PARAM_AccelTime      =   145;  // accel time in msec. clutch must obtain 2/3 of maximum speed in 0.135-0.150 seconds with a full reel of tape
int PARAM_DecelTime      =   155;  // decel time in msec. the stop clutch must stop a full reel of tape from full speed in 0.145-0.160 sec
                                   // note: the accel/decel time increases when tape unit
                                   // get used. Lower values makes jerky spins with thigh 
                                   // oscilation of tape loop in vacuum columns. Higher values
                                   // makes smother spins, with more ample oscilations of tape loop
int PARAM_HeadOpenTime   =  1700;  // time needed by r/w tape head to fully open or close
int PARAM_TakeMotor_RPM  =    40;  // take motor revolutions per minute
int PARAM_HiSpeedRwdSpeed =  500;  // High Speed rewind at 500 inches/sec 


#define     MT_anim_step_nop       -1             // do nothing, just keeps current MT state values
#define     MT_anim_step_inc        0             // incremental animation step
#define     MT_anim_step_rw         1             // read/write tape animation step 
#define     MT_anim_step_HiRew      2             // High Speed Rewind animation step 
#define     MT_anim_finished       99             // this is the final step that signals the animation sequence has finished

void mt_reels_mov(int unit, int cmd, 
                  int * L_VacColMedium_h, int * R_VacColMedium_h, 
                  int * MT_L_Rot, int * MT_R_Rot,
                  int * MT_Reel_Amount, int * MT_head);

// execute animation step. return 1 if animation terminated
int mt_do_animation_seq(int unit, 
                  int * L_VacColMedium_h, int * R_VacColMedium_h, 
                  int * MT_L_Rot, int * MT_R_Rot,
                  int * MT_Reel_Amount, int * MT_head)
{
    int time, nseq, msec, hint, recsize, ang, n, m, n1, n2, u3, u3_dec, p; 
    int tnow = Refresh_tnow; 

    time = tnow - mtcab[unit].nseq_tm0;
    if (time < 0) return 1; // end of sequence
    nseq=0;
    for(;;) {
        msec=mtcab[unit].seq[nseq].msec;
        if (msec==0) return 1;              // exit beacuse end of sequence
        // if time>0 -> we have past this animation step. Only execute pending incremental steps
        // if time < 0 -> we are at this amimation step, so execute it and return 
        time=time-msec;                     
        // type of step
        hint = mtcab[unit].seq[nseq].hint;  
        if (hint == MT_anim_finished) return 1; // exit beacuse end of sequence
        if ((hint == MT_anim_step_nop) && (time < 0)) {
            // set current values for this point of sequence, do nothing
            // already done incremental step. Just keep current state values
            *MT_Reel_Amount = (int) GetState(IBM650.MT[unit]); 
            *MT_L_Rot = (int) GetState(IBM650.MT_L[unit]) % 64; // keeps blur
            *MT_R_Rot = (int) GetState(IBM650.MT_R[unit]) % 64;                           
            *MT_head =  (int) GetState(IBM650.MT_head[unit]);  
            *L_VacColMedium_h = mtcab[unit].reel[0].VacCol_h;
            *R_VacColMedium_h = mtcab[unit].reel[1].VacCol_h;
            return 0; // exit after step execution. tnow is in this step
        } else if (hint == MT_anim_step_inc) { 
            // This is incremental animation step, should be done allways.
            // Execute it and mark as done to avoid issuing it more than once
            mtcab[unit].seq[nseq].hint=MT_anim_step_nop; 
            // apply this step reel angular increment to reels 
            ang = mtcab[unit].reel[0].ang + mtcab[unit].seq[nseq].L_ang_inc; 
            ang = ang % 360; if (ang < 0) ang +=360;
            mtcab[unit].reel[0].ang = ang; 
            ang = mtcab[unit].reel[1].ang + mtcab[unit].seq[nseq].R_ang_inc; 
            ang = ang % 360; if (ang < 0) ang +=360;
            mtcab[unit].reel[1].ang = ang; 
            // apply this step tape medium increment into vacuum cols
            mtcab[unit].reel[0].VacCol_h += mtcab[unit].seq[nseq].L_VacCol_inc; 
            mtcab[unit].reel[1].VacCol_h += mtcab[unit].seq[nseq].R_VacCol_inc; 
            // set the tape position of its elements
            *MT_Reel_Amount = mtcab[unit].seq[nseq].MT_Reel_Amount; 
            if (mtcab[unit].reel[0].color == 1) {
               *MT_L_Rot = mtcab[unit].reel[0].n = (mtcab[unit].reel[0].ang % 120) * 24 / 120;
            } else {
               *MT_L_Rot = mtcab[unit].reel[0].n = mtcab[unit].reel[0].ang * 24 / 360; // get current angular position of reels
            }
            if (mtcab[unit].reel[1].color == 1) {
               *MT_R_Rot = mtcab[unit].reel[1].n = (mtcab[unit].reel[1].ang % 120) * 24 / 120;
            } else {
               *MT_R_Rot = mtcab[unit].reel[1].n = mtcab[unit].reel[1].ang * 24 / 360;
            }
            *MT_head  = mtcab[unit].seq[nseq].MT_head; 
            *L_VacColMedium_h = mtcab[unit].reel[0].VacCol_h;
            *R_VacColMedium_h = mtcab[unit].reel[1].VacCol_h;
            // clear 
            mtcab[unit].reel[0].motor = mtcab[unit].reel[1].motor = 0;
            mtcab[unit].reel[0].tm0 = mtcab[unit].reel[1].tm0 = 0;
            mtcab[unit].reel[0].rpm0 = mtcab[unit].reel[1].rpm0 = 0;
            mtcab[unit].reel[0].ang0 = mtcab[unit].reel[1].ang0 = 0;
            mtcab[unit].reel[0].revs = mtcab[unit].reel[1].revs = 0;
            mtcab[unit].reel[0].rpm = mtcab[unit].reel[1].rpm = 0;
            // do not update mtcab[unit].L|R_VacColMedium_h0
            mtcab[unit].rw_msec = mtcab[unit].rw_recsize = 0;
            // incremental animation step done. Now check time variable. if time < 0 
            // then this is the current amimation step, so return 
            // on the contrary, it time >= 0 this is a pending incremental step that has
            // been executed. continue the main loop execute any other pending incremental
            // step until we arrive at the current animantion step
            if (time < 0) return 0; // exit after step execution. tnow is in this step
        } else if ((hint == MT_anim_step_rw) && (time < 0)) {
            // this step is regualar tape mevement made by the r/w header, either stoped, backwards
            // or forwards, at 75 inch/sec. Simulate also reel spinning to take/load medium in vac col
            // animation variables usage
            //     temporary: seq[nseq].MT_Reel_Amount
            //        to hold tm0 value, to determine the time elapsed from previous refresh
            //     parameter: seq[nseq].MT_head  
            //        to hold the tape movement direction. Can be 1 (forward), -1 (backwards)
            //        or zero (tape movement stoped, but update reels spinning if some not
            //        yet stopped)
            //     parameter: seq[nseq].L_VacCol_inc  
            //        if >0 is the amount of medium to rew at low speed rew/to move fwd or backwrds. 
            //        update mtcab[unit].rew_u3 with the amount of tape remaining to be rew
            //     parameter: seq[nseq].R_VacCol_inc  
            //        if =0 is low speed rew -> update rew_u3
            //        if =1 is moving tape forward/backwards -> update u3;
            u3 = mtcab[unit].seq[nseq].L_VacCol_inc;
            m  = msec+time;  // m=msec+time = time elapsed in this step. msec = this step duration
            time = tnow - mtcab[unit].seq[nseq].MT_Reel_Amount;
            mtcab[unit].seq[nseq].MT_Reel_Amount = tnow;
            if ((time < 0) || (time > 1000)) time=0; // skip this step because is not init yet/invalid                
            n=mtcab[unit].seq[nseq].MT_head; 
            recsize  = n * time * PARAM_RWSpeed; // inches x1000, 
            recsize = recsize / 2; 
            mtcab[unit].reel[0].VacCol_h -= recsize;
            mtcab[unit].reel[1].VacCol_h += recsize;
            mt_reels_mov(unit, 0, 
                     L_VacColMedium_h, R_VacColMedium_h, 
                     MT_L_Rot, MT_R_Rot, 
                     MT_Reel_Amount, MT_head);
            if (u3) {
                if (mtcab[unit].seq[nseq].R_VacCol_inc==0) {
                    // update mtcab[unit].rew_u3 to signal progress of rewind

                    p = 1000 - 1000*m / msec; 
                    if (p>1000) p=1000; if (p<0) p=0; // m=0..1000 is % x10 of time of step remaining
                    // calculate rew_u3 = how much medium has been rew 
                    mtcab[unit].rew_u3 = (int) (((t_int64) (u3)) * ((t_int64) (p)) / 1000);
                } else {
                    // update mt_unit[unit].u3 to signal progress of r/w
                    mt_unit[unit].u3 = (int) (1.0 * u3 * (n < 0 ? msec-m:m) / msec); // to avoid int overflow
                }
                // check fast mode
                if (CpuSpeed_Acceleration == -1) {
                    // if Key Control-F (^F, Ctrl F) being pressed 
                    // reduce duration of this step, make it done when no duration left
                    time = msec - m; // time remaining 
                    m = mtcab[unit].seq[nseq].msec - time / 20; // reduce a fraction of remaining time
                    if (m > 0) {
                        mtcab[unit].seq[nseq].msec = m; 
                    } else {
                        mtcab[unit].seq[nseq].hint=MT_anim_step_nop; 
                        mtcab[unit].seq[nseq].msec = 1; 
                    }
                }
            }       
            return 0; // exit after step execution. tnow is in this step
        } else if ((hint == MT_anim_step_HiRew) && (time < 0)) {
            // high speed rewind 
            // animation variables usage
            //     parameter: seq[nseq].MT_head  
            //        acceleration flag. Can be 1 (acceelerate), -1 (decelerate),
            //        or zero (full speed)
            //     parameter: seq[nseq].L_VacCol_inc  
            //        is the amount of medium at beginning of step
            //     parameter: seq[nseq].R_VacCol_inc  
            //        is the amount of medium to rew 
            //        update mtcab[unit].rew_u3 with the amount of tape remaining to be rew
            u3     = mtcab[unit].seq[nseq].L_VacCol_inc;
            u3_dec = mtcab[unit].seq[nseq].R_VacCol_inc;
            n=mtcab[unit].seq[nseq].MT_head; 
            *MT_Reel_Amount = mtcab[unit].seq[nseq].MT_Reel_Amount; 
            n1 = (int) GetState(IBM650.MT_L[unit]) % 64; 
            n2 = (int) GetState(IBM650.MT_R[unit]) % 64; 
            // msec+time = msecs elapsed of this animation step
            // -time     = msecs remaining to be done on this animation steop until it ends
            // use it on acceleration/deceleration 
            m = msec+time; 
            if (n!=0) { // acceleration/deceleration
                int d;
                if (n<0) m = msec-m; // deceleration -> reverse m time used
                if (mtcab[unit].reel[0].color == 1) {
                    d = (m<msec/3) ? 4 : (m<2*msec/3) ? 5 : 9;
                } else {
                    d = (m<msec/3) ? 2 : (m<2*msec/3) ? 3 : 5;
                }
                n1-=d;
                while (n1 < 24) n1+=24; while (n1 >= 48) n1-=24; 
                if (mtcab[unit].reel[1].color == 1) {
                    d = (m<msec/3) ? 4 : (m<2*msec/3) ? 5 : 9;
                } else {
                    d = (m<msec/3) ? 2 : (m<2*msec/3) ? 3 : 5;
                }
                n2-=d;
                while (n2 < 24) n2+=24; while (n2 >= 48) n2-=24; 
            } else { // full speed
                n1-=3;
                while (n1 < 48) n1+=48; while (n1 >= 48+8) n1-=8; 
                n2-=3;
                while (n2 < 48) n2+=48; while (n2 >= 48+8) n2-=8; 
            }
            if (u3) {
                // update mtcab[unit].rew_u3 to signal progress of rewind
                // msec+time = time elapsed in this step. msec = this step duration
                m = msec+time; 
                m = 100*m / msec; if (m>100) m=100; if (m<0) m=0; // m=0..100 is % of time of step elapsed
                // calculate rew_u3 = how much medium has been rew 
                mtcab[unit].rew_u3 = u3 - u3_dec * m / 100;
                // printf("unit %d, rew_u3 %d elapsed %d msec %d\t\n", unit, mtcab[unit].rew_u3, msec+time, msec);
            }
            // check fast mode
            if (CpuSpeed_Acceleration == -1) {
                // if Key Control-F (^F, Ctrl F) being pressed and remains more that 500 msec
                // to finish this step, then set duration to 500 msec
                mtcab[unit].seq[nseq].msec = 400;
                n1=56; // make L reel transparent
            }
            *MT_head  = 11;               // head wide open
            *MT_L_Rot = n1;
            *MT_R_Rot = n2;
            *L_VacColMedium_h = 0;        // vac col empty
            *R_VacColMedium_h = 0;        // vac col empty
            return 0; // exit after step execution. tnow is in this step
        }
        nseq++; 
    }
    return 0;
}

// dump to console the current animation
void mt_dump_animation_seq(int unit)
{
    int i; 

    printf("Animation set for unit %d\r\n", unit);

    for(i=0;i<mtcab[unit].nseq;i++) {
        printf("nseq %d, hint %d, msec %d, Amnt %d, La=%d, Ra=%d, Lh=%d, Rh=%d, Hd %d \r\n", 
            i, mtcab[unit].seq[i].hint, mtcab[unit].seq[i].msec, mtcab[unit].seq[i].MT_Reel_Amount,
            mtcab[unit].seq[i].L_ang_inc, mtcab[unit].seq[i].R_ang_inc, 
            mtcab[unit].seq[i].L_VacCol_inc, mtcab[unit].seq[i].R_VacCol_inc, 
            mtcab[unit].seq[i].MT_head);
    }
}

// add animation sequence step 
void AddSeq(int unit, int msec, int hint, 
            int L_VacCol_inc, int R_VacCol_inc, 
            int L_ang_inc, int R_ang_inc,
            int MT_Reel_Amount, int MT_head)
{
    int i; 
    i=mtcab[unit].nseq++; // inc sequence
    if (i>=MT_anim_sequence_len) {
        fprintf(stderr, "Cannot add MT sequence step. Animation array full\n");
        mtcab[unit].seq[MT_anim_sequence_len-1].msec = 0; // mark last as end of sequence
        return;
    }
    if ((msec == 0) || (hint == MT_anim_finished)) {
        msec = 0; 
        hint = MT_anim_finished;
    }

    mtcab[unit].seq[i].msec = msec; 
    mtcab[unit].seq[i].hint = hint; 
    mtcab[unit].seq[i].MT_Reel_Amount = MT_Reel_Amount; 
    mtcab[unit].seq[i].L_ang_inc = L_ang_inc; 
    mtcab[unit].seq[i].R_ang_inc = R_ang_inc; 
    mtcab[unit].seq[i].L_VacCol_inc = L_VacCol_inc; 
    mtcab[unit].seq[i].R_VacCol_inc = R_VacCol_inc; 
    mtcab[unit].seq[i].MT_head = MT_head;
}

// add to the current animation sequence the load animation:
//    - spin reels slowly to load medium on vacuum columns
//    - close r/w head
void mt_add_load_seq(int unit)
{
    int MT_Reel_Amount,  MT_head; // state of controls at end of of redraw
    int i, L_h, R_h, L_inc_h, R_inc_h, nHead, msec, ang_inc, r1, r2;

    MT_Reel_Amount   = 1;    // all tape medium on L tape
    MT_head          = 11;   // head full open

    // head needs 1800 msec to go from wide open to close
    // reels rotates 360 + 90 + 45 gr during this time (take motor goes at 46 rpm)
    // prepare animation sequence each given msec
    msec=33;                                   // time for animation step 
    ang_inc = (msec * PARAM_TakeMotor_RPM * 360) / (60 * 1000); // angle reel spins on each animation step
    nHead   = PARAM_HeadOpenTime / msec;      // number of steps in animation of head closing

    // calculate the amount of tape medium that reel loads into vaccol on each step given reel rotation
    r1=(50 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel empty
    r2=(90 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel full
    r1=(int) (0.0393701 * r1 * 1000);          // reel radius in inches x 1000
    r2=(int) (0.0393701 * r2 * 1000);            
    L_inc_h = (int) (r2 * 2 * 3.1416 * ang_inc / 360); // beacuse L reel is full. 
    R_inc_h = (int) (r1 * 2 * 3.1416 * ang_inc / 360); // beacuse R reel is empty
    L_inc_h = L_inc_h  / 2;                            // tape loop position is half on medium loaded
    R_inc_h = R_inc_h  / 2; 

    // tape medium loads first on R vacuum colum. When tape pass upper vaccol sensor
    // the medium starts to enter on L column
    L_h = 0;  // amount of tape medium in each column
    R_h = 0;

    for(i=0;;i++) {
        // head position 
        MT_head = 1 + 10 * (nHead-i) / nHead; 
        if (MT_head<1) MT_head=1;
        // reel rotation
        if (R_h <= PARAM_VacCol_h_Low) {
            // medium loop on vaccol R not passed yet over upper sensor. Both reels feeds column R
            AddSeq(unit, msec, MT_anim_step_inc, 
                0 /* L_VacCol_inc */, L_inc_h + R_inc_h /* R_VacCol_inc */, 
                +ang_inc /* L_ang_inc */, -ang_inc /* R_ang_inc */, 
                MT_Reel_Amount, MT_head);       
            R_h += L_inc_h + R_inc_h; 
        } else if (L_h <= PARAM_VacCol_h_Low) {
            // medium on R col passed over upper sensor. Each reel feeds its own column
            AddSeq(unit, msec, MT_anim_step_inc, 
                L_inc_h /* L_VacCol_inc */, R_inc_h /* R_VacCol_inc */, 
                +ang_inc /* L_ang_inc */, -ang_inc /* R_ang_inc */, 
                MT_Reel_Amount, MT_head);        
            L_h += L_inc_h; 
            R_h += R_inc_h; 
        } else {
            // both cols feeded. Stop take motor
            AddSeq(unit, msec, MT_anim_step_inc, 
                L_inc_h /* L_VacCol_inc */, R_inc_h /* R_VacCol_inc */, 
                0 /* L_ang_inc */, 0 /* R_ang_inc */, 
                MT_Reel_Amount, MT_head);        
            // when vaccol L tape loop pass over upper sensor, load is finished
            // if head close, all the operation is finished
            if (MT_head==1) break;
        }
    }
}

// add to the current animation sequence the un load animation:
// u3 param is the ammount (inches x1000) of tape medium on reel R   
//    - spin reels slowly to un load medium on vacuum columns
void mt_add_unload_seq(int unit, int u3)
{
    int MT_Reel_Amount, MT_head; // state of controls 
    int i, L_h, R_h, L_inc_h, R_inc_h, msec, ang_inc, r1, r2, rL, rR, p;

    MT_Reel_Amount = 3 + (int) (20 * (u3 / (mt_unit[unit].u4*1000.0))); 
    MT_head        = 1;    // head closed

    // calc percent 0..100 of L reel 
    p = (int) ((u3 / (mt_unit[unit].u4*1000.0))*100); 
    if (p>100) p=100; // p=100 -> reel L full, p=0 -> reel L empty

    msec=33;                                   // time for animation step 
    ang_inc=(msec * PARAM_TakeMotor_RPM * 360) / (60 * 1000); // angle reel spins on each animation step

    // calculate the amount of tape medium that reel loads into vaccol on each step 
    // (duration msec) given reel ammount of tape medium in it
    r1=(50 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel empty
    r2=(90 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel full
    rL=r1*100 + (r2-r1)*(100-p);               // L reel current radius in mm x 100
    rR=r1*100 + (r2-r1)*p;                     // R reel current radius in mm x 100
    rL=(int) (0.0393701 * rL * 10);            // reel radius in inches x 1000
    rR=(int) (0.0393701 * rR * 10);            
    L_inc_h = (int) (rL * 2 * 3.1416 * ang_inc / 360); // tape medium taken by L reel each step 
    R_inc_h = (int) (rR * 2 * 3.1416 * ang_inc / 360); // tape medium taken by R reel each step 
    L_inc_h = L_inc_h  / 2;                            // tape loop position is half on medium loaded
    R_inc_h = R_inc_h  / 2; 

    // take tape medium from Vacuum Columns until one col empty
    L_h = mtcab[unit].reel[0].VacCol_h;  // amount of tape medium in each column
    R_h = mtcab[unit].reel[1].VacCol_h;

    for(;;) {
        AddSeq(unit, msec, 0, 
                -L_inc_h /* L_VacCol_inc */, -R_inc_h /* R_VacCol_inc */, 
                -ang_inc /* L_ang_inc */, +ang_inc /* R_ang_inc */, 
                MT_Reel_Amount, MT_head);        
        L_h -= L_inc_h; // both columns stil have tape medium into 
        R_h -= R_inc_h;
        if ((L_h > 0) && (R_h > 0)) {
            continue;             // both columns stil have tape medium into 
        } else if (R_h > 0) {
            R_inc_h += L_inc_h;   // only R column has medium. Taken faster
            L_inc_h = 0;          // as now L reel also take medium from this column
        } else if (L_h > 0) {
            L_inc_h += R_inc_h;   // only L column has medium. Taken faster
            R_inc_h = 0;          // as now R reel also take medium from this column
        } else {
            break;                // both vac col are empty
        }
    }

    // spin both reels forward 45gr to retension tape
    for(i=0;i< (45 / ang_inc) ; i++) {
        AddSeq(unit, msec, 0, 
            0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
            +ang_inc /* L_ang_inc */, +ang_inc /* R_ang_inc */, 
            MT_Reel_Amount, MT_head);        
    }

    // pause 0.5 sec
    msec=500;
    AddSeq(unit, msec, MT_anim_step_inc, 
            0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
            0 /* L_ang_inc */, 0 /* R_ang_inc */, 
            MT_Reel_Amount, MT_head);        

    // now open r/w head
    msec = PARAM_HeadOpenTime / 10; 
    for(i=0;i<10; i++) {
        AddSeq(unit, msec, MT_anim_step_inc, 
                0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
                0 /* L_ang_inc */, 0 /* R_ang_inc */, 
                MT_Reel_Amount, MT_head);        
        MT_head++;
    }
}

// calculate and store in animation array the load animation when a reel 
// in mounted (attached) to tape unit. If cmode = 'F' or 'B' load
// animation will be move all medium forwards/backwards
void mt_set_load_seq(int unit, char cmode)
{
    int MT_Reel_Amount, MT_head; // state of controls at end of of redraw
    int n, msec, r, R_h;

    MT_Reel_Amount   = 1;    // all tape medium on L tape
    MT_head          = 11;   // head full open

    // on load, reels rotates at low speed to feed vacuum columns with tape medium
    // while r/w head is closing. Then goes backwards to read read record backwards 
    // (locate the load point)

    mtcab[unit].nseq=0;                 // init sequence
    mtcab[unit].nseq_tm0=Refresh_tnow; // when animation starts

    if ((cmode == 'F') || (cmode == 'B')) {
        // forward all tape sequence
        msec = mt_unit[unit].u4 * 1000 / PARAM_RWSpeed;
        if (cmode == 'B') {
            mt_unit[unit].u3 = mt_unit[unit].u4 * 1000;
            n=-1; 
        } else {
            n= 1;
        }
        AddSeq(unit, msec, MT_anim_step_rw,  mt_unit[unit].u4 * 1000 ,1,
                                             0,0,0, n /* read fwd/bckwrd */); 
        // end sequence
        AddSeq(unit,    0, MT_anim_finished, 0,0,0,0,0,0); 
        return; 
    }


    // normal load sequence
    // start delay depends on other loads already in progress 
    // This is done to avoid all load animation to be synchonized
    msec=0;
    for (n=0;n<6;n++) {
        if (mtcab[n].mt_is == MT_is_loading_tape) msec += 750;          
    }
    AddSeq(unit, msec, MT_anim_step_inc, 
            0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
            0 /* L_ang_inc */, 0 /* R_ang_inc */, 
            MT_Reel_Amount, MT_head);        
    
    // make sure no medium on vac col
    mtcab[unit].reel[0].VacCol_h = 0;  // amount of tape medium in each column
    mtcab[unit].reel[1].VacCol_h = 0;

    // add the load animation:
    //    - spin reels slowly to load medium on vacuum columns
    //    - close r/w head
    mt_add_load_seq(unit);
    MT_head        = 1;    // head closed

    // Now sense load point by reading backwards
    // Duration of it  depends on how much the user has spinned reel R when 
    // preparing the tape for load
    // when reel is empty, it holds aprox 29,6 inches of tape medium in each revolution
    // r/w heads moves the tape medium at 75 inches/sec -> each revolution done by
    // the user will need 400 msec
    r=(50 * PARAM_Reel_Diameter / 2) / 100;  // radius when reel empty
    r=(int) (0.0393701 * r * 1000);          // reel radius in inches x 1000
    R_h = (int) (r * 2 * 3.1416);            // ammount to medium on R reel circunference. 
    msec = R_h / PARAM_RWSpeed;              // time to read medium on reel circunference. 

    // Asume user has done 5-15 rev when preparing the tape
    msec = 5 * msec + 10 * msec * (sim_rand() & 0xFF) / 256;
    AddSeq(unit, msec, MT_anim_step_rw,  0,0,0,0,0, 
                                         -1 /* read backwards */); 

    // end sequence
    AddSeq(unit,    0, MT_anim_finished, 0,0,0,0,0,0); 
}

// calculate and store in animation array the animation sequence for rewind
void mt_set_rew_seq(int unit)
{
    int MT_Reel_Amount,  MT_head;    // state of controls at end of of redraw
    int u3 = mt_info[unit].recsize;  // use recsize to get u3 value on rewind start 
    int msec, time, bHi, u3_dec; 

    MT_Reel_Amount = 3 + (int) (20 * (u3 / (mt_unit[unit].u4*1000.0))); 
    if (MT_Reel_Amount > 23) MT_Reel_Amount = 23;
    MT_head        = 1;    // head closed

    mtcab[unit].nseq=0;                 // init sequence
    mtcab[unit].nseq_tm0=Refresh_tnow;  // when animation starts

    bHi=0; // flag high/low speed rew
    mtcab[unit].rew_u3 = u3; // amount on tape medium (inch x1000) in reel R that should be rewinded
                             // original mt_unit[].u3 is set to 0 on OP_RWD command start in mt_cmd()

    // check if rew at high speed. Hi Speed rew is done when at least 450 feet in take reel
    // 450 feet -> 5400 inches
    bHi=0;
    if (u3 > 5400 * 1000) { //n has inches x1000 of tape medium used
        // yes, do high speed rew
        bHi=1; 
        // add the unload animation:
        //    - spin reels slowly to unload medium on vacuum columns
        //    - open r/w head
        mt_add_unload_seq(unit, u3);
        MT_head        = 11;    // head open
                
        // pause 0.5 sec
        msec=500;
        AddSeq(unit, msec, MT_anim_step_inc, 
            0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
            0 /* L_ang_inc */, 0 /* R_ang_inc */, 
            MT_Reel_Amount, MT_head);        

        // now start hi speed rew
        // rewind at 500 inch/sec (average) until 400 inches left
        u3 -= 400*1000;                     // u3 in inches x1000 = ammount of medium inches to rewind at hi speed
        time= u3 / PARAM_HiSpeedRwdSpeed;   // hi speed rew total time in msec

        // hi rew with at least 5400 inches (leaves 400 for low speed res)
        // so min amount of reel rew at hi speed is 5000
        // this means 10 sec at 500 inc/sec
        // reduce this time with hi speed rew acceleration deceleration time
        time = time - 1000 - 3000;

        // 1000 msec to accelerate. This is an educated guess
        msec = 1000; 
        AddSeq(unit, msec, MT_anim_step_HiRew, 
                0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
                0 /* L_ang_inc */, 0 /* R_ang_inc */, 
                MT_Reel_Amount, 1 /* acceleration flag: -1 decel, 0=full speed, -1=decelerate */ );        

        // full speed rewind
        // calculate time of each step and how much mediun is rewinded on each step (u3_dec)
        if (MT_Reel_Amount <= 3) {
            MT_Reel_Amount=3;               // safety
            msec = time; 
            u3_dec = u3;  // amount of medium rewinded on each animation step
        } else {
            msec = time / (MT_Reel_Amount-3 +1);   // time to decrease reel input by one
            u3_dec = u3 / (MT_Reel_Amount-3 +1); 
        }
        if (CpuSpeed_Acceleration == -1) msec = 100; // if ^F only 100 msec per decr of reel_amount

        u3 = mtcab[unit].rew_u3; // position of medium at beginning of hi speed rew;
                                 // at end of hi speeed rew this should have the value of 400x1000
        for(;;) {
            AddSeq(unit, msec, MT_anim_step_HiRew, 
                u3 /* L_VacCol_inc */, u3_dec /* R_VacCol_inc */, 
                0 /* L_ang_inc */, 0 /* R_ang_inc */, 
                MT_Reel_Amount, 0 /* acceleration flag: -1 decel, 0=full speed, -1=decelerate */ );        
            u3 -= u3_dec;
            if (MT_Reel_Amount==3) break;
            MT_Reel_Amount--;
        }

        // 3000 msec to decelerate. This is an educated guess
        msec=3000;
        AddSeq(unit, msec, MT_anim_step_HiRew, 
                0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
                0 /* L_ang_inc */, 0 /* R_ang_inc */, 
                MT_Reel_Amount, -1 /* acceleration flag: -1 decel, 0=full speed, -1=decelerate */ );        

        // pause 0.5 sec
        msec=500;
        AddSeq(unit, msec, MT_anim_step_inc, 
            0 /* L_VacCol_inc */, 0 /* R_VacCol_inc */, 
            0 /* L_ang_inc */, 0 /* R_ang_inc */, 
            MT_Reel_Amount, MT_head);        

        // load tape animation
        mt_add_load_seq(unit);
        MT_head        = 1;    // head closed
        
        u3 = 400*1000; // after hi speed rew, this is the amount of tape medium in reel
    }
    // finish rewinding at low speed
    msec = u3 / PARAM_RWSpeed;              // time to read remaining of tape. 
    AddSeq(unit, msec, MT_anim_step_rw,  u3 /* update recsize */,0,0,0,0, 
                                         -1 /* read backwards */); 

    // end sequence
    AddSeq(unit, 0,  MT_anim_finished, 0,0,0,0,0,0); 

    // log on debug precise rew time 
    {
       DEVICE *dptr = find_dev_from_unit(&mt_unit[unit]);
       int i; 
       time = 0; 
       for (i=0;i<MT_anim_sequence_len;i++) {
           msec = mtcab[unit].seq[i].msec; 
           if ((msec == 0) || (mtcab[unit].seq[i].hint == MT_anim_finished)) break;
           time += msec;
       }
       sim_debug(DEBUG_CMD, dptr, "Tape unit %d: %s rewind time needed (%d sec)\n", 
                                   unit, (bHi ? "High Speed":"Low Speed"), time/1000);
    }

    // mt_dump_animation_seq(unit);
}

// tape r/w head & reels & vacuum columns simulation
// update tape controls for loop position on VacCols based on reels movement, loop 
// past the vaccols upper/lower sensors, r/w head reading forwards or backwards
void mt_reels_mov(int unit, int cmd, 
                  int * L_VacColMedium_h, int * R_VacColMedium_h, 
                  int * MT_L_Rot, int * MT_R_Rot,
                  int * MT_Reel_Amount, int * MT_head)
{
    uint32 tnow = Refresh_tnow;   
    static uint32 old_tnow = 0; 

    int time, time1, recsize, ireel, r, r1, r2, p, h,
        ang_inc, ang, n, tm0, msec,  motor0, motor;
    double rpm, rpm0, rpmMax, rpsMax, a, am, revs; 
    
    struct oldrec {
        int h, ang, ang_inc, n;
        double rpm, revs;  
    } old[2]; 

    // save state of reels
    for(ireel=0;ireel<2;ireel++) {  
        old[ireel].h   = mtcab[unit].reel[ireel].VacCol_h;
        old[ireel].ang = mtcab[unit].reel[ireel].ang;
        old[ireel].ang_inc = 0;
        old[ireel].n = mtcab[unit].reel[ireel].n;
        old[ireel].rpm = mtcab[unit].reel[ireel].rpm;
        old[ireel].revs = mtcab[unit].reel[ireel].revs;
    }

    // calc percent 0..100 of reel full
    p = (int) ((mt_unit[unit].u3 / (mt_unit[unit].u4*1000.0))*100); // r=0 -> reel L full, p=100 -> reel R rull
    if (p>100) p=100;

    // calc radius of reel based on how much tape medium stored is in it 
    // reel diameter = 267mmm -> radius = 133mm. 
    r1=(50 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel empty
    r2=(90 * PARAM_Reel_Diameter / 2) / 100;   // radius when reel full
    r1=(int) (0.0393701 * r1 * 10);            // reel radius in inches x 1000
    r2=(int) (0.0393701 * r2 * 10);            // reel radius in inches x 1000

    // calc max reel spin speed. Take into account reduction by motor pulleys of 2/5
    rpsMax = (PARAM_RPM * 2.0 / 5.0) / 60;  // reel revolutions per second at full speed

    // if a r/w operation in progress, then move tape medium under tape head 
    // to/from vacuum colums
    if (mtcab[unit].rw_msec) { 
        // calc time elapsed on r/w cmd
        time = tnow - mtcab[unit].rw_tm0;
        if ((time < 0) || (time > mtcab[unit].rw_msec)) time = mtcab[unit].rw_msec;
        // calc medium (in inches x1000) that has been moved under r/w head
        recsize = (mtcab[unit].rw_recsize * time / mtcab[unit].rw_msec);
        mtcab[unit].rw_recsize -= recsize;
        mtcab[unit].rw_msec    -= time; 
        mtcab[unit].rw_tm0      = tnow;
        // take/give medium to vacuum columns
        recsize = recsize / 2; // when adding/removing medium, loop position in vacuum colum moves half distance
        mtcab[unit].reel[0].VacCol_h -= recsize;
        mtcab[unit].reel[1].VacCol_h += recsize;
        bTapeAnimInProgress=1; // signal tape medium movement in progress 
    }
    // check if a new r/w operation can be schedulled 
    if ((mtcab[unit].rw_msec == 0) && (cmd==1) && (mt_info[unit].cmd_tm0)){ 
        // if no r/w op being animated (rw_msec=0), and now a r/w operation is being done by tape (cmd_tm0 > 0)
        mt_info[unit].cmd_tm0=0;   // mark this as being animated (to avoid animating it twice)
        mtcab[unit].rw_tm0     = tnow; 
        mtcab[unit].rw_msec    = mt_info[unit].cmd_msec;
        mtcab[unit].rw_recsize = mt_info[unit].recsize;
        bTapeAnimInProgress=1; // signal tape medium movement started
    }
    
    // calc reel algular position base on motor acceleratin or decelerating the reel
    for(ireel=0;ireel<2;ireel++) {  
        // get initial motor status
        motor = mtcab[unit].reel[ireel].motor;
        tm0   = mtcab[unit].reel[ireel].tm0;
        if (tm0 == 0) continue;      // motor not started, skip 
        time = tnow - tm0;           // time is elapsed time accelerating/decelerating (in msec)
        if ((time <= 0) || (time > 10*1000)) continue; // safety
        // calc influence in acceleration/deceleration time of quantity of tape medium 
        // currently wound in the reel
        // this is a very simple aproximation. 
        // When reel is full of medium then am=1.0 (keep same accel time as in PARAM)
        // when reel is empty, am=0.3 (time is PARAM *0.3 -> accelerates/decelerates faster)
        am=0.3 + 0.7 * ( (ireel==0) ? 100-p:p) / 100;     
        // calc new angular position
        rpm0 = mtcab[unit].reel[ireel].rpm0;
        if (motor != 0) {
            // motor is accelerating reel forwards/backwards
            rpmMax = rpsMax * 360.0 / 1000;             // ang incr (gr 0..360) per msec at full speed
            if (motor < 0) rpmMax=-rpmMax;              // going backwards
            if (ireel)  rpmMax=-rpmMax;                 // R reel rotates CCW then loading medium (motor=1)
            a = rpmMax / (PARAM_AccelTime * am);        // angular acceleration per msec. >0 turning CW
            // calc spinining speed (rpm = angular increment in degrees per msec )
            rpm = a * time + rpm0;                      
            // check not exceeding max rpm's
            if (  ((rpm > 0) && (rpm > rpmMax)) ||
                  ((rpm < 0) && (rpm < rpmMax)) ) { 
                // if exceeded max speed, recalculate accel time 
                msec  = (int) ((rpmMax - rpm0) / a);    // time accerating
                time1 = time - msec;                    // time at full speed
                time  = msec;                           // time accelerating
                rpm   = rpmMax; 
            } else time1 = 0;
            // calc how many revolutions has done the reel counting from start of motor accelerating
            // = revolutions done while accelerating (during time msecs) + revs done at full speed (during time1 msecs)
            revs = ((a * time * time / 2 + rpm0 * time) + (rpmMax * time1)) / 360; 
        } else {
            // reel is decelerating
            rpmMax = rpsMax * 360.0 / 1000;             // ang incr (gr 0..360) per msec at full speed
            a = rpmMax / (PARAM_DecelTime * am);        // deceleration per msec
            if (rpm0 < 0) a = -a;                       // decelerating while spinning backwards
            // calc time accelerationg (time var), time at full speed (time1) and reel rpm speed
            rpm = -a * time + rpm0;                     // calc reel speed 
            // check if stopped (rpm diferent sign than rpm0)
            if (  ((rpm < 0) && (rpm0 > 0)) ||
                  ((rpm > 0) && (rpm0 < 0)) ||
                  (rpm == 0) || (rpm0 == 0)  ) { 
                // stopped, recalculate decel time 
                time  = (int) (rpm0 / a);               // time decerating (a and rpm allways have same sign)
                rpm   = 0; 
            }
            // calc how many revolutions has done the reel counting from start of motor decelerating
            // = revolutions done while decelerating (during time msecs) (plus nothing more: no revs done when reel stoped)
            revs = (-a * time * time / 2 + rpm0 * time) / 360; 
            mtcab[unit].reel[ireel].rpm = rpm; 
            // check if reel stoped
            if (rpm == 0) {
                mtcab[unit].reel[ireel].tm0 = 0;
            }
        }
        mtcab[unit].reel[ireel].rpm  = rpm;  // save current reel speed
        mtcab[unit].reel[ireel].revs = revs; // save current revolutions done
        // calc how much degrees reel has rotated in this refresh frame. Not normalized! can be <-360 or >360 
        ang_inc = (int) ((revs - old[ireel].revs) * 360); 
        old[ireel].ang_inc = ang_inc; // save here for later use
        // calc new angular position of reel: its original position when accel/decel was 
        // started + revolutions done during accel/decel (0..360 gr)
        ang = (int) (mtcab[unit].reel[ireel].ang0 + revs * 360);
        ang = ang % 360; if (ang < 0) ang += 360; 
        mtcab[unit].reel[ireel].ang  = ang;  // save current angular position
        // calc radius of reel based on how much tape medium stored is in it 
        r=r1*100 + (r2-r1)* ( (ireel==0) ? 100-p:p);     // reel current radius in inches x 1000
        // calc medium (in inches x1000) that has been moved from/to reel
        recsize = (int) (r * 2 * 3.1416 * ang_inc / 360);
        recsize = recsize / 2; // when adding/removing medium, loop position in vacuum column moves half distance
        // recsize and ang have the sing of ang_inc (indicates if load/take medium)
        // when L reel turns forwards (CW direction, that is rpm > 0, ang value increases, 
        // revs > 0) then recsize > 0 -> medium feed/loaded in vaccol. 
        // when R reel turns forwards (CW direction, that is rpm > 0, ang value increases, 
        // revs > 0) then recsize > 0 -> medium take/unloaded from vaccol. 
        // if R reel then change sign of recsize
        if (ireel) recsize = -recsize;
        // take/give medium to reels vacuum columns
        mtcab[unit].reel[ireel].VacCol_h += recsize; 
    }
   
    // sense medium in vac cols and act on reel motors
    for(ireel=0;ireel<2;ireel++) {  
        // get initial motor status
        motor0 = mtcab[unit].reel[ireel].motor;
        // get heigh of medium in vac col (0 = no medium loaded, nnn = these inches of medium loaded
        h = mtcab[unit].reel[ireel].VacCol_h;
        // ammount of medium in vac col is measured from top of column to tape medium loop
        // when no much medium is in vac col, h is low, when colum is full of medium then h is high
        if (h < PARAM_VacCol_h_Low) {              // Upper column sensor h position
            motor=1; // accelerate forward, should load medium in colum
        } else if (h > PARAM_VacCol_h_Hi) {        // Lower column sensor h postion
            motor=-1; // accelerate backwards, should take medium from column
        } else {
            motor=0;   // decelerate motor (break reel)
        }
        // has motor changed its state?
        if (motor0!=motor) {
            // yes!, motor start (accelerate) or motor brake (decelerate). 
            time = 0;
            // check % of tape loop has passed the sensor
            if ((old[ireel].h < PARAM_VacCol_h_Low) && (PARAM_VacCol_h_Low < h)) {
                // tape loop in VacCol going down from very top (reel load medium into near empty column), 
                // tape loop pass over upper sensor
                r = 100 * (PARAM_VacCol_h_Low - old[ireel].h) / (h - old[ireel].h);
            } else if ((h < PARAM_VacCol_h_Hi) && (PARAM_VacCol_h_Hi < old[ireel].h)) {
                // tape loop in VacCol going up from very bottom (reel takes medium from near full column), 
                // tape loop pass over lower sensor
                r = 100 * (PARAM_VacCol_h_Hi - old[ireel].h) / (h - old[ireel].h);
            } else {
                r = 100; // tape loop has not passed over a sensor. 
            }
            if (r < 100) {
                // interpolate rpm, ang, h based on p, calc time = msecs ago tape loop passed sensor
                time = (tnow - old_tnow) * (100 - r) / 100; 
                if ((time < 0) || (time > 10000)) time = 30; // safety
                old[ireel].ang_inc = old[ireel].ang_inc * r / 100; 
                ang = old[ireel].ang + old[ireel].ang_inc; 
                ang = ang % 360; if (ang<0) ang += 360; 
                mtcab[unit].reel[ireel].ang = ang; 
                mtcab[unit].reel[ireel].rpm      = (mtcab[unit].reel[ireel].rpm - old[ireel].rpm) 
                                                       * r / 100 + old[ireel].rpm;
                mtcab[unit].reel[ireel].VacCol_h = (h - old[ireel].h) 
                                                       * r / 100 + old[ireel].h;
            }
            // set new state of motor
            mtcab[unit].reel[ireel].motor = motor;
            // save time (tnow) when motor starts accelerating/decelerating
            mtcab[unit].reel[ireel].tm0   = tnow - time; 
            // init number of revolutions done
            mtcab[unit].reel[ireel].revs = 0; 
            // save reel initial angular position when motor starts accelerating/decelerating
            mtcab[unit].reel[ireel].ang0 = mtcab[unit].reel[ireel].ang;
            // save reel initial reel rpm speed when motor starts accelerating/decelerating
            mtcab[unit].reel[ireel].rpm0 = mtcab[unit].reel[ireel].rpm;             
        }
    }

    // set reel/vacuum columns control state on cpanel
    for(ireel=0;ireel<2;ireel++) {       
        // get heigh of medium in vac col (0 = no medium loaded, nnn = these inches of medium loaded
        h = mtcab[unit].reel[ireel].VacCol_h;  
        // set height of medium 
        if (ireel) *R_VacColMedium_h = h; else *L_VacColMedium_h = h;
        // get angluar position of reel
        ang = mtcab[unit].reel[ireel].ang; 
        // calc state that match with angular position of reel
        if (mtcab[unit].reel[ireel].color == 1) {
            // grey reel (when color == 1) states covers 120 gr because repeats itself 
            // echa state is rotated 5 gr
            n = 24 * (ang % 120) / 120; 
        } else {
            // white/dark grey reel states covers 360 gr. Each state is rotated 15 gr
            n = 24 * ang / 360;              
        }
        // set speed blur
        rpm = mtcab[unit].reel[ireel].rpm;
        if (rpm < 0) rpm =-rpm; // only abs values
        rpmMax = rpsMax * 360.0 / 1000;             // ang incr (gr 0..360) per msec at full speed
        if (rpm < 0.35 * rpmMax) { 
            // no blur, use calculated state
            mtcab[unit].reel[ireel].n = n; 
        } else {
            // blur, set reel control at previous reel angular position respect
            // rotation direction. 
            r = n + ((old[ireel].ang_inc > 0) ? -1:1);
            r = (r % 24); if (r<0) r+=24; 
            if (r != old[ireel].n) n = r; // use it if not same as last reel pos
            mtcab[unit].reel[ireel].n = n; 
            n = 24 + n; // use blur states 24-47
            bTapeAnimInProgress=1;  // signal reel blur started 
        }
        // set angular position state
        if (ireel) *MT_R_Rot = n; else *MT_L_Rot = n;
    }
    // set head/reel amount control state on cpanel
    // % of tape medium used
    *MT_Reel_Amount = 3 + (int) (20 * (mt_unit[unit].u3 / (mt_unit[unit].u4*1000.0))); 
    if (*MT_Reel_Amount > 23) *MT_Reel_Amount = 23;
    // tape head on closed position
    *MT_head = 1; 

    // save last tnow used to know how much time elapsed since last refresh
    old_tnow = tnow;  
}

// dynamic draw of tape medium into vaccum column control CId_VacCol
// if VacColMedium_h = 15000 = PARAM_VacCol_h_Low -> tape medium loop positioned over upper vacuum colum sensor
// if VacColMedium_h = 40000 = PARAM_VacCol_h_Hi  -> tape medium loop positioned over lower vacuum colum sensor
void mt_VacColSetDynamicState(int VacColMedium_h, int * VacColMedium_h0, int CId_VacCol)
{
    int h, y;
    if (VacColMedium_h == *VacColMedium_h0) return; // no changes in height. nothing to draw
    *VacColMedium_h0 = VacColMedium_h; 

    // calculate h
    // control has upper sensor on y=30, lower sensor on y=100, 
    // even if colum control is 151 pixels heigh, only visible a height of 114
    h= 30 + ((100-30) * (VacColMedium_h-PARAM_VacCol_h_Low) / (PARAM_VacCol_h_Hi-PARAM_VacCol_h_Low));
    if (h<0) h=0; 
    // h is vertical position of tape loop base, converto to y coord
    y=152-h; if (y<0) y=0; 

    // Dynamically generate State 0 for CId_VacCol (=IBM650.MT_L|R_VacCol[unit])

    //  - Copy State 0 from control MT_VacColumn (background) to state 0 of CId (state displayed) 
    CopyControlImage(IBM650.MT_VacColumn, 0,       0, 0, 0, 0,  // FromCId, FromState, x0, y0, w, h, 
                     CId_VacCol, 0,                0, 0);       // ToCId, ToState,     x1, y1
    //  - Copy State 0 from MT_VacColMedium (height dependes on medium position) on State 0 -> now vaccol with tape medium
    CopyControlImage(IBM650.MT_VacColMedium, 0,    0, y, 0, 0,  // FromCId, FromState, x0, y0, w, h, 
                     CId_VacCol,                   0, 0, 0);    // ToCId, ToState,     x1, y1
    //  - set dynamically generated image to be redraw 
    cpanel_ControlRedrawNeeded=1; 
    SetState(CId_VacCol, 0);

}


// return 0 if tape is idle (not moving the medium), 1 if tape is doing read/write, -1 if rew
//        mt_indicator=1 if tape has set the indicator light
int get_mt_current_command(int unit, int * mt_indicator)
{
    int cmd;

    cmd=mt_get_last_cmd(unit);
    if (cmd < 0) {
        *mt_indicator = 1; 
        cmd=-cmd; 
    } else *mt_indicator = 0;
    
    if (cmd == OP_RWD) {
        return -1;  
    }
    if ((cmd ==  OP_RTC) || (cmd ==  OP_RTA) || (cmd ==  OP_RTN) ||
        (cmd ==  OP_WTM) || (cmd ==  OP_WTA) || (cmd ==  OP_WTN) ||
        (cmd ==  OP_BST)) {
        return 1; // tape read/write operation
    }
    return 0;
}

void Refresh_MagTape(void)
{
    int unit, n;
    UNIT *uptr;

    int MT_Reel_Amount0, MT_L_Rot0, MT_R_Rot0, MT_head0; // state of controls at beggining of redraw
    int MT_Reel_Amount,  MT_L_Rot,  MT_R_Rot,  MT_head; // state of controls at end of of redraw
    int L_VacColMedium_h, R_VacColMedium_h; // annount of tape medium in vaccol
    int cmd, mt_is, mt_indicator_light;

    bTapeAnimInProgress=0;
    for(unit=0;unit<6;unit++) {
        uptr=&mt_unit[unit];
        // check if unit disabled
        if (uptr->flags & UNIT_DIS) {
            SetState(IBM650.MT_num[unit], 10);     // yes, unit disabled -> tape number no lit on cabinet
        NoTapeAttached:
            SetState(IBM650.MT[unit], 0);          // no magnetic medium on reels
            SetState(IBM650.MT_L[unit], 64);       // default gray reels
            SetState(IBM650.MT_R[unit], 64);                           
            SetState(IBM650.MT_lights[unit], 0);   // lights off
            SetState(IBM650.MT_head[unit], 11);    // r/w tape head wide open
            SetState(IBM650.MT_L_VacCol[unit], 0); // no magnetic medium on vacuum col
            SetState(IBM650.MT_R_VacCol[unit], 0); // no magnetic medium on vacuum col
            continue; 
        } 
        // unit enabled
        SetState(IBM650.MT_num[unit], unit);     // unit enabled -> its number is lit on tape
        // check if tape file attached
        if ((uptr->flags & UNIT_ATT) == 0) goto NoTapeAttached; // no file attached -> clean up tape 
        // tape has file attached (=tape reel mounted)
        // check if just being attached
        if (mt_info[unit].justattached) {  // 1 -> just attached -> should read reel tape options, if any 
            // read reel color options, if any. reel color options are processed each time a tape file is attached to MT device, so
            // reel color options must be set before the attach scp command
            char MT_cab_opt[16] = "MTx";
            char c, cmode;

            memset (&mtcab[unit], 0, sizeof (mtcab[0]) );
            mtcab[unit].reel[0].color = 1; /* Left reel defaults to white */
            mtcab[unit].reel[1].color = 1; /* Right reel defaults to white */
            cmode = ' ';

            MT_cab_opt[2] = '0' + unit;
            if ((IsOption(MT_cab_opt)) && (IsOptionParam) && (*IsOptionParam++ == '/')) {
                // syntax: option=mt1/WG <- tape 1 is set have reels color White(Left) and Gray (right). 
                //                          the third possible color is D (dark gray)                
                c = *IsOptionParam++; // get left reel color
                c = sim_toupper(c);
                mtcab[unit].reel[0].color = (c == 'G') ? 1 /* gray reel */ : (c == 'D') ? 2 /* dark grey reel */ : 0; /* else defaults to white */
                c = *IsOptionParam++; 
                c = sim_toupper(c);
                mtcab[unit].reel[1].color = (c == 'G') ? 1 /* gray reel */ : (c == 'D') ? 2 /* dark grey reel */ : 0; /* else defaults to white */
                c = *IsOptionParam++; 
                if (c == '*') {
                    c = *IsOptionParam++; 
                    c = sim_toupper(c);
                    if ((c == 'R') || (c == 'F') || (c == 'B')) {
                        cmode = c; // load animation will be Forward/backwards all reel, or *R
                    }
                }                
            }
            // reel color set. 
            mt_info[unit].justattached=0;
            // Now init the cabinet states and ... 
            mtcab[unit].reel[0].ang = (sim_rand() & 511) % 360; // reel mounted at random angular position
            mtcab[unit].reel[1].ang = (sim_rand() & 511) % 360;
            mtcab[unit].L_VacColMedium_h0 = -1; // init to -1 last state to force redraw of vaccol 
            mtcab[unit].R_VacColMedium_h0 = -1; 
            // ... signal the load animation can begin
            mtcab[unit].mt_is = MT_is_loading_tape; 
            if (cmode != 'R') {
                // normal load animation, *F or *B 
                 mt_set_load_seq(unit, cmode);
            } else {
                // *R rewind animation 
                // start hi speed rewind. Whis is nice!
                mt_info[unit].recsize = mt_unit[unit].u4 * 1000;
                mt_set_rew_seq(unit);
            }
        }
        // tape is about to be painted on cpanel
        // MT state holds the amount of tape medium in each reel. 
        //   =1  -> all tape on L reel, 
        //   =23 -> all tape on R reel, 
        //   =0  -> no tape, reel unmounted
        // MT_L/MT_R state holds the rotational position of reel, the speed ot reel, and the reel's colour
        //    0..23 -> reel rotated 0gr, 15gr, ... 345gr. To be used when reel is not moving
        //   24..47 -> same reel rotated 0gr, 15gr ... but with some spin blur. To be used when reel is moving
        //   48..55 -> reel rotated 0gr, 45gr, ... whith greater spin blur. To be used when reel is rewinding at fast pace
        //   64..   -> same but with another reel color. There are 3 reels colors.
        // MT_head holds the position of r/w head
        //   =0  -> no head image
        //   =1  -> head closed, prepared to read or write tape medium
        //   =11 -> open head, prepared to manualy remove tape medium 

        // get the current tape control state. If this state changes, then should redraw
        MT_Reel_Amount0  = (int) GetState(IBM650.MT[unit]); 
        MT_L_Rot0        = (int) GetState(IBM650.MT_L[unit]);
        MT_R_Rot0        = (int) GetState(IBM650.MT_R[unit]);
        MT_head0         = (int) GetState(IBM650.MT_head[unit]);

        // what tape is doing now?
        cmd = get_mt_current_command(unit, &mt_indicator_light); // current tape cmd being executed: -1=rew, 0=idle, 1=read/write
        mt_is =  mtcab[unit].mt_is;         // the current animation being done (visual state of tape)

        // check if load/rew animation in progress should be aborted
        if (mt_is == MT_is_loading_tape) {
            if (mt_info[unit].numrw > 0) {
                // if any mt r/w command issued by cpu then abort any load animation in progress
                mtcab[unit].mt_is = mt_is = 0;
                // position of medium on vaccol as it should be at end of load
                mtcab[unit].reel[0].VacCol_h = (int) (PARAM_VacCol_h_Hi  * 0.9); 
                mtcab[unit].reel[1].VacCol_h = (int) (PARAM_VacCol_h_Low * 1.1); 
            }
        } else if (mt_is == MT_is_rewinding) {
            if (  (cmd == 1) || (mt_ready(unit)==1) ) {
                // any mt r/w command in progress terminates any rew animation in progress
                // if tape ready then terminates any rew animation in progress (the rew command has terminated its execution)
                mtcab[unit].mt_is = mt_is = 0;
                // position of medium on vaccol as it should be at end of load
                mtcab[unit].reel[0].VacCol_h = (int) (PARAM_VacCol_h_Hi  * 0.9); 
                mtcab[unit].reel[1].VacCol_h = (int) (PARAM_VacCol_h_Low * 1.1); 
            }
        }
        
        // check if should start rew animation
        if ((mt_is == 0) && (cmd < 0) && (mt_ready(unit)==0)) {
               // if (no animation in progress) and (last tape cmd is rew) and (last cmd in execution) 
               mtcab[unit].mt_is = mt_is = MT_is_rewinding;
               mt_set_rew_seq(unit);
        }

        // advance animation if any 
        if (mt_is > 0) {
            n = mt_do_animation_seq(unit,              
                         &L_VacColMedium_h, &R_VacColMedium_h, 
                         &MT_L_Rot, &MT_R_Rot, 
                         &MT_Reel_Amount, &MT_head);
            if (n==1) {
                mtcab[unit].mt_is = 0; // normal animation termination
                // if rew terminates, notify mt_svr 
                if (mt_is == MT_is_rewinding) {
                    sim_cancel(&mt_unit[unit]);
                    sim_activate(&mt_unit[unit], 1);
                }
                mt_is = 0;
            }
        }

        // if no animation, simulate reel/vacuum col movement
        if (mt_is == 0) {
            // if tape indicator light on, do not perform cmd 
            if (mt_indicator_light) cmd = 0;
            mt_reels_mov(unit, cmd, 
                         &L_VacColMedium_h, &R_VacColMedium_h, 
                         &MT_L_Rot, &MT_R_Rot, 
                         &MT_Reel_Amount, &MT_head);
        }

        // set reel color
        MT_L_Rot = (MT_L_Rot % 64) + 64 *  mtcab[unit].reel[0].color;
        MT_R_Rot = (MT_R_Rot % 64) + 64 *  mtcab[unit].reel[1].color ;
        // update tape control states
        if ((MT_Reel_Amount0 != MT_Reel_Amount) || 
            (MT_L_Rot0 != MT_L_Rot) || (MT_R_Rot0 != MT_R_Rot) || 
            (MT_head0 != MT_head)) {
            // if some state has changed, redraww all controls
            cpanel_ControlRedrawNeeded = 1;
            SetState(IBM650.MT[unit], MT_Reel_Amount);
            cpanel_ControlRedrawNeeded = 1;
            SetState(IBM650.MT_L[unit], MT_L_Rot);
            cpanel_ControlRedrawNeeded = 1;
            SetState(IBM650.MT_R[unit], MT_R_Rot);
            cpanel_ControlRedrawNeeded = 1;
            SetState(IBM650.MT_head[unit], MT_head); 
        }
        // set dynamic state for vacCol controls 
        mt_VacColSetDynamicState(L_VacColMedium_h, &mtcab[unit].L_VacColMedium_h0, IBM650.MT_L_VacCol[unit]);
        mt_VacColSetDynamicState(R_VacColMedium_h, &mtcab[unit].R_VacColMedium_h0, IBM650.MT_R_VacCol[unit]);
        // set tape lights for IBM 727 Magnetic Tape
        n = ((LastTapeSelected==unit)            ? 1 : 0) +      // light_0  ; Select light = tape selected by external source
            (((mt_ready(unit)==1) && (mt_is==0)) ? 2 : 0) +      // light_1  ; Ready light = tape in automatic mode (off when rew/load)
            (sim_tape_wrp(uptr)                  ? 4 : 0) +      // light_2  ; File Protect = tape reel is write protected
            (mt_indicator_light                  ? 8 : 0);       // light_3  ; Tape Indicator = tape reached end of tape

        SetState(IBM650.MT_lights[unit], n);
    }
}
#endif    

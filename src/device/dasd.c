/*
 * microsim360 - Generic CKD (2311/2314) interface.
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

/*

   Structure of a disk. See Hercules CKD disks.

    Numbers are stored least to most significant.

     Devid = "CKD_P370"

       uint8    devid[8]        device header.
       uint32   heads           number of heads per cylinder
       uint32   tracksize       size of track
       uint8    devtype         Hex code of last two digits of device type.
       uint8    fileseq         always 0.
       uint16   highcyl         highest cylinder.

       uint8    resv[492]       pad to 512 byte block

   Each Track has:
       uint8    bin             Track header.
       uint16   cyl             Cylinder number
       uint16   head            Head number.

   Each Record has:
       uint16   cyl             Cylinder number  <- tpos
       uint16   head            Head number
       uint8    rec             Record id.
       uint8    klen            Length of key
       uint16   dlen            Length of data

       uint8    key[klen]       Key data.
       uint8    data[dlen]      Data len.

   cpos points to where data is actually read/written from

   Pad to being track to multiple of 512 bytes.
   Last record has cyl and head = 0xffffffff

*/


/*

    2311   data rate  156,000 b/s  seek 85 ms avg  25ms/rev

           Cyl to cyl      30ms
           cyl 0 to 202   145ms

            5 cyl  40ms
           10 cyl  55ms
           15 cyl  60ms
           20 cyl  75ms
           40 cyl  85ms
           60 cyl  90ms
           80 cyl  95ms
          100 cyl 105ms

            0 to  5 25ms + 5ms/cyl
            5 to 25 55ms + 1ms/cyl
           25 to -  75ms + 1ms/10cyl

          3694 + bytes per track

          3906 bpt

          index to HA  GAP 36 bytes. (34)0x00 - 0xff 0x0e
          alpha gap        18 bytes. 0xcc - (15)0x00 0xff 0x0e
          beta gap         18 bytes  0xcc - (9)0xff, (6)0x00, (1)0xff, 0x0e
          AM gap           -  byte   0xcc -(>21) 0xff, (4)0x00, (1)0xff, (2AM)0xff, 0x0e

          Data rate 6.8us per byte.

          156,000 = 6.4us per byte

          1us step time.
*/

#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif
#include "event.h"
#include "logger.h"
#include "dasd.h"
#include "xlat.h"


#define BIT0    0x80
#define BIT1    0x40
#define BIT2    0x20
#define BIT3    0x10
#define BIT4    0x08
#define BIT5    0x04
#define BIT6    0x02
#define BIT7    0x01

#define DK_POS_INDEX     0        /* At Index Mark */
#define DK_POS_HA        1        /* In home address (c) */
#define DK_POS_GAP1      2        /* Gap after home address */
#define DK_POS_CNT0      3        /* In count (c) */
#define DK_POS_GAP2      4        /* Gap after count feild */
#define DK_POS_AM        5        /* Gap before normal record */
#define DK_POS_CNT1      6        /* In normal record count */
#define DK_POS_KEY       7        /* In Key area */
#define DK_POS_GAP3      8        /* Gap after Key field */
#define DK_POS_DATA      9        /* In Data area */
#define DK_POS_END      10        /* Past end of data */
#define DK_POS_UNK      11        /* Unknown position */

char *disk_state[] = {
      "Index", "HA", "GAP1", "CNT0", "GAP2", "AM",
      "CNT1", "KEY", "GAP3", "DATA", "END", "?"};

struct disk_t
{
    const char         *name;         /* Type Name */
    int                 cyl;          /* Number of cylinders */
    uint32_t            heads;        /* Number of heads/cylinder */
    int                 bpt;          /* Max bytes per track */
    uint8_t             sen_cnt;      /* Number of sense bytes */
    uint8_t             dev_type;     /* Device type code */
    uint8_t             g1;           /* Bytes in Gap 1*/
    uint8_t             g2;           /* Bytes in Gap 2*/
    uint8_t             g4;           /* Gap between index and HA */
    uint8_t             sync;         /* Sync character */
    int                 rate;         /* Number of steps per disk byte */
}

disk_type[] =
{
       {"2303",  80,  10,  4984,  6,  0x03, 72, 36, 36, 6, 13},   /*   4.00 M */
       {"2311",  203, 10,  3717,  6,  0x11, 36, 36, 36, 6, 13},   /*   7.32 M  156k/s 30 ms 145 full */
       {"2302",  250, 46,  4984,  6,  0x02, 72, 36, 36, 6, 13},   /*  57.32 M 50ms, 120ms/10, 180ms> 10 */
       {"2314",  202, 20,  7294,  6,  0x14, 36, 18, 73, 5, 13},   /*  29.17 M */

       {NULL, 0}
};

int
dasd_settype(struct _dasd_t *dasd, char *type)
{
    int  i;

    for (i = 0; disk_type[i].name != NULL; i++) {
        if (strcmp(disk_type[i].name, type) == 0) {
           dasd->type = i;
           return 1;
        }
    }
    dasd->type = -1;
    return 0;
}

void
dasd_setvolid(struct _dasd_t *dasd, char *volid)
{
     int  i;

     for (i = 0; i < 8; i++) {
         if (volid[i] == '\0')
             break;
         dasd->vol_label[i] = volid[i];
     }
     for (;i < 8; i++) {
         dasd->vol_label[i] = ' ';
     }
     dasd->vol_label[i] = '\0';
}

#if 0
/* Gap is: */
static uint8_t  gap0[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x06};

static uint8_t gap1[] = {
      0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x06};

static uint8_t gap2[] = {  /* 0xaa = 0xff + AM */
      0xcc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xaa, 0xaa,
      0x06 };

static uint8_t gap3[] = {
      0xcc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x06};
#endif

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

/* FT register.     Control         Set Cylinder    Set Head   Set Diff
 * Bit 0            Write Gate      track 128       forward     diff 128
 * Bit 1            Read Gate       track 64                    diff 64
 * Bit 2            Seek start      track 32                    diff 32
 * Bit 3            Head reset      track 16                    diff 16
 * Bit 4            Erase Gate      track 8         head 8      diff 8
 * Bit 5            Select head     track 4         head 4      diff 4
 * Bit 6            Return 000      track 2         head 2      diff 2
 * Bit 7            Head advance    track 1         head 1      diff 1
 *                  FT0 & FT4
 */

/* Flags value.
 *
 *  Bit 0            Write current.
 *  Bit 1            Read.
 *  Bit 2            AM search.
 *  Bit 3            Head selected.
 *  Bit 4            2844 head advance.
 *  Bit 5            End of Cylinder.
 *  Bit 6            Head set.
 *  Bit 7            Seek in progress.
 */

static void
seek_callback(struct _device *unit, void *arg, int iarg)
{
    struct _dasd_t *dasd = (struct _dasd_t *)unit;
    log_disk("Seek done %d head %x\n", dasd->ncyl, dasd->head);
    dasd->attn = 1;
    dasd->diff = 0;
    dasd->flags &= ~1;
    dasd->cyl = dasd->ncyl;
    dasd->status |= READY;
}

void
dasd_settags(struct _dasd_t *dasd, uint8_t ft, uint8_t fc)
{
    if ((ft & BIT7) == 0)
        return;
    log_disk("tags  %02x %02x head=%d flags=%02x\n", ft, fc, dasd->head, dasd->flags);
    if (ft & BIT0) {    /* Handle control function */
        if (fc & BIT0) {
            if (dasd->state == DK_POS_UNK) {
                dasd_update(dasd);
            }
        }
        if (fc & BIT1) { /* Read gate */
            dasd->attn = 0;
            log_disk("Clear attn\n");
            if ((fc & BIT5) != 0 && dasd->state == DK_POS_UNK) {
                dasd_update(dasd);
            }
            if (disk_type[dasd->type].dev_type == 0x14) {
                /* Check if turning on read gate */
                if ((dasd->flags & BIT1) == 0) {
                    dasd->am_search = 1;
                    log_disk("Set am search\n");
                }
            } else {
                dasd->am_search = 0;
                if ((fc & (BIT5|BIT7)) == (BIT5|BIT7)) {
                    dasd->am_search = 1;
                    log_disk("Set am search\n");
                }
            }
        }
        if (fc & BIT2) { /* Start seek */
            log_disk("Start seek to %02x, diff = %d, dir=%d\n",
                    dasd->ncyl, dasd->diff, dasd->dir);
            if (dasd->diff != 0) {
                add_event((struct _device *)dasd, seek_callback, 50,  NULL, 0);
                dasd->flags |= 1;
                dasd->status &= ~READY;
            }
        }
        if (fc & BIT3) {  /* Head reset */
            dasd->flags &= 0xF9;
            log_disk("Head reset\n");
        }
        if (fc & BIT5) { /* Select head */
            log_disk("Select head %d\n", dasd->head);
            dasd->flags |= 0x10;
        }
        if (fc & BIT6) {  /* Recalibrate */
            log_disk("Recalibrate %d\n", dasd->head);
            dasd->ncyl = 0;
            dasd->diff = 0;
            dasd->head = 0;
            dasd->flags |= 1;
            dasd->status &= ~READY;
            dasd->tstart = (dasd->tsize * dasd->head);
            add_event((struct _device *)dasd, seek_callback, 50,  NULL, 0);
        }
        if (disk_type[dasd->type].dev_type != 0x14) { /* Head advance */
            if (ft & BIT4) {
               dasd->head++;
               log_disk("Head Advance %d\n", dasd->head);
               if (dasd->head >= disk_type[dasd->type].heads) {
                   dasd->flags |= 4;
                   dasd->head = 0;
               }
               dasd->tstart = (dasd->tsize * dasd->head);
            }
        } else if (fc & BIT7) {
            if (dasd->flags & 0x8) {
                dasd->flags &= ~0x8;
            } else {
                dasd->head++;
                log_disk("Head Advance %d\n", dasd->head);
                if (dasd->head >= disk_type[dasd->type].heads) {
                    dasd->flags |= 4;
                    dasd->head = 0;
                }
                dasd->flags |= 0x8;
                dasd->tstart = (dasd->tsize * dasd->head);
            }
        }
        /* Update read and write gate */
        dasd->flags = (dasd->flags & 0x3f) | (fc & 0xc0);
    }
    if (ft & BIT1) {    /* Set cylinder */
        dasd->ncyl = fc;
        log_disk("set cyl  %02x\n", fc);
    }
    if (ft & BIT2) {    /* Set Head and sign */
        if (dasd->type == 3) {    /* 2314 */
            dasd->dir = (fc & BIT0) != 0;
            dasd->head = (fc & 0xf);
        } else {                  /* 2311 */
            if ((dasd->flags & 2) == 0) {
               dasd->head = (fc & 0xf);
               dasd->flags |= 2;
            } else {
               dasd->dir = (fc & BIT0) != 0;
            }
        }
        dasd->tstart = (dasd->tsize * dasd->head);
        log_disk("set diff %d head %x\n", dasd->dir, dasd->head);
    }
    if (ft & BIT3) {    /* Set Difference */
        dasd->diff = fc;
        log_disk("set diff  %02x\n", fc);
    }
}

uint8_t
dasd_gettags(struct _dasd_t *dasd)
{
    uint8_t   res = 0;
    /* Check if drive online */
    if (dasd->file_name == NULL) {
        return res;
    }
    res |= BIT1;
    /* Check if head invalid */
    if ((dasd->flags & 4) != 0) {
        res |= BIT5;
    }
    /* On 2844 check for write current */
    if (disk_type[dasd->type].dev_type == 0x14) {
        if ((dasd->flags & BIT0) != 0) {
            res |= BIT3;
        }
        /* Check if drive ready */
        if ((dasd->status & READY) == 0) {
            res |= BIT0;
        }
    } else {
        /* Check if drive ready */
        if ((dasd->status & READY) != 0) {
            res |= BIT0;
        }
    }
log_disk("FS = %02x\n", res);
    return res;
}

uint8_t
dasd_cur_cyl(struct _dasd_t *dasd)
{
    return dasd->cyl;
}

int
dasd_check_attn(struct _dasd_t *dasd)
{
    return dasd->attn;
}

int
dasd_check_seek(struct _dasd_t *dasd)
{
    return (dasd->flags & 1) != 0;
}


/**
 *
 *  Update the disk state to the current state and count.
 *  Used when swithing to a new drive.
 */

void
dasd_update(struct _dasd_t *dasd)
{
    int        state = DK_POS_INDEX;  /* Start at index point */
    int        pos;          /* Current position on disk */
    uint8_t    *rec;         /* Pointer to current record header */
    uint8_t    *da;          /* Pointer to current data */
    int        tpos;         /* Position within track */
    int        rpos;         /* Position of head of current record */
    int        count;        /* Position counter */
    int        type  = dasd->type;

    pos = ((dasd->tsize * disk_type[type].heads) * dasd->cyl) + sizeof(struct dasd_header);
    /* Check if read or write command, if so grab correct cylinder */
    if (dasd->fpos != pos) {
        uint32_t tsize = dasd->tsize * disk_type[type].heads;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              (void)write(dasd->fd, dasd->cbuf, tsize);
              dasd->dirty = 0;
        }
        dasd->fpos = pos;
        log_disk("Load cyl=%d %x\n", dasd->cyl, dasd->fpos);
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        (void)read(dasd->fd, dasd->cbuf, tsize);
        dasd->tstart = (dasd->tsize * dasd->head);
    }

    count = tpos = rpos = 0;
log_disk("Update position %d\n", dasd->cpos);
    for (pos = 0; pos < dasd->cpos; pos ++) {
         rec = &dasd->cbuf[rpos + dasd->tstart];
         da = &dasd->cbuf[tpos + dasd->tstart];
log_disk("State=%s %d t=%d r=%d\n", disk_state[state], count, tpos, rpos);
         switch(state) {
         case DK_POS_INDEX:             /* At Index Mark */
              if (count > disk_type[type].g4) {
                  dasd->tstart = dasd->tsize * (dasd->head);
                  tpos = rpos = 0;
                  count = -1;
                  state = DK_POS_HA;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
                  if (disk_type[type].dev_type == 0x14) {
                      dasd->ck_sum[0] ^= disk_type[type].sync;
                  }
              }
log_disk("Index\n");
              break;

         case DK_POS_HA:                /* In home address (c) */
              /* Return 5 bytes + 2 checksum */
              switch (count) {
              case 0:   /* Track flag */
              case 1:   /* Cyl High */
              case 2:   /* Cyl low */
              case 3:   /* Head high */
              case 4:   /* Head low */
                    dasd->ck_sum[count & 1] ^= rec[count];
                    tpos++;
                    break;
              case 5:   /* Checksum 1 */
                    break;
              case 6:   /* Checksum 2 */
                    tpos = rpos = 5;
                    if (disk_type[type].dev_type != 0x14) {
                        state = DK_POS_GAP1;
                        count = -1;
                    }
                    break;
              case 7:  /* 2314 Unit check */
                    break;
              case 8:  /* 2314 Bit count */
                    state = DK_POS_GAP1;
                    count = -1;
                    break;
                   }
//log_disk("HA Check=%02x %02x %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count]);
              break;

         case DK_POS_GAP1:
              if (count < disk_type[type].g1) {
                 break;
              }
              dasd->ck_sum[0] = 0xff;
              dasd->ck_sum[1] = 0xff;
              if (disk_type[type].dev_type == 0x14) {
                  dasd->ck_sum[0] ^= disk_type[type].sync;
              }
              count = -1;
              rec = &dasd->cbuf[rpos + dasd->tstart];
              state = DK_POS_CNT0;
log_disk("GAP1 c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d t=%d r=%d t=%d h=%d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
         rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos, dasd->tstart, dasd->head);
log_disk("Gap1 %d\n", count);
              break;

         case DK_POS_CNT0:              /* In count (c) */
              rec = &dasd->cbuf[rpos + dasd->tstart];
              switch(count) {
              case 0:    /* Flag */
                      /* Check for end of track */
                      if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                         state = DK_POS_END;
                      } else {
                         dasd->klen = rec[5];
                         dasd->dlen = (rec[6] << 8) | rec[7];
                      }
                      break;
                      /* Fall through */
              case 1:   /* Cyl high */
              case 2:   /* Cyl low */
              case 3:   /* Head high */
              case 4:   /* Head low */
              case 5:   /* Record ID */
              case 6:   /* Length of Key */
              case 7:   /* Length of data high */
              case 8:   /* Length of data low */
                      dasd->ck_sum[(count - 1) & 1] ^= rec[count - 1];
                      tpos++;
                      break;
              case 9:   /* Checksum 1 */
                      break;
              case 10:   /* Checksum 2 */
                      if (disk_type[type].dev_type != 0x14) {
                          state = DK_POS_GAP2;
                          if (dasd->klen == 0)
                              state = DK_POS_GAP3;
                          count = -1;
                      }
                      break;
              case 11:  /* 2314 Unit check */
                      break;
              case 12:  /* 2314 Bit count */
                      state = DK_POS_GAP2;
                      if (dasd->klen == 0)
                          state = DK_POS_GAP3;
                      count = -1;
                      break;
              }
log_disk("CNT0 Check=%02x %02x %02x %d %d\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count - 1], count, tpos);
              break;


         case DK_POS_AM:                /* Beginning of record */
              if (count < (disk_type[type].g2))
                  break;
              rec = &dasd->cbuf[rpos + dasd->tstart];
              if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                 state = DK_POS_END;
                 count = -1;
                 break;
              }
              count = -1;
              state = DK_POS_CNT1;
log_disk("AM %d\n", count);
              break;

         case DK_POS_CNT1:
              rec = &dasd->cbuf[rpos + dasd->tstart];
              switch(count) {
              case 0:   /* Flag */
                      /* Check for end of track */
                      if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                         state = DK_POS_END;
                         count = 0;
                      } else {
                         dasd->klen = rec[5];
                         dasd->dlen = (rec[6] << 8) | rec[7];
                      }
                      break;
              case 1:   /* Cyl high */
              case 2:   /* Cyl low */
              case 3:   /* Head high */
              case 4:   /* Head low */
              case 5:   /* Record ID */
              case 6:   /* Length of Key */
              case 7:   /* Length of data high */
              case 8:   /* Length of data low */
                      dasd->ck_sum[(count - 1) & 1] ^= rec[(count - 1)];
                      tpos++;
                      break;
              case 9:   /* Checksum 1 */
                      break;
              case 10:   /* Checksum 2 */
                      state = DK_POS_GAP2;
                      if (dasd->klen == 0)
                          state = DK_POS_GAP3;
                      count = -1;
                      break;
              }
log_disk("CNT1 c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d t=%d r=%d t=%d h=%d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
         rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos, dasd->tstart, dasd->head);
log_disk("CNT1 Check=%02x %02x %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count-1]);
              break;

         case DK_POS_GAP2:                /* Beginning of record */
              if (count < disk_type[type].g2) {
                 break;
              }
              count = -1;
              state = DK_POS_KEY;
              dasd->ck_sum[0] = 0xff;
              dasd->ck_sum[1] = 0xff;
                  if (disk_type[type].dev_type == 0x14) {
                      dasd->ck_sum[0] ^= disk_type[type].sync;
                  }
log_disk("Gap2 %d\n", count);
              break;

         case DK_POS_KEY:               /* In Key area */
              if (count < dasd->klen) {
                  dasd->ck_sum[count & 1] ^= da[count];
                  tpos++;
              }
              if (count == (dasd->klen+2)) {
                  state = DK_POS_GAP3;
                  count = -1;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
                  if (disk_type[type].dev_type == 0x14) {
                      dasd->ck_sum[0] ^= disk_type[type].sync;
                  }
              }
log_disk("Key %d %d %02x\n", count, dasd->klen, da[count]);
              break;

         case DK_POS_GAP3:                /* Beginning of record */
              if (count < disk_type[type].g2) {
                 break;
              }
              count = -1;
              state = DK_POS_DATA;
log_disk("Gap3 %d\n", count);
              break;

         case DK_POS_DATA:              /* In Data area */
              if (count < dasd->dlen) {
                  dasd->ck_sum[count & 1] ^= da[count];
                  tpos++;
              }
              if (count == (dasd->dlen + 2)) {
                  rpos = tpos;
                  state = DK_POS_AM;
                  count = -1;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
                  if (disk_type[type].dev_type == 0x14) {
                      dasd->ck_sum[0] ^= disk_type[type].sync;
                  }
              }
log_disk("Data %d %d %02x\n", count, dasd->dlen, da[count]);
              break;

         case DK_POS_END:               /* Past end of data */
              break;
         }
         count++;
     }
     dasd->count = count;
     dasd->state = state;
     dasd->rpos = rpos;
     dasd->tpos = tpos;
log_disk("Update=%d %d r=%d t=%d\n", state, count, rpos, tpos);
}


/*
 * Step a disk that is not the currently selected one around the track.
 *
 * Must call dasd_update, to resync positions after this is called.
 */
void
dasd_step(struct _dasd_t *dasd, uint8_t *ix)
{
    int                 type = dasd->type;

log_disk("Disk step %d %d %d c=%d h=%d %s\n", dasd->step, dasd->cpos, disk_type[type].bpt, dasd->cyl, dasd->head, dasd->file_name);
    *ix = 0;
    if (dasd->step < disk_type[type].rate) {
        dasd->step++;
        return;
    }
    dasd->state = DK_POS_UNK;
    dasd->step = 0;

    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
        dasd->state = DK_POS_INDEX;
        dasd->cpos = -1;
        *ix = 1;
    }

    dasd->cpos++;
}


/*
 * Read one byte off the disk.
 * If read, return 1. Else return 0.
 * Set am if address mark detected.
 */
int
dasd_read_byte(struct _dasd_t *dasd, uint8_t *data, uint8_t *am, uint8_t *ix)
{
    int                 type = dasd->type;
    uint16_t            count = dasd->count;
    uint8_t             *rec;
    uint8_t             *da;
    size_t               pos;

    if (dasd->step < disk_type[type].rate) {
        dasd->step++;
        return 0;
    }
log_disk("Disk read %s %d %d\n", disk_state[dasd->state], count, dasd->cpos);
    dasd->step = 0;
    *am = 0;
    pos = ((dasd->tsize * disk_type[type].heads) * dasd->cyl) + sizeof(struct dasd_header);
    /* Check if read or write command, if so grab correct cylinder */
    if (dasd->fpos != pos) {
        uint32_t tsize = dasd->tsize * disk_type[type].heads;
        int r;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              r = write(dasd->fd, dasd->cbuf, tsize);
              if (r != tsize) {
                  log_error("Disk write on %s %d\n", dasd->file_name, r);
              }
              dasd->dirty = 0;
        }
        dasd->fpos = pos;
        log_disk("Load cyl=%d %x\n", dasd->cyl, dasd->fpos);
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        r = read(dasd->fd, dasd->cbuf, tsize);
        if (r != tsize) {
            log_error("Disk read on %s %d\n", dasd->file_name, r);
        }
        dasd->tstart = (dasd->tsize * dasd->head);
    }
    log_disk("state %s %d ams=%d h=%d\n", disk_state[dasd->state], dasd->tpos,
                  dasd->am_search, dasd->head);

    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
        log_disk("state end %d\n", dasd->tpos);
        dasd->tstart = (dasd->tsize * dasd->head);
        dasd->state = DK_POS_INDEX;
        dasd->cpos = -1;
        dasd->tpos = 0;
        dasd->count = 0;
        count = 0;
        *ix = 1;
    }

    rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
    da = &dasd->cbuf[dasd->tpos + dasd->tstart];

    dasd->cpos++;
    dasd->count++;
    dasd->dirty = 1;

    switch(dasd->state) {
    case DK_POS_INDEX:             /* At Index Mark */
log_disk("Gap0=%02x %d\n", *data, count);
         if (count == disk_type[type].g4) {
             dasd->tstart = dasd->tsize * (dasd->head);
             dasd->tpos = dasd->rpos = 0;
             dasd->count = 0;
             dasd->state = DK_POS_HA;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             *data = disk_type[type].sync;
             if (disk_type[type].dev_type == 0x14) {
                 *am = 1;
                 dasd->am_search = 0;
             } else {
                 dasd->am_search = 0;
             }
         } else {
             if (count < 3) {
                 *ix = 1;
             }
             return 0;
         }
         break;

    case DK_POS_HA:                /* In home address (c) */
         /* Return 5 bytes + 2 checksum */
         switch (count) {
         case 0:   /* Track flag */
         case 1:   /* Cyl High */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
               *data = rec[count];
               dasd->ck_sum[dasd->count & 1] ^= rec[count];
               dasd->tpos++;
               break;
         case 5:   /* Checksum 1 */
log_disk("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
               *data = dasd->ck_sum[0];
               break;
         case 6:   /* Checksum 2 */
               *data = dasd->ck_sum[1];
               dasd->tpos = dasd->rpos = 5;
               if (disk_type[type].dev_type != 0x14) {
                   dasd->state = DK_POS_GAP1;
                   dasd->count = 0;
               }
               break;
         case 7:  /* 2314 Unit check */
               *data = 0x01;
               break;
         case 8:  /* 2314 Bit count */
               *data = 0xff;
               dasd->state = DK_POS_GAP1;
               dasd->count = 0;
               break;
         }
log_disk("HA Check=%02x %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], dasd->tpos);
         break;

    case DK_POS_GAP1:
log_disk("Gap1  %d\n", count);
         if (count == disk_type[type].g1) {
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             /* Check for end of track */
             if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                dasd->state = DK_POS_END;
                return 0;
             }
             *data = disk_type[type].sync;
             if (disk_type[type].dev_type == 0x14) {
                 *data = 0x3;
                 *am = 1;
                 dasd->am_search = 0;
             }
             dasd->count = 0;
             dasd->state = DK_POS_CNT0;
             if (dasd->am_search)
                 return 0;
         } else {
             return 0;
         }
         break;

    case DK_POS_CNT0:              /* In count (c) */
         switch(count) {
         case 0:    /* Flag */
                 dasd->rcnt = 0;
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
                 *data = 0;
log_disk("CNT0 %02x %02x %02x %02x %02x %d %d\n", rec[0], rec[1], rec[2], rec[3], rec[4], dasd->klen, dasd->dlen);
                 break;

         case 1:    /* Cyl high */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
         case 5:   /* Record ID */
         case 6:   /* Length of Key */
         case 7:   /* Length of data high */
         case 8:   /* Length of data low */
                 *data = rec[count - 1];
                 dasd->ck_sum[(count - 1) & 1] ^= *data;
                 dasd->tpos++;
                 break;
         case 9:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
                 break;
         case 10:  /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 if (disk_type[type].dev_type != 0x14) {
                     dasd->state = DK_POS_GAP2;
                     if (dasd->klen == 0)
                         dasd->state = DK_POS_GAP3;
                     dasd->count = 0;
                 }
                 break;
         case 11:  /* 2314 Unit check */
                 *data = 0x01;
                 break;
         case 12:  /* 2314 Bit count */
                 *data = 0xff;
                 dasd->state = DK_POS_GAP2;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_GAP3;
                 dasd->count = 0;
                 break;
         }
log_disk("CNT0 Check=%02x %02x %d %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data, dasd->tpos);
         if (dasd->am_search)
             return 0;
         break;


    case DK_POS_AM:                /* Beginning of record */
log_disk("AM  %d\n", count);
         if (count == disk_type[type].g2) {
             *data = disk_type[type].sync;
              dasd->ck_sum[0] = 0xff;
              dasd->ck_sum[1] = 0xff;
log_disk("AM %d %02x %02x %02x %02x %02x %d %d\n", dasd->rpos, rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], (rec[6] << 8) | rec[7]);
              /* Check for end of track */
              if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                 dasd->state = DK_POS_END;
                 dasd->count = 0;
                 return 0;
              }
              if (disk_type[type].dev_type == 0x14) {
                 *data = 0x6;
                 *am = 1;
              }
              dasd->count = 0;
              dasd->state = DK_POS_CNT1;
         } else if (count > (disk_type[type].g2 - 2)) {
              *data = 0xff;
              *am = 1;
              dasd->am_search = 0;
              return 0;
         } else {
              return 0;
         }
         break;

    case DK_POS_CNT1:
         rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
         switch(count) {
         case 0:    /* Flag */
                 dasd->rcnt++;
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                    dasd->count = 0;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
                 *data = 0;
                 if (rec[0] & 0x80)
                    *data |= 0x40;
                 if (dasd->rcnt & 1)
                    *data |= 0x80;
log_disk("CNT1x %02x r=%02x %02x %02x %02x %02x %d %d\n", *data, rec[0], rec[1], rec[2], rec[3], rec[4], dasd->klen, dasd->dlen);
                 dasd->ck_sum[1] ^= *data;
                 break;
         case 1:    /* Cyl high */
                 *data = rec[0] & 0x7f;  /* Remove overflow indicator */
                 dasd->ck_sum[0] ^= *data;
                 dasd->tpos++;
                 break;
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
         case 5:   /* Record ID */
         case 6:   /* Length of Key */
         case 7:   /* Length of data high */
         case 8:   /* Length of data low */
                 *data = rec[count - 1];
                 dasd->ck_sum[(count - 1) & 1] ^= *data;
                 dasd->tpos++;
                 break;
         case 9:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
                 break;
         case 10:  /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 if (disk_type[type].dev_type != 0x14) {
                     dasd->state = DK_POS_GAP2;
                     if (dasd->klen == 0)
                         dasd->state = DK_POS_GAP3;
                     dasd->count = 0;
                 }
                 break;
         case 11:  /* 2314 Unit check */
                 *data = 0x01;
                 break;
         case 12:  /* 2314 Bit count */
                 *data = 0xff;
                 dasd->state = DK_POS_GAP2;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_GAP3;
                 dasd->count = 0;
                 break;
         }
log_disk("CNT1 Check=%02x %02x %d %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data, dasd->tpos);
         if (dasd->am_search)
             return 0;
         break;

    case DK_POS_GAP2:
log_disk("GAP2  %d\n", count);
         if (count == disk_type[type].g2) {
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             dasd->count = 0;
             dasd->state = DK_POS_KEY;
             *data = disk_type[type].sync;
             if (disk_type[type].dev_type == 0x14) {
                 *data = 0x02;
                 dasd->am_search = 0;
                 *am = 1;
             }
             if (dasd->am_search)
                 return 0;
         } else {
             return 0;
         }
         break;

    case DK_POS_KEY:               /* In Key area */
         if (count < dasd->klen) {
             *data = *da;
             dasd->ck_sum[count & 1] ^= *da;
             dasd->tpos++;
         } else {
           switch (count - dasd->klen) {
           case 0:
                  *data = dasd->ck_sum[0];
                  break;
           case 1:
                  *data = dasd->ck_sum[1];
                  if (disk_type[type].dev_type != 0x14) {
                      dasd->state = DK_POS_GAP3;
                      dasd->count = 0;
                  }
                  break;
           case 2: /* 2314 Unit check */
                  *data = 0x01;
                  break;
           case 3:  /* 2314 Bit count */
                  *data = 0xff;
                  dasd->state = DK_POS_GAP3;
                  dasd->count = 0;
                  break;
            }
         }
log_disk("KEY Check=%02x %02x %d %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data);
         if (dasd->am_search)
             return 0;
         break;

    case DK_POS_GAP3:
log_disk("GAP3  %d\n", count);
         if (count == disk_type[type].g2) {
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             dasd->count = 0;
             dasd->state = DK_POS_DATA;
             *data = disk_type[type].sync;
             if (disk_type[type].dev_type == 0x14) {
                 *data = 0x01;
                 *am = 1;
                 dasd->am_search = 0;
             }
             if (dasd->am_search)
                 return 0;
         } else {
             return 0;
         }
         break;


    case DK_POS_DATA:              /* In Data area */
         if (count < dasd->dlen) {
             *data = *da;
             dasd->ck_sum[count & 1] ^= *da;
             dasd->tpos++;
         } else {
           switch (count - dasd->dlen) {
           case 0:
                  *data = dasd->ck_sum[0];
                  break;
           case 1:
                  *data = dasd->ck_sum[1];
                  dasd->rpos = dasd->tpos;
                  if (disk_type[type].dev_type != 0x14) {
                      dasd->state = DK_POS_AM;
                      dasd->count = 0;
                  }
                  break;
           case 2: /* 2314 Unit check */
                  *data = 0x01;
                  break;
           case 3:  /* 2314 Bit count */
                  *data = 0xff;
                  dasd->state = DK_POS_AM;
                  dasd->count = 0;
                  break;
            }
         }
log_disk("DATA Check=%02x %02x %d %d %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], count, dasd->dlen, *data);
         if (dasd->am_search)
             return 0;
         break;

    case DK_POS_END:               /* Past end of data */
log_disk("End %d %d\n", dasd->cpos, dasd->tpos);
         dasd->count = 0;
         dasd->klen = 0;
         dasd->dlen = 0;
         if (dasd->cpos >= (disk_type[type].bpt + 1)) {
             dasd->cpos = 0;
             dasd->state = DK_POS_INDEX;             /* At Index Mark */
             *ix = 1;
         }
         return 0;
    }
    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
        dasd->cpos = 0;
        dasd->state = DK_POS_INDEX;             /* At Index Mark */
        *ix = 1;
    }
    if (dasd->am_search)
        return 0;
    return 1;
}

int
dasd_write_byte(struct _dasd_t *dasd, uint8_t *data, uint8_t *am, uint8_t *ix)
{
    int                 type = dasd->type;
    int                 count = dasd->count;
    uint8_t             *rec;
    uint8_t             *da;
    size_t               pos;
    int                  i;

    if (dasd->step < disk_type[type].rate) {
        dasd->step++;
        return 0;
    }
    dasd->step = 0;
    pos = ((dasd->tsize * disk_type[type].heads) * dasd->cyl) + sizeof(struct dasd_header);
    /* Check if read or write command, if so grab correct cylinder */
    if (dasd->fpos != pos) {
        uint32_t tsize = dasd->tsize * disk_type[type].heads;
        int r;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              r = write(dasd->fd, dasd->cbuf, tsize);
              if (r != tsize) {
                  log_error("Disk write on %s %d\n", dasd->file_name, r);
              }
              dasd->dirty = 0;
        }
        dasd->fpos = pos;
        log_disk("Load cyl=%d %x\n", dasd->cyl, dasd->fpos);
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        r = read(dasd->fd, dasd->cbuf, tsize);
        if (r != tsize) {
            log_error("Disk read on %s %d\n", dasd->file_name, r);
        }
        dasd->tstart = (dasd->tsize * dasd->head);
    }
    log_disk("state %s %d\n", disk_state[dasd->state], dasd->cpos);

    if (dasd->tpos >= dasd->tsize) {
        log_disk("state end %d\n", dasd->tpos);
        dasd->state = DK_POS_INDEX;
        dasd->cpos = -1;
        dasd->tpos = -1;
        dasd->count = 0;
        count = 0;
        *ix = 1;
    }

    rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
    da = &dasd->cbuf[dasd->tpos + dasd->tstart];

    dasd->count++;
    dasd->cpos++;
    switch(dasd->state) {
    case DK_POS_INDEX:             /* At Index Mark */
         if (*data == 0) {
             dasd->count = 0;
         }
         if (*data == 0xff) {
             dasd->count++; // = disk_type[type].g1 - 1;
log_disk("All ones\n");
         }
         if (disk_type[dasd->type].dev_type == 0x14) {
             /* If sync start writing HA */
             if (*data == 0xd) {
                 dasd->tpos = dasd->rpos = 0;
                 dasd->count = 0;
                 dasd->state = DK_POS_HA;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 break;
              }
         } else {
             if (*data == 0xe) {
log_disk("Sync\n");
//             dasd->tstart = dasd->tsize * (dasd->head);
                 dasd->tpos = dasd->rpos = 0;
                 dasd->count = 0;
                 dasd->state = DK_POS_HA;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
              }
         }
         break;

    case DK_POS_HA:                /* In home address (c) */
         /* Return 5 bytes + 2 checksum */
         switch (count) {
         case 0:   /* Track flag */
         case 1:   /* Cyl High */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
               dasd->tpos++;
               rec[count] = *data;
               dasd->ck_sum[count & 1] ^= *data;
               break;
         case 5:   /* Checksum 1 */
log_disk("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
               break;
         case 6:   /* Checksum 2 */
               dasd->ck_sum[0] = 0xff;
               dasd->ck_sum[1] = 0xff;
               dasd->tpos = dasd->rpos = 5;
               if (disk_type[type].dev_type != 0x14) {
                   dasd->state = DK_POS_GAP1;
                   dasd->count = 0;
               }
               break;
         case 7:  /* 2314 Unit check */
               break;
         case 8:  /* 2314 Bit count */
               dasd->state = DK_POS_GAP1;
               dasd->count = 0;
               break;
         }
log_disk("HA Check=%02x %02x %d s=%d h=%d\n", dasd->ck_sum[0], dasd->ck_sum[1], dasd->tpos, dasd->tstart, dasd->head);
         break;

    case DK_POS_GAP1:
         if (*data == 0) {
             dasd->count = 0;
         }
         if (*data == 0xff) {
             dasd->count--;
         }
         if (dasd->count >= disk_type[type].g1) {
log_disk("Overrun\n");
             dasd->tpos = dasd->rpos = 5;
             rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
             dasd->count = 0;
             dasd->state = DK_POS_END;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             for (i = 0; i < 8; i++)
                  rec[i] = 0xff;
         }
         if (disk_type[type].dev_type != 0x14) {
             if (*data == 0xe /*&& dasd->count >= disk_type[type].g1*/) {
log_disk("Sync 1\n");
 //            dasd->tstart = dasd->tsize * (dasd->head);
                 dasd->tpos = dasd->rpos = 5;
                 dasd->count = 0;
                 dasd->state = DK_POS_CNT0;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 log_disk("GAP1 c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d t=%d r=%d t=%d h=%d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
                  rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos, dasd->tstart, dasd->head);
               }
               for (i = 0; i < 8; i++)
                  rec[i] = 0xff;
         } else {
               if (*data == 0xb) {
                 dasd->tpos = dasd->rpos = 5;
                 dasd->count = 0;
                 dasd->state = DK_POS_CNT0;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 log_disk("GAP1 c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d t=%d r=%d t=%d h=%d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
                  rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos, dasd->tstart, dasd->head);
               }
               for (i = 0; i < 8; i++)
                  rec[i] = 0xff;
         }
         break;

    case DK_POS_CNT0:              /* In count (c) */
         rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
         switch(count) {
         case 0:    /* Flag */
                 dasd->klen = *data;   /* Save flag for next byte */
log_disk("CNT0 f c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d %d %d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
         rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos);
                 break;
         case 1:    /* Cyl high */
                 *data |= (dasd->klen & 0x40) ? 0x80:0;
                 dasd->klen = 0;
                 /* Fall through */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
         case 5:   /* Record ID */
         case 6:   /* Length of Key */
         case 7:   /* Length of data high */
         case 8:   /* Length of data low */
                 dasd->tpos++;
                 rec[(count - 1)] = *data;
                 dasd->ck_sum[(count - 1) & 1] ^= *data;
log_disk("CNT0 c=%02x %02x h=%02x %02x r=%02x k=%02x d=%02x %02x %d t=%d r=%d t=%d h=%d r=%d\n", rec[0], rec[1], rec[2], rec[3], rec[4],
         rec[5], rec[6], rec[7], count, dasd->tpos, dasd->rpos, dasd->tstart, dasd->head, dasd->rpos);
                 break;
         case 9:   /* Checksum 1 */
                 break;
         case 10:   /* Checksum 2 */
log_disk("CNT0 %02x %02x %02x %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4],
            rec[5], rec[6], rec[7]);
                 dasd->klen = rec[5];
                 dasd->dlen = (rec[6] << 8) | rec[7];
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 if (disk_type[type].dev_type != 0x14) {
                     dasd->state = DK_POS_GAP2;
                     if (rec[5] == 0)  /* Check key length */
                         dasd->state = DK_POS_GAP3;
                     dasd->count = 0;
                 }
                 break;
         case 11:  /* 2314 Unit check */
                 break;
         case 12:  /* 2314 Bit count */
                 dasd->state = DK_POS_GAP2;
                 if (rec[5] == 0)  /* Check key length */
                     dasd->state = DK_POS_GAP3;
                 dasd->count = 0;
                 break;
         }
         break;

    case DK_POS_AM:                /* Beginning of record */
         if (*data == 0) {
             dasd->count = 0;
         }
log_disk("AM %d\n", *am);
         if (*am != 0) {
             for (i = 0; i < 8; i++)
                 rec[i] = 0xff;
         }
         if (*data == 0xff) {
             dasd->count = disk_type[type].g1 - 1;
log_disk("Data 0xff %d\n", dasd->ck_sum[0]);
             dasd->ck_sum[0]++;
             if (dasd->ck_sum[0] > (dasd->count + 10)) {
                 for (i = 0; i < 8; i++)
                    rec[i] = 0xff;
             }
         }
         if (*data == 0xe && dasd->count >= disk_type[type].g1) {
log_disk("Sync am\n");
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             dasd->count = 0;
             dasd->state = DK_POS_CNT1;
             for (i = 0; i < 8; i++)
                  rec[i] = 0xff;
         }
         break;

    case DK_POS_CNT1:
         switch(count) {
         case 0:    /* Flag */
                 dasd->rcnt++;
                 dasd->rpos = dasd->tpos;
                 dasd->klen = *data;   /* Save flag for next byte */
                 break;
         case 1:    /* Cyl high */
                 *data |= (dasd->klen & 0x40) ? 0x80:0;
                 dasd->klen = 0;
                 /* Fall through */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
         case 5:   /* Record ID */
         case 6:   /* Length of Key */
         case 7:   /* Length of data high */
         case 8:   /* Length of data low */
                 dasd->tpos++;
                 *da = *data;
                 dasd->ck_sum[(dasd->count - 1) & 1] ^= *data;
                 break;
         case 9:   /* Checksum 1 */
         case 10:   /* Checksum 2 */
                 dasd->klen = rec[5];
                 dasd->dlen = (rec[6] << 8) | rec[7];
                 dasd->state = DK_POS_GAP2;
                 if (rec[5] == 0)  /* Check key length */
                     dasd->state = DK_POS_GAP3;
                 dasd->count = -1;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 break;
         }
         break;

    case DK_POS_GAP2:
         if (*data == 0) {
             dasd->count = 0;
         }
         if (*data == 0xff) {
             dasd->count--;
         }
         if (disk_type[type].dev_type != 0x14) {
             if (*data == 0xe /*&& dasd->count >= disk_type[type].g1*/) {
log_disk("Sync 2\n");
 //            dasd->tstart = dasd->tsize * (dasd->head);
                 dasd->count = 0;
                 dasd->state = DK_POS_KEY;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 log_disk("GAP3 sync\n");
               }
         } else {
               if (*data == 0xa) {
log_disk("Sync 2\n");
                 dasd->count = 0;
                 dasd->state = DK_POS_KEY;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 log_disk("GAP3 sync\n");
               }
         }
         break;

    case DK_POS_KEY:               /* In Key area */
         if (dasd->count <= dasd->klen) {
             dasd->tpos++;
             *da = *data;
             dasd->ck_sum[dasd->count & 1] ^= *da;
         } else if (dasd->count == (dasd->klen + 2)) {
             dasd->state = DK_POS_GAP3;
             dasd->count = 0;
         }
         break;

    case DK_POS_GAP3:
         if (*data == 0xcc) {
             dasd->count = 0;
         }
         if (*data == 0xff) {
             dasd->count--;
         }
         if (dasd->count >= disk_type[type].g2) {
log_disk("Overrun\n");
             dasd->tpos = dasd->rpos = 5;
             rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
             dasd->count = 0;
             dasd->state = DK_POS_END;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             for (i = 0; i < 8; i++)
                  rec[i] = 0xff;
         }
         if (disk_type[type].dev_type != 0x14) {
             if (*data == 0xe /*&& dasd->count >= disk_type[type].g1*/) {
log_disk("Sync 3\n");
 //            dasd->tstart = dasd->tsize * (dasd->head);
                 dasd->count = 0;
                 dasd->state = DK_POS_DATA;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 log_disk("GAP3 sync\n");
               }
         } else {
               if (*data == 0x9) {
log_disk("Sync 3\n");
                 dasd->count = 0;
                 dasd->state = DK_POS_DATA;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 log_disk("GAP3 sync\n");
               }
         }
#if 0
         if (*data == 0xcc) {
             dasd->count = 0;
         }
         if (*data == 0xff && dasd->count == 1) {
             dasd->count = 0;  /* Still in leader */
         }
         if (*data == 0xe) {
log_disk("Sync 3\n");
             dasd->count = 0;
             dasd->state = DK_POS_DATA;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
         }
#endif
         break;

    case DK_POS_DATA:              /* In Data area */
         if (dasd->count <= dasd->dlen) {
             dasd->tpos++;
             *da = *data;
log_disk("Write data %d : %02x\n", dasd->count, *data);
             dasd->ck_sum[dasd->count & 1] ^= *da;
         } else if (dasd->count == (dasd->dlen + 2)) {
             dasd->state = DK_POS_AM;
             dasd->rpos = dasd->tpos;
log_disk("Write checksum %d : %02x\n", dasd->count, *data);
             dasd->count = 0;
             dasd->ck_sum[0] = 0;
         }
         break;

    case DK_POS_END:               /* Past end of data */
         dasd->tpos+=10;
         dasd->count = 0;
         dasd->klen = 0;
         dasd->dlen = 0;
         break;
    }
    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
        dasd->cpos = -1;
        dasd->state = DK_POS_INDEX;             /* At Index Mark */
        *ix = 1;
    }
    return 1;
}

#if 0
/* Handle processing of disk requests. */
int dasd_srv(struct _dasd_t *dasd)
{
    int                 type = GET_TYPE(uptr->flags);
    int                 state;
    int                 count;
    int                 cpos;
    int                 trk;
    unsigned int        hd;
    int                 i;
    uint8_t             *rec;
    uint8_t             *da;
    uint8_t             ch;
    uint8_t             buf[8];


    state = data->state;
    count = data->count;
    /* Check if read or write command, if so grab correct cylinder */
    if (state != DK_POS_SEEK && rd && data->cyl != data->ccyl) {
        uint32_t tsize = data->tsize * disk_type[type].heads;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              (void)write(dasd->cbuf, 1, tsize, dasd->fd);
              dasd->dirty = 0;
        }
        dasd->ccyl = dasd->cyl;
        dasd->fpos = sizeof(struct dasd_header) + (dasd->ccyl * tsize);
        sim_debug(DEBUG_DETAIL, dptr, "Load unit=%d cyl=%d %x\n", unit, data->cyl, data->cpos);
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        (void)read(dasd->cbuf, 1, tsize, dasd->fd);
        state = DK_POS_INDEX;
        dasd->cpos = 0;
    }
    sim_debug(DEBUG_POS, dptr, "state unit=%d %02x %d\n", unit, state, data->tpos);

    rec = &dasd->cbuf[data->rpos + data->tstart];
    da = &dasd->cbuf[data->tpos + data->tstart];
    if (state != DK_POS_SEEK && dasd->tpos >= dasd->tsize) {
        sim_debug(DEBUG_POS, dptr, "state end unit=%d %d\n", unit, data->tpos);
        state = DK_POS_INDEX;
    }

    dasd->cpos++;
    switch(state) {
    case DK_POS_INDEX:             /* At Index Mark */
         if (dasd->cpos > disk_type[type].g1) {
             dasd->tstart = dasd->tsize * (dasd->head);
             dasd->tpos = dasd->rpos = 0;
             data->state = DK_POS_HA;
             data->rec = 0;
         }
         /* Return 34 0 */
         /* Return 1 0xff */
         /* Return 1 0x0e */
         break;

    case DK_POS_HA:                /* In home address (c) */
         dasd->tpos++;
         if (dasd->count == 4) {
             dasd->tpos = dasd->rpos = 5;
             dasd->state = DK_POS_CNT;
             rec = &data->cbuf[dasd->rpos + dasd->tstart];
             /* Check for end of track */
             if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff)
                dasd->state = DK_POS_END;
             dasd->gpos = 0;
         }
         /* Return 5 bytes + 2 checksum */
         /* Checksum, set reg1 and reg2 = 0xff */
         /* Even byte reg1 ^= byte */
         /* Odd byte reg2 ^= byte */
         break;
    case DK_POS_CNT:               /* In count (c) */
         dasd->gpos++;
         if (dasd->gpos < disk_type[type].g2)
             break;
         /* Record = 0 */
         /* Gap is: */
         /* 1 0xCC */
         /* 15 0x00 */
         /* 1  0xff */
         /* 1  0x0e */
         /* Record != 0 */

         /* 1 0xcc */
         /* 21 0xff */
         /* 4 0x00 */
         /* 1 0xff */
         /* 2 0xff + AM flag */
         /* 1 0x0e */
         data->tpos++;
         if (data->count == 0) {
             /* Check for end of track */
             if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                state = DK_POS_END;
                data->state = DK_POS_END;
                data->gpos = 0;
             } else {
                data->klen = rec[5];
                data->dlen = (rec[6] << 8) | rec[7];
             }
         }
         if (data->count == 7) {
             data->state = DK_POS_KEY;
             if (data->klen == 0)
                 data->state = DK_POS_DATA;
             data->gpos = 0;
         }
         break;
    case DK_POS_KEY:               /* In Key area */
         dasd->gpos++;
         if (dasd->gpos < disk_type[type].g2)
             break;
         /* Gap is: */
         /* 1 0xcc */
         /* 9 0xff */
         /* 6 0x00 */
         /* 1 0xff */
         /* 1 0x0e */
         data->tpos++;
         if (data->count == data->klen) {
             data->state = DK_POS_DATA;
             data->count = 0;
             count = 0;
             state = DK_POS_DATA;
             data->gpos = 0;
         }
         break;
    case DK_POS_DATA:              /* In Data area */
         dasd->gpos++;
         if (dasd->gpos < disk_type[type].g2)
             break;
         data->tpos++;
         if (data->count == data->dlen) {
             data->state = DK_POS_AM;
             data->gpos = 0;
         }
         break;
    case DK_POS_AM:                /* Beginning of record */
         dasd->gpos++;
         if (dasd->gpos < disk_type[type].g2)
             break;
         data->rpos += data->dlen + data->klen + 8;
         data->tpos = data->rpos;
         data->rec++;
         data->state = DK_POS_CNT;
         data->count = 0;
         rec = &data->cbuf[data->rpos + data->tstart];
         /* Check for end of track */
         if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff)
            data->state = DK_POS_END;
         sim_activate(uptr, 20);
         break;
    case DK_POS_END:               /* Past end of data */
         data->tpos+=10;
         data->count = 0;
         data->klen = 0;
         data->dlen = 0;
         sim_activate(uptr, 50);
         break;
    case DK_POS_SEEK:                  /* In seek */
         /* Compute delay based of difference. */
         /* Set next state = index */
         i = (uptr->CCH >> 8) - data->cyl;
         sim_debug(DEBUG_DETAIL, dptr, "seek unit=%d %d %d s=%x\n", unit, uptr->CCH >> 8, i,
                 data->state);
         if (i == 0) {
             uptr->CMD &= ~(DK_INDEX|DK_INDEX2);
             data->state = DK_POS_INDEX;
             sim_activate(uptr, 20);
         } else if (i > 0 ) {
             if (i > 20) {
                data->cyl += 20;
                sim_activate(uptr, 1000);
             } else {
                data->cyl ++;
                sim_activate(uptr, 200);
             }
         } else {
             if (i < -20) {
                data->cyl -= 20;
                sim_activate(uptr, 1000);
             } else {
                data->cyl --;
                sim_activate(uptr, 200);
             }
         }
         sim_debug(DEBUG_DETAIL, dptr, "seek next unit=%d %d %d %x\n", unit, uptr->CCH >> 8,
                data->cyl, data->state);
         break;
    }

}
#endif


static uint8_t ipl1rec[28] = {0xC9,0xD7,0xD3,0xF1,   /* IPL1 */
                              0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x0F,
                              0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
                              0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static uint8_t ipl2key[4] = {0xC9,0xD7,0xD3,0xF2};   /* IPL2 */
static uint8_t volrec[84] = {0xE5,0xD6,0xD3,0xF1,    /* VOL1, key */
                             0xE5,0xD6,0xD3,0xF1,    /* VOL1 */
                             0xF1,0xF1,0xF1,0xF1,0xF1,0xF1,  /* volid */
                             0x40,0x00,0x00,0x00,0x01,0x01,  /* CCHHR */
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0xE2,0xC9,0xD4,0xC8,0x40,  /* SIMH */
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
                             0x40,0x40};

int
dasd_format(struct _dasd_t * dasd, int flag) {
    struct dasd_header  hdr;
    int                 type = dasd->type;
    int                 tsize;
    int                 cyl;
    uint32_t            hd;
    int                 pos;
    int                 r;

    /* Create header */
    log_disk("Format\n");
    memset(&hdr, 0, sizeof(struct dasd_header));
    memcpy(&hdr.devid[0], "CKD_P370", 8);
    hdr.heads = disk_type[type].heads;
    hdr.tracksize = (disk_type[type].bpt | 0x1ff) + 1;
    hdr.devtype = disk_type[type].dev_type;
    (void)lseek(dasd->fd, 0, SEEK_SET);
    r = write(dasd->fd, &hdr, sizeof(struct dasd_header));
    if (r != sizeof(struct dasd_header)) {
        log_error("Disk write on %s %d\n", dasd->file_name, r);
        return 1;
    }

    /* Allocate cylinder buffer */
    tsize = hdr.tracksize * hdr.heads;
    dasd->tsize = hdr.tracksize;
    if (dasd->cbuf == NULL && (dasd->cbuf = (uint8_t *)calloc(tsize, sizeof(uint8_t))) == 0)
        return 1;

    /* Create empty disk with HA and R0 based on standard */
    for (cyl = 0; cyl < disk_type[type].cyl; cyl++) {
        pos = 0;
        for (hd = 0; hd < disk_type[type].heads; hd++) {
            int cpos = pos;
            dasd->cbuf[pos++] = 0;            /* HA */
            dasd->cbuf[pos++] = (cyl >> 8);
            dasd->cbuf[pos++] = (cyl & 0xff);
            dasd->cbuf[pos++] = (hd >> 8);
            dasd->cbuf[pos++] = (hd & 0xff);
            dasd->cbuf[pos++] = (cyl >> 8);   /* R0 */
            dasd->cbuf[pos++] = (cyl & 0xff);
            dasd->cbuf[pos++] = (hd >> 8);
            dasd->cbuf[pos++] = (hd & 0xff);
            dasd->cbuf[pos++] = 0;              /* Rec */
            dasd->cbuf[pos++] = 0;              /* keylen */
            dasd->cbuf[pos++] = 0;              /* dlen */
            dasd->cbuf[pos++] = 8;              /*  */
            pos += 8;
            dasd->cbuf[pos++] = (cyl >> 8);   /* R1 */
            dasd->cbuf[pos++] = (cyl & 0xff);
            dasd->cbuf[pos++] = (hd >> 8);
            dasd->cbuf[pos++] = (hd & 0xff);
            dasd->cbuf[pos++] = 1;              /* Rec */

            /* If flag create dummy IPL and VOLID records */
            if (cyl == 0 && hd == 0 && flag) {
                unsigned int p;
                /* R1, IPL1 */
                dasd->cbuf[pos++] = 4;              /* keylen */
                dasd->cbuf[pos++] = 0;              /* dlen */
                dasd->cbuf[pos++] = 24;              /*  */
                for (p = 0; p < sizeof (ipl1rec); p++)
                    dasd->cbuf[pos++] = ipl1rec[p];
                dasd->cbuf[pos++] = (cyl >> 8);   /* R2 */
                dasd->cbuf[pos++] = (cyl & 0xff);
                dasd->cbuf[pos++] = (hd >> 8);
                dasd->cbuf[pos++] = (hd & 0xff);
                dasd->cbuf[pos++] = 2;              /* Rec */
                /* R2, IPL2 */
                dasd->cbuf[pos++] = 4;              /* keylen */
                dasd->cbuf[pos++] = 0;              /* dlen */
                dasd->cbuf[pos++] = 144;            /*  */
                for (p = 0; p < sizeof (ipl2key); p++)
                    dasd->cbuf[pos++] = ipl2key[p];
                pos += 144;
                dasd->cbuf[pos++] = (cyl >> 8);   /* R3 */
                dasd->cbuf[pos++] = (cyl & 0xff);
                dasd->cbuf[pos++] = (hd >> 8);
                dasd->cbuf[pos++] = (hd & 0xff);
                dasd->cbuf[pos++] = 3;              /* Rec */
                /* R3, VOL1 */
                dasd->cbuf[pos++] = 4;              /* keylen */
                dasd->cbuf[pos++] = 0;              /* dlen */
                dasd->cbuf[pos++] = 80;             /*  */
                for (p = 0; p < sizeof (volrec); p++) {
                    if (p >= 8 && p <= 16 && dasd->vol_label[p - 8] != 0) {
                        dasd->cbuf[pos++] = ascii_to_ebcdic[(int)dasd->vol_label[p - 8]];
                    } else {
                        dasd->cbuf[pos++] = volrec[p];
                    }
                }
            } else {
                dasd->cbuf[pos++] = 0;              /* keylen */
                dasd->cbuf[pos++] = 0;              /* dlen */
                dasd->cbuf[pos++] = 0;              /*  */
            }
            dasd->cbuf[pos++] = 0xff;           /* End record */
            dasd->cbuf[pos++] = 0xff;
            dasd->cbuf[pos++] = 0xff;
            dasd->cbuf[pos++] = 0xff;
            pos = cpos + dasd->tsize;
        }
        r = write(dasd->fd, dasd->cbuf, tsize);
        if (r != tsize) {
            log_error("Disk write on %s %d\n", dasd->file_name, r);
            return 1;
        }
        memset(dasd->cbuf, 0, tsize);
    }
    return 0;
}

int
dasd_attach(struct _dasd_t *dasd, char *file_name, int init)
{
    int                 i;
    struct dasd_header  hdr;
    uint32_t            tsize;
    off_t               isize;
    size_t              dsize;
    int                 r;
    uint8_t             *rec;
    int                 pos;

    /* Attempt to open disk */
    log_info("Attach %s %s\n", file_name, disk_type[dasd->type].name);
    if ((dasd->fd = open(file_name, O_RDWR, 0660)) < 0) {
        if (init) {
           /* If initialize valid, try and create it */
           if ((dasd->fd = open(file_name, O_RDWR|O_CREAT, 0660)) < 0) {
               return 0;
           }
        }
    }

    /* Save file name for later */
    dasd->file_name = strdup(file_name);

    /* Read in header if possible */
    log_trace("File %s %d\n", file_name, dasd->type);
    if (read(dasd->fd, &hdr, sizeof(struct dasd_header)) !=
          sizeof(struct dasd_header) || strncmp(&hdr.devid[0], "CKD_P370", 8) != 0 || init) {
        /* Not there or valid magic number, try and format it if allowed */
        if (dasd_format(dasd, init)) {
            dasd_detach(dasd);
            return -1;
        }
    }

    /* Read in header and try and find disk type */
    isize = lseek(dasd->fd, 0, SEEK_END);
    log_info("Drive %d %d %02x %02x %d %d\r\n",
             hdr.heads, hdr.tracksize, hdr.devtype, hdr.fileseq, hdr.highcyl, isize);
    for (i = 0; disk_type[i].name != 0; i++) {
         tsize = (disk_type[i].bpt | 0x1ff) + 1;
         dsize = sizeof(struct dasd_header) + (tsize * disk_type[i].heads * (disk_type[i].cyl + 1));
         if (hdr.devtype == disk_type[i].dev_type && hdr.tracksize == tsize &&
             hdr.heads == disk_type[i].heads && dsize == isize) {
             if (dasd->type != i) {
                  /* Ask if we should change */
                  log_warn("Wrong type %s\n", disk_type[i].name);
                  dasd->type = i;
             }
             break;
         }
    }

    /* Check if valid name */
    if (disk_type[i].name == 0) {
         dasd_detach(dasd);
         return -1;
    }

    /* Allocate cylinder buffer */
    tsize = hdr.tracksize * hdr.heads;
    dasd->tsize = hdr.tracksize;
    if (dasd->cbuf == NULL && (dasd->cbuf = (uint8_t *)calloc(tsize, sizeof(uint8_t))) == 0) {
        dasd_detach(dasd);
        return -1;
    }
    /* Read in first cylinder */
    (void)lseek(dasd->fd, sizeof(struct dasd_header), SEEK_SET);
    r = read(dasd->fd, dasd->cbuf, tsize);
    if (r != tsize) {
        log_error("Disk read on %s %d\n", dasd->file_name, r);
    }
    dasd->fpos = sizeof(struct dasd_header);
    dasd->status = ONLINE|READY;
    dasd->cyl = 0;
    /* Load in volume ID, from record 3 */
    pos = 5;
    rec = &dasd->cbuf[pos];
    pos += 8 + rec[5] + ((rec[6] << 8) | rec[7]);  /* Skip record 0 */
    rec = &dasd->cbuf[pos];
    pos += 8 + rec[5] + ((rec[6] << 8) | rec[7]);  /* Skip record 1 */
    rec = &dasd->cbuf[pos];
    pos += 8 + rec[5] + ((rec[6] << 8) | rec[7]);  /* Skip record 2 */
    rec = &dasd->cbuf[pos];                        /* Now points to record 3 */
    /* Check if correct length */
    if (rec[5] != 4 || rec[6] != 0 || rec[7] != 80)
        return 1;

    /* VOL1 Key and VOL1 label */
    for (i = 0; i < 8; i++) {
        if (rec[8 + i] != volrec[i])
            return 1;
    }
    for (i = 0; i < 8; i++) {
        dasd->vol_label[i] = ebcdic_to_ascii[rec[16 + i]];
    }
    dasd->vol_label[8] = '\0';   /* Make it a string */
    return 1;
}

void
dasd_detach(struct _dasd_t *dasd)
{
    int                 type = dasd->type;
    uint32_t            tsize = dasd->tsize * disk_type[type].heads;

    if (dasd->dirty) {
          (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
          (void)write(dasd->fd, dasd->cbuf, tsize);
          dasd->dirty = 0;
    }
    if (dasd->fd >= 0) {
        close(dasd->fd);
        dasd->fd = -1;
    }
    free(dasd->cbuf);
    dasd->cbuf = NULL;
    free(dasd->file_name);
    dasd->file_name = NULL;
    dasd->status = 0;
}



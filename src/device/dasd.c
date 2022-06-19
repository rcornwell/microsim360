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
    int                 rate;         /* Number of steps per disk byte */
}

disk_type[] =
{
       {"2303",  80,  10,  4984,  6,  0x03, 72, 36, 13},   /*   4.00 M */
       {"2311",  203, 10,  3717,  6,  0x11, 36, 18, 13},   /*   7.32 M  156k/s 30 ms 145 full */
       {"2314",  202, 20,  7294,  6,  0x14, 36, 18, 13},   /*  29.17 M */
       {NULL, 0}
};


/* Header block */
struct dasd_header
{
       char     devid[8];      /* device header. */
       uint32_t heads;         /* number of heads per cylinder */
       uint32_t tracksize;     /* size of track */
       uint8_t  devtype;       /* Hex code of last two digits of device type. */
       uint8_t  fileseq;       /* always 0. */
       uint16_t highcyl;       /* highest cylinder. */
       uint8_t  resv[492];     /* pad to 512 byte block */
};

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
 * Bit 7            Head adance     track 1         head 1      diff 1
 *                  FT0 & FT4
 */

static void
seek_callback(struct _device *unit, void *arg, int iarg)
{
    struct _dasd_t *dasd = (struct _dasd_t *)unit;
    log_disk("Seek done\n");
    dasd->attn = 1;
    dasd->cyl = dasd->ncyl;
}

void
dasd_settags(struct _dasd_t *dasd, uint8_t ft, uint8_t fc)
{
    if ((ft & BIT7) == 0)
        return;
    log_disk("tags  %02x %02x\n", ft, fc);
    if (ft & BIT0) {    /* Handle control function */
        if (fc & BIT1) { /* Read gate */
            dasd->attn = 0;
            log_disk("Clear attn\n");
            if ((fc & BIT5) != 0 && dasd->state == DK_POS_UNK) {
                dasd_update(dasd);
            }
            dasd->am_search = 0;
            if ((fc & (BIT5|BIT7)) == (BIT5|BIT7))
                dasd->am_search = 1;
        }
        if (fc & BIT2) { /* Start seek */
            log_disk("Start seek to %02x, diff = %d, dir=%d\n",
                    dasd->ncyl, dasd->diff, dasd->dir);
            add_event((struct _device *)dasd, seek_callback, 50,  NULL, 0);
        }
        if (fc & BIT3) {  /* Head reset */
            log_disk("Head reset\n");
        }
        if (fc & BIT5) { /* Select head */
            log_disk("Select head\n");
        }
    }
    if (ft & BIT1) {    /* Set cylinder */
        dasd->ncyl = fc;
        log_disk("set cyl  %02x\n", fc);
    }
    if (ft & BIT2) {    /* Set Head and sign */
        dasd->dir = (fc & BIT0) != 0;
        dasd->head = (fc & 0xf);
        log_disk("set diff %d head  %x\n", dasd->dir, dasd->head);
    }
    if (ft & BIT3) {    /* Set Difference */
        dasd->diff = fc;
        log_disk("set diff  %02x\n", fc);
    }
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

    count = tpos = rpos = 0;
log_disk("Update position %d\n", dasd->cpos);
    for (pos = 0; pos < dasd->cpos; pos ++) {
         rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
         da = &dasd->cbuf[tpos + dasd->tstart];
log_disk("State=%d %d\n", state, count);
         switch(state) {
         case DK_POS_INDEX:             /* At Index Mark */
              if (count > disk_type[type].g1) {
                  dasd->tstart = dasd->tsize * (dasd->head);
                  tpos = rpos = 0;
                  count = -1;
                  state = DK_POS_HA;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
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
                    state = DK_POS_GAP1;
                    count = -1;
              }
log_disk("HA Check=%02x %02x %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count]);
              break;

         case DK_POS_GAP1:
              if (count < disk_type[type].g1) {
                 break;
              }
              dasd->ck_sum[0] = 0xff;
              dasd->ck_sum[1] = 0xff;
              count = -1;
              state = DK_POS_CNT0;
log_disk("Gap1 %d\n", count);
              break;

         case DK_POS_CNT0:              /* In count (c) */
              switch(count) { 
              case 0:    /* Cyl high */
                      rpos = tpos;
                      rec = &dasd->cbuf[rpos + dasd->tstart];
                      /* Check for end of track */
                      if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                         state = DK_POS_END;
                      } else {
                         dasd->klen = rec[5];
                         dasd->dlen = (rec[6] << 8) | rec[7];
                      }
                      /* Fall through */
              case 1:   /* Cyl low */
              case 2:   /* Head high */
              case 3:   /* Head low */
              case 4:   /* Record ID */
              case 5:   /* Length of Key */
              case 6:   /* Length of data high */
              case 7:   /* Length of data low */
                      dasd->ck_sum[count & 1] ^= rec[count];
                      tpos++;
                      break;
              case 8:   /* Checksum 1 */
                      break;
              case 9:   /* Checksum 2 */
                      state = DK_POS_GAP2;
                      if (dasd->klen == 0)
                          state = DK_POS_GAP3;
                      count = -1;
                      break;
              }
log_disk("CNT0 Check=%02x %02x %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count]);
              break;


         case DK_POS_AM:                /* Beginning of record */
              if (count < (disk_type[type].g2 * 2))
                  break;
              count = -1;
              state = DK_POS_CNT1;
log_disk("AM %d\n", count);
              break;

         case DK_POS_CNT1:
              switch(count) { 
              case 0:    /* Cyl high */
                      rpos = tpos;
                      rec = &dasd->cbuf[rpos + dasd->tstart];
                      /* Check for end of track */
                      if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                         state = DK_POS_END;
                         count = 0;
                      } else {
                         dasd->klen = rec[5];
                         dasd->dlen = (rec[6] << 8) | rec[7];
                      }
                      /* Fall through */
              case 1:   /* Cyl low */
              case 2:   /* Head high */
              case 3:   /* Head low */
              case 4:   /* Record ID */
              case 5:   /* Length of Key */
              case 6:   /* Length of data high */
              case 7:   /* Length of data low */
                      dasd->ck_sum[count & 1] ^= rec[count];
                      tpos++;
                      break;
              case 8:   /* Checksum 1 */
                      break;
              case 9:   /* Checksum 2 */
                      state = DK_POS_GAP2;
                      if (dasd->klen == 0)
                          state = DK_POS_GAP3;
                      count = -1;
                      break;
              }
log_disk("CNT0 Check=%02x %02x %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], rec[count]);
              break;

         case DK_POS_GAP2:                /* Beginning of record */
              if (count < disk_type[type].g2) {
                 break;
              }
              count = -1;
              state = DK_POS_KEY;
              dasd->ck_sum[0] = 0xff;
              dasd->ck_sum[1] = 0xff;
              break;

         case DK_POS_KEY:               /* In Key area */
              if (count < dasd->klen) {
                  dasd->ck_sum[count & 1] ^= da[count];
                  tpos++;
              }
              if (count == (dasd->klen+2)) {
                  dasd->state = DK_POS_GAP3;
                  count = -1;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
              }
              break;

         case DK_POS_GAP3:                /* Beginning of record */
              if (count < disk_type[type].g2) {
                 break;
              }
              count = -1;
              state = DK_POS_DATA;
              break;

         case DK_POS_DATA:              /* In Data area */
              if (count < dasd->dlen) {
                  dasd->ck_sum[count & 1] ^= da[count];
                  tpos++;
              }
              if (count == (dasd->dlen + 2)) {
                  rpos += dasd->dlen + dasd->klen + 8;
                  tpos = rpos;
                  state = DK_POS_AM;
                  count = -1;
                  dasd->ck_sum[0] = 0xff;
                  dasd->ck_sum[1] = 0xff;
              }
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
    uint16_t            count = dasd->count;
    uint8_t             *rec;
    uint8_t             *da;
    size_t               pos;

log_disk("Disk step %d %d %d %s\n", dasd->step, dasd->cpos, dasd->tsize, dasd->file_name);
    if (dasd->step < disk_type[type].rate) {
        dasd->step++;
        return;
    }
    dasd->state = DK_POS_UNK;
    dasd->step = 0;

    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
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
log_disk("Disk read %d %d %d\n", dasd->state, count, dasd->cpos);
    dasd->step = 0;
    *am = 0;
    pos = (dasd->tsize * disk_type[type].heads) * dasd->cyl;
    /* Check if read or write command, if so grab correct cylinder */
    if (dasd->fpos != pos) {
        uint32_t tsize = dasd->tsize * disk_type[type].heads;
        size_t   fpos = sizeof(struct dasd_header) + pos;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              (void)write(dasd->fd, dasd->cbuf, tsize);
              dasd->dirty = 0;
        }
        dasd->fpos = fpos;
        log_info("Load cyl=%d %x\n", dasd->cyl, dasd->fpos);
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        (void)read(dasd->fd, dasd->cbuf, tsize);
        dasd->tstart = (dasd->tsize * dasd->head);
    }
    log_info("state %02x %d\n", dasd->state, dasd->tpos);

    if (dasd->cpos >= (disk_type[type].bpt + 1)) {
        log_info("state end %d\n", dasd->tpos);
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

    switch(dasd->state) {
    case DK_POS_INDEX:             /* At Index Mark */
         *data = gap0[count];
log_disk("Gap0=%02x %d\n", *data, count);
         if (*data == 0x6) {
             dasd->tstart = dasd->tsize * (dasd->head);
             dasd->tpos = dasd->rpos = 0;
             dasd->count = 0;
             dasd->state = DK_POS_HA;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
             dasd->am_search = 0;
         } else {
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
               *data = dasd->ck_sum[0];
               dasd->tpos = dasd->rpos = 5;
               dasd->state = DK_POS_GAP1;
               dasd->count = 0;
         }
log_disk("HA Check=%02x %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], dasd->tpos);
         break;

    case DK_POS_GAP1:
         *data = gap1[count];
         if (*data != 0x6) {
            return 0;
         }
         dasd->ck_sum[0] = 0xff;
         dasd->ck_sum[1] = 0xff;
         dasd->count = 0;
         dasd->state = DK_POS_CNT0;
         break;

    case DK_POS_CNT0:              /* In count (c) */
         switch(count) { 
         case 0:    /* Cyl high */
                 dasd->rpos = dasd->tpos;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
log_disk("CNT0 %02x %02x %02x %02x %02x %d %d\n", rec[0], rec[1], rec[2], rec[3], rec[4], dasd->klen, dasd->dlen);
                 /* Fall through */
         case 1:   /* Cyl low */
         case 2:   /* Head high */
         case 3:   /* Head low */
         case 4:   /* Record ID */
         case 5:   /* Length of Key */
         case 6:   /* Length of data high */
         case 7:   /* Length of data low */
                 *data = rec[count];
                 dasd->ck_sum[count & 1] ^= rec[count];
                 dasd->tpos++;
                 break;
         case 8:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
                 break;
         case 9:   /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 dasd->state = DK_POS_GAP2;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_GAP3;
                 dasd->count = 0;
                 break;
         }
log_disk("CNT0 Check=%02x %02x %d %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data, dasd->tpos);
         break;


    case DK_POS_AM:                /* Beginning of record */
         *data = gap2[count];
         if (*data == 0xaa) {
            *data = 0xff;
            *am = 1;
            dasd->am_search = 0;
            return 0;
         }
         if (*data != 0x6) {
            return 0;
         }
         dasd->ck_sum[0] = 0xff;
         dasd->ck_sum[1] = 0xff;
         dasd->count = 0;
         dasd->state = DK_POS_CNT1;
         break;

    case DK_POS_CNT1:
         switch(count) { 
         case 0:    /* Cyl high */
                 dasd->rpos = dasd->tpos;
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                    dasd->count = 0;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
log_disk("CNT1 %02x %02x %02x %02x %02x %d %d\n", rec[0], rec[1], rec[2], rec[3], rec[4], dasd->klen, dasd->dlen);
                 /* Fall through */
         case 1:   /* Cyl low */
         case 2:   /* Head high */
         case 3:   /* Head low */
         case 4:   /* Record ID */
         case 5:   /* Length of Key */
         case 6:   /* Length of data high */
         case 7:   /* Length of data low */
                 *data = rec[count];
                 dasd->ck_sum[count & 1] ^= rec[count];
                 dasd->tpos++;
                 break;
         case 8:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
                 break;
         case 9:   /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 dasd->state = DK_POS_GAP2;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_GAP3;
                 dasd->count = 0;
                 break;
         }
log_disk("CNT1 Check=%02x %02x %d %02x %d\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data, dasd->tpos);
         break;

    case DK_POS_GAP2:
         *data = gap1[count];
         if (*data != 0x6)
            return 0;
         dasd->ck_sum[0] = 0xff;
         dasd->ck_sum[1] = 0xff;
         dasd->count = 0;
         dasd->state = DK_POS_KEY;
         break;

    case DK_POS_KEY:               /* In Key area */
         if (count < dasd->klen) {
             *data = *da;
             dasd->ck_sum[count & 1] ^= *da;
             dasd->tpos++;
         } else if (count == dasd->klen) {
             *data = dasd->ck_sum[0];
         } else {
             *data = dasd->ck_sum[1];
             dasd->state = DK_POS_GAP2;
             dasd->count = 0;
         }
log_disk("KEY Check=%02x %02x %d %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], count, *data);
         break;

    case DK_POS_GAP3:
         *data = gap1[count];
         if (*data != 0x6)
            return 0;
         dasd->ck_sum[0] = 0xff;
         dasd->ck_sum[1] = 0xff;
         dasd->count = 0;
         dasd->state = DK_POS_DATA;
         break;


    case DK_POS_DATA:              /* In Data area */
         if (count < dasd->dlen) {
             *data = *da;
             dasd->ck_sum[count & 1] ^= *da;
             dasd->tpos++;
         } else if (count == dasd->dlen) {
             *data = dasd->ck_sum[0];
         } else {
             *data = dasd->ck_sum[1];
             dasd->state = DK_POS_AM;
             dasd->count = 0;
         }
log_disk("DATA Check=%02x %02x %d %d %02x\n", dasd->ck_sum[0], dasd->ck_sum[1], count, dasd->dlen, *data);
         break;

    case DK_POS_END:               /* Past end of data */
         dasd->count = 0;
         dasd->klen = 0;
         dasd->dlen = 0;
         break;
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

    if (dasd->step < disk_type[type].rate) {
        dasd->step++;
        return 0;
    }
    dasd->step = 0;
    pos = (dasd->tsize * disk_type[type].heads) * dasd->cyl;
    /* Check if read or write command, if so grab correct cylinder */
    if (dasd->fpos != pos) {
        uint32_t tsize = dasd->tsize * disk_type[type].heads;
        size_t   fpos = sizeof(struct dasd_header) + pos;
        if (dasd->dirty) {
              (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
              (void)write(dasd->fd, dasd->cbuf, tsize);
              dasd->dirty = 0;
        }
        log_info("Load cyl=%d %x\n", dasd->cyl, dasd->fpos);
        dasd->fpos = fpos;
        (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
        (void)read(dasd->fd, dasd->cbuf, tsize);
        dasd->tstart = (dasd->tsize * dasd->head);
    }
    log_info("state %02x %d\n", dasd->state, dasd->tpos);

    if (dasd->tpos >= dasd->tsize) {
        log_info("state end %d\n", dasd->tpos);
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
         if (count > disk_type[type].g1) {
             dasd->tstart = dasd->tsize * (dasd->head);
             dasd->tpos = dasd->rpos = 0;
             dasd->count = -1;
             dasd->state = DK_POS_HA;
             dasd->ck_sum[0] = 0xff;
             dasd->ck_sum[1] = 0xff;
         } else {
             *data = gap0[count];
         }
         break;
    
    case DK_POS_HA:                /* In home address (c) */
         dasd->tpos++;
         /* Return 5 bytes + 2 checksum */
         switch (count) {
         case 0:   /* Track flag */
         case 1:   /* Cyl High */
         case 2:   /* Cyl low */
         case 3:   /* Head high */
         case 4:   /* Head low */
               *data = rec[dasd->count];
               dasd->ck_sum[dasd->count & 1] ^= rec[dasd->count];
               break;
         case 5:   /* Checksum 1 */
               *data = dasd->ck_sum[0];
               break;
         case 6:   /* Checksum 2 */
               *data = dasd->ck_sum[0];
               dasd->tpos = 5;
               dasd->state = DK_POS_CNT0;
               dasd->count = -1;
               dasd->ck_sum[0] = 0xff;
               dasd->ck_sum[1] = 0xff;
         }
         break;
    case DK_POS_CNT0:              /* In count (c) */
         if (count < disk_type[type].g1) {
             *data = gap1[count];
             break;
         }
         dasd->tpos++;
         switch(count) { 
         case 0:    /* Cyl high */
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
                 /* Fall through */
         case 1:   /* Cyl low */
         case 2:   /* Head high */
         case 3:   /* Head low */
         case 4:   /* Record ID */
         case 5:   /* Length of Key */
         case 6:   /* Length of data high */
         case 7:   /* Length of data low */
                 *data = rec[dasd->count];
                 dasd->ck_sum[dasd->count & 1] ^= rec[dasd->count];
                 break;
         case 8:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
         case 9:   /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 dasd->state = DK_POS_KEY;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_DATA;
                 dasd->count = -1;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 break;
         }
         break;

    case DK_POS_AM:                /* Beginning of record */
         if (count < (disk_type[type].g2 * 2)) {
             *data = gap2[count];
             if (*data == 0xaa) {
                 *data = 0xff;
                 *am = 1;
             }
             break;
         }
         dasd->tpos++;
         switch(count) { 
         case 0:    /* Cyl high */
                 rec = &dasd->cbuf[dasd->rpos + dasd->tstart];
                 /* Check for end of track */
                 if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
                    dasd->state = DK_POS_END;
                    dasd->count = -1;
                 } else {
                    dasd->klen = rec[5];
                    dasd->dlen = (rec[6] << 8) | rec[7];
                 }
                 /* Fall through */
         case 1:   /* Cyl low */
         case 2:   /* Head high */
         case 3:   /* Head low */
         case 4:   /* Record ID */
         case 5:   /* Length of Key */
         case 6:   /* Length of data high */
         case 7:   /* Length of data low */
                 *data = rec[dasd->count];
                 dasd->ck_sum[dasd->count & 1] ^= rec[dasd->count];
                 break;
         case 8:   /* Checksum 1 */
                 *data = dasd->ck_sum[0];
         case 9:   /* Checksum 2 */
                 *data = dasd->ck_sum[1];
                 dasd->state = DK_POS_KEY;
                 if (dasd->klen == 0)
                     dasd->state = DK_POS_DATA;
                 dasd->count = -1;
                 dasd->ck_sum[0] = 0xff;
                 dasd->ck_sum[1] = 0xff;
                 break;
         }
         break;

    case DK_POS_KEY:               /* In Key area */
         if (count < disk_type[type].g2) {
             *data = gap1[count];
             break;
         }
         dasd->tpos++;
         if (dasd->count < dasd->klen) {
             *data = *da;
             dasd->ck_sum[dasd->count & 1] ^= *da;
         } else if (dasd->count == dasd->klen) {
             *data = dasd->ck_sum[0];
         } else {
             *data = dasd->ck_sum[1];
             dasd->state = DK_POS_DATA;
             dasd->count = -1;
         }
         break;

    case DK_POS_DATA:              /* In Data area */
         if (count < disk_type[type].g2) {
             *data = gap1[count];
             break;
         }
         dasd->tpos++;
         if (dasd->count < dasd->dlen) {
             *data = *da;
             dasd->ck_sum[dasd->count & 1] ^= *da;
         } else if (dasd->count == dasd->dlen) {
             *data = dasd->ck_sum[0];
         } else {
             *data = dasd->ck_sum[1];
             dasd->state = DK_POS_AM;
             dasd->count = -1;
         }
         break;

    case DK_POS_END:               /* Past end of data */
         dasd->tpos+=10;
         dasd->count = 0;
         dasd->klen = 0;
         dasd->dlen = 0;
         break;
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

//    if (flag || get_yn("Initialize dasd? [Y] ", TRUE)) {
    log_info("Format\n");
        memset(&hdr, 0, sizeof(struct dasd_header));
        memcpy(&hdr.devid[0], "CKD_P370", 8);
        hdr.heads = disk_type[type].heads;
        hdr.tracksize = (disk_type[type].bpt | 0x1ff) + 1;
        hdr.devtype = disk_type[type].dev_type;
        (void)lseek(dasd->fd, 0, SEEK_SET);
        write(dasd->fd, &hdr, sizeof(struct dasd_header));
        tsize = hdr.tracksize * hdr.heads;
        dasd->tsize = hdr.tracksize;
        if (dasd->cbuf == NULL && (dasd->cbuf = (uint8_t *)calloc(tsize, sizeof(uint8_t))) == 0)
            return 1;
        for (cyl = 0; cyl <= disk_type[type].cyl; cyl++) {
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
                    for (p = 0; p < sizeof (volrec); p++) 
                        dasd->cbuf[pos++] = volrec[p];
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
            write(dasd->fd, dasd->cbuf,tsize);
        }
        (void)lseek(dasd->fd, sizeof(struct dasd_header), SEEK_SET);
        (void)read(dasd->fd, dasd->cbuf, tsize);
        dasd->cpos = sizeof(struct dasd_header);
        dasd->cyl = 0;
        return 0;
}

int
dasd_attach(struct _dasd_t *dasd, char *file_name, int type, int init)
{
    uint32_t            i;
    struct dasd_header  hdr;
    uint32_t            tsize;
    off_t               isize;
    size_t              dsize;

    dasd->type = type;
    log_info("Attach %s %d\n", file_name, type);
    if ((dasd->fd = open(file_name, O_RDWR, 0660)) < 0) {
        if (init) {
           if ((dasd->fd = open(file_name, O_RDWR|O_CREAT, 0660)) < 0) {
               return 0;
           }
        }
    }
    dasd->file_name = strdup(file_name);

    log_info("File %s %d\n", file_name, type);
    if (read(dasd->fd, &hdr, sizeof(struct dasd_header)) !=
          sizeof(struct dasd_header) || strncmp(&hdr.devid[0], "CKD_P370", 8) != 0 || init) {
        if (dasd_format(dasd, init)) {
            dasd_detach(dasd);
            return -1;
        }
        return 1;
    }

    isize = lseek(dasd->fd, 0, SEEK_END);
    log_info("Drive %d %d %02x %02x %d\r\n",
             hdr.heads, hdr.tracksize, hdr.devtype, hdr.fileseq, hdr.highcyl);
    for (i = 0; disk_type[i].name != 0; i++) {
         tsize = (disk_type[i].bpt | 0x1ff) + 1;
         dsize = 512 + (tsize * disk_type[i].heads * (disk_type[i].cyl + 1));
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
    if (disk_type[i].name == 0) {
         dasd_detach(dasd);
         return -1;
    }
    if (dasd->cbuf == NULL && (dasd->cbuf = (uint8_t *)calloc(tsize, sizeof(uint8_t))) == 0) {
        dasd_detach(dasd);
        return -1;
    }
    tsize = hdr.tracksize * hdr.heads;
    dasd->tsize = hdr.tracksize;
    (void)lseek(dasd->fd, sizeof(struct dasd_header), SEEK_SET);
    (void)read(dasd->fd, dasd->cbuf, tsize);
    dasd->fpos = sizeof(struct dasd_header);
    dasd->cyl = 0;
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
    close(dasd->fd);
    dasd->fd = -1;
    free(dasd->cbuf);
    dasd->cbuf = NULL;
    free(dasd->file_name);
    dasd->file_name = NULL;
}



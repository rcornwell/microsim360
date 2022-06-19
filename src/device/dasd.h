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

#include <stdint.h>

struct _dasd_t
{
     char              *file_name;   /* File name */
     int                fd;          /* File pointer for image */
     int                type;        /* Type of disk */
     int                head;        /* Current head */
     uint8_t            tags;        /* Current file tags */
     uint8_t            diff;        /* Difference register */
     uint8_t            dir;         /* Direction */
     uint8_t            attn;        /* Drive attention status */
     uint8_t            flags;       /* Flags */
     uint8_t            am_search;   /* Searching for address mark */
     uint8_t           *cbuf;        /* Cylinder buffer */
     uint32_t           fpos;        /* Position of head of cylinder in file */
     uint32_t           tstart;      /* Location of start of track */
     uint16_t           ncyl;        /* New Cylinder */
     uint16_t           cyl;         /* Cylinder */
     uint16_t           tpos;        /* Virtual Track position */
     uint16_t           rpos;        /* Start of current record */
     uint16_t           cpos;        /* Current position around disk */
     uint16_t           count;       /* Current position in field */
     uint16_t           dlen;        /* remaining in data */
     uint32_t           tsize;       /* Size of one track include rounding */
     uint8_t            state;       /* Current state */
     uint8_t            dirty;       /* Track is dirty */
     uint8_t            klen;        /* remaining in key */
     uint8_t            ck_sum[2];   /* Record checksum */
     int                step;        /* Byte step count */
};

/**
 *
 *  Update the disk state to the current state and count.
 *  Used when swithing to a new drive.
 */

void dasd_settags(struct _dasd_t *dasd, uint8_t ft, uint8_t fc);

uint8_t dasd_cur_cyl(struct _dasd_t *dasd);

int dasd_check_attn(struct _dasd_t *dasd);

void dasd_update(struct _dasd_t *dasd);

void dasd_step(struct _dasd_t *dasd, uint8_t *ix);

int dasd_read_byte(struct _dasd_t *dasd, uint8_t *data, uint8_t *am, uint8_t *ix);

int dasd_write_byte(struct _dasd_t *dasd, uint8_t *data, uint8_t *am, uint8_t *ix);

int dasd_format(struct _dasd_t * dasd, int flag);

int dasd_attach(struct _dasd_t *dasd, char *file_name, int type, int init);

void dasd_detach(struct _dasd_t *dasd);


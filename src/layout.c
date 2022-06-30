/*
 * microsim360 - Layout peripheral devices on a screen.
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

#include <stdio.h>
#include <stdlib.h>
#include "device.h"

struct _perip {
    int       h;    /* Height of window */
    int       w;    /* Width of window */
    int       x;    /* X position of window */
    int       y;    /* Y position of window */
    int       unit;
    struct _device  *dev;
    struct _perip *next;
} *perph;


int
layout_periph(int *scr_wid, int *scr_hi)
{
    int      i;
    int      j;
    int      max_width;    /* Max width */
    int      max_height;   /* Max height */
    int      width;        /* Width of peripherals */
    int      height;       /* Height of peripherals */
    int      row_height;   /* Height of current row */
    int      screen_height;
    int      screen_width; /* Width of screen */
    int      rows;         /* Number of rows */
    int      waste;        /* Amount of wasted space */
    int      min_waste;    /* Min waste. */
    int      min_width;    /* Min width for min waste */
    struct _device *dev;   /* Point to device */
    struct _perip  *p, *q;     /* Pointer to current peripheral */

    p = NULL;
    max_width = 0;
    max_height = 0;
    width = 0;
    height = 0;
    /* Scan channels looking for windows */
    for (i = 0; i < 6; i++) {
        for (dev = chan[i]; dev != NULL; dev = dev->next) {
             for (j = 0; j < dev->n_units; j++) {
                  struct _perip  *np;
                  if (dev->rect[j].h == 0 || dev->rect[j].w == 0)
                      continue;
                  /* Save size of windows */
                  if ((np = (struct _perip *)calloc(1, sizeof(struct _perip))) == NULL)
                      return 0;
//printf("D=%03x %d %d\n", dev->addr, dev->rect[j].h, dev->rect[j].w);
                  np->h = dev->rect[j].h;
                  np->w = dev->rect[j].w;
                  np->dev = dev;
                  np->unit = j;
                  /* Compute max and total width */
                  height += np->h;
                  width += np->w;
                  if (max_height < np->h)
                      max_height = np->h;
                  if (max_width < np->w)
                      max_width = np->w;
                  if (perph == NULL) {
                      perph = np;
                  } else {
                      p->next = np;
                  }
                  p = np;
            }
       }
   }
//   printf("MW=%d MH=%d TW=%d TH=%d\n", max_width, max_height, width, height);
   min_waste = 10000;
   min_width = 1200;
   for (i = 500; i <= 1200; i += 100) {
       int   w = 0;
       waste = 0;
       rows = 1;
       row_height = 0;
       screen_height = 0;
       for (p = perph; p != NULL; p = p->next) {
           if ((p->w + w) > i) {
              waste += i - w;
              w = 0;
              screen_height += row_height;
              row_height = 0;
              rows++;
           }
           w += p->w;
           if (p->h > row_height)
               row_height = p->h;
       }
       screen_height += row_height;
       if (waste < min_waste && screen_height < 1000) {
          min_waste = waste;
          min_width = i;
       }
//       printf("W=%d H=%d rows=%d waste=%d min=%d\n", i, screen_height, rows, waste, min_width);
   }
   i = 0;
   j = 0;
   row_height = 0;
   for (p = perph; p != NULL; p = p->next) {
        if ((p->w + i) > min_width) {
            i = 0;
            j += row_height;
            row_height = 0;
        }
        p->dev->rect[p->unit].x = i;
        p->dev->rect[p->unit].y = j;
        if (p->h > row_height)
            row_height = p->h;
        i += p->w;
   }
   j += row_height;
   *scr_wid = min_width;
   *scr_hi = j;
   if (j > 1000)
      return 0;
   return 1;
}

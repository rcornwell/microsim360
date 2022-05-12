/*
 * microsim360 - Generic device control.
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
#include <stdint.h>
#include <string.h>
#include "logger.h"
#include "device.h"


char *bus_tags[] = {
    "SLO", "ADO", "CMD", "SRO", "SUP", "HLD", "OPO", NULL,
    "OPI", "ADI", "STI", "SVI", "RQI", NULL, NULL, NULL };


void
print_tags(char *name, int state, uint16_t tags, uint16_t bus_out)
{
    char   buffer[1024];
    int i;

    if ((tags & 0xf8ff) != 0) {
        sprintf(buffer, "%s state=%d Tags: bus=%03x %04x ", name, state, bus_out, tags);
        for (i = 0; i < 16; i++) {
            if (bus_tags[i] != NULL) {
              if ((tags & (0x8000 >> i)) != 0) {
                  strcat(buffer, bus_tags[i]);
                  strcat(buffer, " ");
              } else {
                  strcat(buffer, "    ");
              }
            }
        }
    strcat(buffer, "\n");
    log_device(buffer);
    }
}

void
add_chan(struct _device *dev, uint16_t addr)
{
      uint16_t    ch = (addr >> 8) & 0x7;
      struct   _device *d;

      dev->addr = addr;
      dev->next = NULL;
      /* If channel empty, add it */
      if (chan[ch] == NULL) {
          chan[ch] = dev;
          return;
      }
      /* Walk to end of list */
      for (d = chan[ch]; d->next != NULL; d = d->next);
      if (d != NULL) {
          d->next = dev;
      }
}

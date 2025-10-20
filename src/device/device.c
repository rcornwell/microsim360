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
#include <stdlib.h>
#include "logger.h"
#include "device.h"


static char *bus_tags[] = {
    "SLO", "ADO", "CMD", "SRO", "SUP", "HLD", "OPO", NULL,
    "OPI", "ADI", "STI", "SVI", "RQI", NULL, NULL, NULL };

static char *state_tags[] = {
    "IDLE", "BUSY", "INIT", "CMD", "STATUS", "ACCEPT", "WAIT",
    "END", "ENDACCEPT", "DEVEND", "OPR", "DATA1", "DATA2"};


struct _disk *disk = NULL;       /* Disk controllers */
struct _device *chan[6];         /* Channels */
uint32_t      *M;
uint32_t      mem_max;

char *title = NULL;

void (*setup_cpu)(void *rend) = NULL;

void (*step_cpu)() = NULL;

void
print_tags(char *name, int state, uint16_t tags, uint16_t bus_out)
{

    if ((tags & 0xf8ff) != 0) {
        char   buffer[1024];
        int i;

        sprintf(buffer, "%s state=%s Tags: bus=%03x %04x ", name,
                         state_tags[state], bus_out, tags);
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

struct _device *
find_chan(uint16_t addr, uint16_t mask)
{
      uint16_t    ch = (addr >> 8) & 0x7;
      struct      _device *d;

      for (d = chan[ch]; d != NULL; d = d->next) {
          if ((d->addr & mask) == (addr & mask))
              return d;
      }
      return NULL;
}

void
del_chan(struct _device *dev, uint16_t addr)
{
      uint16_t    ch = (addr >> 8) & 0x7;
      struct   _device *d;

      /* If channel empty, nothing to do. */
      if (chan[ch] == NULL) {
          return;
      }
      /* Check if at head */
      if (chan[ch] == dev) {
          chan[ch] = dev->next;
          return;
      }
      /* Walk to end of list */
      for (d = chan[ch]; d->next != NULL; d = d->next) {
           /* If match, then remove it */
           if (d->next == dev) {
               d->next = d->next->next;
               return;
           }
      }
}

/*
 * Add a disk to list of drives to run every cycle.
 */
void
add_disk(void (*fnc)(void *data), void *drive)
{
    struct _disk   *d;

    if ((d = (struct _disk *)calloc(1, sizeof(struct _disk))) == NULL) {
        log_warn("Unable to create disk structure\n");
        return;
    }
    d->step = fnc;
    d->disk = drive;
    d->next = disk;
    disk = d;
}

/*
 * Delete a disk from list of drives to run.
 */
void
del_disk(void *drive)
{
    struct _disk   *d, *p = NULL;

    if (disk == drive) {
       disk = disk->next;
       return;
    }

    for (d = disk; d != NULL; d = d->next) {
        if (d->disk == drive) {
            if (p == NULL) {
               disk = d->next;
            } else {
               p->next = d->next;
            }
            free(d);
            return;
        }
        p = d;
    }
}

/*
 * Step over disk controllers and run there step routine.
 */
void
step_disk()
{
    struct _disk  *d;

    for (d = disk; d != NULL; d = d->next)
        (d->step)(d->disk);
}


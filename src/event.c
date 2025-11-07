/*
 * microsim360 - Event scheduler
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

#include <stdlib.h>
#include "logger.h"
#include "event.h"

struct _event *event_head, *event_tail;

/* Initialize event system */
void
init_event()
{
    event_head = event_tail = NULL;
}

/* Add an event */
int
add_event(struct _device *dev, _callback func, int time, void *arg, int iarg)
{
    struct _event *new_event;
    struct _event *ptr_event;

    log_event("Add event %d: %x %d\n", time, arg, iarg);
    /* If event time is zero, generate callback immediately */
    if (time == 0) {
         (*func)(dev, arg, iarg);
         return 0;
    }

    if ((new_event = (struct _event *)malloc(sizeof(struct _event))) == NULL) {
        return 1;
    }

    new_event->next = NULL;
    new_event->prev = NULL;
    new_event->time = time;
    new_event->dev = dev;
    new_event->func = func;
    new_event->arg = arg;
    new_event->iarg = iarg;
    if (event_head == NULL) {
        event_head = event_tail = new_event;
        return 0;
    }
    /* List not empty, insert it */
    ptr_event = event_head;
    do {
        /* If event timeout is less, insert it in front */
        if (new_event->time <= ptr_event->time) {
            /* Subtract off event time from next event */
            ptr_event->time -= new_event->time;
            new_event->prev = ptr_event->prev;
            new_event->next = ptr_event;
            ptr_event->prev = new_event;
            if (new_event->prev != NULL) {
               /* Insert it */
                new_event->prev->next = new_event;
            } else {
               /* Make it new head */
               event_head = new_event;
            }
            /* All done */
            return 0;
        }
        /* Make new event relative to head of list */
        new_event->time -= ptr_event->time;
        ptr_event = ptr_event->next;
    } while (ptr_event != NULL);

    /* Get here, put it on tail of list */
    new_event->prev = event_tail;
    event_tail->next = new_event;
    event_tail = new_event;
    return 0;
}

/* Cancel event for device having callback func */
void
cancel_event(struct _device *dev, _callback func)
{
    struct _event *ptr_event;
    struct _event *nxt_event;

    log_event("Cancel event\n");
    ptr_event = event_head;
    if (ptr_event == NULL)   /* No events, just return */
        return;
    do {
        if (ptr_event->dev == dev && ptr_event->func == func) {
           nxt_event = ptr_event->next;
           /* If next event give time to next event */
           if (nxt_event != NULL) {
               nxt_event->time += ptr_event->time;
               /* point next events previous to current previous */
               nxt_event->prev = ptr_event->prev;
           } else {
               /* No next event, point event_tail to previous */
               event_tail = ptr_event->prev;
           }
           /* Point previous event next to next */
           if (ptr_event->prev != NULL) {
               ptr_event->next = ptr_event->next;
           } else {
               /* No previous, at head of list */
               event_head = ptr_event->next;
           }
           /* Event unlinked, can now free it */
           free(ptr_event);
           /* All done */
           return;
        }
        ptr_event = ptr_event->next;
    } while (ptr_event != NULL);
    /* Not found, just exit. */
    return;
}

/* Advance time by one clock cycle */
void
advance()
{
    struct _event *ptr_event;

    ptr_event = event_head;
    if (ptr_event == NULL)
        return;
    log_event("Advance event %d\n", ptr_event->time);
    ptr_event->time--;
    while (ptr_event != NULL && ptr_event->time == 0) {
         (*ptr_event->func)(ptr_event->dev, ptr_event->arg, ptr_event->iarg);
         event_head = ptr_event->next;
         free(ptr_event);
         ptr_event = event_head;
         if (ptr_event != NULL) {
             ptr_event->prev = NULL;
         } else {
             event_tail = NULL;
         }
    }
}



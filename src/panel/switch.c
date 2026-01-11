/*
 * microsim360 - GUI Draws toggle switch.
 *
 * Copyright 2023, Richard Cornwell
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

#include <stdint.h>
#include "switch.h"

struct _switch_t {
    uint32_t    *value;
    int          shift;
    int          type;
};

#define ON_OFF 0
#define MOMENTARY 1
#define THREE 2


static void
display_switch(Widget wid, SDL_Renderer *render)
{
   struct _switch_t *sw = (struct _switch_t *)wid->data;
   SDL_Rect     rect;
   SDL_Rect     rect2;
   uint32_t     bit = 0;

   if (sw->value != 0) {
       bit = (*sw->value) >> sw->shift;
   }
   switch (sw->type) {
   case THREE:
       switch (bit & 3) {
       case 0: bit = 1; break; /* Up */
       case 1: bit = 0; break; /* Middle */
       case 2: bit = 2; break; /* Down */
       }
       break;

   default:
       bit &= 1;
       bit ^= 1;
       break;
   }
   rect.x = bit * 15;
   rect.y = 0;
   rect.w = 15;
   rect.h = 32;
   rect2.x = wid->rect.x;
   rect2.y = wid->rect.y + ((wid->rect.w/2) - 7);
   rect2.w = 15;
   rect2.h = 32;
   SDL_RenderCopy(render, toggle_pic, &rect, &rect2);
}

static void
close_switch(Widget wid)
{
   free(wid->data);
}

static void
click_switch(Widget wid, int x, int y)
{
   struct _switch_t *sw = (struct _switch_t *)wid->data;
   uint32_t     bit = 0;

   if (sw->value != NULL) {
       switch (sw->type) {
       case ON_OFF:
            *sw->value ^= 1 << sw->shift;
            break;

       case MOMENTARY:
            *sw->value |= 1 << sw->shift;
            break;

       case THREE:
           bit = (*sw->value) >> sw->shift;
           if (y > (wid->rect.h / 2)) {  /* Bottom of window */
              if (bit == 0) {
                  bit = 1;
              } else {
                  bit = 2;
              }
           } else {   /* Top */
              if (bit == 2) {
                  bit = 1;
              } else {
                  bit = 0;
              }
           }
           *sw->value &= ~(3 << sw->shift);
           *sw->value |= bit << sw->shift;
          break;
      }
   }
}

static void
release_switch(Widget wid)
{
   struct _switch_t *sw = (struct _switch_t *)wid->data;

   if (sw->value != NULL) {
       switch (sw->type) {
       case THREE:
       case ON_OFF:
            break;

       case MOMENTARY:
            *sw->value &= ~(1 << sw->shift);
            break;
      }
   }
}

/*
 * Add switch.
 */
Widget
add_switch_on_off(Panel win, int x, int y, int w, uint32_t *value, int shift)
{
   Widget       nwid;
   struct _switch_t *l;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _switch_t *)calloc(1, sizeof(struct _switch_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   l->value = value;
   l->shift = shift;
   l->type = ON_OFF;
   /* Fill in the fields */
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = 32;

   nwid->draw = display_switch;
   nwid->close = close_switch;
   nwid->click = click_switch;
   nwid->release = release_switch;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}

/*
 * Add switch.
 */
Widget
add_switch_momentary(Panel win, int x, int y, int w, uint32_t *value, int shift)
{
   Widget       nwid;
   struct _switch_t *l;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _switch_t *)calloc(1, sizeof(struct _switch_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   l->value = value;
   l->shift = shift;
   l->type = MOMENTARY;
   /* Fill in the fields */
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = 32;

   nwid->draw = display_switch;
   nwid->close = close_switch;
   nwid->click = click_switch;
   nwid->release = release_switch;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}

/*
 * Add switch.
 */
Widget
add_switch_three(Panel win, int x, int y, int w, uint32_t *value, int shift)
{
   Widget       nwid;
   struct _switch_t *l;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _switch_t *)calloc(1, sizeof(struct _switch_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   l->value = value;
   l->shift = shift;
   l->type = THREE;
   /* Fill in the fields */
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = 32;

   nwid->draw = display_switch;
   nwid->close = close_switch;
   nwid->click = click_switch;
   nwid->release = release_switch;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}



/*
 * microsim360 - GUI Draw a roller control.
 *
 * Copyright 2025, Richard Cornwell
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


#include "roller.h"
#include <stdint.h>

struct _roller_t {
   SDL_Texture *rollers;            /* Image for rollers */
   SDL_Rect     rect;               /* Image outline */
   int          pos;                /* Current selected roller */
   int          *offsets;           /* Offsets to indicators. */
   _get_row     get_row;            /* Get the data for a row */
   int          color;              /* Color of indicators. */
   int          rows;               /* Number of rows in roller */
   int          positions;          /* Number of positions */
};

static void
display_roller(Widget wid, SDL_Renderer *render)
{
   struct _roller_t *rol = (struct _roller_t *)wid->data;
   SDL_Rect   rect;
   SDL_Rect   rect2;
   uint64_t   bits;
   int        i;
   int        mask;

   rect.x = wid->rect.x;
   rect.y = wid->rect.y;
   rect.w = rol->rect.w;
   rect.h = rol->rect.h;
   rect2.x = 0;
   rect2.y = rol->rect.y + (rol->pos * rol->rect.h);
   rect2.h = rol->rect.h;
   rect2.w = rol->rect.w;
   SDL_RenderCopy(render, rol->rollers, &rect2, &rect);
   rect.y += rol->rect.h + 10;
   rect.w = 15;
   rect.h = 15;
   rect2.x += rol->color * 15;
   rect2.y = 0;
   rect2.w = 15;
   rect2.h = 15;
   bits = (*rol->get_row)(rol->pos);
   mask = 1 << rol->positions;
   for (i = 0; i < rol->positions; i++) {
        rect.x += rol->offsets[i];
        if (LAMP_TEST || (bits & mask) != 0) {
            rect2.y = 15;
        } else {
            rect2.y = 0;
        }
        SDL_RenderCopy(render, lamps, &rect2, &rect);
        mask >>= 1;
   }
}

static void
click_roller(Widget wid, int x, int y)
{
   struct _roller_t *rol = (struct _roller_t *)wid->data;

   if (y > rol->rect.h) {
         rol->pos++;
         if (rol->pos >= rol->rows) {
             rol->pos = 0;
         }
   } else {
         rol->pos--;
         if (rol->pos <= 0) {
             rol->pos = rol->rows;
         }
   }
}

static void
close_roller(Widget wid)
{
    free(wid->data);
}

/*
 * Add a roller to window
 */
Widget
add_roller(Panel win, int x, int y, SDL_Rect *r_rect, SDL_Texture *rollers,
          int rows, _get_row get_row, int positions, int *offsets, int col)
{
   Widget nwid;
   struct _roller_t *rol;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((rol = (struct _roller_t *)calloc(1, sizeof(struct _roller_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   /* Fill in the feilds */
   rol->rollers = rollers;
   rol->rect.x = 0;
   rol->rect.y = r_rect->y;
   rol->rect.w = r_rect->w;
   rol->rect.h = r_rect->h;
   rol->pos = 0;
   rol->rows = rows;
   rol->get_row = get_row;
   rol->offsets = offsets;
   rol->color = col;
   rol->positions = positions;

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = r_rect->w;
   nwid->rect.h = (r_rect->h * 2) + 15;
   nwid->data = (void *)rol;
   nwid->draw = display_roller;
   nwid->click = click_roller;
   nwid->close = close_roller;
   add_widget(win, nwid);
   return nwid;
}

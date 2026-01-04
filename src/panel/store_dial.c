/*
 * microsim360 - GUI the Model 2030 storage control dial
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

#include "store_dial.h"

    
static void
display_store_dial(Widget wid, SDL_Renderer *render)
{
   SDL_Rect  rect;
   uint8_t   value = *((uint8_t *)wid->data);
   int       x, y;

   rect.x = ((int)value & 3) * 81;
   rect.y = (((int)value & 0xc) >> 2)  * 81;
   rect.h = 81;
   rect.w = 81;
   x = wid->rect.x + 40;
   y = wid->rect.y + 40;
   SDL_RenderCopy(render, store_dials, &rect, &wid->rect);
   switch ((value & 0x30) >> 4) {
   case 0:
   case 1:
           SDL_SetRenderDrawColor( render, cb.r, cb.g, cb.b, 0xff);
           SDL_RenderDrawLine( render, x, y, x - 5, y - 5);
           SDL_RenderDrawLine( render, x-1, y, x - 6, y - 5);
           break;
   case 3:
           SDL_SetRenderDrawColor( render, c5.r, c5.g, c5.b, 0xff);
           SDL_RenderDrawLine( render, x, y, x, y - 9);
           SDL_RenderDrawLine( render, x-1, y, x-1, y - 9);
           break;
   case 2:
           SDL_SetRenderDrawColor( render, c1.r, c1.g, c1.b, 0xff);
           SDL_RenderDrawLine( render, x, y, x + 5, y - 5);
           SDL_RenderDrawLine( render, x+1, y, x + 6, y - 5);
           break;
   }

}

static void
click_store_dial(Widget wid, int x, int y)
{
   uint8_t   *value = ((uint8_t *)wid->data);
   uint8_t    v = *value & 0xf;

   printf("x = %d y = %d\n", x, y);
   if (x > 30 && x < 50 && y > 30 && y < 50) {
       switch((*value & 0x30) >> 4) {
       case 0:
       case 1:
                *value = 0x20 | v;
                break;
       case 2:
                *value = 0x30 | v;
                break;
       case 3:
                *value = 0x10 | v;
                break;
       }
       return;
   }
               
   if (x > 40) {
       v = (v - 1) & 0xf;
   } else {
       v = (v + 1) & 0xf;
   }

   *value = (*value & 0xc0) | v;
}


/*
 * Add hex dial.
 */
Widget
add_store_dial(Panel win, int x, int y, uint8_t *value)
{
   Widget       nwid;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }


   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = 80;
   nwid->rect.h = 80;
   nwid->data = (void *)value;
   nwid->draw = display_store_dial;
   nwid->click = click_store_dial;
   add_widget(win, nwid);
   return nwid;
}



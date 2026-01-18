/*
 * microsim360 - GUI a hex dial.
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

#include "hex_dial.h"


static char *hex_digit[] = {
   "0", "1", "2", "3", "4", "5", "6", "7",
   "8", "9", "A", "B", "C", "D", "E", "F"
};

static void
display_hex_dial(Widget wid, SDL_Renderer *render)
{
   SDL_Surface *surf;
   SDL_Texture *text;
   int          hx, wx;
   SDL_Rect  rect;
   uint8_t   value = *((uint8_t *)wid->data);

   rect.x = ((int)value & 3) * 64;
   rect.y = (((int)value & 0xc) >> 2)  * 64;
   rect.h = 64;
   rect.w = 64;
   SDL_RenderCopy(render, hex_dials, &rect, &wid->rect);
   /* Overlay current value */
   surf = TTF_RenderText_Blended(font1, hex_digit[value], c_white);
   text = SDL_CreateTextureFromSurface(render, surf);
   SDL_QueryTexture(text, NULL, NULL, &wx, &hx);
   SDL_FreeSurface(surf);
   rect.x = wid->rect.x + 29;
   rect.y = wid->rect.y + 2;
   rect.h = hx;
   rect.w = wx;
   SDL_RenderCopy(render, text, NULL, &rect);

}

static void
click_hex_dial(Widget wid, int x, int y)
{
   uint8_t   *value = ((uint8_t *)wid->data);

   if (x > 32) {
       *value = (*value - 1) & 0xf;
   } else {
       *value = (*value + 1) & 0xf;
   }
}

/*
 * Add hex dial.
 */
Widget
add_hex_dial(Panel win, int x, int y, uint8_t *value)
{
   Widget       nwid;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = 64;
   nwid->rect.h = 64;
   nwid->data = (void *)value;
   nwid->draw = display_hex_dial;
   nwid->click = click_hex_dial;
   add_widget(win, nwid);
   return nwid;
}



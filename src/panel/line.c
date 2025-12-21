/*
 * microsim360 - GUI controls fill in an area.
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


#include "area.h"

static void
display_line(Widget wid, SDL_Renderer *render)
{
   SDL_SetRenderDrawColor( render, wid->fore_color->r, wid->fore_color->g, wid->fore_color->b, 0xff);
   SDL_RenderDrawLine( render, wid->rect.x, wid->rect.y, wid->rect.w, wid->rect.h);
}

/*
 * Add a horizontal line to a window.
 */
Widget
add_line(Panel win, int x, int y, int w, SDL_Color *col)
{
   Widget nwid;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   /* Fill in the feilds */
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = x+w;
   nwid->rect.h = y;
   nwid->fore_color = col;
   nwid->draw = display_line;
   add_widget(win, nwid);
   return nwid;
}

/*
 * Add a vertical line to a window.
 */
Widget
add_mark(Panel win, int x, int y, int h, SDL_Color *col)
{
   Widget nwid;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   /* Fill in the feilds */
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = x;
   nwid->rect.h = y+h;
   nwid->fore_color = col;
   nwid->draw = display_line;
   add_widget(win, nwid);
   return nwid;
}



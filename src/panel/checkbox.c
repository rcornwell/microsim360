/*
 * microsim360 - GUI Draws a checkbox next to a label.
 *
 * Copyright 2026, Richard Cornwell
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

#include "checkbox.h"
#include "button.h"

struct _checkbox_t {
    SDL_Rect     rect;
    SDL_Texture *label;
    SDL_Color    color;
    int          type;
    int         *value;
    int          shft;
    int          right;
    _button_callback func;
    void        *arg;
    int         iarg;
};


static void
display_checkbox(Widget wid, SDL_Renderer *render)
{
   struct _checkbox_t *box = (struct _checkbox_t *)wid->data;
//   int     x, y, h, w;
   SDL_Rect rect;
   int     bit = 0;

   SDL_SetRenderDrawColor(render, wid->back_color->r, wid->back_color->g, wid->back_color->b, 0xff);
   SDL_RenderFillRect(render, &wid->rect);
   if (box->label != NULL) {
       SDL_RenderCopy(render, box->label, NULL, &box->rect);
   }

   if (box->value != 0) {
       bit = (*box->value >> box->shft) & 1;
   }

   if (box->right) {
      rect.x = wid->rect.x + wid->rect.w - wid->rect.h;
   }
   rect.y = wid->rect.y;
   rect.h = wid->rect.h;
   rect.w = wid->rect.h;
   SDL_SetRenderDrawColor( render, 0x0, 0x0, 0x0, 0xff);
   if (bit) {
       /* Top black */
       SDL_RenderFillRect(render, &rect);
   } else {
       SDL_RenderDrawRect(render, &rect);
   }
}

static void
close_checkbox(Widget wid)
{
   struct _checkbox_t *box = (struct _checkbox_t *)wid->data;
   SDL_DestroyTexture(box->label);
   free(wid->data);
}

static void
click_checkbox(Widget wid, int x, int y)
{
   struct _checkbox_t *box = (struct _checkbox_t *)wid->data;

   if (box->value != NULL) {
       *box->value ^= 1 << box->shft;
   }
}

/*
 * Add a checkbox.
 */
Widget
add_checkbox(Panel win, int x, int y, int h, int w, char *label,
                   int *value, int shft, int right, TTF_Font *font, SDL_Color *f_col, SDL_Color *b_col)
{
   Widget       nwid;
   struct _checkbox_t *box;
   int          hh, wh;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((box = (struct _checkbox_t *)calloc(1, sizeof(struct _checkbox_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->back_color = b_col;
   box->value = value;
   box->shft = shft;
   box->right = right;
   if (label != NULL) {
      SDL_Surface *surf;

      surf = TTF_RenderText_Blended(font, label, *f_col);
      box->label = SDL_CreateTextureFromSurface(win->render, surf);
      SDL_QueryTexture(box->label, NULL, NULL, &wh, &hh);
      SDL_FreeSurface(surf);
      box->rect.x = x;
      box->rect.y = y;
      box->rect.w = wh;
      box->rect.h = hh;

      /* Slide label over to give room to check box */
      if (!right) {
         box->rect.x += 2*h;
      }
   }

   nwid->draw = display_checkbox;
   nwid->close = close_checkbox;
   nwid->click = click_checkbox;
   nwid->data = (void *)box;
   add_widget(win, nwid);
   return nwid;
}

/*
 * microsim360 - GUI Draws lamp indicator, with optional label above.
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

#include "lamp.h"

struct _lamp_t {
    SDL_Rect     rect_label;
    SDL_Rect     lamp;
    SDL_Texture *label;
    int          color;
    uint16_t    *value;
};


static void
display_lamp(Widget wid, SDL_Renderer *render)
{
   struct _lamp_t *l = (struct _lamp_t *)wid->data;
   uint16_t       bit = 0;
   SDL_Rect       rect;

   if (l->value != 0) {
       bit = *l->value;
   }

   rect.x = l->color * 15;
   rect.y = 0;
   rect.w = 15;
   rect.h = 15;
   if (LAMP_TEST || bit) {
       rect.y = 15;
   }
   SDL_RenderCopy(render, lamps, &rect, &l->lamp);

   if (l->label) {
      SDL_RenderCopy( render, l->label, NULL, &l->rect_label);
   }
}

static void
close_lamp(Widget wid)
{
   struct _lamp_t *l = (struct _lamp_t *)wid->data;
   if (l->label != NULL) {
       SDL_DestroyTexture(l->label);
   }
   free(wid->data);
}

/*
 * Add text.
 */
Widget
add_lamp(Panel win, int x, int y, char *label1,
                   uint16_t *value, TTF_Font *font,
                   int color, SDL_Color *t_col)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _lamp_t *l;
   int          hh, wh;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _lamp_t *)calloc(1, sizeof(struct _lamp_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   l->value = value;
   l->color = color;
   l->lamp.x = x;
   l->lamp.y = y;
   l->lamp.h = 15;
   l->lamp.w = 15;

   if (label1 != NULL) {
       surf = TTF_RenderText_Blended(font, label1, *t_col);
       text = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(text, NULL, NULL, &wh, &hh);
       SDL_FreeSurface(surf);
       l->label = text;
       l->rect_label.x = x + 7 - (wh/2);
       l->rect_label.y = y - hh;
       l->rect_label.w = wh;
       l->rect_label.h = hh;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = 15;
   nwid->rect.h = 15;

   nwid->draw = display_lamp;
   nwid->close = close_lamp;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}


/*
 * microsim360 - GUI Draws an indicator with label.
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

#include "light.h"

struct _light_t {
    indicator    ind;
    SDL_Rect     recth;
    SDL_Rect     rectl;
    SDL_Texture *digith_on;
    SDL_Texture *digitl_on;
    SDL_Texture *digith_off;
    SDL_Texture *digitl_off;
};

    
static void
display_label(Widget wid, SDL_Renderer *render)
{
   struct _light_t *l = (struct _light_t *)wid->data;
   uint32_t   bit;

   if (l->ind.type < 0) {
      bit = (*l->ind.value.v16 & (1 << l->ind.shift)) != 0;
   } else if (l->ind.type > 0) {
      bit = (*l->ind.value.v8 & (1 << l->ind.shift)) != 0;
   } else {
      bit = (*l->ind.value.v32 & (1 << l->ind.shift)) != 0;
   }
   if (LAMP_TEST || bit) {
      SDL_RenderCopy(render, l->digith_on, NULL, &l->recth);
      SDL_RenderCopy(render, l->digitl_on, NULL, &l->rectl);
   } else {
      SDL_RenderCopy(render, l->digith_off, NULL, &l->recth);
      SDL_RenderCopy(render, l->digitl_off, NULL, &l->rectl);
   }
}

/*
 * Add text.
 */
Widget
add_light(Panel win, int x, int y, char *label1, char *label2, uint16_t *var,
                      int shift, TTF_Font *font, SDL_Color *con, SDL_Color *coff,
                      SDL_Color *cb)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _light_t *l;
   int          hh, wh, hl, wl, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _light_t *)calloc(1, sizeof(struct _light_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   l->ind.value.v16 = var;
   l->ind.shift = shift;
   l->ind.type = U16;
   /* Fill in the fields */
   surf = TTF_RenderText_Shaded(font, label1, *con, *cb);
   text = SDL_CreateTextureFromSurface(win->render, surf); 
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->digith_on = text;
   surf = TTF_RenderText_Shaded(font, label1, *coff, *cb);
   text = SDL_CreateTextureFromSurface(win->render, surf); 
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->digith_off = text;
   l->recth.x = x;
   l->recth.y = y;
   l->recth.h = hh;
   l->recth.w = wh;
   if (label2 != NULL) {
       surf = TTF_RenderText_Shaded(font, label2, *con, *cb);
       text = SDL_CreateTextureFromSurface(win->render, surf); 
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       l->digitl_on = text;
       surf = TTF_RenderText_Shaded(font, label2, *coff, *cb);
       text = SDL_CreateTextureFromSurface(win->render, surf); 
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       l->digitl_off = text;
       l->rectl.x = x;
       l->rectl.y = y + (hh/2);
       l->rectl.h = hl;
       l->recth.y -= (hh/2);
       if (wl > wh) {
          l->rectl.w = wl;
          l->recth.w = wl;
       } else {
          l->rectl.w = wh;
       }
   } else {
       l->digitl_on = NULL;
       l->digitl_off = NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = l->recth.w;
   nwid->rect.h = l->recth.h+l->rectl.h;

   nwid->back_color = cb;
   nwid->draw = display_label;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}


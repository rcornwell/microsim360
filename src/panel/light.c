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
    SDL_Rect     recth;
    SDL_Rect     rectl;
    SDL_Texture *digith_on;
    SDL_Texture *digitl_on;
    SDL_Texture *digith_off;
    SDL_Texture *digitl_off;
    uint16_t    *value;
    int          shift;
};

    
static void
display_light(Widget wid, SDL_Renderer *render)
{
   struct _light_t *l = (struct _light_t *)wid->data;
   int     bit = 0;

   if (l->value != 0) {
       bit = (*l->value >> l->shift) & 1;
   }
   if (LAMP_TEST || bit) {
      SDL_RenderCopy(render, l->digith_on, NULL, &l->recth);
      SDL_RenderCopy(render, l->digitl_on, NULL, &l->rectl);
   } else {
      SDL_RenderCopy(render, l->digith_off, NULL, &l->recth);
      SDL_RenderCopy(render, l->digitl_off, NULL, &l->rectl);
   }
}

static void
close_light(Widget wid)
{
   struct _light_t *l = (struct _light_t *)wid->data;
   SDL_DestroyTexture(l->digith_on);
   SDL_DestroyTexture(l->digith_off);
   if (l->digitl_off != NULL) {
       SDL_DestroyTexture(l->digitl_on);
       SDL_DestroyTexture(l->digitl_off);
   }
   free(wid->data);
}

/*
 * Add text.
 */
Widget
add_light(Panel win, int x, int y, char *label1, char *label2,
                   uint16_t *value, int shift, TTF_Font *font,
                   SDL_Color *con, SDL_Color *coff)
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

   l->value = value;
   l->shift = shift;
   l->recth.x = x;
   l->recth.y = y;
   /* Fill in the fields */
   surf = TTF_RenderText_Blended(font, label1, *con);
   text = SDL_CreateTextureFromSurface(win->render, surf); 
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->digith_on = text;
   surf = TTF_RenderText_Blended(font, label1, *coff);
   text = SDL_CreateTextureFromSurface(win->render, surf); 
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->digith_off = text;
   l->recth.h = hh;
   l->recth.w = wh;
   l->rectl.x = x;
   l->rectl.x = y;
   if (label2 != NULL) {
       surf = TTF_RenderText_Blended(font, label2, *con);
       text = SDL_CreateTextureFromSurface(win->render, surf); 
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       l->digitl_on = text;
       surf = TTF_RenderText_Blended(font, label2, *coff);
       text = SDL_CreateTextureFromSurface(win->render, surf); 
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       l->digitl_off = text;
       l->rectl.x = x;
       l->rectl.y = y + (hh/2) - 2;
       l->rectl.h = hl;
       l->rectl.w = wl;
       if (wl > wh) {
          l->recth.x += (wl - wh) / 2;
       } else {
          l->rectl.x += (wh - wl) / 2;
       }
       l->recth.y -= (hh/2) + 2;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = l->recth.w;
   nwid->rect.h = l->recth.h+l->rectl.h;

   nwid->draw = display_light;
   nwid->close = close_light;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}


/*
 * microsim360 - GUI Draws a push button button
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

#include "button.h"

struct _button_t {
    SDL_Rect     recth;
    SDL_Rect     rectl;
    SDL_Texture *upper;
    SDL_Texture *lower;
    SDL_Color    color;
    int          type;
    int          active;
    int         *value;
    int          turn_off;
};

    
static void
display_button(Widget wid, SDL_Renderer *render)
{
   struct _button_t *sw = (struct _button_t *)wid->data;
   int     x, y, h, w;

   SDL_SetRenderDrawColor(render, wid->back_color->r, wid->back_color->g, wid->back_color->b, 0xff);
   SDL_RenderFillRect(render, &wid->rect);
   SDL_RenderCopy(render, sw->upper, NULL, &sw->recth);
   if (sw->lower != NULL) {
       SDL_RenderCopy(render, sw->lower, NULL, &sw->rectl);
   }

   x = wid->rect.x;
   y = wid->rect.y;
   h = wid->rect.h + 1;
   w = wid->rect.w + 1;
   if (sw->active) {
       /* Top black */
       SDL_SetRenderDrawColor( render, 0x0, 0x0, 0x0, 0xff);
       SDL_RenderDrawLine( render, x, y, x + w, y);
       SDL_RenderDrawLine( render, x, y, x, y + h);
       /* Bottom white */
       SDL_SetRenderDrawColor( render, 0xff, 0xff, 0xff, 0xff);
       SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
       SDL_RenderDrawLine( render, x + w, y, x + w, y + h);
   } else {
       /* Top white */
       SDL_SetRenderDrawColor( render, 0xff, 0xff, 0xff, 0xff);
       SDL_RenderDrawLine( render, x, y, x + w, y);
       SDL_RenderDrawLine( render, x, y, x, y + h);
       /* Botton black */
       SDL_SetRenderDrawColor( render, 0x0, 0x0, 0x0, 0xff);
       SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
       SDL_RenderDrawLine( render, x + w, y, x + w, y + h);
   }
}

static void
display_blank(Widget wid, SDL_Renderer *render)
{
   int     x, y, h, w;

   SDL_SetRenderDrawColor(render, wid->back_color->r, wid->back_color->g, wid->back_color->b, 0xff);
   SDL_RenderFillRect(render, &wid->rect);

   x = wid->rect.x;
   y = wid->rect.y;
   h = wid->rect.h + 1;
   w = wid->rect.w + 1;
   /* Top white */
   SDL_SetRenderDrawColor( render, 0xff, 0xff, 0xff, 0xff);
   SDL_RenderDrawLine( render, x, y, x + w, y);
   SDL_RenderDrawLine( render, x, y, x, y + h);
   /* Botton black */
   SDL_SetRenderDrawColor( render, 0x0, 0x0, 0x0, 0xff);
   SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
   SDL_RenderDrawLine( render, x + w, y, x + w, y + h);
}


static void
close_button(Widget wid)
{
   struct _button_t *sw = (struct _button_t *)wid->data;
   SDL_DestroyTexture(sw->upper);
   if (sw->lower != NULL) {
       SDL_DestroyTexture(sw->lower);
   }
   free(wid->data);
}

static void
click_button(Widget wid, int x, int y)
{
   struct _button_t *sw = (struct _button_t *)wid->data;

   sw->active = 1;
   if (sw->value != NULL) {
       *sw->value = !*sw->value;
   }
}

static void
release_button(Widget wid)
{
   struct _button_t *sw = (struct _button_t *)wid->data;

   sw->active = 0;
   if (sw->value != NULL && sw->turn_off) {
       *sw->value = 0;
   }
}

/*
 * Add button.
 */
Widget
add_button(Panel win, int x, int y, int h, int w, char *label1, char *label2,
                   int *value, TTF_Font *font, SDL_Color *f_col, SDL_Color *b_col,
                   int turn_off)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _button_t *l;
   int          hh, wh, hl, wl, lh, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _button_t *)calloc(1, sizeof(struct _button_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->back_color = b_col;
   l->value = value;
   l->turn_off = turn_off;
   /* Fill in the fields */
   surf = TTF_RenderText_Blended(font, label1, *f_col);
   text = SDL_CreateTextureFromSurface(win->render, surf); 
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->upper = text;
   l->recth.x = x;
   l->recth.y = y;
   l->recth.w = wh;
   l->recth.h = hh;
   lh = hh;
   if (label2 != NULL) {
       surf = TTF_RenderText_Blended(font, label2, *f_col);
       text = SDL_CreateTextureFromSurface(win->render, surf); 
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       l->lower = text;
       l->rectl.x = x;
       l->rectl.y = y + (hh/2);
       l->rectl.w = wl;
       l->rectl.h = hl;
       if (wl > wh) {
          l->recth.x += (wl - wh) / 2;
       } else {
          l->rectl.x += (wh - wl) / 2;
       }
       l->recth.y = y - (hh/2);
       lh += hl;
       /* Center label */
       l->rectl.x += (w/2) - (wl/2);
       l->rectl.y += (h/2) - (hl/2);
   }

   /* Center label */
   l->recth.x += (w/2) - (wh/2);
   l->recth.y += (h/2) - (hh/2);

   nwid->draw = display_button;
   nwid->close = close_button;
   nwid->click = click_button;
   nwid->release = release_button;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}

/*
 * Add blank button.
 */
Widget
add_blank(Panel win, int x, int y, int h, int w, SDL_Color *b_col)
{
   Widget       nwid;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->back_color = b_col;

   nwid->draw = display_blank;
   add_widget(win, nwid);
   return nwid;
}



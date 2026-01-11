/*
 * microsim360 - GUI Draws interval timer control on Model 30.
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

#include "timer.h"

struct _timer_t {
    SDL_Rect     rect_on;
    SDL_Rect     rect_off;
    SDL_Rect     rect_label;
    SDL_Texture *label;
    SDL_Texture *on;
    SDL_Texture *off;
    SDL_Color    color;
    int          type;
    int          *value;
};


static void
display_timer(Widget wid, SDL_Renderer *render)
{
   struct _timer_t *sw = (struct _timer_t *)wid->data;
   int        status = 0;
   SDL_Rect   rect;

   if (sw->value != NULL) {
      status = *sw->value;
   }

   rect.x = wid->rect.x;
   rect.y = wid->rect.y;
   rect.w = wid->rect.w;
   rect.h = wid->rect.h;

   /* Draw background */
   SDL_SetRenderDrawColor(render, wid->back_color->r, wid->back_color->g, wid->back_color->b, 0xff);
   SDL_RenderFillRect(render, &rect);
   rect.h = wid->rect.h/2;
   SDL_SetRenderDrawColor(render, 0xff, 0xff, 0xff, 0xff);
   if (status) {
       /* Fill in ON label */
       rect.y += wid->rect.h/2;
       SDL_RenderFillRect(render, &rect);
       SDL_RenderCopy(render, sw->on, NULL, &sw->rect_on);
       /* Draw label */
       rect.x = sw->rect_label.x;
       rect.y = sw->rect_label.y;
       rect.w = sw->rect_label.w;
       rect.h = sw->rect_label.h;
       SDL_RenderCopy(render, sw->label, NULL, &rect);
   } else {
       SDL_RenderFillRect(render, &rect);
       /* Fill in OFF label */
       SDL_RenderCopy(render, sw->off, NULL, &sw->rect_off);
       /* Draw label */
       rect.x = sw->rect_label.x;
       rect.y = sw->rect_label.y + wid->rect.h/2;
       rect.w = sw->rect_label.w;
       rect.h = sw->rect_label.h;
       SDL_RenderCopy(render, sw->label, NULL, &rect);
   }
}

static void
close_timer(Widget wid)
{
   struct _timer_t *sw = (struct _timer_t *)wid->data;
   SDL_DestroyTexture(sw->label);
   SDL_DestroyTexture(sw->on);
   SDL_DestroyTexture(sw->off);
   free(wid->data);
}

static void
click_timer(Widget wid, int x, int y)
{
   struct _timer_t *sw = (struct _timer_t *)wid->data;

   if (sw->value != NULL) {
       *sw->value = !*sw->value;
   }
}

/*
 * Add timer.
 */

Widget
add_timer(Panel win, int x, int y, int h, int w, char *label1, int *value,
              TTF_Font *font, SDL_Color *f_col, SDL_Color *b_col)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _timer_t *l;
   int          hh, wh, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _timer_t *)calloc(1, sizeof(struct _timer_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->back_color = b_col;
   l->value = value;
   /* Fill in the fields */
   surf = TTF_RenderText_Blended(font, label1, *f_col);
   text = SDL_CreateTextureFromSurface(win->render, surf);
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->label = text;
   /* Center label */
   l->rect_label.x = x + (w/2) - (wh/2);
   l->rect_label.y = y;
   l->rect_label.w = wh;
   l->rect_label.h = hh;

   surf = TTF_RenderText_Blended(font, "ON", c1);
   text = SDL_CreateTextureFromSurface(win->render, surf);
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->on = text;
   l->rect_on.x = x;
   l->rect_on.y = y + (h/2);
   l->rect_on.w = wh;
   l->rect_on.h = hh;

   surf = TTF_RenderText_Blended(font, "OFF", c1);
   text = SDL_CreateTextureFromSurface(win->render, surf);
   SDL_QueryTexture(text, &f, &k, &wh, &hh);
   SDL_FreeSurface(surf);
   l->off = text;
   l->rect_off.x = x + w - wh;
   l->rect_off.y = y;
   l->rect_off.w = wh;
   l->rect_off.h = hh;

   nwid->draw = display_timer;
   nwid->close = close_timer;
   nwid->click = click_timer;
   nwid->data = (void *)l;
   add_widget(win, nwid);
   return nwid;
}



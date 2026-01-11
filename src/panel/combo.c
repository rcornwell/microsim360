
/*
 * microsim360 - GUI Draws selector box.
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

#include "combo.h"

struct _combo_t {
    SDL_Rect     urect;
    SDL_Rect     drect;
    SDL_Texture *label[8];
    int          lw[8];
    int          lh[8];
    int         *value;
    int          num;
    int          max;
};


static void
display_combo(Widget wid, SDL_Renderer *render)
{
   struct _combo_t *box = (struct _combo_t *)wid->data;
   SDL_Rect    rect;
   int     x, y, h, w;

   SDL_SetRenderDrawColor( render, wid->back_color->r, wid->back_color->g, wid->back_color->b, 0xff);
   SDL_RenderFillRect( render, &wid->rect);
   SDL_SetRenderDrawColor( render, wid->fore_color->r, wid->fore_color->g, wid->fore_color->b, 0xff);
   x = wid->rect.x;
   y = wid->rect.y;
   h = wid->rect.h;
   w = wid->rect.w;
   SDL_RenderDrawLine( render, x, y, x + w, y);
   SDL_RenderDrawLine( render, x, y, x, y + h);
   SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
   SDL_RenderDrawLine( render, x + w, y, x + w, y + h);


   rect.x = box->urect.x + box->urect.w + 3;
   rect.y = box->urect.y;
   rect.w = box->lw[box->num];
   rect.h = box->lh[box->num];
   SDL_RenderCopy(render, box->label[box->num], NULL, &rect);
   if (box->num > 0) {
       rect.x = box->drect.x + 2;
       rect.y = box->drect.y + 3;
       rect.w = box->drect.w - 4;
       rect.h = box->drect.h - 6;
       SDL_RenderDrawLine( render, rect.x, rect.y, rect.x + rect.w, rect.y);
       SDL_RenderDrawLine( render, rect.x, rect.y, rect.x + (rect.w/2), rect.y + rect.h);
       SDL_RenderDrawLine( render, rect.x + rect.w, rect.y, rect.x + (rect.w/2), rect.y + rect.h);
   }
   if (box->num < box->max) {
       rect.x = box->urect.x + 2;
       rect.y = box->urect.y + 3;
       rect.w = box->urect.w - 4;
       rect.h = box->urect.h - 6;
       SDL_RenderDrawLine( render, rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h);
       SDL_RenderDrawLine( render, rect.x, rect.y + rect.h, rect.x + (rect.w/2), rect.y);
       SDL_RenderDrawLine( render, rect.x + rect.w, rect.y + rect.h, rect.x + (rect.w/2), rect.y);
   }

}

static void
close_combo(Widget wid)
{
   struct _combo_t *box = (struct _combo_t *)wid->data;
   int     i;

   for (i = 0; i < 8; i++) {
      if (box->label[i] != NULL) {
         SDL_DestroyTexture(box->label[i]);
      }
   }
   free(wid->data);
}

static void
click_combo(Widget wid, int x, int y)
{
   struct _combo_t *box = (struct _combo_t *)wid->data;

   /* Check for location of cursor in widget */
   if (x <= (wid->rect.w/2)) {
       if (box->num < box->max) {
           box->num ++;
       }
   }
   if (x > (wid->rect.w/2)) {
       if (box->num > 0) {
           box->num --;
       }
   }

   /* Update value */
   if (box->value != NULL) {
       *box->value = box->num;
   }
}

/*
 * Add combination box.
 */
Widget
add_combo(Panel win, int x, int y, int h, int w, char *labels[],
                   int *value, TTF_Font *font, SDL_Color *f_col, SDL_Color *b_col)
{
   Widget       nwid;
   SDL_Surface *surf;
   struct _combo_t *box;
   int          hh, wh;
   int          i;

   if (TTF_SizeText(font, "M", &wh, &hh)) {
      return NULL;
   }

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((box = (struct _combo_t *)calloc(1, sizeof(struct _combo_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   box->value = value;
   box->urect.x = x;
   box->urect.y = y;
   box->urect.h = h;
   box->urect.w = wh;
   box->drect.x = x+w-wh;
   box->drect.y = y;
   box->drect.h = h;
   box->drect.w = wh;
   box->max = 0;
   box->num = 0;
   /* Fill in the fields */
   for (i = 0; i < 8 && labels[i] != NULL; i++) {
       box->max = i;
       surf = TTF_RenderText_Blended(font, labels[i], *f_col);
       box->label[i] = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(box->label[i], NULL, NULL, &box->lw[i], &box->lh[i]);
       SDL_FreeSurface(surf);
   }

   if (value != NULL) {
      box->num = *value;
   }

   if (box->num > box->max) {
      box->num = box->max;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->fore_color = f_col;
   nwid->back_color = b_col;
   nwid->draw = display_combo;
   nwid->close = close_combo;
   nwid->click = click_combo;
   nwid->data = (void *)box;
   add_widget(win, nwid);
   return nwid;
}


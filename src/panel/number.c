/*
 * microsim360 - GUI draws number value of variable.
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


#include "number.h"

struct _number_t {
    TTF_Font    *font;
    int         *value;
};

static void
display_number(Widget wid, SDL_Renderer *render)
{
   struct _number_t *num = (struct _number_t *)wid->data;
   SDL_Surface *surf;
   SDL_Texture *text;
   char buffer[20];
   SDL_Rect    rect;
   int x, y, h, w;

    SDL_SetRenderDrawColor( render, wid->back_color->r,
                   wid->back_color->g, wid->back_color->b, 0xff);
    SDL_RenderFillRect( render, &wid->rect);
    SDL_SetRenderDrawColor( render, wid->fore_color->r,
                 wid->fore_color->g, wid->fore_color->b, 0xff);
    x = wid->rect.x;
    y = wid->rect.y;
    h = wid->rect.h;
    w = wid->rect.w;
    SDL_RenderDrawLine( render, x, y, x + w, y);
    SDL_RenderDrawLine( render, x, y, x, y + h);
    SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
    SDL_RenderDrawLine( render, x + w, y, x + w, y + h);

    if (num->value != NULL) {
        sprintf(buffer, "%d", *num->value);

        /* Draw text display */
        surf = TTF_RenderText_Blended(font14, buffer, *wid->fore_color);
        text = SDL_CreateTextureFromSurface( render, surf);
        SDL_QueryTexture(text, NULL, NULL, &w, &h);
        SDL_FreeSurface(surf);
        rect.x = x + (wid->rect.w - w);
        rect.y = y;
        rect.w = w;
        rect.h = h;
        SDL_RenderCopy(render, text, NULL, &rect);
        SDL_DestroyTexture(text);
    }
}

static void
close_number(Widget wid)
{
   free(wid->data);
}


/*
 * Add number display.
 */
Widget
add_number(Panel win, int x, int y, int h, int w, int *value,
                      TTF_Font *font, SDL_Color *cf, SDL_Color *cb)
{
   Widget            nwid;
   struct _number_t *num;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((num = (struct _number_t *)calloc(1, sizeof(struct _number_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   num->font = font;
   num->value = value;
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->fore_color = cf;
   nwid->back_color = cb;
   nwid->draw = display_number;
   nwid->close = close_number;
   nwid->data = (void *)num;
   add_widget(win, nwid);
   return nwid;
}



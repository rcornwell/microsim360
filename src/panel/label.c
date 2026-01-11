/*
 * microsim360 - GUI draws text on panel.
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


#include "label.h"

static void
display_label(Widget wid, SDL_Renderer *render)
{
   SDL_Texture *text = (SDL_Texture *)wid->data;
   SDL_RenderCopy( render, text, NULL, &wid->rect);
}

static void
close_label(Widget wid)
{
   SDL_Texture *text = (SDL_Texture *)wid->data;
   SDL_DestroyTexture(text);
}

/*
 * Add text.
 */
Widget
add_label(Panel win, int x, int y, char *txt,
                      TTF_Font *font, SDL_Color *cf)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   int          hx, wx, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   /* Fill in the fields */
  surf = TTF_RenderText_Blended(font, txt, *cf);
  text = SDL_CreateTextureFromSurface(win->render, surf);
  SDL_QueryTexture(text, &f, &k, &wx, &hx);

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = wx;
   nwid->rect.h = hx;
   nwid->fore_color = cf;
   nwid->draw = display_label;
   nwid->close = close_label;
   nwid->data = (void *)text;
   add_widget(win, nwid);
   SDL_FreeSurface(surf);
   return nwid;
}

/*
 * Add text, centered around point.
 */
Widget
add_label_center(Panel win, int x, int y, int w, char *txt,
                      TTF_Font *font, SDL_Color *cf)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   int          hx, wx, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   /* Fill in the fields */
  surf = TTF_RenderText_Blended(font, txt, *cf);
  text = SDL_CreateTextureFromSurface(win->render, surf);
  SDL_QueryTexture(text, &f, &k, &wx, &hx);

   nwid->rect.x = x + (w/2) - (wx/2);
   nwid->rect.y = y;
   nwid->rect.w = wx;
   nwid->rect.h = hx;
   nwid->fore_color = cf;
   nwid->draw = display_label;
   nwid->close = close_label;
   nwid->data = (void *)text;
   add_widget(win, nwid);
   SDL_FreeSurface(surf);
   return nwid;
}


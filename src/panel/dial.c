/*
 * microsim360 - GUI a rotary dial.
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

#include "dial.h"


/*
 *        0
 *    11     1
 *  10         2
 * 9             3
 *  8          4
 *    7      5
 *       6
 */

static struct {
        float     x;    /* X position */
        float     y;    /* Y position */
      } scale[12] = {
         {  0.00, -0.80},     /* 0 */
         {  0.79, -0.62},     /* 1 */  /* 38 degrees */
         {  0.98, -0.21},     /* 2 */  /* 20 degrees */
         {  1.00,  0.00},     /* 3 */
         {  0.98,  0.21},     /* 4 */  /* 20 degrees */
         {  0.79,  0.62},     /* 5 */  /* 38 degrees */
         {  0.00,  0.80},     /* 6 */
         { -0.98,  0.21},     /* 7 */  /* 20 degrees */
         { -0.79,  0.62},     /* 8 */  /* 38 degrees */
         { -1.00,  0.00},     /* 9 */
         { -0.98, -0.21},     /* 10 */  /* 20 degrees */
         { -0.79, -0.62},     /* 11 */  /* 38 degrees */
};

struct _dial_t {
    SDL_Rect     recth[12];
    SDL_Rect     rectl[12];
    SDL_Point    center;
    SDL_Point    outside[12];
    SDL_Point    line[12];
    SDL_Texture *upper[12];
    SDL_Texture *lower[12];
    uint8_t      value[12];
    uint8_t     *sel;
    int          pos;
    int          wrap;
};

static void
draw_circle(SDL_Renderer *render, int x, int y, int radius, SDL_Color color)
{
    int w, h;

    SDL_SetRenderDrawColor(render, color.r, color.g, color.b, color.a);
    for (w = 0; w < radius * 2; w++) {
        for (h = 0; h < radius * 2; h++) {
            int dx = radius - w; // horizontal offset
            int dy = radius - h; // vertical offset
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(render, x + dx, y + dy);
            }
        }
    }
}

static void
display_dial(Widget wid, SDL_Renderer *render)
{
   struct _dial_t *l = (struct _dial_t *)wid->data;
   int i, cx, cy;

   cx = l->center.x;
   cy = l->center.y;

   SDL_SetRenderDrawColor( render, wid->fore_color->r, wid->fore_color->g, wid->fore_color->b, 0xff);
   for (i = 0; i < 12; i++) {
       if (l->upper[i] != NULL) {
           SDL_RenderDrawLine( render, cx, cy, l->outside[i].x, l->outside[i].y);
           if (i != 0 && i != 6) {
               SDL_RenderDrawLine( render, l->outside[i].x, l->outside[i].y,
                                       l->line[i].x, l->line[i].y);
           }
           SDL_RenderCopy(render, l->upper[i], NULL, &l->recth[i]);
           if (l->lower[i] != NULL) {
               SDL_RenderCopy(render, l->lower[i], NULL, &l->rectl[i]);
           }
       }
   }
   draw_circle(render, cx, cy, 15, c);
   draw_circle(render, cx, cy, 10, c1);
   SDL_RenderDrawLine( render, cx, cy, l->outside[l->pos].x, l->outside[l->pos].y);
}


static void
click_dial(Widget wid, int x, int y)
{
   struct _dial_t *l = (struct _dial_t *)wid->data;
   int    cur = l->pos;

   if (x > (wid->rect.w / 2)) {
       do {
           if (++l->pos == 12) {
               l->pos = 0;
           }
           if (l->value[l->pos] == -1) {
               l->pos = cur;
               break;
           }
       } while (l->upper[l->pos] == NULL);
   } else {
       do {
           if (--l->pos < 0) {
               l->pos = 11;
           }
           if (l->value[l->pos] == -1) {
               l->pos = cur;
               break;
           }
       } while (l->upper[l->pos] == NULL);
   }
   *l->sel = l->value[l->pos];
}

static void
close_dial(Widget wid)
{
   struct _dial_t *l = (struct _dial_t *)wid->data;
   int   i;
   for (i = 0; i < 12; i++) {
       if (l->upper[i] != NULL) {
           SDL_DestroyTexture(l->upper[i]);
       }
       if (l->lower[i] != NULL) {
           SDL_DestroyTexture(l->lower[i]);
       }
   }
}

/*
 * Add dial.
 */
Widget
add_dial(Panel win, int x, int y, int h, int w, int d, Dial_label labels,
              uint8_t *value, int init, int wrap, TTF_Font *font, SDL_Color *col)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _dial_t *l;
   int          i;
   int          wh, hh, wl, hl, k;
   int          wt, ht;
   int          cent_x, cent_y;
   Uint32       f;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((l = (struct _dial_t *)calloc(1, sizeof(struct _dial_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   cent_x = x;
   cent_y = y;
   x -= w/2;
   y -= h/2;
   l->pos = 0;
   l->wrap = wrap;
   l->sel = value;
   l->center.x = cent_x;
   l->center.y = cent_y;

   /* Compute label positions */
   for (i = 0; i < 12; i++) {
       l->outside[i].x = cent_x + (int)((float)d * scale[i].x);
       l->outside[i].y = cent_y + (int)((float)d * scale[i].y);

       /* Fill in the fields */
       l->upper[i] = NULL;
       l->lower[i] = NULL;
       l->value[i] = labels->value[i];
       if (labels->upper[i] == NULL) {
          continue;
       }

       if (l->value[i] == init) {
           l->pos = i;
       }

       /* Draw labels */
       surf = TTF_RenderText_Blended(font, labels->upper[i], *col);
       text = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(text, &f, &k, &wh, &hh);
       SDL_FreeSurface(surf);
       l->upper[i] = text;
       l->recth[i].h = hh;
       l->recth[i].w = wh;
       l->rectl[i].h = 0;
       l->rectl[i].w = 0;
       if (labels->lower[i] != NULL) {
           surf = TTF_RenderText_Blended(font, labels->lower[i], *col);
           text = SDL_CreateTextureFromSurface(win->render, surf);
           SDL_QueryTexture(text, &f, &k, &wl, &hl);
           SDL_FreeSurface(surf);
           l->lower[i] = text;
           l->rectl[i].h = hl;
           l->rectl[i].w = wl;
       } else {
           hl = wl = 0;
       }

       /* Compute size of text */
       ht = hh + hl;
       if (wh > wl) {
           wt = wh;
       } else {
           wt = wl;
       }

       /* Align text with positions */
       switch(i) {
       case 0:  /* Center over dial */
              l->recth[i].x = cent_x - (wt/2);
              l->rectl[i].x = cent_x - (wt/2);
              l->recth[i].y = l->outside[i].y - hl - 10;
              l->rectl[i].y = l->outside[i].y - 10;
              /* Center over self */
              if (wl > wh) {
                 l->recth[i].x += (wh - wl) / 2;
              } else {
                 l->rectl[i].x += (wl - wh) / 2;
              }
              l->line[i].x = cent_x;
              l->line[i].y = y + ht + 2;
              break;

       case 1:
       case 2:
       case 3:
       case 4:
       case 5:
              /* Right align to width */
              l->recth[i].x = x + w - wh;
              l->rectl[i].x = x + w - wl;
              l->recth[i].y = l->outside[i].y;
              l->rectl[i].y = l->outside[i].y;
              if (wl == 0) {
                 l->recth[i].y -= hh/2;
              } else {
                 l->recth[i].y -= hh - 2;
                 l->rectl[i].y -= 2;
              }
              l->line[i].x = x + w - wt - 2;
              l->line[i].y = l->outside[i].y;
              break;

       case 6:  /* Center under dial */
              l->recth[i].x = cent_x - (wt/2);
              l->rectl[i].x = cent_x - (wt/2);
              l->recth[i].y = l->outside[i].y + 10;
              l->rectl[i].y = l->outside[i].y + 10 + hh;
              /* Center over self */
              if (wl > wh) {
                 l->recth[i].x += (wl - wh) / 2;
              } else {
                 l->recth[i].x += (wh - wl) / 2;
              }
              l->line[i].x = cent_x;
              l->line[i].y = y + (h - ht - 2);
              break;

       case 7:
       case 8:
       case 9:
       case 10:
       case 11:
              /* Align to width */
              l->recth[i].x = x;
              l->rectl[i].x = x;
              l->recth[i].y = l->outside[i].y;
              l->rectl[i].y = l->outside[i].y;
              if (wl == 0) {
                 l->recth[i].y -= hh/2;
              } else {
                 l->recth[i].y -= hh - 2;
                 l->rectl[i].y -= 2;
              }
              l->line[i].x = x + wt + 2;
              l->line[i].y = l->outside[i].y;
              break;
       }
   }

   if (l->sel != NULL) {
      *l->sel = l->value[l->pos];
   }
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->fore_color = col;
   nwid->data = (void *)l;
   nwid->draw = display_dial;
   nwid->close = close_dial;
   nwid->click = click_dial;
   add_widget(win, nwid);
   return nwid;
}



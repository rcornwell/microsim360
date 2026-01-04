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
   int            bit = 0;
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
   int          hh, wh, k;
   Uint32       f;
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
       SDL_QueryTexture(text, &f, &k, &wh, &hh);
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

#if 0
#define SET_INDICATOR16(ind, data, sh, msk) (ind)->type=U16; (ind)->mask=msk; (ind)->shift=sh; (ind)->value.v16=data;
#define SET_INDICATOR8(ind, data, sh, msk) (ind)->type=U8; (ind)->mask=msk; (ind)->shift=sh; (ind)->value.v8=data;
#define SET_INDICATORNON(ind) (ind)->type=U32; (ind)->mask=0; (ind)->shift=0; (ind)->value.v32=NULL;

/* Generic lamps */
extern struct _lamp {
    SDL_Rect      rect;           /* Area to show rectangle */
    int           col;            /* Color */
    indicator     ind;
} lamp[1000];


    text = IMG_ReadXPMFromArray(lamps_img);
    lamps = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);

    /* Lastly draw the Lamps */
    for(i = 0; i < lamp_ptr; i++) {
        rect.x = lamp[i].col * 15;
        rect.y = 0;
        rect.w = 15;
        rect.h = 15;
        if (LAMP_TEST || get_indicator(&lamp[i].ind))
            rect.y = 15;
        SDL_RenderCopy(render, lamps, &rect, & lamp[i].rect);
    }


/*
 * Get the value of an indicator.
 * If Mask is none zero compute parity instead of bit.
 */
uint32_t
get_indicator(indicator *ind)
{
    uint32_t   v = 0;

    if (ind->type == 0 && ind->value.v32 != NULL)
       v = *ind->value.v32;
    else if (ind->type < 0 && ind->value.v16 != NULL)
       v = (uint32_t)(*ind->value.v16);
    else if (ind->type > 0 && ind->value.v8 != NULL)
       v = (uint32_t)(*ind->value.v8);
    v >>= ind->shift;
    if (ind->mask != 0) {
        int        k, p = 1;
        uint32_t   mask = ind->mask;
        for (k = 0; k < 32 && mask != 0; k++) {
            uint32_t  m = 1<<k;
            if (m & mask) {
                p ^= ((v & m) != 0);
                mask ^= m;
            }
        }
        v = p;
     } else {
        v &= 1;
     }
     return v;
}
#endif

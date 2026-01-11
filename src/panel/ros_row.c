/*
 * microsim360 - GUI draws ros row.
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


#include "ros_row.h"
#include "line.h"

struct _ros_bits_w {
      int          sz;
      SDL_Rect     rect_bit[40];      /* Area where to show */
      SDL_Rect     rect_label[40];
      SDL_Texture *digit_on[40];      /* Digit on and off */
      SDL_Texture *digit_off[40];
      SDL_Texture *label[40];
      int          shift[40];
      uint32_t    *value;
};

/*
 * Display row of ROS.
 */
static void
display_ros(Widget wid, SDL_Renderer *render)
{
   struct  _ros_bits_w *row_ptr = (struct _ros_bits_w *)wid->data;
   uint32_t bit = 0;
   int      i;

   if (row_ptr->value != NULL) {
       bit = *row_ptr->value;
   }

   for (i = row_ptr->sz; i >= 0; i--) {
       if (LAMP_TEST || (bit & (1 << row_ptr->shift[i])) != 0) {
          SDL_RenderCopy(render, row_ptr->digit_on[i], NULL, &row_ptr->rect_bit[i]);
       } else {
          SDL_RenderCopy(render, row_ptr->digit_off[i], NULL, &row_ptr->rect_bit[i]);
       }
       if (row_ptr->label[i] != NULL) {
          SDL_RenderCopy( render, row_ptr->label[i], NULL, &row_ptr->rect_label[i]);
       }
   }
}

/*
 * Cleanup row of ROS.
 */
static void
close_ros(Widget wid)
{
   struct _ros_bits_w *row_ptr = (struct _ros_bits_w *)wid->data;
   int     i;

   for (i = row_ptr->sz; i >= 0; i--) {
       SDL_DestroyTexture(row_ptr->digit_on[i]);
       SDL_DestroyTexture(row_ptr->digit_off[i]);
       SDL_DestroyTexture(row_ptr->label[i]);
   }
   free(wid->data);
}

/*
 * Add Ros.
 */
Widget
add_ros_row(Panel win, int x, int y, Ros_row row,
               TTF_Font *font, uint32_t *bits, int *pos, SDL_Color *cmark)
{
    Widget       nwid;
    struct _ros_bits_w *row_ptr;
    SDL_Surface *surf;
    int          shift = row->start_bit;
    int          hx, wx, hd;
    int          i;
    int          tx;
    int          digit = 0;
    char         lab[4];

    if (TTF_SizeText(font, "M", &wx, &hx) != 0) {
        return NULL;
    }

    if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
        return NULL;
    }
    if ((row_ptr = (struct _ros_bits_w *)calloc(1, sizeof(struct _ros_bits_w))) == NULL) {
        free(nwid);
        return NULL;
    }
    nwid->rect.x = x;
    nwid->rect.y = y;
    nwid->rect.w = wx;
    row_ptr->value = bits;
    if (row->lower == NULL) {
       hd = hx;
    } else {
       hd = hx * 2;
    }
    nwid->rect.h = hd;
    *pos++ = x;
    for (i = 0; row->upper[i] != '\0'; i++) {
        lab[0] = row->upper[i];
        lab[1] = '\0';
        row_ptr->rect_bit[digit].x = x + wx;
        row_ptr->rect_bit[digit].y = y;
        row_ptr->rect_bit[digit].h = hx;
        row_ptr->rect_bit[digit].w = wx;
        switch(row->upper[i]) {
        case '|':
            tx = x + wx + (wx/2);
            add_mark(win, tx, y+1, hd-2, cmark);
            *pos++ = tx;
            x += wx*3;
            continue;

        case '!':
            *pos++ = x+2;
            add_mark(win, x+1, y+1, hd-2, cmark);
            continue;

        case ' ':
            x += wx*3;
            continue;

        case 'L':
            lab[1] = 'P';
            lab[2] = '\0';
            row_ptr->rect_bit[digit].x = x + (wx/2);
            row_ptr->rect_bit[digit].w = 2 * wx;
            break;
        }
        /* Fill in the fields */
        surf = TTF_RenderText_Blended(font, lab, *row->c_on);
        row_ptr->digit_on[digit] = SDL_CreateTextureFromSurface(win->render, surf);
        SDL_FreeSurface(surf);
        surf = TTF_RenderText_Blended(font, lab, *row->c_off);
        row_ptr->digit_off[digit] = SDL_CreateTextureFromSurface(win->render, surf);
        SDL_FreeSurface(surf);
        row_ptr->shift[digit] = shift;
        shift--;

        if (row->lower != NULL && row->lower[i] != ' ') {
            row_ptr->rect_label[digit].x = row_ptr->rect_bit[digit].x;
            row_ptr->rect_label[digit].y = y + hx;
            row_ptr->rect_label[digit].h = hx;
            row_ptr->rect_label[digit].w = wx;
            lab[0] = row->lower[i];
            lab[1] = '\0';
            surf = TTF_RenderText_Blended(font, lab, *row->c_off);
            row_ptr->label[digit] = SDL_CreateTextureFromSurface(win->render, surf);
            SDL_FreeSurface(surf);
        }
        digit++;
        x += 3*wx;
   }

   row_ptr->sz = digit;
   *pos++ = x;
   nwid->rect.w = (x - nwid->rect.x);
   nwid->fore_color = NULL;
   nwid->back_color = NULL;
   nwid->draw = display_ros;
   nwid->close = close_ros;
   nwid->data = (void *)row_ptr;
   add_widget(win, nwid);
   return nwid;
}

/*
 * Compute width of ROS line.
 */
int
ros_row_width(Panel win, Ros_row row, TTF_Font *font)
{
    int          hx, wx;
    int          i;
    int          width;

    if (TTF_SizeText(font, "M", &wx, &hx) != 0) {
        return 0;
    }

    width = 0;
    for (i = 0; row->upper[i] != '\0'; i++) {
        if(row->upper[i] != '!') {
            width += wx*3;
        }
    }

   return width;
}



/*
 * microsim360 - GUI Provided text based input.
 *
 * Copyright 2026, Richard Cornwell
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

#include "text.h"
#include "logger.h"


/* Text input selection */
struct _text_t {
     char        text[256];
     SDL_Rect    rect;
     SDL_Rect    srect;
     int         cpos_x;      /* Current x position */
     int         spos;        /* Starting position of selection */
     int         epos;        /* Ending position of selection */
     int         ipos;        /* Initial selection position */
     int         sel;
     int         selecting;
     int         cpos;        /* Current position in buffer */
     int         enable;
     int         len;
};

static void
display_textedit(Widget wid, SDL_Renderer *render)
{
     struct _text_t *data = (struct _text_t *)wid->data;
     SDL_Surface  *surf;
     SDL_Texture  *text;
     SDL_Rect      rect;
     SDL_Rect      rect2;
     int           x, y, h, w, sw, sh;

     /* Draw text display */
     SDL_SetRenderDrawColor( render, c_white.r, c_white.g, c_white.b, 0xff);
     SDL_RenderFillRect(render, &wid->rect);
     SDL_SetRenderDrawColor( render, c_black.r, c_black.g, c_black.b, 0xff);
     x = wid->rect.x;
     y = wid->rect.y;
     h = wid->rect.h;
     w = wid->rect.w;
     SDL_RenderDrawLine( render, x, y, x + w, y);
     SDL_RenderDrawLine( render, x, y, x, y + h);
     SDL_RenderDrawLine( render, x, y + h, x + w, y + h);
     SDL_RenderDrawLine( render, x + w, y, x + w, y + h);

     if (data->enable) {
       //  SDL_SetRenderDrawColor( render, c_black.r, c_black.g, c_black.b, 0xff);
         surf = TTF_RenderText_Blended(font14, data->text, c_black);
     } else {
       // SDL_SetRenderDrawColor( render, c_outline.r, c_outline.g, c_outline.b, 0xff);
        surf = TTF_RenderText_Blended(font14, data->text, c_outline);
     }
     text = SDL_CreateTextureFromSurface(render, surf);
     SDL_QueryTexture(text, NULL, NULL, &w, &h);
     SDL_FreeSurface(surf);
     rect.x = x + 1;
     rect.y = y + 2;
     rect.w = w;
     rect.h = h;
     rect2.x = 0;
     rect2.y = 0;
     rect2.w = w;
     rect2.h = h;
     /* If longer then input, chop it off */
     if (rect2.w > wid->rect.w)
        rect2.w = wid->rect.w;
     SDL_RenderCopy(render, text, &rect2, &rect);
     /* If there is a text selection, draw it */
     if (data->enable && data->sel) {
         SDL_Rect dest;
         char     buf[256];

         strncpy(buf, &data->text[data->spos], data->epos - data->spos);
         surf = TTF_RenderText_Blended(font14, buf, c_white);
         text = SDL_CreateTextureFromSurface( render, surf);
         SDL_QueryTexture(text, NULL, NULL, &sw, &sh);
         SDL_FreeSurface(surf);
         dest.x = x + data->srect.x;
         dest.y = y;
         dest.w = sw;
         dest.h = sh;
         SDL_SetRenderDrawColor(render, c_black.r, c_black.g, c_black.b, 0xff);
         SDL_RenderFillRect(render, &dest);
         SDL_RenderCopy(render, text, NULL, &dest);
     }
     SDL_DestroyTexture(text);
     /* Draw the cursor */
     if (data->enable) {
         SDL_SetRenderDrawColor(render, c_black.r, c_black.g, c_black.b, 0xff);
         x = wid->rect.x + data->cpos_x + 3;
         y = wid->rect.y + wid->rect.h - 3;
         SDL_RenderDrawLine(render, x, y, x + 2, y + 2);
         SDL_RenderDrawLine(render, x, y, x - 2, y + 2);
     }
}

static int
findtextpos(struct _text_t *text, int x)
{
    char     buffer[256];
    int      h;
    int      w = 0;
    int      pos = 0;

    for (pos = 0; text->text[pos] != '\0'; pos++) {
        buffer[pos] = text->text[pos];
        buffer[pos+1] = '\0';
        TTF_SizeText(font14, buffer, &w, &h);
        if (x < w)
           break;
    }
    text->cpos_x = w;
    text->cpos = pos;
    return pos;
}

int
text_width(char *text, int pos)
{
    char     buffer[256];
    int      w;
    int      h;

    if (pos == 0) {
       return 0;
    }
    strncpy(buffer, text, pos);
    buffer[pos] = '\0';
    TTF_SizeText(font14, buffer, &w, &h);
    return w;
}

static void
textcutpaste(struct _text_t *text, int remove, int insert, int copy)
{
     char     buffer[256];
     int      i, j;

     /* Grab current selected text */
     if (copy) {
         if (text->sel && text->spos < text->epos) {
             strncpy(buffer, &text->text[text->spos], text->epos - text->spos);
             buffer[text->epos - text->spos] = '\0';
         } else {
             buffer[0] = '\0';
         }
         SDL_SetClipboardText(buffer);
     }
     /* Remove text if asked too */
     if (remove && text->sel && text->spos < text->epos) {
         /* Copy up to start of selection */
         for (i = 0; i < text->spos && text->text[i] != '\0'; i++)
             buffer[i] = text->text[i];
         /* Position is now end of previous beginning */
         j = i;
         text->cpos = i;
         text->cpos_x = text->spos;
         /* Copy remainder */
         i = text->epos;
         while (text->text[i] != '\0')
             buffer[j++] = text->text[i++];
         buffer[j] = '\0';
         strcpy(text->text, buffer);
         text->len = strlen(text->text);
         text->epos = text->spos;
         text->sel = 0;
     }
     /* If inserting text and have a clipboard */
     if (insert && SDL_HasClipboardText()) {
         char *p = SDL_GetClipboardText();
         /* Copy up to start of selection */
         for (i = 0; i < text->cpos && text->text[i] != '\0'; i++)
             buffer[i] = text->text[i];
         /* Copy the buffer into place */
         text->spos = i;
         for (j = 0; i < sizeof(buffer) && p[j] != '\0'; j++) {
             if (p[j] == '\t')  /* Convert tabs to space */
                p[j] = ' ';
             if (p[j] < ' ')   /* Anything not character, abort */
                break;
             buffer[i++] = p[j];
         }
         /* Copy rest of buffer */
         j = text->epos;
         text->epos = i;
         text->cpos = i;
         while (i < sizeof(buffer) && text->text[j] != '\0')
             buffer[i++] = text->text[j++];
         buffer[i++] = '\0';
         /* Set text to new buffer */
         strcpy(text->text, buffer);
         text->sel = 1;
         SDL_free(p);
     }
     /* Update pixel positions */
     text->cpos_x = text_width(text->text, text->cpos);
     if (text->sel) {
         text->srect.x = text_width(text->text, text->spos);
         text->srect.w = text_width(text->text, text->epos) - text->srect.x;
     }
     text->len = strlen(text->text);
     log_trace("Text update (%s)\n", text->text);
}

static void
insert_textedit(Widget wid, SDL_TextInputEvent *text)
{
     struct _text_t *data = (struct _text_t *)wid->data;
     char     buffer[256];
     int      i, j;
     int      w, h;
     char     *t;

     t = text->text;
     /* Remove the selection before inserting new text */
     if (data->sel) {
         return textcutpaste(data, 1, 0, 0);
     }
     /* Copy up to start of selection */
     for (i = 0; i < data->cpos && data->text[i] != '\0'; i++) {
          buffer[i] = data->text[i];
     }
     j = i;  /* Save position */
     while (i < sizeof(buffer) && *t != '\0')
         buffer[i++] = *t++;
     data->cpos = i;
     buffer[i] = '\0';
     TTF_SizeText(font14, buffer, &w, &h);
     data->cpos_x = w;
     while (i < sizeof(buffer) && data->text[j] != '\0')
          buffer[i++] = data->text[j++];
     buffer[i++] = '\0';
     /* Set text to new buffer */
     strcpy(data->text, buffer);
     data->len = strlen(data->text);
}

static void
textdelete(struct _text_t *text)
{
     char     buffer[256];
     int      i;

     /* Remove the selection before inserting new text */
     if (text->sel)
         textcutpaste(text, 1, 0, 0);
     /* Nothing to delete if at beginning of text */
     if (text->cpos == 0)
         return;
     strncpy(buffer, text->text, 256);
     if (text->cpos >= 1) {
         text->cpos--;
         for (i = text->cpos; buffer[i + 1] != '\0'; i++) {
              buffer[i] = buffer[i+1];
         }
         buffer[i] = '\0';
         /* Set text to new buffer */
         strcpy(text->text, buffer);
         text->cpos_x = text_width(buffer, text->cpos);
     } else {
         text->cpos_x = 0;
     }
     text->len = strlen(text->text);
}

static void
click_textedit(Widget wid, int x, int y)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     data->enable = 1;
     SDL_StartTextInput();
     data->cpos = findtextpos(data, x);
     data->spos = data->cpos;
     data->epos = data->cpos;
     data->ipos = data->cpos;
     data->srect.x = data->cpos_x;
     data->srect.w = 0;
     data->selecting = 1;
     data->sel = 0;
     wid->focus = 1;
log_trace("enable %d %d %d\n", x, data->cpos, data->cpos_x);
}

static void
motion_textedit(Widget wid, int x, int y)
{
     struct _text_t *data = (struct _text_t *)wid->data;
     int cur_pos = data->cpos;  /* Save current position */
     int pos;

     if (!data->selecting) {
         return;
     }

    /* Find current mouse position in text */
    pos = findtextpos(data, x);
    log_trace("Motion %d pos=%d, %d\n", x, pos, data->cpos);
    if (pos < cur_pos) {
        data->spos = pos;
        data->epos = cur_pos;
        data->sel = 1;
    } else if (pos > cur_pos) {
        data->spos = cur_pos;
        data->epos = pos;
        data->sel = 1;
    } else {
        data->sel = 0;
    }
    if (data->spos > 1)
        data->srect.x = text_width(data->text, data->spos - 1);
    else
        data->srect.x = 0;
    data->srect.w = text_width(data->text, data->epos) - data->srect.x;
log_trace("Motion %d %d %d %d\n", data->spos, data->epos, data->sel, data->cpos);
}


static void
close_textedit(Widget wid)
{
     free(wid->data);
}

static void
keypress_textedit(Widget wid, SDL_KeyboardEvent *key)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     if (key->keysym.mod & KMOD_CTRL) {
         if (key->keysym.sym == SDLK_a) {
             int h, w;
             TTF_SizeText(font14, data->text, &w, &h);
             log_trace("Select All %d\n", w);
             data->spos = 0;
             data->epos = strlen(data->text);
             data->sel = 1;
             data->cpos = data->epos;
             data->cpos_x = w;
             data->srect.x = 0;
             data->srect.w = w;
         } else if (key->keysym.sym == SDLK_x) {
             textcutpaste(data, 1, 0, 0);
             log_trace("Control x\n");
         } else if (key->keysym.sym == SDLK_c) {
             textcutpaste(data, 0, 0, 1);
             log_trace("Control c\n");
         } else if (key->keysym.sym == SDLK_v) {
             textcutpaste(data, 1, 1, 0);
             log_trace("Control v\n");
         }
     }
     switch (key->keysym.scancode) {
     case SDL_SCANCODE_U:
          data->cpos = 0;
          data->cpos_x = 0;
          data->sel = 0;
          data->text[0] = '\0';
          data->len = 0;
          break;
     case SDL_SCANCODE_RETURN:
     case SDL_SCANCODE_HOME:
          data->cpos = 0;
          data->cpos_x = 0;
          data->sel = 0;
          break;
     case SDL_SCANCODE_END:
          data->cpos = data->len;
          data->cpos_x = text_width(data->text, data->cpos);
          data->sel = 0;
          break;
     case SDL_SCANCODE_LEFT:
          if (data->cpos > 0) {
              data->cpos--;
              data->cpos_x = text_width(data->text, data->cpos);
          }
          data->sel = 0;
          break;
     case SDL_SCANCODE_RIGHT:
          if (data->cpos < data->len) {
              data->cpos++;
              data->cpos_x = text_width(data->text, data->cpos);
          }
          data->sel = 0;
          break;
     case SDL_SCANCODE_DELETE:
     case SDL_SCANCODE_BACKSPACE:
          textdelete(data);
          log_trace("Key %d\n", key->keysym.scancode);
          break;
     default:
          log_trace("Key default %d\n", key->keysym.scancode);
          break;
     }
}

static void
release_textedit(Widget wid)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     if (data->selecting) {
         data->selecting = 0;
         data->cpos = data->epos;
         data->cpos_x = data->srect.x + data->srect.w;
     }
}

char *
copy_textbuffer(Widget wid, char *dest)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     if (dest != NULL) {
         strncpy(dest, data->text, 256);
     }
     return data->text;
}

char *
get_textbuffer(Widget wid)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     return data->text;
}

void
set_textbuffer(Widget wid, char *source)
{
     struct _text_t *data = (struct _text_t *)wid->data;

     if (source != NULL) {
         strncpy(data->text, source, 256);
     } else {
         data->text[0] = '\0';
     }
   data->len = strlen(data->text);
   data->cpos = data->len;
   data->cpos_x = text_width(data->text, data->cpos);
}

Widget
add_textinput(Panel win, int x, int y, int h, int w, char *text)
{
   Widget       nwid;
   struct _text_t *txt;

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((txt = (struct _text_t *)calloc(1, sizeof(struct _text_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   if (text != NULL) {
       strncpy(txt->text, text, 256);
   } else {
       txt->text[0] = '\0';
   }
   txt->len = strlen(txt->text);
   txt->cpos = txt->len;
   txt->cpos_x = text_width(txt->text, txt->cpos);
   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   nwid->back_color = &c_white;
   nwid->draw = display_textedit;
   nwid->close = close_textedit;
   nwid->click = click_textedit;
   nwid->release = release_textedit;
   nwid->motion = motion_textedit;
   nwid->keypress = keypress_textedit;
   nwid->input = insert_textedit;
   nwid->data = (void *)txt;
   add_widget(win, nwid);
   return nwid;
}

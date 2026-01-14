/*
 * microsim360 - shortcut to add in all widgets.
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

#ifndef _WIDGETS_H_
#define _WIDGETS_H_
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>

struct _labels {
      char       *upper;
      char       *lower;
};

typedef struct _widget_t {
    struct _widget_t *next;      /* Next widget to draw */
    SDL_Rect    rect;            /* Area of widget */
    SDL_Color   *fore_color;     /* Foreground color */
    SDL_Color   *back_color;     /* Background color */
    int         active;          /* Current widget active */
    int         focus;           /* Widget that has focus */
    void        *data;           /* Private data */

    /* Called to draw the image */
    void        (*draw)(struct _widget_t *w, SDL_Renderer *render);

    /* Called when the control is updated */
    void        (*update)(struct _widget_t *w);

    /* If there is a mouse down event in this widget, let it know */
    void        (*click)(struct _widget_t *w, int x, int y);

    /* Mouse button released this widget */
    void        (*release)(struct _widget_t *w);

    /* Mouse motion events */
    void        (*motion)(struct _widget_t *w, int x, int y);

    /* Key press */
    void        (*keypress)(struct _widget_t *w, SDL_KeyboardEvent *key);

    /* Text Edit event */
    void        (*textedit)(struct _widget_t *w, SDL_TextEditingEvent *text);

    /* Text input event */
    void        (*input)(struct _widget_t *w, SDL_TextInputEvent *input);

    /* Clean up any data held by widget */
    void        (*close)(struct _widget_t *w);
} widget, *Widget;

typedef struct _panel_t {
    widget     *list;            /* Pointer to list of widget */
    widget     *last_item;       /* Pointer to last widget on screen */
    int         windowID;        /* ID for this window */
    int         parentID;        /* ID for window that created up */
    void       (*notify_parent_close)(struct _panel_t *panel, int windowID);
    widget     *focus;           /* Window that currently has focus */
    SDL_Window *screen;          /* Pointer to screen. */
    SDL_Renderer *render;        /* Pointer to renderer */
} panel, *Panel;

void run_sim();

extern uint64_t    step_count;

void SDL_Setup(char *title);

Panel create_window(char *title, int w, int h, int popup);

void add_widget(Panel win, Widget wid);

extern int LAMP_TEST;

extern TTF_Font   *font0;
extern TTF_Font   *font1;
extern TTF_Font   *font10;
extern TTF_Font   *font12;
extern TTF_Font   *font14;

extern SDL_Color c_white;     /* White */
extern SDL_Color c_black;         /* Black */
extern SDL_Color c_green;    /* Green */
extern SDL_Color c_blue;    /* Blue */
extern SDL_Color c_gray;    /* Gray */
extern SDL_Color c_red;    /* Red */
extern SDL_Color c_red_off;   /* off Red */
extern SDL_Color c_back;    /* Background */
extern SDL_Color c_outline;    /* Outline color */
extern SDL_Color c_label;    /* Label background */
extern SDL_Color c_on;   /* On digit */
extern SDL_Color c_off;   /* Off digit */

extern SDL_Texture *lamps;
extern SDL_Texture *hex_dials;
extern SDL_Texture *store_dials;
extern SDL_Texture *toggle_pic;

#endif

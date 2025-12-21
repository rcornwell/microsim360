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

/* Generic indicator for widgets */
typedef struct _indicator {
    uint32_t     mask;
    uint16_t     shift;
    int16_t      type;
    union {
       uint32_t  *v32;
       uint16_t  *v16;
       uint8_t   *v8;
    } value;
} indicator;

#define      U32     0
#define      U16     -1
#define      U8      1

typedef struct _widget_t {
    struct _widget_t *next;      /* Next widget to draw */
    SDL_Rect    rect;            /* Area of widget */
    SDL_Color   *fore_color;     /* Foreground color */
    SDL_Color   *back_color;     /* Background color */
    void        *data;           /* Private data */

    /* Called to draw the image */
    void        (*draw)(struct _widget_t *w, SDL_Renderer *render);

    /* Called when the control is updated */
    void        (*update)(struct _widget_t *w);

    /* Check if there is an update to the widget */
    int         (*click)(struct _widget_t *w, int x, int y);

    /* Clean up any data held by widget */
    void        (*close)(struct _widget_t *w);
} widget, *Widget;

typedef struct _panel_t {
    widget     *list;            /* Pointer to list of widget */
    widget     *last_item;       /* Pointer to last widget on screen */
    SDL_Window *screen;          /* Pointer to screen. */
    SDL_Renderer *render;        /* Pointer to renderer */
} panel, *Panel;

int layout_periph(int *scr_wid, int *scr_hi);

void SDL_Setup(char *title, int scr_wid, int scr_hi);

void run_sim();

extern Panel       cpu_panel;

extern uint64_t    step_count;

extern char *title;

extern void (*setup_cpu)(void *rend);

extern void (*step_cpu)();

void add_widget(Panel win, Widget wid);

extern int LAMP_TEST;

#include "panel.h"
#endif

/*
 * microsim360 - Front panel control definitions.
 *
 * Copyright 2021, Richard Cornwell
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

#ifndef _PANEL_H_
#define _PANEL_H_
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>


/* Structure of device control popup */
struct _popup {
      int        unit_num;
      SDL_Window *screen;
      SDL_Renderer *render;
      void       *device;
      void       (*update)(struct _popup *pop, void *device, int index);
      Panel      panel;
};

typedef struct _window_t {
      struct   _window_t  *next, *prev;
      SDL_Window   *screen;
      SDL_Renderer  *render;
      int           windowID;
      Panel         panel;
      char         *title;
      int           popup;
} window, *Window;

void run_sim();

extern uint64_t    step_count;

extern char *title;

extern void (*setup_cpu)(void *rend);

extern void (*step_cpu)();

#endif

/*
 * microsim359 - GUI draw ROS rows.
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


#ifndef _ROS_ROW_H_
#define _ROS_ROW_H_
#include "widgets.h"

/* Hold label information */
typedef struct _ros_row {
      char       *upper;
      char       *lower;
      int        start_bit;
      SDL_Color  *c_on;
      SDL_Color  *c_off;
} ros_row, *Ros_row;

Widget add_ros_row(Panel win, int x, int y, Ros_row row,
              TTF_Font *font, uint32_t *bits, int *pos, SDL_Color *cmark);

int ros_row_width(Panel win, Ros_row row, TTF_Font *font);

#endif

/*
 * microsim360 - Model 2415 header.
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

/*
 *  Commands.
 *
 *            01234567
 *  Write     00000001       
 *  Read      00000010     
 *  Sense     00000100
 *  Readback  00001100
 *
 *  Control   00CCC111   Tape motion control
 *              000      Rewind
 *              001      Rewind and unload
 *              010      Erase Gap
 *              011      Write tape mark.
 *              100      Backspace block
 *              101      Backspace file.
 *              110      Forward space block.
 *              111      Forward space file.
 *  Mode      DDMMM011   7 track
 *                       den, odd, even, conv, noconv, trans, notrans
 *              000          NOP
 *              001          Reserved.
 *              010       y   y          y                     y
 *              011          9 track only
 *              100       y         y             y            y
 *              101       y         y             y      y
 *              110       y   y                   y            y
 *              111       y   y                   y      y
 *
 *            00          200bpi
 *            01          556bpi
 *            10          800bpi
 *            11          9 track mode. Models 4-6
 *
 *  Mode      11NNN011    9 track.
 *              000       1600 bpi
 *              001       800 bpi
 */


#ifndef _MODEL2415_H_
#define _MODEL2415_H_
#include <SDL.h>
#include <stdint.h>
#include "device.h"

struct _device *model2415_init(SDL_Renderer *render); 
#endif

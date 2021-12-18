/*
 * microsim360 - Front panel display.
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

#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <SDL_main.h>
#endif
#include <fcntl.h>

#include "panel.h"
#include "model2030.h"
#include "model1050.h"
#include "model1442.h"
#include "model1443.h"
#include "model2415.h"
#include "lamps_img.xpm"
#include "hex_dial_img.xpm"
#include "store_dials_img.xpm"

int process(void *data);
uint32_t timer_callback(uint32_t interval, void *param);

#define ADD_LABEL(x1, y1, ww, t, cf, cb) ctl_label[ctl_ptr].rect.y = (y1); \
                              text = TTF_RenderText_Shaded(font1, t, cf, cb); \
                              ctl_label[ctl_ptr].text = \
                                       SDL_CreateTextureFromSurface(render, text) ; \
                              SDL_QueryTexture(ctl_label[ctl_ptr].text, &f, &k, &wx, &hx); \
                              ctl_label[ctl_ptr].rect.x = (x1) + (ww / 2) - (wx/2); \
                              ctl_label[ctl_ptr].rect.h = hx; \
                              ctl_label[ctl_ptr].rect.w = wx; \
                              SDL_FreeSurface(text); ctl_ptr++;
#define ADD_LABEL1(x1, y1, t) ctl_label[ctl_ptr].rect.x = (x1); \
                             ctl_label[ctl_ptr].rect.y = (y1); \
                             ctl_label[ctl_ptr].rect.h = hd; \
                             ctl_label[ctl_ptr].rect.w = wd * strlen(t); \
                             text = TTF_RenderText_Shaded(font1, t, c, cl); \
                             ctl_label[ctl_ptr].text = \
                                      SDL_CreateTextureFromSurface(render, text) ; \
                             SDL_FreeSurface(text); ctl_ptr++;
#define ADD_LABEL2(x1, y1, t) ctl_label[ctl_ptr].rect.x = (x1); \
                              ctl_label[ctl_ptr].rect.y = (y1); \
                              ctl_label[ctl_ptr].rect.h = hd; \
                              ctl_label[ctl_ptr].rect.w = wd * strlen(t); \
                              text = TTF_RenderText_Shaded(font1, t, c1, cl); \
                              ctl_label[ctl_ptr].text = \
                                       SDL_CreateTextureFromSurface(render, text) ; \
                              SDL_FreeSurface(text); ctl_ptr++;
#define ADD_LABEL3(x1, y1, t, cf, cb) ctl_label[ctl_ptr].rect.y = (y1); \
                              text = TTF_RenderText_Shaded(font1, t, cf, cb); \
                              ctl_label[ctl_ptr].text = \
                                       SDL_CreateTextureFromSurface(render, text) ; \
                              SDL_QueryTexture(ctl_label[ctl_ptr].text, &f, &k, &wx, &hx); \
                              ctl_label[ctl_ptr].rect.x = (x1); \
                              ctl_label[ctl_ptr].rect.h = hx; \
                              ctl_label[ctl_ptr].rect.w = wx; \
                              SDL_FreeSurface(text); ctl_ptr++;
#define ADD_MARK(x, y, h, col) marks[mrk_ptr].x1 = (x); marks[mrk_ptr].y1 = (y); \
             marks[mrk_ptr].x2 = (x); marks[mrk_ptr].y2 = (y+h); \
             marks[mrk_ptr].c = (&col); mrk_ptr++;

#define ADD_LINE(x, y, w, col) marks[mrk_ptr].x1 = (x); marks[mrk_ptr].y1 = (y); \
             marks[mrk_ptr].x2 = (x+w); marks[mrk_ptr].y2 = (y); \
             marks[mrk_ptr].c = (&col); mrk_ptr++;

#define ADD_AREA(x1, y1, h1, w1, col) areas[area_ptr].rect.x = x1; areas[area_ptr].rect.y = y1; \
            areas[area_ptr].rect.h = h1; areas[area_ptr].rect.w = w1; areas[area_ptr++].c = col;

#define inrect(px, py, r) ((px > r.x) && (px < (r.x + r.w)) && (py > r.y) && (py < (r.y + r.h)))

TTF_Font *font1;
TTF_Font *font12;
TTF_Font *font14;
SDL_Color c= {0xff, 0xff, 0xff};	/* White */
SDL_Color c1= {0x0, 0x0, 0x0};		/* Black */
SDL_Color c2= {0x83, 0x89, 0x7f};	/* Green */
SDL_Color c3= {0x17, 0x69, 0x99};	/* Blue */
SDL_Color c4= {0xc0, 0xbc, 0xb9};	/* Gray */
SDL_Color c5= {0xe3, 0x20, 0x4e};   /* Red */
SDL_Color c5o= {0x52, 0x08, 0x1f};   /* off Red */
SDL_Color cc= {0xdd, 0xd8, 0xc5};   /* Background */
SDL_Color cb= {0x7d, 0x79, 0x78};   /* Outline color */
SDL_Color cl= {0xb4, 0xb0, 0xa5};   /* Label background */
SDL_Color con= {0xd8, 0xcb, 0x72};   /* On digit */
SDL_Color cof= {0x1a, 0x1a, 0x1a};   /* Off digit */
SDL_Window *screen = NULL;
SDL_Window *screen2 = NULL;
SDL_Window *screen3 = NULL;
SDL_Renderer *render = NULL;
SDL_Renderer *render2 = NULL;
SDL_Renderer *render3 = NULL;
SDL_Texture *digit_on[100];
SDL_Texture *digit_off[100];
SDL_Texture *digit2_on[100];
SDL_Texture *digit2_off[100];
SDL_Texture *on, *off;
SDL_Texture *lamps;
SDL_Texture *hex_dials;
SDL_Texture *store_dials[3];

#define DIG_LP   17
#define DIG_CD   18
#define DIG_CC   19
#define DIG_SLI  20
#define DIG_SKP  21
#define DIG_PCI  22
#define DIG_OP   23

struct _labels {
    char      *upper;
    char      *lower;
} labels[] = {
    { "0", NULL },
    { "1", NULL },
    { "2", NULL },
    { "3", NULL },
    { "4", NULL },
    { "5", NULL },
    { "6", NULL },
    { "7", NULL },
    { "8", NULL },
    { "9", NULL },
    { "A", NULL },
    { "B", NULL },
    { "C", NULL },
    { "D", NULL },
    { "E", NULL },
    { "F", NULL },
    { "P", NULL },
    { "LP", NULL },
    { "CD", NULL },
    { "CC", NULL },
    { "SLI", NULL },
    { "SKIP", NULL },
    { "PCI", NULL },
    { "OP", "IN" },
    { "ADR", "IN" },
    { "STAT", "IN" },
    { "SERV", "IN" },
    { "SEL", "OUT" },
    { "ADR", "OUT" },
    { "CMND", "OUT" },
    { "SERV", "OUT" },
    { "SUP", "OUT" },
    { "IL", NULL },
    { "PROG", NULL },
    { "PROT", NULL },
    { "CHNL", "DATA" },
    { "CHNL", "CTRL" },
    { "INT", "FACE" },
    { "MAIN STOR", NULL },
    { "AUX STOR", NULL },
    { "EX", NULL },
    { "MATCH", NULL },
    { "ALLOW", "WRITE" },
    { "1050", "INTV" },
    { "1050", "REQ" },
    { "MPX", "CHNL" },
    { "SEL" "CHNL" },
    { "COMP", "MODE" },
    { "STOR", "ADR" },
    { "STOR", "DATA" },
    { "A", "REG" },
    { "B", "REG" },
    { "ALU", NULL },
    { "ROS", "ADR" },
    { "ROS", "SALS" },
    { "CTRL", "REG" },
    { NULL, NULL }
};


struct _labels sw_labels[] = {
    { "SYSTEM", "RESET" },
    { "ROAR", "RESET" },
    { "START", NULL },
    { "SET", "IC" },
    { "CHECK", "RESET" },
    { "STOP", NULL },
    { "INT TMR", NULL },
    { "STORE", NULL },
    { "LAMP", "TEST" },
    { "DISPLAY", NULL },
    { "POWER", "ON" },
    { "POWER", "OFF" },
    { "INTERRUPT", NULL },
    { "LOAD", NULL },
    { NULL, NULL }
};

             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row1 =    "P012345  P L P18421  P84218421";
char *row1a=    "             P34567  P01234567";
int   pos1[32];
             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row2 =    "P0123 0123 A0123 0101201AP0123";
int   pos2[32];
             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row3 =    "P 0123      012  01 01012A0123";
int   pos3[32];
char *row_cnt = "     P84218421 P84217421      ";
char *row_cnta= "     P01234576 P01234576      ";
int   pos_cnt[32];

char *chan_one = "     P84218421 P8421 8421     ";
char *chan_onea= "     P01234576 P0123 4567     ";
int   pos_chan1[32];
int   pos_chan2[32];
char *chan_two = "     P84218421 P8421 8421     ";
char *chan_twoa= "     P01234576 P0123 4567     ";

char *store_addr = "P84218421 P84218421 ";
char *store_addra= "P01234567 P01234567 ";
char *data_reg = "P84218421  84218421 ";
int pos_mpx[32];
int pos_store[32];
int pos_main[32];
int pos_data[32];
int pos_breg[32];

int ros_ptr = 0;
int lamp_ptr = 0;
int led_ptr = 0;
int area_ptr = 0;
int mrk_ptr = 0;
int ctl_ptr = 0;
int sws_ptr = 0;
int ind_ptr = 0;
int hex_ptr = 0;
int store_ptr = 0;

#define CHAR              001777
#define SHFT              000100
#define TOP               000200
#define META              000400
#define CTRL              001000

int key_state = 0;
int text_entry = -1;

void
draw_circle(int x, int y, int radius, SDL_Color color)
{
    SDL_SetRenderDrawColor(render, color.r, color.g, color.b, color.a);
    for (int w = 0; w < radius * 2; w++)
    {
        for (int h = 0; h < radius * 2; h++)
        {
            int dx = radius - w; // horizontal offset
            int dy = radius - h; // vertical offset
            if ((dx*dx + dy*dy) <= (radius * radius))
            {
                SDL_RenderDrawPoint(render, x + dx, y + dy);
            }
        }
    }
}

void
add_switch(int *value, int x, int y, int w, int h, int t, SDL_Color *col, struct _labels *lab, TTF_Font *font)
{
    SDL_Surface *text;
   
    if (lab != NULL && lab->upper != NULL) {
        sws[sws_ptr].lab = lab->upper;
        text = TTF_RenderText_Shaded(font, lab->upper, (col == &c) ? c1 : c, *col);
        sws[sws_ptr].top = SDL_CreateTextureFromSurface( render, text);
        sws[sws_ptr].top_len = strlen(lab->upper);
        SDL_FreeSurface(text);
    } else {
        sws[sws_ptr].top = NULL;
        sws[sws_ptr].top_len = 0;
    }
    if (lab != NULL && lab->lower != NULL) {
        text = TTF_RenderText_Shaded(font, lab->lower, (col == &c) ? c1 : c , *col);
        sws[sws_ptr].bot = SDL_CreateTextureFromSurface( render, text);
        sws[sws_ptr].bot_len = strlen(lab->lower);
        SDL_FreeSurface(text);
    } else {
        sws[sws_ptr].bot = NULL;
        sws[sws_ptr].bot_len = 0;
    }
    sws[sws_ptr].rect.x = x;
    sws[sws_ptr].rect.y = y;
    sws[sws_ptr].rect.w = w;
    sws[sws_ptr].rect.h = h;
    sws[sws_ptr].c[0] = col;
    sws[sws_ptr].value = value;
    sws[sws_ptr].type = t;
    sws_ptr++;
}

/*
 * Add in a lamp display.
 */
int
add_led(uint16_t *value, int shf, int x, int y, int hd, int wd, int idx)
{
    int w = 0;

    if (labels[idx].lower != NULL) {
        led_bits[led_ptr].rectl.x = x;
        led_bits[led_ptr].rectl.y = y + (hd/2);
        led_bits[led_ptr].rectl.h = hd;
        led_bits[led_ptr].rectl.w = wd * strlen(labels[idx].lower);
        w = led_bits[led_ptr].rectl.w;
        led_bits[led_ptr].digitl_on = digit2_on[idx];
        led_bits[led_ptr].digitl_off = digit2_off[idx];
        y -= (hd/2);
    } else {
        led_bits[led_ptr].digitl_on = NULL;
        led_bits[led_ptr].digitl_off = NULL;
    }
    led_bits[led_ptr].recth.x = x;
    led_bits[led_ptr].recth.y = y;
    led_bits[led_ptr].recth.h = hd;
    led_bits[led_ptr].recth.w = wd * strlen(labels[idx].upper);
    if (led_bits[led_ptr].recth.w > w)
        w = led_bits[led_ptr].recth.w;
    led_bits[led_ptr].digith_on = digit_on[idx];
    led_bits[led_ptr].digith_off = digit_off[idx];
    led_bits[led_ptr].value = value;
    led_bits[led_ptr].shift = shf;
    led_ptr++;
    return w;
}

void
draw_screen30(int hd, int wd)
{
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    char          buf[100];

    /* Clear display */
    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);
    /* Draw backgrounds */
    for (i = 0; i < area_ptr; i++) {
        SDL_SetRenderDrawColor( render, areas[i].c->r,
                                        areas[i].c->g,
                                        areas[i].c->b, 0xff);
        SDL_RenderFillRect( render, &areas[i].rect);
    }
    /* Draw all labels */
    for (i = 0; i < ctl_ptr; i++) {
        SDL_RenderCopy(render, ctl_label[i].text, NULL, & ctl_label[i].rect);
    }
    /* Draw all lines */
    for (i = 0; i < mrk_ptr; i++) {
        SDL_SetRenderDrawColor( render, marks[i].c->r,
                                        marks[i].c->g,
                                        marks[i].c->b, 0xff);
        SDL_RenderDrawLine( render, marks[i].x1, marks[i].y1,
                                    marks[i].x2, marks[i].y2);
    }
    /* Draw ROS lights */
    for (i = 0; i < ros_ptr; i++) {
        int row = 0;
        switch (ros_bits[i].row) {
        case 0: row = ros_2030[cpu_2030.WX].row1; break;
        case 1: row = ros_2030[cpu_2030.WX].row2; break;
        case 2: row = ros_2030[cpu_2030.WX].row3; break;
        }
        if (LAMP_TEST || (row & (1 << ros_bits[i].shift)) != 0)
            SDL_RenderCopy(render, ros_bits[i].digit_on, NULL, & ros_bits[i].rect);
        else
            SDL_RenderCopy(render, ros_bits[i].digit_off, NULL, & ros_bits[i].rect);
    }

    /* Draw rest of lights */
    for (i = 0; i < led_ptr; i++) {
        uint16_t row = 0;
        if (led_bits[i].value != 0)
           row = *led_bits[i].value;
        if (LAMP_TEST || (row & (1 << led_bits[i].shift)) != 0) {
            SDL_RenderCopy(render, led_bits[i].digith_on, NULL, & led_bits[i].recth);
            SDL_RenderCopy(render, led_bits[i].digitl_on, NULL, & led_bits[i].rectl);
        } else {
            SDL_RenderCopy(render, led_bits[i].digith_off, NULL, & led_bits[i].recth);
            SDL_RenderCopy(render, led_bits[i].digitl_off, NULL, & led_bits[i].rectl);
        }
    }

    /* Draw push buttons */
    for (i = 0; i < sws_ptr; i++) {
        SDL_SetRenderDrawColor( render, sws[i].c[0]->r,
                                        sws[i].c[0]->g,
                                        sws[i].c[0]->b, 0xff);
        SDL_RenderFillRect( render, &sws[i].rect);
        if (sws[i].type == ONOFF) {  /* Handle timer interrupt switch */
           SDL_SetRenderDrawColor( render, c.r, c.g, c.b, 0xff);
           SDL_RenderFillRect( render, &sws[i].rect);
           rect.x = sws[i].rect.x;
           rect.y = sws[i].rect.y;
           rect.w = wd * 2;
           rect.h = hd;
           SDL_RenderFillRect( render, &rect);
           SDL_RenderCopy(render, on, NULL, & rect);
           rect.x = sws[i].rect.x + sws[i].rect.w - (wd * 3);
           rect.y = sws[i].rect.y + hd;
           rect.w = wd * 3;
           rect.h = hd;
           SDL_RenderCopy(render, off, NULL, & rect);
 
           SDL_SetRenderDrawColor( render, sws[i].c[0]->r,
                                           sws[i].c[0]->g,
                                           sws[i].c[0]->b, 0xff);
           rect.x = sws[i].rect.x;
           rect.y = sws[i].rect.y;
           if (*sws[i].value == 0)
               rect.y += hd;
           rect.w = sws[i].rect.w;
           rect.h = hd;
           SDL_RenderFillRect( render, &rect);
           rect.x = sws[i].rect.x + (sws[i].rect.w / 2) - ((wd * sws[i].top_len)/2);
           rect.w = wd * sws[i].top_len;
           SDL_RenderCopy(render, sws[i].top, NULL, & rect);
        } else if (sws[i].top != NULL && sws[i].bot != NULL) {
           rect.x = sws[i].rect.x + (sws[i].rect.w / 2) - ((wd * sws[i].top_len)/2);
           rect.y = sws[i].rect.y;
           rect.w = wd * sws[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(render, sws[i].top, NULL, & rect);
           rect.x = sws[i].rect.x + (sws[i].rect.w / 2) - ((wd * sws[i].bot_len)/2);
           rect.w = wd * sws[i].bot_len;
           rect.y += hd;
           SDL_RenderCopy(render, sws[i].bot, NULL, & rect);
        } else if (sws[i].top != NULL) {
           rect.x = sws[i].rect.x + (sws[i].rect.w / 2) - ((wd * sws[i].top_len)/2);
           rect.y = sws[i].rect.y + (hd/2);
           rect.w = wd * sws[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(render, sws[i].top, NULL, & rect);
        }
        if (sws[i].active) {
           SDL_SetRenderDrawColor( render, 0, 0, 0, 0xff);
           SDL_RenderDrawRect( render, &sws[i].rect);
        }
     }

    /* Draw indicator lights */
    for (i = 0; i < ind_ptr; i++) {
        j = 0;
        if (ind[i].value != 0 && *ind[i].value)
            j = 1;
        SDL_SetRenderDrawColor( render, ind[i].c[j]->r,
                                        ind[i].c[j]->g,
                                        ind[i].c[j]->b, 0xff);
        SDL_RenderFillRect( render, &ind[i].rect);
        if (ind[i].top != NULL && ind[i].bot != NULL) {
           rect.x = ind[i].rect.x + (ind[i].rect.w / 2) - ((wd * ind[i].top_len)/2);
           rect.y = ind[i].rect.y;
           rect.w = wd * ind[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(render, ind[i].top, NULL, & rect);
           rect.w = wd * ind[i].bot_len;
           rect.y += hd;
           SDL_RenderCopy(render, ind[i].bot, NULL, & rect);
        } else if (ind[i].top != NULL) {
           rect.x = ind[i].rect.x + (ind[i].rect.w / 2) - ((wd * ind[i].top_len)/2);
           rect.y = ind[i].rect.y + (hd/2);
           rect.w = wd * ind[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(render, ind[i].top, NULL, & rect);
        }
     }

     /* Draw rotary dial switches */
     for (i = 0; i < 4; i++ ) {
          SDL_SetRenderDrawColor( render, c1.r, c1.g, c1.b, 0xff);
          for (j = 0; j <= dial[i].max; j++) {
              SDL_RenderDrawLine( render, dial[i].pos_x[j], dial[i].pos_y[j], 
                                      dial[i].center_x, dial[i].center_y);
          }
          draw_circle(dial[i].center_x, dial[i].center_y, (2 *hd) , c);
          draw_circle(dial[i].center_x, dial[i].center_y, (2 *hd) - (hd/2), c1);
          SDL_SetRenderDrawColor( render, c1.r, c1.g, c1.b, 0xff);
          SDL_RenderDrawLine( render, dial[i].pos_x[*(dial[i].value)],
                                      dial[i].pos_y[*(dial[i].value)], 
                                      dial[i].center_x, dial[i].center_y);
    }

    /* Draw hex selector switches */
    for (i = 0; i < hex_ptr; i++) {
         uint8_t  d = *hex_dial[i].digit;
         rect.x = ((int)d & 3) * 64;
         rect.y = (((int)d & 0xc) >> 2) * 64;
         rect.h = 64;
         rect.w = 64;
         SDL_RenderCopy(render, hex_dials, &rect, &hex_dial[i].rect);
    }

    /* Draw store selector switches */
    for (i = 0; i < store_ptr; i++) {
         uint8_t  d = *store_dial[i].digit;
         int       x, y;
         rect.x = ((int)d & 3) * 81;
         rect.y = (((int)d & 0xc) >> 2) * 81;
         rect.h = 81;
         rect.w = 81;
         x = store_dial[i].rect.x + 40;
         y = store_dial[i].rect.y + 40;
         SDL_RenderCopy(render, store_dials[0], &rect, &store_dial[i].rect);
         switch (store_dial[i].sel & 3) {
         case 0:
                 SDL_SetRenderDrawColor( render, cb.r, cb.g, cb.b, 0xff);
                 SDL_RenderDrawLine( render, x, y, x - 5, y - 5);
                 SDL_RenderDrawLine( render, x-1, y, x - 6, y - 5);
                 break;
         case 1:
         case 3:
                 SDL_SetRenderDrawColor( render, c5.r, c5.g, c5.b, 0xff);
                 SDL_RenderDrawLine( render, x, y, x, y - 9);
                 SDL_RenderDrawLine( render, x-1, y, x-1, y - 9);
                 break;
         case 2:
                 SDL_SetRenderDrawColor( render, c1.r, c1.g, c1.b, 0xff);
                 SDL_RenderDrawLine( render, x, y, x + 5, y - 5);
                 SDL_RenderDrawLine( render, x+1, y, x + 6, y - 5);
                 break;
         }
    }

    /* Lastly draw the Lamps */
    for(i = 0; i < lamp_ptr; i++) {
        rect.x = lamp[i].col * 15;
        rect.y = 0;
        rect.w = 15;
        rect.h = 15;
        if (LAMP_TEST || (lamp[i].value != 0 && *lamp[i].value & (1 << lamp[i].shift)))
            rect.y = 15;
        SDL_RenderCopy(render, lamps, &rect, & lamp[i].rect);
    }

    sprintf(buf, "%10d", cpu_2030.count);
    cpu_2030.count = 0;
    rect.x = 700;
    rect.y = 10;
    rect.h = hd;
    rect.w = 10*wd;
    text = TTF_RenderText_Shaded(font1, buf, c1, c);
    txt = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    SDL_RenderCopy(render, txt, NULL, & rect);
    SDL_RenderPresent( render );
}

void
draw_popup(struct _popup *popup, int hd, int wd)
{
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int          w, h;
    char          buf[100];

    /* Clear display */
    SDL_SetRenderDrawColor( popup->render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( popup->render);
    /* Draw backgrounds */
    for (i = 0; i < popup->area_ptr; i++) {
        SDL_SetRenderDrawColor( popup->render, popup->areas[i].c->r,
                                        popup->areas[i].c->g,
                                        popup->areas[i].c->b, 0xff);
        SDL_RenderFillRect( popup->render, &popup->areas[i].rect);
    }
    /* Draw all labels */
    for (i = 0; i < popup->ctl_ptr; i++) {
        SDL_RenderCopy(popup->render, popup->ctl_label[i].text, NULL, & popup->ctl_label[i].rect);
    }
    /* Draw all lines */
    for (i = 0; i < popup->mrk_ptr; i++) {
        SDL_SetRenderDrawColor( popup->render, popup->marks[i].c->r,
                                        popup->marks[i].c->g,
                                        popup->marks[i].c->b, 0xff);
        SDL_RenderDrawLine( popup->render, popup->marks[i].x1, popup->marks[i].y1,
                                    popup->marks[i].x2, popup->marks[i].y2);
    }

    /* Draw rest of lights */
    for (i = 0; i < popup->led_ptr; i++) {
        uint16_t row = 0;
        if (popup->led_bits[i].value != 0)
           row = *popup->led_bits[i].value;
        if ((row & (1 << popup->led_bits[i].shift)) != 0) {
            SDL_RenderCopy(popup->render, popup->led_bits[i].digith_on, NULL, & popup->led_bits[i].recth);
            SDL_RenderCopy(popup->render, popup->led_bits[i].digitl_on, NULL, & popup->led_bits[i].rectl);
        } else {
            SDL_RenderCopy(popup->render, popup->led_bits[i].digith_off, NULL, & popup->led_bits[i].recth);
            SDL_RenderCopy(popup->render, popup->led_bits[i].digitl_off, NULL, & popup->led_bits[i].rectl);
        }
    }

    /* Draw push buttons */
    for (i = 0; i < popup->sws_ptr; i++) {
        SDL_SetRenderDrawColor( popup->render, popup->sws[i].c[0]->r,
                                        popup->sws[i].c[0]->g,
                                        popup->sws[i].c[0]->b, 0xff);
        SDL_RenderFillRect( popup->render, &popup->sws[i].rect);
        if (popup->sws[i].top != NULL && popup->sws[i].bot != NULL) {
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((wd * popup->sws[i].top_len)/2);
           rect.y = popup->sws[i].rect.y;
           rect.w = wd * popup->sws[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(popup->render, popup->sws[i].top, NULL, & rect);
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((wd * popup->sws[i].bot_len)/2);
           rect.w = wd * popup->sws[i].bot_len;
           rect.y += hd;
           SDL_RenderCopy(popup->render, popup->sws[i].bot, NULL, & rect);
        } else if (popup->sws[i].top != NULL) {
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((wd * popup->sws[i].top_len)/2);
           rect.y = popup->sws[i].rect.y + (hd/2);
           rect.w = wd * popup->sws[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(popup->render, popup->sws[i].top, NULL, & rect);
        }
        if (sws[i].active) {
           SDL_SetRenderDrawColor( popup->render, 0, 0, 0, 0xff);
           SDL_RenderDrawRect( popup->render, &popup->sws[i].rect);
        }
     }

    /* Draw indicator lights */
    for (i = 0; i < popup->ind_ptr; i++) {
        j = 0;
        if (popup->ind[i].value != 0 && *popup->ind[i].value)
            j = 1;
        SDL_SetRenderDrawColor( popup->render, popup->ind[i].c[j]->r,
                                        popup->ind[i].c[j]->g,
                                        popup->ind[i].c[j]->b, 0xff);
        SDL_RenderFillRect( popup->render, &popup->ind[i].rect);
        SDL_SetRenderDrawColor( popup->render, popup->ind[i].ct->r,
                                        popup->ind[i].ct->g,
                                        popup->ind[i].ct->b, 0xff);
        SDL_RenderDrawLine( popup->render, popup->ind[i].rect.x, popup->ind[i].rect.y + 2, 
                                           popup->ind[i].rect.x + popup->ind[i].rect.w, popup->ind[i].rect.y + 2);
        SDL_RenderDrawLine( popup->render, popup->ind[i].rect.x, popup->ind[i].rect.y + popup->ind[i].rect.h - 2, 
                                           popup->ind[i].rect.x + popup->ind[i].rect.w, popup->ind[i].rect.y + popup->ind[i].rect.h - 2);
        if (popup->ind[i].top != NULL && popup->ind[i].bot != NULL) {
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((wd * popup->ind[i].top_len)/2);
           rect.y = popup->ind[i].rect.y + 2;
           rect.w = wd * popup->ind[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(popup->render, popup->ind[i].top, NULL, & rect);
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((wd * popup->ind[i].bot_len)/2);
           rect.w = wd * popup->ind[i].bot_len;
           rect.y += hd - 1;
           SDL_RenderCopy(popup->render, popup->ind[i].bot, NULL, & rect);
        } else if (popup->ind[i].top != NULL) {
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((wd * popup->ind[i].top_len)/2);
           rect.y = popup->ind[i].rect.y + (hd/2);
           rect.w = wd * popup->ind[i].top_len;
           rect.h = hd;
           SDL_RenderCopy(popup->render, popup->ind[i].top, NULL, & rect);
        }
     }

    /* Lastly draw the Text entry */
    for(i = 0; i < popup->txt_ptr; i++) {
        /* Draw text display */
        if (popup->text[i].enable) {
            SDL_SetRenderDrawColor( popup->render, c1.r, c1.g, c1.b, 0xff);
            text = TTF_RenderText_Solid(font14, popup->text[i].text, c1);
        } else {
           SDL_SetRenderDrawColor( popup->render, cb.r, cb.g, cb.b, 0xff);
           text = TTF_RenderText_Solid(font14, popup->text[i].text, cb);
        }
        txt = SDL_CreateTextureFromSurface( popup->render, text);
        SDL_QueryTexture(txt, NULL, NULL, &w, &h);
        SDL_FreeSurface(text);
        rect.x = popup->text[i].rect.x + 1;
        rect.y = popup->text[i].rect.y + 2;
        rect.w = popup->text[i].rect.w;
        rect.h = h;
        rect2.x = 0;
        rect2.y = 0;
        rect2.w = w;
        rect2.h = h;
        if (rect.w > rect2.w)
           rect.w = rect2.w;
        SDL_RenderDrawRect ( popup->render, &popup->text[i].rect);
        SDL_RenderCopy(popup->render, txt, &rect2, &rect);
        SDL_DestroyTexture(txt);
        /* If there is a text selection, draw it */
        if (popup->text[i].enable && popup->text[i].sel) {
            SDL_Rect dest;
            strncpy(buf, &popup->text[i].text[popup->text[i].spos],
                       popup->text[i].epos - popup->text[i].spos);
            text = TTF_RenderText_Solid(font14, popup->text[i].text, c);
            txt = SDL_CreateTextureFromSurface( popup->render, text);
            SDL_FreeSurface(text);
            rect2.x = popup->text[i].srect.x;
            rect2.y = 0;
            rect2.w = popup->text[i].srect.w;
            rect2.h = h;
            dest.x = rect.x + rect2.x;
            dest.y = rect.y;
            dest.w = rect2.w;
            dest.h = h;
            SDL_SetRenderDrawColor( popup->render, c1.r, c1.g, c1.b, 0xff);
            SDL_RenderFillRect( popup->render, &dest);
            SDL_RenderCopy(popup->render, txt, &rect2, &dest);
            SDL_DestroyTexture(txt);
        }
        /* Draw the cursor */
        if (popup->text[i].enable) {
            SDL_SetRenderDrawColor( popup->render, c1.r, c1.g, c1.b, 0xff);
            rect.x = popup->text[i].rect.x + popup->text[i].cpos + 3;
            rect.y = popup->text[i].rect.y + rect.h + 2;
            SDL_RenderDrawLine( popup->render, rect.x, rect.y, rect.x + 2, rect.y + 2);
            SDL_RenderDrawLine( popup->render, rect.x, rect.y, rect.x - 2, rect.y + 2);
        }
    }

    /* Draw counters */
    for(i = 0; i < popup->num_ptr; i++) {
        char buffer[10];

        SDL_SetRenderDrawColor( popup->render, popup->number[i].c->r,
                                        popup->number[i].c->g,
                                        popup->number[i].c->b, 0xff);
        SDL_RenderFillRect( popup->render, &popup->number[i].rect);
        SDL_SetRenderDrawColor( popup->render, c1.r, c1.g, c1.b, 0xff);
        SDL_RenderDrawRect ( popup->render, &popup->number[i].rect);
        if (popup->number[i].value != 0) {
            sprintf(buffer, "%d", *popup->number[i].value);
            
            /* Draw text display */
            text = TTF_RenderText_Solid(font14, buffer, c1);
            txt = SDL_CreateTextureFromSurface( popup->render, text);
            SDL_QueryTexture(txt, NULL, NULL, &w, &h);
            SDL_FreeSurface(text);
            rect.x = popup->number[i].rect.x + (popup->number[i].rect.w - w);
            rect.y = popup->number[i].rect.y;
            rect.w = w;
            rect.h = h;
            SDL_RenderCopy(popup->render, txt, NULL, &rect);
            SDL_DestroyTexture(txt);
        }
    }

    /* Draw combo boxes */
    for(i = 0; i < popup->cmb_ptr; i++) {
        SDL_SetRenderDrawColor( popup->render, 0xff, 0xff, 0xff, 0xff);
        SDL_RenderFillRect( popup->render, &popup->combo[i].rect);
        SDL_SetRenderDrawColor( popup->render, 0x00, 0x00, 0x00, 0xff);
        SDL_RenderDrawRect ( popup->render, &popup->combo[i].rect);
        rect.x = popup->combo[i].rect.x + hd;
        rect.y = popup->combo[i].rect.y;
        rect.w = popup->combo[i].lw[popup->combo[i].num];
        rect.h = popup->combo[i].lh[popup->combo[i].num];
        SDL_RenderCopy(popup->render, popup->combo[i].label[popup->combo[i].num], NULL, &rect);
        if (popup->combo[i].num > 0) {
            rect.x = popup->combo[i].drect.x + 2;
            rect.y = popup->combo[i].drect.y + 3;
            rect.w = popup->combo[i].drect.w - 4;
            rect.h = popup->combo[i].drect.h - 6;
            SDL_RenderDrawLine( popup->render, rect.x, rect.y, rect.x + rect.w, rect.y);
            SDL_RenderDrawLine( popup->render, rect.x, rect.y, rect.x + (rect.w/2), rect.y + rect.h);
            SDL_RenderDrawLine( popup->render, rect.x + rect.w, rect.y, rect.x + (rect.w/2), rect.y + rect.h);
        }
        if (popup->combo[i].num < popup->combo[i].max) {
            rect.x = popup->combo[i].urect.x + 2;
            rect.y = popup->combo[i].urect.y + 3;
            rect.w = popup->combo[i].urect.w - 4;
            rect.h = popup->combo[i].urect.h - 6;
            SDL_RenderDrawLine( popup->render, rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h);
            SDL_RenderDrawLine( popup->render, rect.x, rect.y + rect.h, rect.x + (rect.w/2), rect.y);
            SDL_RenderDrawLine( popup->render, rect.x + rect.w, rect.y + rect.h, rect.x + (rect.w/2), rect.y);
         }
    }

    /* Lastly draw the Lamps */
    for(i = 0; i < popup->lamp_ptr; i++) {
        rect.x = popup->lamp[i].col * 15;
        rect.y = 0;
        rect.w = 15;
        rect.h = 15;
        if ((popup->lamp[i].value != 0 && *popup->lamp[i].value & (1 << popup->lamp[i].shift)))
            rect.y = 15;
        SDL_RenderCopy(popup->render, lamps, &rect, & popup->lamp[i].rect);
    }

    SDL_RenderPresent( popup->render );
}

int
findtextpos(struct _text *text, int x)
{
    char     buffer[256];
    int      i;
    int      h;
    int      w = 0;
    int      pos = 0;

    x -= text->rect.x;
    for (pos = 0; text->text[pos] != '\0'; pos++) {
        buffer[pos] = text->text[pos];
        buffer[pos+1] = '\0';
        TTF_SizeText(font14, buffer, &w, &h);
        if (x < w)
           break;
    }
    text->cpos = w;
    text->pos = pos;
    return pos;
}

int
textpos(struct _text *text, int pos)
{
    char     buffer[256];
    int      w;
    int      h;

    if (pos == 0) {
       return 0;
    }
    strncpy(buffer, &text->text[0], pos);
    buffer[pos+1] = '\0';
    TTF_SizeText(font14, buffer, &w, &h);
    return w;
}

void
textcutpaste(struct _text *text, int remove, int insert, int copy)
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
         text->pos = i;
         text->cpos = text->spos;
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
         for (i = 0; i < text->pos && text->text[i] != '\0'; i++)
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
         text->pos = i;
         while (i < sizeof(buffer) && text->text[j] != '\0')
             buffer[i++] = text->text[j++];
         buffer[i++] = '\0';
         /* Set text to new buffer */
         strcpy(text->text, buffer);
         text->sel = 1;
         SDL_free(p);
     }
     /* Update pixel positions */
     text->cpos = textpos(text, text->pos);
     if (text->sel) {
         text->srect.x = textpos(text, text->spos);
         text->srect.w = textpos(text, text->epos) - text->srect.x;
     }
     text->len = strlen(text->text);
     printf("Text update (%s)\n", text->text);
}

void
textinsert(struct _text *text, char *t)
{
     char     buffer[256];
     int      i, j;
     int      w, h;

     /* Remove the selection before inserting new text */
     if (text->sel)
         textcutpaste(text, 1, 0, 0);
     /* Copy up to start of selection */
     for (i = 0; i < text->pos && text->text[i] != '\0'; i++)
          buffer[i] = text->text[i];
     j = i;  /* Save position */
     while (i < sizeof(buffer) && *t != '\0')
         buffer[i++] = *t++;
     text->pos = i;
     buffer[i] = '\0';
     TTF_SizeText(font14, buffer, &w, &h);
     text->cpos = w;
     while (i < sizeof(buffer) && text->text[j] != '\0')
          buffer[i++] = text->text[j++];
     buffer[i++] = '\0';
     /* Set text to new buffer */
     strcpy(text->text, buffer);
     text->len = strlen(text->text);
}

void
textdelete(struct _text *text)
{
     char     buffer[256];
     int      i, j;
     int      w, h;

     /* Remove the selection before inserting new text */
     if (text->sel)
         textcutpaste(text, 1, 0, 0);
     /* Nothing to delete if at beginning of text */
     if (text->pos == 0)
         return;
     if (text->pos > 1) {
         strncpy(buffer, text->text, text->pos - 1);
         buffer[text->pos] = '\0';
         TTF_SizeText(font14, buffer, &w, &h);
         text->cpos = w;
     } else {
         text->cpos = 0;
     }
     strcpy(&buffer[text->pos - 1], &text->text[text->pos]);
     text->pos--;
     /* Set text to new buffer */
     strcpy(text->text, buffer);
     text->len = strlen(text->text);
}

void
setup_fp(int hd, int wd, int h2, int w2)
{
    SDL_Rect rect, rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int      shf;
    int	     i, j, k;
    int      wb;
    int	     hx, wx;
    Uint32   f;

    wb = (strlen(row1) + 1) * (3 * wd);

    /* Draw top of display */
    ADD_AREA(0, 0, 975, 1100, &cc);

    /* Draw bottom switch panel */
    ADD_AREA(0, (66 * h2) - (hd/4), (31 * h2), 1100, &cl);

    /* Draw top box */
    ADD_AREA(10, 10, h2 + 10, wb + 40, &cb);

    ADD_AREA(10, 10 + h2, (h2 * 12) + 10, 10, &cb);

    ADD_AREA(10+(wb+30), 10 + h2, (h2 * 12) + 10, 10, &cb);
    ADD_LABEL(10, 10 + (h2/2), wb, "READ ONLY STORAGE", c, cb)

    /* Draw ROS boxes */
    ADD_AREA(30, 10 + (h2 * 2), h2 * 4, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 3), wb, c);
    rect.x = 50 + wd;
    rect.y = 15 + (h2 * 3);
    rect.h = hd;
    rect.w = wd;
    ros_ptr = 0;
    shf = 23; 
    for (i = 0; row1[i] != '\0'; i++) {
        pos1[i] = rect.x;
        switch (row1[i]) {
        case 'L': j = DIG_LP; pos1[i] += wd/2; break;
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos1[i] = rect.x + wd + (wd/2); goto next_row1;
        }
        ros_bits[ros_ptr].rect.x = pos1[i];
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = hd;
        ros_bits[ros_ptr].rect.w = wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 0;
        if (j == DIG_LP) {
           ros_bits[ros_ptr].rect.x -= wd/2;
           ros_bits[ros_ptr].rect.w = 2*wd;
        }
        shf --;
        ros_ptr++;
next_row1:
        rect.x += 3*wd;
        switch (row1a[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    rect.y = 15 + (h2 * 3);

    ADD_LABEL1(pos1[3] + wd, 10 + (h2 * 2), "CN");
    ADD_LABEL1(pos1[9] - wd, 10 + (h2 * 2), "ADR");
    ADD_LABEL1(pos1[14] - wd, 10 + (h2 * 2), "W REGISTER");
    ADD_LABEL1(pos1[23] + wd, 10 + (h2 * 2), "X REGISTER");
    ADD_MARK(pos1[1] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[7], rect.y, hd*2, c);
    ADD_MARK(pos1[11] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[12] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[14] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[19] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[22] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[26] - wd, rect.y, hd*2, c);
    ADD_MARK(pos1[7], rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos1[11] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos1[12] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos1[19] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);

    /* Second ROS row */
    ADD_AREA(30, 10 +(h2 * 7), h2 * 3, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 8), wb, c);

    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 8);
    rect.h = hd;
    rect.w = wd;
    shf = 26;
    for (i = 0; row2[i] != '\0'; i++) {
        pos2[i] = rect.x;
        switch (row2[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos2[i] = rect.x + wd + (wd/2); rect.x += 3*wd; continue;
        }
        ros_bits[ros_ptr].rect.x = rect.x;
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = hd;
        ros_bits[ros_ptr].rect.w = wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 1;
        ros_ptr++;
        rect.x = pos2[i] + 3*wd;
        shf--;
    }
             /*            1         2           */
             /*  0123456789012345678901234567890 */
             /* "P0123 0123 A0123 0101201AP0123" */
             /* SA CH   CL   CA   CB CMCU  CK */

    ADD_LABEL1(pos2[0] - (wd / 2), 10 + (h2 * 7), "SA");
    ADD_LABEL1(pos2[3] - (wd*2), 10 + (h2 * 7), "CH");
    ADD_LABEL1(pos2[8] - (wd*2), 10 + (h2 * 7), "CL");
    ADD_LABEL1(pos2[13] - wd, 10 + (h2 * 7), "CA");
    ADD_LABEL1(pos2[17] + wd, 10 + (h2 * 7), "CB");
    ADD_LABEL1(pos2[20] - wd, 10 + (h2 * 7), "CM");
    ADD_LABEL1(pos2[22] + wd, 10 + (h2 * 7), "CU");
    ADD_LABEL1(pos2[26] + wd, 10 + (h2 * 7), "CK");

    ADD_MARK(pos2[1] - wd, rect.y, hd, c);
    ADD_MARK(pos2[5] - wd, rect.y, hd, c);
    ADD_MARK(pos2[10] - wd, rect.y, hd, c);
    ADD_MARK(pos2[16],  rect.y, hd, c);
    ADD_MARK(pos2[19] - wd, rect.y, hd, c);
    ADD_MARK(pos2[22] - wd, rect.y, hd, c);
    ADD_MARK(pos2[24] - wd, rect.y, hd, c);
    ADD_MARK(pos2[25] - wd, rect.y, hd, c);
    ADD_MARK(pos2[26] - wd, rect.y, hd, c);

    ADD_MARK(pos2[1] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[5] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[10] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[16], rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[19] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[22] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos2[24] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);

    /* 3rd ROS Row */
    ADD_AREA(30, 10 + (h2 * 11), h2 * 3, wb , &cl);
    ADD_LINE(30, 10 + (h2 * 12), wb, c);

    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 12);
    rect.h = hd;
    rect.w = wd;
    shf = 19;
    for (i = 0; row3[i] != '\0'; i++) {
        pos3[i] = rect.x;
        switch (row3[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos3[i] = rect.x + wd + (wd/2); rect.x += 3*wd; continue;
        }
        ros_bits[ros_ptr].rect.x = rect.x;
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = hd;
        ros_bits[ros_ptr].rect.w = wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 2;
        ros_ptr++;
        rect.x = pos3[i] + 3*wd;
        shf--;
    }

    ADD_LABEL1(pos3[0] - (wd / 2), 10 + (h2 * 11), "CR");
    ADD_LABEL1(pos3[3], 10 + (h2 * 11), "CD");
    ADD_LABEL1(pos3[13] + wd, 10 + (h2 * 11), "CF");
    ADD_LABEL1(pos3[17] + wd, 10 + (h2 * 11), "CG");
    ADD_LABEL1(pos3[20], 10 + (h2 * 11), "CV");
    ADD_LABEL1(pos3[23] - wd, 10 + (h2 * 11), "CC");
    ADD_LABEL1(pos3[27] - wd, 10 + (h2 * 11), "CS");

    ADD_MARK(pos3[1] - wd, rect.y, hd, c);
    ADD_MARK(pos3[6] - wd, rect.y, hd, c);
    ADD_MARK(pos3[11] - wd, rect.y, hd, c);
    ADD_MARK(pos3[16] - wd, rect.y, hd, c);
    ADD_MARK(pos3[19] - wd, rect.y, hd, c);
    ADD_MARK(pos3[22] - wd, rect.y, hd, c);
    ADD_MARK(pos3[25] - wd, rect.y, hd, c);
    ADD_MARK(pos3[1] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[6] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[11] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[16] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[19] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[22] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos3[25] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);

    /* Count register */
    ADD_AREA(30, 10 + (h2 * 15), h2 * 4, wb, &cl);
    /* Draw Count title */
    ADD_LABEL(30, 15 + (h2 * 14) + (h2/2), wb, "COUNT REGISTER", c, cl);
    ADD_LINE(30, 10 + (h2 * 16), wb, c);
    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 16);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    j = 0;
    for (i = 0; row_cnt[i] != '\0'; i++) {
        pos_cnt[i] = rect.x;
        switch (row_cnt[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_cnt[i] = rect.x + wd + (wd/2); rect.x += 3 * wd; continue;
        }
        if (j) 
            (void)add_led(&cpu_2030.GHY, shf, pos_cnt[i], rect.y, hd, wd, j);
        else
            (void)add_led(&cpu_2030.GHZ, shf, pos_cnt[i], rect.y, hd, wd, j);
        shf--;
        if (shf == -1) {
           j++;
           shf = 8;
        }
        rect.x += 3*wd;
        switch (row_cnta[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_cnt[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_MARK(pos_cnt[6] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_cnt[10] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_cnt[14] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_cnt[16] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_cnt[20] - wd, rect.y, hd * 2, c);



    /* Channel number one */
    ADD_AREA(10, 10 + (h2 * 20), h2 + 10, wb + 40, &cb);
    ADD_AREA(10, 10 + (h2 * 21), (h2 * 10), 10, &cb);
    ADD_AREA(10 + (wb + 30), 10 + (h2 * 21), (h2 * 10), 10, &cb);

    /* Channel one title */
    ADD_LABEL(10, 10 + (h2 * 20) + (h2 /2), wb, "CHANNEL NUMBER ONE", c, cb);

    /* Channel one register */
    ADD_AREA(30, 10 + (h2 * 22), h2 * 4, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 23), wb, c);

    rect.x = 20 + wd;
    rect.y = 15 + (h2 * 23);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    k = 0;
    for (i = 0; chan_one[i] != '\0'; i++) {
        pos_chan1[i] = rect.x;
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_chan1[i] = rect.x + wd + (wd/2); rect.x += 3 * wd; continue;
        }
        switch (k) {
        case 0:  (void)add_led(&cpu_2030.GR[0], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        case 1:  (void)add_led(&cpu_2030.GK[0], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        case 2:  (void)add_led(&cpu_2030.GG[0], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        }
        shf--;
        if (shf == -1) {
           k++;
           if (k == 1)
              shf = 4;
           else
              shf = 3;
        }
        rect.x += 3*wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_chan1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_LABEL1(pos_chan1[8] - (wd / 2), 10 + (h2 * 22), "DATA REGISTER");
    ADD_LABEL1(pos_chan1[17] - wd, 10 + (h2 * 22), "KEY");
    ADD_LABEL1(pos_chan1[22], 10 + (h2 * 22), "COMMAND");
    ADD_MARK(pos_chan1[5] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[6] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[10] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[14] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[16] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[20] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[25] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[5] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[14] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[20] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[25] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);

    /* Channel one Status */
    ADD_AREA(30, 10 + (h2 * 26) + (hd/4), h2 * 6, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 27) + (hd/4), wb, c);
    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 28);
    rect.h = hd;
    rect.w = wd;
    pos_chan2[0] = rect.x;
    (void) add_led(&cpu_2030.GF[0], 7, rect.x, rect.y, hd, wd, DIG_CD);
    rect.x += wd * 5;
    pos_chan2[1] = rect.x;
    (void) add_led(&cpu_2030.GF[0], 6, rect.x, rect.y, hd, wd, DIG_CC);
    rect.x += wd * 5;
    pos_chan2[2] = rect.x;
    (void) add_led(&cpu_2030.GF[0], 5, rect.x, rect.y, hd, wd, DIG_SLI);
    rect.x += wd * 5;
    pos_chan2[3] = rect.x;
    (void) add_led(&cpu_2030.GF[0], 4, rect.x, rect.y, hd, wd, DIG_SKP);
    rect.x += wd * 5;
    pos_chan2[4] = rect.x;
    (void) add_led(&cpu_2030.GF[0], 3, rect.x, rect.y, hd, wd, DIG_PCI);
    rect.x += wd * 5;
    pos_chan2[5] = rect.x;
    (void)add_led(&cpu_2030.SEL_TI[0], 7, rect.x, rect.y, hd, wd, DIG_OP);
    rect.x += wd * 6;
    pos_chan2[6] = rect.x;
    (void)add_led(&cpu_2030.SEL_TI[0], 6, rect.x, rect.y, hd, wd, 24);
    rect.x += wd * 6;
    pos_chan2[7] = rect.x;
    (void)add_led(&cpu_2030.SEL_TI[0], 5, rect.x, rect.y, hd, wd, 25);
    rect.x += wd * 6;
    pos_chan2[8] = rect.x;
    (void)add_led(&cpu_2030.SEL_TI[0], 4, rect.x, rect.y, hd, wd, 26);
    rect.x += wd * 6;
    rect.y += hd * 2;
    rect.x = pos_chan2[5];
    (void)add_led(&cpu_2030.SEL_TAGS[0], 15, rect.x, rect.y, hd, wd, 27);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[0], 14, rect.x, rect.y, hd, wd, 28);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[0], 13, rect.x, rect.y, hd, wd, 29);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[0], 12, rect.x, rect.y, hd, wd, 30);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[0], 11, rect.x, rect.y, hd, wd, 31);
    rect.y -= hd * 2;
    rect.x += wd * 6;
    pos_chan2[9] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 7, rect.x, rect.y, hd, wd, 32);
    rect.x += wd * 6;
    pos_chan2[10] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 6, rect.x, rect.y, hd, wd, 33);
    rect.x += wd * 6;
    pos_chan2[11] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 5, rect.x, rect.y, hd, wd, 34);
    rect.x += wd * 6;
    pos_chan2[12] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 4, rect.x, rect.y, hd, wd, 35);
    rect.x += wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 3, rect.x, rect.y, hd, wd, 36);
    rect.x += wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&cpu_2030.GE[0], 2, rect.x, rect.y, hd, wd, 37);
    ADD_LABEL1(pos_chan2[2], 15 + (h2 * 26), "FLAGS");
    ADD_LABEL1(pos_chan2[7], 15 + (h2 * 26), "TAGS");
    ADD_LABEL1(pos_chan2[11], 15 + (h2 * 26), "CHECKS");
    ADD_MARK(pos_chan2[5] - wd, rect.y - (h2/2), hd * 4, c);
    ADD_MARK(pos_chan2[9] - wd, rect.y - (h2/2), hd * 4, c);
    ADD_MARK(pos_chan2[5] - wd, rect.y - (h2 * 2), hd - (h2/4), c);
    ADD_MARK(pos_chan2[9] - wd, rect.y - (h2 * 2), hd - (h2/4), c);

    /* Channel number two */
    ADD_AREA(10, 10 + (h2 * 33), h2 + 10, wb + 40, &cb);
    ADD_AREA(10, 10 + (h2 * 34), (h2 * 10), 10, &cb);
    ADD_AREA(10 + (wb + 30), 10 + (h2 * 34), (h2 * 10), 10, &cb);

    /* Channel two title */
    ADD_LABEL(10, 10 + (h2 * 33) + (h2 /2), wb, "CHANNEL NUMBER TWO", c, cb);
    ADD_AREA(30, 10 + (h2 * 35), (h2 * 4), wb, &cl);
    ADD_LINE(30, 10 + (h2 * 36), wb, c);

    rect.y = 15 + (h2 * 36);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    k = 0;
    for (i = 0; chan_one[i] != '\0'; i++) {
        rect.x = pos_chan1[i];
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': rect.x += 3 * wd; continue;
        }
        switch (k) {
        case 0:  (void)add_led(&cpu_2030.GR[1], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        case 1:  (void)add_led(&cpu_2030.GK[1], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        case 2:  (void)add_led(&cpu_2030.GG[1], shf, pos_chan1[i], rect.y, hd, wd, j); break;
        }
        shf--;
        if (shf == -1) {
           k++;
           if (k == 1)
              shf = 4;
           else
              shf = 3;
        }
        rect.x += 3*wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_chan1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + h2;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_LABEL1(pos_chan1[8] - (wd / 2), 10 + (h2 * 35), "DATA REGISTER");
    ADD_LABEL1(pos_chan1[17] - wd, 10 + (h2 * 35), "KEY");
    ADD_LABEL1(pos_chan1[22], 10 + (h2 * 35), "COMMAND");
    ADD_MARK(pos_chan1[5] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[6] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[10] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[14] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[16] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[20] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[25] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_chan1[5] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[14] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[20] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);
    ADD_MARK(pos_chan1[25] - wd, rect.y - h2 - (h2/4), hd - (h2/4), c);

    /* Channel Status */
    ADD_AREA(30, 10 + (h2 * 39) + (hd/4), h2 * 6, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 40) + (hd/4), wb, c);
    rect.x = 30 + wd;
    rect.y = 10 + (h2 * 41) + (hd/2);
    rect.h = hd;
    rect.w = wd;
    rect.x = pos_chan2[0];
    (void) add_led(&cpu_2030.GF[1], 7, rect.x, rect.y, hd, wd, DIG_CD);
    rect.x += wd * 5;
    (void) add_led(&cpu_2030.GF[1], 6, rect.x, rect.y, hd, wd, DIG_CC);
    rect.x += wd * 5;
    (void) add_led(&cpu_2030.GF[1], 5, rect.x, rect.y, hd, wd, DIG_SLI);
    rect.x += wd * 5;
    (void) add_led(&cpu_2030.GF[1], 4, rect.x, rect.y, hd, wd, DIG_SKP);
    rect.x += wd * 5;
    (void) add_led(&cpu_2030.GF[1], 3, rect.x, rect.y, hd, wd, DIG_PCI);
    rect.x += wd * 5;
    (void)add_led(&cpu_2030.SEL_TI[1], 7, rect.x, rect.y, hd, wd, DIG_OP);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TI[1], 6, rect.x, rect.y, hd, wd, 24);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TI[1], 5, rect.x, rect.y, hd, wd, 25);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TI[1], 4, rect.x, rect.y, hd, wd, 26);
    rect.x += wd * 6;
    rect.x = pos_chan2[5];
    rect.y += hd * 2;
    (void)add_led(&cpu_2030.SEL_TAGS[1], 15, rect.x, rect.y, hd, wd, 27);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[1], 14, rect.x, rect.y, hd, wd, 28);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[1], 13, rect.x, rect.y, hd, wd, 29);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[1], 12, rect.x, rect.y, hd, wd, 30);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.SEL_TAGS[1], 11, rect.x, rect.y, hd, wd, 31);

    rect.y -= hd * 2;
    rect.x += wd * 6;
    rect.x = pos_chan2[9];
    (void)add_led(&cpu_2030.GE[1], 7, rect.x, rect.y, hd, wd, 32);
    rect.x += wd * 6;
    (void)add_led(&cpu_2030.GE[1], 6, rect.x, rect.y, hd, wd, 33);
    rect.x += wd * 6;
    pos_chan2[11] = rect.x;
    (void)add_led(&cpu_2030.GE[1], 5, rect.x, rect.y, hd, wd, 34);
    rect.x += wd * 6;
    pos_chan2[12] = rect.x;
    (void)add_led(&cpu_2030.GE[1], 4, rect.x, rect.y, hd, wd, 35);
    rect.x += wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&cpu_2030.GE[1], 3, rect.x, rect.y, hd, wd, 36);
    rect.x += wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&cpu_2030.GE[1], 2, rect.x, rect.y, hd, wd, 37);
    ADD_LABEL1(pos_chan2[2], 15 + (h2 * 39), "FLAGS");
    ADD_LABEL1(pos_chan2[7], 15 + (h2 * 39), "TAGS");
    ADD_LABEL1(pos_chan2[11], 15 + (h2 * 39), "CHECKS");
    ADD_MARK(pos_chan2[5] - wd, rect.y - (h2/2), hd * 4, c);
    ADD_MARK(pos_chan2[9] - wd, rect.y - (h2/2), hd * 4, c);
    ADD_MARK(pos_chan2[5] - wd, rect.y - (h2 * 2), hd - (h2/4), c);
    ADD_MARK(pos_chan2[9] - wd, rect.y - (h2 * 2), hd - (h2/4), c);

    /* MPX register */
    ADD_AREA(30, 10 + (h2 * 46) + (hd/4), h2 * 4, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 47) + (hd/4), wb, c);
    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 48);
    rect.h = hd;
    rect.w = wd;
    pos_mpx[0] = rect.x;
    (void) add_led(&cpu_2030.MPX_TI, 7, rect.x, rect.y, hd, wd, DIG_OP);  /* OP IN */
    rect.x += wd * 6;
    pos_mpx[1] = rect.x;
    (void) add_led(&cpu_2030.MPX_TI, 6, rect.x, rect.y, hd, wd, 24);   /* ADR IN */
    rect.x += wd * 6;
    pos_mpx[2] = rect.x;
    (void) add_led(&cpu_2030.MPX_TI, 5, rect.x, rect.y, hd, wd, 25);  /* STAT IN */
    rect.x += wd * 6;
    pos_mpx[3] = rect.x;
    (void) add_led(&cpu_2030.MPX_TI, 4, rect.x, rect.y, hd, wd, 26);  /* SERV IN */
    rect.x += wd * 6;
    pos_mpx[4] = rect.x;
    (void) add_led(&cpu_2030.MPX_TAGS, 15, rect.x, rect.y, hd, wd, 27); /* SEL OUT */
    rect.x += wd * 6;
    pos_mpx[5] = rect.x;
    (void)add_led(&cpu_2030.MPX_TAGS, 14, rect.x, rect.y, hd, wd, 28); /* ADR OUT */
    rect.x += wd * 6;
    pos_mpx[6] = rect.x;
    (void)add_led(&cpu_2030.MPX_TAGS, 13, rect.x, rect.y, hd, wd, 29);  /* CMD OUT */
    rect.x += wd * 6;
    pos_mpx[7] = rect.x;
    (void)add_led(&cpu_2030.MPX_TAGS, 12, rect.x, rect.y, hd, wd, 30);  /* SERV OUT */
    rect.x += wd * 6;
    pos_mpx[8] = rect.x;
    (void)add_led(&cpu_2030.MPX_TAGS, 11, rect.x, rect.y, hd, wd, 31);  /* SUP OUT */
    rect.x += wd * 6;
    pos_mpx[9] = rect.x;
    rect.x += wd * 3;
    rect.y = 15 + (h2 * 48) - (hd/2);
    shf = 8;
    for (i = 15; chan_one[i] != '\0'; i++) {
        pos_mpx[i] = rect.x;
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': rect.x += 3 * wd; continue;
        }
        (void)add_led(&cpu_2030.O_REG, shf, pos_mpx[i], rect.y, hd, wd, j);
        shf--;
        rect.x += 3*wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_mpx[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    ADD_LABEL1(pos_mpx[3], 15 + (h2 * 46), "MPX CHANNEL TAGS");
    ADD_LABEL1(pos_mpx[15], 15 + (h2 * 46), "MPX CHANNEL BUS-OUT REGISTER");
    ADD_MARK(pos_mpx[9] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_mpx[9] - wd, rect.y - h2 - (h2/4), hd - (hd/4), c);

    /* Seperator */
    ADD_AREA(30, 15 + (h2 * 50) + 4, hd * 4, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 51) + 4, wb, c);
    rect.x = 30 + wd;
    rect.y = 10 + (h2 * 52) + (hd/4);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    k = 0;
    for (i = 0; store_addr[i] != '\0'; i++) {
        pos_store[i] = rect.x;
        switch (store_addr[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_store[i] = rect.x + wd + (wd/2); rect.x += 3 * wd; continue;
        }
        if (k) 
            (void)add_led(&cpu_2030.N_REG, shf, pos_store[i], rect.y, hd, wd, j);
        else
            (void)add_led(&cpu_2030.M_REG, shf, pos_store[i], rect.y, hd, wd, j);
        shf--;
        if (shf == -1) {
           k++;
           shf = 8;
        }
        rect.x += 3*wd;
        switch (store_addra[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_store[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    shf--;
    (void)add_led(&store, 0, rect.x + wd, 10 + rect.y - hd, hd, wd, 38);
    shf--;
    (void)add_led(&store, 1, rect.x + wd, 10 + rect.y + hd - 5, hd, wd, 39);
    ADD_LABEL1(pos_store[11] - wd, 15 + (h2 * 50) + 4, "MAIN STORAGE ADDRESS REGISTER");
    ADD_MARK(pos_store[1] - wd, rect.y + (hd/4), hd * 2, c);
    ADD_MARK(pos_store[5] - wd, rect.y + (hd/4), hd * 2, c);
    ADD_MARK(pos_store[9] - wd, rect.y - h2, hd * 3, c);
    ADD_MARK(pos_store[11] - wd, rect.y + (hd/4), hd * 2, c);
    ADD_MARK(pos_store[15] - wd, rect.y + (hd/4), hd * 2, c);
    ADD_MARK(pos_store[19] - wd, rect.y + (hd/4), hd * 2, c);

    /* Seperator */
    ADD_AREA(30, 10 + (h2 * 55), h2 * 4, wd * 59, &cl);
    ADD_LINE(30, 15 + (h2 * 56), wd * 59, c);
    rect.x = 40 + wd;
    rect.y = 10 + (h2 * 57) + (hd/2);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    k = 0;
    for (i = 0; data_reg[i] != '\0'; i++) {
        pos_data[i] = rect.x;
        switch (data_reg[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_data[i] = rect.x + wd + (wd/2); rect.x += 3*wd; continue;
        }
        if (k)
            (void)add_led(&cpu_2030.Alu_out, shf, pos_data[i], rect.y, hd, wd, j);
        else
            (void)add_led(&cpu_2030.R_REG, shf, pos_data[i], rect.y, hd, wd, j);
        rect.x = pos_data[i] + 3*wd;
        shf--;
        if (shf == -1) {
           shf = 7;
           k++;
        }
    }
    ADD_LABEL1(pos_data[0] + wd, 15 + (h2 * 55), "MAIN STORAGE DATA REGISTER");
    ADD_LABEL1(pos_data[13] - wd, 15 + (h2 * 55), "ALU OUTPUT");
    ADD_MARK(pos_data[1] - wd, rect.y, hd, c);
    ADD_MARK(pos_data[5] - wd, rect.y, hd, c);
    ADD_MARK(pos_data[9] - wd, rect.y - (h2 * 2) - (h2/4), hd, c);
    ADD_MARK(pos_data[9] - wd, rect.y - h2, hd * 2, c);
    ADD_MARK(pos_data[11] - wd, rect.y, hd, c);
    ADD_MARK(pos_data[15] - wd, rect.y, hd, c);
    ADD_AREA(30, 10 + (h2 * 60) - (hd/2), (h2 * 4) + hd, wd * 59, &cl);
    ADD_LINE(30, 15 + (h2 * 61), wd * 59, c);
    rect.x = 30 + wd;
    rect.y = 15 + (h2 * 62);
    rect.h = hd;
    rect.w = wd;
    shf = 8;
    k = 0;
    for (i = 0; store_addr[i] != '\0'; i++) {
        pos_breg[i] = rect.x;
        switch (store_addr[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_breg[i] = rect.x + wd + (wd/2); rect.x += 3 * wd; continue;
        }
        if (k) 
            (void)add_led(&cpu_2030.Bbus, shf, pos_breg[i], rect.y, hd, wd, j);
        else
            (void)add_led(&cpu_2030.Abus, shf, pos_breg[i], rect.y, hd, wd, j);
        shf--;
        if (shf == -1) {
           shf = 8;
           k++;
        }
        rect.x += 3*wd;
        switch (store_addra[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_breg[i];
        ctl_label[ctl_ptr].rect.y = rect.y + hd;
        ctl_label[ctl_ptr].rect.h = hd;
        ctl_label[ctl_ptr].rect.w = wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    ADD_LABEL1(pos_breg[3] + wd, 15 + (h2 * 60) - (hd/2), "B REGISTER");
    ADD_LABEL1(pos_breg[13] - wd, 15 + (h2 * 60) - (hd/2), "A REGISTER");
    ADD_MARK(pos_breg[1] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_breg[5] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_breg[9] - wd, rect.y - (h2 * 2) - (h2/4), hd, c);
    ADD_MARK(pos_breg[9] - wd, rect.y - h2, hd * 3, c);
    ADD_MARK(pos_breg[11] - wd, rect.y, hd * 2, c);
    ADD_MARK(pos_breg[15] - wd, rect.y, hd * 2, c);

    rect.x = 40 + pos_data[17];
    rect.y = 10 + (h2 * 55);
    rect.h = (h2 * 9) + (h2/2);
    rect.w = (30 + wb) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &cl);
    rect.y = 15 + (h2 * 56);
    ADD_LINE(rect.x, 15 + (h2 * 56), rect.w, c);
    ADD_LABEL1(rect.x + (wd * 3), 15 + (h2 * 55), "CPU STATUS");
    rect.x += wd;
    pos_data[18] = rect.x;
    rect.y += h2;       /* EX */
    (void)add_led(0, 7, rect.x, rect.y, hd, wd, 40);
    rect.x += (wd * 4); /* MATCH */
    (void)add_led(&match, 0, rect.x, rect.y, hd, wd, 41);
    rect.x += (wd * 7);  /* ALLOW WRITE */
    (void)add_led(&allow_write, 0, rect.x, rect.y, hd, wd, 42);
    rect.y += 2*h2 + (hd/2); /* 1050 Intv */
    rect.x = pos_data[18];
    (void)add_led(0, 4, rect.x, rect.y, hd, wd, 43);
    rect.x += (wd * 4);
    rect.x += (wd * 7);    /* 1050 req */
    (void)add_led(0, 3, rect.x, rect.y, hd, wd, 44);
    rect.y += 2*h2 + (hd/2);
    rect.x = pos_data[18];  /* MPX CHNL */
    (void)add_led(0, 2, rect.x, rect.y, hd, wd, 45);
    rect.x += (wd * 4);     /* SELCH */
    (void)add_led(0, 1, rect.x, rect.y, hd, wd, 46);
    rect.x += (wd * 7);    /* Compute */
    (void)add_led(0, 0, rect.x, rect.y, hd, wd, 47);
    rect.x += (wd * 8);
    pos_data[19] = rect.x;
    rect.y = 15 + (h2 * 57);
    ADD_LABEL1(rect.x + (wd*2), 15 + (h2 * 55), "CPU CHECKS");
    ADD_MARK(rect.x - wd, 15 + (h2 * 55), (hd * 8) + (hd/2), c);
    /* STORE ADR */
    (void)add_led(&cpu_2030.MC_REG, 5, rect.x, rect.y, hd, wd, 48);
    rect.x += wd * 4;
    /* STORE DATA */
    (void)add_led(&cpu_2030.MC_REG, 1, rect.x, rect.y, hd, wd, 49);
    rect.x = pos_data[19];
    rect.y += 2*h2 + (hd/2);
    /* B reg */
    (void)add_led(&cpu_2030.MC_REG, 7, rect.x, rect.y, hd, wd, 50);
    rect.x += wd * 4;
    /* A reg */
    (void)add_led(&cpu_2030.MC_REG, 6, rect.x, rect.y, hd, wd, 51);
    rect.x += wd * 4;
    /* ALU */
    (void)add_led(&cpu_2030.MC_REG, 0, rect.x, rect.y, hd, wd, 52);
    rect.x = pos_data[19];
    rect.y += 2*h2 + (hd/2);
    /* ROS ADDR */
    (void)add_led(&cpu_2030.MC_REG, 2, rect.x, rect.y, hd, wd, 53);
    rect.x += wd * 4;
    /* ROS SALS */
    (void)add_led(&cpu_2030.MC_REG, 3, rect.x, rect.y, hd, wd, 54);
    rect.x += wd * 4;
    /* CTL REG */
    (void)add_led(&cpu_2030.MC_REG, 4, rect.x, rect.y, hd, wd, 55);

    add_switch(&SYS_RST, 10, (h2 * 67), wd * 10, hd * 2, SW, &c3, &sw_labels[0], font1);
    add_switch(&ROAR_RST,10, (h2 * 70), wd * 10, hd * 2, SW, &c3, &sw_labels[1], font1);
    add_switch(NULL, 10, (h2 * 73), wd * 10, hd * 2, SW, &c, NULL, NULL);
    add_switch(&START, 10, (h2 * 76), wd * 10, hd * 2, SW, &c2, &sw_labels[2], font1);
    add_switch(NULL, 85, (h2 * 67), wd * 10, hd * 2, SW, &c, NULL, font1);
    add_switch(&SET_IC, 85, (h2 * 70), wd * 10, hd * 2, SW, &c3, &sw_labels[3], font1);
    add_switch(&CHECK_RST, 85, (h2 * 73), wd * 10, hd * 2, SW, &c3, &sw_labels[4], font1);
    add_switch(&STOP, 85, (h2 * 76), wd * 10, hd * 2, SW, &c5, &sw_labels[5], font1);
    add_switch(&INT_TMR, 160, (h2 * 67), wd * 10, hd * 2, ONOFF, &c3, &sw_labels[6], font1);
    add_switch(&STORE, 160, (h2 * 70), wd * 10, hd * 2, SW, &c3, &sw_labels[7], font1);
    add_switch(&LAMP_TEST, 160, (h2 * 73), wd * 10, hd * 2, SW, &c3, &sw_labels[8], font1);
    add_switch(&DISPLAY, 160, (h2 * 76), wd * 10, hd * 2, SW, &c3, &sw_labels[9], font1);
    add_switch(NULL, 780, (h2 * 66), wd * 10, hd * 2, SW, &c, &sw_labels[10], font1);
    add_switch(&POWER, 1000, (h2 * 66), wd * 10, hd * 2, SW, &c5, &sw_labels[11], font1);
    add_switch(&INTR, 780, (h2 * 79), wd * 10, hd * 2, SW, &c5, &sw_labels[12], font1);
    add_switch(&LOAD, 1000, (h2 * 79), wd * 10, hd * 2, SW, &c3, &sw_labels[13], font1);

    ADD_LABEL(620,(h2 * 34), (wd * 40), "ROS CONTROL", c1, cc);
    ADD_LABEL(620 + (wd * 4), (h2 * 36), wd*6, "INHBIT", c1, cc);
    ADD_LABEL(620 + (wd * 4), (h2 * 37), wd * 7, "CF STOP", c1, cc);
    ADD_LABEL(620, (h2 * 36), wd * 40, "PROCESS", c1, cc);
    ADD_LABEL(620 + (wd * 30), (h2 * 36), wd * 3, "ROS", c1, cc);
    ADD_LABEL(620 + (wd * 30), (h2 * 37), wd*4, "SCAN", c1, cc);
    ADD_LINE(620 + (wd * 11), (h2 * 37), (wd * 3), c1);
    ADD_LINE(620 + (wd * 26)+1, (h2 * 37), (wd * 3) - 1, c1);
    dial[0].boxd.x = 620 + (wd * 10);
    dial[0].boxd.y = h2 * 36;
    dial[0].boxd.w = (wd * 20);
    dial[0].boxd.h = h2 * 5;
    dial[0].boxu.x = 620 + (wd * 20);
    dial[0].boxu.y = h2 * 36;
    dial[0].boxu.w = (wd * 20);
    dial[0].boxu.h = h2 * 5;
    dial[0].center_x = 620 + (wd * 20);
    dial[0].center_y = (h2 * 40);
    dial[0].pos_x[0] = 620 + (wd * 14);
    dial[0].pos_x[1] = 620 + (wd * 20);
    dial[0].pos_x[2] = 620 + (wd * 26);
    dial[0].pos_y[0] = h2 * 37;
    dial[0].pos_y[1] = h2 * 37;
    dial[0].pos_y[2] = h2 * 37;
    dial[0].init = 1;
    dial[0].value = &PROC_SW;
    dial[0].max = 2;
    dial[0].wrap = 0;

    ADD_LABEL(900, (h2 * 34), (wd * 23), "RATE", c1, cc);
    ADD_LABEL(900, (h2 * 36), (wd * 5), "INSTR", c1, cc);
    ADD_LABEL(900, (h2 * 37), (wd * 4), "STEP", c1, cc);
    ADD_LABEL(900, (h2 * 36), wd * 23, "PROCESS", c1, cc);
    ADD_LABEL(900 + (wd * 21), (h2 * 36), wd * 5, "SINGLE", c1, cc);
    ADD_LABEL(900 + (wd * 21), (h2 * 37), wd * 5, "CYCLE", c1, cc);
    ADD_LINE(900 + (wd * 5), (h2 * 37), (wd * 2), c1);
    ADD_LINE(900 + (wd * 16), (h2 * 37), (wd * 3), c1);
    dial[1].boxd.x = 900;
    dial[1].boxd.y = h2 * 36;
    dial[1].boxd.w = (wd * 12);
    dial[1].boxd.h = h2 * 5;
    dial[1].boxu.x = 900 + (wd * 12);
    dial[1].boxu.y = h2 * 36;
    dial[1].boxu.w = (wd * 12);
    dial[1].boxu.h = h2 * 5;
    dial[1].center_x = 900 + (wd * 12);
    dial[1].center_y = (h2 * 40);
    dial[1].pos_x[0] = 900 + (wd * 7);
    dial[1].pos_x[1] = 900 + (wd * 12);
    dial[1].pos_x[2] = 900 + (wd * 16);
    dial[1].pos_y[0] = h2 * 37;
    dial[1].pos_y[1] = h2 * 37;
    dial[1].pos_y[2] = h2 * 37;
    dial[1].init = 1;
    dial[1].value = &RATE_SW;
    dial[1].max = 2;
    dial[1].wrap = 0;

    ADD_LABEL(620, (h2 * 46), (wd * 40), "ADDRESS COMPARE", c1, cc);
    ADD_LABEL(620, (h2 * 48) - (hd/2), wd*40, "PROCESS", c1, cc);
    ADD_LABEL3(620, (h2 * 48), "ROAR SYNC", c1, cc);
    ADD_LINE(620 + (wd * 10), (h2 * 49), (wd * 5), c1);
    ADD_LABEL3(620, (h2 * 50), "ROAR STOP", c1, cc);
    ADD_LINE(620 + (wd * 10), (h2 * 51), (wd * 4), c1);
    ADD_LABEL3(620, (h2 * 52), "EARLY ROAR", c1, cc);
    ADD_LINE(620 + (wd * 10), (h2 * 53), (wd * 4), c1);
    ADD_LABEL3(620, (h2 * 53), "STOP", c1, cc);
    ADD_LABEL3(620, (h2 * 55), "ROAR RESTART", c1, cc);
    ADD_LABEL3(620, (h2 * 56), "WITHOUT RESET", c1, cc);
    ADD_LABEL(620, (h2 * 56), wd*40, "ROAR", c1, cc);
    ADD_LABEL(620, (h2 * 57), wd*40, "RESTART", c1, cc);
    ADD_LABEL3(620 + (wd * 29), (h2 * 48), "SAR DELAYED", c1, cc);
    ADD_LINE(620 + (wd * 26) + 1, (h2 * 49), (wd * 2), c1);
    ADD_LABEL3(620 + (wd * 29), (h2 * 49), "STOP", c1, cc);
    ADD_LABEL3(620 + (wd * 32), (h2 * 51) - (hd/2), "SAR STOP", c1, cc);
    ADD_LINE(620 + (wd * 26) + 1, (h2 * 51), (wd * 4), c1);
    ADD_LABEL3(620 + (wd * 29), (h2 * 53) - (hd/2), "SAR RESTART", c1, cc);
    ADD_LINE(620 + (wd * 26) + 1, (h2 * 53), (wd * 2), c1);
    ADD_LABEL3(620 + (wd * 28), (h2 * 55), "ROAR RESTART", c1, cc);
    ADD_LABEL3(620 + (wd * 28), (h2 * 56), "STORE BYPASS", c1, cc);
    dial[2].boxd.x = 620 + (wd * 10);
    dial[2].boxd.y = h2 * 48;
    dial[2].boxd.w = (wd * 10);
    dial[2].boxd.h = h2 * 5;
    dial[2].boxu.x = 620 + (wd * 20);
    dial[2].boxu.y = h2 * 48;
    dial[2].boxu.w = (wd * 10);
    dial[2].boxu.h = h2 * 5;
    dial[2].center_x = 620 + (wd * 20);
    dial[2].center_y = (h2 * 52);
    dial[2].pos_x[0] = 620 + (wd * 20);
    dial[2].pos_x[1] = 620 + (wd * 26);
    dial[2].pos_x[2] = 620 + (wd * 26);
    dial[2].pos_x[3] = 620 + (wd * 26);
    dial[2].pos_x[4] = 620 + (wd * 26);
    dial[2].pos_x[5] = 620 + (wd * 20);
    dial[2].pos_x[6] = 620 + (wd * 14);
    dial[2].pos_x[7] = 620 + (wd * 14);
    dial[2].pos_x[8] = 620 + (wd * 14);
    dial[2].pos_x[9] = 620 + (wd * 15);
    dial[2].pos_y[0] = h2 * 49;
    dial[2].pos_y[1] = (h2 * 49);
    dial[2].pos_y[2] = (h2 * 51);
    dial[2].pos_y[3] = (h2 * 53);
    dial[2].pos_y[4] = (h2 * 55);
    dial[2].pos_y[5] = (h2 * 56);
    dial[2].pos_y[6] = (h2 * 56);
    dial[2].pos_y[7] = (h2 * 53);
    dial[2].pos_y[8] = (h2 * 51);
    dial[2].pos_y[9] = h2 * 49;
    dial[2].init = 0;
    dial[2].value = &MATCH_SW;
    dial[2].max = 9;
    dial[2].wrap = 1;

    ADD_LABEL(900, (h2 * 46), wd*23, "CHECK CONTROL", c1, cc);
    ADD_LABEL3(900 - (wd *5), (h2 * 48) + (hd/2), "DISABLE", c1, cc);
    ADD_LINE(900 + (wd * 3), (h2 * 49), (wd * 4), c1);
    ADD_LABEL3(900 - (wd*5), (h2 * 50) + (hd/2), "DIAGNOSTIC", c1, cc);
    ADD_LABEL(900, (h2 * 48) - (hd/2), wd*23, "PROCESS", c1, cc);
    ADD_LABEL(900 + (wd * 24), (h2 * 48) + (hd/2), wd*4, "STOP", c1, cc);
    ADD_LINE(900 + (wd * 16), (h2 * 49), (wd * 6), c1);
    ADD_LABEL(900 + (wd * 21), (h2 * 50) + (hd/2), wd*7, "RESTART", c1, cc);
    dial[3].boxd.x = 900;
    dial[3].boxd.y = h2 * 48;
    dial[3].boxd.w = (wd * 12);
    dial[3].boxd.h = h2 * 5;
    dial[3].boxu.x = 900 + (wd * 12);
    dial[3].boxu.y = h2 * 48;
    dial[3].boxu.w = (wd * 12);
    dial[3].boxu.h = h2 * 5;
    dial[3].center_x = 900 + (wd * 12);
    dial[3].center_y = (h2 * 52);
    dial[3].pos_x[0] = 900 + (wd * 6);
    dial[3].pos_x[1] = 900 + (wd * 7);
    dial[3].pos_x[2] = 900 + (wd * 12);
    dial[3].pos_x[3] = 900 + (wd * 16);
    dial[3].pos_x[4] = 900 + (wd * 18);
    dial[3].pos_y[0] = h2 * 51;
    dial[3].pos_y[1] = (h2 * 49);
    dial[3].pos_y[2] = (h2 * 49);
    dial[3].pos_y[3] = (h2 * 49);
    dial[3].pos_y[4] = (h2 * 51);
    dial[3].init = 2;
    dial[3].value = &CHK_SW;
    dial[3].max = 4;
    dial[3].wrap = 0;
    PROC_SW = dial[0].init;
    RATE_SW = dial[1].init;
    MATCH_SW = dial[2].init;
    CHK_SW = dial[3].init;

    rect.x = 250;
    for (i = 0; i < 8; i++ ) {
        hex_dial[i].rect.x = rect.x + wd;
        hex_dial[i].rect.y = (h2 * 73);
        hex_dial[i].rect.w = 64;
        hex_dial[i].rect.h = 64;
        hex_dial[i].boxu.x = rect.x + wd;
        hex_dial[i].boxu.y = (h2 * 73);
        hex_dial[i].boxu.w = 32;
        hex_dial[i].boxu.h = 64;
        hex_dial[i].boxd.x = rect.x + wd + 32;
        hex_dial[i].boxd.y = (h2 * 73);
        hex_dial[i].boxd.w = 32;
        hex_dial[i].boxd.h = 64;
        if (i == 3) {
            rect.x += (wd * 15);
        }
        hex_ptr++;
        rect.x += (wd * 15);
    }

    rect.x = 250 + (wd * 58);
    store_dial[0].rect.x = rect.x + wd;
    store_dial[0].rect.y = (h2 * 73);
    store_dial[0].rect.w = 80;
    store_dial[0].rect.h = 80;
    store_dial[0].boxu.x = rect.x;
    store_dial[0].boxu.y = (h2 * 73);
    store_dial[0].boxu.w = 40;
    store_dial[0].boxu.h = 80;
    store_dial[0].boxd.x = rect.x + 32;
    store_dial[0].boxd.y = (h2 * 73);
    store_dial[0].boxd.w = 40;
    store_dial[0].boxd.h = 80;
    store_ptr = 1;

    hex_dial[0].digit = &A_SW;
    hex_dial[1].digit = &B_SW;
    hex_dial[2].digit = &C_SW;
    hex_dial[3].digit = &D_SW;
    hex_dial[4].digit = &F_SW;
    hex_dial[5].digit = &G_SW;
    hex_dial[6].digit = &H_SW;
    hex_dial[7].digit = &J_SW;
    store_dial[0].digit = &E_SW;
    store_dial[0].sel = 0;
    *store_dial[0].digit = 0;

    rect.x= hex_dial[0].boxu.x;
    rect.y=h2 * 69 - (h2 / 2);
    rect.h=h2;
    rect.w= (hex_dial[3].boxd.x + hex_dial[3].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "COMPARE ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + h2/3, 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + h2/3, 2, &c);

    rect.x= hex_dial[0].boxu.x;
    rect.y=h2 * 70;
    rect.h=h2;
    rect.w= (hex_dial[3].boxd.x + hex_dial[3].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "MAIN STORAGE ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (3 * hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (3 * hd), 2, &c);

    rect.x= store_dial[0].rect.x;
    rect.y=h2 * 69 - (h2 / 2);
    rect.h=h2;
    rect.w= store_dial[0].rect.w;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL3(rect.x + (wd/2), rect.y, "DISPLAY STOR SEL", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (5*hd), 2, &c);
    ADD_AREA(rect.x + rect.w,  rect.y, h2 + (5*hd), 2, &c);

    rect.x= hex_dial[4].boxu.x;
    rect.y=h2 * 69 - (h2/2);
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "INSTRUCTION ADDRESS - ROS ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (5 * hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (h2 / 3), 2, &c);
    rect.x= hex_dial[5].boxu.x;
    rect.y=h2 * 70;
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "LOAD UNIT", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (3 * hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (h2 / 3), 2, &c);

    rect.x= hex_dial[6].boxu.x;
    rect.y=h2 * 71 + (h2/2);
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "DATA", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + hd, 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + hd, 2, &c);
    ADD_LABEL2(790+(wd * 10), (h2 * 78) + (h2/2), "SYS");
    ADD_LABEL2(790+(wd * 15), (h2 * 78) + (h2/2), "MAN");
    ADD_LABEL2(790+(wd * 20), (h2 * 78) + (h2/2), "WAIT");
    ADD_LABEL2(790+(wd * 25), (h2 * 78) + (h2/2), "TEST");
    ADD_LABEL2(790+(wd * 30), (h2 * 78) + (h2/2), "LOAD");

    /* Add in status lights */
    lamp[0].rect.x = 790 + (wd * 10);
    lamp[0].rect.y = h2 * 79 + (h2/2);
    lamp[0].rect.h = 15;
    lamp[0].rect.w = 15;
    lamp[0].col = 0;
    lamp[0].value = &clock_start_lch;
    lamp[0].shift = 0;
    lamp[1].rect.x = 790 + (wd * 15);
    lamp[1].rect.y = h2 * 79 + (h2/2);
    lamp[1].rect.h = 15;
    lamp[1].rect.w = 15;
    lamp[1].col = 0;
    lamp[1].value = &allow_man_operation;
    lamp[1].shift = 0;
    lamp[2].rect.x = 790 + (wd * 20);
    lamp[2].rect.y = h2 * 79 + (h2/2);
    lamp[2].rect.h = 15;
    lamp[2].rect.w = 15;
    lamp[2].col = 0;
    lamp[2].value = &wait;
    lamp[2].shift = 0;
    lamp[3].rect.x = 790 + (wd * 25);
    lamp[3].rect.y = h2 * 79 + (h2/2);
    lamp[3].rect.h = 15;
    lamp[3].rect.w = 15;
    lamp[3].col = 1;
    lamp[3].value = &test_mode;
    lamp[3].shift = 0;
    lamp[4].rect.x = 790 + (wd * 30);
    lamp[4].rect.y = h2 * 79 + (h2/2);
    lamp[4].rect.h = 15;
    lamp[4].rect.w = 15;
    lamp[4].col = 0;
    lamp[4].value = &cpu_2030.FT;
    lamp[4].shift = 3;
    lamp_ptr = 5;
}

uint32_t
timer_callback(uint32_t interval, void *param)
{
    SDL_Event     event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;
    timer_event = 1;

    SDL_PushEvent(&event);
    return interval;
}

int main(int argc, char *argv[]) {
    SDL_Rect rect, rect2;
    SDL_Thread *thrd;
    SDL_TimerID  disp_timer;
    SDL_Event event;
    SDL_Surface *text;
    SDL_Texture *txt;
    struct _popup   *pop_wind = NULL;
    Uint32	f;
    int      shf;
    int	     i, j, k;
    int	     h, w;
    int	     h2, w2;
    int      hd, wd;
    int      wb;
    char     buffer[4];
    int      mWindowID = -1;
    int      mDeviceID = -1;
    int      mPopID = -1;

    /* Start SDL */
    SDL_Init( SDL_INIT_EVERYTHING );
    TTF_Init();
    POWER = 1;
    SYS_RST = 1;  /* Force system reset */

    /* Set up screen */
    screen = SDL_CreateWindow("IBM360/30",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1100, 975, SDL_WINDOW_RESIZABLE ); 
    render = SDL_CreateRenderer( screen, -1, SDL_RENDERER_ACCELERATED);
    screen2 = SDL_CreateWindow("Devices",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1000, 1000, SDL_WINDOW_RESIZABLE ); 
    render2 = SDL_CreateRenderer( screen2, -1, SDL_RENDERER_ACCELERATED);

    /* Create fonts */
    font1 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 9);
    font12 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 12);
    font14 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 14);
    /* Load in base images for CPU */
    text = IMG_ReadXPMFromArray(lamps_img);
    lamps = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    text = IMG_ReadXPMFromArray(hex_dial_img);
    hex_dials = SDL_CreateTextureFromSurface( render, text);
    SDL_SetTextureBlendMode(hex_dials, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(text);
    text = IMG_ReadXPMFromArray(store_dials_img);
    store_dials[0] = SDL_CreateTextureFromSurface( render, text);
    SDL_SetTextureBlendMode(store_dials[0], SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(text);

    hd = 0;
    wd = 0;
    j = 0;

    /* Create textures for labels and buttons */
    for (i = 0; labels[i].upper != NULL; i++) {
       if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
          text = TTF_RenderText_Shaded(font1, labels[i].upper, c5o, cl);
       } else {
          text = TTF_RenderText_Shaded(font1, labels[i].upper, cof, cl);
       }
       digit_off[i] = SDL_CreateTextureFromSurface( render, text);
       SDL_FreeSurface(text);
       if (labels[i].lower != NULL) {
           if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, c5o, cl);
           } else {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, cof, cl);
           }
           digit2_off[i] = SDL_CreateTextureFromSurface( render, text);
           SDL_FreeSurface(text);
       } else {
           digit2_off[i] = NULL;
       }
       if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
           text = TTF_RenderText_Shaded(font1, labels[i].upper, c5, cl);
       } else {
           text = TTF_RenderText_Shaded(font1, labels[i].upper, con, cl);
       }
       digit_on[i] = SDL_CreateTextureFromSurface( render, text);
       SDL_FreeSurface(text);
       if (labels[i].lower != NULL) {
           if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, c5, cl);
           } else {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, con, cl);
           }
           digit2_on[i] = SDL_CreateTextureFromSurface( render, text);
           SDL_FreeSurface(text);
       } else {
           digit2_on[i] = NULL;
       }
       if (strlen(labels[i].upper) == 1) {
           SDL_QueryTexture(digit_on[i], &f, &k, &w, &h);
           if (w > wd)
              wd = w;
           if (h > hd)
              hd = h;
       }
    }

    text = TTF_RenderText_Shaded(font1, "M", c, cb);
    txt = SDL_CreateTextureFromSurface( render, text);
    SDL_QueryTexture(txt, &f, &k, &w2, &h2);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "ON", c1, c);
    on = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "OFF", c1, c);
    off = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);

    setup_fp(hd, wd, h2, w2);

    (void)model1443_init(render2, 0x00b);
    (void)model1442_init(render2, 0x00a);
    (void)model2415_init(render2, 0x0c0);
    model1050_init();
    thrd = SDL_CreateThread(process, "CPU", NULL);
    disp_timer = SDL_AddTimer(20, &timer_callback, NULL);
    mWindowID = SDL_GetWindowID( screen );
    mDeviceID = SDL_GetWindowID( screen2 );
    while(POWER) {
        while(SDL_PollEvent(&event)) {
           if (event.window.windowID == mWindowID) {
               switch(event.type) {
               case SDL_MOUSEBUTTONDOWN:
                    for (i = 0; i < sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, sws[i].rect)) {
                             if (i == 8) {
                                 *sws[i].value = !*sws[i].value;
                             } else if (sws[i].value != 0) {
                                 *sws[i].value = 1;
                             }
                             sws[i].active = 1;
                        }
                    }
                    for (i = 0; i < 4; i++) {
                        if (inrect(event.button.x, event.button.y, dial[i].boxd)) {
                            if (*(dial[i].value) == 0 && dial[i].wrap) {
                                *(dial[i].value) = dial[i].max;
                            } else if (*(dial[i].value) > 0) {
                                *dial[i].value -= 1;
                            }
                        }
                        if (inrect(event.button.x, event.button.y, dial[i].boxu)) {
                            if (*(dial[i].value) == dial[i].max && dial[i].wrap) {
                                *(dial[i].value) = 0;
                            } else if (*(dial[i].value) < dial[i].max) {
                                *dial[i].value += 1;
                            }
                        }
                    }
                    for (i = 0; i < hex_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, hex_dial[i].boxd)) {
                            *hex_dial[i].digit = ((*hex_dial[i].digit - 1) & 0xf);
                        }
                        if (inrect(event.button.x, event.button.y, hex_dial[i].boxu)) {
                            *hex_dial[i].digit = ((*hex_dial[i].digit + 1) & 0xf);
                        }
                    }
                    for (i = 0; i < store_ptr; i++) {
                        if (event.button.x > (store_dial[i].rect.x + 30) &&
                            event.button.x < (store_dial[i].rect.x + 50) &&
                            event.button.y > (store_dial[i].rect.y + 30) &&
                            event.button.y < (store_dial[i].rect.y + 50)) {
                            store_dial[i].sel = ((store_dial[i].sel + 1) & 0x3);
                            *store_dial[i].digit &= 0xf;
                            if (store_dial[i].sel == 3) {
                                *store_dial[i].digit |= 0x20;
                            } else {
                                *store_dial[i].digit |= (store_dial[i].sel + 1) << 4;
                            }
                        } else 
                        if (inrect(event.button.x, event.button.y, store_dial[i].boxd)) {
                            *store_dial[i].digit = (*store_dial[i].digit & 0xf0) | 
                                      ((*store_dial[i].digit - 1) & 0xf);
                        } else
                        if (inrect(event.button.x, event.button.y, store_dial[i].boxu)) {
                            *store_dial[i].digit = (*store_dial[i].digit & 0xf0) | 
                                      ((*store_dial[i].digit + 1) & 0xf);
                        }
                    }
                    break;
               case SDL_MOUSEBUTTONUP:
                    for (i = 0; i < sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, sws[i].rect)) {
                             if (i != 8 && sws[i].value != 0) {
                                 *sws[i].value = 0;
                             }
                             sws[i].active = 0;
                        }
                    }
                    break;
               default:
                    break;
               }
            }
            if (event.window.windowID == mDeviceID) {
               switch(event.type) {
               case SDL_MOUSEBUTTONDOWN:
                    printf("Dev %d %d\n", event.button.x, event.button.y);
                    if (pop_wind != NULL)
                        break;
                    for (i = 0; i < sizeof(chan)/sizeof(struct _device *); i++) {
                        struct _device *dev;
                        for (dev = chan[i]; dev != NULL; dev = dev->next) {
                             for (j = 0; j < dev->n_units; j++) {
                                 if (inrect(event.button.x, event.button.y, dev->rect[j])) {
                                     pop_wind = dev->create_ctrl(dev, hd, wd, j);
                                     mPopID = SDL_GetWindowID( pop_wind->screen );
                                     break;
                                 }
                             }
                        }
                        if (pop_wind != NULL)
                            break;
                    }
                    break;

               default:
                    break;
               }
            }
            if (event.window.windowID == mPopID) {
               switch(event.type) {
               case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
printf("Close\n");
                          /* Draw all labels */
                          for (i = 0; i < pop_wind->ctl_ptr; i++) {
                              SDL_DestroyTexture(pop_wind->ctl_label[i].text);
                          }
                         
                          /* Draw push buttons */
                          for (i = 0; i < pop_wind->sws_ptr; i++) {
                              if (pop_wind->sws[i].top != NULL) {
                                 SDL_DestroyTexture(pop_wind->sws[i].top);
                              }
                              if (pop_wind->sws[i].bot != NULL) {
                                 SDL_DestroyTexture(pop_wind->sws[i].bot);
                              }
                          }
                         
                          /* Draw indicator lights */
                          for (i = 0; i < pop_wind->ind_ptr; i++) {
                              if (pop_wind->ind[i].top != NULL) {
                                 SDL_DestroyTexture(pop_wind->ind[i].top);
                              }
                              if (pop_wind->ind[i].bot != NULL) {
                                 SDL_DestroyTexture(pop_wind->ind[i].bot);
                              }
                          }
                          SDL_DestroyRenderer(pop_wind->render);
                          SDL_DestroyWindow(pop_wind->screen);
                          free(pop_wind);
                          pop_wind = NULL;
                          break;
                    }
                    break;
               case SDL_QUIT:
                   printf("Quit\n");
                    break;
               case SDL_MOUSEBUTTONDOWN:
                    for (i = 0; i < pop_wind->sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->sws[i].rect)) {
                            if (pop_wind->sws[i].value != 0) {
                                *pop_wind->sws[i].value = 1;
                            }
printf("switch %d\n", i);
                            pop_wind->sws[i].active = 1;
                            text_entry = -1;
                            if (pop_wind->update != NULL) {
                               (pop_wind->update)(pop_wind, pop_wind->device, i);
                            }
                        }
                    }
                    for (i = 0; i < pop_wind->cmb_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->combo[i].urect)) {
                            if (pop_wind->combo[i].num < pop_wind->combo[i].max)
                                pop_wind->combo[i].num++;
                            if (pop_wind->combo[i].value != 0) {
                                *pop_wind->combo[i].value = pop_wind->combo[i].num; 
                            }
printf("combo %d %d\n", i, pop_wind->combo[i].num);
                        }
                        if (inrect(event.button.x, event.button.y, pop_wind->combo[i].drect)) {
                            if (pop_wind->combo[i].num > 0)
                                pop_wind->combo[i].num--;
                            if (pop_wind->combo[i].value != 0) {
                                *pop_wind->combo[i].value = pop_wind->combo[i].num; 
                            }
printf("combo %d %d\n", i, pop_wind->combo[i].num);
                        }
                    }
                    for (i = 0; i < pop_wind->txt_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->text[i].rect)) {
                            pop_wind->text[i].enable = 1;
                            SDL_StartTextInput();
                            text_entry = i;
                            pop_wind->text[i].pos = findtextpos(&pop_wind->text[i], event.button.x);
                            pop_wind->text[i].spos = pop_wind->text[i].pos;
                            pop_wind->text[i].epos = pop_wind->text[i].pos;
                            pop_wind->text[i].srect.x = pop_wind->text[i].cpos;
                            pop_wind->text[i].srect.w = 0;
                            pop_wind->text[i].selecting = 1;
                            pop_wind->text[i].sel = 0;
printf("enable %d %d %d %d\n", i, event.button.x, pop_wind->text[i].pos, pop_wind->text[i].cpos);
                        }
                    }
                    break;

               case SDL_KEYDOWN:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        if (event.key.keysym.sym == SDLK_a) {
                            struct _text *t = &pop_wind->text[text_entry];
                            int h, w;
                            TTF_SizeText(font14, t->text, &w, &h);
printf("Select All %d\n", w);
                            t->spos = 0;
                            t->epos = strlen(t->text);;
                            t->sel = 1; 
                            t->pos = t->epos;
                            t->cpos = w;
                            t->srect.x = 0;
                            t->srect.w = w;
                        } else if (event.key.keysym.sym == SDLK_x) {
                            textcutpaste(&pop_wind->text[text_entry], 1, 0, 0);
        printf("Control x\n");
                        } else if (event.key.keysym.sym == SDLK_c) {
                            textcutpaste(&pop_wind->text[text_entry], 0, 0, 1);
        printf("Control c\n");
                        } else if (event.key.keysym.sym == SDLK_v) {
                            textcutpaste(&pop_wind->text[text_entry], 1, 1, 0);
        printf("Control v\n");
                        }
                    }
                    switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_U:
                         pop_wind->text[text_entry].pos = 0;
                         pop_wind->text[text_entry].cpos = 0;
                         pop_wind->text[text_entry].sel = 0;
                         pop_wind->text[text_entry].text[0] = '\0';
                         pop_wind->text[text_entry].len = 0;
                         break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_HOME:
                         pop_wind->text[text_entry].pos = 0;
                         pop_wind->text[text_entry].cpos = 0;
                         pop_wind->text[text_entry].sel = 0;
                         break;
                    case SDL_SCANCODE_END:
                         pop_wind->text[text_entry].pos = pop_wind->text[text_entry].len;
                         pop_wind->text[text_entry].cpos = textpos(&pop_wind->text[text_entry], 
                                    pop_wind->text[text_entry].pos); 
                         pop_wind->text[text_entry].sel = 0;
                         break;
                    case SDL_SCANCODE_LEFT:
                         if (pop_wind->text[text_entry].pos > 0) {
                             pop_wind->text[text_entry].pos--;
                             pop_wind->text[text_entry].cpos = textpos(&pop_wind->text[text_entry], 
                                    pop_wind->text[text_entry].pos); 
                         }
                         pop_wind->text[text_entry].sel = 0;
                         break;
                    case SDL_SCANCODE_RIGHT:
                         if (pop_wind->text[text_entry].pos < pop_wind->text[text_entry].len) {
                             pop_wind->text[text_entry].pos++;
                             pop_wind->text[text_entry].cpos = textpos(&pop_wind->text[text_entry], 
                                    pop_wind->text[text_entry].pos); 
                         }
                         pop_wind->text[text_entry].sel = 0;
                         break;
                    case SDL_SCANCODE_DELETE:
                    case SDL_SCANCODE_BACKSPACE:
                         textdelete(&pop_wind->text[text_entry]);
        printf("Key %d\n", event.key.keysym.scancode);
                         break;
                    default:
        printf("Key default %d\n", event.key.keysym.scancode);
                         break;
                    }
                    break;

               case SDL_TEXTINPUT:
                    textinsert(&pop_wind->text[text_entry], event.text.text);
 printf("Text input %s\n", event.text.text);
                    break;

               case SDL_TEXTEDITING:
 printf("Text Editing %s %d %d\n", event.edit.text, event.edit.start, event.edit.length);
                    break;

               case SDL_MOUSEMOTION:
                    if (text_entry >= 0 && pop_wind->text[text_entry].selecting) {
                        struct _text *t = &pop_wind->text[text_entry];
                        int cpos = t->pos;
                        int pos;
                        pos = findtextpos(t, event.button.x);
printf("Motion %d %d pos=%d, %d\n", text_entry, event.button.x, pos, t->pos);
                        if (pos < cpos) {
                            t->spos = pos;
                            t->epos = cpos;
                            t->sel = 1; 
                        } else if (pos > cpos) {
                            t->spos = cpos;
                            t->epos = pos;
                            t->sel = 1;
                        } else {
                            t->sel = 0;
                        }
                        t->pos = cpos;
                        if (t->spos > 1)
                            t->srect.x = textpos(t, t->spos - 1);
                        else
                            t->srect.x = 0;
                        t->srect.w = textpos(t, t->epos) - t->srect.x;
printf("Motion %d %d %d %d\n", t->spos, t->epos, t->sel, t->pos);
                    }
                    break;

               case SDL_MOUSEBUTTONUP:
                    if (text_entry >= 0 && pop_wind->text[text_entry].selecting) {
                        pop_wind->text[text_entry].selecting = 0;
                        pop_wind->text[text_entry].pos = pop_wind->text[text_entry].epos;
                        pop_wind->text[text_entry].cpos = pop_wind->text[text_entry].srect.x +
                                                          pop_wind->text[text_entry].srect.w;
                    }
printf("mouse up %d %d %d %d\n", i, event.button.x, pop_wind->text[i].pos, pop_wind->text[i].cpos);
                    for (i = 0; i < pop_wind->sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->sws[i].rect)) {
                            if (pop_wind->sws[i].value != 0) {
                                *pop_wind->sws[i].value = 0;
                            }
                            pop_wind->sws[i].active = 0;
printf("switch off %d\n", i);
                        }
                    }
                    break;
               default:
                    break;
               }
            }
            switch(event.type) {
            case SDL_USEREVENT:
                 draw_screen30(hd, wd);
                 /* Clear display */
                 SDL_SetRenderDrawColor( render2, 0x00, 0x00, 0x00, 0xFF);
                 SDL_RenderClear( render2);
                 for (i = 0; i < sizeof(chan)/sizeof(struct _device *); i++) {
                     struct _device *dev;
                     for (dev = chan[i]; dev != NULL; dev = dev->next) {
                          if (dev->draw_model != NULL)
                              dev->draw_model(dev, render2);
                     }
                 }
                 SDL_RenderPresent( render2 );
                 if (pop_wind != NULL) {
                    draw_popup(pop_wind, hd, wd);
                 }
                 break;
            case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
printf("Close\n");
                         break;
                    }
                 break;
            case SDL_QUIT:
printf("Quit\n");
                 POWER = 0;
                 break;
            default:
                 break;
            }
        }
    }
  done:
	printf("Done\n");
    SDL_WaitThread(thrd, NULL);
    model1050_done();
    SDL_RemoveTimer(disp_timer);
    TTF_CloseFont(font1);
    TTF_CloseFont(font12);
    TTF_CloseFont(font14);
    TTF_Quit();
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(screen);
    SDL_Quit(); 
	return 0;
} 


int process(void *data) {
printf("Process start\n");
    while(POWER) {
       cycle();
    }
    return 0;
}



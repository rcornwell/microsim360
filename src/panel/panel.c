/*
 * microsim360 - Front panel display.
 *
 * Copyright 2022, Richard Cornwell
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

#include "config.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#ifndef _WIN32
#include <SDL_main.h>
#endif

#include "logger.h"
#include "event.h"
#include "widgets.h"
#include "number.h"
#include "cpu.h"
#include "lamps_img.xpm"
#include "hex_dial_img.xpm"
#include "store_dials_img.xpm"
#include "switch_img.xpm"
/*#include "font1.h" */

/* Declare an error to be of a give color */
struct _area areas[100];

/* Draw line */
struct _mark marks[1000];

/* Model 2030 ROS bits. */
struct _ros_bits ros_bits[1000];

/* Generic lamps */
struct _lamp lamp[1000];

/* Two level indicators */
struct _led_bits led_bits[1000];

/* Text label */
struct _ctl_label ctl_label[1000];

/* Push button switch */
struct _switch sws[100];

/* Data entry toggle switches */
struct _toggle toggles[1000];

/* Indicator button */
struct _ind ind[100];

/* Rotory dial */
struct _dial dial[5];

/* Hex digit dial */
struct _hex hex_dial[10];

/* Storage selector for model 2030 */
struct _store store_dial[2];

/* Structure defining roller display */
struct _roller roller[6];

int process(void *data);
uint32_t timer_callback(uint32_t interval, void *param);


#define inrect(px, py, r) ((px > r.x) && (px < (r.x + r.w)) && (py > r.y) && (py < (r.y + r.h)))

TTF_Font *font0;                     /* Small font */
TTF_Font *font1;
TTF_Font *font10;
TTF_Font *font12;
TTF_Font *font14;
int         f1_hd;                   /* Font 1 Height */
int         f1_wd;                   /* Font 1 Width */
SDL_Color c= {0xff, 0xff, 0xff};     /* White */
SDL_Color c1= {0x0, 0x0, 0x0};	     /* Black */
SDL_Color c2= {0x83, 0x89, 0x7f};    /* Green */
SDL_Color c3= {0x17, 0x69, 0x99};    /* Blue */
SDL_Color c4= {0xc0, 0xbc, 0xb9};    /* Gray */
SDL_Color c5= {0xe3, 0x20, 0x4e};    /* Red */
SDL_Color c5o= {0x52, 0x08, 0x1f};   /* off Red */
SDL_Color cc= {0xdd, 0xd8, 0xc5};    /* Background */
SDL_Color cb= {0x7d, 0x79, 0x78};    /* Outline color */
SDL_Color cl= {0xb4, 0xb0, 0xa5};    /* Label background */
SDL_Color con= {0xd8, 0xcb, 0x72};   /* On digit */
SDL_Color cof= {0x1a, 0x1a, 0x1a};   /* Off digit */
SDL_Color c_white = {0xff, 0xff, 0xff};     /* White */
SDL_Color c_black = {0x0, 0x0, 0x0};	     /* Black */
SDL_Color c_green = {0x83, 0x89, 0x7f};    /* Green */
SDL_Color c_blue = {0x17, 0x69, 0x99};    /* Blue */
SDL_Color c_gray = {0xc0, 0xbc, 0xb9};    /* Gray */
SDL_Color c_red = {0xe3, 0x20, 0x4e};    /* Red */
SDL_Color c_red_off = {0x52, 0x08, 0x1f};   /* off Red */
SDL_Color c_back = {0xdd, 0xd8, 0xc5};    /* Background */
SDL_Color c_outline = {0x7d, 0x79, 0x78};    /* Outline color */
SDL_Color c_label = {0xb4, 0xb0, 0xa5};    /* Label background */
SDL_Color c_on = {0xd8, 0xcb, 0x72};   /* On digit */
SDL_Color c_off = {0x1a, 0x1a, 0x1a};   /* Off digit */
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
SDL_Texture *toggle_pic;
SDL_Texture *hex_dials;
SDL_Texture *store_dials;
int          fps;

SDL_bool over_cycle = SDL_FALSE;     /* Indicates over number of count cycles */
SDL_mutex  *display_mutex;           /* Lock for display update */
SDL_cond   *display_wait;            /* Display waiting for update */

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
int roller_ptr = 0;
int toggle_ptr = 0;

#define CHAR              001777
#define SHFT              000100
#define TOP               000200
#define META              000400
#define CTRL              001000

int key_state = 0;
int text_entry = -1;

uint64_t step_count;
int      cpu_count;

int      SYS_RST;
int      ROAR_RST;
int      START;
int      SET_IC;
int      CHECK_RST;
int      STOP;
int      INT_TMR;
int      STORE;
int      DISPLAY;
int      LAMP_TEST;
int      POWER;
int      INTR;
int      LOAD;
int      timer_event;

uint32_t ADR_CMP;
uint32_t INST_REP;
uint32_t ROS_CMP;
uint32_t ROS_REP;
uint32_t SAR_CMP;
uint32_t FORC_IND;
uint32_t FLT_MODE;
uint32_t CHN_MODE;
uint8_t  SEL_SW;
int      SEL_ENTER;

uint8_t  A_SW;
uint8_t  B_SW;
uint8_t  C_SW;
uint8_t  D_SW;
uint8_t  E_SW;
uint8_t  F_SW;
uint8_t  G_SW;
uint8_t  H_SW;
uint8_t  J_SW;

uint8_t  PROC_SW;
uint8_t  RATE_SW;
uint8_t  CHK_SW;
uint8_t  MATCH_SW;
uint8_t  STORE_SW;

uint16_t end_of_e_cycle;
uint16_t store;
uint16_t allow_write;
uint16_t match;
uint16_t t_request;
uint8_t  allow_man_operation;
uint8_t  wait;
uint8_t  test_mode;
uint8_t  clock_start_lch;
uint8_t  load_mode;
Panel    cpu_panel;
Panel    popup_panel;


void
SDL_Setup(char *title, int scr_wid, int scr_hi)
{
    SDL_Rect rect, rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    SDL_RWops    *rw_font;

    /* Start SDL */
    SDL_Init( SDL_INIT_EVERYTHING );
    TTF_Init();

    /* Create display locks */
    display_mutex = SDL_CreateMutex();
    display_wait = SDL_CreateCond();


    /* Set up screen */
    screen = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1100, 975, SDL_WINDOW_RESIZABLE );
    render = SDL_CreateRenderer( screen, -1, SDL_RENDERER_ACCELERATED);
    screen2 = SDL_CreateWindow("Devices", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		scr_wid, scr_hi, SDL_WINDOW_RESIZABLE );
    render2 = SDL_CreateRenderer( screen2, -1, SDL_RENDERER_ACCELERATED);

    if ((cpu_panel = (Panel)calloc(1, sizeof(struct _panel_t))) == NULL) {
        return;
    }
    cpu_panel->list = NULL;
    cpu_panel->last_item = NULL;
    cpu_panel->windowID = SDL_GetWindowID( screen );
    cpu_panel->screen = screen;
    cpu_panel->render = render;
/*    rw_font = SDL_RWFromConstMem(base_font, sizeof(base_font)); */
    /* Create fonts */
    font0 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 6);
    font1 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 9);
    font10 = TTF_OpenFont("../fonts/SourceCodePro-Black.ttf", 10);
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
    store_dials = SDL_CreateTextureFromSurface( render, text);
    SDL_SetTextureBlendMode(store_dials, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(text);
    text = IMG_ReadXPMFromArray(toggle_img);
    toggle_pic = SDL_CreateTextureFromSurface( render, text);
    SDL_SetTextureBlendMode(toggle_pic, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(text);

    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);
    if (setup_cpu != NULL)
        (*setup_cpu)((void *)render);
    cpu_count = 0;
    add_number(cpu_panel, 800, 5, 16, 80, font14, &cpu_count, &c_black, &c_white);
    system_init((void *)render2);
}

/* Add widget to window list */
void
add_widget(Panel win, Widget wid)
{
   wid->next = NULL;
   if (win->list == NULL) {
       win->list = wid;
   } else {
       win->last_item->next = wid;
   }
   win->last_item = wid;
}

void
draw_circle(int x, int y, int radius, SDL_Color color)
{
    int w, h;

    SDL_SetRenderDrawColor(render, color.r, color.g, color.b, color.a);
    for (w = 0; w < radius * 2; w++)
    {
        for (h = 0; h < radius * 2; h++)
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

void
add_toggle(uint32_t *value, int s, int x, int y, int t)
{
    toggles[toggle_ptr].rect.x = x;
    toggles[toggle_ptr].rect.y = y;
    toggles[toggle_ptr].rect.w = 15;
    toggles[toggle_ptr].rect.h = 32;
    toggles[toggle_ptr].value = value;
    toggles[toggle_ptr].shift = s;
    toggles[toggle_ptr].type = t;
    toggle_ptr++;
}

/*
 * Add in a lamp display.
 */
int
add_led(struct _labels *lab, uint16_t *value, int shf, int x, int y, int idx)
{
    int w = 0;

    if (lab->lower != NULL) {
        led_bits[led_ptr].rectl.x = x;
        led_bits[led_ptr].rectl.y = y + (f1_hd/2);
        led_bits[led_ptr].rectl.h = f1_hd;
        led_bits[led_ptr].rectl.w = f1_wd * strlen(lab->lower);
        w = led_bits[led_ptr].rectl.w;
        led_bits[led_ptr].digitl_on = digit2_on[idx];
        led_bits[led_ptr].digitl_off = digit2_off[idx];
        y -= (f1_hd/2);
    } else {
        led_bits[led_ptr].digitl_on = NULL;
        led_bits[led_ptr].digitl_off = NULL;
    }
    led_bits[led_ptr].recth.x = x;
    led_bits[led_ptr].recth.y = y;
    led_bits[led_ptr].recth.h = f1_hd;
    led_bits[led_ptr].recth.w = f1_wd * strlen(lab->upper);
    if (led_bits[led_ptr].recth.w > w)
        w = led_bits[led_ptr].recth.w;
    led_bits[led_ptr].digith_on = digit_on[idx];
    led_bits[led_ptr].digith_off = digit_off[idx];
    led_bits[led_ptr].value = value;
    led_bits[led_ptr].shift = shf;
    led_ptr++;
    return w;
}

static int roller_light_offset[37] = {
   /* 0  1   2   3   4   5   6   7   8   9 */
      0, 24, 23, 24, 23, 24, 23, 24, 23, 24,
   /*10  11  12  13  14  15  16  17  18  19*/
     23, 24, 23, 25, 23, 24, 23, 24, 52, 25,
   /*20  21  22  23  24  25  26  27  28  29*/
     24, 25, 24, 25, 24, 25, 24, 24, 24, 24,
   /*30  31  32  33  34  35  */
     23, 23, 23, 23, 23, 23, 23
};

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
            uint32_t  m = (uint32_t)1<<k;
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

void
draw_screen()
{
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    char          buf[100];
    Widget       wp;

    /* Clear display */
    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);

    /* Render all cpu panel */
    for (wp = cpu_panel->list; wp != NULL; wp = wp->next) {
         if (wp->draw != NULL)
             wp->draw(wp, render);
    }

//    sprintf(buf, "%10d fps=%d", cpu_count, fps);
//    rect.x = 700;
//    rect.y = 10;
//    rect.h = f1_hd;
//    rect.w = 20*f1_wd;
//    text = TTF_RenderText_Shaded(font1, buf, c1, c);
//    txt = SDL_CreateTextureFromSurface( render, text);
//    SDL_FreeSurface(text);
//    SDL_RenderCopy(render, txt, NULL, & rect);
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
    Widget       wp;
    char          buf[100];

    /* Clear display */
    SDL_SetRenderDrawColor( popup->render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( popup->render);
    /* Render all popup function */
    if (popup->panel != NULL && popup->panel->list != NULL) {
        for (wp = popup->panel->list; wp != NULL; wp = wp->next) {
             if (wp->draw != NULL)
                 wp->draw(wp, popup->render);
        }
    }
#if 0

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
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((f1_wd * popup->sws[i].top_len)/2);
           rect.y = popup->sws[i].rect.y;
           rect.w = f1_wd * popup->sws[i].top_len;
           rect.h = f1_hd;
           SDL_RenderCopy(popup->render, popup->sws[i].top, NULL, & rect);
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((f1_wd * popup->sws[i].bot_len)/2);
           rect.w = f1_wd * popup->sws[i].bot_len;
           rect.y += f1_hd;
           SDL_RenderCopy(popup->render, popup->sws[i].bot, NULL, & rect);
        } else if (popup->sws[i].top != NULL) {
           rect.x = popup->sws[i].rect.x + (popup->sws[i].rect.w / 2) - ((f1_wd * popup->sws[i].top_len)/2);
           rect.y = popup->sws[i].rect.y + (f1_hd/2);
           rect.w = f1_wd * popup->sws[i].top_len;
           rect.h = f1_hd;
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
        if (popup->ind[i].value != 0 && (*popup->ind[i].value >> popup->ind[i].shift) & 1)
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
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((f1_wd * popup->ind[i].top_len)/2);
           rect.y = popup->ind[i].rect.y + 2;
           rect.w = f1_wd * popup->ind[i].top_len;
           rect.h = f1_hd;
           SDL_RenderCopy(popup->render, popup->ind[i].top, NULL, & rect);
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((f1_wd * popup->ind[i].bot_len)/2);
           rect.w = f1_wd * popup->ind[i].bot_len;
           rect.y += f1_hd - 1;
           SDL_RenderCopy(popup->render, popup->ind[i].bot, NULL, & rect);
        } else if (popup->ind[i].top != NULL) {
           rect.x = popup->ind[i].rect.x + (popup->ind[i].rect.w / 2) - ((f1_wd * popup->ind[i].top_len)/2);
           rect.y = popup->ind[i].rect.y + (f1_hd/2);
           rect.w = f1_wd * popup->ind[i].top_len;
           rect.h = f1_hd;
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
        rect.x = popup->combo[i].rect.x + f1_hd;
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
        if (get_indicator(&popup->lamp[i].ind))
            rect.y = 15;
        SDL_RenderCopy(popup->render, lamps, &rect, & popup->lamp[i].rect);
    }
#endif

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
     log_trace("Text update (%s)\n", text->text);
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

uint32_t
timer_callback(uint32_t interval, void *param)
{
    SDL_Event     event;
    SDL_UserEvent userevent;

    memset(&userevent, 0, sizeof(SDL_UserEvent));
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

void
run_sim()
{
    SDL_Rect rect, rect2;
    SDL_Thread *thrd;
    SDL_TimerID  disp_timer;
    SDL_Event event;
    SDL_Surface *text;
    SDL_Texture *txt;
    SDL_RWops    *rw_font;
    Widget       wp;
    Widget       wp2;
    struct _popup   *pop_wind = NULL;
    Uint32	f;
    int      shf;
    int	     i, j, k;
    int	     h, w;
    int	     h2, w2;
    int      wb;
    char     buffer[4];
    int      mWindowID = -1;
    int      mDeviceID = -1;
    int      mPopID = -1;
    uint32_t ticks;

    POWER = 1;
    SYS_RST = 1;  /* Force system reset */
    thrd = SDL_CreateThread(process, "CPU", NULL);
    disp_timer = SDL_AddTimer(20, &timer_callback, NULL);
    mWindowID = SDL_GetWindowID( screen );
    mDeviceID = SDL_GetWindowID( screen2 );
    while(POWER) {
        while(SDL_PollEvent(&event)) {
           if (event.window.windowID == mWindowID) {
               switch(event.type) {
               case SDL_WINDOWEVENT_CLOSE:
                     /* Close all widgets */
                     wp = cpu_panel->list;
                     while (wp != NULL) {
                          if (wp->close != NULL) {
                              wp->close(wp);
                          }
                          wp2 = wp->next;
                          free(wp);
                          wp = wp2;
                     }
                     break;

               case SDL_QUIT:
                   log_trace("Quit\n");
                    break;

               case SDL_MOUSEBUTTONDOWN:
                    /* Check for click */
                    for (wp = cpu_panel->list; wp != NULL; wp = wp->next) {
                         if (wp->click != NULL) {
//                             if (SDL_PointInRect(&event.button, &wp->rect) == SDL_TRUE) {
                             if (inrect(event.button.x, event.button.y, wp->rect)) {
                                  wp->click(wp, event.button.x - wp->rect.x,
                                                event.button.y - wp->rect.y);
                                  wp->active = 1;
                                  if (wp->focus) {
                                      if (cpu_panel->focus != NULL) {
                                          cpu_panel->focus->focus = 0;
                                      }
                                      cpu_panel->focus = wp;
                                  }
                             }
                         }
                    }
                    break;

               case SDL_MOUSEBUTTONUP:
                    /* Check for release */
                    for (wp = cpu_panel->list; wp != NULL; wp = wp->next) {
                         if (wp->active) {
                             if (wp->release != NULL) {
                                 wp->release(wp);
                             }
                             wp->active = 0;
                         }
                    }


               case SDL_KEYDOWN:
                    if (cpu_panel->focus != NULL && cpu_panel->focus->keypress != NULL) {
                         cpu_panel->focus->keypress(cpu_panel->focus, &event.key);
                    }
                    break;

               case SDL_TEXTINPUT:
                    if (cpu_panel->focus != NULL && cpu_panel->focus->input != NULL) {
                         cpu_panel->focus->input(cpu_panel->focus, &event.text);
                    }
                    break;

               case SDL_TEXTEDITING:
                    break;

               case SDL_MOUSEMOTION:
                    if (cpu_panel->focus != NULL && cpu_panel->focus->motion != NULL) {
                        Widget w = cpu_panel->focus;
                        w->motion(w, event.button.x - w->rect.x, event.button.y - w->rect.y);
                    }
                    break;

               default:
                    break;
               }
            }
            else if (event.window.windowID == mDeviceID) {
               switch(event.type) {
               case SDL_MOUSEBUTTONDOWN:
                    log_device("Dev %d %d\n", event.button.x, event.button.y);
                    if (pop_wind != NULL)
                        break;
                    for (i = 0; i < sizeof(chan)/sizeof(struct _device *); i++) {
                        struct _device *dev;
                        for (dev = chan[i]; dev != NULL; dev = dev->next) {
                             for (j = 0; j < dev->n_units; j++) {
                                 if (dev->create_ctrl != NULL &&
                                       inrect(event.button.x, event.button.y, dev->rect[j])) {
                                     pop_wind = dev->create_ctrl(dev, f1_hd, f1_wd, j);
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
            else if (event.window.windowID == mPopID) {
               switch(event.type) {
               case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
printf("Close\n");
                          if (pop_wind == NULL)
                              break;
                          /* Close all widgets */
                          if (pop_wind->panel != NULL && pop_wind->panel->list != NULL) {
                              wp = pop_wind->panel->list;
                              while (wp != NULL) {
                                   if (wp->close != NULL) {
                                       wp->close(wp);
                                   }
                                   wp2 = wp->next;
                                   free(wp);
                                   wp = wp2;
                              }
                          }
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
                    /* Check for click */
                    if (pop_wind->panel != NULL && pop_wind->panel->list != NULL) {
                        for (wp = pop_wind->panel->list; wp != NULL; wp = wp->next) {
                             if (wp->click != NULL) {
//                                 if (SDL_PointInRect(&event.button, &wp->rect) == SDL_TRUE) {
                                 if (inrect(event.button.x, event.button.y, wp->rect)) {
                                      wp->click(wp, event.button.x - wp->rect.x,
                                                    event.button.y - wp->rect.y);
                                      wp->active = 1;
                                  if (wp->focus) {
                                      if (pop_wind->panel->focus != NULL) {
                                          pop_wind->panel->focus->focus = 0;
                                      }
                                      pop_wind->panel->focus = wp;
                                  }
                                 }
                             }
                        }
                    }
                    for (i = 0; i < pop_wind->sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->sws[i].rect)) {
                            if (pop_wind->sws[i].value != 0) {
                                *pop_wind->sws[i].value = 1;
                            }
log_trace("switch %d\n", i);
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
log_trace("combo %d %d\n", i, pop_wind->combo[i].num);
                        }
                        if (inrect(event.button.x, event.button.y, pop_wind->combo[i].drect)) {
                            if (pop_wind->combo[i].num > 0)
                                pop_wind->combo[i].num--;
                            if (pop_wind->combo[i].value != 0) {
                                *pop_wind->combo[i].value = pop_wind->combo[i].num;
                            }
log_trace("combo %d %d\n", i, pop_wind->combo[i].num);
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
log_trace("enable %d %d %d %d\n", i, event.button.x, pop_wind->text[i].pos, pop_wind->text[i].cpos);
                        }
                    }
                    break;

               case SDL_KEYDOWN:
                    if (pop_wind != NULL && pop_wind->panel != NULL &&
                          pop_wind->panel->focus != NULL && pop_wind->panel->focus->keypress != NULL) {
                         pop_wind->panel->focus->keypress(pop_wind->panel->focus, &event.key);
                    }
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        if (event.key.keysym.sym == SDLK_a) {
                            struct _text *t = &pop_wind->text[text_entry];
                            int h, w;
                            TTF_SizeText(font14, t->text, &w, &h);
log_trace("Select All %d\n", w);
                            t->spos = 0;
                            t->epos = strlen(t->text);;
                            t->sel = 1;
                            t->pos = t->epos;
                            t->cpos = w;
                            t->srect.x = 0;
                            t->srect.w = w;
                        } else if (event.key.keysym.sym == SDLK_x) {
                            textcutpaste(&pop_wind->text[text_entry], 1, 0, 0);
        log_trace("Control x\n");
                        } else if (event.key.keysym.sym == SDLK_c) {
                            textcutpaste(&pop_wind->text[text_entry], 0, 0, 1);
        log_trace("Control c\n");
                        } else if (event.key.keysym.sym == SDLK_v) {
                            textcutpaste(&pop_wind->text[text_entry], 1, 1, 0);
        log_trace("Control v\n");
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
        log_trace("Key %d\n", event.key.keysym.scancode);
                         break;
                    default:
        log_trace("Key default %d\n", event.key.keysym.scancode);
                         break;
                    }
                    break;

               case SDL_TEXTINPUT:
                    if (pop_wind != NULL && pop_wind->panel != NULL &&
                        pop_wind->panel->focus != NULL && pop_wind->panel->focus->input != NULL) {
                         pop_wind->panel->focus->input(pop_wind->panel->focus, &event.text);
                    }
//                    textinsert(&pop_wind->text[text_entry], event.text.text);
 log_trace("Text input %s\n", event.text.text);
                    break;

               case SDL_TEXTEDITING:
 log_trace("Text Editing %s %d %d\n", event.edit.text, event.edit.start, event.edit.length);
                    break;

               case SDL_MOUSEMOTION:
                    if (pop_wind != NULL && pop_wind->panel != NULL &&
                        pop_wind->panel->focus != NULL && pop_wind->panel->focus->motion != NULL) {
                        Widget w = pop_wind->panel->focus;
                        w->motion(w, event.button.x - w->rect.x, event.button.y - w->rect.y);
                    }
                    if (pop_wind->panel != NULL && pop_wind->panel->list != NULL) {
                        for (wp = pop_wind->panel->list; wp != NULL; wp = wp->next) {
                             if (wp->motion != NULL) {
//                                 if (SDL_PointInRect(&event.button, &wp->rect) == SDL_TRUE) {
                                 if (inrect(event.button.x, event.button.y, wp->rect)) {
                                 }
                             }
                        }
                    }
                    if (text_entry >= 0 && pop_wind->text[text_entry].selecting) {
                        struct _text *t = &pop_wind->text[text_entry];
                        int cpos = t->pos;
                        int pos;
                        pos = findtextpos(t, event.button.x);
log_trace("Motion %d %d pos=%d, %d\n", text_entry, event.button.x, pos, t->pos);
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
log_trace("Motion %d %d %d %d\n", t->spos, t->epos, t->sel, t->pos);
                    }
                    break;

               case SDL_MOUSEBUTTONUP:
                    /* Check for release */
                    if (pop_wind != NULL && pop_wind->panel != NULL &&
                        pop_wind->panel != NULL && pop_wind->panel->list != NULL) {
                        for (wp = pop_wind->panel->list; wp != NULL; wp = wp->next) {
                             if (wp->active) {
                                 if (wp->release != NULL) {
                                     wp->release(wp);
                                 }
                                 wp->active = 0;
                             }
                        }
                    }
                    if (text_entry >= 0 && pop_wind->text[text_entry].selecting) {
                        pop_wind->text[text_entry].selecting = 0;
                        pop_wind->text[text_entry].pos = pop_wind->text[text_entry].epos;
                        pop_wind->text[text_entry].cpos = pop_wind->text[text_entry].srect.x +
                                                          pop_wind->text[text_entry].srect.w;
                    }
log_trace("mouse up %d %d %d %d\n", i, event.button.x, pop_wind->text[i].pos, pop_wind->text[i].cpos);
                    for (i = 0; i < pop_wind->sws_ptr; i++) {
                        if (inrect(event.button.x, event.button.y, pop_wind->sws[i].rect)) {
                            if (pop_wind->sws[i].value != 0) {
                                *pop_wind->sws[i].value = 0;
                            }
                            pop_wind->sws[i].active = 0;
log_trace("switch off %d\n", i);
                        }
                    }
                    break;
               default:
                    break;
               }
            }
            else {
            switch(event.type) {
            case SDL_USEREVENT:
                 ticks = SDL_GetTicks();
                 draw_screen();
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
                    draw_popup(pop_wind, f1_hd, f1_wd);
                 }
                 SDL_LockMutex(display_mutex);
                 cpu_count = 0;
                 SDL_CondSignal(display_wait);
                 SDL_UnlockMutex(display_mutex);
                 fps = (int)(SDL_GetTicks() - ticks);
                 SDL_FlushEvent(SDL_USEREVENT);
                 if (fps < 18)
                     SDL_Delay(18 - fps);
                 break;
            case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
log_trace("Close\n");
                         break;
                    }
                 break;
            case SDL_QUIT:
log_trace("Quit\n");
                 POWER = 0;
                 cpu_count = 0;
                 break;
            default:
                 break;
            }
            }
        }
    }
  done:
	log_trace("Done\n");
    system_shutdown();
    /* Clear any widgets that have resources to remove */
    for (wp = cpu_panel->list; wp != NULL;) {
         Widget wpn = wp->next;
         if (wp->close != NULL)
             wp->close(wp);
         free(wp);
         wp = wpn;
    }

    SDL_WaitThread(thrd, NULL);
    SDL_RemoveTimer(disp_timer);
    TTF_CloseFont(font1);
    TTF_CloseFont(font10);
    TTF_CloseFont(font12);
    TTF_CloseFont(font14);
    TTF_Quit();
    SDL_DestroyCond(display_wait);
    SDL_DestroyMutex(display_mutex);
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(screen);
    SDL_Quit();
	return;
}


int process(void *data) {
    log_info("Process start %d\n", cpu_count);
    cpu_count = 0;
    while(POWER) {
       cpu_count++;
       step_count++;
       if (cpu_count > 20000) {
          SDL_LockMutex(display_mutex);
          while (cpu_count > 20000 && POWER) {
               SDL_CondWaitTimeout(display_wait, display_mutex, 50);
          }
          SDL_UnlockMutex(display_mutex);
       }
       (*step_cpu)();
       step_disk();
       step_disk();
       advance();
    }
    return 0;
}



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
#include "panel.h"
#include "number.h"
#include "cpu.h"
#include "panel_device.h"
#include "lamps_img.xpm"
#include "hex_dial_img.xpm"
#include "store_dials_img.xpm"
#include "switch_img.xpm"

int process(void *data);
uint32_t timer_callback(uint32_t interval, void *param);


#define inrect(px, py, r) ((px > r.x) && (px < (r.x + r.w)) && (py > r.y) && (py < (r.y + r.h)))

TTF_Font *font0;                     /* Small font */
TTF_Font *font1;
TTF_Font *font10;
TTF_Font *font12;
TTF_Font *font14;
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
SDL_Texture *lamps;
SDL_Texture *toggle_pic;
SDL_Texture *hex_dials;
SDL_Texture *store_dials;
int          fps;

SDL_bool over_cycle = SDL_FALSE;     /* Indicates over number of count cycles */
SDL_mutex  *display_mutex;           /* Lock for display update */
SDL_cond   *display_wait;            /* Display waiting for update */


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

Window    win_list_head, win_list_tail;

Panel
create_window(char *title, int w, int h, int popup)
{
    Window   win;

    if ((win = (struct _window_t *)calloc(1, sizeof(struct _window_t))) == NULL)
        return NULL;
    if ((win->panel = (struct _panel_t *)calloc(1, sizeof(struct _panel_t))) == NULL) {
        free(win);
        return NULL;
    }
    win->screen = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED,
                      SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_RESIZABLE );
    win->render = SDL_CreateRenderer( win->screen, -1, SDL_RENDERER_ACCELERATED);
    win->windowID = SDL_GetWindowID(win->screen);
    win->popup = popup;
    win->panel->list = NULL;
    win->panel->last_item = NULL;
    win->panel->screen = win->screen;
    win->panel->render = win->render;
    win->panel->windowID = win->windowID;
    win->title = strdup(title);

    if (win_list_head == NULL) {
        win_list_head = win;
        win_list_tail = win;
    } else {
        win_list_tail->next = win;
        win->prev = win_list_tail;
        win->next = NULL;
        win_list_tail = win;
   }
   printf("Create %s %d\n", win->title, win->windowID);
   return win->panel;
}

void
close_window(Window win)
{
    Widget    wp, wp2;
    Window       winp;

    /* Generate error, should never happen. */
    if (win_list_head == NULL) {
        return;
    }

    if (win->prev != NULL) {
       win->prev->next = win->next;
    } else {
       win_list_head = win->next;
    }

    if (win->next != NULL) {
       win->next->prev = win->prev;
    } else {
       win_list_tail = win->prev;
    }

    wp = win->panel->list;

    /* Call close routines to clean up widgets */
    while (wp != NULL) {
         if (wp->close != NULL) {
             wp->close(wp);
         }
         wp2 = wp->next;
         free(wp);
         wp = wp2;
    }

    if (win->panel->notify_parent_close != NULL) {
        for (winp = win_list_head; winp != NULL; winp = winp->next) {
             if (winp->windowID == win->panel->parentID) {
                 if (win->panel->notify_parent_close != NULL) {
                     win->panel->notify_parent_close(winp->panel, win->windowID);
                 }
                 break;
             }
       }
    }

    free(win->title);
    free(win->panel);
    SDL_DestroyRenderer(win->render);
    SDL_DestroyWindow(win->screen);
    free(win);
}

void
SDL_Setup(char *title)
{
    SDL_Surface *text;
    Panel       dev_panel;

    /* Start SDL */
    SDL_Init( SDL_INIT_EVERYTHING );
    TTF_Init();

    /* Create display locks */
    display_mutex = SDL_CreateMutex();
    display_wait = SDL_CreateCond();
    win_list_head = NULL;
    win_list_tail = NULL;


    /* Set up screen */
    cpu_panel = create_window(title, 1100, 975, 0);
    screen = cpu_panel->screen;
    render = cpu_panel->render;

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
    add_number(cpu_panel, 800, 5, 16, 80, &cpu_count, font14, &c_black, &c_white);
    dev_panel = create_device_window();
    system_init((void *)dev_panel->render);
}

void
draw_panels()
{
    Widget       wp;

    /* Clear display */
    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);

    /* Render all cpu panel */
    for (wp = cpu_panel->list; wp != NULL; wp = wp->next) {
         if (wp->draw != NULL)
             wp->draw(wp, render);
    }

    SDL_RenderPresent( render );
}


void
draw_screen()
{
    Window       winp;
    Widget       wp;

    for (winp = win_list_head; winp != NULL; winp = winp->next) {
        /* Clear display */
        SDL_SetRenderDrawColor( winp->render, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear( winp->render);

        /* Render all panels */
        for (wp = winp->panel->list; wp != NULL; wp = wp->next) {
             if (wp->draw != NULL)
                 wp->draw(wp, winp->render);
        }
        SDL_RenderPresent( winp->render );
    }
}

void
draw_popup(struct _popup *popup)
{
    Widget       wp;

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
    SDL_Thread *thrd;
    SDL_TimerID  disp_timer;
    SDL_Event event;
    Window       winp;
    Widget       wp;
    uint32_t ticks;

    POWER = 1;
    SYS_RST = 1;  /* Force system reset */
    thrd = SDL_CreateThread(process, "CPU", NULL);
    disp_timer = SDL_AddTimer(20, &timer_callback, NULL);
    while(POWER) {
        while(SDL_PollEvent(&event)) {
           if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                       for (winp = win_list_head; winp != NULL; winp = winp->next) {
                           if (winp->windowID == event.window.windowID) {
                               if (winp->popup) {
                                  close_window(winp);
                               }
                               break;
                           }
                       }
                }
                continue;
           }
           for (winp = win_list_head; winp != NULL; winp = winp->next) {
               if (winp->windowID == event.window.windowID) {
                  break;
               }
           }

           if (winp != NULL) {
               switch(event.type) {
               case SDL_MOUSEBUTTONDOWN:
                    /* Check for click */
                    for (wp = winp->panel->list; wp != NULL; wp = wp->next) {
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
                    for (wp = winp->panel->list; wp != NULL; wp = wp->next) {
                         if (wp->active) {
                             if (wp->release != NULL) {
                                 wp->release(wp);
                             }
                             wp->active = 0;
                         }
                    }
                    break;

               case SDL_KEYDOWN:
                    if (winp->panel->focus != NULL && winp->panel->focus->keypress != NULL) {
                         winp->panel->focus->keypress(winp->panel->focus, &event.key);
                    }
                    break;

               case SDL_TEXTINPUT:
                    if (winp->panel->focus != NULL && winp->panel->focus->input != NULL) {
                         winp->panel->focus->input(winp->panel->focus, &event.text);
                    }
                    break;

               case SDL_TEXTEDITING:
                    break;

               case SDL_MOUSEMOTION:
                    if (winp->panel->focus != NULL && winp->panel->focus->motion != NULL) {
                        Widget w = winp->panel->focus;
                        w->motion(w, event.button.x - w->rect.x, event.button.y - w->rect.y);
                    }
                    break;

               default:
                    break;
               }
           } else {
               switch(event.type) {
               case SDL_USEREVENT:
                    ticks = SDL_GetTicks();
                    draw_screen();
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
                            for (winp = win_list_head; winp != NULL; winp = winp->next) {
                                if (winp->windowID == event.window.windowID) {
                                    if (winp->popup) {
                                       close_window(winp);
                                    }
                                    break;
                                }
                            }
                            printf("Close\n");
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



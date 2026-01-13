/*
 * microsim360 - Layout peripheral devices on a screen.
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

#include <stdio.h>
#include <stdlib.h>
#include "device.h"
#include "widgets.h"
#include "panel.h"

struct _perip {
    int       h;    /* Height of window */
    int       w;    /* Width of window */
    int       x;    /* X position of window */
    int       y;    /* Y position of window */
    int       unit;
    struct _device  *dev;
    struct _perip *next;
} *perph;


/* Widget structure to hold pointer to control create routine */
struct _device_win_t {
    void    (*display_device)(struct _device *unit, void *render, int u);
    void    *(*create_control)(struct _device *unit, int u, int x, int y);
    struct  _device *unit;
    Panel    popup;
    int      popupID;
    int      parentID;
    int      u;
};

static void
close_child(Panel panel, int windowID)
{
    Widget   wp;

    for (wp = panel->list; wp != NULL; wp = wp->next) {
         struct _device_win_t *ctrl = (struct _device_win_t *)wp->data;
         if (ctrl->popupID == windowID) {
             ctrl->popup = NULL;
             ctrl->popupID = -1;
             break;
         }
    }
}

static void
display_device(Widget wid, SDL_Renderer *render)
{
    struct _device_win_t *ctrl = (struct _device_win_t *)wid->data;
    
    (*ctrl->display_device)(ctrl->unit, (void *)render, ctrl->u);
}

static void
close_device(Widget wid)
{
     free(wid->data);
}

static void
click_device(Widget wid, int x, int y)
{
    struct _device_win_t *ctrl = (struct _device_win_t *)wid->data;
    Panel    popup;

    printf("Click %d, %d\n", x, y);

    if (ctrl->popup != NULL) {
        SDL_RaiseWindow(ctrl->popup->screen);
        return;
    }
    if (ctrl->create_control != NULL) {
        popup = (Panel)(*ctrl->create_control)(ctrl->unit, ctrl->u, x, y);
        ctrl->popup = popup;
        ctrl->popupID = popup->windowID;
        popup->parentID =  ctrl->parentID;
        popup->notify_parent_close = close_child;
    }
}

/*
 * Layout devices to fit into windows.
 */
Panel
create_device_window()
{
    int      i;
    int      j;
    int      max_width;    /* Max width */
    int      max_height;   /* Max height */
    int      width;        /* Width of peripherals */
    int      height;       /* Height of peripherals */
    int      row_height;   /* Height of current row */
    int      screen_height;
    int      screen_width; /* Width of screen */
    int      rows;         /* Number of rows */
    int      waste;        /* Amount of wasted space */
    int      min_waste;    /* Min waste. */
    int      min_width;    /* Min width for min waste */
    int      min_height;   /* Min height */
    Panel   panel;         /* Panels for devices. */
    Widget  nwid;           /* Widget for device */
    struct _device *dev;   /* Point to device */
    struct _perip  *p;     /* Pointer to current peripheral */
    struct _device_win_t  *ndev; /* widget structure */

    p = NULL;
    max_width = 0;
    max_height = 0;
    width = 0;
    height = 0;
    /* Scan channels looking for windows */
    for (i = 0; i < 6; i++) {
        for (dev = chan[i]; dev != NULL; dev = dev->next) {
             for (j = 0; j < dev->n_units; j++) {
                  struct _perip  *np;
                  if (dev->rect[j].h == 0 || dev->rect[j].w == 0)
                      continue;
                  /* Save size of windows */
                  if ((np = (struct _perip *)calloc(1, sizeof(struct _perip))) == NULL)
                      return 0;
//printf("D=%03x %d %d\n", dev->addr+j, dev->rect[j].h, dev->rect[j].w);
                  np->h = dev->rect[j].h;
                  np->w = dev->rect[j].w;
                  np->dev = dev;
                  np->unit = j;
                  /* Compute max and total width */
                  height += np->h;
                  width += np->w;
                  if (max_height < np->h)
                      max_height = np->h;
                  if (max_width < np->w)
                      max_width = np->w;
                  if (perph == NULL) {
                      perph = np;
                  } else {
                      p->next = np;
                  }
                  /* If offset to pair device, create one with no size */
                  if (dev->rect[j].u_offset_x != 0 || dev->rect[j].u_offset_y != 0) {
                      struct _perip  *np2;
                      if ((np2 = (struct _perip *)calloc(1, sizeof(struct _perip))) == NULL) {
                          return 0;
                      }
//printf("D=%03x %d %d\n", dev->addr+j, dev->rect[j].h, dev->rect[j].w);
                      np2->h = 0;
                      np2->w = 0;
                      np2->dev = dev;
                      np2->unit = j + 1;
                      np2->next = np;
                      np = np2;
                  }
                  p = np;
             }

        }
   }
//   printf("MW=%d MH=%d TW=%d TH=%d\n", max_width, max_height, width, height);
   /* Try various layouts to figure out best window size */
   min_waste = 10000;
   min_width = 1200;
   for (i = 500; i <= 1200; i += 100) {
       int   w = 0;
       waste = 0;
       rows = 1;
       row_height = 0;
       screen_height = 0;
       for (p = perph; p != NULL; p = p->next) {
           if ((p->w + w) > i) {
              waste += i - w;
              w = 0;
              screen_height += row_height;
              row_height = 0;
              rows++;
           }
           w += p->w;
           if (p->h > row_height)
               row_height = p->h;
       }
       screen_height += row_height;
       if (waste < min_waste && screen_height < 1000) {
          min_waste = waste;
          min_width = i;
       }
  //     printf("W=%d H=%d rows=%d waste=%d min=%d\n", i, screen_height, rows, waste, min_width);
   }
   i = 0;
   j = 0;
   /* Assign windows to a given position */
   screen_height = 0;
   row_height = 0;
   for (p = perph; p != NULL; p = p->next) {
        if ((p->w + i) > min_width) {
            i = 0;
            j += row_height;
            screen_height += row_height;
            row_height = 0;
        }
        p->dev->rect[p->unit].x = i;
        p->dev->rect[p->unit].y = j;
//printf("Set=%03x %d %d %d %d\n", p->dev->addr, p->dev->rect[p->unit].x, p->dev->rect[p->unit].y,
 //                                   p->dev->rect[p->unit].h, p->dev->rect[p->unit].w);
        if (p->h > row_height)
            row_height = p->h;
        i += p->w;
   }
   screen_height += row_height;
//printf("size=%d %d\n", min_width, min_height);
   panel = create_window("Devices", min_width, screen_height, 0);

   /* Create widgets for each window */
   for (p = perph; p != NULL; p = p->next) {
       if (p->dev->rect[p->unit].h == 0 || p->dev->rect[p->unit].w == 0) {
           continue;
       }
       if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
           continue;
       }

       if ((ndev = (struct _device_win_t *)calloc(1, sizeof(struct _device_win_t))) == NULL) {
          free(nwid);
          continue;
       }
      
       ndev->create_control = p->dev->create_ctrl;
       ndev->display_device = p->dev->draw_model;
       ndev->unit = p->dev;
       ndev->u = p->unit;
       ndev->parentID = panel->windowID;
      
       nwid->rect.x = p->dev->rect[p->unit].x;
       nwid->rect.y = p->dev->rect[p->unit].y;
       nwid->rect.w = p->dev->rect[p->unit].h;
       nwid->rect.h = p->dev->rect[p->unit].w;
       nwid->back_color = &c_black;
       nwid->draw = display_device;
       nwid->click = click_device;
       nwid->close = close_device;
       nwid->data = (void *)ndev;
       if (p->dev->rect[p->unit].u_offset_x != 0 || p->dev->rect[p->unit].u_offset_y != 0) {
            Widget    second;
            struct _device_win_t  *ndev2; /* widget structure */
            if ((second = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
                continue;
            }
          
            if ((ndev2 = (struct _device_win_t *)calloc(1, sizeof(struct _device_win_t))) == NULL) {
               free(second);
               continue;
            }
           
            ndev2->create_control = p->dev->create_ctrl;
            ndev2->display_device = NULL;
            ndev2->unit = p->dev;
            ndev2->u = p->unit + 1;
            ndev2->parentID = panel->windowID;
           
            second->rect.x = p->dev->rect[p->unit].x + p->dev->rect[p->unit].u_offset_x;
            second->rect.y = p->dev->rect[p->unit].y + p->dev->rect[p->unit].u_offset_y;
            second->rect.w = p->dev->rect[p->unit].h + p->dev->rect[p->unit].u_offset_x;
            second->rect.h = p->dev->rect[p->unit].w + p->dev->rect[p->unit].u_offset_y;
            nwid->rect.w = p->dev->rect[p->unit].w - p->dev->rect[p->unit].u_offset_x;
            nwid->rect.h = p->dev->rect[p->unit].h - p->dev->rect[p->unit].u_offset_y;
            second->back_color = &c_black;
            second->click = click_device;
            second->close = close_device;
            second->data = (void *)ndev2;
            add_widget(panel, second);
      }
      add_widget(panel, nwid);
   }

   return panel;
}

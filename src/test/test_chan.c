/*
 * microsim360 - Test channel driver.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_chan.h"
#include "logger.h"
#include "device.h"
#include "xlat.h"
#include "ctest.h"

uint32_t cmd_addr;
uint32_t data_addr;
uint16_t data_cnt;
uint8_t  flags;
uint16_t opr;
uint32_t Mem[8*1024];  /* 32K memory */

static char *states[] = {
    /* 0       1         2       3      4          5        6       7 */
     "Init", "Select", "Start", "CMD", "Accept", "Status", "Data", "DWait",
    /* 8               9         10          11       12       13     */
      "Final Status", "Polling", "Reselect", "Addr", "CCW", "Oper",
    /* 14      15          16    17        18       19   */
      "HWait", "HSelect", "HS2", "HALT", "HWAIT2", "Wait OPR"};

static char *bus_tags[] = {
    "SLO", "ADO", "CMD", "SRO", "SUP", "HLD", "OPO", NULL,
    "OPI", "ADI", "STI", "SVI", "RQI", NULL, NULL, NULL };

static void
log_tags(char *name, int state, uint8_t flags, uint16_t tags, uint16_t bus_out, uint16_t bus_in)
{

    if ((tags & 0xf8ff) != 0) {
        char   buffer[1024];
        int i;

        sprintf(buffer, "%s state=%s flags=%02x Tags: bus in=%03x bus out=%03x %04x ", name,
                         states[state], flags, bus_in, bus_out, tags);
        for (i = 0; i < 16; i++) {
            if (bus_tags[i] != NULL) {
              if ((tags & (0x8000 >> i)) != 0) {
                  strcat(buffer, bus_tags[i]);
                  strcat(buffer, " ");
              } else {
                  strcat(buffer, "    ");
              }
            }
        }
        strcat(buffer, "\n");
        log_trace(buffer);
    }
}

/* Read word from main memory */
uint32_t
get_mem(int addr)
{
    return Mem[addr >> 2];
}

/* Set word into main memory */
void
set_mem(int addr, uint32_t data)
{
    Mem[addr >> 2] = data;
}

/* Read byte from main memory */
uint8_t
get_mem_b(int addr)
{
    uint32_t   v = Mem[addr >> 2];
    return (uint8_t)(v >> (8 * (3 - (addr & 3))) & 0xff);
}

/* Set byte into main memory */
void
set_mem_b(int addr, uint8_t data)
{
    int        offset = 8 * (3 - (addr & 3));
    uint32_t   mask = 0xff;
    Mem[addr>>2] &= ~(mask << offset);
    Mem[addr>>2] |= ((uint32_t)data << offset);
}

/**
 * Start I/O operation.
 *
 * Process a chain of commands.
 */
uint16_t
start_io(uint8_t device, uint16_t caw, int sel, int halt)
{
    int         chan_clk = 0;
    int         dly = 50;
    int         chan_end;
    device_t    *dev;
    uint8_t     cmd;
    uint32_t    word;
    uint16_t    tags;
    uint16_t    tags_in = 0;
    uint16_t    bus_out = 0x100;
    uint16_t    bus_in = 0x100;
    uint16_t    status;

    /* Fetch initial CCW */
    cmd_addr = caw;
    word = get_mem(cmd_addr);
    cmd = (word >> 24) & 0xff;
    data_addr = word & 0xffffff;
    word = get_mem(cmd_addr+4);
    flags = (word >> 24) & 0xff;
    data_cnt = word & 0xffff;
    cmd_addr += 8;
    tags = CHAN_OPR_OUT;
    opr = (cmd & 1) | ((!(cmd & 1)) << 2);
    chan_end = 0;
    if ((cmd & 0xf) == 0xc) {
        opr |= 2;
    }
    while (1) {
        /* Run the device */
        tags_in &= IN_TAGS;
        tags_in |= tags;
        for(dev = chan[0]; dev != NULL; dev = dev->next) {
           dev->bus_func(dev, &tags_in, bus_out, &bus_in);
        }
        test_advance();
        log_tags("start_io", chan_clk, flags, tags_in, bus_out, bus_in);
        switch (chan_clk) {
        case 0:  /* Init */
                 /* Present address */
                 if (dly != 0) {
                     dly--;
                     break;
                 }
                 tags |= CHAN_ADR_OUT;
                 chan_clk = 1;
                 bus_out = (device & 0xff) | odd_parity[(device & 0xff)];
                 break;

        case 1:  /* Select */
                 /* Present select out */
                 tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                 chan_clk = 2;
                 break;

        case 2:  /* Start */
                 /* Check if not selected */
                 if ((tags_in & (CHAN_SEL_IN)) != 0) {
                     tags &= (CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                     log_trace("No device\n");
                     set_mem(0x40, cmd_addr);
                     return 0x100; /* Return no device */
                 }

                 /* Check quick busy */
                 if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN)) == (CHAN_STA_IN)) {
                     log_trace("Busy\n");
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                     set_mem(0x40, cmd_addr);
                     set_mem_b(0x44, bus_in & 0xff);
                     set_mem_b(0x45, 0);
                     status = 0x200 | (bus_in & 0xff);
                     chan_clk = 19;
                     break;
                 }

                 /* Device dropped address in and raise oper out */
                 if ((tags_in & (CHAN_OPR_IN)) != 0) {
                     tags &= ~(CHAN_ADR_OUT);
                 }

                 /* Check for Operator IN and Address IN */
                 if ((tags_in & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                     tags &= ~CHAN_SUP_OUT;
                     /* Compare addressed device */
                     if (bus_in != (device | odd_parity[device])) {
                         log_trace("Invalid\n");
                         set_mem(0x40, cmd_addr);
                         set_mem_b(0x44, bus_in & 0xff);
                         set_mem_b(0x45, 0);
                         status = 0x300;
                         chan_clk = 19;
                         break;
                     }
                     /* For mpx channel drop select out */
                     if (!sel) {
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     }
                     chan_clk = 3;
                 }
                 break;

        case 3:  /* CMD */
                 /* Present command */
                 bus_out = cmd | odd_parity[cmd];
                 if ((tags_in & CHAN_ADR_IN) != 0) {
                     tags |= (CHAN_CMD_OUT);
                 }

                 /* Device dropped address in drop command out */
                 if ((tags_in & CHAN_ADR_IN) == 0) {
                     tags &= ~(CHAN_CMD_OUT);
                 }

                 /* Device presented initial status */
                 if ((tags_in & CHAN_STA_IN) != 0) {
                    status = bus_in;
                    chan_clk = 4;
                 }
                 break;

        case 4:  /* Accept */
                 /* Accept status */
                 tags |= CHAN_SRV_OUT;
                 chan_clk = 5;
                 break;

        case 5:  /* Status */
                 /* wait status in to drop */
                 if ((tags_in & CHAN_STA_IN) != 0) {
                    break;
                 }
                 tags &= ~(CHAN_SRV_OUT|CHAN_SUP_OUT);
                 if ((status & 0xb3) != 0) {
                     log_trace("Error status\n");
                     set_mem(0x40, cmd_addr);
                     set_mem(0x44, ((status & 0xff) << 24) | data_cnt);
                     status &= 0xff;
                     chan_clk = 19;
                     break;
                 }
                 /* On device end, either return, or chain */
                 if ((status & (SNS_DEVEND)) != 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     if ((flags & 0x40) == 0) {  /* Check if command chain */
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                     status &= 0xff;
                     chan_clk = 19;
                     break;
                     }
                     chan_clk = 12;  /* Fetch next CCW */
                     break;
                 }

                 /* On channel end, either return or wait if chaining */
                 if ((status & (SNS_CHNEND)) != 0) {
                     chan_end = 1;
                     if ((flags & 0x40) == 0) {  /* Check if command chain */
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                         status &= 0xff;
                         chan_clk = 19;
                         break;
                     }
                     opr = 0;
                     if (!sel) {  /* Hold select out up until final status */
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     }
                 }
                 chan_clk = 6;  /* Go transfer data */
                 break;

        case 6:  /* Data */
                 /* Wait for data transfer */
                 /* If oper in drops, go to polling mode */
                 if ((tags_in & CHAN_OPR_IN) == 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                     if (sel == 0) {
                        tags &= ~(CHAN_SUP_OUT);
                     }
                     chan_clk = 9;
                     break;
                 }
                 /* Check if data transfer */
                 if ((tags_in & CHAN_SRV_IN) != 0) {
                    if (data_cnt == 0) {
                        if ((flags & 0x80) != 0) {
                            chan_clk = 12; /* Fetch next CCW first */
                            break;
                        }
                        tags |= CHAN_CMD_OUT;
                        if ((flags & 0x20) == 0) {
                            flags |= 1; /* Set length error */
                        }
                        chan_clk = 7; /* Wait for service in to drop */
                        break;
                    }
                    tags |= CHAN_SRV_OUT; /* Acknowlege data */
                    switch(opr) {
                    case 4:  /* Read */
                            if ((flags & 0x10) == 0) {  /* Skip */
                                set_mem_b(data_addr, bus_in & 0xff);
                            }
                            bus_out = 0x100;
                            data_addr ++;
                            break;
                    case 6:  /* Read Backwards. */
                            if ((flags & 0x10) == 0) {  /* Skip */
                                set_mem_b(data_addr, bus_in & 0xff);
                            }
                            bus_out = 0x100;
                            data_addr --;
                            break;
                    case 1:  /* Write */
                            bus_out = get_mem_b(data_addr);
                            bus_out |= odd_parity[bus_out];
                            data_addr ++;
                            break;
                    }
                    data_cnt--;
                    log_trace("Xfer: %06x %04x %x\n", data_addr, data_cnt, opr);
                    chan_clk = 7; /* Go wait for service in to drop */
                 }
                 /* Check for status in */
                 if ((tags_in & CHAN_STA_IN) != 0) {
                    status = bus_in;
                    tags |= CHAN_SRV_OUT; /* Acknowlege it */
                    if ((flags & 0x40) != 0) {  /* Check if command chain */
                       tags |= CHAN_SUP_OUT;
                    }
                    chan_clk = 8;  /* Go validate status */
                 }
                 break;

        case 7:  /* DWait */
                 /* Wait for device to acknowlege Service out */
                 if ((tags_in & CHAN_SRV_IN) != 0) {
                     break;
                 }
                 tags &= ~(CHAN_SRV_OUT|CHAN_CMD_OUT); /* Acknowlege data */
                 if (data_cnt == 0 && halt && (flags & 0x80) != 0) {
                     dly = 20;
                     halt = 0;
                     chan_clk = 14;
                 } else {
                     chan_clk = 6; /* Go wait for some more data */
                 }
                 break;

        case 8:  /* Final Status */
                 /* Wait for status and operin to drop */
                 if ((tags_in & (CHAN_STA_IN|CHAN_SRV_IN)) != 0) {
                     break;
                 }
                 tags &= ~(CHAN_SRV_OUT|CHAN_CMD_OUT); /* Acknowlege data */
                 if (chan_end == 0 && data_cnt != 0 && (flags & 0x20) == 0) {
                     flags |= 1;
                 }

                 /* Any error bits, done */
                 if ((status & 0xb3) != 0) {
                     set_mem(0x40, cmd_addr);
                     set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                     status &= 0xff;
                     chan_clk = 19;
                     break;
                 }

                 if ((flags & 1) != 0) { /* Check length */
                     set_mem(0x40, cmd_addr);
                     set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                     status &= 0xff;
                     chan_clk = 19;
                     break;
                 }

                 /* On device end, either return, of chain */
                 if ((status & (SNS_DEVEND)) != 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     if ((flags & 0x40) == 0) {  /* Check if command chain */
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                         status &= 0xff;
                         chan_clk = 19;
                         break;
                     }
                     opr = 0;
                     chan_clk = 12;  /* Fetch next CCW */
                     break;
                 }

                 /* On channel end, if not in chain, return. Otherwise wait */
                 if ((status & (SNS_CHNEND)) != 0) {
                     if ((flags & 0x40) == 0) {  /* Check if command chain */
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                         status &= 0xff;
                         chan_clk = 19;
                         break;
                     }
                     if (halt) {
                         dly = 20;
                         halt = 0;
                         chan_clk = 14;
                         break;
                     }
                     if (!sel) {  /* Hold select out up until final status */
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     }
                     opr = 0;
                     chan_clk = 6;  /* Go wait final status */
                 }
                 break;

        case 9:  /* Polling */
                 /* Wait for request in */
                 if ((tags_in & CHAN_REQ_IN) != 0) {
                     tags |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
                     bus_out = 0x100;
                     chan_clk = 10;
                 }
                 break;

        case 10: /* Reselect */
                 /* Check for Operator IN and Address IN */
                 if ((tags_in & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                     /* Compare addressed device */
                     if (bus_in != (device | odd_parity[device])) {
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((bus_in & 0xff) << 24) | data_cnt);
                         status &= 0xff;
                         chan_clk = 19;
                         break;
                     }
                     /* For select channel drop select out */
                     if (!sel) {
                         tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     }
                     chan_clk = 11;
                     bus_out = 0x100;
                     tags |= CHAN_CMD_OUT;
                 }
                 break;

        case 11: /* Addr */
                 /* Wait for address in to drop */
                 if ((tags_in & (CHAN_ADR_IN)) == 0) {
                     chan_clk = 6; /* Resume transfer */
                     tags &= ~CHAN_CMD_OUT;
                 }
                 break;

        case 12: /* CCW */
                 /* Fetch next CCW */
                 if ((status & SNS_SMS) != 0) {
                    cmd_addr += 8;
                 }
                 word = get_mem(cmd_addr);
                 cmd = (word >> 24) & 0xff;
                 data_addr = word & 0xffffff;
                 word = get_mem(cmd_addr+4);
                 log_trace("CCW: %08x %08x\n", get_mem(cmd_addr), get_mem(cmd_addr+4));
                 cmd_addr += 8;
                 if (cmd == 0x8) { /* TIC command */
                     cmd_addr = data_addr;
                     status &= ~(SNS_SMS);
                     break;
                 }
                 if ((flags & 0x80) != 0) { /* CD */
                     flags = (word >> 24) & 0xff;
                     data_cnt = word & 0xffff;
                     chan_clk = 6;
                     break;
                 }
                 /* Otherwise must be CC */
                 opr = (cmd & 1) | ((!(cmd & 1)) << 2);
                 if ((cmd & 0xf) == 0xc) {
                     opr |= 2;
                 }
                 log_trace("CCW: %x %02x\n", opr, cmd);
                 flags = (word >> 24) & 0xff;
                 data_cnt = word & 0xffff;
                 chan_clk = 13;
                 break;

        case 13: /* Oper */
                 /* Wait for oper in to drop */
                 if ((tags_in & CHAN_OPR_IN) == 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     dly = 50;
                     chan_clk = 0; /* Go start next command */
                     break;
                 }
                 break;

        case 14: /* HWait */
                 /* Prepare to halt device */
                 if (dly == 0) {
                         set_mem(0x40, cmd_addr);
                         set_mem(0x44, ((status & 0xff) << 24) | ((flags & 1) << 22)| data_cnt);
                     if ((tags_in & CHAN_OPR_IN) == 0) {
                         chan_clk = 15;
                     } else {
                         chan_clk = 17;
                    }
                 } else {
                     dly--;
                 }
                 break;

        case 15: /* HSelect */
                 /* If device not selected, select it */
                 tags |= CHAN_ADR_OUT;
                 chan_clk = 16;
                 bus_out = (device & 0xff) | odd_parity[(device & 0xff)];
                 break;

        case 16: /* HS2 */
                 /* Present select out */
                 tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                 chan_clk = 17;
                 break;

        case 17: /* HALT */
                 /* Check if not selected */
                 if ((tags_in & (CHAN_SEL_IN)) != 0) {
                     log_trace("No device\n");
                     set_mem(0x40, cmd_addr);
                     status = 0x100;
                     chan_clk = 19;
                     break;
                 }

                 /* Check quick busy */
                 if ((tags_in & (CHAN_STA_IN|CHAN_OPR_IN)) != 0) {
                     tags &= ~CHAN_SEL_OUT|CHAN_HLD_OUT;
                     tags |= CHAN_ADR_OUT;
                     chan_clk = 18;
                 }
                 break;

        case 18: /* HWAIT2 */
                 /* Present command */
                 if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN)) == 0) {
                     tags &= ~(CHAN_ADR_OUT|CHAN_SUP_OUT);
                     if ((flags & 0x40) != 0) {
                         return status;
                     }
                     chan_clk = 9;
                 }
                 break;
        case 19: /* Wait opr */
                 /* Wait for Oper IN to drop */
                 tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT);
                 if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_SRV_IN)) == 0) {
                     return status;
                 }
                 break;
        }
    }
}

/**
 * Test I/O operation.
 *
 * Issue test I/O to device.
 */
uint16_t
test_io(uint8_t device)
{
    int         chan_clk = 0;
    device_t    *dev;
    uint16_t    tags;
    uint16_t    tags_in = 0;
    uint16_t    bus_out = 0x100;
    uint16_t    bus_in = 0x100;
    uint16_t    status;

    tags = CHAN_OPR_OUT;
    while (1) {
        log_tags("test io", chan_clk, 0, tags_in, bus_out, bus_in);
        /* Run the device */
        tags_in &= IN_TAGS;
        tags_in |= tags;
        for(dev = chan[0]; dev != NULL; dev = dev->next) {
           dev->bus_func(dev, &tags_in, bus_out, &bus_in);
        }
        test_advance();
        switch (chan_clk) {
        case 0: /* Init */
                /* Present address */
                tags |= CHAN_ADR_OUT;
                chan_clk = 1;
                bus_out = (device & 0xff) | odd_parity[(device & 0xff)];
                break;

        case 1: /* Select */
                /* Present select out */
                tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                chan_clk = 2;
                break;

        case 2: /* Start */
                /* Check if not selected */
                if ((tags_in & (CHAN_SEL_IN)) != 0) {
                    tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                    log_trace("No device\n");
                    return 0x100; /* Return no device */
                }

                /* Check quick busy */
                if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN)) == (CHAN_STA_IN)) {
                    log_trace("Busy\n");
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                     set_mem(0x40, cmd_addr);
                     set_mem_b(0x44, bus_in & 0xff);
                     set_mem_b(0x45, 0);
                     status = 0x200 | (bus_in & 0xff);
                     chan_clk = 19;
                     break;
                }

                /* Device dropped address in and raise oper out */
                if ((tags_in & (CHAN_OPR_IN)) != 0) {
                    tags &= ~(CHAN_ADR_OUT);
                }

                /* Check for Operator IN and Address IN */
                if ((tags_in & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                    /* Compare addressed device */
                    if (bus_in != (device | odd_parity[device])) {
                        log_trace("Invalid\n");
                        log_trace("Invalid\n");
                        set_mem(0x40, cmd_addr);
                        set_mem_b(0x44, bus_in & 0xff);
                        set_mem_b(0x45, 0);
                        status = 0x300;
                        chan_clk = 19;
                        break;
                    }
                    tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                    chan_clk = 3;
                }
                break;

        case 3: /* CMD */
                bus_out = 0x100;
                /* Present command */
                if ((tags_in & CHAN_ADR_IN) != 0) {
                    tags |= (CHAN_CMD_OUT);
                }

                /* Device dropped address in drop command out */
                if ((tags_in & CHAN_ADR_IN) == 0) {
                    tags &= ~(CHAN_CMD_OUT);
                }

                /* Device presented initial status */
                if ((tags_in & CHAN_STA_IN) != 0) {
                   status = bus_in;
                   chan_clk = 4;
                }
                break;

        case 4: /* Accept */
                /* Accept status */
                tags |= CHAN_SRV_OUT;
                chan_clk = 5;
                break;

        case 5: /* Status */
                /* wait status in to drop */
                if ((tags_in & (CHAN_STA_IN|CHAN_SRV_IN)) != 0) {
                   break;
                }
                tags &= ~(CHAN_SRV_OUT|CHAN_SUP_OUT);
                chan_clk = 19;
                status &= 0xff;
                break;

        case 19: /* Wait opr */
                 /* Wait for Oper IN to drop */
                 tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT);
                 if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_SRV_IN)) == 0) {
                     return status;
                 }
                 break;
        }
    }
}

/**
 *  Wait for device to give final status.
 */
uint16_t
wait_dev(uint8_t  device)
{
    int         chan_clk = 0;
    device_t    *dev;
    uint16_t    tags;
    uint16_t    tags_in = 0;
    uint16_t    bus_out = 0x100;
    uint16_t    bus_in = 0x100;
    uint16_t    status;

    tags = CHAN_OPR_OUT;
    while (1) {
        log_tags("wait dev", chan_clk, 0, tags_in, bus_out, bus_in);
        /* Run the device */
        tags_in &= IN_TAGS;
        tags_in |= tags;
        for(dev = chan[0]; dev != NULL; dev = dev->next) {
           dev->bus_func(dev, &tags_in, bus_out, &bus_in);
           test_advance();
        }
        switch (chan_clk) {
        case 0:  /* Wait for data transfer */
                 /* If oper in drops, go to polling mode */
                 if ((tags_in & CHAN_OPR_IN) == 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                     chan_clk = 9;
                     break;
                 }
                break;
        case 6:  /* Data */
                 /* Wait for data transfer */
                 /* If oper in drops, go to polling mode */
                 if ((tags_in & CHAN_OPR_IN) == 0) {
                     tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT);
                     chan_clk = 9;
                     break;
                 }
                 /* Check if data transfer */ /* Acknowlege and ignore */
                 if ((tags_in & CHAN_SRV_IN) != 0) {
                    tags |= CHAN_SRV_OUT; /* Acknowlege data */
                    chan_clk = 7; /* Go wait for service in to drop */
                 }
                 /* Check for status in */
                 if ((tags_in & CHAN_STA_IN) != 0) {
                    status = bus_in & 0xff;
                    tags |= CHAN_SRV_OUT; /* Acknowlege it */
                    chan_clk = 8;  /* Go validate status */
                 }
                 break;

        case 7:  /* DWait */
                 /* Wait for device to acknowlege Service out */
                 if ((tags_in & CHAN_SRV_IN) != 0) {
                     break;
                 }
                 tags &= ~(CHAN_SRV_OUT|CHAN_CMD_OUT); /* Acknowlege data */
                 chan_clk = 6; /* Go wait for some more data */
                 break;

        case 8:  /* Final Status */
                 /* Wait for status and operin to drop */
                 if ((tags_in & (CHAN_STA_IN|CHAN_SRV_IN)) != 0) {
                     break;
                 }
                 tags &= ~(CHAN_SRV_OUT|CHAN_CMD_OUT); /* Acknowlege data */

                 log_trace("Save final status %02x\n", status);
                 set_mem_b(0x44, status & 0xff);
                 set_mem_b(0x45, 0);
                 chan_clk = 19;
                 break;

        case 9:  /* Polling */
                 /* Wait for request in */
                 if ((tags_in & CHAN_REQ_IN) != 0) {
                     tags |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
                     bus_out = 0x100;
                     chan_clk = 10;
                 }
                 break;

        case 10: /* Reselect */
                 /* Check for Operator IN and Address IN */
                 if ((tags_in & (CHAN_ADR_IN|CHAN_OPR_IN)) == (CHAN_ADR_IN|CHAN_OPR_IN)) {
                     /* Compare addressed device */
                     if (bus_in != (device | odd_parity[device])) {
                         status = 0x300;
                         chan_clk = 19;
                         break;
                     }
                     chan_clk = 11;
                     bus_out = 0x100;
                     tags |= CHAN_CMD_OUT;
                 }
                 break;

        case 11: /* Addr */
                 /* Wait for address in to drop */
                 if ((tags_in & (CHAN_ADR_IN)) == 0) {
                     chan_clk = 6; /* Resume transfer */
                     tags &= ~CHAN_CMD_OUT;
                 }
                 break;

        case 19: /* Wait opr */
                 /* Wait for Oper IN to drop */
                 tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT);
                 if ((tags_in & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_SRV_IN)) == 0) {
                     return status;
                 }
                 break;
        }
    }
}

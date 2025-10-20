/*
 * microsim360 - Test channel driver.
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

#include <stdio.h>
#include <stdlib.h>
#include "logger.h"
#include "device.h"
#include "event.h"
#include "ctest.h"
#include "xlat.h"
#include "model2844.h"

uint64_t   step_count = 0;

void
print_bin(struct _dasd_t *dasd, int unit)
{
    int             pos;
    uint8_t         *rec;
    uint8_t         *da;
    int             dlen;
    int             klen;
    int             i;
    int             end = 0;

    pos = (dasd->tsize * dasd->head);
    rec = &dasd->cbuf[pos];
    log_trace("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[5];
    log_trace("RECa c=%02x%02x h=%02x%02x r=%02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[7];
    log_trace("RECb c=%02x%02x h=%02x%02x r=%02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
}

void
print_track(struct _dasd_t *dasd, int unit)
{
    int             pos;
    uint8_t         *rec;
    uint8_t         *da;
    int             dlen;
    int             klen;
    int             i;
    int             end = 0;

    pos = (dasd->tsize * dasd->head);

    rec = &dasd->cbuf[pos];
    log_trace("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[5];
    for (i = 0; !end; i++) {
         if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
            log_trace("End\n");
            break;
         } else {
            klen = rec[5];
            dlen = (rec[6] << 8) | rec[7];
         }
         log_trace("REC%d c=%02x%02x h=%02x%02x r=%02x k=%d d=%d\n", i, rec[0], rec[1], rec[2],
                 rec[3], rec[4], klen, dlen);
         rec += 8 + klen + dlen;
    }
}

uint16_t
initial_select(struct _device *dev, uint16_t *tags, int cmd)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status;
    int         i;
    int         sel = 0;
    int         sts = 0;

    *tags |= CHAN_OPR_OUT;
    log_trace("Initial select\n");
    for (i = 0; i < 200; i++) {
        step_disk();
        step_disk();
        advance();
        step_count++;
        if (i == 30) {
           *tags |= CHAN_ADR_OUT;
           bus_out = 0x91;
        }
        if (i == 31)
           sel = 1;
        if (sel) {
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        }
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x91, bus_in);
           *tags &= ~CHAN_ADR_OUT;
           *tags &= ~CHAN_SUP_OUT;
           bus_out = (cmd & 0xff) | odd_parity[cmd & 0xff];
           *tags |= CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           *tags &= ~CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_ADR_OUT)) == (CHAN_STA_IN|CHAN_ADR_OUT)) {
           log_trace("Unit busy %02x\n", bus_in);
           status = bus_in;
           bus_out = 0x100;
           sts = 0;
           *tags &= ~(CHAN_ADR_OUT);
           *tags |= CHAN_SRV_OUT;
           for (i = 0; i < 500; i++) {
               step_2844((struct _2844_context *)dev->dev);
               step_2844((struct _2844_context *)dev->dev);
               advance();
               step_count++;
               dev->bus_func(dev, tags, bus_out, &bus_in);
               if ((*tags & (CHAN_STA_IN)) == 0 && !sts) {
                   *tags &= ~(CHAN_SRV_OUT);
                   *tags |= (CHAN_SEL_OUT);
               }
               if ((*tags & (CHAN_OPR_IN|CHAN_ADR_IN)) ==
                            (CHAN_OPR_IN|CHAN_ADR_IN) &&
                    bus_in == 0x91) {
                   *tags |= (CHAN_CMD_OUT);
                    bus_out = (cmd & 0xff) | odd_parity[cmd & 0xff];
               }
               if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
                  log_trace("Drop command out\n");
                  bus_out = 0x100;
                  *tags &= ~CHAN_CMD_OUT;
               }
               if ((*tags & (CHAN_STA_IN)) != 0 && bus_in == 0x20) {
                   *tags |= (CHAN_SRV_OUT);
                   sts = 1;
               }
               if ((*tags & (CHAN_STA_IN)) == 0 && sts) {
                   *tags &= ~(CHAN_SRV_OUT);
                   *tags |= (CHAN_SEL_OUT);
               }
           }
           break;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in %02x\n", bus_in);
           bus_out = 0x100;
           status = bus_in;
           *tags |= CHAN_SRV_OUT;
           sts = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           sel = 0;
        }
        if (sts && (*tags & (CHAN_STA_IN|CHAN_SRV_IN)) == (0)) {
           log_trace("Service out drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           break;
        }
    }
    return status;
}

uint16_t
read_data(struct _device *dev, uint16_t *tags, uint8_t *data, int *num, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 1;
    int         byte = 0;
    int         sta_in = 0;

    bus_out = 0x100;
    log_trace("Read data\n");
    for (i = 0; i < 30000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           status = bus_in;   /* Device end and channel end */
           *tags |= CHAN_SRV_OUT;
           if (cc)
               *tags |= CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           *tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
               break;
           }
        }
        if ((*tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           if (byte <= *num) {
               data[byte] = bus_in;
           }
           log_trace("Service in %03x %02x\n", bus_in, byte);
           byte++;
           *tags |= (CHAN_SRV_OUT);
        }
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    *num = byte;
    return status;
}

uint16_t
write_data(struct _device *dev, uint16_t *tags, uint8_t *data, int *num, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 1;
    int         byte = 0;
    int         sta_in = 0;

    log_trace("Write data\n");
    bus_out = 0x100;
    for (i = 0; i < 50000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & CHAN_STA_IN) == CHAN_STA_IN) {
           status = bus_in;   /* Device end and channel end */
           log_trace("Status in %02x\n", status);
           bus_out = 0x100;
           *tags |= CHAN_SRV_OUT;
           if (cc)
               *tags |= CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           if (sta_in && cc == 0) {
               log_trace("Drop select out write data\n");
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
           /* Check data chaining end */
           if (sta_in && cc == 1 && (status & 0x4) != 0) {
               break;
           }
        }
        if ((*tags & (CHAN_SRV_IN|CHAN_CMD_OUT)) == (CHAN_CMD_OUT)) {
           log_trace("Command in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_CMD_OUT);
        }
        if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           if (byte <= *num) {
               bus_out = data[byte];
               bus_out |= odd_parity[bus_out];
           }
           log_trace("Service in %03x %02x %02x\n", bus_in, data[byte], byte);
           byte++;
           if (byte > *num) {
               *tags |= CHAN_CMD_OUT;
           } else {
               *tags |= (CHAN_SRV_OUT);
           }
        }
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    log_trace("Write data end\n");
    *num = byte;
    return status;
}

uint16_t
wait_dev(struct _device *dev, uint16_t *tags, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 0;
    int         sta = 0;

    *tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT);
    if ((*tags & (CHAN_OPR_IN)) != 0) {
        *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        sel = 1;
    } else {
        sel = 0;
    }
    for (i = 0; i < 30000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
           sel = 0;
           if (sta && (status & 0x4) != 0) {
               break;
           }
        }
        if ((*tags & (CHAN_REQ_IN)) != 0) {
           sel = 1;
        }
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;

        if ((*tags & (CHAN_ADR_IN)) != 0) { // && bus_in == 0x190) {
           log_trace("Address in %02x\n", bus_in);
           *tags |= CHAN_CMD_OUT;
           bus_out = 0x100;
        }

        if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           *tags &= ~CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           if (cc)
               *tags |= CHAN_SUP_OUT;
           *tags |= CHAN_SRV_OUT;
           sta = 1;
           status = bus_in;
           log_trace("Status in %02x\n", status);
        }
        if ((*tags & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           *tags &= ~CHAN_SRV_OUT;
           sel = 0;
        }
    }
    return status;
}


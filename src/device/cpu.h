/*
 * microsim360 - CPU external front panel switch definitions.
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


#include <stdint.h>

#ifndef _CPU_H_
#define _CPU_H_

extern int      SYS_RST;
extern int      ROAR_RST;
extern int      START;
extern int      SET_IC;
extern int      CHECK_RST;
extern int      STOP;
extern int      INT_TMR;
extern int      STORE;
extern int      DISPLAY;
extern int      LAMP_TEST;
extern int      POWER;
extern int      INTR;
extern int      LOAD;
extern int      timer_event;
extern uint32_t ADR_CMP;
extern uint32_t INST_REP;
extern uint32_t ROS_CMP;
extern uint32_t ROS_REP;
extern uint32_t SAR_CMP;
extern uint32_t FORC_IND;
extern uint32_t FLT_MODE;
extern uint32_t CHN_MODE;
extern uint8_t  SEL_SW;
extern int      SEL_ENTER;

extern uint8_t  A_SW;
extern uint8_t  B_SW;
extern uint8_t  C_SW;
extern uint8_t  D_SW;
extern uint8_t  E_SW;
extern uint8_t  F_SW;
extern uint8_t  G_SW;
extern uint8_t  H_SW;
extern uint8_t  J_SW;

extern uint8_t  PROC_SW;
extern uint8_t  RATE_SW;
extern uint8_t  CHK_SW;
extern uint8_t  MATCH_SW;
extern uint8_t  STORE_SW;

extern uint8_t  wait;
extern uint8_t  test_mode;
extern uint8_t  load_mode;

extern uint32_t *M;
extern uint32_t mem_max;     /* 8K = 0x1FFF, 16K = 0x3FFF, 32K = 0x7FF, 64k = 0xFFFF */
#endif

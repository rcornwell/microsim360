/*
 * microsim360 - Test framework caller function.
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

#define CTEST_MAIN
#define CTEST_NO_COLORS

#include <stdint.h>
#include <stdio.h>
#include "logger.h"
#include "ctest.h"
#include "cpu.h"
#include "conf.h"
#include "model2030.h"
#include "model1052.h"

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

uint32_t *M;
uint32_t mem_max;

/* The following functions are referenced by the 2030 simulator, 
   but do not need to preform any function during unit test.
*/
void
setup_fp2030(void *rend)
{
}

void
model1052_out(void *ctx, uint16_t out_char)
{
}

void
model1052_in(void *ctx, uint16_t *in_char)
{
}

void
model1052_func(void *ctx, uint16_t *tags_out, uint16_t tags_in, uint16_t *t_request)
{
}

void *
model1052_init_ctx(uint16_t port)
{
    return NULL;
}

int
main(int argc, const char *argv[])
{
    load_line("2030F/1");
    log_level = 0x37f;
    log_init("debug.log");
    int result = ctest_main(argc, argv);
    return result;
}

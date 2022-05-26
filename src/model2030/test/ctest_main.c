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

#include <stdio.h>
#include "logger.h"

#define CTEST_MAIN

#define CTEST_SEGFAULT
#define CTEST_NO_COLORS

#include "ctest.h"

int SYS_RST;
int ROAR_RST;
int START;
int SET_IC;
int CHECK_RST;
int STOP;
int INT_TMR;
int STORE;
int DISPLAY;
int LAMP_TEST;
int POWER;
int INTR;
int LOAD;
int timer_event;

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


int main(int argc, const char *argv[])
{
    log_level = 0x27f;
    log_init("debug.log");
    int result = ctest_main(argc, argv);
    return result;
}

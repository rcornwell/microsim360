/*
 * microsim360 - Model 2030 CPU instruction test cases.
 *
 * Copyright 2022, Richard Cornwell
 *                 Original test cases by Ken Shirriff
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

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "ctest.h"
#include "xlat.h"
#include "logger.h"
#include "cpu.h"
#include "model_test.h"


uint64_t         step_count;
int              testcycles = 100;

#define FTEST(a, b)   CTEST_SKIP(a, b)
#define DTEST(a, b)   CTEST(a, b)
#define MTEST(a, b)   CTEST_SKIP(a, b)

/* Set MASK */
void set_mask(uint8_t mask)
{
    MASK = mask;
    cpu_2030.LS[0x7b8] = mask;
    cpu_2030.LS[0x7b8] |= odd_parity[cpu_2030.LS[0x7b8]];
}

/* Get MASK */
uint8_t get_mask()
{
    return cpu_2030.LS[0x7b8] & 0xff;
}

/* Set ILC code */
void
set_ilc(int num)
{
    cpu_2030.LS[0x78C] = (1 << num);
    cpu_2030.LS[0x78c] |= odd_parity[cpu_2030.LS[0x78c]];
}

/* get ILC code */
int
get_ilc()
{
    return (cpu_2030.LS[0x78C]);
}

/* Set AMWP */
void
set_amwp(int num)
{
    cpu_2030.ASCII = (num & 0x8) != 0;
    cpu_2030.LS[0x7b9] &= 0xf0;
    cpu_2030.LS[0x7b9] |= num;
    cpu_2030.LS[0x7b9] |= odd_parity[cpu_2030.LS[0x7b9]];
}

/* Get AMWP */
int
get_amwp()
{
    return cpu_2030.LS[0x7b9] & 0x0f;
}

/* Set key */
void
set_key(int num)
{
    cpu_2030.LS[0x7b9] &= 0x0f;
    cpu_2030.LS[0x7b9] |= (num << 4);
    cpu_2030.LS[0x7b9] |= odd_parity[cpu_2030.LS[0x7b9]];
    cpu_2030.Q_REG &= 0x0f;
    cpu_2030.Q_REG |= (num << 4);
}

/* Get key */
int
get_key()
{
    return ((cpu_2030.Q_REG >> 4) & 0x0f);
}

void
set_cc(int cc)
{
    cpu_2030.LS[0x7bb] &= 0x0f;
    cpu_2030.LS[0x7bb] |= cc;
    cpu_2030.LS[0x7bb] |= odd_parity[cpu_2030.LS[0x7bb]];
}

/* Read register */
uint32_t
get_reg(int num)
{
    uint32_t    v;
    int         r = num << 4;
    v  = (cpu_2030.LS[r + 0 + 0x700] & 0xff) << 24;
    v |= (cpu_2030.LS[r + 1 + 0x700] & 0xff) << 16;
    v |= (cpu_2030.LS[r + 2 + 0x700] & 0xff) << 8;
    v |= (cpu_2030.LS[r + 3 + 0x700] & 0xff) << 0;
    return v;
}

/* Write register */
void
set_reg(int num, uint32_t data)
{
    int    r = num << 4;
    int    i;
    cpu_2030.LS[r + 0 + 0x700] = (data >> 24) & 0xff;
    cpu_2030.LS[r + 1 + 0x700] = (data >> 16) & 0xff;
    cpu_2030.LS[r + 2 + 0x700] = (data >> 8) & 0xff;
    cpu_2030.LS[r + 3 + 0x700] = (data >> 0) & 0xff;
    for (i = 0; i < 4; i++) {
        cpu_2030.LS[r + i + 0x700] |= odd_parity[cpu_2030.LS[r + i + 0x700]];
    }
}

/* Read word from main memory */
uint32_t
get_mem(int addr)
{
    uint32_t data;
    data  = (M[addr + 0] & 0xff) << 24;
    data |= (M[addr + 1] & 0xff) << 16;
    data |= (M[addr + 2] & 0xff) << 8;
    data |= (M[addr + 3] & 0xff) << 0;
    return data;
}

/* Set word into main memory */
void
set_mem(int addr, uint32_t data)
{
    int  i;
    M[addr + 0] = (data >> 24) & 0xff;
    M[addr + 1] = (data >> 16) & 0xff;
    M[addr + 2] = (data >> 8) & 0xff;
    M[addr + 3] = (data >> 0) & 0xff;
    for (i = 0; i < 4; i++) {
        M[addr + i] |= odd_parity[M[addr + i]];
    }
}

/* Get the memory protection key for a given address */
uint8_t
get_mem_key(int addr)
{
    return cpu_2030.MP[0xe0| ((addr & 0xf800) >> 11)];
}

/* Set the memory protection key for a given address */
void
set_mem_key(int addr, int key)
{
    cpu_2030.MP[0xe0| ((addr & 0xf800) >> 11)] = key;
}

/* Read byte from main memory */
uint8_t
get_mem_b(int addr)
{
    return (M[addr] & 0xff);
}

/* Set byte into main memory */
void
set_mem_b(int addr, uint8_t data)
{
    M[addr] = data & 0xff;
    M[addr] |= odd_parity[data & 0xff];
}

/* Get a floating point register */
uint32_t
get_fpreg_s(int num)
{
    uint32_t    v;
    int         r = num << 4;
    if (num & 1)
       r += 4;
    v  = (uint32_t)(cpu_2030.LS[r + 8 + 0x700] & 0xff) << 24;
    v |= (uint32_t)(cpu_2030.LS[r + 9 + 0x700] & 0xff) << 16;
    v |= (uint32_t)(cpu_2030.LS[r + 10 + 0x700] & 0xff) << 8;
    v |= (uint32_t)(cpu_2030.LS[r + 11 + 0x700] & 0xff) << 0;
    return v;
}

/* Set a floating point register short */
void
set_fpreg_s(int num, uint32_t data)
{
    int    r = num << 4;
    int    i;
    if (num & 1)
       r += 4;
    cpu_2030.LS[r + 8 + 0x700] = (data >> 24) & 0xff;
    cpu_2030.LS[r + 9 + 0x700] = (data >> 16) & 0xff;
    cpu_2030.LS[r + 10 + 0x700] = (data >> 8) & 0xff;
    cpu_2030.LS[r + 11 + 0x700] = (data >> 0) & 0xff;
    r = num << 4;
    for (i = 8; i < 16; i++) {
        cpu_2030.LS[r + i] = (cpu_2030.LS[r + i + 0x700] & 0xff) |odd_parity[cpu_2030.LS[r + i + 0x700] & 0xff];
    }
}

/* Get a floating point register */
uint64_t
get_fpreg_d(int num)
{
    uint64_t    v = 0;
    int         i;
    int         s = 56;
    int         r = num << 4;
    for (i = 8; i < 16; i++, s -= 8) {
       v |= (uint64_t)(cpu_2030.LS[r + i + 0x700] & 0xff) << s;
    }
    return v;
}

/* Set a floating point register short */
void
set_fpreg_d(int num, uint64_t data)
{
    int    r = num << 4;
    int    i;
    int         s = 56;
    for (i = 8; i < 16; i++, s -= 8) {
        cpu_2030.LS[r + i + 0x700] = (data >> s) & 0xff;
        cpu_2030.LS[r + i + 0x700] |= odd_parity[cpu_2030.LS[r + i + 0x700] & 0xff];
    }
}


/* Return a random floating point number scaled roughly
   to 2**-powRange to 2**powRange */
double
randfloat(int powRange)
{
    double f = (double)(rand() + rand()) / pow(2, 32);
    double p = (double)rand() / ((double)RAND_MAX);
    int pw = (int)(p * (double)powRange * 2.0) - powRange;
    int    s = rand();
    f = f * pow(2, (double)pw) * 4;
    if (s < (RAND_MAX/2)) {
       f = -f;
    }
    return f;
}

/* Initialize the CPU to run tests */
void
init_cpu()
{
    SYS_RST = 1;
    CHK_SW = 2;
    RATE_SW = 1;
    PROC_SW = 1;
    mem_max = 0xffff;
    do {
        cycle_2030();
        step_count++;
    } while (cpu_2030.WX != 0x328);
}

int trap_flag;

/* Run one instructions */
void
test_inst(int mask)
{
    cpu_2030.LS[0x7AA] = 0x100;
    cpu_2030.LS[0x7A9] = 0x04;
    cpu_2030.LS[0x7BB] = mask | (cpu_2030.LS[0x7BB] & 0xf0);
    cpu_2030.LS[0x7BB] |= odd_parity[cpu_2030.LS[0x7BB]];
    trap_flag = 0;
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("first\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("second\n");
}

/* Execute two instructions */
void
test_inst2()
{
    cpu_2030.LS[0x7AA] = 0x100;
    cpu_2030.LS[0x7A9] = 0x04;
    cpu_2030.LS[0x7BB] = 0x100;
    set_cc(CC3);
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("first\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("second\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("third\n");
}

/* Convert a floating point value to a 64-bit FP register. */
int floatToFpreg(int num, double val)
{
    uint64_t s = 0;
    uint64_t f;
    uint8_t charac = 64;

    if (val == 0) {
        set_fpreg_d(num, 0);
        return 0;
    }
    if (val < 0) {
        s = 0x8000000000000000LL;
        val = -val;
    }
    while (val >= 1 && charac < 128) {
        charac += 1;
        val /= 16;
    }
    while (val < 1/16. && charac >= 0) {
        charac -= 1;
        val *= 16;
    }
    if (charac < 0 || charac >= 128) {
        return 1;
    }
    val *= 1 << 24;
    f = s | ((uint64_t)charac << 56) | ((uint64_t)val) << 32;
    f |= (uint64_t)((val - (uint32_t)val) * (1LL << 32));
    set_fpreg_d(num, f);
    return 0;
}

/* load floating point register as double */
double cnvt_32_float(int num)
{
    uint32_t        t32 = get_fpreg_s(num);
    double          d;
    int             e;
    e = ((t32 >> 24) & 0x7f) - 64;
    d = (double)(((uint64_t)(0x00ffffff & t32)) << 32);
    d *= exp2(-56 + 4*e);
    if ((0x80000000 & t32) != 0)
        d *= -1.0;
    return d;
}

/* load floating point register as double */
double cnvt_64_float(int num)
{
    uint64_t        t64 = get_fpreg_d(num);
    double          d;
    int             e;
    e = ((t64 >> 56) & 0x7f) - 64;
    d = (double)(0x00ffffffffffffffLL & t64);
    d *= exp2(-56 + 4*e);
    if ((0x8000000000000000LL & t64) != 0)
        d *= -1.0;
    return d;
}

/* Run instructions until execute of 0 opcode */
void
test_io_inst(int mask)
{
    int    stop_flag = 0;
    cpu_2030.LS[0x7AA] = 0x100;
    cpu_2030.LS[0x7A9] = 0x04;
    cpu_2030.LS[0x7BB] = mask | (cpu_2030.LS[0x7BB] & 0xf0);
    cpu_2030.LS[0x7BB] |= odd_parity[cpu_2030.LS[0x7BB]];
    trap_flag = 0;
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    log_trace("Do io\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
        if (cpu_2030.WX == 0x2E0 && cpu_2030.R_REG == 0x100)
            stop_flag = 1;
    } while (stop_flag == 0);
    log_trace("first\n");
}

/* Execute two instructions */
void
test_io_inst2()
{
    cpu_2030.LS[0x7AA] = 0x100;
    cpu_2030.LS[0x7A9] = 0x04;
    cpu_2030.LS[0x7BB] = 0x100;
    set_cc(CC3);
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("first\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("second\n");
    do {
        cycle_2030();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
    } while (cpu_2030.WX != 0x100);
    log_trace("third\n");
}


#include "inst_test_cases.h"

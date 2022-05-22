/*
 * microsim360 - Model 2065 CPU instruction test cases.
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

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ctest.h"
#include "xlat.h"
#include "logger.h"
#include "model2065.h"

#define CC_REG cpu_2065.CC
#define CC0    0x0
#define CC1    0x1
#define CC2    0x2
#define CC3    0x3

#define FTEST(a, b)   CTEST(a, b)
#define DTEST(a, b)   CTEST(a, b)

#define IAR  (cpu_2065.IA_REG)

#define MASK cpu_2065.MASK

#define PM            cpu_2065.PMASK

#define set_ilc(n)    cpu_2065.ILC = n

#define get_ilc()     cpu_2065.ILC

void
set_amwp(int n) {
   cpu_2065.LS[0x17] &= 0xfff0ffff;
   cpu_2065.LS[0x17] |= (n << 16);
   cpu_2065.AMWP = n;
}

#define get_amwp()    cpu_2065.AMWP

void
set_key(int n) {
    cpu_2065.LS[0x17] = (MASK << 24) | (n << 20);
    cpu_2065.KEY = n;
}

#define get_key()     cpu_2065.KEY

#define set_cc(n)     CC_REG = n
uint64_t         step_count;
int              testcycles = 100;
int              irq_mask = 0xff;

/* Read register */
uint32_t
get_reg(int num)
{
    return cpu_2065.LS[num + 0x30];
}

/* Write register */
void
set_reg(int num, uint32_t data)
{
    cpu_2065.LS[num + 0x30] = data;
}

/* Read word from main memory */
uint32_t
get_mem(int addr)
{
    return cpu_2065.M[addr >> 2];
}

/* Set word into main memory */
void
set_mem(int addr, uint32_t data)
{
    cpu_2065.M[addr >> 2] = data;
}

/* Get the memory protection key for a given address */
uint8_t
get_mem_key(int addr)
{
    return cpu_2065.MP[((addr & 0xf800) >> 11)];
}

/* Set the memory protection key for a given address */
void
set_mem_key(int addr, int key)
{
    cpu_2065.MP[((addr & 0xf800) >> 11)] = key;
}

/* Read byte from main memory */
uint8_t
get_mem_b(int addr)
{
    uint32_t   v = cpu_2065.M[addr >> 2];
    return (uint8_t)(v >> (8 * (3 - (addr & 3))) & 0xff);
}

/* Set byte into main memory */
void
set_mem_b(int addr, uint8_t data)
{
    int        offset = 8 * (3 - (addr & 3));
    uint32_t   mask = 0xff;
    cpu_2065.M[addr>>2] &= ~(mask << offset);
    cpu_2065.M[addr>>2] |= ((uint32_t)data << offset);
}

/* Get a floating point register */
uint32_t
get_fpreg_s(int num)
{
    return cpu_2065.LS[num + 0x20];
}

/* Set a floating point register short */
void
set_fpreg_s(int num, uint32_t data)
{
    cpu_2065.LS[num + 0x20] = data;
}

/* Get a floating point register */
uint64_t
get_fpreg_d(int num)
{
    uint64_t    v;
    v =  (uint64_t)cpu_2065.LS[num + 0x20] << 32;
    v |=  (uint64_t)cpu_2065.LS[num + 0x21];
    return v;
}

/* Set a floating point register short */
void
set_fpreg_d(int num, uint64_t data)
{
    cpu_2065.LS[num + 0x20] = (uint32_t)(data >> 32);
    cpu_2065.LS[num + 0x21] = (uint32_t)(data & 0xffffffff);
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

/* Return a random floating point number scaled roughly
   to 2**-powRange to 2**powRange */
double
randfloat(unsigned int *seed, int powRange)
{
    double f = (double)(rand_r(seed) + rand_r(seed)) / pow(2, 32);
    double p = (double)rand_r(seed) / ((double)RAND_MAX);
    int pw = (int)(p * (double)powRange * 2.0) - powRange;
    int    s = rand_r(seed);
    f = f * pow(2, (double)pw) * 4;
    if (s < (RAND_MAX/2)) {
       f = -f;
    }
    return f;
}

void
init_cpu()
{
    SYS_RST = 0;
    CHK_SW = 2;
    RATE_SW = 1;
    PROC_SW = 1;
    set_amwp(0);
#if 0
    do {
        cycle();
        step_count++;
    } while (cpu_2030.ROAR != 0x328);
#endif
}

int trap_flag;

void
test_inst(int mask)
{
    int      max = 0;
    cpu_2065.IA_REG = 0x400;
    cpu_2065.PMASK = (mask & 0xf);
    trap_flag = 0;
    cpu_2065.ROAR = 0x190;
    cpu_2065.REFETCH = 1;
    cpu_2065.mem_state = 0;
        log_trace("Start inst\n");
    do {
        cycle_2065();
        step_count++;
        max++;
        if ((cpu_2065.ROAR & 0xffc) == 0x148)
           break;
        if ((cpu_2065.ROAR == 0x188) && (cpu_2065.SDR_REG == 0))
           break;
        if (cpu_2065.ROAR == 0x10e)
           trap_flag = 1;
        log_trace("ROAR = [%03X]\n", cpu_2065.ROAR);
    } while (max < 500);
    if (max > 200)
        log_trace("overrun\n");

}

void
test_inst2()
{
    int      max = 0;
    int      count;
    cpu_2065.IA_REG = 0x400;
    cpu_2065.PMASK = 0;
    cpu_2065.ROAR = 0x190;
    cpu_2065.REFETCH = 1;
    cpu_2065.mem_state = 0;
    trap_flag = 0;
    count = 0;
    do {
        cycle_2065();
        step_count++;
        max++;
        log_trace("ROAR = [%03X]\n", cpu_2065.ROAR);
        if ((cpu_2065.ROAR & 0xffc) == 0x148) {
           if (count++ == 2)
               break;
        }
        if ((cpu_2065.ROAR == 0x188) && (cpu_2065.SDR_REG == 0))
           break;
        if (cpu_2065.ROAR == 0x10e)
           trap_flag = 1;
    } while (max < 500);
}

#include "inst_test_cases.h"

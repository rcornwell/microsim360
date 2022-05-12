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
#include <stdint.h>
#include <stdlib.h>

#include <SDL.h>
#include "ctest.h"
#include "xlat.h"
#include "logger.h"
#include "model2030.h"

#define CC_REG (cpu_2030.LS[0xBB] & 0xf0)
#define CC0    0x80
#define CC1    0x40
#define CC2    0x20
#define CC3    0x10

#define PM     (cpu_2030.LS[0xBB] & 0x0f)

#define IAR   (((cpu_2030.I_REG & 0xff) << 8) | (cpu_2030.J_REG & 0xff))

#define MASK   cpu_2030.MASK

uint64_t         step_count;
int              testcycles = 100;

/* Null function calls for functions that are not used in test */
DECLSPEC SDL_Thread *SDLCALL
SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) 
{
     return NULL;
}

void SDL_WaitThread(SDL_Thread * thread, int *status)
{
}

/* Set ILC code */
void
set_ilc(int num)
{
    cpu_2030.LS[0x8C] = (1 << num);
    cpu_2030.LS[0x8c] |= odd_parity[cpu_2030.LS[0x8c]];
}

/* get ILC code */
int
get_ilc()
{
    return (cpu_2030.LS[0x8C]);
}

/* Set AMWP */
void
set_amwp(int num)
{
    cpu_2030.ASCII = (num & 0x8) != 0; 
    cpu_2030.LS[0xb9] &= 0xf0;
    cpu_2030.LS[0xb9] |= num;
    cpu_2030.LS[0xb9] |= odd_parity[cpu_2030.LS[0xb9]];
}

/* Get AMWP */
int
get_amwp()
{
    return cpu_2030.LS[0xb9] & 0x0f;
}

/* Set key */
void
set_key(int num)
{
    cpu_2030.LS[0xb9] &= 0x0f;
    cpu_2030.LS[0xb9] |= (num << 4);
    cpu_2030.LS[0xb9] |= odd_parity[cpu_2030.LS[0xb9]];
}

/* Get key */
int
get_key()
{
    return (cpu_2030.LS[0xb9] & 0xf0) >> 4;
}

void
set_cc(int cc)
{
    cpu_2030.LS[0xbb] &= 0x0f;
    cpu_2030.LS[0xbb] |= cc;
    cpu_2030.LS[0xbb] |= odd_parity[cpu_2030.LS[0xbb]];
}

/* Read register */
uint32_t
get_reg(int num)
{
    uint32_t    v;
    int         r = num << 4;
    v  = (cpu_2030.LS[r + 0] & 0xff) << 24;
    v |= (cpu_2030.LS[r + 1] & 0xff) << 16;
    v |= (cpu_2030.LS[r + 2] & 0xff) << 8;
    v |= (cpu_2030.LS[r + 3] & 0xff) << 0;
    return v;
}

/* Write register */
void
set_reg(int num, uint32_t data)
{
    int    r = num << 4;
    int    i;
    cpu_2030.LS[r + 0] = (data >> 24) & 0xff;
    cpu_2030.LS[r + 1] = (data >> 16) & 0xff;
    cpu_2030.LS[r + 2] = (data >> 8) & 0xff;
    cpu_2030.LS[r + 3] = (data >> 0) & 0xff;
    for (i = 0; i < 4; i++) {
        cpu_2030.LS[r + i] |= odd_parity[cpu_2030.LS[r + i]];
    }
}

/* Read word from main memory */
uint32_t
get_mem(int addr)
{
    uint32_t data;
    data  = (cpu_2030.M[addr + 0] & 0xff) << 24;
    data |= (cpu_2030.M[addr + 1] & 0xff) << 16;
    data |= (cpu_2030.M[addr + 2] & 0xff) << 8;
    data |= (cpu_2030.M[addr + 3] & 0xff) << 0;
    return data;
}

/* Set word into main memory */
void
set_mem(int addr, uint32_t data)
{
    int  i;
    cpu_2030.M[addr + 0] = (data >> 24) & 0xff;
    cpu_2030.M[addr + 1] = (data >> 16) & 0xff;
    cpu_2030.M[addr + 2] = (data >> 8) & 0xff;
    cpu_2030.M[addr + 3] = (data >> 0) & 0xff;
    for (i = 0; i < 4; i++) {
        cpu_2030.M[addr + i] |= odd_parity[cpu_2030.M[addr + i]];
    }
}

/* Get a floating point register */
uint32_t
get_fpreg_s(int num)
{
    uint32_t    v;
    int         r = num << 4;
    if (num & 1)
       r += 4;
    v  = (uint32_t)(cpu_2030.LS[r + 8] & 0xff) << 24;
    v |= (uint32_t)(cpu_2030.LS[r + 9] & 0xff) << 16;
    v |= (uint32_t)(cpu_2030.LS[r + 10] & 0xff) << 8;
    v |= (uint32_t)(cpu_2030.LS[r + 11] & 0xff) << 0;
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
    cpu_2030.LS[r + 8] = (data >> 24) & 0xff;
    cpu_2030.LS[r + 9] = (data >> 16) & 0xff;
    cpu_2030.LS[r + 10] = (data >> 8) & 0xff;
    cpu_2030.LS[r + 11] = (data >> 0) & 0xff;
    r = num << 4;
    for (i = 8; i < 16; i++) {
        cpu_2030.LS[r + i] = (cpu_2030.LS[r + i] & 0xff) |odd_parity[cpu_2030.LS[r + i] & 0xff];
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
       v |= (uint64_t)(cpu_2030.LS[r + i] & 0xff) << s;
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
        cpu_2030.LS[r + i] = (data >> s) & 0xff;
        cpu_2030.LS[r + i] |= odd_parity[cpu_2030.LS[r + i] & 0xff];
    }
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
    SYS_RST = 1;
    CHK_SW = 2;
    RATE_SW = 1;
    PROC_SW = 1;
    do {
        cycle();
        step_count++;
    } while (cpu_2030.WX != 0x328);
}

int trap_flag;

void
test_inst(int mask)
{
    cpu_2030.LS[0xAA] = 0x100;
    cpu_2030.LS[0xA9] = 0x04;
    cpu_2030.LS[0xBB] = mask | odd_parity[mask];
    trap_flag = 0;
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    do {
        cycle();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
log_trace("WX = [%03X]\n", cpu_2030.WX);
    } while (cpu_2030.WX != 0x100);
log_trace("first\n");
    do {
        cycle();
        step_count++;
        if (cpu_2030.WX == 0x147)
           trap_flag = 1;
log_trace("WX = [%03X]\n", cpu_2030.WX);
    } while (cpu_2030.WX != 0x100);
log_trace("second\n");
}

void
test_inst2()
{
    cpu_2030.LS[0xAA] = 0x100;
    cpu_2030.LS[0xA9] = 0x04;
    cpu_2030.LS[0xBB] = 0x100;
    cpu_2030.WX = 0x102;
    START = 1;
    cpu_2030.I_REG = 0x4;
    cpu_2030.J_REG = 0x100;
    do {
        cycle();
        step_count++;
log_trace("WX = [%03X]\n", cpu_2030.WX);
    } while (cpu_2030.WX != 0x100);
log_trace("first\n");
    do {
        cycle();
        step_count++;
log_trace("WX = [%03X]\n", cpu_2030.WX);
    } while (cpu_2030.WX != 0x100);
log_trace("second\n");
    do {
        cycle();
        step_count++;
log_trace("WX = [%03X]\n", cpu_2030.WX);
    } while (cpu_2030.WX != 0x100);
log_trace("third\n");
}

  CTEST( instruct, fp_conversion) {
    ASSERT_EQUAL(0, floatToFpreg(0, 0.0));
    ASSERT_EQUAL(0, get_fpreg_s(0));
    ASSERT_EQUAL(0, get_fpreg_s(1));

    /* From Princ Ops page 157 */
    ASSERT_EQUAL(0, floatToFpreg(0, 1.0));
    ASSERT_EQUAL(0x41100000, get_fpreg_s(0));
    ASSERT_EQUAL(0, get_fpreg_s(1));

    ASSERT_EQUAL(0, floatToFpreg(0, 0.5));
    ASSERT_EQUAL(0x40800000, get_fpreg_s(0));
    ASSERT_EQUAL(0, get_fpreg_s(1));

    ASSERT_EQUAL(0, floatToFpreg(0, 1.0/64.0));
    ASSERT_EQUAL(0x3f400000, get_fpreg_s(0));
    ASSERT_EQUAL(0, get_fpreg_s(1));

    ASSERT_EQUAL(0, floatToFpreg(0, -15.0));
    ASSERT_EQUAL(0xc1f00000, get_fpreg_s(0));
    ASSERT_EQUAL(0, get_fpreg_s(1));
  }

  CTEST(instruct, fp_32_conversion) {
    ASSERT_EQUAL(0, floatToFpreg(0, 0.0));

    set_fpreg_s(0, 0xff000000);
    ASSERT_EQUAL(0.0, cnvt_32_float(0));

    set_fpreg_s(0, 0x41100000);
    ASSERT_EQUAL(1.0, cnvt_32_float(0));

    set_fpreg_s(0, 0x40800000);
    ASSERT_EQUAL(0.5, cnvt_32_float(0));

    set_fpreg_s(0, 0x3f400000);
    ASSERT_EQUAL(1.0/64.0, cnvt_32_float(0));

    set_fpreg_s(0, 0xc1f00000);
    ASSERT_EQUAL(-15.0, cnvt_32_float(0));

    unsigned int seed = 1;
    for (int i = 0; i < 20; i++) {
      double f = rand_r(&seed) / (double)(RAND_MAX);
      int p = (rand_r(&seed) / (double)(RAND_MAX) * 400) - 200;
      f = f * pow(2, p);
      if (rand_r(&seed) & 1) {
        f = -f;
      }
      (void)floatToFpreg(0, f);
      double fp = cnvt_32_float(0);
      // Compare within tolerance
      double ratio = abs((fp - f) / f);
      ASSERT_TRUE(ratio < .000001);
    }
  }

  CTEST(instruct, fp_64_conversion) {
    ASSERT_EQUAL(0, floatToFpreg(0, 0.0));

    set_fpreg_s(0, 0xff000000);
    set_fpreg_s(1, 0);
    ASSERT_EQUAL(0.0, cnvt_64_float(0));

    set_fpreg_s(0, 0x41100000);
    set_fpreg_s(1, 0);
    ASSERT_EQUAL(1.0, cnvt_64_float(0));

    set_fpreg_s(0, 0x40800000);
    set_fpreg_s(1, 0);
    ASSERT_EQUAL(0.5, cnvt_64_float(0));

    set_fpreg_s(0, 0x3f400000);
    set_fpreg_s(1, 0);
    ASSERT_EQUAL(1.0/64.0, cnvt_64_float(0));

    set_fpreg_s(0, 0xc1f00000);
    set_fpreg_s(1, 0);
    ASSERT_EQUAL(-15.0, cnvt_64_float(0));

    unsigned int seed = 1;
    for (int i = 0; i < 20; i++) {
      double f = rand_r(&seed) / (double)(RAND_MAX);
      int p = (rand_r(&seed) / (double)(RAND_MAX) * 400) - 200;
      f = f * pow(2, p);
      if (rand_r(&seed) & 1) {
        f = -f;
      }
      (void)floatToFpreg(0, f);
      double fp = cnvt_64_float(0);
      ASSERT_EQUAL(f, fp);
    }
  }

  /* Roughly test characteristics of random number generator */
  CTEST(instruct, randfloat) {
    unsigned int seed = 5;
    int pos = 0, neg = 0;
    int big = 0, small = 0;
    for (int i = 0; i < 100; i++) {
      double f = randfloat(&seed, 200);
      if (f < 0) {
        neg ++;
      } else {
        pos ++;
      }
      if (fabs(f) > pow(2, 100)) {
        big++;
      } else if (fabs(f) < pow(2, -100)) {
        small++;
      } 
    }
    ASSERT_TRUE(pos > 30);
    ASSERT_TRUE(neg > 30);
    ASSERT_TRUE(big > 15);
    ASSERT_TRUE(small > 15);

    /* Test scaling */
    big = 0;
    small = 0;
    for (int i = 0; i < 100; i++) {
      double f = randfloat(&seed, 10);
      if (f < 0) {
        neg ++;
      } else {
        pos ++;
      }
      if (fabs(f) > pow(2, 10)) {
        big++;
      } else if (fabs(f) < pow(2, -10)) {
        small++;
      } 
    }
    ASSERT_TRUE(big < 8);
    ASSERT_TRUE(small < 8);
  }

  CTEST(instruct, load_reg) {
    init_cpu();
    set_mem(0x400, 0x18310000);
    set_reg(1, 0x12345678);
    test_inst(0);
    ASSERT_EQUAL(0x12345678, get_reg(3));
    ASSERT_EQUAL(0x100, CC_REG);
  }

  CTEST(instruct, loadtest_reg) {
    init_cpu();
    set_mem(0x400, 0x12340000);
    /* Test negative number */
    set_reg(4, 0xcdef1234);
    test_inst(0);
    ASSERT_EQUAL(0xcdef1234, get_reg(3));
    ASSERT_EQUAL(CC1, CC_REG);
    /* Test zero */
    set_reg(4, 0x00000000);
    test_inst(0);
    ASSERT_EQUAL(0x0, get_reg(3));
    ASSERT_EQUAL(CC0, CC_REG);
    /* Test positive number */
    set_reg(4, 0x12345678);
    test_inst(0);
    ASSERT_EQUAL(0x12345678, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG);
  }

  CTEST(instruct, loadcom_reg) {
    init_cpu();
    set_mem(0x400, 0x13340000);
    /* Test positive number */
    set_reg(4, 0x00001000);
    test_inst(0);
    ASSERT_EQUAL(0xfffff000, get_reg(3));
    ASSERT_EQUAL(CC1, CC_REG);
    /* Test negative number */
    set_reg(4, 0xffffffff);
    test_inst(0);
    ASSERT_EQUAL(0x1, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG);
    /* Test zero */
    set_reg(4, 0x00000000);
    test_inst(0);
    ASSERT_EQUAL(0x0, get_reg(3));
    ASSERT_EQUAL(CC0, CC_REG);
    /* Test overflow */
    set_reg(4, 0x80000000);
    test_inst(0);
    ASSERT_EQUAL(0x80000000, get_reg(3));
    ASSERT_EQUAL(CC3, CC_REG);
  }

  CTEST(instruct, loadpos_reg) {
    init_cpu();
    set_mem(0x400, 0x10340000);
    set_reg(4, 0xffffffff);
    test_inst(0);
    ASSERT_EQUAL(0x00000001, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG);
    /* Test positive */
    set_reg(4, 0x00000001);
    test_inst(0);
    ASSERT_EQUAL(0x1, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG);
    /* Test zero */
    set_reg(4, 0x00000000);
    test_inst(0);
    ASSERT_EQUAL(0x0, get_reg(3));
    ASSERT_EQUAL(CC0, CC_REG);
    /* Test overflow */
    set_reg(4, 0x80000000);
    test_inst(0);
    ASSERT_EQUAL(0x80000000, get_reg(3));
    ASSERT_EQUAL(CC3, CC_REG);
  }

  CTEST(instruct, loadneg_reg) {
    init_cpu();
    set_mem(0x400, 0x11340000);
    set_reg(4, 0xffffffff);
    test_inst(0);
    ASSERT_EQUAL(0xffffffff, get_reg(3));
    ASSERT_EQUAL(CC1, CC_REG);
    /* Test positive */
    set_reg(4, 0x00000001);
    test_inst(0);
    ASSERT_EQUAL(0xffffffff, get_reg(3));
    ASSERT_EQUAL(CC1, CC_REG);
    /* Test zero */
    set_reg(4, 0x00000000);
    test_inst(0);
    ASSERT_EQUAL(0x0, get_reg(3));
    ASSERT_EQUAL(CC0, CC_REG);
    /* Test overflow */
    set_reg(4, 0x80000000);
    test_inst(0);
    ASSERT_EQUAL(0x80000000, get_reg(3));
    ASSERT_EQUAL(CC1, CC_REG);
  }

  CTEST(instruct, add_reg) {
    init_cpu(0);
    set_mem(0x400, 0x1a120000);
    set_reg(1, 0x12345678);
    set_reg(2, 0x00000005);
    test_inst(0);
    ASSERT_EQUAL(0x1234567d, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG);
  }

  CTEST(instruct, twoadd_reg) {
    init_cpu();
    set_mem(0x400, 0x1a121a31);
    set_reg(1, 0x12345678);
    set_reg(2, 0x00000001);
    set_reg(3, 0x00000010);
    test_inst2();
    ASSERT_EQUAL(0x12345679, get_reg(1));
    ASSERT_EQUAL(0x12345689, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG);
  }

  CTEST(instruct, add_neg_reg) {
    init_cpu();
    set_mem(0x400, 0x1a120000);
    set_reg(1, 0x81234567);
    set_reg(2, 0x00000001);
    test_inst(0);
    ASSERT_EQUAL(0x81234568, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG);
  }

  CTEST(instruct, add_zero_reg) {
    init_cpu();
    set_mem(0x400, 0x1a120000);
    set_reg(1, 0x81234567);
    set_reg(2, 0x00000001);
    test_inst(0);
    ASSERT_EQUAL(0x81234568, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG);
  }

  CTEST(instruct, add_over_reg) {
    init_cpu();
    set_mem(0x400, 0x1a120000);
    set_reg(1, 0x7fffffff);
    set_reg(2, 0x00000001);
    test_inst(0);
    ASSERT_EQUAL(0x80000000, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG);
  }

  CTEST(instruct, add_overtrap_reg) {
    uint32_t   psw1, psw2;
    init_cpu();
    set_mem(0x400, 0x1a120000);
    set_reg(1, 0x7fffffff);
    set_reg(2, 0x00000001);
    test_inst(0x8);
    psw1 = get_mem(0x28);
    psw2 = get_mem(0x2c);
    ASSERT_TRUE(trap_flag);
    ASSERT_EQUAL(0x8, psw1);
    ASSERT_EQUAL(0x78000402, psw2);
    ASSERT_EQUAL(0x80000000, get_reg(1));
    ASSERT_EQUAL(CC0, CC_REG);
  }

  CTEST(instruct, add) {
    init_cpu();
    set_mem(0x400, 0x5a156200);
    set_reg(1, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x34567890);
    test_inst(0);
    ASSERT_EQUAL(0x12345678 + 0x34567890, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG);
  }

  CTEST(instruct, add_half) {
    init_cpu();
    set_mem(0x400, 0x4a156200);
    set_reg(1, 1);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0xfffe1234);  /* Only fffe (-2) used */
    test_inst(0x0);
    ASSERT_EQUAL(0xffffffff, get_reg(1)); /* -1 */
    ASSERT_EQUAL(CC1, CC_REG); /* Negative */
  }

  CTEST(instruct, add_logic_zero) {
    init_cpu();
    set_mem(0x400, 0x1e120000); 
    set_reg(1, 0);
    set_reg(2, 0);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC0, CC_REG); /* zero, no carry */
  }

  CTEST(instruct, add_logic_nonzero) {
    init_cpu();
    set_mem(0x400, 0x1e120000); 
    set_reg(1, 0xffff0000);
    set_reg(2, 0x00000002);
    test_inst(0x0);
    ASSERT_EQUAL(0xffff0002, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); /* non zero, no carry */
  }

  CTEST( instruct, add_logic_zero_carry) {
    init_cpu();
    set_mem(0x400, 0x1e120000); 
    set_reg(1, 0xfffffffe);
    set_reg(2, 0x00000002);
    test_inst(0x0);
    ASSERT_EQUAL(0x00000000, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); /* zero, carry */
  }


  CTEST( instruct, add_logic_nonzero_carry) {
    init_cpu();
    set_mem(0x400, 0x1e120000); 
    set_reg(1, 0xfffffffe);
    set_reg(2, 0x00000003);
    test_inst(0x0);
    ASSERT_EQUAL(0x00000001, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); /* not zero, carry */
  }

  CTEST(instruct, add_logic2) {
    init_cpu();
    set_mem(0x400, 0x5e156200); 
    set_reg(1, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0xf0000000);
    test_inst(0x0);
    ASSERT_EQUAL(0x02345678, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); /* not zero, carry */
  }

  CTEST(instruct, subtract) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x00000001);
    set_mem(0x400, 0x1b120000);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_reg(1));
  }

  CTEST(instruct, subtract2) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x12300000);
    set_mem(0x400, 0x5b156200);
    test_inst(0x0);
    ASSERT_EQUAL(0x00045678, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sub_half) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x12300000);
    set_mem(0x400, 0x4b156200);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 - 0x1230, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sub_logical) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x12345678);
    set_mem(0x400, 0x1f120000);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Difference is zero (carry));
  }

  CTEST(instruct, sub_logical2) {
    init_cpu();
    set_reg(1, 0xffffffff);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x11111111);
    set_mem(0x400, 0x5f156200);
    test_inst(0x0);
    ASSERT_EQUAL(0xeeeeeeee, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Non-zero, carry (no borrow));
  }

  CTEST(instruct, sub_logical3) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x23456789);
    set_mem(0x400, 0x5f156200);
    test_inst(0x0);
    ASSERT_EQUAL(((uint32_t)0x12345678 - (uint32_t)0x23456789), get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Non-zero, no carry (borrow));
  }

  CTEST(instruct, cp_reg) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x12345678);
    set_mem(0x400, 0x19120000);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678, get_reg(1)); // Unchanged);
    ASSERT_EQUAL(CC0, CC_REG); // Operands are equal);
  }

  CTEST(instruct, cp_reg2) {
    init_cpu();
    set_reg(1, 0xfffffffe); /* -2 */
    set_reg(2, 0xfffffffd); /* -3 */
    set_mem(0x400, 0x19120000);
    test_inst(0x0);
    ASSERT_EQUAL(0xfffffffe, get_reg(1)); // Unchanged);
    ASSERT_EQUAL(CC2, CC_REG); // First operand is high);
  }

  CTEST(instruct, cp_reg3) {
    init_cpu();
    set_reg(1, 2);
    set_reg(2, 3);
    set_mem(0x400, 0x19120000);
    test_inst(0x0);
    ASSERT_EQUAL(2, get_reg(1)); // Unchanged);
    ASSERT_EQUAL(CC1, CC_REG); // First operand is low);
  }

  CTEST(instruct, comp) {
    init_cpu();
    set_reg(1, 0xf0000000);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x12345678);
    set_mem(0x400, 0x59156200);
    test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // First operand is low);
  }

  CTEST(instruct, mult) {
    init_cpu();
    set_reg(3, 28);
    set_reg(4, 19);
    set_mem(0x400, 0x1c240000);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(2));
    ASSERT_EQUAL(28 * 19, get_reg(3));
  }

  CTEST(instruct, mult_rand) {
    unsigned int seed = 1;
    init_cpu();
    for (int i = 0; i < 100; i++) {
      int n1 = (int)((double)rand_r(&seed) / (double)(RAND_MAX) * 1000.0);
      int n2 = (int)((double)rand_r(&seed) / (double)(RAND_MAX) * 1000.0);
      if (n1 * n2 >= 0x10000) continue;
      set_reg(3, n1);
      set_reg(4, n2);
      set_mem(0x400, 0x1c240000);
      test_inst(0x0);
      ASSERT_EQUAL(0, get_reg(2));
      ASSERT_EQUAL(n1 * n2, get_reg(3));
    }
  }

  CTEST(instruct, mult_long) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_reg(4, 0x34567890);
    set_mem(0x400, 0x1c240000);
    test_inst(0x0);
    ASSERT_EQUAL(0x3b8c7b8, get_reg(2));
    ASSERT_EQUAL(0x3248e380, get_reg(3));
  }

  CTEST(instruct, mult_longer) {
    init_cpu();
    set_reg(3, 0x7fffffff);
    set_reg(4, 0x7fffffff);
    set_mem(0x400, 0x1c240000);
    test_inst(0x0);
    ASSERT_EQUAL(0x3fffffff, get_reg(2));
    ASSERT_EQUAL(0x00000001, get_reg(3));
  }

  CTEST(instruct, mult_neg) {
    init_cpu();
    set_reg(3, 0xfffffffc); /* -4 */
    set_reg(4, 0xfffffffb); /* -5 */
    set_mem(0x400, 0x1c240000);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(2));
    ASSERT_EQUAL(20, get_reg(3));
  }

  CTEST(instruct, mult_negpos) {
    init_cpu();
    set_reg(3, 0xfffffffc); /* -4 */
    set_reg(4, 0x0000000a); /* 10 */
    set_mem(0x400, 0x1c240000);
    test_inst(0x0);
    ASSERT_EQUAL(0xffffffff, get_reg(2));
    ASSERT_EQUAL((uint32_t)(-40), get_reg(3));
  }

  CTEST(instruct, mult_mem) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x34567890);
    set_mem(0x400, 0x5c256200);
    test_inst(0x0);
    ASSERT_EQUAL(0x03b8c7b8, get_reg(2)); /* High 32-bits */
    ASSERT_EQUAL(0x3248e380, get_reg(3)); /* Low 32-bits */
  }

  CTEST(instruct, mult_half) {
    init_cpu();
    set_reg(3, 4);
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0x00000003); 
    set_mem(0x400, 0x4c356202);
    test_inst(0x0);
    ASSERT_EQUAL(12, get_reg(3)); /* Low 32-bits */
  }

  CTEST(instruct, mult_half2) {
    init_cpu();
    set_reg(3, 0x00000015); /* 21 */
    set_reg(5, 0x00000100);
    set_reg(6, 0x00000200);
    set_mem(0x500, 0xffd91111); /* -39 */
    set_mem(0x400, 0x4c356200);
    test_inst(0x0);
    ASSERT_EQUAL(0xfffffccd, get_reg(3)); // Low 32-bits);
  }

  CTEST(instruct, div_big) {
    init_cpu();
    set_reg(2, 0x00112233);
    set_reg(3, 0x44556677);
    set_reg(4, 0x12345678); /* 0x1122334455667788 / 0x12345678 */
    set_mem(0x400, 0x1d240000);
    // divide R2/R3 by R4
    test_inst(0x0);
    ASSERT_EQUAL(0x11b3d5f7, get_reg(2)); /* Remainder */
    ASSERT_EQUAL(0x00f0f0f0, get_reg(3)); /* Quotient */
  }

  CTEST(instruct, div_reg) {
    init_cpu();
    set_reg(2, 0x1);
    set_reg(3, 0x12345678);
    set_reg(4, 0x00000234);
    set_mem(0x400, 0x1d240000);
    // divide R2/R3 by R4
    test_inst(0x0);
    ASSERT_EQUAL(0x112345678 % 0x234, get_reg(2)); // Remainder);
    ASSERT_EQUAL(0x112345678 / 0x234, get_reg(3)); // Quotient);
  }

  CTEST(instruct, div_neg) {
    init_cpu();
    set_reg(2, 0x1);
    set_reg(3, 0x12345678);
    set_reg(4, (uint32_t)(-0x00000234));
    set_mem(0x400, 0x1d240000);
    // divide R2/R3 by R4
    test_inst(0x0);
    ASSERT_EQUAL(0x112345678 % 0x234, get_reg(2)); /* Remainder */
    ASSERT_EQUAL((uint32_t)(-0x112345678 / 0x234), get_reg(3)); /* Quotient */
  }

  CTEST(instruct, div_over) {
    init_cpu();
    set_reg(2, 0x12345678);
    set_reg(3, 0x9abcdef0);
    set_reg(5, 0x100);
    set_reg(6, 0x200);
    set_mem(0x500, 0x23456789);
    set_mem(0x400, 0x5d256200);
    test_inst(0x8);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, div_mem) {
    init_cpu();
    set_reg(2, 0x12345678);
    set_reg(3, 0x9abcdef0);
    set_reg(5, 0x100);
    set_reg(6, 0x200);
    set_mem(0x500, 0x73456789);
    set_mem(0x400, 0x5d256200);
    test_inst(0x0);
    ASSERT_EQUAL(0x50c0186a, get_reg(2)); /* Remainder */
    ASSERT_EQUAL(0x286dead6, get_reg(3)); /* Quotient */
  }

  CTEST(instruct, sla) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x00000001);
    set_mem(0x400, 0x8b1f2001);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 << 2, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sla2) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x00000001);
    set_mem(0x400, 0x8b1f2fc1);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 << 2, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sla_zero) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_mem(0x400, 0x8b100000);
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sla_zero2) {
    init_cpu();
    set_reg(1, 0x92345678);
    set_mem(0x400, 0x8b1f0000);
    test_inst(0x0);
    ASSERT_EQUAL(0x92345678, get_reg(1)); // Should be unchanged);
    ASSERT_EQUAL(CC1, CC_REG); // Negative);
  }


  CTEST(instruct, sla_zero3) {
    init_cpu();
    set_reg(1, 0);
    set_mem(0x400, 0x8b1f0000);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC0, CC_REG); // Zero);
  }

  CTEST(instruct, sla_over) {
    init_cpu();
    set_reg(1, 0x10000000);
    set_reg(2, 2); // Shift by 2 still fits
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
    ASSERT_EQUAL(0x40000000, get_reg(1));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);

    set_reg(1, 0x10000000);
    set_reg(2, 3); // Shift by 3 overflows
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
    ASSERT_EQUAL(0x00000000, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla4) {
    init_cpu();
    set_reg(1, 0x7fffffff);
    set_reg(2, 0x0000001f); // Shift by 31 shifts out entire number
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla5) {
    init_cpu();
    set_reg(1, 0x7fffffff);
    set_reg(2, 0x00000020); // Shift by 32 shifts out entire number
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
fprintf(stderr, "reg %08x %03x\n", get_reg(1), CC_REG);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla6) {
    init_cpu();
    set_reg(1, 0x80000000);
    set_reg(2, 0x0000001f); // Shift by 31 shifts out entire number
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
    ASSERT_EQUAL(0x80000000, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla7) {
    init_cpu();
    set_reg(1, 0x80000000);
    set_reg(2, 21); // Shift by 2 should overflow
    set_mem(0x400, 0x8b1f2000);
    test_inst(0x0);
    ASSERT_EQUAL(0x80000000, get_reg(1));
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla8) {
    init_cpu();
    set_reg(1, 0x80000001);
    set_reg(2, 0x00000001);
    set_mem(0x400, 0x8b1f2001);
    test_inst(0x0);
    ASSERT_EQUAL(0x80000004, get_reg(1)); // Keep the sign);
    ASSERT_EQUAL(CC3, CC_REG); // Overflow);
  }

  CTEST(instruct, sla9) {
    init_cpu();
    set_reg(1, 0xf0000001);
    set_reg(2, 0x00000001);
    set_mem(0x400, 0x8b1f2001);
    test_inst(0x0);
    ASSERT_EQUAL(0xc0000004, get_reg(1)); // Keep the sign);
    ASSERT_EQUAL(CC1, CC_REG); // Negative);
  }

  CTEST(instruct, ap_small) {
    init_cpu();
    set_mem(0x100, 0x0000002c); // 2+
    set_mem(0x200, 0x00003c00); // 3+
    set_mem(0x400, 0xfa000103);
    set_mem(0x404, 0x02020000);
    test_inst(0x0);
    ASSERT_EQUAL(0x0000005c, get_mem(0x100)); // 5+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ap_one) {
    init_cpu();
    set_mem(0x100, 0x2888011c); // 2888011+
    set_mem(0x200, 0x1112292c); // 1112292+
    set_mem(0x400, 0xfa330100);
    set_mem(0x404, 0x02000000);
    test_inst(0x0);
    ASSERT_EQUAL(0x4000303c, get_mem(0x100)); // 4000303+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ap_one2) {
    init_cpu();
    set_mem(0x100, 0x0000002c); // 2+
    set_mem(0x200, 0x0000003c); // 3+
    set_mem(0x400, 0xfa330100);
    set_mem(0x404, 0x02000000);
    test_inst(0x0);
    ASSERT_EQUAL(0x0000005c, get_mem(0x100)); // 5+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ap_offset) {
    init_cpu();
    set_mem(0x100, 0x0043212c); // 2+
    set_mem(0x200, 0x0023413c); // 3+
    set_mem(0x400, 0xfa220101);
    set_mem(0x404, 0x02010000);
    test_inst(0x0);
    ASSERT_EQUAL(0x0066625c, get_mem(0x100)); // 5+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ap_nooffset) {
    init_cpu();
    set_mem(0x100, 0x0043212c); // 2+
    set_mem(0x200, 0x0023413c); // 3+
    set_mem(0x400, 0xfa330100);
    set_mem(0x404, 0x02000000);
    test_inst(0x0);
    ASSERT_EQUAL(0x0066625c, get_mem(0x100)); // 5+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ap_offset2) {
    // Example from Princ Ops p136.2
    init_cpu();
    set_reg(12, 0x00002000);
    set_reg(13, 0x000004fd);
    set_mem(0x2000, 0x38460d00); // 38460-
    set_mem(0x500, 0x0112345c); // 112345+
    set_mem(0x400, 0xfa23c000);
    set_mem(0x404, 0xd0030000);
    test_inst(0x0);
fprintf(stderr, "Mem %08x %03x\n", get_mem(0x2000), CC_REG);
    ASSERT_EQUAL(0x73885c00, get_mem(0x2000)); // 73885+);
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }


  CTEST(instruct, sth) {
    init_cpu();
    set_reg(3, 0xaabbccdd);
    set_reg(4, 1);
    set_reg(5, 1);
    set_mem(0x1000, 0x12345678);
    set_mem(0x400, 0x40345ffe);
    test_inst(0x0);
    ASSERT_EQUAL(0xccdd5678, get_mem(0x1000));
  }

  CTEST(instruct, sth2) {
    init_cpu();
    set_reg(3, 0xaabbccdd);
    set_reg(4, 1);
    set_reg(5, 3);
    set_mem(0x1000, 0x12345678);
    set_mem(0x400, 0x40345ffe);
    test_inst(0x0);
    ASSERT_EQUAL(0x1234ccdd, get_mem(0x1000));
  }

  CTEST(instruct, sth3) {
    init_cpu();
    set_reg(3, 0xaabbccdd);
    set_reg(4, 1);
    set_reg(5, 2);
    set_mem(0x1000, 0x12345678);
    set_mem(0x400, 0x40345ffe);
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, lra) {
    init_cpu();
    // From Princ Ops p147
    set_mem(0x400, 0x41100800);
    test_inst(0x0);
    ASSERT_EQUAL(2048, get_reg(1));
  }

  CTEST(instruct, lra2) {
    init_cpu();
    // From Princ Ops p147
    set_reg(5, 0x00123456);
    set_mem(0x400, 0x4150500a);
    test_inst(0x0);
    ASSERT_EQUAL(0x00123460, get_reg(5));
  }

  CTEST(instruct, stc) {
    int  i;
    int  shift;
    int  desired;
    init_cpu();

    for (i = 0; i < 4; i++) { // Test all 4 offsets
      set_reg(5, 0xffffff12); // Only 12 used
      set_reg(1, i);
      set_mem(0x100, 0xaabbccdd);
      set_mem(0x400, 0x42501100); // "STC 5,100(0,1)");
      test_inst(0x0);
      shift = (3 - i) * 8;
      desired = ((0xaabbccdd & ~(0xff << shift)) | (0x12 << shift));
      ASSERT_EQUAL(desired, get_mem(0x100));
    }
  }

  CTEST(instruct, ic) {
    int i;
    int desired;
    init_cpu();
    for (i = 0; i < 4; i++) { // Test all 4 offsets
      set_reg(5, 0xaabbccdd);
      set_reg(1, i);
      set_mem(0x100, 0x00112233);
      set_mem(0x400, 0x43501100); //, "IC 5,100(0,1)");
      test_inst(0x0);
      desired = (0xaabbcc00 | (i * 17));
      ASSERT_EQUAL(desired, get_reg(5));
    }
  }

  CTEST(instruct, ex) {
    init_cpu();
    set_mem(0x100, 0x1a000000); // Target instruction AR 0,0
    set_reg(1, 0x00000045); // Modification: AR 4,5
    set_reg(4, 0x100);
    set_reg(5, 0x200);
    set_mem(0x400, 0x44100100); //, "EX 1,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x300, get_reg(4));
  }

  CTEST(instruct, ex_ex) {
    init_cpu();
    set_mem(0x100, 0x44100100); // Target instruction EX 1,100(0,0)
    set_reg(1, 0x00000045); // Modification: EX 4,100(5,0)
    set_mem(0x400, 0x44100100); //}, "EX 1,100(0,0)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, bal) {
    init_cpu();
    set_reg(3, 0x12300000);
    set_reg(4, 0x00045600);
    set_ilc(0);         // overwritten with 2
    set_cc(CC3);
    set_mem(0x100, 0x45134078); //, "BAL 1,78(3,4)");
    test_inst(0xa);
    ASSERT_EQUAL(0xba000404, get_reg(1)); // low-order PSW: ILC, CR, PROGMASK, return IAR);
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, bct) {
    init_cpu();
    set_reg(1, 3); // Counter
    set_reg(2, 0x00345678); // Branch destination
    set_reg(3, 0x00000010);
    set_mem(0x400, 0x46123100); //, "BCT 1,100(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL(2, get_reg(1));
    ASSERT_EQUAL(0x00345788, IAR);
  }

  CTEST(instruct, bc) {
    int i;
    unsigned int seed = 42;
    init_cpu();
    for (i = 0; i < 16; i++) {
        uint32_t op = 0x47000100 | (i << 20);
        switch(rand_r(&seed) & 3) {
           case 0:   set_cc(CC0); break;
           case 1:   set_cc(CC1); break;
           case 2:   set_cc(CC2); break;
           case 3:   set_cc(CC3); break;
        }
        set_mem(0x400, op);
        test_inst(0x0);
        if (((i & 8) && (CC_REG == CC0)) ||
            ((i & 4) && (CC_REG == CC1)) ||
            ((i & 2) && (CC_REG == CC2)) ||
            ((i & 1) && (CC_REG == CC3))) {
          // Taken
          ASSERT_EQUAL(0x100, IAR);
        } else {
          ASSERT_EQUAL(0x404, IAR);
        }
    }
  }

  CTEST(instruct, lh_ext) {
    init_cpu();
    set_reg(3, 0xffffffff);
    set_reg(4, 0x1000);
    set_reg(5, 0x200);
    set_mem(0x1b84, 0x87654321);
    set_mem(0x400, 0x48345984); //, "LH 3,984(4,5)");
    // LH 3, 984(4, 5): load R3 with mem[984+R4+R45)
    ASSERT_EQUAL(0xffff8765, get_reg(3)); // sign extension);
  }

  CTEST(instruct, lh_ext2) {
    init_cpu();
    set_reg(3, 0xffffffff);
    set_reg(4, 0x1000);
    set_reg(5, 0x202);
    set_mem(0x1b84, 0x07658321);
    set_mem(0x400, 0x48345984); //, "LH 3,984(4,5)");
    test_inst(0x0);
    // LH 3, 984(4, 5): load R3 with mem[984+R4+R45)
    ASSERT_EQUAL(0xffff8321, get_reg(3)); // sign extension);
  }

  CTEST(instruct, lh) {
    init_cpu();
    set_reg(3, 0xffffffff);
    set_reg(4, 0x1000);
    set_reg(5, 0x200);
    set_mem(0x1b84, 0x87654321);
    set_mem(0x400, 0x48345986); //, "LH 3,986(4,5)");
    test_inst(0x0);
    // LH 3, 986(4, 5): load R3 with mem[986+R4+R45)
    ASSERT_EQUAL(0x00004321, get_reg(3));
  }

  CTEST(instruct, ch_equal) {
    init_cpu();
    set_reg(3, 0x00005678);
    set_mem(0x100, 0x5678abcd);
    set_mem(0x400, 0x49300100); //, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // equal);
  }

  CTEST(instruct, ch_equal_ext) {
    init_cpu();
    set_reg(3, 0xffff9678);
    set_mem(0x100, 0x9678abcd);
    set_mem(0x400, 0x49300100); //, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // equal);
  }

  CTEST(instruct, ch_high) {
    init_cpu();
    set_reg(3, 0x00001235);
    set_mem(0x100, 0x1234abcd);
    set_mem(0x400, 0x49300100); //, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC2, CC_REG); // First operand high);
  }

  CTEST(instruct, ch_high_ext) {
    init_cpu();
    set_reg(3, 0x00001235);
    set_mem(0x100, 0x8234abcd);
    set_mem(0x400, 0x49300100); //, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC2, CC_REG); // First operand high);
  }

  CTEST(instruct, ch_low) {
    init_cpu();
    set_reg(3, 0x80001235);
    set_mem(0x100, 0x1234abcd);
    set_mem(0x400, 0x49300100); //}, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // First operand low);
  }

  CTEST(instruct, ch_low_ext) {
    init_cpu();
    set_reg(3, 0xfffffffc);
    set_mem(0x100, 0xfffd0000);
    set_mem(0x400, 0x49300100); //, "CH 3,100(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // First operand low);
  }

  // Halfword second operand is sign-extended and added to first register.
  CTEST(instruct, ah) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_mem(0x200, 0x1234eeee);
    set_mem(0x400, 0x4a300200); //, "AH 3,200(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 + 0x1234, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ah_ext) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_mem(0x200, 0xfffe9999); // -2
    set_mem(0x400, 0x4a300200); //, "AH 3,200(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345676, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, ah_two) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_reg(1, 2);
    set_mem(0x200, 0x99991234);
    set_mem(0x400, 0x4a310200); //, "AH 3,200(1,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 + 0x1234, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sh) {
    set_reg(3, 0x12345678);
    set_mem(0x200, 0x1234eeee);
    set_mem(0x400, 0x4b300200); //, "SH 3,200(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 - 0x1234, get_reg(3));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, mh) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_mem(0x200, 0x00059999); // 5
    set_mem(0x400, 0x4c300200);  //, "MH 3,200(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 * 5, get_reg(3));
  }

  CTEST(instruct, mh_neg) {
    init_cpu();
    set_reg(3, (uint32_t)(-0x12345678));
    set_mem(0x200, 0xfffb9999); // -5
    set_mem(0x400, 0x4c300200); //, "MH 3,200(0,0)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678 * 5, get_reg(3));
  }

  CTEST(instruct, cvb) {
    // Example from Principles of Operation p122
    init_cpu();
    set_reg(5, 50); // Example seems to have addresses in decimal?
    set_reg(6, 900);
    set_mem(1000, 0x00000000);
    set_mem(1004, 0x0025594f);
    set_mem(0x400, 0x4f756032); // }, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_EQUAL(25594, get_reg(7)); // Note: decimal, not hex);
  }

  CTEST(instruct, cvb_bad_sign) { 
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x00000000);
    set_mem(1004, 0x00255941); // 1 is not a valid sign
    set_mem(0x400, 0x4f756032); //, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  // Needs DC0 to support correction properly
  CTEST(instruct, cvb_bad_digit) {
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x00000000);
    set_mem(1004, 0x002a594f);
    set_mem(0x400, 0x4f756032); //, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, cvb_unalign) {
    init_cpu();
    set_reg(5, 0);
    set_reg(6, 0);
    set_mem(1000, 0x00000000);
    set_mem(1004, 0x002a594f);
    set_mem(0x400, 0x4f756034); //, "CVB 7,34(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
    set_mem(0x400, 0x4f756032); //}, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
    set_mem(0x400, 0x4f756031); //}, "CVB 7,31(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, cvb_overflow) {
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x00000214);
    set_mem(1004, 0x8000000f);
    set_mem(0x400, 0x4f756032); //, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
    ASSERT_EQUAL(2148000000, get_reg(7)); // Note: decimal, not hex);
  }

  CTEST(instruct, cvb_big_overflow) {
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x12345678);
    set_mem(1004, 0x4800000f);
    set_mem(0x400, 0x4f756032); //, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, cvb_large) {
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x00000021);
    set_mem(1004, 0x2345678f);
    set_mem(0x400, 0x4f756032); //}, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_EQUAL(212345678, get_reg(7)); // Note: decimal, not hex);
  }

  CTEST(instruct, cvb_neg) {
    init_cpu();
    set_reg(5, 50);
    set_reg(6, 900);
    set_mem(1000, 0x00000000);
    set_mem(1004, 0x0025594d); // d is negative
    set_mem(0x400, 0x4f756032); //, "CVB 7,32(5,6)");
    test_inst(0x0);
    ASSERT_EQUAL((uint32_t)(-25594), get_reg(7)); // Note: decimal, not hex);
  }

  // QE900/073C, CLF 112
  CTEST(instruct, cvb2) {
    init_cpu();
    set_reg(5, 0x100);
    set_reg(6, 0x200);
    set_mem(0x500, 0);
    set_mem(0x504, 0x1234567f); // Decimal 1234567+
    set_mem(0x400, 0x4f156200); //, "CVB 1,200(5,6)");
    test_inst(0x0);
    ASSERT_EQUAL(234567, get_reg(1)); // Note: decimal, not hex);
  }

  CTEST(instruct, cvb_neg2) {
    init_cpu();
    set_reg(5, 0x100);
    set_reg(6, 0x200);
    set_mem(0x500, 0);
    set_mem(0x504, 0x1234567b); // Decimal 1234567-
    set_mem(0x400, 0x4f156200); //, "CVB 1,200(5,6)");
    test_inst(0x0);
    ASSERT_EQUAL((uint32_t)(-1234567), get_reg(1)); // Note: decimal, not hex);
  }

  CTEST(instruct, cvd) { 
    init_cpu();
    // Princ Ops p142
    set_reg(1, 0x00000f0f); // 3855 dec
    set_reg(13, 0x00007600);
    set_amwp(0); // EBCDIC
    set_mem(0x400, 0x4e10d008); //, "CVD 1,8(0,13)");
    test_inst(0x0);
    ASSERT_EQUAL(0x00000000, get_mem(0x7608));
    ASSERT_EQUAL(0x0003855c, get_mem(0x760C));
  }

  CTEST(instruct, cvd_ascii) {
    init_cpu();
    set_reg(1, 0x00000f0f); // 3855 dec
    set_reg(13, 0x00007600);
    set_amwp(8);             // ASCII
    set_mem(0x400, 0x4e10d008); //, "CVD 1,8(0,13)");
    test_inst(0x0);
    set_amwp(0); // EBCDIC
    ASSERT_EQUAL(0x00000000, get_mem(0x7608));
    ASSERT_EQUAL(0x0003855d, get_mem(0x760c));
  }

  CTEST(instruct, st) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x100);
    set_reg(3, 0x100);
    set_mem(0x400, 0x50123400); //, "ST 1,400(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678, get_mem(0x600));
  }

  CTEST(instruct, and) {
    init_cpu();
    set_reg(1, 0x11223344);
    set_reg(2, 0x200);
    set_reg(3, 0x300);
    set_mem(0x954, 0x12345678);
    set_mem(0x400, 0x54123454); //, "N 1,454(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL((0x11223344 & 0x12345678), get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, cl) {
    init_cpu();
    set_reg(1, 0x12345678);
    set_reg(2, 0x200);
    set_reg(3, 0x300);
    set_mem(0x900, 0x12345678);
    set_mem(0x400, 0x55123400); //}, "CL 1,400(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // Equal);
  }

  CTEST(instruct, or) {
    init_cpu();
    set_reg(1, 0x11223344);
    set_reg(2, 0x200);
    set_reg(3, 0x300);
    set_mem(0x954, 0x12345678);
    set_mem(0x400, 0x56123454); //, "O 1,454(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL((0x11223344 | 0x12345678), get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, xor) {
    init_cpu();
    set_reg(1, 0x11223344);
    set_reg(2, 0x200);
    set_reg(3, 0x300);
    set_mem(0x954, 0x12345678);
    set_mem(0x400, 0x57123454); //, "X 1,454(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL((0x11223344 ^ 0x12345678), get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST( instruct, xor_zero) {
    init_cpu();
    set_reg(1, 0x11223344);
    set_reg(2, 0x200);
    set_reg(3, 0x300);
    set_mem(0x954, 0x11223344);
    set_mem(0x400, 0x57123454); //, "X 1,454(2,3)");
    test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC0, CC_REG); // Zero);
  }

  CTEST(instruct, load) {
    init_cpu();
    set_reg(4, 0x1000);
    set_reg(5, 0x200);
    set_mem(0x1b84, 0x12345678);
    set_mem(0x400, 0x58345984); //, "L 3,984(4,5)");
    // L 3, 984(4, 5): load R3 with mem[984+R4+R45)
    test_inst(0x0);
    ASSERT_EQUAL(0x12345678, get_reg(3));
  }

  CTEST(instruct, comp2) {
    init_cpu();
    set_reg(3, 0x12345678);
    set_reg(4, 0x1000);
    set_reg(5, 0x200);
    set_mem(0x1b84, 0x12345678);
    set_mem(0x400, 0x59345984); //, "C 3,984(4,5)");
    test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // Operands are equal);
  }

  CTEST(instruct, add_rand) {
    int i;
    unsigned int seed = 42;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      int n1 = rand_r(&seed);
      int n2 = rand_r(&seed);
      int sum = (int32_t)(n1) + (int32_t)(n2);
      set_reg(1, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5a100100); //, "A 1,100(0,0)");
      test_inst(0x0);
      if (sum >= 0x80000000 || sum < -0x80000000) {
        ASSERT_EQUAL(CC3, CC_REG); // Overflow);
        continue;
      } else if (sum == 0) {
        ASSERT_EQUAL(CC0, CC_REG); // Zero);
      } else if (sum > 0) {
        ASSERT_EQUAL(CC2, CC_REG); // Positive);
      } else {
        ASSERT_EQUAL(CC1, CC_REG); // Negative);
      }
      ASSERT_EQUAL(sum, (int32_t)(get_reg(1)));
    }
  }

  CTEST(instruct, sub_rand) {
    unsigned int seed = 123;
    int   i;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      int n1 = rand_r(&seed);
      int n2 = rand_r(&seed);
      int result = (int32_t)(n1) - (int32_t)(n2);
      set_reg(1, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5b100100); //, "S 1,100(0,0)");
      test_inst(0x0);
      if (result >= 0x80000000 || result < -0x80000000) {
        ASSERT_EQUAL(CC3, CC_REG); // Overflow);
        continue;
      } else if (result == 0) {
        ASSERT_EQUAL(CC0, CC_REG); // Zero);
      } else if (result > 0) {
        ASSERT_EQUAL(CC2, CC_REG); // Positive);
      } else {
        ASSERT_EQUAL(CC1, CC_REG); // Negative);
      }
      ASSERT_EQUAL(result, (int32_t)(get_reg(1)));
    }
  }

  CTEST(instruct, mult_rand2) {
      unsigned int seed = 42;
      int  i;
      init_cpu();
      for (i = 0; i < testcycles; i++) {
        int n1 = rand_r(&seed);
        int n2 = rand_r(&seed);
        int desired = (int32_t)(n1) * (int32_t)(n2);
        int result;
        set_reg(3, n1); // Note: multiplicand in reg 3 but reg 2 specified.
        set_mem(0x100, n2);
        set_mem(0x400, 0x5c200100);  //, "M 2,100(0,0)");
        test_inst(0x0);
        result = get_reg(2) * pow(2., 32) + get_reg(3);
        if (get_reg(2) & 0x80000000) {
          // Convert 64-bit 2"s complement
          result = -((~get_reg(2)) * pow(2., 32) + ((~get_reg(3))) + 1);
        }
        ASSERT_EQUAL(desired, result);
        if (result != desired) break;
        // No condition code
      }
  }

  CTEST(instruct, div_rand) {
    unsigned int seed = 124;
    int   i;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
        uint32_t n1 = 0; // XXX implement this
        uint32_t n2 = 0; // XXX
      int quotient = rand_r(&seed);
      int remainder = rand_r(&seed);
      int divisor = rand_r(&seed);
      int result, result0, result1;
      if (quotient * divisor * remainder < 0) {
        remainder = -remainder;
      }
      //int dividend = quotient * divisor + remainder;
      set_reg(2, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5d200100); //, "D 2,100(0,0)");
      test_inst(0x0);
      result = (int32_t)(n1) * (int32_t)(n2);
      if (result == 0) {
        ASSERT_EQUAL(CC0, CC_REG); // Zero);
      } else if (result > 0) {
        ASSERT_EQUAL(CC2, CC_REG); // Positive);
      } else {
        ASSERT_EQUAL(CC1, CC_REG); // Negative);
      }
      result0 = result / pow(2., 32);
      ASSERT_EQUAL(result0, get_reg(2));
      result1 = result - result0 * pow(2., 32);
      ASSERT_EQUAL(result1, get_reg(3));
    }
  }

  CTEST(instruct, add_log_rand) {
    unsigned int seed = 125;
    int       i;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      uint32_t n1 = rand_r(&seed);
      uint32_t n2 = rand_r(&seed);
      uint64_t result = n1+n2;
      int carry = 0;
      set_reg(2, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5e200100);// , "AL 2,100(0,0)");
      test_inst(0x0);
      if (result >= 0x100000000) {
        carry = 1;
        result -= 0x100000000;
      }
      if (carry == 0) {
        if (result == 0) {
          ASSERT_EQUAL(CC0, CC_REG); // Zero, no carry);
        } else {
          ASSERT_EQUAL(CC1, CC_REG); // Nonzero, no carry);
        }
      } else {
        if (result == 0) {
          ASSERT_EQUAL(CC2, CC_REG); // Zero, carry);
        } else {
          ASSERT_EQUAL(CC3, CC_REG); // Nonzero, carry);
        }
      }
      ASSERT_EQUAL(result, get_reg(2));
    }
  }

  CTEST(instruct, sub_log_rand) {
    unsigned int seed = 44;
    int    i;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      uint32_t n1 = rand_r(&seed);
      uint32_t n2 = rand_r(&seed);
      uint64_t result = n1 + ((n2 ^ 0xffffffff)) + 1;
      int carry = 0;
      set_reg(2, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5f200100); //, "SL 2,100(0,0)");
      test_inst(0x0);
      if (result >= 0x100000000) {
        carry = 1;
        result -= 0x100000000;
      }
      if (carry == 0) {
        if (result == 0) {
          ASSERT_EQUAL(CC0, CC_REG); // Zero, no carry);
        } else {
          ASSERT_EQUAL(CC1, CC_REG); // Nonzero, no carry);
        }
      } else {
        if (result == 0) {
          ASSERT_EQUAL(CC2, CC_REG); // Zero, carry);
        } else {
          ASSERT_EQUAL(CC3, CC_REG); // Nonzero, carry);
        }
      }
      ASSERT_EQUAL(result, get_reg(2));
    }
  }


  CTEST(instruct, ssm) {
    init_cpu();
    MASK = 0xff;
    set_key(3);
    set_amwp(0x8); // Privileged
    set_cc(CC1);
    set_reg(3, 0x11);
    set_mem(0x110, 0xaabbccdd); // Access byte 1
    set_mem(0x400, 0x80ee3100); // "SSM 100(3)");
    test_inst(0xa);
    ASSERT_EQUAL(0xbb, MASK);
    ASSERT_EQUAL(3, get_key());
    ASSERT_EQUAL(0x8, get_amwp());
    ASSERT_EQUAL(CC1, CC_REG);
    ASSERT_EQUAL(0xa, PM);
    ASSERT_EQUAL(0x404, IAR);
  }

  CTEST(instruct, ssm_unpriv) {
    init_cpu();
    MASK = 0xff;
    set_key(3);
    set_amwp(0x1); // problem state
    set_cc(CC1);
    set_mem(0x400, 0x80ee3100); //, "SSM 100(3)");
    test_inst(0xa);
    ASSERT_TRUE(trap_flag);
    set_amwp(0);  // Privileged
  }

  CTEST(instruct, lpsw) {
    init_cpu();
    set_amwp(0);  // Privileged
    set_reg(3, 0x10);
    set_mem(0x110, 0x12345678);
    set_mem(0x114, 0x9a123450); // Branch to 123450
    set_mem(0x400, 0x82003100); //, "LPSW 100(3)");
    test_inst(0x0);
    ASSERT_EQUAL(0x12, MASK);
    ASSERT_EQUAL(0x3, get_key());
    ASSERT_EQUAL(0x4, get_amwp());
    ASSERT_EQUAL(CC_REG, CC1);
    ASSERT_EQUAL(0xa, PM);
    ASSERT_EQUAL(0x123450, IAR);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxh_high) {
    init_cpu();
    set_reg( 1, 0x12345678); // Value
    set_reg( 4, 1); // Increment
    set_reg( 5, 0x12345678); // Comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x86142200); //}, "BXH 1, 4, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345679, get_reg(1));
    ASSERT_EQUAL(0x1200, IAR); // Branch taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxh_high_decr) {
    init_cpu();
    set_reg( 1, 0x12345678); // Value
    set_reg( 4, 0xffffffff); // Increment -1
    set_reg( 5, 0x12345678); // Comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x86142200); //}, "BXH 1, 4, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_reg(1));
    ASSERT_EQUAL(0x404, IAR); // Branch not taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxh_high1) {
    init_cpu();
    set_reg( 1, 1); // Value
    set_reg( 3, 0x12345678); // Increment and comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x86132200); //, "BXH 1, 3, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345679, get_reg(1));
    ASSERT_EQUAL(0x1200, IAR); // Branch taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxh_high2) {
    init_cpu();
    set_reg( 1, 0xffffffff); // Value
    set_reg( 3, 0x12345678); // Increment and comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x86132200); //}, "BXH 1, 3, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_reg(1));
    ASSERT_EQUAL(0x404, IAR); // Branch not taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxle) {
    init_cpu();
    set_reg( 1, 0x12345678); // Value
    set_reg( 4, 1); // Increment
    set_reg( 5, 0x12345678); // Comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x87142200); //}, "BXLE 1, 4, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345679, get_reg(1));
    ASSERT_EQUAL(0x404, IAR); // Branch not taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxle_low) {
    init_cpu();
    set_reg( 1, 0x12345678); // Value
    set_reg( 4, 0xffffffff); // Increment -1
    set_reg( 5, 0x12345678); // Comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x87142200);  //}, "BXLE 1, 4, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_reg(1));
    ASSERT_EQUAL(0x1200, IAR); // Branch taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxle_share) {
    init_cpu();
    set_reg( 1, 1); // Value
    set_reg( 3, 0x12345678); // Increment and comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x87132200); //}, "BXLE 1, 3, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345679, get_reg(1));
    ASSERT_EQUAL(0x404, IAR); // Branch not taken);
  }

  // Add increment to first operand, compare with odd register after R3
  CTEST(instruct, bxle_share1) {
    init_cpu();
    set_reg( 1, 0xffffffff); // Value
    set_reg( 3, 0x12345678); // Increment and comparand
    set_reg( 2, 0x1000); // Branch target
    set_mem(0x400, 0x87132200); //}, "BXLE 1, 3, 200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_reg(1));
    ASSERT_EQUAL(0x1200, IAR); // Branch taken);
  }

  CTEST(instruct, sll) {
    init_cpu();
    set_reg( 1, 0x82345678);
    set_reg( 2, 0x12340003); // Shift 3 bits
    set_mem(0x400, 0x891f2100); //}, "SLL 1,100(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x11a2b3c0, get_reg(1));
  }

  CTEST(instruct, srl) {
    init_cpu();
    set_reg( 1, 0x82345678);
    set_reg( 2, 0x12340003); // Shift 3 bits
    set_mem(0x400, 0x881f2100); //}, "SRL 1,100(2)");
      test_inst(0x0);
    ASSERT_EQUAL(0x82345678 >> 3, get_reg(1));
  }

  CTEST(instruct, sra) {
    init_cpu();
    set_reg( 2, 0x11223344);
    set_mem(0x400, 0x8a2f0105); //}, "SRA 2,105(0)"); // Shift right 5
      test_inst(0x0);
    ASSERT_EQUAL(0x0089119a, get_reg(2));
  }

  CTEST(instruct, sla3) {
    init_cpu();
    // From Princ Ops p143
    set_reg( 2, 0x007f0a72);
    set_mem(0x400, 0x8b2f0008); //}, "SLA 2,8(0)"); // Shift left 8
      test_inst(0x0);
    ASSERT_EQUAL(0x7f0a7200, get_reg(2));
  }

  CTEST(instruct, srdl) {
    init_cpu();
    set_reg( 4, 0x12345678);
    set_reg( 5, 0xaabbccdd);
    set_mem(0x400, 0x8c4f0118); //}, "SRDL 4,118(0)"); // Shift right 24 (x18)
      test_inst(0x0);
    ASSERT_EQUAL(0x00000012, get_reg(4));
    ASSERT_EQUAL(0x345678aa, get_reg(5));
  }

  CTEST(instruct, sldl) {
    init_cpu();
    set_reg( 4, 0x12345678);
    set_reg( 5, 0xaabbccdd);
    set_reg( 6, 8);
    set_mem(0x400, 0x8d4f6100); //}, "SLDL 4,100(6)"); // Shift left 8
      test_inst(0x0);
    ASSERT_EQUAL(0x345678aa, get_reg(4));
    ASSERT_EQUAL(0xbbccdd00, get_reg(5));
  }

  CTEST(instruct, sldl2) {
    init_cpu();
    set_reg( 4, 0x12345678);
    set_reg( 5, 0x00010001);
    set_mem(0x400, 0x8d4f051b); //}, "SLDL 4,51b(0)"); // Shift left 27
      test_inst(0x0);
    ASSERT_EQUAL(0xc0000800, get_reg(4));
    ASSERT_EQUAL(0x08000000, get_reg(5));
  }

  CTEST(instruct, sldl3) {
    init_cpu();
    set_mem(0x400, 0x8d1f2100); //}, "SLDL 1,100(2)");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, srda) {
    init_cpu();
    set_reg( 4, 0x12345678);
    set_reg( 5, 0xaabbccdd);
    set_mem(0x400, 0x8e4f0118); //}, "SRDA 4,118(0)"); // Shift right 24 (x18)
      test_inst(0x0);
    ASSERT_EQUAL(0x00000012, get_reg(4));
    ASSERT_EQUAL(0x345678aa, get_reg(5));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, srda_zero) {
    init_cpu();
    set_reg( 4, 0x02345678);
    set_reg( 5, 0xaabbccdd);
    set_mem(0x400, 0x8e4f013c); //}, "SRDA 4,13c(0)"); // Shift right 60 (x3c)
      test_inst(0x0);
    ASSERT_EQUAL(0x00000000, get_reg(4));
    ASSERT_EQUAL(0x00000000, get_reg(5));
    ASSERT_EQUAL(CC0, CC_REG); // Zero);
  }

  CTEST(instruct, srda2) {
    init_cpu();
    set_reg( 4, 0x92345678);
    set_reg( 5, 0xaabbccdd);
    set_mem(0x400, 0x8e4f0118); //}, "SRDA 4,118(0)"); // Shift right 24 (x18)
      test_inst(0x0);
    ASSERT_EQUAL(0xffffff92, get_reg(4));
    ASSERT_EQUAL(0x345678aa, get_reg(4));
    ASSERT_EQUAL(CC1, CC_REG); // Negative);
  }

  CTEST(instruct, slda3) {
    init_cpu();
    // From Princ Ops p143
    set_reg( 2, 0x007f0a72);
    set_reg( 3, 0xfedcba98);
    set_mem(0x400, 0x8f2f001f); //}, "SLDA 2,1f(0)");
      test_inst(0x0);
    ASSERT_EQUAL(0x7f6e5d4c, get_reg(2));
    ASSERT_EQUAL(0x00000000, get_reg(3));
  }

  CTEST(instruct, stm) {
    init_cpu();
    // From Princ Ops p143
    set_reg( 14, 0x00002563);
    set_reg( 15, 0x00012736);
    set_reg( 0, 0x12430062);
    set_reg( 1, 0x73261257);
    set_reg( 6, 0x00004000);
    set_mem(0x400, 0x90e16050); //, "STM 14,1,50(6)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00002563, get_mem(0x4050));
    ASSERT_EQUAL(0x00012736, get_mem(0x4054));
    ASSERT_EQUAL(0x12430062, get_mem(0x4058));
    ASSERT_EQUAL(0x73261257, get_mem(0x405C));
  }

  CTEST(instruct, tm) {
    init_cpu();
    // From Princ Ops p147
    set_mem(0x9998, 0xaafbaaaa);
    set_reg( 9, 0x00009990);
    set_mem(0x400, 0x91c39009); //}, "TM 9(9),c3");
      test_inst(0x0);
    ASSERT_EQUAL(CC3, CC_REG);
  }

  CTEST(instruct, tm2) {
    // From Princ Ops p147
    init_cpu();
    set_mem(0x9998, 0xaa3caaaa);
    set_reg( 9, 0x00009990);
    set_mem(0x400, 0x91c39009); //}, "TM 9(9),c3");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG);
  }

  CTEST(instruct, mvi) {
    init_cpu();
    set_reg( 1, 0x3456);
    set_cc(CC2);
    set_mem(0x3464, 0x12345678);
    set_mem(0x400, 0x92421010); //}, "MVI 10(1),42");
      test_inst(0x0);
    ASSERT_EQUAL(0x12344278, get_mem(0x3464));
    ASSERT_EQUAL(CC2, CC_REG); // Unchanged);
  }

  CTEST(instruct, ni) {
    init_cpu();
    set_reg( 1, 0x3456);
    set_mem(0x3464, 0x12345678);
    set_mem(0x400, 0x94f01010); //}, "NI 10(1),f0");
    ASSERT_EQUAL(0x12345078, get_mem(0x3464));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, ni_zero) {
    init_cpu();
    set_reg( 1, 0x3456);
    set_mem(0x3464, 0x12345678);
    set_mem(0x400, 0x94001010); //}, "NI 10(1),0");
      test_inst(0x0);
    ASSERT_EQUAL(0x12340078, get_mem(0x3464));
    ASSERT_EQUAL(CC0, CC_REG); // Zero);
  }

  CTEST(instruct, cli_zero) {
    init_cpu();
    set_reg( 1, 0x3456);
    set_mem(0x3464, 0x12345678);
    set_mem(0x400, 0x95561010); //}, "CLI 10(1),56");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // Equal);
  }

  CTEST(instruct, cli_low) {
    init_cpu();
    set_reg( 1, 0x3456);
    set_mem(0x3464, 0x12345678);
    set_mem(0x400, 0x95ff1010); //}, "CLI 10(1),ff");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // First operand is low);
  }

  CTEST(instruct, cli_all) {
    int  i;
    init_cpu();
    for (i = 0; i < 256 && i < testcycles * 3; i++) {
      set_reg( 1, 0x3456);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x95001010 | (i << 16)); //}, instr.str());
      test_inst(0x0);
      if (i == 0x56) {
        ASSERT_EQUAL(CC0, CC_REG); // Equal);
      } else if (i < 0x56) {
        ASSERT_EQUAL(CC2, CC_REG); // First operand is high);
      } else {
        ASSERT_EQUAL(CC1, CC_REG); // First operand is low);
      }
    }
  }

  CTEST(instruct, oi) {
    init_cpu();
    set_reg( 1, 2);
    set_mem(0x1000, 0x12345678);
    set_mem(0x400, 0x96421fff); //}, "OI fff(1),42");
      test_inst(0x0);
    ASSERT_EQUAL(0x12765678, get_mem(0x1000));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, xi) {
    init_cpu();
    set_reg( 0, 0x100); // Not used
    set_mem(0x120, 0x12345678);
    set_mem(0x400, 0x970f0123); //}, "XI 123(0),f");
      test_inst(0x0);
    ASSERT_EQUAL(0x12345677, get_mem(0x120));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, mvn) {
    init_cpu();
    // From Princ Ops p144
    set_mem(0x7090, 0xc1c2c3c4);
    set_mem(0x7094, 0xc5c6c7c8);
    set_mem(0x7040, 0xaaf0f1f2);
    set_mem(0x7044, 0xf3f4f5f6);
    set_mem(0x7048, 0xf7f8aaaa);
    set_reg( 14, 0x00007090);
    set_reg( 15, 0x00007040);
    set_mem(0x400, 0xd103f001);
    set_mem(0x404, 0xe000aaaa); //}, "MVN 1(4,15),0(14)");
      test_inst(0x0);
    ASSERT_EQUAL(0xc1c2c3c4, get_mem(0x7090));
    ASSERT_EQUAL(0xaaf1f2f3, get_mem(0x7040));
    ASSERT_EQUAL(0xf4f4f5f6, get_mem(0x7044));
    ASSERT_EQUAL(0xf7f8aaaa, get_mem(0x7048));
  }

  CTEST(instruct, mvc) {
    init_cpu();
    set_mem(0x100, 0x12345678);
    set_mem(0x200, 0x11223344);
    set_mem(0x400, 0xd2030100);
    set_mem(0x404, 0x02000000); //}, "MVC 100(4,0),200(0)"); // Move 4 bytes from 200 to 100
      test_inst(0x0);
    ASSERT_EQUAL(0x11223344, get_mem(0x100));
    ASSERT_EQUAL(0x11223344, get_mem(0x200)); // Unchanged);
  }

  CTEST(instruct, mvc2) {
    init_cpu();
    set_mem(0x100, 0x12345678);
    set_mem(0x104, 0xabcdef01);
    set_reg( 1, 2);
    set_reg( 2, 0);
    set_mem(0x400, 0xd2011100);
    set_mem(0x404, 0x01050000); //}, "MVC 100(2,1),105(0)"); // Move 2 bytes from 105 to 102
      test_inst(0x0);
    ASSERT_EQUAL(0x1234cdef, get_mem(0x100));
    ASSERT_EQUAL(0xabcdef01, get_mem(0x104)); // Unchanged);
  }

  CTEST(instruct, mvz) {
    init_cpu();
    // From Princ Ops page 144
    set_mem(0x800, 0xf1c2f3c4);
    set_mem(0x804, 0xf5c6aabb);
    set_reg( 15, 0x00000800);
    set_mem(0x400, 0xd304f001);
    set_mem(0x404, 0xf000aabb);  //}, "MVZ 1(5,15),0(15)");
      test_inst(0x0);
    ASSERT_EQUAL(0xf1f2f3f4, get_mem(0x800));
    ASSERT_EQUAL(0xf5f6aabb, get_mem(0x804));
  }

  CTEST(instruct, nc) {
    init_cpu();
    set_mem(0x358, 0x00001790);
    set_mem(0x360, 0x00001401);
    set_reg( 7, 0x00000358);
    set_mem(0x400, 0xd4037000);
    set_mem(0x404, 0x7008aaaa); //}, "NC 0(4,7),8(7)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00001400, get_mem(0x358));
  }

  CTEST(instruct, clc_equal) {
    init_cpu();
    set_reg( 1, 0x100);
    set_reg( 2, 0x100);
    set_mem(0x200, 0x12345633);
    set_mem(0x300, 0x12345644);
    set_mem(0x400, 0xd5021100);
    set_mem(0x404, 0x22000000); //}, "CLC 100(3,1),200(2)");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // equal);
  }

  CTEST(instruct, clc) {
    init_cpu();
    set_reg( 1, 0x100);
    set_reg( 2, 0x100);
    set_mem(0x200, 0x12345678);
    set_mem(0x300, 0x12345678);
    // 123456 vs 345678 because of offset
    set_mem(0x400, 0xd5021100);
    set_mem(0x404, 0x22010000); //}, "CLC 100(3,1),201(2)");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // first operand is low);
  }

  CTEST(instruct, oc) {
    init_cpu();
    set_mem(0x358, 0x00001790);
    set_mem(0x360, 0x00001401);
    set_reg( 7, 0x00000358);
    set_mem(0x400, 0xd6037000); 
    set_mem(0x404, 0x7008aaaa); //}, "OC 0(4,7),8(7)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00001791, get_mem(0x358));
  }

  CTEST(instruct, xc) {
    init_cpu();
    // From Princ Ops p146
    set_mem(0x358, 0x00001790);
    set_mem(0x360, 0x00001401);
    set_reg( 7, 0x00000358);
    set_mem(0x400, 0xd7037000);
    set_mem(0x404, 0x7008aaaa); //}, "XC 0(4,7),8(7)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00000391, get_mem(0x358));
    set_mem(0x400, 0xd7037008);
    set_mem(0x404, 0x7000aaaa); //}, "XC 8(4,7),0(7)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00001790, get_mem(0x360));
    set_mem(0x400, 0xd7037000);
    set_mem(0x404, 0x7008aaaa); //}, "XC 0(4,7),8(7)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00001401, get_mem(0x358));
  }

  CTEST(instruct, tr) {
    int   i;
    init_cpu();
    // Based on Princ Ops p147
    for (i = 0; i < 256; i += 4) {
      // Table increments each char by 3. Don"t worry about wrapping.
      set_mem(0x1000 + i, (((i + 3) << 24) | ((i + 4) << 16) | ((i + 5) << 8) | (i + 6)));
    }
    set_mem(0x2100, 0x12345678);
    set_mem(0x2104, 0xabcdef01);
    set_mem(0x2108, 0x11223344);
    set_mem(0x210c, 0x55667788);
    set_mem(0x2110, 0x99aabbcc);
    set_reg( 12, 0x00002100);
    set_reg( 15, 0x00001000);
    set_mem(0x400, 0xdc13c000);
    set_mem(0x404, 0xf000aaaa); //}, "TR 0(20,12),0(15)");
      test_inst(0x0);
    ASSERT_EQUAL(0x1537597b, get_mem(0x2100));
    ASSERT_EQUAL(0xaed0f204, get_mem(0x2104));
    ASSERT_EQUAL(0x14253647, get_mem(0x2108));
    ASSERT_EQUAL(0x58697a8b, get_mem(0x210c));
    ASSERT_EQUAL(0x9cadbecf, get_mem(0x2110));
  }

  CTEST(instruct, trt) {
    int   i;
    init_cpu();
    // Based on Princ Ops p147
    for (i = 0; i < 256; i += 4) {
      set_mem(0x1000 + i, 0);
    }
    set_mem(0x2020, 0x10203040);

    set_mem(0x3000, 0x12345621); // 21 will match table entry 20
    set_mem(0x3004, 0x11223344);
    set_mem(0x3008, 0x55667788);
    set_mem(0x300c, 0x99aabbcc);
    set_mem(0x400, 0xdd0f1000);
    set_mem(0x404, 0xf000aaaa); //}, "TRT 0(16,1),0(15)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00003003, get_reg(1)); // Match at 3003
    ASSERT_EQUAL(0x00000020, get_reg(2)); // Function value from table
    ASSERT_EQUAL(CC1, CC_REG); // not completed);
  }

  CTEST(instruct, ed) {
    init_cpu();
    // Princ Ops page 149
    set_mem(0x1200, 0x0257426c);
    set_mem(0x1000, 0x4020206b);
    set_mem(0x1004, 0x2020214b);
    set_mem(0x1008, 0x202040c3);
    set_mem(0x100c, 0xd9ffffff);
    set_mem(0x400, 0xde0cc000);
    set_mem(0x404, 0xc200aaaa); //}, "ED 0(13,12),200(12)");
      test_inst(0x0);
    ASSERT_EQUAL(0x4040f26b, get_mem(0x1000));
    ASSERT_EQUAL(0xf5f7f44b, get_mem(0x1004));
    ASSERT_EQUAL(0xf2f64040, get_mem(0x1008));
    ASSERT_EQUAL(0x40ffffff, get_mem(0x100c));
    ASSERT_EQUAL(CC2, CC_REG); // Result greater than zero);
  }

  CTEST(instruct, ed2) {
    init_cpu();
    // Princ Ops page 149
    set_mem(0x1200, 0x0000026d);
    set_mem(0x1000, 0x4020206b);
    set_mem(0x1004, 0x2020214b);
    set_mem(0x1008, 0x202040c3);
    set_mem(0x100c, 0xd9ffffff);
    set_mem(0x400, 0xde0cc000);
    set_mem(0x404, 0xc200aaaa); //, "ED 0(13,12),200(12)");
      test_inst(0x0);
    ASSERT_EQUAL(0x40404040, get_mem(0x1000));
    ASSERT_EQUAL(0x4040404b, get_mem(0x1004));
    ASSERT_EQUAL(0xf2f640c3, get_mem(0x1008));
    ASSERT_EQUAL(0xd9ffffff, get_mem(0x100c));
    ASSERT_EQUAL(CC1, CC_REG); // Result less than zero);
  }

  CTEST(instruct, edmk) {
    init_cpu();
    set_reg( 1, 0xaabbccdd);
    set_mem(0x1200, 0x0000026d);
    set_mem(0x1000, 0x4020206b);
    set_mem(0x1004, 0x2020214b);
    set_mem(0x1008, 0x202040c3);
    set_mem(0x100c, 0xd9ffffff);
    set_mem(0x400, 0xde0cc000);
    set_mem(0x404, 0xc200aaaa); //}, "ED 0(13,12),200(12)");
      test_inst(0x0);
    ASSERT_EQUAL(0x40404040, get_mem(0x1000));
    ASSERT_EQUAL(0x4040404b, get_mem(0x1004));
    ASSERT_EQUAL(0xf2f640c3, get_mem(0x1008));
    ASSERT_EQUAL(0xd9ffffff, get_mem(0x100c));
    ASSERT_EQUAL(CC1, CC_REG); // Result less than zero);
    ASSERT_EQUAL(0xaa001000, get_reg(1)); // Need to adjust this address);
  }

  CTEST(instruct, mvo) {
    // Princ Ops 152
    set_reg( 12, 0x00005600);
    set_reg( 15, 0x00004500);
    set_mem(0x5600, 0x7788990c);
    set_mem(0x4500, 0x123456ff);
    set_mem(0x400, 0xf132c000);
    set_mem(0x404, 0xf0000000); //}, "MVO 0(4, 12), 0(3, 15)");
      test_inst(0x0);
    ASSERT_EQUAL(0x0123456c, get_mem(0x5600));
  }

  CTEST(instruct, pack) {
    init_cpu();
    // Princ Ops p151
    set_reg( 12, 0x00001000);
    set_mem(0x1000, 0xf1f2f3f4);
    set_mem(0x1004, 0xc5000000);
    set_mem(0x400, 0xf244c000); 
    set_mem(0x404, 0xc0000000);  //, "PACK 0(5, 12), 0(5, 12)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00001234, get_mem(0x1000));
    ASSERT_EQUAL(0x5c000000, get_mem(0x1004));
  }

  CTEST(instruct, unpk) {
    init_cpu();
    // Princ Ops p151
    set_reg( 12, 0x00001000);
    set_reg( 13, 0x00002500);
    set_mem(0x2500, 0xaa12345d);
    set_mem(0x400, 0xf342c000);
    set_mem(0x404, 0xd0010000); //}, "UNPK 0(5, 12), 1(3, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(0xf1f2f3f4, get_mem(0x1000));
    ASSERT_EQUAL(0xd5000000, get_mem(0x1004));
  }

  CTEST(instruct, zap) {
    init_cpu();
    // Princ Ops p150
    set_reg( 9, 0x00004000);
    set_mem(0x4000, 0x12345678);
    set_mem(0x4004, 0x90aaaaaa);
    set_mem(0x4500, 0x38460dff);
    set_mem(0x400, 0xf8429000);
    set_mem(0x404, 0x95000000); //, "ZAP 0(5, 9), 500(3, 9)");
      test_inst(0x0);
    ASSERT_EQUAL(0x00003846, get_mem(0x4000));
    ASSERT_EQUAL(0x0daaaaaa, get_mem(0x4000));
    ASSERT_EQUAL(CC1, CC_REG); // Result less than zero);
  }

  CTEST(instruct, zap_short) {
    init_cpu();
    set_amwp(8); // ASCII
    set_mem(0x100, 0x2a000000); // 2+
    set_mem(0x200, 0x3a000000); // 3+
    set_mem(0x400, 0xf8000100);
    set_mem(0x404, 0x02000000); ///}, "ZAP 100(1, 0), 200(1, 0)");
      test_inst(0x0);
    ASSERT_EQUAL(0x3a000000, get_mem(0x100)); // 3+);
  }

  CTEST(instruct, zap_offest) {
    init_cpu();
    set_amwp(8); // ASCII
    set_mem(0x100, 0x002a0000); // 2+
    set_mem(0x200, 0x00003a00); // 3+
    set_mem(0x400, 0xf8000101);
    set_mem(0x404, 0x02020000);  //}, "ZAP 101(1, 0), 202(1, 0)");
      test_inst(0x0);
    ASSERT_EQUAL(0x003a0000, get_mem(0x100)); // 3+);
  }

  CTEST(instruct, cp) {
    init_cpu();
    // Princ Op page 150
    set_reg( 12, 0x00000600);
    set_reg( 13, 0x00000400);
    set_mem(0x700, 0x1725356d);
    set_mem(0x500, 0x0672142d);
    set_mem(0x400, 0xf933c100);
    set_mem(0x404, 0xd1000000);  //}, "CP 100(4, 12), 100(4, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // First lower);
  }

  CTEST(instruct, cp0) {
    init_cpu();
    set_reg( 12, 0x00000600);
    set_reg( 13, 0x00000400);
    set_mem(0x700, 0x1725356d);
    set_mem(0x500, 0x00172535);
    set_mem(0x504, 0x6d000000);
    set_mem(0x400, 0xf933c100);
    set_mem(0x404, 0xd1010000); //, "CP 100(4, 12), 101(4, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // Equal);
  }

  CTEST(instruct, cp3) {
    init_cpu();
    set_reg( 12, 0x00000600);
    set_reg( 13, 0x00000400);
    set_mem(0x700, 0x1725346d);
    set_mem(0x500, 0x00172535);
    set_mem(0x504, 0x6d000000);
    set_mem(0x400, 0xf933c100);
    set_mem(0x404, 0xd1010000); //, "CP 100(4, 12), 101(4, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(CC2, CC_REG); // First higher);
  }

  CTEST(instruct, ap) {
    init_cpu();
    // PrincOps p 150
    set_reg( 12, 0x00002000);
    set_reg( 13, 0x000004fd);
    set_mem(0x2000, 0x38460d00);
    set_mem(0x500, 0x0112345c);
    set_mem(0x400, 0xfa23c000);
    set_mem(0x404, 0xd0030000); //}, "AP 0(3, 12), 3(4, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(0x73885c00, get_mem(0x2000));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, sp) {
    init_cpu();
    set_reg( 12, 0x00002000);
    set_reg( 13, 0x000004fd);
    set_mem(0x2000,  0x38460c00); // arg1
    set_mem(0x500,  0x0112345c); // arg2
    set_mem(0x400, 0xfb23c000);
    set_mem(0x404, 0xd0030000); //}, "SP 0(3, 12), 3(4, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(0x73885d00, get_mem(0x2000));
    ASSERT_EQUAL(CC1, CC_REG); // Negative);
  }

  CTEST(instruct, mp) {
    // PrincOps p 151
    init_cpu();
    set_reg( 4, 0x00001200);
    set_reg( 6, 0x00000500);
    set_mem(0x1200, 0xffff3846);
    set_mem(0x1204, 0x0dffffff);
    set_mem(0x500, 0x321dffff);
    set_mem(0x400, 0xfc414100);
    set_mem(0x404, 0x60000000); //}, "MP 100(5, 4), 0(2, 6)");
      test_inst(0x0);
    ASSERT_EQUAL(0x01234566, get_mem(0x1300));
    ASSERT_EQUAL(0x0c000000, get_mem(0x1304));
    ASSERT_EQUAL(CC2, CC_REG); // Positive);
  }

  CTEST(instruct, dp) {
    // PrincOps p 151
    init_cpu();
    set_reg( 12, 0x00002000);
    set_reg( 13, 0x00003000);
    set_mem(0x2000, 0x01234567);
    set_mem(0x2004, 0x8cffffff);
    set_mem(0x3000, 0x321dffff);
    set_mem(0x400, 0xfd41c000);
    set_mem(0x404, 0xd0000000); //, "DP 0(5, 12), 0(2, 13)");
      test_inst(0x0);
    ASSERT_EQUAL(0x38460d01, get_mem(0x2000));
    ASSERT_EQUAL(0x8cffffff, get_mem(0x2004));
  }

  CTEST(instruct, lm) {
    init_cpu();
    set_reg( 3, 0x10);
    set_mem(0x110, 0x12345678);
    set_mem(0x114, 0x11223344);
    set_mem(0x118, 0x55667788);
    set_mem(0x11c, 0x99aabbcc);
    set_mem(0x400, 0x98253100); //}, "LM 2,5,100(3)");
    // Load registers 2 through 5 starting at 0x110
      test_inst(0x0);
    ASSERT_EQUAL(0x12345678, get_reg(2));
    ASSERT_EQUAL(0x11223344, get_reg(3));
    ASSERT_EQUAL(0x55667788, get_reg(4));
    ASSERT_EQUAL(0x99aabbcc, get_reg(5));
  }

  CTEST(instruct, mvi2) {
    init_cpu();
    set_mem(0x100, 0x11223344);
    set_reg( 1, 1);
    set_mem(0x400, 0x92551100); //}, "MVI 100(1),55"); // Move byte 55 to location 101
      test_inst(0x0);
    ASSERT_EQUAL(0x11553344, get_mem(0x100));
  }

  CTEST(instruct, clr) {
    init_cpu();
    set_reg( 1, 0x12345678);
    set_reg( 2, 0x12345678);
    set_mem(0x400, 0x15120000); //}, "CLR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // equal);

    set_reg( 1, 0x12345678);
    set_reg( 2, 0x12345679);
    set_mem(0x400, 0x15120000); //}, "CLR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // first operand is low);

    set_reg( 1, 0x12345679);
    set_reg( 2, 0x12345678);
    set_mem(0x400, 0x15120000); //}, "CLR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(CC2, CC_REG); // first operand is high);

    set_reg( 1, 0x7fffffff);
    set_reg( 2, 0x8fffffff);
    set_mem(0x400, 0x15120000); //}, "CLR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // first operand is low);
  }

  CTEST(instruct, clr2) {
    init_cpu();
    set_reg( 1, 0x12345678);
    set_reg( 2, 0x100);
    set_reg( 3, 0x100);
    set_mem(0x300, 0x12345678);
    set_mem(0x400, 0x55123100); //}, "CL 1,100(2,3)");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // equal);
  }

  CTEST(instruct, nr) {
    init_cpu();
    set_reg( 1, 0xff00ff00);
    set_reg( 2, 0x12345678);
    set_mem(0x400, 0x14120000); //}, "NR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL( 0x12005600, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, nr0) {
    init_cpu();
    set_reg( 1, 0x12345678);
    set_reg( 2, 0xedcba987);
    set_mem(0x400, 0x14120000); //}, "NR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(CC0, CC_REG); // Zero);
  }

  CTEST(instruct, or2) {
    init_cpu();
    set_reg( 1, 0xff00ff00);
    set_reg( 2, 0x12345678);
    set_mem(0x400, 0x16120000); //, "OR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0xff34ff78, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, xr) {
    init_cpu();
    set_reg( 1, 0xff00ff00);
    set_reg( 2, 0x12345678);
    set_mem(0x400, 0x17120000); //, "XR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0xed34a978, get_reg(1));
    ASSERT_EQUAL(CC1, CC_REG); // Not zero);
  }

  CTEST(instruct, sll2) {
    int i;
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      set_reg( 1, 1);
      set_reg( 2, 0x12340000 + i); // Shift i bits
      set_mem(0x400, 0x891f2100); //}, "SLL 1,100(2)");
      test_inst(0x0);
      ASSERT_EQUAL((uint32_t)(1 << i), get_reg(1));
    }
  }

  CTEST(instruct, bcr) {
    init_cpu();
    set_reg( 1, 0x12345678); // Branch destination
    set_cc(CC0);
    set_mem(0x400, 0x07810000); //}, "BCR 8,1");
      test_inst(0x0);
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, bcr_always) {
    init_cpu();
    set_reg( 1, 0x12345678); // Branch destination
    set_cc(CC0);
    set_mem(0x400, 0x07f10000); //}, "BCR 15,1"); // always
      test_inst(0x0);
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, bcr_not) {
    init_cpu();
    set_reg( 1, 0x12345678); // Branch destination
    set_cc(CC1);
    set_mem(0x400, 0x07810000); //}, "BCR 8,1");
      test_inst(0x0);
    ASSERT_EQUAL(0x402, IAR);
  }

  CTEST(instruct, balr) {
    init_cpu();
    set_ilc(2);
    set_cc(CC3);
    set_reg( 2, 0x12345678); // Branch destination
    set_mem(0x400, 0x05120000); //, "BALR 1,2");
      test_inst(0xa);
    ASSERT_EQUAL(0x7a000402, get_reg(1)); // low-order PSW: ILC, CR, PROGMASK, return IAR);
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, balr_not) {
    init_cpu();
    set_ilc(2); // overwritten with 1
    set_cc(CC3);
    set_mem(0x400, 0x05100000); //}, "BALR 1,0");
      test_inst(0xa);
    ASSERT_EQUAL(0x7a000402, get_reg(1)); // low-order PSW: ILC, CR, PROGMASK, return IAR);
    ASSERT_EQUAL(0x402, IAR);
  }

  CTEST(instruct, bctr_taken) {
    init_cpu();
    set_reg( 1, 3); // Counter
    set_reg( 2, 0x12345678); // Branch destination
    set_mem(0x400, 0x06120000); //}, "BCTR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(2, get_reg( 1));
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, bctr_taken_neg) {
    init_cpu();
    set_reg( 1, 0); // Counter
    set_reg( 2, 0x12345678); // Branch destination
    set_mem(0x400, 0x06120000); //}, "BCTR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0xffffffff, get_reg(1));
    ASSERT_EQUAL(0x00345678, IAR);
  }

  CTEST(instruct, bctr_not_taken) {
    init_cpu();
    set_reg( 1, 1); // Counter
    set_reg( 2, 0x12345678); // Branch destination
    set_mem(0x400, 0x06120000); //, "BCTR 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0, get_reg(1));
    ASSERT_EQUAL(0x402, IAR);
  }

  CTEST(instruct, spm) {
    init_cpu();
    set_reg( 1, 0x12345678); // Mask 2
    set_mem(0x400, 0x041f0000); //, "SPM 1");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG);
    ASSERT_EQUAL(0x2, PM);
  }

  CTEST(instruct, svc) {
    init_cpu();
    // Need more testing here
      set_mem(0x400, 0x0a120000); // "SVC 12");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, ssk) {
    init_cpu();
    set_amwp(0); // Privileged
    set_mem(0x400, 0x08120000); //, "SSK 1,2");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, ssk2) {
    init_cpu();
    set_amwp(0);  // Privileged
    set_reg( 1, 0x11223344); // Key
    set_reg( 2, 0x00005670); // Address: last 4 bits must be 0
    set_mem(0x400, 0x08120000); //, "SSK 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(4, cpu_2030.MP[(0x00005678 & 0x00fff800) >> 11]);
  }

  CTEST(instruct, ssk3) {
    init_cpu();
    set_amwp(0);  // Privileged
    set_reg( 1, 0x11223344); // Key
    set_reg( 2, 0x12345674); // Unaligned: last 4 bits not 0
    set_mem(0x400, 0x08120000); //}, "SSK 1,2");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  // ISK reads the storage key
  CTEST(instruct, isk) {
    init_cpu();
    set_amwp(0);  // Privileged
    cpu_2030.MP[(0x00005670 & 0x00fff800) >> 11] = 2;
    set_reg( 1, 0x89abcdef);
    set_reg( 2, 0x00005670); // Aligned: last 4 bits 0
    set_mem(0x400, 0x09120000); //, "ISK 1,2");
      test_inst(0x0);
    ASSERT_EQUAL(0x89abcd20, get_reg(1));
  }

  CTEST(instruct, isk2) {
    init_cpu();
    set_amwp(1);  // Unprivileged
    cpu_2030.MP[(0x12345670 & 0x00fff800) >> 11] = 2;
    set_reg( 1, 0xaabbccdd);
    set_reg( 2, 0x12345674); // Unaligned: last 4 bits not 0
    set_mem(0x400, 0x09120000); //}, "ISK 1,2");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, isk3) {
    init_cpu();
    set_amwp(0); // Privileged
    cpu_2030.MP[(0x12345670 & 0x00fff800) >> 11] = 2;
    set_reg( 1, 0xaabbccdd);
    set_reg( 2, 0x12345674); // Unaligned: last 4 bits not 0
    set_mem(0x400, 0x09120000); //, "ISK 1,2");
      test_inst(0x0);
    ASSERT_TRUE(trap_flag);
  }

  CTEST(instruct, ts) {
    init_cpu();
    set_reg( 2, 2); // Index
    set_mem(0x100, 0x83857789); // 102 top bit not set
    set_mem(0x400, 0x93002100); //, "TS 100(2)");
      test_inst(0x0);
    ASSERT_EQUAL(CC0, CC_REG); // Not set);
    ASSERT_EQUAL(0x8385ff89, get_mem(0x100));
  }

  CTEST(instruct, ts2) {
    init_cpu();
    set_reg( 2, 2); // Index
    set_mem(0x100, 0x8385c789); // 102 top bit set
    set_mem(0x400, 0x93002100); //, "TS 100(2)");
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG); // Set);
    ASSERT_EQUAL(0x8385ff89, get_mem(0x100));
  }

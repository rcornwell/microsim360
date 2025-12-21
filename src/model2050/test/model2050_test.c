/*
 * microsim360 - Model 2050 CPU instruction test cases.
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
#include "device.h"
#include "logger.h"
#include "ctest.h"
#include "cpu.h"
#include "conf.h"
#include "model2050.h"
#include "xlat.h"
#include "model_test.h"
#include "test_device.h"


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

int       verbose = 0;

char     *test_log_file = "debug.log";
char     *test_log_level = "info warn error trace itrace micro reg mem mpxchn selchn device";


void
init_tests()
{
    load_line("2050F");
    RATE_SW = 1;
}


void
setup_fp2050(void *rend)
{
}

#define FTEST(a, b)   CTEST(a, b)
#define DTEST(a, b)   CTEST(a, b)
#define MTEST(a, b)   CTEST(a, b)

uint64_t         step_count;         /** Current step number */
int              testcycles = 100;   /** Number of test cycles for random tests */
int              trap_flag;          /** Indicates if CPU traped */


/* Set MASK */
void set_mask(uint8_t mask)
{
    /* Set backup MASK */
    cpu_2050.LS[0x17] &= 0x00ffffff;
    cpu_2050.LS[0x17] |= (mask << 24);
    /* Set hardware MASK */
    cpu_2050.MASK = mask;
}

/* Get MASK */
uint8_t get_mask()
{
    return cpu_2050.MASK;
}

/* Set AMWP mask */
void
set_amwp(int n) {
   /* Set backup AMWP flags */
   cpu_2050.LS[0x17] &= 0xfff0ffff;
   cpu_2050.LS[0x17] |= (n << 16);
   /* Set hardware AMWP flags */
   cpu_2050.AMWP = n;
}

/* Set storage Key */
void
set_key(int n) {
    cpu_2050.LS[0x17] = (cpu_2050.MASK << 24) | (n << 20);
    cpu_2050.KEY = n;
}

/* Read register */
uint32_t
get_reg(int num)
{
    return cpu_2050.LS[num + 0x30];
}

/* Write register */
void
set_reg(int num, uint32_t data)
{
    cpu_2050.LS[num + 0x30] = data;
}

/* Read word from main memory */
uint32_t
get_mem(int addr)
{
    return M[addr >> 2];
}

/* Set word into main memory */
void
set_mem(int addr, uint32_t data)
{
    M[addr >> 2] = data;
}

/* Get the memory protection key for a given address */
uint8_t
get_mem_key(int addr)
{
    return cpu_2050.MP[((addr & 0xf800) >> 11)];
}

/* Set the memory protection key for a given address */
void
set_mem_key(int addr, int key)
{
    cpu_2050.MP[((addr & 0xf800) >> 11)] = key;
}

/* Read byte from main memory */
uint8_t
get_mem_b(int addr)
{
    uint32_t   v = M[addr >> 2];
    return (uint8_t)(v >> (8 * (3 - (addr & 3))) & 0xff);
}

/* Set byte into main memory */
void
set_mem_b(int addr, uint8_t data)
{
    int        offset = 8 * (3 - (addr & 3));
    uint32_t   mask = 0xff;
    M[addr>>2] &= ~(mask << offset);
    M[addr>>2] |= ((uint32_t)data << offset);
}

/* Get program counter */
uint32_t
get_pc()
{
     return cpu_2050.IA_REG;
}

/* Get a floating point register */
uint32_t
get_fpreg_s(int num)
{
    return cpu_2050.LS[num + 0x20];
}

/* Set a floating point register short */
void
set_fpreg_s(int num, uint32_t data)
{
    cpu_2050.LS[num + 0x20] = data;
}

/* Get a floating point register */
uint64_t
get_fpreg_d(int num)
{
    uint64_t    v;
    v =  (uint64_t)cpu_2050.LS[num + 0x20] << 32;
    v |=  (uint64_t)cpu_2050.LS[num + 0x21];
    return v;
}

/* Set a floating point register short */
void
set_fpreg_d(int num, uint64_t data)
{
    cpu_2050.LS[num + 0x20] = (uint32_t)(data >> 32);
    cpu_2050.LS[num + 0x21] = (uint32_t)(data & 0xffffffff);
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
    uint64_t        t64 = get_fpreg_d(num);
    double          d;
    int             e;
    e = ((t64 >> 56) & 0x7f) - 64;
    d = (double)(0x00ffffff00000000LL & t64);
    d *= exp2(-56 + 4*e);
    if ((0x8000000000000000LL & t64) != 0)
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

/** Initialize CPU before testing.
 *
 * Issue reset to CPU wait for it to get to
 * fetching first instruction.
 */
void
init_cpu()
{
    SYS_RST = 1;
    CHK_SW = 2;
    RATE_SW = 1;
    PROC_SW = 1;
    log_trace("Reset CPU\n");
    do {
        cycle_2050();
    } while (cpu_2050.ROAR != 0x150);
    set_amwp(0);
    set_key(0);
    set_cc(0);
    set_mask(0);
}

/** Test run of one instruction.
 *
 * Test running one instruction. Stop on fetch of next,
 * attempt to execute 00 instruction. 
 * Set trap flag if CPU traps
 */
void
test_inst(int mask)
{
    int      max = 0;
    cpu_2050.IA_REG = 0x400;
    cpu_2050.PMASK = (mask & 0xf);
    trap_flag = 0;
    cpu_2050.ROAR = 0x190;
    cpu_2050.REFETCH = 1;
    cpu_2050.mem_state = 0;
        log_trace("Start inst\n");
    do {
        cycle_2050();
        step_count++;
        max++;
         if ((cpu_2050.ROAR & 0xffc) == 0x148)
           break;
        if ((cpu_2050.ROAR == 0x188) && (cpu_2050.SDR_REG == 0))
           break;
        if (cpu_2050.ROAR == 0x10e)
           trap_flag = 1;
        log_trace("ROAR = [%03X]\n", cpu_2050.ROAR);
    } while (max < 1000);
    if (max > 900)
        log_trace("overrun\n");

}

/** Run several instructions.
 *
 * Run instructions until 00 opcode is executed.
 * Set trap flag if CPU traps.
 */
void
test_inst2()
{
    int      max = 0;
    int      count = 0;

    cpu_2050.IA_REG = 0x400;
    START = 1;
    cpu_2050.ROAR = 0x190;
    cpu_2050.REFETCH = 1;
    cpu_2050.mem_state = 0;
    trap_flag = 0;
    do {
        cycle_2050();
        step_count++;
        max++;
        log_trace("ROAR = [%03X]\n", cpu_2050.ROAR);
        if ((cpu_2050.ROAR & 0xffc) == 0x148) {
           if (count++ == 2)
               break;
        }
        if ((cpu_2050.ROAR == 0x182) && (cpu_2050.SDR_REG == 0))
           break;
        if (cpu_2050.ROAR == 0x10e)
           trap_flag = 1;
    } while (max < 1000);
}

/** Run CPU and channels until 00 instruction.
 *
 * Run CPU and channel devices, set trap flag if CPU traps.
 */
void
test_io_inst(int mask)
{
    int      max = 0;
    int      stop_flag = 0;
    device_t *dev;
    int      i;
    int      flag = 0;

    cpu_2050.IA_REG = 0x400;
    cpu_2050.PMASK = (mask & 0xf);
    START = 1;
    trap_flag = 0;
    log_trace("Test IO\n");
    do {
        cycle_2050();
        if (flag) {
            for(i = 0; i < 4; i++) {
                for (dev = chan[i]; dev != NULL; dev = dev->next) {
                    test_step(dev);
                }
            }
        }
        flag = !flag;
        step_count++;
        max++;
        if ((cpu_2050.ROAR == 0x182)) {// && (cpu_2050.SDR_REG == 0)) {
           stop_flag = 1;
        }
        if (cpu_2050.ROAR == 0x10e)
           trap_flag = 1;
        log_trace("ROAR = [%03X]\n", cpu_2050.ROAR);
    } while (stop_flag == 0);
    log_trace("Test IO Done\n");
}

/** Run I/O instruction for at most 20000 microcycles.
 */
void
test_io_inst2()
{
    int      max = 0;
    device_t *dev;
    int      i;
    int      flag = 0;

    cpu_2050.IA_REG = 0x400;
    cpu_2050.PMASK = 0;
    trap_flag = 0;
    do {
        cycle_2050();
        if (flag) {
            for(i = 0; i < 4; i++) {
                for (dev = chan[i]; dev != NULL; dev = dev->next) {
                    test_step(dev);
                }
            }
        }
        flag = !flag;
        step_count++;
        max++;
        log_trace("ROAR = [%03X]\n", cpu_2050.ROAR);
        if (cpu_2050.IA_REG == 0x428) {
            break;
        }
        if (cpu_2050.ROAR == 0x10e)
           trap_flag = 1;
    } while (max < 20000);
    log_trace("Max = %d\n", max);
}

#include "inst_test_cases.h"

/*
 * microsim360 - Model 2050 CPU instruction test definitions.
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

#include "model2050.h"

#define CC_REG cpu_2050.CC
#define CC0    0x0
#define CC1    0x1
#define CC2    0x2
#define CC3    0x3

#define IAR  (cpu_2050.IA_REG)

#define MASK cpu_2050.MASK

#define PM            cpu_2050.PMASK

#define set_ilc(n)    cpu_2050.ILC = n

#define get_ilc()     cpu_2050.ILC

#define get_amwp()    cpu_2050.AMWP

#define get_key()     cpu_2050.KEY

#define set_cc(n)     CC_REG = n

extern uint64_t         step_count;
extern int              testcycles;
extern int              irq_mask;

/* Set AMWP */
void set_amwp(int num);

/* Set key */
void set_key(int num);

/* Read register */
uint32_t get_reg(int num);

/* Write register */
void set_reg(int num, uint32_t data);

/* Read word from main memory */
uint32_t get_mem(int addr);

/* Set word into main memory */
void set_mem(int addr, uint32_t data);

/* Get the memory protection key for a given address */
uint8_t get_mem_key(int addr);

/* Set the memory protection key for a given address */
void set_mem_key(int addr, int key);

/* Read byte from main memory */
uint8_t get_mem_b(int addr);

/* Set byte into main memory */
void set_mem_b(int addr, uint8_t data);

/* Get a floating point register */
uint32_t get_fpreg_s(int num);

/* Set a floating point register short */
void set_fpreg_s(int num, uint32_t data);

/* Get a floating point register */
uint64_t get_fpreg_d(int num);

/* Set a floating point register short */
void set_fpreg_d(int num, uint64_t data); 

/* Initialize the CPU to run tests */
void init_cpu();

extern int trap_flag;

/* Run one instructions */
void test_inst(int mask);

/* Execute two instructions */
void test_inst2();

/* Run one instructions */
void test_io_inst(int mask);

/* Run one instructions */
void test_io_inst2();


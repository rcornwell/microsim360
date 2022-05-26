/*
 * microsim360 - Model 2030 CPU instruction test definitions.
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

#define CC_REG (cpu_2030.LS[0xBB] & 0xf0)
#define CC0    0x80
#define CC1    0x40
#define CC2    0x20
#define CC3    0x10

#define PM     (cpu_2030.LS[0xBB] & 0x0f)

#define IAR   (((cpu_2030.I_REG & 0xff) << 8) | (cpu_2030.J_REG & 0xff))

#define MASK   cpu_2030.MASK

extern uint64_t         step_count;
extern int              testcycles;
extern int              irq_mask;

/* Set ILC code */
void set_ilc(int num);

/* get ILC code */
int get_ilc();

/* Set AMWP */
void set_amwp(int num);

/* Get AMWP */
int get_amwp();

/* Set key */
void set_key(int num);

/* Get key */
int get_key();

void set_cc(int cc);

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


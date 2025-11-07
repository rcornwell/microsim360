/*
 * microsim360 - Test channel driver.
 *
 * Copyright 2025, Richard Cornwell
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
#include "device.h"

/* Read word from main memory */
uint32_t get_mem(int addr);

/* Set word into main memory */
void set_mem(int addr, uint32_t data);

/* Read byte from main memory */
uint8_t get_mem_b(int addr);

/* Set byte into main memory */
void set_mem_b(int addr, uint8_t data);

/**
 * Start I/O operation.
 *
 * Process a chain of commands.
 * Returns status, and flags.
 * 0x100 no device.
 * 0x200 device busy.
 * 0x300 Device did not match requested.
 * 0x4xx Length error.
 * 0x800 Invalid sequence.
 */
uint16_t start_io(uint8_t device, uint16_t caw, int sel, int halt);
/**
 * Issue a test I/O to device.
 */
uint16_t test_io(uint8_t device);
/**
 *  Wait for device to give final status.
 */
uint16_t wait_dev(uint8_t  device);

/**
 * Function to advance device by one clock cycle.
 */
void     test_advance();

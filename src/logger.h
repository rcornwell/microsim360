/*
 * microsim360 - Logger class
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

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>
#include <stdarg.h>

extern int log_level;
extern FILE *log_file;

#define LOG_INFO     0x0001              /* Log informational messages */
#define LOG_WARN     0x0002              /* Log warnings */
#define LOG_ERROR    0x0004              /* Log error */
#define LOG_TRACE    0x0008              /* Generic trace messages */
#define LOG_ITRACE   0x0010              /* Log Instruction trace */
#define LOG_MICRO    0x0020              /* Log micro instructions */
#define LOG_REG      0x0040              /* Log Micro register state */
#define LOG_MEM      0x0080              /* Log memory access */
#define LOG_MPXCHN   0x0100              /* Log muliplex channel status */
#define LOG_SELCHN   0x0200              /* Log selecter channel status */
#define LOG_DEVICE   0x0400              /* Log device messages */
#define LOG_CONSOLE  0x0800              /* Log console traffic */
#define LOG_TAPE     0x1000              /* Log detailed tape information */
#define LOG_DISK     0x2000              /* Log detailed disk information */
#define LOG_CARD     0x4000              /* Log detailed card information */

void log_init(char *filename);
void log_print_c(int level, const char *fmt, ...);
void log_print_s(int level, char *filename, int line, const char *fmt, ...);
void log_print(int level, char *filename, int line, const char *fmt, ...);

#define log_info(...) if ((log_level & LOG_INFO) != 0) \
                              log_print( LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define log_warn(...) if ((log_level & LOG_WARN) != 0) \
                              log_print( LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

#define log_error(...) if ((log_level & LOG_ERROR) != 0) \
                              log_print( LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define log_trace(...) if ((log_level & LOG_TRACE) != 0) \
                              log_print( LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

#define log_itrace(...) if ((log_level & LOG_ITRACE) != 0) \
                              log_print( LOG_ITRACE, __FILE__, __LINE__, __VA_ARGS__)

#define log_itrace_s(...) if ((log_level & LOG_ITRACE) != 0) \
                              log_print_s( LOG_ITRACE, __FILE__, __LINE__, __VA_ARGS__)

#define log_itrace_c(...) if ((log_level & LOG_ITRACE) != 0) \
                              log_print_c( LOG_ITRACE, __VA_ARGS__)

#define log_micro(...) if ((log_level & LOG_MICRO) != 0) \
                              log_print( LOG_MICRO, __FILE__, __LINE__, __VA_ARGS__)

#define log_reg(...) if ((log_level & LOG_REG) != 0) \
                              log_print( LOG_REG, __FILE__, __LINE__, __VA_ARGS__)

#define log_mem(...) if ((log_level & LOG_MEM) != 0) \
                              log_print( LOG_MEM, __FILE__, __LINE__, __VA_ARGS__)

#define log_mpxchn(...) if ((log_level & LOG_MPXCHN) != 0) \
                              log_print( LOG_MPXCHN, __FILE__, __LINE__, __VA_ARGS__)

#define log_selchn(...) if ((log_level & LOG_SELCHN) != 0) \
                              log_print( LOG_SELCHN, __FILE__, __LINE__, __VA_ARGS__)

#define log_device(...) if ((log_level & LOG_DEVICE) != 0) \
                              log_print( LOG_DEVICE, __FILE__, __LINE__, __VA_ARGS__)

#define log_console(...) if ((log_level & LOG_CONSOLE) != 0) \
                              log_print( LOG_CONSOLE, __FILE__, __LINE__, __VA_ARGS__)

#define log_tape(...) if ((log_level & LOG_TAPE) != 0) \
                              log_print( LOG_TAPE, __FILE__, __LINE__, __VA_ARGS__)

#define log_tape_c(...) if ((log_level & LOG_TAPE) != 0) \
                              log_print_c( LOG_TAPE, __VA_ARGS__)

#define log_tape_s(...) if ((log_level & LOG_TAPE) != 0) \
                              log_print_s( LOG_TAPE, __FILE__, __LINE__, __VA_ARGS__)

#define log_card(...) if ((log_level & LOG_CARD) != 0) \
                              log_print( LOG_CARD, __FILE__, __LINE__, __VA_ARGS__)

#define log_card_c(...) if ((log_level & LOG_CARD) != 0) \
                              log_print_c( LOG_CARD, __VA_ARGS__)

#define log_card_s(...) if ((log_level & LOG_CARD) != 0) \
                              log_print_s( LOG_CARD, __FILE__, __LINE__, __VA_ARGS__)
#define log_disk(...) if ((log_level & LOG_DISK) != 0) \
                              log_print( LOG_DISK, __FILE__, __LINE__, __VA_ARGS__)

#define log_disk_c(...) if ((log_level & LOG_DISK) != 0) \
                              log_print_c( LOG_DISK, __VA_ARGS__)

#define log_disk_s(...) if ((log_level & LOG_DISK) != 0) \
                              log_print_s( LOG_DISK, __FILE__, __LINE__, __VA_ARGS__)
#endif

/*
 * microsim360 - Model 2030 microcode simulator.
 *
 * Copyright 2021, Richard Cornwell
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "device.h"
#include "xlat.h"
#include "cpu.h"
#include "model2030.h"
#include "model1052.h"


struct CPU_2030 cpu_2030;

/* Machine check bits */
#define AREG    0x80
#define BREG    0x40
#define MNREG   0x20
#define ALU     0x01

#define E_SW_SEL_Q    (E_SW == 0x10)
#define E_SW_SEL_C    (E_SW == 0x11)
#define E_SW_SEL_F    (E_SW == 0x12)
#define E_SW_SEL_TT   (E_SW == 0x13)
#define E_SW_SEL_TI   (E_SW == 0x14)
#define E_SW_SEL_JI   (E_SW == 0x15)
#define E_SW_SEL_GS   (E_SW == 0x16)
#define E_SW_SEL_GT   (E_SW == 0x17)
#define E_SW_SEL_GUV  (E_SW == 0x18)
#define E_SW_SEL_HS   (E_SW == 0x19)
#define E_SW_SEL_HT   (E_SW == 0x1a)
#define E_SW_SEL_HUV  (E_SW == 0x1b)
#define E_SW_SEL_MAIN_STG   (E_SW == 0x20)
#define E_SW_SEL_AUX_STG    (E_SW == 0x21)
#define E_SW_SEL_GX   (E_SW == 0x1e)
#define E_SW_SEL_GY   (E_SW == 0x1f)
#define E_SW_SEL_I    (E_SW == 0x30)
#define E_SW_SEL_J    (E_SW == 0x31)
#define E_SW_SEL_U    (E_SW == 0x32)
#define E_SW_SEL_V    (E_SW == 0x33)
#define E_SW_SEL_L    (E_SW == 0x34)
#define E_SW_SEL_T    (E_SW == 0x35)
#define E_SW_SEL_D    (E_SW == 0x36)
#define E_SW_SEL_R    (E_SW == 0x37)
#define E_SW_SEL_S    (E_SW == 0x38)
#define E_SW_SEL_G    (E_SW == 0x39)
#define E_SW_SEL_H    (E_SW == 0x3a)
#define E_SW_SEL_FI   (E_SW == 0x3b)
#define E_SW_SEL_FT   (E_SW == 0x3c)

static char *ch_name[] = {
     "0", "1", "RO", "V67=0", "STI", "OPI", "AC", "S0",
     "S1", "S2", "S4", "S6", "G0", "G2", "G4", "G6"};

static char *cl_name[] = {
     "0", "1", "CA>W", "AI", "SVI", "R=VDD", "1CB", "Z=0",
     "G7", "S3", "S5", "S7", "G1", "G3", "G5", "INTR" };

static char *cm_name[] = {
     "Write", "Comp", "Store", "Read IJ", "Read UV", "Read T",
     "Read CKN", "Read GUV"};

static char *cu1_name[] = {   /* Names for cm = 03-7 */
     "MS", "LS", "MPX", "MLS" };
static char *cu2_name[] = {   /* Name for cm = 0-2 */
     "x", "GR", "K>W", "FWX>WX"};

static char *ca_name[] = {
     "FT", "TT", "", "", "S", "H", "FI", "R",
     "D", "L", "G", "T", "V", "U", "J", "I",
     "F", "SFG", "MC", "",  "C", "Q", "JI", "TI",
     "", "", "", "", "GR", "GS", "GT", "GJ"};

static char *cb_name[] = {
     "R", "L", "D", "K"};

static char *ck_name[] = {
     "0", "1", "2", "3", "4", "5", "6", "7",
     "8", "9", "a", "b", "c", "d", "e", "f",
     "", "UV>WX", "WRAP>Y", "WRAP>X6", "SHI", "ACFORCE",
     "Rhl", "Sll", "1>OE", "ASCII", "INT>X6X7", "0>MC",
     "Y>WRAP", "0>IPL", "0>F", "1>F0" };

static char *ckb_name[] = {
     "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
     "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"};

static char *cd_name[] = {
     "Z", "TE", "JE", "Q", "TA", "H", "S", "R",
     "D", "L", "G", "T", "V", "U", "J", "I" };

static char *cf_name[] = {
     "0", "L", "H", "", "Stop", "XL", "XH", "X" };

static char *cg_name[] = {
     "0", "L", "H", "" };

static char *cc_name[] = {
     "+", "+1", ".", "|", "0c", "1c", "cc", "^" };

static char *cs_name[] = {
     "", "LZ>S5", "HZ>S4", "LZ>S5,HZ>S4", "0>S4,S5", "TR>S1", "0>S0", "1>S0",
     "0>S2", "ZNZ>S2", "0>S6", "1>S6", "0>S7", "1>S7", "K>FB", "K>FA",
     "", "", "", "", "", "", "GUV>GCD", "GR>GK",
     "GR>GF", "GR>GG", "GR>GU", "GR>GV", "K>GH", "GI>GR", "K>GB", "K>GA"};

static char hex[] = {
     '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static int        suppr_half_trap_lch;  /* Holds Machine check flag 03AA3 */
static int        start_sw_rst;         /* Reset from start switch 03CA3 */
static int        e_cy_stop_sample;     /* Cycle Stop sample flag 03CB3 */
static int        clock_stop;           /* Indicate clock to stop 03CB4 */
static int        clock_rst;            /* Reset clock start 03CB4 */
static int        set_ic_allowed;       /* Set IC switch pressed 03CC3 */
static int        set_ic_start;         /* Start of set IC */
static int        cf_stop;
static int        stop_req;
static int        process_stop;
static int        read_call;
static int        proc_stop_loop_active;
static int        protect_loc_cpu_or_mpx;
static int        interrupt;
static int        any_mach_chk;
static int        chk_restart;
static int        priority;
static int        priority_bus;
static int        priority_stack_reg;
static int        priority_lch;
static int        any_priority_lch;
static int        any_priority_pulse;
static int        force_ij_req;
static int        hard_stop;
static int        second_err_stop;
static int        gate_sw_to_wx;
static int        allow_a_reg_chk;
static int        first_mach_chk_req;
static int        suppr_a_reg_chk;
static int        mach_chk_pulse;
static int        stg_prot_req;
static int        inh_stg_prot;
static int        mem_wrap_req;
static int        i_wrap_cpu;
static int        u_wrap_cpu;
static int        u_wrap_mpx;
static int        wrap_buf;
static int        alu_chk;
static int        mpx_share_req;
static int        mpx_share_pulse;
static int        mpx_cmd_start;
static int        mpx_start_sel;
static int        mpx_supr_out_lch;
static int        chk_or_diag_stop_sw;
static int        even_parity;
static int        mem_prot;
static int        timer_update;
static int        tc;
static int        sel_start_sel;
static int        sel_ros_req;
static int        sel_chnl_chk;
static int        sel_chain_pulse;
static int        sel_share_req;
static int        sel_read_cycle[2];
static int        sel_write_cycle[2];
static int        sel_gr_full[2];
static int        sel_halt_io[2];
static int        sel_poll_ctrl[2];
static int        sel_cnt_rdy_not_zero[2];
static int        sel_cnt_rdy_zero[2];
static int        sel_diag_tag_ctrl[2];
static int        sel_diag_mode[2];
static int        sel_bus_out_ctrl[2];
static int        sel_chan_busy[2];
static int        sel_intrp_lch[2];
static int        sel_status_stop_cond[2];
static int        sel_chain_req[2];
static int        sel_chain_det[2];

static int        cg_mask[4] = { 0x00, 0x0f, 0xf0, 0xff };

/* MATCH_SW

   0     PROCESS  MN
   1     SAR DELAYED MN
   2     SAR STOP  MN
   3     SAR RESTART MN
   4     ROAR RESTART STORE BYPASS  WX
   5     ROAR RESTART  WX
   6     ROAR RESTART WITHOUT RESET WX
   7     EARLY ROAR STOP  WX
   8     ROAR STOP  WX
   9     ROAR SYNC  WX
  */

/* CHK_SW

   0     DIAGNOSTIC
   1     DISABLE
   2     PROCESS
   3     STOP
   4     RESTART
 */

/* RATE_SW

   0     INSTRUCTION STEP
   1     PROCESS
   2     SINGLE CYCLE
*/

/* PROC_SW

   0     INHBIT CF STOP
   1     PROCESS
   2     ROS SCAN
*/

static char  dis_buffer[1024];

void
cycle_2030()
{
   uint16_t          nextWX = cpu_2030.WX;
   struct ROS_2030  *sal;
   int               dec;
   int               carry_in;
   uint16_t          abus_f;
   uint16_t          bbus_f;
   static uint16_t   carries;
   int               i;
   struct _device   *dev;
   uint16_t          h_backup;

   sal = &ros_2030[nextWX];
   cpu_2030.ros_row1 = sal->row1;
   cpu_2030.ros_row2 = sal->row2;
   cpu_2030.ros_row3 = sal->row3;
   priority = 0;
   chk_or_diag_stop_sw = (CHK_SW == 3);
   /* See if address matches selected switches */
   if (MATCH_SW > 3) {
      match = cpu_2030.WX == ((B_SW << 8) | (C_SW << 4) | (D_SW));
   } else if (MATCH_SW != 0 && (store == MAIN)) {
      match = cpu_2030.MN_REG == ((A_SW << 12) | (B_SW << 8) | (C_SW << 4) | (D_SW));
   } else if (MATCH_SW == 0) {
      match = cpu_2030.MN_REG == ((A_SW << 12) | (B_SW << 8) | (C_SW << 4) | (D_SW));
   }

   test_mode = (MATCH_SW != 0) || (CHK_SW != 2) || (PROC_SW != 1) || (RATE_SW != 1) ||
                   even_parity | alu_chk;

   /* If SAR_DELAY_SW and Match or Rate Instruction step */
   if (((MATCH_SW == 1) && match) || (RATE_SW == 2)) {
      process_stop = 1;
   }

   /* Clear match on SYNC or process */
   if ((MATCH_SW == 9) || (MATCH_SW == 0))
      match = 0;

   if (proc_stop_loop_active)
      priority_lch = 0;

   /* Check Restart latch */
   if (CHK_SW == 4 && any_mach_chk)
       chk_restart = 1;
   else if (any_priority_lch || SYS_RST)
       chk_restart = 0;

   /* SAR Restart SW */
   if ((MATCH_SW == 3 && match && !allow_write) ||
       ((MATCH_SW == 4 || MATCH_SW == 5 || MATCH_SW == 6) && chk_restart) ||
       set_ic_allowed) {
       force_ij_req = 1;
       cf_stop = 0;
   }

   if ((match && (MATCH_SW == 4)) ||
       (match && allow_write && (MATCH_SW == 5 || MATCH_SW == 6)))
      gate_sw_to_wx = 1;

   if ((((cpu_2030.FT & BIT3) != 0 || sel_ros_req) && proc_stop_loop_active) ||
        set_ic_start) {
      log_trace("CY start %d\n", allow_man_operation);
      e_cy_stop_sample = 1;
   }

   proc_stop_loop_active = 0;

   /* Handle front panel switches */
   if (CHECK_RST) {
      suppr_half_trap_lch = 0;
      first_mach_chk_req = 0;
      cpu_2030.MC_REG = 0;
      any_mach_chk = 0;
      CHECK_RST = 0;
   }

   if (INTR) {
       cpu_2030.F_REG |= BIT1;
       INTR = 0;
       log_trace("Set interrupt\n");
   }

   if (START) {
      if (allow_man_operation) {
          log_trace("Start\n");
          start_sw_rst = 1;
          process_stop = 0;
          suppr_half_trap_lch = 0;
          cf_stop = 0;
          e_cy_stop_sample = 1;
          hard_stop = 0;
          match = 0;
      }
      START = 0;
   }

   if (STOP) {
      process_stop = 1;
      STOP = 0;
   }

   if (LOAD) {
      log_trace("Load\n");
      cpu_2030.FT |= BIT4;  /* Set load flag. */
      load_mode = 1;
      cf_stop = 0;
      allow_man_operation = 0;
      suppr_half_trap_lch = 0;
      priority_lch = 0;
      even_parity = 0;
      alu_chk = 0;
      SYS_RST = 1;
   }

   if (SET_IC) {
      if (allow_man_operation) {
         log_trace("Set IC %d\n", allow_man_operation);
         set_ic_allowed = 1;
      }
      SET_IC = 0;
   }

   if (ROAR_RST) {
       log_trace("Set Roar %d\n", allow_man_operation);
       if (allow_man_operation) {
          gate_sw_to_wx = 1;
          priority_stack_reg = 0;
       }
       ROAR_RST = 0;
   }

   if (SYS_RST) {
      log_trace("System Reset\n");
      hard_stop = 0;
      force_ij_req = 0;
      gate_sw_to_wx = 0;
      clock_start_lch = 1;
      second_err_stop = 0;
      first_mach_chk_req = 0;
      cf_stop = 0;
      clock_stop = 0;
      suppr_a_reg_chk = 1;
      priority_stack_reg = 0;
      priority_lch = 1;
      cpu_2030.WX = 0;
      cpu_2030.H_REG = 0;
      cpu_2030.S_REG = 0;
      cpu_2030.MC_REG = 0;
      cpu_2030.C_REG = 0;
      cpu_2030.I_REG = 0x100;
      cpu_2030.J_REG = 0x100;
      cpu_2030.U_REG = 0x100;
      cpu_2030.V_REG = 0x100;
      cpu_2030.T_REG = 0x100;
      cpu_2030.G_REG = 0x100;
      cpu_2030.L_REG = 0x100;
      cpu_2030.D_REG = 0x100;
      allow_man_operation = !LOAD;
      e_cy_stop_sample = 1;
      suppr_half_trap_lch = 0;
      allow_write = 0;
      read_call = 0;
      even_parity = 0;
      inh_stg_prot = 0;
      alu_chk = 0;
      cpu_2030.ASCII = 0;
      SYS_RST = 0;
      LOAD = 0;
      /* Set memory parity to valid */
      for (i = 0; i < mem_max; i++)
          M[i] = odd_parity[M[i]&0xff] | (M[i]&0xff);
      for (i = 0; i < 2048; i++)
          cpu_2030.LS[i] = odd_parity[cpu_2030.LS[i]&0xff] | (cpu_2030.LS[i]&0xff);
      /* Reset selector channels */
      for (i = 0; i < 2; i++) {
         cpu_2030.SEL_TAGS[i] = 0;
         cpu_2030.SEL_TI[i] = 0;  /* Clear tags */
         for (dev = chan[i+1]; dev != NULL; dev = dev->next) {
             dev->bus_func(dev, &cpu_2030.SEL_TI[i], cpu_2030.GO[i], &cpu_2030.GI[i]);
         }
      }
   }

   /* Process Display and Store switches */
   if (allow_man_operation) {
       if (DISPLAY) {
           cpu_2030.Abus = 0;
           switch(E_SW) {
           case 0x10:  cpu_2030.Abus = cpu_2030.Q_REG;  /* Q */  break;
           case 0x11:  cpu_2030.Abus = cpu_2030.C_REG;  /* C */  break;
           case 0x12:  cpu_2030.Abus = cpu_2030.F_REG;  /* F */  break;
           case 0x13:  cpu_2030.Abus = cpu_2030.TT; /* TT */  break;
           case 0x14:  cpu_2030.Abus = cpu_2030.TI; /* TI */  break;
           case 0x15:  cpu_2030.Abus = cpu_2030.JI; /* JI */  break;
           case 0x16:  /* GS Virtual register */
                       cpu_2030.Abus = 0;
                       /* GR Full */
                       if (sel_gr_full[0])
                           cpu_2030.Abus |= BIT0;
                       /* Chain detect. */
                       if (sel_chain_det[0])
                           cpu_2030.Abus |= BIT1;
                       /* Interrupt condition */
                       if ((cpu_2030.SEL_TAGS[0] & CHAN_ADR_OUT) != 0)
                           cpu_2030.Abus |= BIT3;
                       /* CD */
                       if ((cpu_2030.GF[0] & BIT0) != 0)
                           cpu_2030.Abus |= BIT4;
                       /* Channel select. */
                       /* Chain Request */
                       if (sel_chain_req[0] != 0)
                           cpu_2030.Abus |= BIT7;
                       cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                       allow_a_reg_chk = 1;
                       break;
           case 0x17:
                       /* GT Virtual register */
                       cpu_2030.Abus = 0;
                       /* Select in */
                       if ((cpu_2030.SEL_TI[0] & CHAN_SEL_IN) != 0)
                           cpu_2030.Abus |= BIT0;
                       /* Service In & Not Service Out */
                       if ((cpu_2030.SEL_TI[0] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_IN))
                           cpu_2030.Abus |= BIT1;
                       /* Poll control */
                       if (sel_poll_ctrl[0])
                           cpu_2030.Abus |= BIT2;
                       /* Channel Busy */
                       if (sel_chan_busy[0])
                           cpu_2030.Abus |= BIT3;
                       /* Address In */
                       if ((cpu_2030.SEL_TI[0] & CHAN_ADR_IN) != 0)
                           cpu_2030.Abus |= BIT4;
                       /* Status In */
                       if ((cpu_2030.SEL_TI[0] & CHAN_STA_IN) != 0)
                           cpu_2030.Abus |= BIT5;
                       /* SX Interrupt Latch */
                       if (sel_intrp_lch[0])
                           cpu_2030.Abus |= BIT6;
                       /* Oper In */
                       if ((cpu_2030.SEL_TI[0] & CHAN_OPR_IN) != 0)
                           cpu_2030.Abus |= BIT7;
                       cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                       break;
           case 0x18:   /* GUV */
                       cpu_2030.M_REG = cpu_2030.GU[0];
                       cpu_2030.N_REG = cpu_2030.GV[0];
                       cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) |
                                                        (cpu_2030.N_REG & 0xff);
                       break;
           case 0x19:  /* HS Virtual register */
                       cpu_2030.Abus = 0;
                       /* GR Full */
                       if (sel_gr_full[1])
                           cpu_2030.Abus |= BIT0;
                       /* Chain detect. */
                       if (sel_chain_det[1])
                           cpu_2030.Abus |= BIT1;
                       /* Interrupt condition */
                       if ((cpu_2030.SEL_TAGS[1] & CHAN_ADR_OUT) != 0)
                           cpu_2030.Abus |= BIT3;
                       /* CD */
                       if ((cpu_2030.GF[1] & BIT0) != 0)
                           cpu_2030.Abus |= BIT4;
                       /* Channel select. */
                       cpu_2030.Abus |= BIT5;
                       /* Chain Request */
                       if (sel_chain_req[1] != 0)
                           cpu_2030.Abus |= BIT7;
                       cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                       allow_a_reg_chk = 1;
                       break;
           case 0x1a:  /* HT Virtual register */
                       cpu_2030.Abus = 0;
                       /* Select in */
                       if ((cpu_2030.SEL_TI[1] & CHAN_SEL_IN) != 0)
                           cpu_2030.Abus |= BIT0;
                       /* Service In & Not Service Out */
                       if ((cpu_2030.SEL_TI[1] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_IN))
                           cpu_2030.Abus |= BIT1;
                       /* Poll control */
                       if (sel_poll_ctrl[1])
                           cpu_2030.Abus |= BIT2;
                       /* Channel Busy */
                       if (sel_chan_busy[1])
                           cpu_2030.Abus |= BIT3;
                       /* Address In */
                       if ((cpu_2030.SEL_TI[1] & CHAN_ADR_IN) != 0)
                           cpu_2030.Abus |= BIT4;
                       /* Status In */
                       if ((cpu_2030.SEL_TI[1] & CHAN_STA_IN) != 0)
                           cpu_2030.Abus |= BIT5;
                       /* SX Interrupt Latch */
                       if (sel_intrp_lch[1])
                           cpu_2030.Abus |= BIT6;
                       /* Oper In */
                       if ((cpu_2030.SEL_TI[1] & CHAN_OPR_IN) != 0)
                           cpu_2030.Abus |= BIT7;
                       cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                       break;
           case 0x1b:   /* HUV */
                       cpu_2030.M_REG = cpu_2030.GU[1];
                       cpu_2030.N_REG = cpu_2030.GV[1];
                       cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) |
                                                        (cpu_2030.N_REG & 0xff);
                       break;
           case 0x20:
           case 0x21:
                   if (allow_write)
                       break;
                   cpu_2030.M_REG = (A_SW << 4) | (B_SW);
                   cpu_2030.M_REG |= odd_parity[cpu_2030.M_REG & 0xff];
                   cpu_2030.M_REG = (C_SW << 4) | (D_SW);
                   cpu_2030.N_REG |= odd_parity[cpu_2030.N_REG & 0xff];
                   cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) |
                                                 (cpu_2030.N_REG & 0xff);
                   if (E_SW == 0x20) {
                       cpu_2030.R_REG = M[cpu_2030.MN_REG] ^ 0x100;
                       store = MAIN;
                   }
                   if (E_SW == 0x21) {
                       cpu_2030.R_REG = cpu_2030.LS[((cpu_2030.M_REG << 5) & 0x700) |
                                        (cpu_2030.N_REG & 0xff)] ^ 0x100;
                       store = LOCAL;
                   }
                   break;
           case 0x30:  cpu_2030.Abus = cpu_2030.I_REG;  /* I */
                    if (allow_write == 0) {
                       cpu_2030.M_REG = cpu_2030.I_REG;
                       cpu_2030.N_REG = cpu_2030.J_REG;
                    }
                    break;
           case 0x31:  cpu_2030.Abus = cpu_2030.J_REG;         /* J */
                    if (allow_write == 0) {
                       cpu_2030.M_REG = cpu_2030.I_REG;
                       cpu_2030.N_REG = cpu_2030.J_REG;
                    }
                    break;
           case 0x32:  cpu_2030.Abus = cpu_2030.U_REG;  /* U */
                    if (allow_write == 0) {
                       cpu_2030.M_REG = cpu_2030.U_REG;
                       cpu_2030.N_REG = cpu_2030.V_REG;
                    }
                    break;
           case 0x33:  cpu_2030.Abus = cpu_2030.V_REG;         /* V */
                    if (allow_write == 0) {
                       cpu_2030.M_REG = cpu_2030.U_REG;
                       cpu_2030.N_REG = cpu_2030.V_REG;
                    }
                    break;
           case 0x34:  cpu_2030.Abus = cpu_2030.L_REG;            /* L */  break;
           case 0x35:  cpu_2030.Abus = cpu_2030.T_REG;            /* T */  break;
           case 0x36:  cpu_2030.Abus = cpu_2030.D_REG;            /* D */  break;
           case 0x37:  cpu_2030.Abus = cpu_2030.R_REG;            /* R */  break;
           case 0x38:  cpu_2030.Abus = cpu_2030.S_REG;            /* S */  break;
           case 0x39:  cpu_2030.Abus = cpu_2030.G_REG;            /* G */  break;
           case 0x3a:  cpu_2030.Abus = cpu_2030.H_REG;            /* H */  break;
           case 0x3b:  cpu_2030.Abus = cpu_2030.FI;               /* FI */  break;
           case 0x3c:  cpu_2030.Abus = cpu_2030.FT;               /* FT */  break;
           }

           DISPLAY = 0;
       }
       if (STORE) {
           cpu_2030.Bbus = (H_SW << 4) | (J_SW);
           cpu_2030.Bbus |= odd_parity[cpu_2030.Bbus];
           cpu_2030.Alu_out = cpu_2030.Bbus;
           cpu_2030.Abus = cpu_2030.Bbus;
           switch(E_SW) {
           case 0x10:  cpu_2030.Q_REG = cpu_2030.Alu_out; /* Q */  break;
           case 0x20:
           case 0x21:
                   if (allow_write)
                       break;
                   cpu_2030.R_REG = cpu_2030.Alu_out;
                   cpu_2030.M_REG = (A_SW << 4) | (B_SW);
                   cpu_2030.M_REG |= odd_parity[cpu_2030.M_REG];
                   cpu_2030.M_REG = (C_SW << 4) | (D_SW);
                   cpu_2030.N_REG |= odd_parity[cpu_2030.N_REG];
                   cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) | (cpu_2030.N_REG & 0xff);
                   if (E_SW == 0x20) {
                       M[cpu_2030.MN_REG] = cpu_2030.R_REG ^ 0x100;
                       store = MAIN;
                   }
                   if (E_SW == 0x21) {
                       cpu_2030.LS[((cpu_2030.M_REG << 5) & 0x700) | (cpu_2030.N_REG & 0xff)] = cpu_2030.R_REG ^ 0x100;
                       store = LOCAL;
                   }
                   cpu_2030.Abus = cpu_2030.R_REG;
                   break;
           case 0x30:  cpu_2030.I_REG = cpu_2030.Alu_out;  /* I */  break;
           case 0x31:  cpu_2030.J_REG = cpu_2030.Alu_out;       /* J */  break;
           case 0x32:  cpu_2030.U_REG = cpu_2030.Alu_out;  /* U */  break;
           case 0x33:  cpu_2030.V_REG = cpu_2030.Alu_out;       /* V */  break;
           case 0x34:  cpu_2030.L_REG = cpu_2030.Alu_out;                /* L */  break;
           case 0x35:  cpu_2030.T_REG = cpu_2030.Alu_out;                /* T */  break;
           case 0x36:  cpu_2030.D_REG = cpu_2030.Alu_out;                /* D */  break;
           case 0x37:  cpu_2030.R_REG = cpu_2030.Alu_out;                /* R */  break;
           case 0x38:  cpu_2030.S_REG = cpu_2030.Alu_out;                /* S */  break;
           case 0x39:  cpu_2030.G_REG = cpu_2030.Alu_out;                /* G */  break;
           case 0x3a: cpu_2030.H_REG = cpu_2030.Alu_out;                /* H */  break;
           default:
                    break;
           }
           STORE = 0;
       }
   }

   if (set_ic_allowed || start_sw_rst) {
       process_stop = 0;
   }

   interrupt = 0;
   /* Handle interval timer */
   if (timer_event) {
       timer_event = 0; /* Mark we have done it */
       if (INT_TMR) {
           if (cpu_2030.C_REG != 0xf)
               cpu_2030.C_REG ++;
           timer_update = 1;
       }
   }
   if ((cpu_2030.MASK & BIT7) && cpu_2030.F_REG != 0) {
       interrupt = 1;
   }
   if ((cpu_2030.MASK & BIT0) != 0 && (cpu_2030.FT & BIT7) != 0) {
       interrupt = 1;
   }
   if ((cpu_2030.MASK & BIT1) && (sel_intrp_lch[0] || (cpu_2030.GF[0] & BIT4) != 0)) {
       interrupt = 1;
   }
   if ((cpu_2030.MASK & BIT2) && (sel_intrp_lch[1] || (cpu_2030.GF[1] & BIT4) != 0)) {
       interrupt = 1;
   }
   stop_req = !(process_stop & (!interrupt) & end_of_e_cycle);

   clock_rst = hard_stop | ((sal->CA != 0xE) & cf_stop);
   clock_stop = (proc_stop_loop_active & (!sel_ros_req) & (!mpx_share_req));
   if (clock_stop || clock_rst) {
      clock_start_lch = 0;
      e_cy_stop_sample = 0;
      allow_man_operation = 1;
   }

   if (e_cy_stop_sample && allow_man_operation) {
      allow_man_operation = 0;
      set_ic_allowed = 0;
      force_ij_req = 0;
      clock_start_lch = 1;
   }

   set_ic_start = ((priority_stack_reg & BIT2) != 0) & set_ic_allowed;

   if (gate_sw_to_wx) {
      cpu_2030.WX = (G_SW << 8) | (H_SW << 4) | (J_SW);
      priority_lch = 0;
      gate_sw_to_wx = 0;
      match = 0;
   }

   if (CHK_SW == 0 || (hard_stop == 0 && any_priority_lch)) {
      priority_lch = 1;
   }

   if (mach_chk_pulse)
      second_err_stop = 1;

   priority_bus = 0;
   mach_chk_pulse = 0;
   if (!priority_lch && read_call == 0 && allow_write == 0) {
       if (!suppr_half_trap_lch && !gate_sw_to_wx && (priority_stack_reg & BIT0) != 0) {
           priority_bus = BIT5;   /* Mach_chk_pulse */
           mach_chk_pulse = 1;
           suppr_a_reg_chk = 1;
           priority_stack_reg &= ~BIT0;
       } else if ((priority_stack_reg & BIT1) != 0 && (cpu_2030.H_REG & BIT0) == 0) {
           priority_bus = BIT6;   /* IPL Pulse */
           priority_stack_reg &= ~BIT1;
       } else if ((priority_stack_reg & BIT2) != 0 && (cpu_2030.H_REG & BIT4) == 0) {
           priority_bus = BIT7;   /* force_ij_pulse */
           force_ij_req = 0;
           priority_stack_reg &= ~BIT2;
       } else if ((priority_stack_reg & BIT3) != 0 && (cpu_2030.H_REG & BIT2) == 0) {
           priority_bus = BIT2;   /* wrap_pulse */
           priority_stack_reg &= ~BIT3;
       } else if ((priority_stack_reg & BIT4) != 0 && (cpu_2030.H_REG & BIT3) == 0) {
           priority_bus = BIT1;   /* protect_pulse */
           priority_stack_reg &= ~BIT4;
       } else if ((priority_stack_reg & BIT5) != 0) {
           priority_bus = BIT0;   /* Stop request */
           priority_stack_reg &= ~BIT5;
       } else if ((priority_stack_reg & BIT6) != 0 && (cpu_2030.H_REG & BIT5) == 0) {
           priority_bus = BIT4;   /* sx_chain_pulse */
           priority_stack_reg &= ~BIT6;
           sel_chain_pulse = 1;
       } else if ((priority_stack_reg & BIT7) != 0 && (cpu_2030.H_REG & (BIT5|BIT6)) == 0) {
           priority_bus = BIT3;   /* mpx_share_pulse */
           priority_stack_reg &= ~BIT7;
           mpx_share_pulse = 1;
       }
    }
    any_priority_pulse = (priority_bus != 0);

    /* Check if we should run a select memory cycle or not */
    if (sel_share_req != 0 && allow_write == 0 && read_call == 0) {
       if (sel_share_req & 1)
          i = 0;
       else
          i = 1;
       /* Read memory from previous request */
       if (sel_read_cycle[i]) {
           /* Copy GUV to MN */
           cpu_2030.M_REG = cpu_2030.GU[i];
           cpu_2030.N_REG = cpu_2030.GV[i];
           /* Count -1 to GZY */
           cpu_2030.GHZ = (cpu_2030.GD[i] - 1) & 0xff;
           cpu_2030.GHY = (cpu_2030.GC[i] & 0xff);
           if (cpu_2030.GHZ == 0xff)
               cpu_2030.GHY--;
           /* Clear flag if count reached zero */
           if (cpu_2030.GHZ == 0 && cpu_2030.GHY == 0) {
               sel_cnt_rdy_not_zero[i] = 0;
           }
           cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) | (cpu_2030.N_REG & 0xff);
           /* Check if in range of memory */
           if (((0xFFFF ^ mem_max) & cpu_2030.MN_REG) != 0) {
               cpu_2030.GE[i] |= BIT2;
               log_trace("Set prog check\n");
           }

           /* Check validity of M and N registers */
           if (((odd_parity[cpu_2030.M_REG & 0xff] ^ cpu_2030.M_REG) & 0x100) != 0 ||
                 ((odd_parity[cpu_2030.N_REG & 0xff] ^ cpu_2030.N_REG) & 0x100) != 0) {
                 cpu_2030.MC_REG |= MNREG;
           }

           /* Copy GUV to GCD, and New Count in GZY to GUV */
           cpu_2030.GC[i] = cpu_2030.GU[i];
           cpu_2030.GD[i] = cpu_2030.GV[i];
           cpu_2030.GV[i] = cpu_2030.GHZ| odd_parity[cpu_2030.GHZ];
           cpu_2030.GU[i] = cpu_2030.GHY| odd_parity[cpu_2030.GHY];
           /* If output and GR empty */
           if (sel_cnt_rdy_zero[i] == 0 && (cpu_2030.GG[i] & 1) == 1 && sel_gr_full[i] == 0) {
               cpu_2030.GR[i] = M[cpu_2030.MN_REG];
               sel_gr_full[i] = 1;
               log_mem("Read main sel%d %04x %03x\n", i, cpu_2030.MN_REG, cpu_2030.GR[i]);
           }
           /* Update Q with selector memory protection */
           cpu_2030.Q_REG &= 0xf0;
           cpu_2030.Q_REG |= cpu_2030.MP[(0xE0) | (cpu_2030.M_REG >> 3)] & 0xf;
           sel_write_cycle[i] = 1;
           sel_read_cycle[i] = 0;
       } else if (sel_write_cycle[i]) {
           /* If moving backwards, address - 1 */
           if (cpu_2030.GG[cpu_2030.ch_sel] == 0x10c) {
               cpu_2030.GHZ = (cpu_2030.GD[i] - 1) & 0xff;
               cpu_2030.GHY = (cpu_2030.GC[i] & 0xff);
               if (cpu_2030.GHZ == 0xff)
                   cpu_2030.GHY--;
           } else {
               /* Else address +1 */
               cpu_2030.GHZ = (cpu_2030.GD[i] + 1) & 0xff;
               cpu_2030.GHY = (cpu_2030.GC[i] & 0xff);
               if (cpu_2030.GHZ == 0x00)
                   cpu_2030.GHY++;
               if (cpu_2030.GHY & 0x100) {
                   cpu_2030.GE[i] |= BIT2;
                   log_trace("Set prog check\n");
               }
           }
           /* Count back to GCD and new address to GUV */
           cpu_2030.GC[i] = cpu_2030.GU[i];
           cpu_2030.GD[i] = cpu_2030.GV[i];
           cpu_2030.GV[i] = cpu_2030.GHZ| odd_parity[cpu_2030.GHZ];
           cpu_2030.GU[i] = cpu_2030.GHY| odd_parity[cpu_2030.GHY];
           sel_write_cycle[i] = 0;

           /* Check if input */
           if (sel_gr_full[i] != 0 && ((cpu_2030.GG[i] & 3) == 2 || (cpu_2030.GG[i] & 5) == 4)) {
               if ((cpu_2030.Q_REG & 0xf0) != 0 &&
                         ((cpu_2030.GK[i] ^ cpu_2030.Q_REG) & 0xf) != 0) {
                   cpu_2030.GE[i] |= BIT3;
                   cpu_2030.GR[i] = M[cpu_2030.MN_REG];
                   log_mem("Read main sel%d %04x %03x\n", i, cpu_2030.MN_REG, cpu_2030.GR[i]);
               }
               /* Check skip flag */
               if ((cpu_2030.GF[i] & BIT3) == 0) {
                   M[cpu_2030.MN_REG] = cpu_2030.GR[i];
                   log_mem("Read write sel%d %04x %03x\n", i, cpu_2030.MN_REG, cpu_2030.GR[i]);
               }
               sel_gr_full[i] = 0;
               /* If Service In and count left Acknowledge */
               if (((cpu_2030.SEL_TI[i] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_IN)) &&
                    sel_cnt_rdy_zero[i] == 0)
                   cpu_2030.SEL_TAGS[i] |= CHAN_SRV_OUT;
           }

           /* If count 0 update other register */
           if (sel_cnt_rdy_not_zero[i] == 0 && sel_gr_full[i] == 0)
               sel_cnt_rdy_zero[i] = 1;

           /* Check validity of GR register */
           if (((odd_parity[cpu_2030.GR[i]& 0xff] ^ cpu_2030.GR[i]) & 0x100) != 0) {
               cpu_2030.GE[i] |= BIT4;
           }

           sel_share_req &= ~(1<< i);
       }

    } else {
        h_backup = cpu_2030.H_REG;
        /* If we have interrupt, transfer to it */
        if (any_priority_pulse) {
           priority_lch = 1;
           clock_start_lch = 1;
           if (mpx_share_pulse) {
              cpu_2030.FWX = nextWX;
              cpu_2030.MPX_STAT = cpu_2030.STAT_REG;
              if (t_request == 0) {
                 cpu_2030.MPX_TAGS |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
                 cpu_2030.FT |= BIT6;
              }
              mpx_share_pulse = 0;
           }

           if (sel_chain_pulse) {
              cpu_2030.GWX = nextWX;
              cpu_2030.SEL_STAT = cpu_2030.STAT_REG;
              cpu_2030.ch_sav = cpu_2030.ch_sel;
              if (sel_ros_req & 1) {
                 cpu_2030.ch_sel = 0;
                 sel_ros_req &= 2;
                 if (sel_chain_req[0])
                     priority_bus |= 3;
              } else {
                 cpu_2030.ch_sel = 1;
                 sel_ros_req &= 1;
                 if (sel_chain_req[1])
                     priority_bus |= 3;
              }
              sel_chain_pulse = 0;
           }
           cpu_2030.WX = priority_bus;
           goto chan_scan;
        }
        /* Otherwise see if CPU clock is running */
        if (clock_start_lch) {
           sal = &ros_2030[cpu_2030.WX];
           cpu_2030.ros_row1 = sal->row1;
           cpu_2030.ros_row2 = sal->row2;
           cpu_2030.ros_row3 = sal->row3;

          /* Print instruction and registers */
          if (cpu_2030.WX == 0x109 && (log_level & LOG_ITRACE) != 0) {
             uint8_t    mem[6];

             for (i = 0; i < 6; i++) {
                 mem[i] = M[cpu_2030.MN_REG + i] & 0xff;
             }
             print_inst(mem);
             log_itrace_c(" IC=%02x%02x CC=%02x MSK=%02x AMWP=%x MC=%02x", cpu_2030.I_REG & 0xff,
                      cpu_2030.J_REG & 0xff, cpu_2030.LS[0x7BB], cpu_2030.MASK,
                      cpu_2030.LS[0x7b9] & 0x0f, cpu_2030.MC_REG);
             log_itrace("\n");

             log_itrace_s(" ");
             for (i = 0; i < 16; i++) {
                 log_itrace_c(" GR%02d = %02x%02x%02x%02x", i,
                      cpu_2030.LS[(i << 4) + 0 + 0x700] & 0xff,
                      cpu_2030.LS[(i << 4) + 1 + 0x700] & 0xff,
                      cpu_2030.LS[(i << 4) + 2 + 0x700] & 0xff,
                      cpu_2030.LS[(i << 4) + 3 + 0x700] & 0xff);
                 if ((i & 0x3) == 0x3) {
                      log_itrace_s(" ");
                 }
             }
          }

          /* Disassemble micro instruction */
          if ((log_level & LOG_MICRO) != 0 && (cpu_2030.WX != 0xAE)) {
              sprintf (dis_buffer, "%s %03X: %02x ", sal->note, cpu_2030.WX, sal->CK);
              if (sal->CK < 0x10) {
                  if (sal->PK != 0 || sal->CB == 3 || sal->CU == 2) {
                      char buf[3];
                      strcat(dis_buffer, ckb_name[sal->CK]);
                      buf[0] = ',';
                      buf[1] = '0'+sal->PK;
                      buf[2] = '\0';
                      strcat(dis_buffer, buf);
                  }
              } else {
                  strcat(dis_buffer, ck_name[sal->CK]);
                  if (sal->PK != 0)
                     strcat(dis_buffer, ",1");
              }
              if (sal->CF == 4) {
                  strcat(dis_buffer, " STP");
              } else if (sal->CF == 0 && sal->CA == 0) {
                  strcat(dis_buffer, " 0");
              } else {
                  strcat(dis_buffer, " ");
                  strcat(dis_buffer, ca_name[sal->CA]);
                  strcat(dis_buffer, cf_name[sal->CF]);
              }

              if (sal->CG == 0 && sal->CV == 0 && sal->CC == 0) {
              } else {
                  switch (sal->CC) {
                  case 0:
                  case 1:
                  case 4:
                  case 5:
                  case 6:  strcat(dis_buffer, "+"); break;
                  case 2:  strcat(dis_buffer, "&"); break;
                  case 3:  strcat(dis_buffer, "|"); break;
                  case 7:  strcat(dis_buffer, "^"); break;
                  }
                  if (sal->CV == 1)
                      strcat(dis_buffer, "-");
                  if (sal->CG == 0 && sal->CB == 0) {
                     strcat(dis_buffer, "0");
                  } else {
                     strcat(dis_buffer, cb_name[sal->CB]);
                     if (sal->CG == 0)
                        strcat(dis_buffer, "0");
                  }

              }
              if (sal->CG != 0)
                 strcat(dis_buffer, cg_name[sal->CG]);
              switch (sal->CC) {
              case 5:
              case 1:  strcat(dis_buffer, "+1");  break;
              case 6:  strcat(dis_buffer, "+C"); break;
              }
              strcat(dis_buffer, ">");
              strcat(dis_buffer, cd_name[sal->CD]);
              if (sal->CC >= 4 && sal->CC < 7)
                 strcat(dis_buffer, "C");

              if (sal->CV > 1)
                 strcat(dis_buffer, (sal->CV & 1) ? " DEC": " BIN");

              if (sal->CS != 0) {
                  strcat(dis_buffer, " ");
                  strcat(dis_buffer, cs_name[sal->CS]);
              }
              strcat(dis_buffer, "  ");
              if (sal->CM < 3 && sal->CU == 2) {
                  char buf[10];
                  strcat(dis_buffer, cm_name[sal->CM]);
                  buf[0] = '(';
                  buf[1] = hex[sal->CK & 0xf];
                  buf[2] = '>';
                  buf[3] = 'W';
                  buf[4] = ')';
                  buf[5] = ' ';
                  buf[6] = hex[(sal->CN >> 4) & 0xf];
                  buf[7] = hex[sal->CN & 0xf];
                  buf[8] = ' ';
                  buf[9] = '\0';;
                  strcat(dis_buffer, buf);
                  strcat(dis_buffer, ch_name[sal->CH]);
                  strcat(dis_buffer, " ");
                  strcat(dis_buffer, cl_name[sal->CL]);
              } else if (sal->CM == 6) {
                  char buf[10];
                  int val =  0x88 | ((sal->CN & 0x80) >> 2) | ((sal->CK & 0x8) << 1) | (sal->CK & 0x7);
                  buf[0] = hex[(val >> 4) & 0xf];
                  buf[1] = hex[val & 0xf];
                  buf[2] = '(';
                  buf[3] = '\0';
                  strcat(dis_buffer, buf);
                  strcat(dis_buffer, cu1_name[sal->CU]);
                  buf[0] = ')';
                  buf[1] = ' ';
                  buf[2] = hex[(sal->CN >> 4) & 0xf];
                  buf[3] = hex[sal->CN & 0xf];
                  buf[4] = ' ';
                  buf[5] = '\0';;
                  strcat(dis_buffer, buf);
                  strcat(dis_buffer, ch_name[sal->CH]);
                  strcat(dis_buffer, " ");
                  strcat(dis_buffer, cl_name[sal->CL]);
              } else {
                  char buf[10];
                  strcat(dis_buffer, cm_name[sal->CM]);
                  strcat(dis_buffer, "(");
                  strcat(dis_buffer, (sal->CM < 3) ? cu2_name[sal->CU]: cu1_name[sal->CU]);
                  buf[0] = ' ';
                  buf[1] = hex[(sal->CN >> 4) & 0xf];
                  buf[2] = hex[sal->CN & 0xf];
                  buf[3] = ' ';
                  buf[4] = '\0';;
                  strcat(dis_buffer, buf);
                  strcat(dis_buffer, ch_name[sal->CH]);
                  strcat(dis_buffer, " ");
                  strcat(dis_buffer, cl_name[sal->CL]);
              }
              if ((cpu_2030.FT & BIT7) != 0)
                  strcat(dis_buffer, " SUP");
              strcat(dis_buffer, "\n");
              log_micro(dis_buffer);
           }

           /* Read memory from previous request */
           if (read_call) {

               protect_loc_cpu_or_mpx = 0;
               mem_prot = 0;
               stg_prot_req = 0;
               /* Check validity of M and N registers */
               if (((odd_parity[cpu_2030.M_REG & 0xff] ^ cpu_2030.M_REG) & 0x100) != 0 ||
                     ((odd_parity[cpu_2030.N_REG & 0xff] ^ cpu_2030.N_REG) & 0x100) != 0) {
                   cpu_2030.MC_REG |= BIT2;
                   mem_prot = 1;
               }

               /* Check if in range of memory */
               if (store == MAIN && ((0xFFFF ^ mem_max) & cpu_2030.MN_REG) != 0) {
                   mem_prot = 1;
               }
               if (store == MAIN && mem_prot == 0) {
                   cpu_2030.Q_REG &= 0xf0;
                   cpu_2030.Q_REG |= cpu_2030.MP[cpu_2030.SA_REG] & 0xf;
                   if (!inh_stg_prot) {
                       if ((cpu_2030.H_REG & BIT5) == 0 && (cpu_2030.Q_REG & 0xf0) != 0 &&
                                 (((cpu_2030.Q_REG >> 4) ^ cpu_2030.Q_REG) & 0xf) != 0) {
                           protect_loc_cpu_or_mpx = 1;
                           if (sal->CM == 2)
                              stg_prot_req = 1;
                           log_mem("Protect check\n");
                       }
                   }
               }
               if (store == MPX) {
                   cpu_2030.Q_REG &= 0xf0;
                   cpu_2030.Q_REG |= cpu_2030.MP[cpu_2030.SA_REG] & 0xf;
                   log_mem("read mpx %02x %x\n", cpu_2030.SA_REG, cpu_2030.Q_REG & 0xf);
               }
               if (sal->CM != 2 && mem_prot == 0) {
                   switch (store) {
                   case MAIN:
                        if (sal->CU == 1) {
                            cpu_2030.GR[cpu_2030.ch_sel] = M[cpu_2030.MN_REG];
                        } else {
                            cpu_2030.R_REG = M[cpu_2030.MN_REG];
                            log_mem("Read main %04x %03x %x %d\n", cpu_2030.MN_REG, cpu_2030.R_REG,
                                   cpu_2030.Q_REG & 0xf, inh_stg_prot);
                        }
                        M[cpu_2030.MN_REG] = 0x00;
                        break;
                   case MPX:
                   case LOCAL:
                        if (sal->CU == 1)
                            cpu_2030.GR[cpu_2030.ch_sel] = cpu_2030.LS[cpu_2030.MN_REG];
                        else {
                            cpu_2030.R_REG = cpu_2030.LS[cpu_2030.MN_REG];
                        }
                        cpu_2030.LS[cpu_2030.MN_REG] = 0x00;
                        break;
                   }
               }
               allow_write = 1;
               read_call = 0;
           }


           /* Compute next address */
           switch (sal->CM) {
           case 2:       /* Store */
                   if (stg_prot_req)
                       break;
                   /* Fall through */
           case 0:       /* Write */
                  /* Do write to memory from this request */
                  if (allow_write) {
                      switch (store) {
                      case MAIN:
                           if (sal->CU == 1) {
                               M[cpu_2030.MN_REG] = cpu_2030.GR[cpu_2030.ch_sel];
                           } else {
                               M[cpu_2030.MN_REG] = cpu_2030.R_REG;
                               log_mem("Write main %04x %03x\n", cpu_2030.MN_REG, cpu_2030.R_REG);
                           }
                           cpu_2030.MP[cpu_2030.SA_REG] = cpu_2030.Q_REG & 0x0f;
                           break;
                      case MPX:
                           log_mem("Write mpx %04x %03x %02x %x\n", cpu_2030.MN_REG, cpu_2030.R_REG,
                                    cpu_2030.SA_REG, cpu_2030.Q_REG & 0xf);
                           cpu_2030.MP[cpu_2030.SA_REG] = cpu_2030.Q_REG & 0xf;
                           /* Fall through */
                      case LOCAL:
                           if (sal->CU == 1)
                               cpu_2030.LS[(cpu_2030.MN_REG & 0x7ff)] = cpu_2030.GR[cpu_2030.ch_sel];
                           else
                               cpu_2030.LS[(cpu_2030.MN_REG & 0x7ff)] = cpu_2030.R_REG;
                           break;
                      }
                      allow_write = 0;
                      read_call = 0;
                      inh_stg_prot = 0;
                  }
                  break;
           case 1:          /* Compute */
                  break;
           case 3:          /* Read IJ */
                  cpu_2030.M_REG = cpu_2030.I_REG;
                  cpu_2030.N_REG = cpu_2030.J_REG;
                  inh_stg_prot = 0;
                  break;
           case 4:          /* Read UV */
                  cpu_2030.M_REG = cpu_2030.U_REG;
                  cpu_2030.N_REG = cpu_2030.V_REG;
                  inh_stg_prot = 0;
                  break;
           case 5:          /* Read T */
                  cpu_2030.M_REG = 0x100;
                  cpu_2030.N_REG = cpu_2030.T_REG;
                  inh_stg_prot = 1;
                  break;
           case 6:          /* Read CK */
                  cpu_2030.M_REG = 0x100;
                  cpu_2030.N_REG = 0x88 | ((sal->CN & 0x80) >> 2) | ((sal->CK & 0x8) << 1) |
                         (sal->CK & 0x7);
                  /* Use selector channel 2 */
                  if (cpu_2030.ch_sel && ((sal->CK & 0x1e) == 06 || sal->CK == 05))
                      cpu_2030.N_REG |= 0x10;
                  cpu_2030.N_REG |= odd_parity[cpu_2030.N_REG];
                  inh_stg_prot = 1;
                  break;
           case 7:
                  cpu_2030.M_REG = cpu_2030.GU[cpu_2030.ch_sel];
                  cpu_2030.N_REG = cpu_2030.GV[cpu_2030.ch_sel];
                  inh_stg_prot = 0;
                  break;
           }

           /* If load new address, generate SA and Main/MPX request */
           if (sal->CM >= 3) {
               store = MAIN;
               mem_wrap_req = 0;
               switch (sal->CU) {
               case 0:
                   /* Check if in range of memory */
                   if (((0xFFFF ^ mem_max) & cpu_2030.MN_REG) != 0) {
                        mem_wrap_req = 1;
                        log_trace("Memory wrap %04x\n", cpu_2030.MN_REG);
                   }
                   if (i_wrap_cpu && sal->CM == 3 && (cpu_2030.H_REG & BIT6) == 0) {
                        mem_wrap_req = 1;
                        log_trace("Memory wrap i wrap\n");
                   }
                   if (u_wrap_cpu && sal->CM == 4 && (cpu_2030.H_REG & BIT6) == 0) {
                        mem_wrap_req = 1;
                        log_trace("Memory wrap u wrap\n");
                   }
                   if (u_wrap_mpx && sal->CM == 4 && (cpu_2030.H_REG & BIT6) != 0) {
                        mem_wrap_req = 1;
                        log_trace("Memory wrap u wrap\n");
                   }
                   break;
               case 1:
                   store = LOCAL;
                   cpu_2030.M_REG = BIT0|BIT1|BIT2;
                   cpu_2030.M_REG |= odd_parity[cpu_2030.M_REG];
                   break;
               case 2:
                   store = MPX;
                   cpu_2030.M_REG = cpu_2030.XX_REG;
                   cpu_2030.M_REG |= odd_parity[cpu_2030.M_REG];
                   cpu_2030.SA_REG = (cpu_2030.XX_REG & 0xE0) | ((cpu_2030.N_REG >> 3) & 0x1f);
                   break;
               case 3:
                   if ((cpu_2030.G_REG & (BIT0|BIT1)) == 0) {
                       cpu_2030.M_REG = BIT0|BIT1|BIT2;
                       cpu_2030.M_REG |= odd_parity[cpu_2030.M_REG];
                       store = LOCAL;
                   }
               }
               if (!allow_write && !mem_wrap_req) {
                   read_call = 1;
               }
               if (store == MAIN) {
                   cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xff) << 8) | (cpu_2030.N_REG & 0xff);
                   cpu_2030.SA_REG = (0xE0) | ((cpu_2030.M_REG >> 3) & 0x1F);
               } else {
                   cpu_2030.MN_REG = ((cpu_2030.M_REG & 0xE0) << 3) | (cpu_2030.N_REG & 0xff);
               }
               /* Check validity of M and N registers */
               if (((odd_parity[cpu_2030.M_REG & 0xff] ^ cpu_2030.M_REG) & 0x100) != 0 ||
                     ((odd_parity[cpu_2030.N_REG & 0xff] ^ cpu_2030.N_REG) & 0x100) != 0) {
                   cpu_2030.MC_REG |= BIT2;
                   mem_prot = 1;
               }

           }

           /* Base next address. */
           nextWX = (cpu_2030.WX & 0xf00) | sal->CN;

           /* Decode the X6 bit */
           switch (sal->CH) {
           case 0:
                   break;
           case 1:
                   nextWX |= 0x2;
                   break;
           case 2:
                   if ((cpu_2030.R_REG & 0x80) != 0)
                       nextWX |= 0x2;
                   break;
           case 3:
                   if ((cpu_2030.V_REG & 0x3) == 0)
                       nextWX |= 0x2;
                   break;
           case 4: /* Status IN */
                   if ((cpu_2030.STAT_REG & BIT1) != 0)
                      nextWX |= 0x2;
                   break;
           case 5: /* If operation in MPX */
                   if ((cpu_2030.STAT_REG & BIT2) != 0)
                      nextWX |= 0x2;
                   break;
           case 6: /* If carry from previous */
                   if ((cpu_2030.STAT_REG & BIT5) != 0)
                      nextWX |= 0x2;
                   break;
           case 7:  /* S0 */
                   if ((cpu_2030.S_REG & BIT0) != 0)
                      nextWX |= 0x2;
                   break;
           case 8:  /* S1 */
                   if ((cpu_2030.S_REG & BIT1) != 0)
                      nextWX |= 0x2;
                   break;
           case 9:  /* S2 */
                   if ((cpu_2030.S_REG & BIT2) != 0)
                      nextWX |= 0x2;
                   break;
           case 10: /* S4 */
                   if ((cpu_2030.S_REG & BIT4) != 0)
                      nextWX |= 0x2;
                   break;
           case 11: /* S6 */
                   if ((cpu_2030.S_REG & BIT6) != 0)
                      nextWX |= 0x2;
                   break;
           case 12: /* G0 */
                   if ((cpu_2030.G_REG & BIT0) != 0)
                      nextWX |= 0x2;
                   break;
           case 13: /* G2 */
                   if ((cpu_2030.G_REG & BIT2) != 0)
                      nextWX |= 0x2;
                   break;
           case 14: /* G4 */
                   if ((cpu_2030.G_REG & BIT4) != 0)
                      nextWX |= 0x2;
                   break;
           case 15: /* G6 */
                   if ((cpu_2030.G_REG & BIT6) != 0)
                      nextWX |= 0x2;
                   break;
           }

           end_of_e_cycle = 0;
           /* Decode the X7 bit */
           switch (sal->CL) {
           case 0:
                   break;
           case 1:
                   nextWX |= 0x1;
                   break;
           case 2: /* CA to W */
                   nextWX = ((sal->CA & 0xF) << 8) | (nextWX & 0xff) | 1;
                   break;
           case 3:
                   /* If address in MPX */
                   if ((cpu_2030.STAT_REG & BIT0) != 0)
                      nextWX |= 0x1;
                   break;
           case 4:
                   /* If service in MPX */
                   if ((cpu_2030.STAT_REG & BIT3) != 0)
                      nextWX |= 0x1;
                   break;
           case 5: /* VDD */
                   nextWX |= !(((cpu_2030.R_REG | (cpu_2030.R_REG << 1)) & (cpu_2030.R_REG >> 1)) & 0x44);
                   break;
           case 6: /* IBC Carry from bit 1 to 0. */
                   if ((cpu_2030.STAT_REG & BIT6) != 0)
                      nextWX |= 0x1;
                   break;
           case 7: /* Z == 0 */
                   if ((cpu_2030.STAT_REG & BIT4) != 0)
                      nextWX |= 0x1;
                   break;
           case 8: /* G7 */
                   if ((cpu_2030.G_REG & BIT7) != 0)
                      nextWX |= 0x1;
                   break;
           case 9: /* S3 */
                   if ((cpu_2030.S_REG & BIT3) != 0)
                      nextWX |= 0x1;
                   break;
           case 10: /* S5 */
                   if ((cpu_2030.S_REG & BIT5) != 0)
                      nextWX |= 0x1;
                   break;
           case 11: /* S7 */
                   if ((cpu_2030.S_REG & BIT7) != 0)
                      nextWX |= 0x1;
                   break;
           case 12: /* G1 */
                   if ((cpu_2030.G_REG & BIT1) != 0)
                      nextWX |= 0x1;
                   break;
           case 13: /* G3 */
                   if ((cpu_2030.G_REG & BIT3) != 0)
                      nextWX |= 0x1;
                   break;
           case 14: /* G5 */
                   if ((cpu_2030.G_REG & BIT5) != 0)
                      nextWX |= 0x1;
                   break;
           case 15: /* INTR */
                   end_of_e_cycle = 1;
                   if (interrupt) {
                      nextWX |= 0x1;
                   }
                   break;
           }

           /* Handle alternate CK thst change branch address */
           switch(sal->CK) {
           case 0x11:  nextWX = ((cpu_2030.U_REG & 0xff) << 8) | (cpu_2030.V_REG & 0xff);
                       break;
           case 0x12: /* Reset wrap */
                       break;
           case 0x13: /* Wrap */
                       if (i_wrap_cpu)
                          nextWX &= 0xffd;
                       if (u_wrap_mpx)
                          nextWX &= 0xffe;
                       break;
           case 0x14: /* set switches HI to K */
                       break;
           case 0x15: /* If previous carry force X to 0 */
                       if ((cpu_2030.STAT_REG & BIT5) != 0)
                          nextWX &= 0xf00;
                       break;
           case 0x16: /* Reset 1050 home loop */
                       break;
           case 0x17: /* set 1050 home loop */
                       break;
           case 0x18: /* Set even parity */
                       break;
           case 0x19: /* Check ascii flag */
                       if (cpu_2030.ASCII)
                          nextWX &= 0xffd;  /* Clear X6 if set */
                       break;
           case 0x1A: /* Select interrupt SEL1 -> 0,x SEL2 -> x,0, timer -> 0,0 */
                       if ((cpu_2030.MASK & BIT1) && (sel_intrp_lch[0] || (cpu_2030.GF[0] & BIT4) != 0))
                          nextWX &= 0xffe;
                       else if ((cpu_2030.MASK & BIT2) && (sel_intrp_lch[0] || (cpu_2030.GF[1] & BIT4) != 0))
                          nextWX &= 0xffd;
                       else if (((cpu_2030.MASK & BIT7) && cpu_2030.F_REG != 0) || timer_update)
                          nextWX &= 0xffc;
                       break;
           case 0x1B:  /* Clear MC */
                       break;
           case 0x1C:  /* store wrap */
                       break;
           case 0x1d:  /* reset IPL load */
                       break;
           case 0x1e:  /* Reset External interrupts */
                       break;
           case 0x1f:  /* Set external interrupt */
                       break;
           }

           if (sal->CM < 3 && sal->CU == 2) {
               nextWX &= 0xff;
               nextWX |= (sal->CK & 0xF) << 8;
           }

           cpu_2030.WX = nextWX;

           /* Handle alternate CK that don't change branch address */
           switch(sal->CK) {
           case 0x11:  break;
           case 0x12: /* Reset wrap */
                       i_wrap_cpu = wrap_buf;
                       u_wrap_cpu = wrap_buf;
                       break;
           case 0x13: /* Wrap */
                       break;
           case 0x14: /* set switches HI to K */
                       break;
           case 0x15: /* If previous carry force X to 0 */
                       break;
           case 0x16: /* Reset 1050 home loop */
                       log_console("Reset 1050 Home Loop\n");
                       break;
           case 0x17: /* set 1050 home loop */
                       log_console("Set 1050 Home Loop\n");
                       break;
           case 0x18: /* Set even parity */
                       if (even_parity != 0)
                          alu_chk = 1;
                       even_parity = 0x100;
                       break;
           case 0x19: /* Check ascii flag */
                       break;
           case 0x1A: /* Select interrupt SEL1 -> 0,x SEL2 -> x,0, timer -> 0,0 */
                       break;
           case 0x1B:  /* Clear MC */
                       cpu_2030.MC_REG = 0;
                       break;
           case 0x1C:  /* store wrap */
                       wrap_buf = i_wrap_cpu;
                       break;
           case 0x1d:  /* reset IPL load */
                       cpu_2030.FT &= ~BIT4;
                       load_mode = 0;
                       even_parity = 0;
                       alu_chk = 0;
                       break;
           case 0x1e:  /* Reset External interrupts */
                       cpu_2030.F_REG ^= cpu_2030.F_REG & (cpu_2030.L_REG | 0x80) & 0xff;
                       break;
           case 0x1f:  /* Set external interrupt */
                       cpu_2030.F_REG |= 0x80;
                       break;
           }

           if (sal->CK == 0x14) {
               cpu_2030.Bbus = (H_SW << 4) | J_SW;
               cpu_2030.Bbus |= odd_parity[cpu_2030.Bbus];
           } else {
               /* Set B Bus input */
               switch (sal->CB) {
               case 0:       /* R */
                      cpu_2030.Bbus = cpu_2030.R_REG;
                      break;
               case 1:       /* L */
                      cpu_2030.Bbus = cpu_2030.L_REG;
                      break;
               case 2:       /* D */
                      cpu_2030.Bbus = cpu_2030.D_REG;
                      break;
               case 3:
                      cpu_2030.Bbus = ((sal->CK << 4) & 0xf0) | (sal->CK & 0xf);
                      cpu_2030.Bbus |= odd_parity[cpu_2030.Bbus];
                      break;
               }
           }


           /* Check parity on B bus */
           if (!second_err_stop &&
                     ((odd_parity[cpu_2030.Bbus & 0xff] ^ cpu_2030.Bbus) & 0x100) != 0) {
               log_warn("Set B bus %03x\n", cpu_2030.Bbus);
               cpu_2030.MC_REG |= BIT1;
           }

           allow_a_reg_chk = 0;
           /* Gate register to A Bus */
           switch (sal->CA) {
           case 0x00:    /* FT */
                  /* Virtual register */
                  cpu_2030.Abus = cpu_2030.FT;
                  break;
           case 0x01:    /* TT */
                  cpu_2030.Abus = cpu_2030.TT;
                  break;
           case 0x02:
           case 0x03:
                  cpu_2030.Abus = 0x100;
                  break;
           case 0x04:    /* S */
                  cpu_2030.Abus = cpu_2030.S_REG;
                  break;
           case 0x05:    /* H */
                  cpu_2030.Abus = cpu_2030.H_REG;
                  break;
           case 0x06:    /* FI */
                  /* MPX Channel Bus-in */
                  cpu_2030.Abus = cpu_2030.FI;
                  allow_a_reg_chk = 1;
                  break;
           case 0x07:    /* R */
                  cpu_2030.Abus = cpu_2030.R_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x08:    /* D */
                  cpu_2030.Abus = cpu_2030.D_REG;
                  allow_a_reg_chk = 1;
                  suppr_a_reg_chk = 0;
                  break;
           case 0x09:    /* L */
                  cpu_2030.Abus = cpu_2030.L_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0A:    /* G */
                  cpu_2030.Abus = cpu_2030.G_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0B:    /* T */
                  cpu_2030.Abus = cpu_2030.T_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0C:    /* V */
                  cpu_2030.Abus = cpu_2030.V_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0D:    /* U */
                  cpu_2030.Abus = cpu_2030.U_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0E:    /* J */
                  cpu_2030.Abus = cpu_2030.J_REG;
                  proc_stop_loop_active = process_stop & cf_stop;
                  allow_a_reg_chk = 1;
                  break;
           case 0x0F:    /* I */
                  cpu_2030.Abus = cpu_2030.I_REG;
                  allow_a_reg_chk = 1;
                  break;
           case 0x10:    /* F */
                  cpu_2030.Abus = ~cpu_2030.F_REG;
                  break;
           case 0x11:    /* SFG */
                  cpu_2030.Abus = (F_SW << 4) | (G_SW);
                  cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                  break;
           case 0x12:
                  if (CHK_SW == 1)
                     cpu_2030.Abus = 0;
                  else
                     cpu_2030.Abus = cpu_2030.MC_REG;
                  break;
           case 0x13:
                  cpu_2030.Abus = 0x100;
                  allow_a_reg_chk = 1;
                  break;
           case 0x14:    /* C */
                  cpu_2030.Abus = cpu_2030.C_REG;
                  cpu_2030.C_REG = 0;
                  timer_update = 0;
                  break;
           case 0x15:    /* Q */
                  cpu_2030.Abus = cpu_2030.Q_REG;
                  cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                  allow_a_reg_chk = 1;
                  break;
           case 0x16:    /* JI */
                  cpu_2030.Abus = cpu_2030.JI;
                  break;
           case 0x17:    /* TI */
                  model1052_in(cpu_2030.console, &cpu_2030.TI);
                  cpu_2030.Abus = cpu_2030.TI;
                  allow_a_reg_chk = 1;
                  break;
           case 0x18:
           case 0x19:
           case 0x1A:
           case 0x1B:
                  cpu_2030.Abus = 0x100;
                  break;
           case 0x1C:    /* GR */
                   /* Check if gating GI to GR, do before looking at GR */
                   if (sal->CS == 0x1d)
                       cpu_2030.GR[cpu_2030.ch_sel] = cpu_2030.GI[cpu_2030.ch_sel];
                  cpu_2030.Abus = cpu_2030.GR[cpu_2030.ch_sel];
                  if (((odd_parity[cpu_2030.GR[cpu_2030.ch_sel]& 0xff] ^ cpu_2030.GR[cpu_2030.ch_sel]) & 0x100) != 0) {
                      cpu_2030.GE[cpu_2030.ch_sel] |= BIT5;
                  }
                  break;
           case 0x1D:    /* GS */
                  /* Virtual register */
                  cpu_2030.Abus = 0;
                  /* GR Full */
                  if (sel_gr_full[cpu_2030.ch_sel])
                      cpu_2030.Abus |= BIT0;
                  /* Chain detect. */
                  if (sel_chain_det[cpu_2030.ch_sel])
                      cpu_2030.Abus |= BIT1;
                  /* Interrupt condition */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & CHAN_STA_IN) != 0) {
                           /* CD = 0  or CC = 1 */
                      if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT0) != 0 ||
                                   (cpu_2030.GF[cpu_2030.ch_sel] & BIT1) == 0) {
                           if (sel_poll_ctrl[cpu_2030.ch_sel] == 0 ||
                                   (cpu_2030.GI[cpu_2030.ch_sel] & BIT4) != 0)
                              cpu_2030.Abus |= BIT3;
                           if ((cpu_2030.GI[cpu_2030.ch_sel] & (BIT0|BIT2|BIT3|BIT6|BIT7)) != 0)
                              cpu_2030.Abus |= BIT3;
                      }
                  }
                  /* CD */
                  if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT0) != 0)
                      cpu_2030.Abus |= BIT4;
                  /* Channel select. */
                  if (cpu_2030.ch_sel == 0)
                      cpu_2030.Abus |= BIT5;
                  /* Chain Request */
                  if (sel_chain_req[cpu_2030.ch_sel] != 0)
                      cpu_2030.Abus |= BIT7;
                  cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                  allow_a_reg_chk = 1;
                  break;
           case 0x1E:    /* GT */
                  /* Virtual register */
                  cpu_2030.Abus = 0;
                  /* Select in */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & CHAN_SEL_IN) != 0)
                      cpu_2030.Abus |= BIT0;
                  /* Service In & Not Service Out */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_IN))
                      cpu_2030.Abus |= BIT1;
                  /* Poll control */
                  if (sel_poll_ctrl[cpu_2030.ch_sel])
                      cpu_2030.Abus |= BIT2;
                  /* Channel Busy */
                  if (sel_chan_busy[cpu_2030.ch_sel])
                      cpu_2030.Abus |= BIT3;
                  /* Address In */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & CHAN_ADR_IN) != 0)
                      cpu_2030.Abus |= BIT4;
                  /* Status In */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & CHAN_STA_IN) != 0)
                      cpu_2030.Abus |= BIT5;
                  /* SX Interrupt Latch */
                  if (sel_intrp_lch[cpu_2030.ch_sel])
                      cpu_2030.Abus |= BIT6;
                  /* Oper In */
                  if ((cpu_2030.SEL_TI[cpu_2030.ch_sel] & CHAN_OPR_IN) != 0)
                      cpu_2030.Abus |= BIT7;
                  cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                  break;
           case 0x1F:    /* GJ */
                  allow_a_reg_chk = 1;
                  switch(sal->CK) {
                  default: cpu_2030.Abus = 0x100; break;
                  case 1: cpu_2030.Abus = cpu_2030.GC[cpu_2030.ch_sel]; break;
                  case 2: cpu_2030.Abus = cpu_2030.GD[cpu_2030.ch_sel]; break;
                  case 3: cpu_2030.Abus = cpu_2030.GK[cpu_2030.ch_sel]; break;
                  case 4: cpu_2030.Abus = cpu_2030.GE[cpu_2030.ch_sel];
                          if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT4) != 0)
                              cpu_2030.Abus |= BIT0;
                          allow_a_reg_chk = 0;
                          break;
                  case 8: cpu_2030.Abus = cpu_2030.GO[cpu_2030.ch_sel]; break;
                  case 6:
                          cpu_2030.Abus = 0;
                          /* SX CNT Ready and Not Zero */
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_OPR_OUT) != 0)
                              cpu_2030.Abus |= BIT0;
                          /* SLI */
                          if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT2) != 0)
                              cpu_2030.Abus |= BIT1;
                          /* Com 7 bit output */
                          if ((cpu_2030.GG[cpu_2030.ch_sel] & BIT7) != 0)
                              cpu_2030.Abus |= BIT2;
                          /* Count ready and zero */
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_ADR_OUT) != 0)
                              cpu_2030.Abus |= BIT3;
                          /* CC */
                          if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT1) != 0)
                              cpu_2030.Abus |= BIT5;
                          /* Read backwards */
                          if (cpu_2030.GG[cpu_2030.ch_sel] == 0xc)
                              cpu_2030.Abus |= BIT6;
                          /* Skip */
                          if ((cpu_2030.GF[cpu_2030.ch_sel] & BIT3) != 0)
                              cpu_2030.Abus |= BIT7;
                          cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                          break;

                  case 7:
                          cpu_2030.Abus = 0;
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_OPR_OUT) != 0)
                              cpu_2030.Abus |= BIT7;
                          /* Bus out ctrl */
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_SRV_OUT) != 0)
                              cpu_2030.Abus |= BIT6;
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_SRV_OUT) != 0)
                              cpu_2030.Abus |= BIT5;
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_CMD_OUT) != 0)
                              cpu_2030.Abus |= BIT4;
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_ADR_OUT) != 0)
                              cpu_2030.Abus |= BIT3;
                          /* SX1 ROS req */
                          if (sel_ros_req)
                              cpu_2030.Abus |= BIT2;
                          if ((cpu_2030.SEL_TAGS[cpu_2030.ch_sel] & CHAN_SUP_OUT) != 0)
                              cpu_2030.Abus |= BIT1;
                          /* Input command */
                          if ((cpu_2030.GF[cpu_2030.ch_sel] & 3) == 2 || (cpu_2030.GF[cpu_2030.ch_sel] & 5) == 4)
                              cpu_2030.Abus |= BIT0;
                          cpu_2030.Abus |= odd_parity[cpu_2030.Abus];
                          break;
                  }
                  break;
           }

           if (sal->CL == 2 || any_priority_lch || suppr_a_reg_chk)
               allow_a_reg_chk = 0;

           if (allow_a_reg_chk &&
                     ((odd_parity[cpu_2030.Abus & 0xff] ^ cpu_2030.Abus) & 0x100) != 0) {
               log_itrace("Set A bus %03x\n", cpu_2030.Abus);
               cpu_2030.MC_REG |= BIT0;
           }

           /* Set up Alu A input */
           abus_f = 0;
           switch (sal->CF) {
           case 0:
                   break;
           case 1:
                   abus_f = cpu_2030.Abus & 0xf;
                   break;
           case 2:
                   abus_f = cpu_2030.Abus & 0xf0;
                   break;
           case 3:
                   abus_f = cpu_2030.Abus & 0xff;
                   break;
           case 4:
                   if (PROC_SW != 0)
                       cf_stop = 1;
                   abus_f = 0;
                   break;
           case 5:
                   abus_f = (cpu_2030.Abus >> 4) & 0xf;
                   break;
           case 6:
                   abus_f = (cpu_2030.Abus << 4) & 0xf0;
                   break;
           case 7:
                   abus_f = ((cpu_2030.Abus >> 4) & 0xf) | ((cpu_2030.Abus << 4) & 0xf0);
                   break;
           }

           dec = (sal->CV == 3);
           /* Set up B alu input. */
           bbus_f = cpu_2030.Bbus & cg_mask[sal->CG];
           if ((sal->CV & 0x2) ? ((cpu_2030.S_REG & BIT0) != 0): (sal->CV == 1)) {
              bbus_f ^= 0xff;
              tc = 1;
           } else {
              if (dec)
                 bbus_f = ((bbus_f + 0x60) & 0xf0) | ((bbus_f + 0x6) & 0x0f);
              tc = 0;
           }

           /* Carry into Alu */
           carry_in = 0;
           switch (sal->CC) {
           case 0:
           case 2:
           case 3:
           case 4:
           case 7:
                   carry_in = 0;
                   break;
           case 5:
           case 1:
                   carry_in = 1;
                   break;
           case 6:
                   carry_in = (cpu_2030.S_REG & BIT3) != 0;
                   break;
           }

           /* Do Alu operation */
           carries = 0;
           switch (sal->CC) {
           case 0:
           case 1:
           case 4:
           case 5:
           case 6:
                   /* Compute final sum */
                   cpu_2030.Alu_out = abus_f + bbus_f + carry_in;
                   /* Compute bit carries */
                   carries = ((abus_f & bbus_f) | ((abus_f ^ bbus_f) & ~cpu_2030.Alu_out));
                   cpu_2030.Alu_out &= 0xff;
                   break;
           case 2:
                   cpu_2030.Alu_out = abus_f & bbus_f;
                   break;
           case 3:
                   cpu_2030.Alu_out = abus_f | bbus_f;
                   break;
           case 7:
                   cpu_2030.Alu_out = abus_f ^ bbus_f;
                   break;
           }

           /* Fix up if decimal mode */
           if (dec) {
               if ((carries & BIT4) == 0)
                  cpu_2030.Alu_out -= 0x6;
               if ((carries & BIT0) == 0)
                  cpu_2030.Alu_out -= 0x60;
               if (sal->CC == 7) {
                   cpu_2030.ASCII = (cpu_2030.R_REG & BIT4) != 0;
                   suppr_half_trap_lch = (cpu_2030.R_REG & BIT5) == 0;
               }
               if (sal->CC == 3) {
                   wait = 1;
               } else {
                   wait = 0;
               }
           }
           cpu_2030.Alu_out |= odd_parity[cpu_2030.Alu_out] ^ even_parity;



           /* Save results into destination */
           switch (sal->CD) {
           case 0:
                   break;
           case 1:
                   cpu_2030.TE = cpu_2030.Alu_out;
                   model1052_out(cpu_2030.console, cpu_2030.TE);
                   break;
           case 2:
                   cpu_2030.JE = cpu_2030.D_REG & 0xff;
                   break;
           case 3:
                   cpu_2030.Q_REG = cpu_2030.Alu_out & 0xff;
                   break;
           case 4:
                   cpu_2030.TA = cpu_2030.Alu_out & 0xff;
                   break;
           case 5:
                   cpu_2030.H_REG = cpu_2030.Alu_out;
                   priority_lch = 0;
                   break;
           case 6:
                   cpu_2030.S_REG = cpu_2030.Alu_out & 0xFF;
                   break;
           case 7:
                   /* If protection violation, don't let R get changed */
                   if ((allow_write && protect_loc_cpu_or_mpx) || mem_prot) {
                       stg_prot_req = 1;
                       break;
                   }
                   cpu_2030.R_REG = cpu_2030.Alu_out;
                   if (((odd_parity[cpu_2030.R_REG & 0xff] ^ cpu_2030.R_REG) & 0x100) != 0) {
                        cpu_2030.MC_REG |= BIT6;
                   }
                   break;
           case 8:
                   cpu_2030.D_REG = cpu_2030.Alu_out;
                   break;
           case 9:
                   cpu_2030.L_REG = cpu_2030.Alu_out;
                   break;
           case 10:
                   cpu_2030.G_REG = cpu_2030.Alu_out;
                   break;
           case 11:
                   cpu_2030.T_REG = cpu_2030.Alu_out;
                   break;
           case 12:
                   cpu_2030.V_REG = cpu_2030.Alu_out;
                   break;
           case 13:
                   cpu_2030.U_REG = cpu_2030.Alu_out;
                   if (sal->CG == 0 && ((((tc) ? 0 : 0x80) ^ carries) & 0x80) == 0) {
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == BIT5)
                           u_wrap_mpx = 1;
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == (0))
                           u_wrap_cpu = 1;
                       if (u_wrap_cpu) {
                           log_trace("Set U wrap\n");
                       }
                   } else {
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == BIT5)
                           u_wrap_mpx = 0;
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == (0))
                           u_wrap_cpu = 0;
                   }
                   break;
           case 14:
                   cpu_2030.J_REG = cpu_2030.Alu_out;
                   break;
           case 15:
                   cpu_2030.I_REG = cpu_2030.Alu_out;
                   if (sal->CG == 0 && ((((tc) ? 0 : 0x80) ^ carries) & 0x80) == 0) {
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == (0)) {
                           i_wrap_cpu = 1;
                           log_trace("Set I wrap\n");
                       }
                   } else {
                       if ((cpu_2030.H_REG & (BIT5|BIT6)) == (0))
                           i_wrap_cpu = 0;
                   }
                   break;
           }

           /* Save carry from AC if requested */
           if ((sal->CC & 0x4) != 0 && sal->CC != 7) {
               if ((carries & BIT0) != 0)
                   cpu_2030.S_REG |= BIT3;
               else
                   cpu_2030.S_REG &= ~BIT3;
           }

           /* Set MC flag for ALU error */
           if (even_parity || alu_chk) {
               cpu_2030.MC_REG |= BIT7;
           }

           /* Should be bad parity used for testing */
           if (cpu_2030.WX == 0xba0 || cpu_2030.WX == 0xb60) {
               cpu_2030.MC_REG |= BIT3|BIT4|BIT5;
           }



           /* Check if there are any machine checks */
           any_mach_chk = (cpu_2030.MC_REG != 0) | sel_chnl_chk;

           if ((CHK_SW == 2) && suppr_half_trap_lch && any_mach_chk) {
               first_mach_chk_req = 1;
           }

           /* Update static flags */
           switch (sal->CS) {
           case 0x00:
                   break;
           case 0x01:  /* LZ -> S5 */
                   cpu_2030.S_REG &= ~BIT5;
                   if ((cpu_2030.Alu_out & 0x0f) == 0)
                       cpu_2030.S_REG |= BIT5;
                   break;
           case 0x02:  /* HZ -> S4 */
                   cpu_2030.S_REG &= ~BIT4;
                   if ((cpu_2030.Alu_out & 0xf0) == 0)
                       cpu_2030.S_REG |= BIT4;
                   break;
           case 0x03:  /* LZ,HZ-> S5,S4 */
                   cpu_2030.S_REG &= ~(BIT5|BIT4);
                   if ((cpu_2030.Alu_out & 0xf0) == 0)
                       cpu_2030.S_REG |= BIT4;
                   if ((cpu_2030.Alu_out & 0x0f) == 0)
                       cpu_2030.S_REG |= BIT5;
                   break;
           case 0x04: /* 0 -> S5,S4 */
                   cpu_2030.S_REG &= ~(BIT4|BIT5);
                   break;
           case 0x05: /* Treq */
                   /* If 1050 request set BIT1 */
                   cpu_2030.S_REG &= ~(BIT1);
                   if ((cpu_2030.MPX_TI & CHAN_OPR_IN) == 0 && t_request)
                      cpu_2030.S_REG |= BIT1;
                   break;
           case 0x06:   /* 0 -> S0 */
                   cpu_2030.S_REG &= ~(BIT0);
                   break;
           case 0x07:   /* 1 -> S0 */
                   cpu_2030.S_REG |= BIT0;
                   break;
           case 0x08: /* 0 -> S2 */
                   cpu_2030.S_REG &= ~(BIT2);
                   break;
           case 0x09: /* ANSNZ -> S2 */
                   if ((cpu_2030.Alu_out & 0xff) != 0)
                      cpu_2030.S_REG |= BIT2;
                   break;
           case 0x0A: /* 0 -> S6 */
                   cpu_2030.S_REG &= ~(BIT6);
                   break;
           case 0x0B: /* 1 -> S6 */
                   cpu_2030.S_REG |= BIT6;
                   break;
           case 0x0C: /* 0 -> S7 */
                   cpu_2030.S_REG &= ~(BIT7);
                   break;
           case 0x0D: /* 1 -> S7 */
                   cpu_2030.S_REG |= BIT7;
                   break;
           case 0x0E:
                   /* Set FB from K, FB is psuedo MPX control register */
                   /* 0011  MPX set reset based on R
                          R<0> MPX mask latch set on.
                          R<1> Select 1 mask set on.
                          R<2> Select 2 mask on.
                          R<7> External trap mask on */
                   if ((sal->CK & BIT6) != 0 && (sal->CK & BIT7) != 0)  {
                          /* Set interrupt mask */
                          cpu_2030.MASK = (BIT0|BIT1|BIT2|BIT7) & cpu_2030.R_REG;
                   }
                   /* 1001 Set or reset based on S<0,1,2>
                         S<0> set XX high latch on.
                         S<1> Sets X high latch on.
                         S<2> Sets X low latch on.
                          control M <1,2,3> */
                   if ((sal->CK & BIT4) != 0 && (sal->CK & BIT7) != 0)  {
                          /* Set high order bump address */
                          cpu_2030.XX_REG = (BIT0|BIT1|BIT2) & cpu_2030.S_REG;
                   }
                   /* 1100  MPX Interrupt latch on */
                   if ((sal->CK & BIT4) != 0 && (sal->CK & BIT5) != 0)  {
                          cpu_2030.FT &= ~BIT7;
                          if (sal->PK)
                              cpu_2030.FT |= BIT7;
                   }
                   /* 0110  MPX Operation latch on */
                   if ((sal->CK & BIT5) != 0 && (sal->CK & BIT6) != 0)  {
                          cpu_2030.FT &= ~BIT2;
                          if (sal->PK)
                              cpu_2030.FT |= BIT2;
                   }
                   /* 1010  MPX Suppress out latch on */
                   if ((sal->CK & BIT4) != 0 && (sal->CK & BIT6) != 0)  {
                          mpx_supr_out_lch = sal->PK;
                   }
                   /* 0101  MPX Operation out control latch on */
                   if ((sal->CK & BIT5) != 0 && (sal->CK & BIT7) != 0)  {
                          cpu_2030.MPX_TAGS = 0;
                          cpu_2030.MPX_TI = 0;      /* Clear inbound tags */
                          mpx_start_sel = 0;
                          if (sal->PK)
                              cpu_2030.MPX_TAGS |= CHAN_OPR_OUT;
                   }
                   if ((cpu_2030.MPX_TAGS & (CHAN_SEL_OUT|CHAN_ADR_OUT)) == (CHAN_SEL_OUT|CHAN_ADR_OUT) &&
                       (cpu_2030.MPX_TI & (CHAN_STA_IN)) != 0) {
                       mpx_start_sel = 0;
                       cpu_2030.MPX_TAGS &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                   }
                   break;
           case 0x0F:
                   /* K->FA controls to MPX channel */
                   /* 10000 PI Sets command-start latch on */
                   /* 01000 PO set bus out register from R. */
                   /* 00100 PO set address-out line on. */
                   /* 00010 PO set command out line on */
                   /* 00001 PS set service out line on */
                   if (sal->CK & BIT4) {
                       cpu_2030.O_REG = cpu_2030.R_REG;
                   }
                   if (sal->PK) {
                       mpx_cmd_start = 1;
                   } else {  /* Check */
                       mpx_cmd_start = 0;
                   }
                   if (sal->CK & BIT5) {
                        cpu_2030.MPX_TAGS |= CHAN_ADR_OUT;
                        if (sal->CK & BIT4) {
                            cpu_2030.MPX_TAGS |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
                        }
                   } else {
                        cpu_2030.MPX_TAGS &= ~(CHAN_ADR_OUT);
                        if ((sal->CK & BIT4) == 0) {
                            cpu_2030.MPX_TAGS &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                        }
                   }
                   if (sal->CK & BIT6) {
                        cpu_2030.MPX_TAGS |= CHAN_CMD_OUT;
                   }
                   if (sal->CK & BIT7) {
                        cpu_2030.MPX_TAGS |= CHAN_SRV_OUT;
                   }
                   break;
           case 0x16:
                   cpu_2030.GC[cpu_2030.ch_sel] = cpu_2030.GU[cpu_2030.ch_sel];
                   cpu_2030.GD[cpu_2030.ch_sel] = cpu_2030.GV[cpu_2030.ch_sel];
                   sel_chain_req[cpu_2030.ch_sel] = 0;
                   break;
           case 0x17:
                   /* Selector R to selector protection */
                   cpu_2030.GK[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   break;
           case 0x18:
                   /* Selector R to selector flag register */
                   /* Don't reset PCI if set */
                   cpu_2030.GF[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel] |
                          (cpu_2030.GF[cpu_2030.ch_sel] & BIT4);
                   sel_cnt_rdy_zero[cpu_2030.ch_sel] = 0;
                   break;
           case 0x19:
                   /* Selector R to selector command */
                   cpu_2030.GG[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   break;
           case 0x1a:
                   /* Selector R to address U */
                   cpu_2030.GU[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   break;
           case 0x1b:
                   /* Selector R to address V */
                   cpu_2030.GV[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   break;
           case 0x1c:
                   /* K > GH */
                   switch (sal->CK & 0xf) {
                   /* k = 0, selector channel 1 & 2 reset */
                   case 0:
                            sel_chan_busy[0] = 0;
                            sel_intrp_lch[0] = 0;
                            sel_gr_full[0] = 0;
                            sel_cnt_rdy_not_zero[0] = 0;
                            sel_status_stop_cond[0] = 0;
                            sel_chan_busy[1] = 0;
                            sel_intrp_lch[1] = 0;
                            sel_gr_full[1] = 0;
                            sel_cnt_rdy_not_zero[1] = 0;
                            sel_status_stop_cond[1] = 0;
                            break;
                   /* k = 1, diagnostic and tag controls are set */
                   case 1:  sel_diag_mode[cpu_2030.ch_sel] = 1;
                            break;
                   /* k = 2, tag control is reset. */
                   case 2:  sel_diag_tag_ctrl[cpu_2030.ch_sel] = sel_diag_mode[cpu_2030.ch_sel];
                            break;
                   /* k = 7, chain detect is set */
                   case 7:  sel_chain_det[cpu_2030.ch_sel] = 1;
                            break;
                   /* k = c, select out */
                   case 0xc: cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= CHAN_SEL_OUT|CHAN_HLD_OUT;
                            break;
                   /* k = d, reset chain control */
                   case 0xd:
                            sel_chain_det[cpu_2030.ch_sel] = 0;
                            sel_poll_ctrl[cpu_2030.ch_sel] = 1;
                            break;
                   default:
                           break;
                   }
                   break;
           case 0x1d:
                   /* Selector bus in to GR */
                   cpu_2030.GR[cpu_2030.ch_sel] = cpu_2030.GI[cpu_2030.ch_sel];
                   break;
           case 0x1e:
                   /* K > GB */
                   switch (sal->CK & 0xf) {
                   /* Set program check */
                   case 0:  cpu_2030.GE[cpu_2030.ch_sel] |= BIT2; break;
                   /* Select working channel */
                   case 1:  cpu_2030.ch_sel = sal->PK; break;
                   /* Reset operational out */
                   case 2:  cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~(CHAN_OPR_OUT); break;
                   /* Reset PCI */
                   case 3:  cpu_2030.GF[cpu_2030.ch_sel] &= ~BIT4; break;
                   /* Set interrupt flag */
                   case 4:  sel_intrp_lch[cpu_2030.ch_sel] = 1; break;
                   /* Set Channel control check */
                   case 5:  cpu_2030.GE[cpu_2030.ch_sel] |= BIT5; break;
                   /* Set GR to zero */
                   case 6:  cpu_2030.GR[cpu_2030.ch_sel] = 0x100; break;
                   /* Save CPU */
                   case 7: break;
                   /* Count ready set/reset */
                   case 8:  sel_cnt_rdy_not_zero[cpu_2030.ch_sel] = 1;
                            sel_cnt_rdy_zero[cpu_2030.ch_sel] = 0;
                            sel_status_stop_cond[cpu_2030.ch_sel] = 0;
                            sel_chain_det[cpu_2030.ch_sel] = 0;
                            sel_chain_req[cpu_2030.ch_sel] = 0;
                            sel_poll_ctrl[cpu_2030.ch_sel] = 0;
                            sel_ros_req &= ~(1 << cpu_2030.ch_sel);
                            break;
                   /* Channel reset/poll reset */
                   case 9:
                            if (sal->PK) {
                                sel_poll_ctrl[cpu_2030.ch_sel] = 0;
                                sel_ros_req &= ~(1 << cpu_2030.ch_sel);
                            }
                            sel_chain_req[cpu_2030.ch_sel] = 0;
                            sel_cnt_rdy_not_zero[cpu_2030.ch_sel] = 0;
                            sel_cnt_rdy_zero[cpu_2030.ch_sel] = 0;
                            sel_chain_det[cpu_2030.ch_sel] = 0;
                            sel_chan_busy[cpu_2030.ch_sel] = 0;
                            sel_intrp_lch[cpu_2030.ch_sel] = 0;
                            sel_halt_io[cpu_2030.ch_sel] = 0;
                            sel_gr_full[cpu_2030.ch_sel] = 0;
                            sel_status_stop_cond[cpu_2030.ch_sel] = 0;
                            cpu_2030.GE[cpu_2030.ch_sel] = 0;
                            cpu_2030.GF[cpu_2030.ch_sel] = 0;
                            cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~CHAN_SUP_OUT;
                            break;
                   /* Set Suppress out */
                   case 0xa: if (sal->PK)
                                 cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= CHAN_SUP_OUT;
                             else
                                 cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~CHAN_SUP_OUT;
                             break;
                   /* Set Poll control */
                   case 0xb: sel_poll_ctrl[cpu_2030.ch_sel] = sal->PK; break;
                   /* Reset select out */
                   case 0xc:
                            cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                            if (sel_poll_ctrl[cpu_2030.ch_sel] == 0)
                                cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~(CHAN_ADR_OUT);
                             log_selchn("Reset select out\n"); break;
                            break;
                   /* Set channel busy */
                   case 0xd: sel_chan_busy[cpu_2030.ch_sel] = 1;
                             log_selchn("Set channel busy\n"); break;
                   /* Set Halt IO */
                   case 0xe: sel_halt_io[cpu_2030.ch_sel] = 1;
                             sel_chain_req[cpu_2030.ch_sel] = 0;
                             break;
                   /* Set Interface check */
                   case 0xf: cpu_2030.GE[cpu_2030.ch_sel] |= BIT6; break;
                   default:
                            break;
                   }
                   /* K = 0, program check
                      K = 1 and KP = 0 selector channel 1
                            and KP = 1 selector channel 2
                      K = 2 operational out reset
                      K = 3 PCI flag reset
                      K = 4 selector channel interrupt is set
                      K = 5 channel control check
                      K = 6 GR to zero is set
                      K = 7 CPU stored.
                      K = 8 and KP = 0, count ready is reset.
                                KP = 1, count ready is set.
                      K = 9 and KP = 0, channel reset.
                                KP = 1, poll controll reset and channel reset
                      K = A and KP = 0, suppress out reset.
                                KP = 1, suppress out set.
                      K = B and KP = 0, poll control reset.
                                KP = 1, poll control is set.
                      K = C select-out is reset
                      K = D channel busy is set.
                      K = E halt io latch is set.
                      K = F interfac control check.
                      */
                     break;
             case 0x1f:
                   /* K > GA */
                     /* K = 0001 sets service out line on */
                     /* K = 0010 sets command out line on */
                     /* K = 0100 sets address out line on. */
                     /* K = 1000 sets buss out control line on. */
                   if (sal->CK & BIT4) {
                       sel_bus_out_ctrl[cpu_2030.ch_sel] = 1;
                   }
                   if (sal->CK & BIT5) {
                        cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= CHAN_ADR_OUT;
                        cpu_2030.GO[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                        if (sal->CK & BIT4) {
                            sel_start_sel = 1;
                            sel_status_stop_cond[cpu_2030.ch_sel] = 0;
                        }
                   } else {
                        cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~CHAN_ADR_OUT;
                   }
                   if (sal->CK & BIT6) {
                        cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= CHAN_CMD_OUT;
                        cpu_2030.GO[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   }
                   if (sal->CK & BIT7) {
                        cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= CHAN_SRV_OUT;
                        cpu_2030.GO[cpu_2030.ch_sel] = cpu_2030.GR[cpu_2030.ch_sel];
                   }
                   break;
             }

           /* Set hard stop if needed */
           if ((second_err_stop && first_mach_chk_req)) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }
           if (chk_or_diag_stop_sw && (any_mach_chk || alu_chk)) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }
           if ((CHK_SW == 0) && (sal->CS == 9) && suppr_half_trap_lch &&
                       (cpu_2030.Alu_out & 0xff) != 0) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }
           if (match && ((MATCH_SW == 2) || (MATCH_SW == 7) || (MATCH_SW == 8))) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }
           if (RATE_SW == 0) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }
           if  (RATE_SW == 2 && any_priority_pulse && PROC_SW == 2) {
              hard_stop = 1;
              if (any_priority_lch)
                  priority_lch = 1;
           }

           if (sal->CM == 0 || sal->CM == 2) {
               if (((odd_parity[cpu_2030.R_REG & 0xff] ^ cpu_2030.R_REG) & 0x100) != 0) {
                   cpu_2030.MC_REG |= BIT6;
               }
           }

           /* Save status of flags for next cycle to check */
           /* But don't update if doing a restore cycle */
           if (sal->CM >= 3 || sal->CU != 3) {
                cpu_2030.STAT_REG = (((carries & BIT0) != 0) ? BIT5 : 0) |
                                    (((carries & BIT1) != 0) ? BIT6 : 0) |
                                    (((cpu_2030.Alu_out & 0xff)== 0) ? BIT4 : 0);
           }

          if ((cpu_2030.WX != 0xAE)) {

           log_reg("D=%02x F=%02x G=%02x H=%02x L=%02x Q=%02x R=%02x S=%02x T=%02x MC=%02x FT=%02x MASK=%02x XX=%x %02x %s %02x -> %02x asc=%d\n",
                   cpu_2030.D_REG, cpu_2030.F_REG, cpu_2030.G_REG, cpu_2030.H_REG, cpu_2030.L_REG,
                   cpu_2030.Q_REG, cpu_2030.R_REG, cpu_2030.S_REG, cpu_2030.T_REG, cpu_2030.MC_REG,
                   cpu_2030.FT, cpu_2030.MASK, cpu_2030.XX_REG, abus_f, cc_name[sal->CC], bbus_f,
                   cpu_2030.Alu_out, cpu_2030.ASCII);
           log_reg("M=%02x N=%02x I=%02x J=%02x U=%02x V=%02x WX=%03x FWX=%03x GWX=%03x ST=%02x O=%02x car=%02x %d aw=%d rc=%d 2nd=%d tc=%d\n",
                   cpu_2030.M_REG, cpu_2030.N_REG, cpu_2030.I_REG, cpu_2030.J_REG,
                   cpu_2030.U_REG, cpu_2030.V_REG, cpu_2030.WX, cpu_2030.FWX, cpu_2030.GWX,
                   cpu_2030.STAT_REG, cpu_2030.O_REG, carries, priority_lch, allow_write, read_call,
                   second_err_stop, tc);
           log_selchn("GE[0]=%02x GF[0]=%02x GG[0]=%02x GI[0]=%02x GK[0]=%02x GR[0]=%02x GO[0]=%02x GCD=%02x%02x GUV=%02x%02x\n",
                   cpu_2030.GE[0] & 0xff, cpu_2030.GF[0] & 0xff, cpu_2030.GG[0] & 0xff,
                   cpu_2030.GI[0] & 0xff, cpu_2030.GK[0] & 0xff, cpu_2030.GR[0] & 0xff,
                   cpu_2030.GO[0] & 0xff, cpu_2030.GC[0] & 0xff, cpu_2030.GD[0] & 0xff,
                   cpu_2030.GU[0] & 0xff, cpu_2030.GV[0] & 0xff);
           log_selchn("GE[1]=%02x GF[1]=%02x GG[1]=%02x GI[1]=%02x GK[1]=%02x GR[1]=%02x GO[1]=%02x GCD=%02x%02x GUV=%02x%02x\n",
                   cpu_2030.GE[1] & 0xff, cpu_2030.GF[1] & 0xff, cpu_2030.GG[1] & 0xff,
                   cpu_2030.GI[1] & 0xff, cpu_2030.GK[1] & 0xff, cpu_2030.GR[1] & 0xff,
                   cpu_2030.GO[1] & 0xff, cpu_2030.GC[1] & 0xff, cpu_2030.GD[1] & 0xff,
                   cpu_2030.GU[1] & 0xff, cpu_2030.GV[1] & 0xff);
          }
        }

chan_scan:
        /* Scan each channel */
        /* Start with MPX channel */
        cpu_2030.FT &= ~BIT3;
        /* Call out to console first */
        model1052_func(cpu_2030.console, &cpu_2030.TT, cpu_2030.TA, &t_request);
        if (t_request)
            cpu_2030.FT |= BIT3;
        if (mpx_start_sel) {
            cpu_2030.MPX_TAGS |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
            cpu_2030.FT |= BIT3;
        }
        cpu_2030.MPX_TAGS &= ~CHAN_SUP_OUT;
        if (mpx_supr_out_lch || ((cpu_2030.MPX_TI & CHAN_OPR_IN) == 0 &&
                                 (cpu_2030.FT & BIT7) != 0)) {
           cpu_2030.MPX_TAGS |= CHAN_SUP_OUT;
        }
        cpu_2030.MPX_TI &= IN_TAGS;  /* Clear outbound tags */
        cpu_2030.MPX_TI |= cpu_2030.MPX_TAGS;  /* Copy current tags to output */
        cpu_2030.FI = 0;
        print_tags("CPU", 0, cpu_2030.MPX_TI, cpu_2030.O_REG);
        for (dev = chan[0]; dev != NULL; dev = dev->next) {
             dev->bus_func(dev, &cpu_2030.MPX_TI, cpu_2030.O_REG, &cpu_2030.FI);
        }
        print_tags("CPU In", 0, cpu_2030.MPX_TI, cpu_2030.FI);

        /* Check for MPX request */
        if (mpx_cmd_start == 0 && mpx_start_sel == 0 && (cpu_2030.FT & BIT3) == 0 &&
              (cpu_2030.MPX_TI & (CHAN_REQ_IN|CHAN_OPR_IN|CHAN_OPR_OUT)) == (CHAN_REQ_IN|CHAN_OPR_OUT)) {
            mpx_start_sel = 1;
        }

        /* Pending select? */
        if (cpu_2030.MPX_TI == (CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN))
            cpu_2030.FT |= BIT3;

        /* Copy Select signals */
        cpu_2030.FT &= ~(BIT5|BIT6);
        if ((cpu_2030.MPX_TI & CHAN_SEL_IN) != 0)
            cpu_2030.FT |= BIT5;
        if ((cpu_2030.MPX_TAGS & CHAN_SEL_OUT) != 0)
            cpu_2030.FT |= BIT6;

        /* Update Service output line */
        if ((cpu_2030.MPX_TI & (CHAN_SRV_OUT|CHAN_STA_IN|CHAN_SRV_IN)) == CHAN_SRV_OUT)
            cpu_2030.MPX_TAGS &= ~CHAN_SRV_OUT;

        /* Update Command output line */
        if ((cpu_2030.MPX_TI & CHAN_CMD_OUT) != 0 && (cpu_2030.MPX_TI & (CHAN_ADR_IN)) == 0)
            cpu_2030.MPX_TAGS &= ~CHAN_CMD_OUT;
        if (mpx_start_sel && (cpu_2030.MPX_TI & (CHAN_STA_IN|CHAN_CMD_OUT)) != 0) {
            mpx_start_sel = 0;
            cpu_2030.MPX_TAGS &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
        }

        /* Drop address out when we get operation in */
        if ((cpu_2030.MPX_TI & (CHAN_ADR_OUT|CHAN_OPR_IN)) == (CHAN_ADR_OUT|CHAN_OPR_IN)) {
            cpu_2030.MPX_TAGS &= ~CHAN_ADR_OUT;
        }

        /* If device responded drop select out */
        if ((cpu_2030.MPX_TAGS & CHAN_SEL_OUT) != 0 &&
               (cpu_2030.MPX_TI & (CHAN_OPR_IN|CHAN_ADR_IN)) == (CHAN_OPR_IN|CHAN_ADR_IN)) {
            cpu_2030.FT &= ~BIT6;
            cpu_2030.MPX_TAGS &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
        }

        /* Handle Selector channels */
        for (i = 0; i < 2; i++) {
             sel_ros_req &= ~(1 << i);
             if (sel_diag_mode[i] || (sal->CS == 0x1E && sal->CK == 0x2)) {
                cpu_2030.SEL_TAGS[i] &= ~CHAN_OPR_OUT;
             } else {
                cpu_2030.SEL_TAGS[i] |= CHAN_OPR_OUT;
             }
             cpu_2030.SEL_TI[i] &= IN_TAGS;               /* Clear outbound tags */
             cpu_2030.SEL_TI[i] |= cpu_2030.SEL_TAGS[i];  /* Copy current tags to output */

             for (dev = chan[i+1]; dev != NULL; dev = dev->next) {
                  dev->bus_func(dev, &cpu_2030.SEL_TI[i], cpu_2030.GO[i], &cpu_2030.GI[i]);
             }
             if (sel_diag_tag_ctrl[i]) {
                 cpu_2030.SEL_TI[i] = cpu_2030.SEL_TAGS[i];
                 if (cpu_2030.O_REG & BIT0)  /* Select In */
                    cpu_2030.SEL_TI[i] |= CHAN_SEL_IN;
                 if (cpu_2030.O_REG & BIT1)     /* Service in */
                    cpu_2030.SEL_TI[i] |= CHAN_SRV_IN;
                 if (cpu_2030.O_REG & BIT2)   /* Op in */
                    cpu_2030.SEL_TI[i] |= CHAN_OPR_IN;
                 if (cpu_2030.O_REG & BIT3)  /* Address in */
                    cpu_2030.SEL_TI[i] |= CHAN_ADR_IN;
                 if (cpu_2030.O_REG & BIT4)  /* Status in */
                    cpu_2030.SEL_TI[i] |= CHAN_STA_IN;
                 if (cpu_2030.O_REG & BIT5)  /* Request in */
                    cpu_2030.SEL_TI[i] |= CHAN_REQ_IN;
             }
             log_selchn("Select %d tags: b=%d p=%d i=%d %x\n",
                    i, sel_chan_busy[i], sel_poll_ctrl[i], sel_intrp_lch[i], sel_ros_req);

             /* If device has acknoweleged address out, drop it */
             if (cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 cpu_2030.SEL_TAGS[i] &= ~(CHAN_ADR_OUT);
                 log_selchn("Ack Addr Out\n");
             }

             /* If output try and refill GR with next location, or clear flag */
             if ((cpu_2030.GG[i] & 1) != 0 && sel_gr_full[i] == 0) {
                 if (sel_cnt_rdy_not_zero[i] && !sel_halt_io[i]) {
                     log_selchn("Fill channel %d\n", i);
                     sel_share_req |= 1 << i;
                     sel_read_cycle[i] = 1;
                 }
             }

             /* If input and has data save it in GR */
             if (cpu_2030.SEL_TI[i] == (CHAN_HLD_OUT|CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_SRV_IN) &&
                ((cpu_2030.GG[i] & 3) == 2 || (cpu_2030.GG[i] & 5) == 4)) {
                 log_selchn("Get data\n");
                 if (sel_gr_full[i] == 0 && sel_cnt_rdy_not_zero[i]) {
                     log_selchn("Read data %02x\n", cpu_2030.GI[i]);
                     cpu_2030.GR[i] = cpu_2030.GI[i];
                     sel_share_req |= 1 << i;
                     sel_read_cycle[i] = 1;
                     sel_gr_full[i] = 1;
                 }

                 if (sel_cnt_rdy_zero[i] != 0 && sel_gr_full[i] == 0 &&
                     (cpu_2030.GE[i] & BIT4) == 0 && sel_chan_busy[i]) {
                     log_selchn("Read end\n");
                     if ((cpu_2030.GF[i] & BIT2) == 0)
                         cpu_2030.GE[i] |= BIT1;
                     cpu_2030.SEL_TAGS[i] |= CHAN_CMD_OUT;
                 }

             }

             /* Output and device requesting service */
             if (cpu_2030.SEL_TI[i] == (CHAN_HLD_OUT|CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_SRV_IN) &&
                (cpu_2030.GG[i] & 1) != 0) {
                 /* Check for stop condition */
                 if (sel_cnt_rdy_not_zero[i] == 0 && sel_gr_full[i] == 0 &&
                     (cpu_2030.GE[i] & BIT4) == 0 && sel_chan_busy[i]) {
                     cpu_2030.SEL_TAGS[i] |= CHAN_CMD_OUT;
                     if ((cpu_2030.GF[i] & BIT2) == 0) {  /* Set length error */
                          cpu_2030.GE[i] |= BIT1;
                     }
                     log_selchn("Send end\n");
                 }

                 if (sel_gr_full[i] != 0 && !sel_halt_io[i]) {
                    cpu_2030.GO[i] = cpu_2030.GR[i];
                    sel_gr_full[i] = 0;
                    cpu_2030.SEL_TAGS[i] |= CHAN_SRV_OUT;
                    if (sel_cnt_rdy_not_zero[i] == 0)
                        sel_cnt_rdy_zero[i] = 1;
                    log_selchn("Send data %02x %d\n", cpu_2030.GO[i], sel_cnt_rdy_zero[i]);
                    /* If count 0 update other register */

                 }
             }

             /* Drop select out when status in present, end of operation and device end */
             if ((cpu_2030.SEL_TI[i] & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN|CHAN_SRV_OUT) &&
                 sel_status_stop_cond[i]) {
                 if ((cpu_2030.GF[i] & (BIT1)) == (0) || (cpu_2030.GI[i] & SNS_DEVEND) != 0) {
                      cpu_2030.SEL_TAGS[i] &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                      log_selchn("Drop Select out\n");
                 }
             }

             /* If we have acknowledgement of command out */
             if ((cpu_2030.SEL_TI[i] & (CHAN_ADR_IN|CHAN_STA_IN|CHAN_SRV_IN)) == 0) {
                 cpu_2030.SEL_TAGS[i] &= ~(CHAN_CMD_OUT|CHAN_SRV_OUT);
                 log_selchn("Drop service out %d\n", sel_status_stop_cond[i]);
             }

             /* If Data chain and no more data, Set chain flag, trigger channel ROS request */
             if (sel_gr_full[i] == 0 && sel_cnt_rdy_zero[i] && (cpu_2030.GF[i] & BIT0) != 0) {
                 sel_chain_req[i] = 1;
                 sel_ros_req |= 1 << i;
                 log_selchn("Trigger ROS\n");
             }

             /* Set command chain flag */
             if (((cpu_2030.SEL_TI[i] & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN)) &&
                   sel_poll_ctrl[i] && (cpu_2030.GF[i] & (BIT1)) == (BIT1)) {
                 /* Check if length incomplete */
                 if (sel_cnt_rdy_not_zero[i] && (cpu_2030.SEL_TI[i] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == CHAN_SRV_OUT &&
                      sel_gr_full[i] == 0 && (cpu_2030.GF[i] & BIT2) == 0) {
                      cpu_2030.GE[i] |= BIT1;
                 }

                /* Set suppress out to indicate command chaining */
                 if (sel_status_stop_cond[i]) {
                     cpu_2030.SEL_TAGS[i] |= CHAN_SUP_OUT;
                 }

                 /* If there are errors, set interrupt */
                 if ((cpu_2030.GI[i] & 0xf3) != 0) {
                     sel_ros_req |= 1 << i;
                 }

                 /* On device end, trigger channel ROS request */
                 if ((cpu_2030.H_REG & BIT5) == 0 && sel_cnt_rdy_zero[i] &&
                                 (cpu_2030.GI[i] & SNS_DEVEND) != 0) {
                     sel_chain_req[i] = 1;
                     sel_ros_req |= 1 << i;
                 }
                 log_selchn("Sel CC %d %03x cnt=%d\n", i, cpu_2030.GI[i],
                       sel_cnt_rdy_not_zero[i]);
             } else if (((cpu_2030.SEL_TI[i] & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN)) &&
                   sel_poll_ctrl[i] && (cpu_2030.GF[i] & (BIT0|BIT1)) == (0)) {
                 /* Check if length incomplete */
                 if (sel_cnt_rdy_not_zero[i] && (cpu_2030.SEL_TI[i] & (CHAN_SRV_IN|CHAN_SRV_OUT)) == CHAN_SRV_OUT &&
                      sel_gr_full[i] == 0 && (cpu_2030.GF[i] & BIT2) == 0) {
                      cpu_2030.GE[i] |= BIT1;
                 }

                 /* On device end, trigger channel ROS request */
                 if ((cpu_2030.H_REG & BIT5) == 0 && sel_intrp_lch[i] == 0 &&
                       sel_poll_ctrl[i] == 0 && (cpu_2030.GI[i] & SNS_DEVEND) != 0) {
                     sel_ros_req |= 1 << i;
                 }

                 log_selchn("Sel No CC %d %03x pol=%d cnt=%d busy=%d R=%d\n", i, cpu_2030.GI[i],
                       sel_poll_ctrl[i], sel_cnt_rdy_not_zero[i], sel_chan_busy[i], sel_ros_req);
             }

             /* Set command chain flag */
             if (sel_chan_busy[i] && ((cpu_2030.SEL_TI[i] & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN)) &&
                   sel_cnt_rdy_zero[i] && sel_poll_ctrl[i] && (cpu_2030.GF[i] & (BIT0|BIT1)) == 0) {
                sel_ros_req |= 1 << i;
                log_selchn("Sel end channel %d %d\n", i, sel_chan_busy[i]);
             }

             /* If channel not busy, watch for Request In */
             if (sel_poll_ctrl[i] == 0 && sel_chan_busy[i] == 0) {
                if (sel_intrp_lch[i] == 0 && cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_REQ_IN)) {
                    cpu_2030.SEL_TAGS[i] |= (CHAN_SEL_OUT|CHAN_HLD_OUT);
                    log_selchn("Select request\n");
                }
                if (sel_intrp_lch[i] == 0 &&
                     cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                    sel_ros_req |= 1 << i;
                    log_selchn("Select addressed\n");
                }
             }

             /* Set poll control if halt I/O and not chaining requests */
             if (sel_halt_io[i] && sel_chain_req[i] == 0 && sel_intrp_lch[i] == 0) {
                 sel_poll_ctrl[cpu_2030.ch_sel] = 1;
             }

             /* When we loss operator IN, the device has disconnected, signal clear address out */
             if (sel_halt_io[i] && (cpu_2030.SEL_TI[i] & (CHAN_ADR_OUT|CHAN_OPR_IN)) == CHAN_ADR_OUT) {
                 cpu_2030.SEL_TAGS[cpu_2030.ch_sel] &= ~(CHAN_ADR_OUT);
             }

             /* If device attempts to connect to use, generate interrupt */
             if (sel_poll_ctrl[i] == 0 &&
                (cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT))) {
                 sel_ros_req |= 1 << i;
                 log_selchn("reselect interrupt\n");
             }

             /* If we are connected to device and get status in. Generate an interrupt */
             if (sel_poll_ctrl[i] == 0 && (cpu_2030.H_REG & BIT5) == 0 &&
                (cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_STA_IN|CHAN_SUP_OUT) ||
                 cpu_2030.SEL_TI[i] == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_STA_IN|CHAN_SUP_OUT))) {
                  /* Command chaining */
                   if ((cpu_2030.GF[i] & (BIT1)) != (0)) {
                        if ((cpu_2030.GI[i] & SNS_DEVEND) != 0) {
                             sel_chain_req[i] = 1;
                             sel_ros_req |= 1 << i;
                             log_selchn("Status chain interrupt\n");
                        } else {
                             cpu_2030.SEL_TAGS[cpu_2030.ch_sel] |= (CHAN_SRV_OUT|CHAN_SUP_OUT);
                             log_selchn("Status chain hold\n");
                        }
                   }
                   if ((cpu_2030.GF[i] & (BIT1)) == (0)) {
                        sel_ros_req |= 1 << i;
                        log_selchn("Status interrupt\n");
                   }
             }

             /* If error on status in, set stop flag */
             if (sel_chan_busy[i] && sel_poll_ctrl[i] == 0 && (
                 ((cpu_2030.GE[i] & (BIT1|BIT2|BIT3|BIT5|BIT6)) != 0) ||
                 ((cpu_2030.GE[i] & BIT4) == 0 && sel_cnt_rdy_not_zero[i] == 0 && (cpu_2030.GF[i] & BIT0) == 0) ||
                 ((cpu_2030.GE[i] & BIT4) != 0 && CHK_SW == 0))) {
                 sel_status_stop_cond[i] = 1;
                 log_selchn("set stop %d %d %02x %d\n", i, sel_poll_ctrl[i], cpu_2030.GF[i], sel_ros_req);
             }
        }

        if (CHK_SW == 0) { /* Diagnostics mode */
            cpu_2030.FT &= ~BIT5;
            if (cpu_2030.O_REG & BIT0)  /* Select In */
               cpu_2030.FT |= BIT5;
            if ((cpu_2030.O_REG & (BIT4|BIT7)) == (BIT4|BIT7))  /* Status in */
               cpu_2030.STAT_REG |= BIT1;
            if ((cpu_2030.O_REG & (BIT5|BIT7)) == (BIT5|BIT7))  /* Service in */
               cpu_2030.STAT_REG |= BIT3;
            if (cpu_2030.O_REG & BIT6)  /* Address in */
               cpu_2030.STAT_REG |= BIT0;
            if (cpu_2030.O_REG & BIT7) { /* Op in */
               cpu_2030.FT |= BIT3;
            } else
               cpu_2030.STAT_REG |= BIT1|BIT3;
        } else {
            /* If OP IN down, STI and SVI up */
            /* If OP IN UP, service-in and no service out, or no command out, SVI up */
            /* If OP IN UP, status-in no service in, or no command out, STI up */
            /* If OP IN UP, no service in or status in, both down */
            /* If Adr out up, op in down, sti down, STI down */
            cpu_2030.FT &= ~BIT5;
            if ((cpu_2030.MPX_TI & CHAN_SEL_IN) != 0)
               cpu_2030.FT |= BIT5;
            if ((cpu_2030.MPX_TI & CHAN_STA_IN) != 0)
               cpu_2030.STAT_REG |= BIT1;
            if ((cpu_2030.MPX_TI & CHAN_SRV_IN) != 0)
               cpu_2030.STAT_REG |= BIT3;
            if ((cpu_2030.MPX_TI & CHAN_OPR_IN) != 0) {
               cpu_2030.STAT_REG |= BIT2;
            } else if ((cpu_2030.MPX_TI & CHAN_ADR_OUT) == 0) {
               cpu_2030.STAT_REG |= BIT1|BIT3;
            }
            if ((cpu_2030.MPX_TI & CHAN_ADR_IN) != 0)
               cpu_2030.STAT_REG |= BIT0;
        }

        /* Handle special case of CU when CM specifies read or write */
        if (sal->CM < 3 && sal->CU == 3) {
            if (h_backup & BIT5) {
                sel_ros_req &= ~(1 << cpu_2030.ch_sel);
                cpu_2030.ch_sel = cpu_2030.ch_sav;
                cpu_2030.WX = cpu_2030.GWX;
                cpu_2030.STAT_REG = cpu_2030.SEL_STAT;
                log_selchn("SEL IRQ2 %d %d\n", sel_ros_req, h_backup & BIT5);
            } else {
                cpu_2030.WX = cpu_2030.FWX;
                cpu_2030.STAT_REG = cpu_2030.MPX_STAT;
                log_mpxchn("MPX IRQ %d %d\n", sel_ros_req, h_backup & BIT5);
            }
        }
        /* Update the priority latch */
        if (allow_write == 0 || gate_sw_to_wx) {
            priority_stack_reg = 0;
            if (first_mach_chk_req) {
                priority_stack_reg |= BIT0;
            }
            if (cpu_2030.FT & BIT4)
                priority_stack_reg |= BIT1;
            if (force_ij_req) {
                priority_stack_reg |= BIT2;
            }
            if (mem_wrap_req)
                priority_stack_reg |= BIT3;
            if (stg_prot_req)
                priority_stack_reg |= BIT4;
            if (!stop_req)
                priority_stack_reg |= BIT5;
            /* Select channel request */
            if ((cpu_2030.H_REG & BIT5) == 0 && sel_ros_req) {
                priority_stack_reg |= BIT6;
                log_selchn("SEL Share %d\n", sel_ros_req);
            }
            /* MPX share request */
            if ((cpu_2030.H_REG & (BIT6|BIT5)) == 0 && (cpu_2030.FT & BIT3) != 0) {
                log_mpxchn("MPX Share\n");
                priority_stack_reg |= BIT7;
            }
        }

    }
    /* Restore Operational out */
    cpu_2030.MPX_TAGS |= CHAN_OPR_OUT;

}

struct _device *
model2030_init(void *render, uint16_t addr)
{
    return NULL;
}

/* Create a 2030 cpu system. */
int
model2030_create(struct _option *opt)
{
    extern  void setup_fp2030(void *rend);
    int     msize;
    uint16_t         port = 3270;
    struct _option   opts;

    if (title != NULL) {
        fprintf(stderr, "CPU already defined, can't support more then one\n");
        return 0;
    }
    title = "IBM360/30";
    setup_cpu = &setup_fp2030;
    step_cpu = &cycle_2030;

    while (get_option(&opts)) {
         int       v;

         if (strcmp(opts.opt, "PORT") == 0 && get_integer(&opts, &v)) {
             port = v;
         } else {
             fprintf(stderr, "Invalid option %s\n", opts.opt);
             return 0;
         }
    }

    if (opt->model != '\0') {
        msize = 2048 << (opt->model - 'A');
        if (msize > (64 * 1024)) {
            return 0;
        }
    } else {
        msize = 64 * 1024;
    }
    if ((M = (uint32_t *)calloc(msize, sizeof(uint32_t))) == NULL)
        return 0;
    mem_max = msize - 1;
    log_info("Model 30 configured %d %04x mem\n", msize, mem_max);
    cpu_2030.console = model1052_init_ctx(port);
    return 1;
}

DEV_LIST_STRUCT(2030, CPU_TYPE, CHAR_OPT|NUM_MOD);

/*
 * microsim360 - Model 2065 CPU.
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

#include <string.h>

#include "logger.h"
#include "device.h"
#include "model2065.h"


/* Forced address to ROSAR */

/*
   External interrupt       0006    QU001
   Invalid Instruction adr  0002    QT041
   I/O Interrupt            000E    QU001
   Machine Check            000C    QU001
   Machine Reset            0003    QY041
   Manual Control Stop      0026    QY041
   Paging Spec              0007    QU001
   Power on Reset           000B    QY041
   Program Interrupt        000A    QU001
   Program Store Compare    0004    QT041
   Pulse Mode               0005    QY051
   Q Refill RX Format       0022    QT041
   Q Refill RR Format       0030    QT041
   Q Refill RX Format       0032    QT041
   Q Refill RX Format       003A    QT041
   Q Refill RS-SI Format    0024    QT041
   Q Refill RS-SI Format    0034    QT041
   Refill Shift Instruct    0020    QJ001
   Repeat Instruct          0028    QY051
   Sap Interrupt            002E    QU001
   Scan-log out             0019    QY001
   Scan MCW4                0009    QY031
   Scan Mask                0011    QY031
   Specification            0010    QT041
   SVC Interrupt            0008    QU001
   Time Clock step          0014    QT041
   Wait                     002A    QY051

*/



/* Card A */
char *a_field[] = { "", "B", "B,IC", "A,B", "AB17", "AB", "AB38",
                    "B38M", "J9B8", "MS>AB", "IC", "A", "J49B",
                    "B8", "AB18", "" };
/* Card L */
char *b_field[] = { "", ">S", ">T", ">S,T" };

char *c_field[] = { "", "PSW>ST", "PSW>S23", "ST04>PSW", "T", "DT", "JWT", "T,D",
                    "MS1>T", "MS>ST", "MSO>T", "ARD-KEYS>D", "DWS", "ST", "D", 
                    "ATRSEL RESET", "T4>PSW", "DATA-KEYS>ST", "TB>MCW", "MS>Q",
                    "TIF", "G", "]LM>N,Q>R*IC", "DTS", "MS>T*D(21)", "S0>PSW", 
                    "MSO>S", "]K", "]N1"};
/* Card C */
char *d_field[] = { "", "NEOP", "EEOP", "BEOP", "DIR-CNTL>F", "F0", "F1", "F"};

/* Card ? */
char *e_field[] = { "", "E2+1", "E3+1", "E", "E2,3-1", "E2-1", "E3-1", "E23-1", "CON>E3",
                     "D1>E3", "D1>E3", "R>E", "R", "Q01>R", "Q23>R", "Q45>R", "Q67>R", "]NX>C",
                     "0001>V0", "0010>V0", "0011>V0", "0100>V0", "0101>V0", "0110>V0", "0111>V0",
            "1000>VO", "1001>V0", "1010>V0", "1011>V0", "1100>V0", "1101>V0", "1110>V0", "1111>V0"};

char *f_field[] = { "", "RESET", "SIGNS>STATS", "1>STAA*J47=0", "0>STAA*J47!0", "1>STAB*W=0",
                    "SAVE-SIGNS", "1>STOP-LOOP", "0>STAD", "1>STAD", "A(07)=0>INH", "A1!0>INH",
                    "MPLY-END", "]RDD-T-O-CHK", "IC>ABC", "0>ABC", "]1>IVSPEC", "1>TCL-TGR",
                    "DVD-CHECK", "1>INV-OP-TGR", "1>INTREQ-TGR", "1>OFLO/UFLO", "SAJOS",
                    "]512CRY>ICLT", "SASCR", "0>INT-TGR", "1>ASC*J57=0", "0>ASC,XEC", "0>TIME-STEP",
                    "1>TIME-GATE", "0>TIME-GATE", "INH-W-PAR-CK", "]0>CC", "]1>CC", "]2>CC", "]3>CC",
                    "STAC>STAF", "]512CRY>DLT}", "IF-INV>TGR", "1>STAB*J31=1", "RSLT-SIGN>LS", 
                    "1>SCAN-MODE", "1>XEC-TGR", "J7>STC", "0>STP,STPLP", "0>STAG",
                    "J7>ABC", "D>STC", "ABC,STC-1", "ABC-1", "STC-1", "0>STC",
                    "ABC,STC+1", "ABC+1", "STC+1", "3>STC", "E3>ABC,STC",
                    "E3>ABC", "E3>STC", "1>STC(0)", "EDIT-CTL", "]ABC-1,STC+1",
                    "SET-CR" };

/* Card C */
char *g_field[] = {  "", "1>STAA*W!0", "1>STAA*J18=0", "FXPOFLO>STAB",
                     "]SE-DEF>F", "0>STAH", "DECOFLO>STAB", "1>STAB,STAG",
                     "1>STAG", "Q->R*D", "]1>INTR-GATE", "]0>INTR-GATE",
                     "", "1>STAH", "1>STAB*J2623", "RASCR",
                     "0>IC(21,22)", "1>IC(21,22)", "2>IC(21,22)", "3>IC(21,22)", 
                     "DVDLO", "DVDL1", "1>INST-MSREQ", "0>BR-INV-ADR",
                     "INV-ADR>TGR", "INH-MS-PROT", "SET-KEY", "INSERT-KEY",
                     "]CE-ID>F", "]PIR>F", "SEL-MPL*E3", "TEST-AND-SET" };

/* Card D */
char *h_field[] = { "", "]T>EXTREG", "]T>PSBAR", "]T>SELREG",
                   "RG*Q0", "RG*Q2", "RG*Q4", "RG*Q6", "RF*E2|1", "WF*E2|1",
                   "RF*E2", "WF*E2", "RG*E2|1", "WG*E2|1", "RG*E2", "WG*E2",
                   "RF*E3|1", "RF*E3", "RG*E3|1", "W*E11-15", "R*E3", "x", "y",
                   "R*E11-15", "RF*R2", "RG*R2", "R*24", "W*24", "]T>DARMSK", "]ST04>ATR",
    };
                  
/* Card R */
/* Set ROSAR Bit 11 */
char *j_field[] = { "0", "1", "W-CRY", "T(32)", "JCRY28", "LS-PB", "]TCS",
             "ABC=0", "J47=0.OFLO", "J57=0", "J47=0", "BCNM|", "]LMT", "IC3=3", "~F04",
             "D2=3", "E3!1", "E3!1", "]STAC", "MCW04", "XECTGR", "STO-PB", "E(03)",
             "STC=7", "]E3.RR", "DIS-PB", "HSMOVE", "]ATRSEL", "UFLO", "M/DVD",
             "IPL]PSW", "IC-PB", "E3!4", "E2=E3", "STT-PB", "]FLT", "HLD]IN", 
             "SB-PB", "D22=1", "ABC=7", "]T(63)", "]IOCE", "]IOERR", "]STATE0",
             "]REG-SET", "]512CRY", "]SAS=18", "", "E3=255", "SAS=0", "STAB", 
             "", "RELI/O", "SAS=13", "", "STC!3", "", "", "", "", "", "", "", "",
             /* 64 */"4<E3", "ABC!3", "STAH", "CONTIN", "W=0", "TCS/ST", "",
             "3<STC", "", "", "", "", "", "", "", "", "J47=1", "STAA" "DECDIV",
             "]TIC", "STAF=C", "ROS-PB", "JCRY4", "", "", "", "", "", "", "", "", "",
             "DREG(18-23)", "NEXTINST", "DECIMAL", "IC(21-22)", "W1=(01-15)", 
             "STAE,F1SGN+", "E(04-07)>ROA", "E02-07)>ROA", "EDIT", "FLPT-UN,COMP", 
             "]W(01-02)", "LOGIC-COMP", "STAD,STAG", "J1=0/1,J18=0", "SAS2,3,4",
             "J1=0,J17=0", "", "", "", "", "", "", "", "", "FLT", "NEXT-INST*D", 
             "J(58-63)", "A1=0,J5=0", "ALT,MS-TEST", "]J47!0." };

/* Card R */
/* Set ROSAR Bit 10 */
char *k_field[] = { "0", "1", "E3=15", "E2=0/", "E2=15", "STAD", "D(21)", "]RRS",
               "STAG", "PSW39", "DEC", "STAE", "]RR.C", "W1=15", "J47!0",
               "W1=1", "E23=0", "WCRY", "E=0/", "E3=0", "E3=0/", "INTRP", "F1!1",
               "F1!9", "T>RAR", "EXCEP", "6<E23", "MOVE", "UFMSK", "", "", "SPEC"};

/* Card S */
char *l_field[] = { "", "STOP1", "STOP2", "]MS-REQ-LOG", "SET-MARK-0-7",
               "SET-MARK-0-3", "SET-MARK*STC", "SET-MARK*J61", "MS-REQ-IC-3",
               "MS-REQ*IC-4", "MS-REQ*D-3", "MS-REQ*D4", "MS*IC-3*D=11",
               "MS-REQ*SCAN4", "]RQ-XY-STO*D", "" };

/* Card B */
char *m_field[] = { "+0", "", "", "", "+]DECAB", "-U,CRY", "", "-]U", "",
                  "", "", "", "?U*E1", "+U1,U0", "", "+U1,0", "", "+U0,0", ".U",
                  "+U", "^U", "+15,U0", "|U", "+15,U1", "+6U", "+6U0,0", "+0,U1",
                  "-U", "+0,U0", "-U0,0", "+0,U" };
/* Card B */
char *n_field[] = { "0", "", "V", "0,V1", "V0,+", "QUOT", "0,V", "9,V1",
                    "-64", "", "V0,0", "-1", "V0,-", "", "V0,V0", "1"};

char *p_field[] = { ">", "SCAN-BYPASS", "R>", "",  ">HOLD", "L>", ",C8", "]R1>" };

/* Card A */
char *q_field[] = { "", "+P", "+8", "+1", "-16", "+TIME", "-8", "-1" };

/* Card b */
char *r_field[] = { "AB>U", "F>U" };
/* Card A */
char *t_field[] = { "0", "BL2", "IC", "DEC", "F1", "ABL2", "8", "B8J9",
               "A", "B8", "A13", "B", "AB", "]2", "AB17", "B489", "AB18" };

/* Card A */
char *u_field[] = { "0", "-TL1", "-D", "+D", "+S", "-DTL1", "+TL1", "+DTL1",
                   "+T45R", "+T67", "-D+7", "+DJ13", "-DT", "-T", "+T", 
                   "+DT", "+]K", "]FMT0*E13-15", "]FMTN*E13-15", "]FMTW*E13-15",
                   "+]2", "+]32", "", "", "+]T45R", "+]T67" };
/* Card A */
char *v_field[] = {"0", "E3", "E2", "E23", "Q7", "Q5", "Q3", "Q1"};

/* Card C */
char *w_field[] = { "0", "]13>ADR-SQCR", "]LMT1", "]LMT2", "SAMLE]VALUE", "]MS>LM",
             "", "", "*R]ATR2", "]LOAD-REG", "**]NO>V", "*R]GREG", "*R]EXTBUS",
             "]GEN-BUS-PAR", "**]N1>V", "]T>M" };

/* Card C */
char *sd_field[] = { "", "", "ADRSQNCR-1", "]16>ADR-SQCR", "]15>ADR>SQCR", "MSK-ADR>SAB",
                      "SCAN-SREG" };
char *sf_field[] = { "", "1>CTRCTL-TGR", "", "", "STOP.1>UNCND", "0>TIC,GAP", 
                     "1>MCH-CK TRP", "SCANOUT-TREG", "0>PASS/FAIL" };

char *sg_feild[] = { "", "SCANOUT-RTWD", "", "", "SCANOUT-LTWD", "", "", "",
                  "1>PASS/FAIL", "]INVERT-BFR1", "FLT]INITLIZE", "]7>ADR-SQCR",
                  "]MACH-RESET", "0>SCAN-MODE", "SCAN-IN", "]23>ADR-SQCR" };
void
cycle_2065()
{
    struct ROS_2065 *sal;

    switch(sal->J) {
    case 0:    /* 0 */
    case 1:    /* 1 */
    case 2:    /* if carry out of serial adder */
    case 3:    /* if T32=1 */
    case 4:    /* if PAL 28 carry = 0 */
    case 5:    /* if local store pushbutton on */
    case 6:    /* if clock step */
    case 7:    /* if ABC == 0 */
    case 8:    /* if Pal63 == 0 and (stag & F4-7==Pos or not stag & F4-7==neg) */
    csae 9:    /* if Pal 40-64==0 */
    case 10:   /* if Pal 32-63 == 0 */
    case 11:   /* if condition not met for bc or if rr format and e12-15==0 */
    case 12:   /* if limit latch is on */
    case 13:   /* If ic21-22==11 */
    case 14:   /* If f04==0 and pal carry 32 = 1 */
    case 15:   /* if D21-22 == 1 */
    case 16:   /* If e12-15 != 0001 */
    case 17:   /* if e14-15!= 11 */
    case 18:   /* stac == 0 */
    csae 19:   /* maint control word 04 =1 */
    csae 20:   /* if execute tgr = 1 */
    case 21:   /* if sotr pushbutton on */
    case 22:   /* if e03=1 */
    case 23:   /* if stc=111 */
    case 24:   /* if RR and E12-15 == 0000 */
    case 25:   /* if display pushbutton */
    case 26:   /* if e08-15 greater than 6 and abc and stc == 0 */
    case 27:   /* if receive atr select on */
    case 28:   /* if sal0=1 and ingating not inhibited or f0=1 */
    case 29:   /* if decimal mult or divide */
    csae 30:   /* If IPL or PSW restrt pushbutton */
    case 31:   /* If instr counter pushbutton */
    case 32:   /* If e12-15 not = 100 */
    case 33:   /* If e08-11 = E12-15 */
    case 34:   /* If start pushbutton */
    case 35:   /* if flt mode on */
    case 36:   /* if hold i/o line */
    case 37:   /* if main store byte pushbutton */
    case 38:   /* If D22=1 */
    case 39:   /* if ABC=111 */
    case 40:   /* if T bit 63 = 1 */
    case 41:   /* If IOCE operation */
    case 42:   /* If ioce error */
    case 43:   /* If in state 0 */
    case 44:   /* if register set pushbutton */
    case 45:   /* if carry into pal 54 */
    case 46:   /* If scan address sequencer = 18 */
    case 48:   /* If e08-15 = 1111,1111 */
    case 49:   /* If scan address sequncer = 0 */
    case 50:   /* If stab */
    case 52:   /* if Release I/O line */
    case 53:   /* if Address sequncer = 13 */
    case 55:   /* if stc != 011 */
    case 64:   /* if e12-13 does not = 00 */
    case 65:   /* if abc != 011 */
    case 66:   /* If stah != 1 (serial adder carry) */
    case 67:   /* If one of the following.
                 MCW 5 6  pass  fail
                     0 0  x     1
                     0 x  1     0 */
    case 68:   /* If SAL00-07 == 0 */
    case 69:   /* IF interrupt or time clock step or stop tgr=1 */
    case 71:   /* If stc greater then 011 */
    case 80:   /* if pal32-62 == 0 and pal63 = 1 */
    case 81:   /* if staa = 1*/
    case 82:   /* If decimal divide */
    case 83:   /* If tic or gap or ut bit) and not repeat flt */
    case 84:   /* If staf= stac (signs alike */
    case 85:   /* If ros transfer pushbutton */
    case 86:   /* If carry exist into Pal4 during flpt op */
    case 96:   /* Set ROSAR 09 if D18-21=0.
                  Set ROSAR 10 if D22=1.
                  Set ROSAR 11 if D23=1. */
    case 97:   /* Set ROSAR 06 if effective R00=1.
                  Set ROSAR 07 if effective R01=1
                  Set ROSAR 08 if effective RX format and R12-15=0000.
                  Set ROSAR 09 if not effective RR and 4 bits of Q selected by IC21,22 == 0000.
                  Set ROSAR 10 if IC21=1
                  Set ROSAR 11 if IC22=1 */

    case 98:   /* Set ROSAR 09 if E12-15 == 0000.
                  Set ROSAR 10 if E08-11 == 0000 or STC=000.
                  Set ROSAR 11 if ABC != 000 or E12-15 == 0000. */
    case 99:   /* Set ROSAR 10 if IC21=1.
                  Set ROSAR 11 if IC22=1. */
    case 100:  /* Set ROSAR 10 if SAL04-07 = 1 thru 4 or 10 thru 15. ??
                  Set ROSAR 11 if SAL04-07 = 1 thru 4 or 10 thru 15. */
    case 101:  /* Set ROSAR 10 if STAE == 1.
                  Set ROSAR 11 if F04-07 == Plus sign */
    case 102:  /* Set ROSAR 08 if E04 = 1.
                  Set ROSAR 09 if E05 = 1.
                  Set ROSAR 10 if E06 = 1.
                  Set ROSAR 11 if E07 = 1. */
    case 103:  /* Set ROSAR 06 if E02 = 1.
                  Set ROSAR 07 if E03 = 1.
                  Set ROSAR 08 if E04 = 1.
                  Set ROSAR 09 if E05 = 1.
                  Set ROSAR 10 if E06 = 1.
                  Set ROSAR 11 if E07 = 1. */
    case 104:  /* Set ROSAR 09 if ABC = 111.
                  Set ROSAR 10 if E08-15 == 0000 or STC=111.
                  Set ROSAR 11 if E07=1 and EDIT and MARK LATCH = 1. */
    case 105:  /* Set ROSAR 10 if Fltg pt unnorm.
                  Set ROSAR 11 if fltg pt compare oper */
    case 106:  /* Set ROSAR 10 if SAL bit 01 == 1.
                  Set ROSAR 11 if SAL bit 02 == 1. */
    case 107:  /* Set ROSAR 09 if SAL00-07 == 0. 
                  Set ROSAR 10 if (ABC==111 and SAL00-07 == 0) or 
                                 (SAL00-07 != 0 and carry out s adder.
                  Set ROSAR 11 if STC==111 or E08-15 == 0 */
    case 108:  /* Set ROSAR 10 if STAD==1.
                  Set ROSAR 11 if STAG==1. */
    case 109:  /* Set ROSAR 10 if PAL07-11 == 0 or PAL 06,08-11 all ones.
                  Set ROSAR 11 if PAL07-67 == 0. */
    case 110:  /* Set ROSAR 09 if scan address sequence 2 = 1.
                  Set ROSAR 10 if scan address sequence 3 = 1.
                  Set ROSAR 11 if scan address sequence 4 = 1 */
    case 111:  /* Set ROSAR 10 if PAL08-15 == 0.
                  Set ROSAR 11 if PAL07-63 == 0. */
    case 120:  /* Set ROSAR 08 if carry out of serial adder (STAH not used).
                  Set ROSAR 09 if STAC == STAF.
                  Set ROSAR 10 if Flt pt character diff not > 8 for short or 16 for long. */
    case 121:  /* Set ROSAR 06 if Effective R00=1.
                  Set ROSAR 07 if Effective R01=1.
                  Set ROSAR 08 if Effective R12-15 = 0000.
                  Set ROSAR 09 if Not Effecitve RR format and 4 bits of Q
                           selected by d21,22 == 0000.
                  Set ROSAR 10 if D21==1.
                  Set ROSAR 11 if D22==1. */
    case 122:  /* Set ROSAR 09 if PAL58-61==0000.
                  Set ROSAR 10 if PAL62 == 1.
                  Set ROSAR 11 if PAL63 == 1. */
    case 123:  /* Set ROSAR 10 if A08-11 == 0000.
                  Set ROSAR 11 if PAL40-43 == 0000. */
    case 124:  /* Set ROSAR 10 if Alternate test must be fetched.
                  Set ROSAR 11 if Test is storage FLT. */
    case 125:  /* Set ROSAR 10 if no carry from PAL32 and PAL32-63 != 0.
                  Set ROSAR 11 if carry from PAL32 and PAL32-63 != 0. */
    }

    switch(sal->K) {
    case 0:    /* 0 */
    case 1:    /* 1 */
    case 2:    /* if E12-15 == 1111 */
    case 3:    /* If E08-11 incr latches == 0000 or STC = 000 */
    case 4:    /* if E08-11 == 1111 */
    case 5:    /* if STAD */
    case 6:    /* If D21 == 1 */
    case 7:    /* if Restart timeout */
    case 8:    /* If STAG */
    case 9:    /* if PSW39 = 1, significane mask */
    case 10:   /* if E00-03 == 1111 VFL Decimal. */
    case 11:   /* if STAE */
    case 12:   /* If IC21-22 = 11 or (not rr and ic21-22 == 00) */
    case 13:   /* If SAL 04-07 == 1111 */
    case 14:   /* If e07 = 0 and pal32 not eor fixed pt overflow.
                  or e07 = 1 and pal32 eor fix pt overflow
                  or e07 = 1 and pal32-63=0. */
    case 15:   /* if SAL 04-07 == 0001. */
    case 16:   /* If E08-15 == 0000. */
    case 17:   /* If carry out of serial adder STAH Not used */
    case 18:   /* IF E08-15 == 0000 or STC==111 */
    case 19:   /* If E12-15 == 0000 */
    case 20:   /* if E12-15 == 0000 or STC=111 */
    case 21:   /* If program interrupt */
    case 22:   /* If F04-07 != 0001 */
    case 23:   /* If F04-07 != 1001 */ 
    case 24:   /* Set ROSAR 00-11 to T40-51 Scan */
    case 25:   /* If exception force address of ROSAR per source of exception */
    case 26:   /* If E08-15 > 6 */
    case 27:   /* If move operation */
    case 28:   /* If f01==1 and psw bit 38==0 or 
                     STAD==0 and PSW bit 38 == 0 and mult or dvd */
    case 31:   /* If interrupt and force addr 010 */
    }

    switch(sal->L) {
    case 0:     
    case 1:     /* Inhibit rise of next P2 for 1 cycle */
    case 2:     /* Inhibit rise of next P2 for 2 cycle */
    case 3:     /* Request logout words from storage per ABC */
    case 4:     /* Set mark 0-7 */
    case 5:     /* Set mark 0-3 */
    case 6:     /* Set marks per stc */
    case 7:     /* set marks 0-3 if PAL61 == 0,
                   set marks 4-7 if pal61 == 1 */
    case 8:     /* IC request store (3 cycle) */
    case 9:     /* IC request store (4 cycle) */
    case 10:    /* D request store (3 cycle) */
    case 11:    /* D request store (4 cycle) */
    case 12:    /* if D21-23=3 IC request store (3 cycle) turn on instruction mem tgr */
    case 13:    /* Scan request store (4 cycle ) */
    case 14:    /* Request to store XY reg (requires set marks */
    };

    /* Ingate to A,B,IC */ 
    switch (sal->A) {
    case 0:  /*       Nop */
    case 1:  /* B     PAL32-63 to B32-63 */
    case 2:  /* B,IC  PAL32-63 to B32-63 and PAL 40-63 to IC00-23 */
    case 3:  /* A,B   PAL32-63 to A00-31 and B32-63 */
    case 4:  /* AB17  PAL 0-63 to AB08-63 A0-7 untouched */
    case 5:  /* AB    PAL 4-67 to AB04-67, zero to A0-3 */
    case 6:  /* AB38  PAL24-67 to AB24-67 */
    case 7:  /* B38M  PAL24-67 to AB24-67 if PAL66,67 == 0 M1M2 to PAL64,65 */
    case 8:  /* J9B8  PAL28-31 to B64-67 (PAL24-27 must be 0000) */
    case 9:  /* MS>AB SDB0 00-63 to AB00-63 */
    case 10: /* IC    PAL40-63 to IC00-23 */
    case 11: /* A     PAL32-63 to A00-31 */
    case 12: /* J49B  PAL32-63 to B32-63, PAL 28-31 to B64-67, PAL24-27 must be 0000 */
    case 13: /* B8    PAL64-67 to B64-67 */
    case 14: /* AB18  PAL08-67 to AB08-67, A0-7 unchanged */
    case 15: /*       Nop */
             break;
    }

    /* Ingate local store to S,T */
    switch (sal->B) {
    case 0:  /* nop */
    case 1:  /* Local Store data to S00-31 */
    case 2:  /* Local store data to T32-63 */
    case 3:  /* Local store data to S00-31 and T32-63 */
    }

    /* Misc controls */                       
    switch (sal->F) {
    case 0:  /* Nop */
    case 1:  /* Reset STAA thru STAH. Set LALO if flpop, E8-11 to LAL on RR Branch.
                Reset STC and ABC to 000 (stc to 100 on RR).
                Reset 3 cycle req trg if RS branch.
                Gate for q-buffer refill on branch instructions. */
    case 2:  /* Set stat C if not vfl dec and sto=1.
                   vfl dec and sba4-7 is positive.
                   vfl dec and sba4-7 is negative and not.
                Set stat e if vfl dec and invalid sign.
                Set stat f if vfl dec and sbb sign is negative. */
    case 3:  /* Set staa if PAL 32-63=0 */
    case 4:  /* Set staa if PAL 32-63!= 0 or staa latch== 0 */
    case 5:  /* Set stab if SAL00-07==0. */
    case 6:  /* Set stat c if flpt and (sub or comp) and 
                      sbb 0== 0 or flpt and not (sub or comp) and sbb0==1.
                Set stat d if flpt (mult or div) and sal0 == 1.
                Set stat e if not flpt and sbb4-7 = invalid sign.
                Set stat f if flpt and ab0=1 or not flpt and sbb is negative */
    case 7:  /* Set stop loop (manual) trigger */
    case 8:  /* Reset stad and reset block i fetch tgr. */
    case 9:  /* Set stad */
    case 10: /* Inhibit ingating for 1 cycle to f,d,ab,st,rosar if a07=0 */
    case 11: /* Inhibit ingating for 1 cycle to f,d,ab,st,rosar, if a08-11 does not == 0000 */
    case 12: /* Selects multiplier bits from S per E12-15 and tx tgr during terminal 
                cycle of fiex and flpt mult */
    case 13: /* Read direct time out interrupt. turn on machine check and psw bit 29 */
    case 14: /* ic21-23 and abc incr latches */
    case 15: /* 000 to abc incr. latches */
    case 16: /* sets specification interrupt, psw bits 29, 30 unconditional if 9020 op. */
    case 17: /* Sets time clock at limit tgr. */
    case 18: /* set program interrupt tgrs per significance or divide chk conditions. */
    case 19: /* Set program interrupt tgr 1 (invalid op).
    case 20: /* Set supervisor call tgr and prog interrupt tgrs if supervisor call. */
    case 21: /* Set program interrupt tgrs per flpt or decimal overflow or underflow */
    case 22: /* Set staa if e06=1 and pal 32-63 == 0, gate result sign onto local
                store bus (t32 = 1 if result sign minus, =0 if result sign plus */
    case 23: /* Set ic carry latch if padder 512 carry */
    case 24: /* set addr store comp tgr if pal 32 carry */
    case 25: /* reset mach. chk supv. call interrupt tgrs interrupt
                priority tgrs. set stop tgr if instr. step. */
    case 26: /* set address store compare tgr if pal 40-63==0 or ic21,22 == 11 and
                pal 40-62 = 0 */
    case 27: /* reset addr store compare and execute tgrs. */
    case 28: /* Reset time clock step tgr, condition set of pulse mde adjust tgr. */
    case 29: /* Set timing gate tgr */
    case 30: /* reset timing gate tgr */
    case 31: /* inhibit serial adder parity check */
    case 32: /* Set condition code to zero */
    case 33: /* Set condition code to 1 */
    case 34: /* Set condition code to 2 */
    case 35: /* Set condition code to 3 */
    case 36: /* Set sta to state of stac */
    case 37: /* Set d carry latch if padder 512 carry */
    case 38: /* If not 1>inst-msreq (G22 micro-order) or unsucceessful branch on
                condition and ic21-22!= then set gate i fetch addr tgr and reset i
                fetch invalid addr tgr and invalid instruction addr trg. */
    case 39: /* set stab if pal31=1 */
    case 40: /* get result sign onto local store bus (t32 = 1 if result sign minus,
                =0 if result sign plus */
    case 41: /* set scan mode tgr */
    case 42: /* Set execute tgr and make store req. (4 cyc) per D if ss format and 
                D21-22 = 10. */
    case 43: /* pal61-63 to stc */
    case 44: /* reset stop, stop loop (manual), and pulse mode adjust tgrs, 
                block interrupts on start if not wait state. */
    case 45: /* reset stag */
    case 46: /* PAL61-63 to ABC */
    case 47: /* D21-23 to STC incr latches */
    case 48: /* ABC-1 to ABC incr latch, STC-1 to STC incr latches */
    case 49: /* ABC-1 to ABC incr latchs */
    case 50: /* STC-1 to STC incr latches */
    case 51: /* Set STC to zero. */
    case 52: /* ABC+1 to ABC, STC+1 to STC */
    case 53: /* ABC+1 to ABC latches */
    case 54: /* STC+1 to STC latches */
    case 55: /* 011 to STC latches */
    case 56: /* E13-15 to ABC and STC latches */
    case 57: /* E13-15 to ABC latches */
    case 58: /* E13-15 to STC latches */
    case 59: /* 1 to STC(0) */
    case 60: /* If stag == 1, conditiona edit hardware ctls */
    case 61: /* ABC-1 to ABC incr, STC+1 to STC incr */
    case 62: /* Set CC 1 if 
                  1) TM and STAA *Mixed*
                  2) Not STAA and result minus and
                      (NOt logical insgtr and not fxpt cmp and fxpt or
                         shift arith) *less than zero*
                  3)  (translate or edit) and staa and (e06 and s trg or not
                        e06 and not stag) *trt incomplete* *edit less than 0*
                  4) FLPT and not STAA and result minus. *less than 0*
                  5) Dec compare and (sta(a and F and H) or sta(c and f and H)
                      or (sta(a and c and not h)). *first op is low*
                  6) (Dec add or dec sub or zap) and sta(a and h)
                               *less than 0 *
                  7) ((and) or (or) or (xor)) and staa *not 0*
                  8) CLI/C (SI or SS) and STA(A and Not H) *1st low*
                  9) TS and ST32 *left bit is one *
                 10) Add or sub log and not staa and not ab31
                                                  *!=0, no carry*
                 11) Fxpt cmp and not sta(a and b) and st32 or fxpt cmp and
                      STA(not a and b) and not st32 *1st low*
                 12) SIO or TIO *csw stored*
                 13) HIO *halted*
                 14) TCH *csw ready*
              Set CC 10 if
                  1) TM and STA(A and H) *bits all one*.
                  2) Not STAA and Result + and (not log instr and not
                       fxpt cmp and fxpt or shift arith)
                                          *greater than 0*
                  3) (translate or edit) and STAA and ((Not S trg and E06) or
                       (stag and not e06) *trt complt* *edit greater than 0*
                  4) FLPT and NOT STAA and result pos *greater than 0*
                  5) Dec comp and (sta(a and not f and h) or sta(a and not c and
                     not h).
                  6) (dec add or dec sub or zap) and sta(a and not f)
                                         *greater than 0*
                  7) CLI/C and sta(a and h) *first high*
                  8) CLR (RR or RX) and nt staa and ab31 *first high*
                  9) (Add or sub) log and staa and ab31 *equal 0, carry *
                 10) fxpt cmp and nt staa and ((stab and st32) or (not stab
                        and not st32)) *first high*
                 11) sio *busy*
                 12) hio *stopped*
                 13) tio or tch *working*
             Set CC 01 and 10 if
                  1) (fxpt and not fxpt cmp or arith shift) and stab *overflow*
                  2) dec (add or sub) or zap and stab *overflow*
                  3) (add or sub log and ab31 and not staa *!=0, carry*
                  4) FP overflow
             */
    }



    switch(sal->H) {
    case 0:   /* Nop */
    case 1:   /* Gate T 32-63 to external */
    case 2:   /* Gate T 32-63 to PSBAR */
    case 3:   /* Gate T 32-63 to Select reg */
    csae 4:   /* Read gen purpose per Q00-03 set stad if Q00-03 == 0 */
    case 5:   /* Read gen purpose per Q16-19 set stad if Q16-19 == 0 */
    case 6:   /* Read gen purpose per Q32-35 set stad if Q32-35 == 0 */
    case 7:   /* Read gen purpose per Q48-51 set stad if Q48-51 == 0 */
    case 8:   /* read fp per E08-11 (LAR04=1 or E11) */
    case 9:   /* write fp per E08-11 (LAR04=1 or E11) */
    case 10:  /* read fp per E08-11  */
    case 11:  /* write fp per E08-11 */
    case 12:  /* read gen purpose per E08-11 (LAR04=1 or E11) */
    case 13:  /* write gen purpose per E08-11 (LAR04=1 or E11) */
    case 14:  /* read gen purpose per E08-11 */
    case 15:  /* write gen purpose per E08-11 */
    case 16:  /* read fp per E12-15 (LAR04=1 or E15) */
    case 17:  /* read fp per E12-15 */
    case 18:  /* read gen purpose per E12-15 (LAR04=1 or E15) */
    case 19:  /* write gen purpose per E11-15 */
    case 20:  /* read gen purpose per E12-15 */
    case 21:  /* undefined */
    case 22:  /* undefined */
    case 23:  /* write gen purpose per E11-15 */
    case 24:  /* read fp per R08-11 */
    case 25:  /* read gen purpose per R08-11 */
    case 26:  /* read gen purpose per R08-11 */
    case 27:  /* read work reg 24 */
    case 28:  /* read work reg 24 */
    case 29:  /* Gate T32-63 to DAR mask register */
    case 30:  /* Undefined */
    case 31:  /* get st00-39 to address translation register */
    case 32:  case 47: /* Read Gp register */
    case 48:  case 55: /* Read FP register */
    case 56:  case 63: /* Undefined */
    case 64:  case 95: /* 9020 specific */
    case 96:  case 111: /* write GP register */
    case 112: case 119: /* write FP register */
    }

    /* Ingating to E,R,E increment and decrement and emit */
    switch (sal->E) {
    case 0:  /* Nop */
    case 1:  /* E08-11 plus 1 to E08-11 */
    case 2:  /* E12-15 plus 1 to E12-15 */
    case 3:  /* Pal 56-63 to E08-12 */
    case 4:  /* E08-11 minus 1 to E08-11, E12-15 minus 1 to E12-15 */
    case 5:  /* E08-11 minus 1 to E08-11 */
    case 6:  /* E12-15 minus 1 to E12-15 */
    case 7:  /* E08-15 minuw 1 to E08-15 */
    case 8:  /* Constant to E12-15 (5 if fltg pt divide, 15 if flt pt or fxd pt mul, 0 otherwise */
    case 9:  /* D18-21 to E12-15 */
    case 10: /* R0-15 to E0-15 also set ILC per R0-1 */
    case 11: /* PAL 56-63 to R08-15 */
    case 12: /* Q0-15 to R0-15 */
    case 13: /* Q16-31 to R0-15 */
    case 14: /* Q32-47 to R0-15 */
    case 15: /* Q48-63 to R0-15 */
    case 16: /* Gate N byte to SBB-0-7 per W10 or W14 micro-order */
    case 17: /* 00010000 to SBB 0-7 */
    case 18: /* 00100000 to SBB 0-7 */
    case 19: /* 00110000 to SBB 0-7 */
    case 20: /* 01000000 to SBB 0-7 */
    case 21: /* 01010000 to SBB 0-7 */
    case 22: /* 01100000 to SBB 0-7 */
    case 23: /* 01110000 to SBB 0-7 */
    case 24: /* 10000000 to SBB 0-7 */
    case 25: /* 10010000 to SBB 0-7 */
    case 26: /* 10100000 to SBB 0-7 */
    case 27: /* 10110000 to SBB 0-7 */
    case 28: /* 11000000 to SBB 0-7 */
    case 29: /* 11010000 to SBB 0-7 */
    case 30: /* 11000000 to SBB 0-7 */
    case 31: /* 11110000 to SBB 0-7 */
    }



    /* Serial Adder A Side */
    switch(sal->M) {
    case 0:     /* +0 */
    case 4:     /* F05-07 set bit of AB byte to SA06 zeros to SA00-05,07 */
    case 5:     /* SBA00-07 comp to SA00-07, if carry out-save in STAH */
    case 7:     /* SBA00-07 comp to SA00-07, Hot carry. 1 to SA00 if not 9020 op */
    case 12:    /* And function if E06-07 == 00, or function if E06-07=10,
                    xor if e06-07 == 11 */
    case 13:    /* SBA00-03 to SA04-07, SBA04-07 to SA00-03 */
    case 15:    /* SBA04-07 L4 to SA00-03 (SA04-07 == 0) */
    case 17:    /* SBA00-03 to SA00-03 (SA04-07 == 0) */
    case 18:    /* And function SBA00-07 to SA00-07 */
    case 19:    /* SBA00-07 to SA00-07 */
    case 20:    /* XOR function SBA00-07 to SA00-07 */
    case 21:    /* Zone to SA00-03 ((1111 if PSW12=0, 0101 if PSW12 == 1)
                   SBA00-03 and SBA00-03 comp to SA00-03 used.
                   SBA00-03 R4 to SA04-07 */
    case 22:    /* Or function SBA00-07 to SA00-07 */
    case 23:    /* Zone to SA00-03 ((111 if PSW12=0, 0101 if PSW12=1).
                   Both SBA00-03 and SBA00-03 comp to SA00-03 used).
                   SBA04-07 to SA04-07 */
    case 24:    /* SBA00-03+6 to SA00-03.
                   SBA04-07+6 to SA04-07.
                   Serial Carry to SA07.
                   Decimal Correct 00-03, 04-07.
                   Set STAA if sum ! zero.
                   Set STAE if invalid digit.
                   Set STAH if carry, reset STAH if no carry. */
    case 25:    /* SBA00-03+6 to SA00-03
                   (SA04-07=0000), decimal correct 00-03.
                   Decimal Correct 00-03, 04-07.
                   Set STAA if sum ! zero.
                   Set STAE if invalid digit.
                   Set STAH if carry, reset STAH if no carry. */
    case 26:    /* SBA00-03+6 to SA00-03,
                   correct sign to SA04-07
                   (1100 or 1101 if PSW12=0,
                    1010 or 1011 if PSW12=1).
                   Decimal correct 00-03.
                   Set STAA if sum ! zero.
                   Set STAE if invalid digit.
                   Set STAC if SBA04-07 is Neg sign.
                   Set STAH if carry, reset STAH if no carry. */
    case 27:    /* SBA04-07 to SA04-07, SA00-03 = 0 */
    case 28:    /* SBA00-07 COMP to SA00-07, serial carry to SA07.
                   Decimal ccorrect 00-03, 04-07.
                   Set STAA if sum not zero,
                   Set STAE if invalid digit */
    case 29:    /* SBA00-03 R4 to SA04-07 (SA00-03 = 0) */
    case 30:    /* SBA00-03 comp to SA00-03, (SA04-07=0000), 
                   Carry to SA03, Decimal correct 00-03.
                   Set STAA if sum not zero,
                   Set STAE if invalid digit */
    case 31:    /* SBA01-07 to SA01-07, 0 to SA00 */
    };

    /* Serial adder B side controls */


    switch(sal->N) {
    case 0:      /* Zeros */
    case 2:      /* SBB00-07 to SB00-07 */
    case 3:      /* SBB04-07 to SB04-07, 00 to SB00-03 */
    case 4:      /* Plus sign to SB04-07 (1100 if PSW12-0, 1010 if PSW12=1),
                    SBB00-03 to SB00-03 */
    case 5:      /* B66-67 to 2 bits of SAL00-07 select by multiple set, quotient
                    insert order g20, 21 */
    case 6:      /* SBB01-07 to SB01-07, 0 to SB00 */
    case 7:      /* 1001 to SB00-03, SBB04-07 to SB04-07 */
    case 8:      /* 1100,0000 to SB00-07 */
    case 10:     /* SBB00-03 to SB00-03 zero to SB04-07 */
    case 11:     /* 1111,1111 to SB00-07 */
    case 12:     /* Minus sign to SB04-07 (1101 if PSW12=0, 1011 if PSW12=1),
                    SBB00-03 to SB00-03 */
    case 14:     /* SBB0-03 to SB00-03 and SB04-07 */
    case 15:     /* 0000,0001 to SB00-07 */
    };

    /* Outgate to serial adder inbus A side */
    if (sal->R == 0) {
        /* AB Byte (selected by ABC) to SBA00-07 */
    } else {
        /* F00-07 to SBA00-07 */
    }

    /* End ops nd ingating to serial adder to F */
    switch (sal->D) {
    case 0:  /* Nop */
    case 1:  /* Normal end op. set block I fetch trg if exceptional condition to I fetch.
                Gate interrupt priority.
                If R register op is not RR format, then gate q to lal per ic21-22,
                if rr and not branch then gate r8-11 to lal,
                if rr and branch then gate r12-15 to lal.
                check for q buffer refill and start i fetch sequence if the buffer is to be refilled. */
    case 2:  /* Early End op. Check for Q-buffer refill and start I-fetch sequence if the buffer is
                to be refilled */
    case 3:  /* Branch end op, set block I fetch trg if exceptional condition to I fetch.
                Gate interrupt priority.
                if R register op is not RR format then gate q to lal per D21-22,
                if RR and not branch then gate r8-11 to lal.
                if rr and branch gate r12-15 to lal.
                check for q buffer refill and start i fetch sequence if the buffer is to be refilled. */
    case 4:   /* Direct control 00-07 to F00-7 */
    case 5:   /* Sal 0-3 to f0-3 */
    case 6:   /* Sal 4-7 to f4-7 */
    case 7:   /* sal 0-7 to f0-7 */
    }


    /* Card A */
    /* Hot ones to Parrallel adder a side */
    switch (sal->Q) {
    case 0:       /* Nop */
    case 1:       /* Hot ones to PA26-31 (for fixed pt non logical ops propagate
                     sign (T32) per true or comp operation ) */
    case 2:       /* Hot one to PA60 (+8) */
    case 3:       /* Hot carry to PA63 (blocked if in convert to decimal if SAL 00=0) */
    case 4:       /* Hot ones to PA32-59 (-16) */
    csae 5:       /* Hot ones to PA32-52, 54, 55 (For clock update */
    case 6:       /* Hot ones to PA32-60 (-8) */
    case 7:       /* Hot ones to PA32-63 (-1) */
    };

    /* E and Q registers Parallel adder B side */
    switch(sal->V) {
    case 0:      /* 0 */
    case 1:      /* E12-15 to PB60-63 */
    case 2:      /* E08-11 to PB60-63 */
    case 3:      /* E08-15 to PB56-63 */
    case 4:      /* Q52-63 to PB52-63 */
    case 5:      /* Q36-47 to PB52-63 */
    case 6:      /* Q20-31 to PB52-63 */
    case 7:      /* Q03-15 to PB52-63 */
    };

    /* Outgates to Padder A side from S, T, D */
    switch(sal->U) {
    case 0:     /* 0 */
    case 1:     /* T32-63 comp L1 to PA31-62 (1 + Hot carry to PA63) */
    case 2:     /* D00-23 comp to PA40-63 (Hot carry to PA63), Insert ones to PA32-39 */
    case 3:     /* D00-23 to PA40-63 */
    case 4:     /* S00-31 to PA32-63 */
    case 5:     /* Ones to PA04-06, D00-23 comp L1 to PA07-30, T32-63.
                   Comp L1 to PA31-62 (1 + hot carry to PA63) */
    case 6:     /* T32-63 L1 to PA31-62 (PA63 = 0) */
    case 7:     /* D00-23 L1 to PA07-30, T32-63 L1 to PA31-62 (PA63 = 0) */
    case 8:     /* T32-47 to PA48-63, 1 to PA47 if T32=0 */
    case 9:     /* T48-63 to PA48-63, 1 to PA47 if T48 = 0 */
    case 10:    /* D00-23 comp to PA40-63 (Hot carry to PA63),
                   or ones into PA61-63, Insert Ones to PA32-39 */
    case 11:    /* D00-23 to PA08-31 */
    case 12:    /* One to PA04-07, D00-23 to PA08-31 Comp,
                   T32-63 comp to PA32-63 (hot carry to PA63) */
    case 13:    /* T32-63 comp to PA32-63 (hot carry to PA63 */
    case 14:    /* T32-63 to PA32-63 */
    case 15:    /* D00-23 to PA08-31, T32-63 tyo PA32-63 */
    case 16:    /* K00-31 to PA32-63 */
    case 17:    /* Format LM reg to XY reg (history) per E13-15 */
    case 18:    /* Format LM reg to XY reg (css-new) per E13-15 */
    case 19:    /* Format LM reg to xy reg (weather) per E14-15 */
    case 20:    /* Hot one to PA62 (effectively adds +2) */
    case 21:    /* Hot one to PA58 +32 */
    case 24:    /* T32-47 to PA48-63 */
    case 25:    /* T48-63 to PA48-63 */
    };

    /* Outgates to PADDER B side */
    switch(sal->T) {
    case 0:      /* 0 */
    case 1:      /* B32-67 L2 to PB30-65 (PB66-67 = 00) if Exp MYP, 
                     propagate sign (B32 To PB28,29 */
    case 2:      /* IC00-23 to PB40-63 */
    case 3:      /* Generate excess 6 decimal correct factor to PB28-63 for
                    convert to decimal with field U6) */
    case 4:      /* F04-07 to PB60-63 */
    case 5:      /* AB06-67 L2 to PB04-65 (PB66,67=0)*/
    case 6:      /* Hot one to PB60 +8 */
    case 7:      /* B64-67 to PB28-31 */
    case 8:      /* A00-31 to PB32-63 */
    case 9:      /* P64-67 to PB64-67 */
    case 10:     /* A08-31 to PB32-63 */
    case 11:     /* B32-63 to PB32-63 */
    case 12:     /* AB04-67 to PB04-67 (A00-03 must be zero) */
    case 13:     /* Hot one to PB62 +2 */
    case 14:     /* AB08-63 to PB08-63 */
                 /* AB08-67 to PB08-67 FL PT Add, Sub, Cmp */
    case 15:     /* B32-63 to PB32-63, B64-67 to PB28-31 */
    }


    /* Parallel adder latch control */
    switch (sal->P) {
    case 0:      /* PADDER 04-67 to PAL04-67  no shift */
    case 1:      /* PADDER 04-31, 64-67, to PAL 04-31, 64-67,
                    Scan out buss 32-63 to PAL 32-63 */
    case 2:      /* PADDER 05-63 R4 to PAL 08-67 (PADDER 04 propagates to PAL04-08 */
    case 4:      /* Hold value in PAL04-67 for 1 cycle */
    case 5:      /* PADDER 08-67 L4 to PAL04-63 0000 to PAL64-67 */
    case 6:      /* PB64-67 2' compliment to PADDER 64-67,
                    PB64-67 != 0 block hot carry to PA63 */
    case 7:      /* PADDER 32-63 R1 to PAL 33-64 zeros to PAL 32 and 48 */
    };

    /* Card D */
    /* Ingate to D,K,Q,S,T,PSW,N,G */
    switch (sal->C) {
    case 0:  /* Nop */
    case 1:  /* PSW Bits to S00-19, T32-39 */
    case 2:  /* PSW Bits to S20-31 IRQ code */
    case 3:  /* S0-07 and 16-19 to PSW (system mask), S8-15 to PSW (KEY,AMWP), T34-39 to CC,Prog mask */
    case 4:  /* PAL32-63 to T32-63 */
    case 5:  /* PAL08-31 to D00-32, PAL 32-63 to T32-63 */
    case 6:  /* PAL40-63 to T40-63 and SAL0-7 to T31-39 */
    case 7:  /* PAL32-63 to T32-63, PAL40-63 to D00-23 */
    case 8:  /* SDB0 32-63 to T32-63 */
    case 9:  /* SDB0 00-63 to ST0-63 */
    case 10: /* SDBO 00-31 to T32-63 */
    case 11: /* Address Keys to D0-23 */
    case 12: /* PAL40-63 to D0-23, SAL0-7 to ST PER STC */
    case 13: /* Undefined */
    case 14: /* SAL0-7 to ST PER STC */
    case 15: /* PAL40-63 to D023 */
    case 16: /* Reset ATR Select Latch */
    case 17: /* T34-39 to PSW (CC, Prog mask) */
    case 18: /* Data keys to ST00-63 */
    case 19: /* T32-39, 52 to Maint work 0-7,20,T53-57, To adr seq 0-4, t58-61 to flt counter 0-3,
                T62,63 to FLT clok 0,1 B to extended MCW */
    case 20: /* SDBO0-63 to Q0-63 */
    case 21: /* SDBO0-63 to Q00-63 and Two bytes of Q to R per D21,22.
                if (ROSAR 10=1, PAL32-63 T33-63. reset execute and addr store comp
                if BXH or BXLR and ROSAR10=1 */
    case 22: /* SAL0-7 to GH00-7 Direct control */
    case 23: /* Two bytes of LM to N and two bytes of Q to R per IC 21-22 */
    case 24: /* PAL08-31 to D00-23, PAL32-63 to T32-63, SAL0-7 to ST per STC */
    case 25: /* Undefined */
    case 26: /* SDBO0-31 to T32-63 if D21 = 0, SDBO32-63 to T32-64 if D21=1 */
    case 27: /* Undefined */
    case 28: /* S00-7, 16-19 to PSW00-7 and PSW 16-19 */
    case 29: /* SDBO0-31 to S0-31 */
    case 30: /* PAL32-63 to K 00-31 */
    case 31: /* SAL0-7 to N 08-15 */
    }

    switch(sal->G) {
    case 0:  /* Nop */
    case 1:  /* Set STAA if SAL 00-07 != 0 */
    case 2:  /* Set STAA if PAL 07-67 = 0 */
    case 3:  /* Set STAB if fix pt overflow */
    case 4:  /* Gate SE to F00-07 */
    case 5:  /* reset STAH (serial adder cry tgr */
    case 6:  /* Set stab if decinal overflow (0>STAD) */
    case 7:  /* Set stab if B32=1 and STAG if T32=1 */
    case 8:  /* Set stag */
    case 9:  /* Two bytes of Q to R per D21-22 */
    case 10: /* set interrupt gate tgr */ 
    case 11: /* clear interrupt gate tgr */ 
    case 12: /* Undefined */
    case 13: /* Set stah (serial adder carry tgr) */
    case 14: /* set stab if left shift overflow (
                blocks reset of stab during program interrupt) */
    case 15: /* Reset addr store comp tgr if sum 32 carry and 
                PAL 40-63 does not=0 and ic 21,22 does not = 11 or
                pal 40-62 does not = 0) and ic 21,22 = 11, and 
                execute not in progress. */
    case 16: /* Set ic 21-22 to 00 */
    case 17: /* Set ic 21-22 to 01 */
    case 18: /* Set ic 21-22 to 10 */
    case 19: /* Set ic 21-22 to 11 */
    case 20: /* Quotient bit to odd serial adder latch bit per
                e12-15. d,t to pa per multiple select */
    case 21: /* Quotient bit to even serial adder latch bit per
                e12-15. d,t to pa per multiple select. */
    case 22: /* Turn on instruction memory fetch tgr. */
    case 23: /* Reset invalid branch and instr address tgrs */
    case 24: /* set cond code if branch invalid address tg is on
                in ss format (not edit and not dec compare or e08-11=15 */
    case 25: /* turn on inhinbut storage protect tgr. */
    case 26: /* Turn on set key tgr and set stad */
    case 27: /* Turn on insert key tgr. key 00-03 to f00-03. set stad */
    csse 28: /* Gate ce indentiry bits to f06-07. zeros to f 00-05 */
    case 29: /* gate program interrupt reg to f 04-06. zeros to f 00-03,07 */
    case 30: /* Selects muliplier bits from s per e12-15 and tx tgr */
    case 31: /* storage request per d store all one in byte with mask set */
    }




    /* FAA nd Misc control lines */

    /* * order activate H64 */
    /* ** activate E16 */

    switch (sal->W) {
    case 0:     /* Nop */
    case 1:     /* Set scan addres sequencer to 13 */
    case 2:     /* Reset limit latch, then set limit latch if no carry
                   from either padder bit 32 or 48 */
    case 3:     /* Set limit latch if no carry
                   from either padder bit 32 or 48 */
    case 4:     /* Sample to se-de assignment compare on bits 0-3 of
                   f reg on satr. Check a reg 02-05 for valid ce on scon */
    case 5:     /* SDB 00-63 to LM reg 00-63 */
    case 8:     /* Gate address translation register section 2 to L.S. Buss * */
    case 9:     /* T Reg to atr on psbar, or external reg to ccr per
                   register selection switch */
    case 10:    /* Gate N00-07 to serial adder B side 00-07 ** */
    case 11:    /* Gate C00-07 to L.S. Buss bits 24-31 * */
    case 12:    /* Gate external buss to l.s. buss * */
    case 13:    /* Generate local store buss */
    case 14:    /* Gate N08-15 to serial adder B side 00-07 ** */
    case 15:    /* Gate T32-63 to LM 32-63 */
    };

    /* Scan mode */


    switch (sal->SD) {
    case 2:     /* sub 1 from address sequencer */
    case 3:     /* Set address sequencer to 16 */
    case 4:     /* Set address sequencer to 15 */
    case 5:     /* Gate mask address to SAB */
    case 6:     /* Scan out S reg */
    };


    switch (sal->SF) {
    case 1:     /* Set scan counter control tgr. */
    case 4:     /* Stop scan and ton unconditional term */
    case 5:     /* Reset TIC and GAP latches */
    case 6:     /* Set machine check interrupt */
    case 7:     /* Scan out T reg */
    case 8:     /* Reset pass or fail trgs */
    };

    switch (sal->SG) {
    case 1:     /* Scan out right indicator word */
    case 4:     /* Scan out left indicator word */
    case 8:     /* Set pass or fail trgs */
    case 9:     /* Invert the buffer 1 tgr */
    case 10:    /* Initialize at start of record */
    case 11:    /* Set address sequences to 7 */
    case 12:    /* Machine reset */
    case 13:    /* Reset scan mode tgr. */
    case 14:    /* Scan in */
    case 15:    /* Set address sequencers to 23 */
    };
}



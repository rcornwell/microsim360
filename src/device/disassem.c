/*
 * microsim360 - Various transaction tables.
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
#include <string.h>
#include <stdint.h>
#include "logger.h"

/* Opcode definitions */
#define OP_SPM            0x04     /* src1 = R1, src2 = R2 */
#define OP_BALR           0x05     /* src1 = R1, src2 = R2 */
#define OP_BCTR           0x06     /* src1 = R1, src2 = R2 */
#define OP_BCR            0x07     /* src1 = R1, src2 = R2 */
#define OP_SSK            0x08     /* src1 = R1, src2 = R2 */
#define OP_ISK            0x09     /* src1 = R1, src2 = R2 */
#define OP_SVC            0x0A     /* src1 = R1, src2 = R2 */
#define OP_BASR           0x0D     /* src1 = R1, src2 = R2 */
#define OP_MVCL           0x0E    /* 370 Move long */
#define OP_CLCL           0x0F    /* 370 Compare logical long */
#define OP_LPR            0x10     /* src1 = R1, src2 = R2 */
#define OP_LNR            0x11     /* src1 = R1, src2 = R2 */
#define OP_LTR            0x12     /* src1 = R1, src2 = R2 */
#define OP_LCR            0x13     /* src1 = R1, src2 = R2 */
#define OP_NR             0x14     /* src1 = R1, src2 = R2 */
#define OP_CLR            0x15     /* src1 = R1, src2 = R2 */
#define OP_OR             0x16     /* src1 = R1, src2 = R2 */
#define OP_XR             0x17     /* src1 = R1, src2 = R2 */
#define OP_CR             0x19     /* src1 = R1, src2 = R2 */
#define OP_LR             0x18     /* src1 = R1, src2 = R2 */
#define OP_AR             0x1A     /* src1 = R1, src2 = R2 */
#define OP_SR             0x1B     /* src1 = R1, src2 = R2 */
#define OP_MR             0x1C     /* src1 = R1, src2 = R2 */
#define OP_DR             0x1D     /* src1 = R1, src2 = R2 */
#define OP_ALR            0x1E     /* src1 = R1, src2 = R2 */
#define OP_SLR            0x1F     /* src1 = R1, src2 = R2 */
#define OP_LPDR           0x20
#define OP_LNDR           0x21
#define OP_LTDR           0x22
#define OP_LCDR           0x23
#define OP_HDR            0x24
#define OP_LRDR           0x25
#define OP_MXR            0x26
#define OP_MXDR           0x27
#define OP_LDR            0x28
#define OP_CDR            0x29
#define OP_ADR            0x2A
#define OP_SDR            0x2B
#define OP_MDR            0x2C
#define OP_DDR            0x2D
#define OP_AWR            0x2E
#define OP_SWR            0x2F
#define OP_LPER           0x30
#define OP_LNER           0x31
#define OP_LTER           0x32
#define OP_LCER           0x33
#define OP_HER            0x34
#define OP_LRER           0x35
#define OP_AXR            0x36
#define OP_SXR            0x37
#define OP_LER            0x38
#define OP_CER            0x39
#define OP_AER            0x3A
#define OP_SER            0x3B
#define OP_MER            0x3C
#define OP_DER            0x3D
#define OP_AUR            0x3E
#define OP_SUR            0x3F
#define OP_STH            0x40  /* src1 = R1, src2= A1 */
#define OP_LA             0x41  /* src1 = R1, src2= A1 */
#define OP_STC            0x42  /* src1 = R1, src2= A1 */
#define OP_IC             0x43  /* src1 = R1, src2= A1 */
#define OP_EX             0x44  /* src1 = R1, src2= A1 */
#define OP_BAL            0x45  /* src1 = R1, src2= A1 */
#define OP_BCT            0x46  /* src1 = R1, src2= A1 */
#define OP_BC             0x47  /* src1 = R1, src2= A1 */
#define OP_LH             0x48  /* src1 = R1, src2= MH */
#define OP_CH             0x49  /* src1 = R1, src2= MH */
#define OP_AH             0x4A  /* src1 = R1, src2= MH */
#define OP_SH             0x4B  /* src1 = R1, src2= MH */
#define OP_MH             0x4C  /* src1 = R1, src2= MH */
#define OP_BAS            0x4D  /* src1 = R1, src2= A1 */
#define OP_CVD            0x4E  /* src1 = R1, src2= A1 */
#define OP_CVB            0x4F  /* src1 = R1, src2= A1 */
#define OP_ST             0x50  /* src1 = R1, src2= A1 */
#define OP_N              0x54  /* src1 = R1, src2= M */
#define OP_CL             0x55  /* src1 = R1, src2= M */
#define OP_O              0x56  /* src1 = R1, src2= M */
#define OP_X              0x57  /* src1 = R1, src2= M */
#define OP_L              0x58  /* src1 = R1, src2= M */
#define OP_C              0x59  /* src1 = R1, src2= M */
#define OP_A              0x5A  /* src1 = R1, src2= M */
#define OP_S              0x5B  /* src1 = R1, src2= M */
#define OP_M              0x5C  /* src1 = R1, src2= M */
#define OP_D              0x5D  /* src1 = R1, src2= M */
#define OP_AL             0x5E  /* src1 = R1, src2= M */
#define OP_SL             0x5F  /* src1 = R1, src2= M */
#define OP_STD            0x60
#define OP_MXD            0x67
#define OP_LD             0x68
#define OP_CD             0x69
#define OP_AD             0x6A
#define OP_SD             0x6B
#define OP_MD             0x6C
#define OP_DD             0x6D
#define OP_AW             0x6E
#define OP_SW             0x6F
#define OP_STE            0x70
#define OP_LE             0x78
#define OP_CE             0x79
#define OP_AE             0x7A
#define OP_SE             0x7B
#define OP_ME             0x7C
#define OP_DE             0x7D
#define OP_AU             0x7E
#define OP_SU             0x7F
#define OP_SSM            0x80
#define OP_LPSW           0x82
#define OP_DIAG           0x83
#define OP_BXH            0x86
#define OP_BXLE           0x87
#define OP_SRL            0x88
#define OP_SLL            0x89
#define OP_SRA            0x8A
#define OP_SLA            0x8B
#define OP_SRDL           0x8C
#define OP_SLDL           0x8D
#define OP_SRDA           0x8E
#define OP_SLDA           0x8F
#define OP_STM            0x90
#define OP_TM             0x91
#define OP_MVI            0x92
#define OP_TS             0x93
#define OP_NI             0x94
#define OP_CLI            0x95
#define OP_OI             0x96
#define OP_XI             0x97
#define OP_LM             0x98
#define OP_SIO            0x9C
#define OP_TIO            0x9D
#define OP_HIO            0x9E
#define OP_TCH            0x9F
#define OP_STNSM          0xAC  /* 370 Store then and system mask */
#define OP_STOSM          0xAD  /* 370 Store then or system mask */
#define OP_SIGP           0xAE  /* 370 Signal processor */
#define OP_MC             0xAF  /* 370 Monitor call */
#define OP_STMC           0xB0  /* 360/67 Store control */
#define OP_LRA            0xB1
#define OP_370            0xB2  /* Misc 370 system instructions */
#define OP_STCTL          0xB6  /* 370 Store control */
#define OP_LCTL           0xB7  /* 370 Load control */
#define OP_LMC            0xB8  /* 360/67 Load Control */
#define OP_CS             0xBA  /* 370 Compare and swap */
#define OP_CDS            0xBB  /* 370 Compare double and swap */
#define OP_CLM            0xBD  /* 370 Compare character under mask */
#define OP_STCM           0xBE  /* 370 Store character under mask */
#define OP_ICM            0xBF  /* 370 Insert character under mask */
#define OP_MVN            0xD1
#define OP_MVC            0xD2
#define OP_MVZ            0xD3
#define OP_NC             0xD4
#define OP_CLC            0xD5
#define OP_OC             0xD6
#define OP_XC             0xD7
#define OP_TR             0xDC
#define OP_TRT            0xDD
#define OP_ED             0xDE
#define OP_EDMK           0xDF
#define OP_MVCIN          0xE8   /* 370 Move inverse */
#define OP_SRP            0xF0   /* 370 Shift and round decimal */
#define OP_MVO            0xF1
#define OP_PACK           0xF2
#define OP_UNPK           0xF3
#define OP_ZAP            0xF8
#define OP_CP             0xF9
#define OP_AP             0xFA
#define OP_SP             0xFB
#define OP_MP             0xFC
#define OP_DP             0xFD

/* Symbol tables */
typedef struct _opcode {
       uint8_t    opbase;
       const char *name;
       uint8_t    type;
} t_opcode;

#define RR       01
#define RX       02
#define RS       03
#define SI       04
#define SS       05
#define LNMSK    07
#define ONEOP    010
#define IMDOP    020
#define TWOOP    030
#define ZEROOP   040
#define OPMSK    070

t_opcode  optab[] = {
       { OP_SPM,       "SPM",  RR|ONEOP },
       { OP_BALR,      "BALR", RR },
       { OP_BCTR,      "BCTR", RR },
       { OP_BCR,       "BCR",  RR },
       { OP_SSK,       "SSK",  RR },
       { OP_ISK,       "ISK",  RR },
       { OP_SVC,       "SVC",  RR|IMDOP },
       { OP_LPR,       "LPR",  RR },
       { OP_LNR,       "LNR",  RR },
       { OP_LTR,       "LTR",  RR },
       { OP_LCR,       "LCR",  RR },
       { OP_NR,        "NR",   RR },
       { OP_OR,        "OR",   RR },
       { OP_XR,        "XR",   RR },
       { OP_CLR,       "CLR",  RR },
       { OP_CR,        "CR",   RR },
       { OP_LR,        "LR",   RR },
       { OP_AR,        "AR",   RR },
       { OP_SR,        "SR",   RR },
       { OP_MR,        "MR",   RR },
       { OP_DR,        "DR",   RR },
       { OP_ALR,       "ALR",  RR },
       { OP_SLR,       "SLR",  RR },
       { OP_LPDR,      "LPDR", RR },
       { OP_LNDR,      "LNDR", RR },
       { OP_LTDR,      "LTDR", RR },
       { OP_LCDR,      "LCDR", RR },
       { OP_HDR,       "HDR",  RR },
       { OP_LRDR,      "LRDR", RR },
       { OP_MXR,       "MXR",  RR },
       { OP_MXDR,      "MXDR", RR },
       { OP_LDR,       "LDR",  RR },
       { OP_CDR,       "CDR",  RR },
       { OP_ADR,       "ADR",  RR },
       { OP_SDR,       "SDR",  RR },
       { OP_MDR,       "MDR",  RR },
       { OP_DDR,       "DDR",  RR },
       { OP_AWR,       "AWR",  RR },
       { OP_SWR,       "SWR",  RR },
       { OP_LPER,      "LPER", RR },
       { OP_LNER,      "LNER", RR },
       { OP_LTER,      "LTER", RR },
       { OP_LCER,      "LCER", RR },
       { OP_HER,       "HER",  RR },
       { OP_LRER,      "LRER", RR },
       { OP_AXR,       "AXR",  RR },
       { OP_SXR,       "SXR",  RR },
       { OP_LER,       "LER",  RR },
       { OP_CER,       "CER",  RR },
       { OP_AER,       "AER",  RR },
       { OP_SER,       "SER",  RR },
       { OP_MER,       "MER",  RR },
       { OP_DER,       "DER",  RR },
       { OP_AUR,       "AUR",  RR },
       { OP_SUR,       "SUR",  RR },
       { OP_STH,       "STH",  RX },
       { OP_LA,        "LA",   RX },
       { OP_STC,       "STC",  RX },
       { OP_IC,        "IC",   RX },
       { OP_EX,        "EX",   RX },
       { OP_BAL,       "BAL",  RX },
       { OP_BCT,       "BCT",  RX },
       { OP_BC,        "BC",   RX },
       { OP_LH,        "LH",   RX },
       { OP_CH,        "CH",   RX },
       { OP_AH,        "AH",   RX },
       { OP_SH,        "SH",   RX },
       { OP_MH,        "MH",   RX },
       { OP_CVD,       "CVD",  RX },
       { OP_CVB,       "CVB",  RX },
       { OP_ST,        "ST",   RX },
       { OP_N,         "N",    RX },
       { OP_CL,        "CL",   RX },
       { OP_O,         "O",    RX },
       { OP_X,         "X",    RX },
       { OP_L,         "L",    RX },
       { OP_C,         "C",    RX },
       { OP_A,         "A",    RX },
       { OP_S,         "S",    RX },
       { OP_M,         "M",    RX },
       { OP_D,         "D",    RX },
       { OP_AL,        "AL",   RX },
       { OP_SL,        "SL",   RX },
       { OP_STD,       "STD",  RX },
       { OP_MXD,       "MXD",  RX },
       { OP_LD,        "LD",   RX },
       { OP_CD,        "CD",   RX },
       { OP_AD,        "AD",   RX },
       { OP_SD,        "SD",   RX },
       { OP_MD,        "MD",   RX },
       { OP_DD,        "DD",   RX },
       { OP_AW,        "AW",   RX },
       { OP_SW,        "SW",   RX },
       { OP_STE,       "STE",  RX },
       { OP_LE,        "LE",   RX },
       { OP_CE,        "CE",   RX },
       { OP_AE,        "AE",   RX },
       { OP_SE,        "SE",   RX },
       { OP_ME,        "ME",   RX },
       { OP_DE,        "DE",   RX },
       { OP_AU,        "AU",   RX },
       { OP_SU,        "SU",   RX },
       { OP_SSM,       "SSM",  SI|ZEROOP },
       { OP_LPSW,      "LPSW", SI|ZEROOP },
       { OP_DIAG,      "DIAG", SI },
       { OP_BXH,       "BXH",  RS|TWOOP },
       { OP_BXLE,      "BXLE", RS|TWOOP },
       { OP_SRL,       "SRL",  RS|ZEROOP },
       { OP_SLL,       "SLL",  RS|ZEROOP },
       { OP_SRA,       "SRA",  RS|ZEROOP },
       { OP_SLA,       "SLA",  RS|ZEROOP },
       { OP_SRDL,      "SRDL", RS|ZEROOP },
       { OP_SLDL,      "SLDL", RS|ZEROOP },
       { OP_SRDA,      "SRDA", RS|ZEROOP },
       { OP_SLDA,      "SLDA", RS|ZEROOP },
       { OP_STM,       "STM",  RS|TWOOP },
       { OP_TM,        "TM",   SI },
       { OP_MVI,       "MVI",  SI },
       { OP_TS,        "TS",   SI|ZEROOP },
       { OP_NI,        "NI",   SI },
       { OP_CLI,       "CLI",  SI },
       { OP_OI,        "OI",   SI },
       { OP_XI,        "XI",   SI },
       { OP_LM,        "LM",   RS|TWOOP },
       { OP_SIO,       "SIO",  SI|ZEROOP },
       { OP_TIO,       "TIO",  SI|ZEROOP },
       { OP_HIO,       "HIO",  SI|ZEROOP },
       { OP_TCH,       "TCH",  SI|ZEROOP },
       { OP_MVN,       "MVN",  SS },
       { OP_MVC,       "MVC",  SS },
       { OP_MVZ,       "MVZ",  SS },
       { OP_NC,        "NC",   SS },
       { OP_CLC,       "CLC",  SS },
       { OP_OC,        "OC",   SS },
       { OP_XC,        "XC",   SS },
       { OP_TR,        "TR",   SS },
       { OP_TRT,       "TRT",  SS },
       { OP_ED,        "ED",   SS },
       { OP_EDMK,      "EDMK", SS },
       { OP_MVO,       "MVO",  SS|TWOOP },
       { OP_PACK,      "PACK", SS|TWOOP },
       { OP_UNPK,      "UNPK", SS|TWOOP },
       { OP_ZAP,       "ZAP",  SS|TWOOP },
       { OP_CP,        "CP",   SS|TWOOP },
       { OP_AP,        "AP",   SS|TWOOP },
       { OP_SP,        "SP",   SS|TWOOP },
       { OP_MP,        "MP",   SS|TWOOP },
       { OP_DP,        "DP",   SS|TWOOP },
       { 0,            NULL, 0 }
};

void print_inst(uint8_t *val) {
    t_opcode        *tab;
    char            buffer[256];
    char            b2[100];

    for (tab = optab; tab->name != NULL; tab++) {
       if (tab->opbase == val[0]) {
          switch (tab->type & LNMSK) {
          case RR:  /* op, reg1:reg2 */
                    if (tab->type & IMDOP) {
                        sprintf(buffer, "%s %02x", tab->name, val[1]);
                    } else {
                        if (tab->type & ONEOP)
                            sprintf(buffer, "%s %d", tab->name, (val[1] >> 4) & 0xf);
                        else
                            sprintf(buffer, "%s %d,%d", tab->name, (val[1] >> 4) & 0xf, val[1] & 0xf);
                    }
                    break;
          case RX:  /* op, reg1:reg2 reg3:off off */
                    sprintf(buffer, "%s %d,%01x%02x(%d,%d)", tab->name, (val[1] >> 4) & 0xf,
                                val[2] & 0xf, val[3] & 0xff, val[1] & 0xf,
                                (val[2] >> 4) & 0xf);
                    break;
          case RS:  /* op, reg1:reg2 reg3:off off */
                    sprintf(buffer, "%s %d,", tab->name, (val[1] >> 4) & 0xf);
                    if ((tab->type & ZEROOP) == 0) {
                        sprintf(b2, "%d,", val[1] & 0xf);
                        strcat(buffer, b2);
                    }
                    sprintf(b2, "%01x%02x", val[2] & 0xf, val[3] & 0xff);
                    strcat(buffer, b2);
                    if (val[2] & 0xf0) {
                        sprintf( b2,  "(%d)", (val[2] >> 4) & 0xf);
                        strcat(buffer, b2);
                    }
                    break;
          case SI:
                    sprintf(buffer, "%s %01x%02x", tab->name, val[2] & 0xf, val[3] & 0xff);
                    if (val[2] & 0xf0) {
                        sprintf(b2, "(%d)", (val[2] >> 4) & 0xf);
                        strcat(buffer, b2);
                    }
                    if ((tab->type & ZEROOP) == 0) {
                        sprintf(b2, ",%02x", val[1] & 0xff);
                        strcat(buffer, b2);
                    }
                    break;
          case SS:
                    sprintf(buffer, "%s %01x%02x", tab->name, val[2] & 0xf, val[3] & 0xff);
                    if (tab->type & TWOOP) {
                       sprintf(b2, "(%d", (val[1] >> 4) & 0xf);
                    } else {
                       sprintf(b2, "(%d", val[1] & 0xff);
                    }
                    strcat(buffer, b2);
                    if (val[2] & 0xf0) {
                        sprintf(b2,",%d", (val[2] >> 4) & 0xf);
                        strcat(buffer, b2);
                    }
                    sprintf(b2, "),%01x%02x", val[4] & 0xf, val[5] & 0xff);
                    strcat(buffer, b2);
                    if (tab->type & TWOOP) {
                       sprintf(b2, "(%d,", val[1] & 0xf);
                    } else {
                       sprintf(b2, "(");
                    }
                    strcat(buffer, b2);
                    sprintf(b2, "%d)", (val[4] >> 4) & 0xf);
                    strcat(buffer, b2);
                    break;
          }
          break;
      }
   }
   if (tab->name == NULL)
       sprintf(buffer, "?%02x?", val[0]);
   log_itrace(buffer);
}



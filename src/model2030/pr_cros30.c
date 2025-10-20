
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#include "model2030.h"

struct ROS_2030 ros_2030[4096];

u_int16_t const odd_parity[256] = {
    /*    0    1    2    3    4    5    6    7 */
    /* 00x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 01x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 02x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 03x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 04x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 05x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 06x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 07x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 10x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 11x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 12x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 13x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 14x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 15x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 16x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 17x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 20x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 21x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 22x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 23x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 24x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 25x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 26x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 27x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 30x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 31x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 32x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 33x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 34x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 35x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 36x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 37x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100
};


char *ch_name[] = {
     "0", "1", "RO", "VZ", "STI", "OPI", "AC", "S0",
     "S1", "S2", "S4", "S6", "G0", "G2", "G4", "G6"};

char *cl_name[] = {
     "0", "1", "CA>W", "AI", "SVI", "R=VDD", "1BC", "Z=0",
     "G7", "S3", "S5", "S7", "G1", "G3", "G5", "INTR" };

char *cm_name[] = {
     "WRITE", "Comp", "STORE", "IJ>MN", "UV>MN", "T>MN",
     "Read CKN", "GUV>MN"};

char *cu1_name[] = {   /* Names for cm = 03-7 */
     "MS", "LS", "MPX", "MLS" };
char *cu2_name[] = {   /* Name for cm = 0-2 */
     "x", "GR", "K>W", "FWX>WX"};

char *ca_name[] = {
     "FT", "TT", "", "", "S", "H", "FI", "R",
     "D", "L", "G", "T", "V", "U", "J", "I",
     "F", "SFG", "MC", "", "C", "Q", "JI", "TI",
     "", "", "", "", "GR", "GS", "GT", "GJ"};

char *cb_name[] = {
     "R", "L", "D", "K"};

char *ck_name[] = {
     "0", "1", "2", "3", "4", "5", "6", "7",
     "8", "9", "a", "b", "c", "d", "e", "f",
     "", "UV>WX", "WRAP>Y", "WRAP>X6", "SHI", "ACFORCE",
     "Rhl", "Sll", "OE", "ASCII>X6", "INT>X6X7", "0>MC",
     "Y>WRAP", "0>IPL", "0>F", "1>F0" };

char *ckb_name[] = {
     "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
     "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"};

char *cd_name[] = {
     "Z", "TE", "JE", "Q", "TA", "H", "S", "R",
     "D", "L", "G", "T", "V", "U", "J", "I" };

char *cf_name[] = {
     "0", "L", "H", "", "Stop", "XL", "XH", "X" };

char *cg_name[] = {
     "0", "L", "H", "" };

char *cv_name[] = {
     "bin", "comp", "+2", "+3" };

char *cc_name[] = {
     "add", "+1", "and", "or", "0c", "1c", "cc", "^" };

char *cs_name[] = {
     "", "LZ>S5", "HZ>S4", "HZ>S4,LZ>S5", "0>S4,S5", "TR>S1", "0>S0", "1>S0",
     "0>S2", "ANSNZ>S2", "0>S6", "1>S6", "0>S7", "1>S7", "K>FB", "K>FA",
     "", "", "", "", "", "", "GUV>GCD", "GR>GK",
     "GR>GF", "GR>GG", "GR>GU", "GR>GV", "K>GH", "GI>GH", "K>GB", "K>GA"};

char row_label[] = { "ABCDEFGHJKLMNPQRST" };
int  ros_input[4096][256];

void
add_input(int node, int input) {
    int i;

    if (*ros_2030[input].note == '\0')
         return;
    for (i = 0; i < 255; i++) {
        /* Check if already in list */
        if (ros_input[node][i] == input)
           return;
        /* Check if empty element */
        if (ros_input[node][i] == 0) {
           ros_input[node][i] = input;
           ros_input[node][i+1] = 0;
           return;
        }
    }
    printf("Node table full %03x %s->%s\n", node, ros_2030[node].note, ros_2030[input].note);
}

int
compare(const void *a, const void *b)
{
   int *ai = (int *)a;
   int *bi = (int *)b;
   return strcmp( ros_2030[*ai].note, ros_2030[*bi].note);
}

int
main(int argc, char *argv[])
{
    char  line[200];
    char  *p, *p2;
    int   i;
    int   f;
    int   num, dig;
    char  c;
    int   base;
    int   shift;
    int   addr;
    int   ros_sort[4096];
    char  page[200][240];
    char  *curr_page;
    int   x, y;

    base = 16;
    shift = 4;
loop:
    while (fgets(line, sizeof(line), stdin) != NULL) {
        p = line;
        f = 0;
        base = 16;
        shift = 4;

        while (*p != '#') {
            num = 0;
            while (*p == ' ') p++;  /* Skip blanks */
            while (*p != ' ' && *p != '#') {
                c = *p++;
                if (c >= '0' && c <= '9')
                   dig = c - '0';
                else if (c >= 'A' && c <= 'F')
                   dig = (c - 'A') + 0xA;
                else if (c == '?')
                   dig = 0;
                if (dig >= base) {
                    fprintf(stderr, "Invalid digit %x in %s\n", dig, p);
                    goto loop;
                }
                num <<= shift;
                num += dig;
            }

            switch (f) {
            case 0:                 /* AAA */
                addr = num;
                break;
            case 1:                 /* CN */
                ros_2030[addr].CN = num & 0xFC;
                base = 2;
                shift = 1;
                break;
            case 2:                 /* CH */
                ros_2030[addr].CH = num;
                break;
            case 3:                 /* CL */
                ros_2030[addr].CL = num;
                break;
            case 4:                 /* CM */
                ros_2030[addr].CM = num;
                break;
            case 5:                 /* CU */
                ros_2030[addr].CU = num;
                break;
            case 6:                 /* CA */
                ros_2030[addr].CA = num;
                break;
            case 7:                 /* CB */
                ros_2030[addr].CB = num;
                break;
            case 8:                 /* CK */
                ros_2030[addr].CK = num;
                break;
            case 9:                 /* CD */
                ros_2030[addr].CD = num;
                break;
            case 10:                /* CF */
                ros_2030[addr].CF = num;
                break;
            case 11:                /* CG */
                ros_2030[addr].CG = num;
                break;
            case 12:                /* CV */
                ros_2030[addr].CV = num;
                break;
            case 13:                /* CC */
                ros_2030[addr].CC = num;
                break;
            case 14:                /* CS */
                ros_2030[addr].CS = num;
                break;
            case 15:                /* AA */
                ros_2030[addr].CA |= (num << 4);
                break;
            case 16:                /* AS */
                ros_2030[addr].CS |= (num << 4);
                break;
            case 17:                /* AK */
                ros_2030[addr].CK |= (num << 4);
                break;
            case 18:                /* PK */
                ros_2030[addr].PK = num;
                break;
            }
            f++;
        }
        if (*p == '#' && f != 0) {
           while (*++p == ' ');
           for (i = 0; i < 16; i++) {
              if (*p == '\n' || *p == '\0')
                 break;
              ros_2030[addr].note[i] = *p++;
           }
           ros_2030[addr].note[i] = '\0';
        }
#if 0
        i = addr;
        printf (" %03X: N=%02X %4s %5s M=%8s %4s A=%4s B=%4s K=%4s D=%4s F=%4s G=%4s V=%4s C=%4s S=%4s: %s\n", i, ros_2030[i].CN,
           ch_name[ros_2030[i].CH], cl_name[ros_2030[i].CL], cm_name[ros_2030[i].CM],
           (ros_2030[i].CM < 3) ? cu2_name[ros_2030[i].CU]: cu1_name[ros_2030[i].CU],
           ca_name[ros_2030[i].CA], cb_name[ros_2030[i].CB], ck_name[ros_2030[i].CK],
           cd_name[ros_2030[i].CD], cf_name[ros_2030[i].CF], cg_name[ros_2030[i].CG],
           cv_name[ros_2030[i].CV], cc_name[ros_2030[i].CC], cs_name[ros_2030[i].CS],

                   ros_2030[i].note);
#endif
    }

    /* Construct sort array */
    for (i = 0; i < 4096; i++) {
        ros_sort[i] = i;
        ros_input[i][0] = 0;
    }

    for (i = 0; i < 4096; i++) {
        int j;
        struct ROS_2030 *r = &ros_2030[i];
        j = r->CN;
        if (r->CM < 3 && r->CU == 2)
           j |= (r->CK & 0xf) << 8;
        else if (r->CL == 2)
           j |= ((r->CA & 0xf) << 8) | 1;
        else
           j |= (i & 0xf00);
        if (r->CH < 2) {
           if (r->CH == 1)
              j |= 2;
           if (r->CL > 2) {
              add_input(j, i);
              j |= 1;
              add_input(j, i);
           } else {
              if (r->CL == 1)
                  j |= 1;
              add_input(j, i);
           }
        } else if (r->CL < 3) {
              if (r->CL == 1 || r->CL == 2)
                  j |= 1;
              if (r->CH > 1) {
                  add_input(j, i);
                  j |= 2;
                  add_input(j, i);
              } else {
                 if (r->CH == 1)
                    j |= 2;
                 add_input(j, i);
              }
        } else {
              add_input(j, i);
              add_input(j|1, i);
              add_input(j|2, i);
              add_input(j|3, i);
        }
        /* Handle AC Force */
        if (r->CK == 0x15)
           add_input(j & 0xf00, i);

    }


    qsort(ros_sort, 4096, sizeof(int), compare);
    page[0][0] = '\0';

    curr_page = &ros_2030[4095].note[0];
    for (i = 0; i < 4096; i++) {
        int    addr = ros_sort[i];
        int    inputs[10];
        struct ROS_2030 *r = &ros_2030[addr];
        char   *p;
        int    j, k;

        p = strchr(curr_page, ':');
        if (p == NULL && r->note[0] == '\0') {
           continue;
        }
        if (p == NULL)
            p = strchr(r->note, ':');
        if (curr_page == NULL || strncmp(r->note, curr_page, p - curr_page) != 0) {
           printf(" page\n");
           /* Dump page */
           if (page[0][0] != '\0') {
              for (j = 0; j < (20 * 5); j++) {
                  for(k = 239; k > 0; k--) {
                      if (page[j][k] != ' ' || k == 1) {
                          page[j][k+1] = '\0';
                          break;
                      }
                  }
                  puts(&page[j][0]);
              }
           }
           /* Clear page */
           for (j = 0; j < 200; j++) {
               for(k = 0; k < 240; k++)
                  page[j][k] = ' ';
           }

           /* Set up top legend */
           for (j = 1; j < 11; j++)
               page[0][(j * 22)] = '0' + (j-1);
           for (j = 1; j < 19; j++) {
               page[(j * 5) - 2][0] = row_label[j-1];
           }
           curr_page = &r->note[0];
        }

        printf ("%s %03X: ", r->note, addr);
        p = strchr(r->note, ':');
        y = ((p[2] - '0') + 1) * 22;
        p2 = strchr(row_label, p[1]);
        if (p2 == NULL)
            x = 0;
        else
            x = p2 - row_label;
        x = (x * 5) + 2;

        y -= 14;

        for(k = 0; k < 14; k++) {
           page[(x)][y + k] = '-';
           page[(x)+7][y + k] = '-';
        }
        page[(x)+6][y + 14 ] = '+';
        for (j = 0; j < 7; j++) {
            page[(x) + j][(y)] = '|';
            page[(x) + j][(y) + 14] = '|';
        }
        page[(x) + 3][(y) + 15] = '*';
        sprintf(line, "%04X", addr);
        page[(x)+7][(y)] = p[1];
        page[(x)+7][(y) + 1] = p[2];
        page[(x)][(y)] = ' ';
        page[(x)][(y) + 1] = ' ';
        page[(x)][(y) + 2] = ' ';
        page[(x)][(y) + 3] = (addr & 2) ? '1': '0';
        page[(x)][(y) + 4] = (addr & 1) ? '1': '0';
        page[(x)][(y) + 5] = ' ';
        page[(x)][(y) + 10] = ' ';
        page[(x)][(y) + 11] = line[0];
        page[(x)][(y) + 12] = line[1];
        page[(x)][(y) + 13] = line[2];
        page[(x)][(y) + 14] = line[3];
        if (r->CK < 0x10) {
            if (r->PK != 0 || r->CB == 3 || r->CU == 2 || r->CM == 6) {
                page[(x)+1][(y)] = 'K';
                page[(x)+1][(y) + 1] = ' ';
                page[(x)+1][(y) + 2] = ckb_name[r->CK][0];
                page[(x)+1][(y) + 3] = ckb_name[r->CK][1];
                page[(x)+1][(y) + 4] = ckb_name[r->CK][2];
                page[(x)+1][(y) + 5] = ckb_name[r->CK][3];
                page[(x)+1][(y) + 6] = ',';
                page[(x)+1][(y) + 7] = (r->PK) ? '1': '0';
                printf("%s,%d", ckb_name[r->CK], r->PK);
            }
        } else {
            page[(x)+1][(y)] = 'K';
            page[(x)+1][(y) + 1] = ' ';
            page[(x)+1][(y) + 2] = ckb_name[r->CK & 0xf][0];
            page[(x)+1][(y) + 3] = ckb_name[r->CK & 0xf][1];
            page[(x)+1][(y) + 4] = ckb_name[r->CK & 0xf][2];
            page[(x)+1][(y) + 5] = ckb_name[r->CK & 0xf][3];
            page[(x)+5][(y)] = 'R';
            for(j = 0; ck_name[r->CK][j] != '\0'; j++)
                page[(x)+5][(y) + 2 + j] = ck_name[r->CK][j];
            printf("%s", ck_name[r->CK]);
            if (r->PK != 0)
               printf(",1");
        }
        line[0] = '\0';
        if (r->CF == 4) {
            printf(" STP");
            strcpy(line, "SP");
        } else if (r->CF == 0 && (r->CA == 0 || r->CL == 2)) {
            printf(" 0");
            strcpy(line, "0");
        } else {
            printf(" %s%s", ca_name[r->CA], cf_name[r->CF]);
            strcpy(line, ca_name[r->CA]);
            strcat(line, cf_name[r->CF]);
        }

        if (r->CG == 0 && r->CV == 0 && r->CC == 0) {
        } else {
            switch (r->CC) {
            case 0:
            case 1:
            case 4:
            case 5:
            case 6:
                     if (r->CV != 1) {
                         printf("+");
                         strcat(line, "+");
                     }
                     break;
            case 2:  printf("&"); strcat(line, "&"); break;
            case 3:  printf("|"); strcat(line, "|"); break;
            case 7:  printf("^"); strcat(line, "^"); break;
            }
            if (r->CV == 1) {
                printf("-");
                strcat(line, "-");
            }

            if (r->CG == 0) {
               printf("0");
               strcat(line, "0");
            } else {
               printf("%s", cb_name[r->CB]);
               strcat(line, cb_name[r->CB]);
            }
        }
        if (r->CG != 0) {
           printf("%s", cg_name[r->CG]);
           strcat(line, cg_name[r->CG]);
        }
        switch (r->CC) {
        case 5:
        case 1:  printf("+1"); strcat(line, "+1"); break;
        case 6:  printf("+C"); strcat(line, "+C"); break;
        }
        strcat(line, ">");
        strcat(line, cd_name[r->CD]);
        printf(">%s", cd_name[r->CD]);
        if (r->CC >= 4 && r->CC < 7) {
           printf("C");
           strcat(line, "C");
        }
        if (strcmp(line, "0>Z") == 0)
           line[0] = '\0';

        if (line[0] != '\0') {
                page[(x)+2][(y)] = 'A';
                page[(x)+2][(y) + 1] = ' ';
                for(j = 0; line[j] != '\0'; j++)
                   page[(x)+2][(y) + 2 + j] = line[j];
        }

        if (r->CV == 2) {
            page[(x)+1][(y) + 11] = 'B';
            page[(x)+1][(y) + 12] = 'I';
            page[(x)+1][(y) + 13] = 'N';
            page[(x)+1][(y) + 14] = 'A';
        }
        if (r->CV == 3) {
            page[(x)+1][(y) + 11] = 'D';
            page[(x)+1][(y) + 12] = 'E';
            page[(x)+1][(y) + 13] = 'C';
            page[(x)+1][(y) + 14] = 'A';
        }

        line[0] = '\0';
        if (r->CM < 3 && r->CU == 2) {
            if (r->CM != 1)
                strcpy(line, cm_name[r->CM]);
            page[(x)+3][(y) + 10] = 'K';
            page[(x)+3][(y) + 11] = '>';
            page[(x)+3][(y) + 12] = 'W';
            page[(x)+3][(y) + 13] = ' ';
            page[(x)+3][(y) + 14] = 'R';
            printf("  %s(%x>W) %02x %s %s ", cm_name[r->CM], r->CK, r->CN, ch_name[r->CH], cl_name[r->CL]);
        } else if (r->CM == 6)  {
            sprintf(line, "*%02X", 0x88 | ((r->CN & 0x80) >> 2) | ((r->CK & 0x08) << 1) | (r->CK & 0x7));
            printf("  %x(%s) %02x %s %s ", 0x88 | ((r->CN & 0x80) >> 2) | ((r->CK & 0x08) << 1) |
                 (r->CK & 0x7), (r->CM < 3) ? cu2_name[r->CU]: cu1_name[r->CU],
                       r->CN, ch_name[r->CH], cl_name[r->CL]);
            j = 13 - strlen(cu1_name[r->CU]);
            for(k = 0; cu1_name[r->CU][k] != '\0'; k++, j++)
                page[(x)+3][(y) + j] = cu1_name[r->CU][k];
            page[(x)+3][(y) + 13] = ' ';
            page[(x)+3][(y) + 14] = 'S';
        } else {
            if (r->CM != 1)
               strcpy(line, cm_name[r->CM]);
            if (r->CM < 3 && r->CU != 0) {
                j = 12 - strlen(cu2_name[r->CU]);
                for(k = 0; cu1_name[r->CU][k] != '\0'; k++, j++)
                    page[(x)+3][(y) + j] = cu2_name[r->CU][k];
                page[(x)+3][(y) + 13] = ' ';
                page[(x)+3][(y) + 14] = 'R';
            } else if (r->CM >= 3) {
                j = 13 - strlen(cu1_name[r->CU]);
                for(k = 0; cu1_name[r->CU][k] != '\0'; k++, j++)
                    page[(x)+3][(y) + j] = cu1_name[r->CU][k];
                page[(x)+3][(y) + 13] = ' ';
                page[(x)+3][(y) + 14] = 'S';
            }
            printf("  %s(%s) %02x %s %s ", cm_name[r->CM], (r->CM < 3) ? cu2_name[r->CU]: cu1_name[r->CU],
                       r->CN, ch_name[r->CH], cl_name[r->CL]);
        }
        page[(x)+6][(y) + 14] = '0' + r->CU;
        page[(x)+5][(y) + 14] = '0' + r->CM;

        if (line[0] != '\0') {
                page[(x)+3][(y)] = 'S';
                page[(x)+3][(y) + 1] = ' ';
                for(j = 0; line[j] != '\0'; j++)
                   page[(x)+3][(y) + 2 + j] = line[j];
        }

        j = r->CN;
        if (r->CH == 1)
           j |= 2;
        if (r->CL == 1 || r->CL == 2)
           j |= 1;
        sprintf(line, "%02XR", j);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+6][(y) + 12 + j] = line[j];

        if (cs_name[r->CS][0] != '\0') {
            page[(x)+4][(y)] = 'C';
            for(j = 0; cs_name[r->CS][j] != '\0'; j++)
                page[(x)+4][(y) + 2 + j] = cs_name[r->CS][j];
        }

        if (r->CL == 2)
            sprintf(line, "R %s,CA%02X>W", ch_name[r->CH], r->CA);
        else
            sprintf(line, "R %s,%s", ch_name[r->CH], cl_name[r->CL]);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+6][(y) + j] = line[j];

        page[(x)+7][(y) + 7] = ' ';
        page[(x)+7][(y) + 8] = ' ';
        if (r->CH < 2) {
            page[(x)+7][(y) + 10] = '0' + r->CH;
        } else {
            page[(x)+7][(y) + 10] = '*';
        }
        if (r->CL < 2) {
            page[(x)+7][(y) + 9] = '0' + r->CL;
        } else {
            page[(x)+7][(y) + 9] = '*';
        }

        if (r->CS != 0)
            printf("%s ", cs_name[r->CS]);
        j = r->CN;
        if (r->CM < 3 && r->CU == 2)
           j |= (r->CK & 0xf) << 8;
        else if (r->CL == 2)
           j |= ((r->CA & 0xf) << 8) | 1;
        else
           j |= (addr & 0xf00);
        if (r->CH < 2) {
           if (r->CH == 1)
              j |= 2;
           if (r->CL > 2) {
              printf("%s %03x, ", ros_2030[j].note, j);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j].note[k] == '\0')
                       break;
                   page[(x)+3][(y) + 14 + k] = ros_2030[j].note[k];
              }
              j |= 1;
              printf("%s %03x", ros_2030[k].note, j);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j].note[k] == '\0')
                       break;
                   page[(x)+4][(y) + 14 + k] = ros_2030[j].note[k];
              }
           } else {
              if (r->CL == 1 || r->CL == 2)
                  j |= 1;
              printf("%s %03x", ros_2030[j].note, j);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j].note[k] == '\0')
                       break;
                   page[(x)+4][(y) + 14 + k] = ros_2030[j].note[k];
              }
           }
        } else if (r->CL < 3) {
              if (r->CL == 1 || r->CL == 2)
                  j |= 1;
              if (r->CH > 1) {
                  printf("%s %03x, ", ros_2030[j].note, j);
                  for (k = 1; k < 8; k++) {
                       if (ros_2030[j].note[k] == '\0')
                           break;
                       page[(x)+3][(y) + 14 + k] = ros_2030[j].note[k];
                  }
                  j |= 2;
                  printf("%s %03x", ros_2030[j].note, j);
                  for (k = 1; k < 8; k++) {
                       if (ros_2030[j].note[k] == '\0')
                           break;
                       page[(x)+4][(y) + 14 + k] = ros_2030[j].note[k];
                  }
              } else {
                 if (r->CH == 1)
                    j |= 2;
                 printf("%s %03x", ros_2030[j].note, j);
                 for (k = 1; k < 8; k++) {
                      if (ros_2030[j].note[k] == '\0')
                          break;
                      page[(x)+4][(y) + 14 + k] = ros_2030[j].note[k];
                 }
              }
        } else {
              printf("%s %03x, ", ros_2030[j].note, j);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j].note[k] == '\0')
                       break;
                   page[(x)+3][(y) + 14 + k] = ros_2030[j].note[k];
              }
              printf("%s %03x, ", ros_2030[j|1].note, j|1);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j|1].note[k] == '\0')
                       break;
                   page[(x)+4][(y) + 14 + k] = ros_2030[j|1].note[k];
              }
              printf("%s %03x, ", ros_2030[j|2].note, j|2);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j|2].note[k] == '\0')
                       break;
                   page[(x)+5][(y) + 14 + k] = ros_2030[j|2].note[k];
              }
              printf("%s %03x", ros_2030[j|3].note, j|3);
              for (k = 1; k < 8; k++) {
                   if (ros_2030[j|3].note[k] == '\0')
                       break;
                   page[(x)+6][(y) + 14 + k] = ros_2030[j|3].note[k];
              }
        }

        printf(" from: ");
        for (j = 0; ros_input[addr][j] != 0; j++) {
            printf("%s(%03X), ", ros_2030[ros_input[addr][j]].note, ros_input[addr][j]);
        }
        sprintf(line, "Input=%d", j);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+8][(y) + j] = line[j];

        /* Find inputs on same page */
    printf(" On page %s ", curr_page);
        for(j = 0; ros_input[addr][j] != 0; j++) {
            struct ROS_2030 *q = &ros_2030[ros_input[addr][j]];
            p = strchr(q->note, ':');
            strncpy(line, q->note, p - q->note);
            line[p - q->note] = '\0';
            printf("%s(%03X)", line, ros_input[addr][j]);
            if (strncmp(q->note, curr_page, p - q->note) == 0) {
                printf("+");
            }
            printf(" ");
        }
        printf("\n");
    }

    /* Fill in the display values */
    for (i = 0; i < 4096; i++) {
        struct ROS_2030 *r = &ros_2030[i];
        int         t = (i >> 8) & 0x1f;
        int         x;

        /* Shift starts at 23 */
        /*  P on CN, ADR P, P W, P X*/
        r->row1 = odd_parity[i & 0xff] | (i & 0xff);  /* X */
        r->row1 |= (((odd_parity[t] != 0) ? 0x20 : 0) | t) << 9;   /* W */
        r->row1 |= (((odd_parity[r->CN] != 0) ? 0x40 : 0) | r->CN) << 17;

        /* Shift starts at 26 */
        /* SA, PK */
                                                      /* CK */
        r->row2 = (r->CK & 0xf) | (r->PK << 4) | ((r->CK & 0x10) << 1);
        r->row2 |= (r->CU << 6) | (r->CM << 8) | (r->CB << 11);
        r->row2 |= (r->CA << 13) | (r->CL << 18) | (r->CH << 22);
        x = odd_parity[r->row2 & 0xff];
        x ^= odd_parity[(r->row2 >> 8) & 0xff];
        x ^= odd_parity[(r->row2 >> 16) & 0xff];
        x ^= odd_parity[(r->row2 >> 24) & 0xff];
        r->row2 |= (x != 0) << 25;
        /* Shift starts at 19 */
        /* CR */
        r->row3 = (r->CS) | (r->CC << 5) | (r->CV << 8) | (r->CG << 10);
        r->row3 |= (r->CF << 12) | (r->CD << 15);
        x = odd_parity[r->row3 & 0xff];
        x ^= odd_parity[(r->row3 >> 8) & 0xff];
        x ^= odd_parity[(r->row3 >> 16) & 0xff];
        r->row3 |= (x != 0) << 19;
    }



#if 0
    printf("/*  CN   CH   CL   CM   CU    CA   CB    CK   CD    CF  CG   CV   CC    CS   PK        R1        R2        R3  Note  */\n");
    for (i = 0; i < 4096; i++) {
        struct ROS *r = &ros_2030[i];
                 /* CN     CH    CL    CM    CU    CA     CB    CK */
        printf("{ 0x%02x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%02x, 0x%x, 0x%02x, ",
                  r->CN, r->CH, r->CL, r->CM, r->CU, r->CA, r->CB, r->CK);
                /* CD  CF    CG   CV    CC    CS    PK   */
        printf("0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%02x, 0x%x, ",
                  r->CD, r->CF, r->CG, r->CV, r->CC, r->CS, r->PK);
        printf("0x%06x, 0x%06x, 0x%06x, \"%s\" }, \n",
                  r->row1, r->row2, r->row3, (r->note[0] == '\0') ? " ": r->note);
#if 0
        printf (" %03X: N=%02X %4s %5s M=%8s %4s A=%4s B=%4s K=%4s D=%4s F=%4s G=%4s V=%4s C=%4s S=%4s: %s\n", i, ros_2030[i].CN,
           ch_name[ros_2030[i].CH], cl_name[ros_2030[i].CL], cm_name[ros_2030[i].CM],
           (ros_2030[i].CM < 3) ? cu2_name[ros_2030[i].CU]: cu1_name[ros_2030[i].CU],
           ca_name[ros_2030[i].CA], cb_name[ros_2030[i].CB], ck_name[ros_2030[i].CK],
           cd_name[ros_2030[i].CD], cf_name[ros_2030[i].CF], cg_name[ros_2030[i].CG],
           cv_name[ros_2030[i].CV], cc_name[ros_2030[i].CC], cs_name[ros_2030[i].CS],

                   ros_2030[i].note);
#endif
    }
#endif
}


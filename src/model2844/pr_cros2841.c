#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>


struct _ros_2844 {
      int    ca;   /* A bus input, includes aa */
      int    cb;   /* B bus input */
      int    ck;   /* Constant */
      int    cl;   /* X7 input select */
      int    ch;   /* X6 input select */
      int    pa;   /* Parity of address */
      int    ps;   /* Parity of CA,CB,CK,CL,CA ALT, PA, CH */
      int    cn;   /* Next address */
      int    pn;   /* Next address parity */
      int    cd;   /* Destination register, includes cda */
      int    cv;   /* Invert B input */
      int    cc;   /* Alu function */
      int    cs;   /* Status */
      int    pc;   /* Parity of CD,CD Alternate, CV, CC, CS, BP */
      int    bp;   /* Bypass ALU */
      char   note[20];
      char   ec[20];
} ros_2844[4096];

                      /* 0    1     2      3    4     5     6     7  */
char *ca_name[32] =  { "0",  "GL", "BY", "BX", "FR", "KL", "DL", "DH",
                       "OP", "GP", "UR", "DW", "DR", "ER", "IE", "IH",
                       "SW","STP", "12", "13", "14", "15", "16", "17",
                       "18", "19", "1A", "1B", "SC", "FS", "OA", "IS"};

char *cb_name[4] = { "0", "BY", "CK", "DR" };

                      /* 0    1     2      3     4      5      6      7 */
char *cl_name[16] = {   "0", "1", "ST3", "ST5", "ST7", "D=0", "A>X", "TY1",
                      "SERVO", "SORSP", "SELTO", "OP1", "OP3", "OP5", "Index", "OP7" };

                     /* 0    1     2      3     4      5      6      7 */
char *ch_name[16] = {  "0", "1", "ST0", "OP6", "ST2", "ST4", "ST6", "TY0",
                      "CK>W", "Carry", "COMMD", "SUPPO", "", "OP0", "OP2", "OP4"};

                     /* 0    1     2     3     4     5     6     7 */
char *cd_name[32] =  { "D", "GL", "BY", "BX", "FR", "KL", "DL", "DH",
                     /* 8    9     10    11    12    13    14    15 */
                      "OP", "GP", "UR", "DW", "DR", "FT", "FC", "IG",
                      "SW", "11", "12", "13", "14", "15", "16", "17",
                      "18", "19", "1A", "1B", "1C", "1D", "1E", "1F"};

                     /* 0       1          2         3         4          5       6         7 */
char *cs_name[16] = { "",      "0->ST0", "1->ST0", "0->ST1", "1->ST1", "0->ST2", "DNST21", "0->ST3",
                     "1->ST3", "0->ST4", "0->ST5", "1->ST5", "0->ST6", "1->ST6", "0->ST7", "1->ST7" };

int  ros_input[4096][256];

void
add_input(int node, int input) {
    int i;

    if (*ros_2844[input].note == '\0')
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
    printf("Node table full %03x %s->%s\n", node, ros_2844[node].note, ros_2844[input].note);
}

int
compare(const void *a, const void *b)
{
   int *ai = (int *)a;
   int *bi = (int *)b;
   return strcmp( ros_2844[*ai].note, ros_2844[*bi].note);
}

int
main(int argc, char *argv[])
{

    char   line[200];
    int    ln = 0;
    char   *p;
    int    i, j, b;
    int    fld;
    int    io;
    int    addr1, addr2, addr3;
    char   *q;
    char   note[20];
    u_int32_t  bits[4];
    int    parity;
    int    addr;
    int    ros_sort[4096];
    char   page[100][241];
    char   *curr_page;
    int    x, y;

loop:
    while (fgets(line, sizeof(line), stdin) != NULL) {
        ln++;
        /* Ignore lines that are blank */
        if (line[0] == ' ')
            goto loop;
        addr1 = addr2 = 0;

        /* Grab first address */
        for (p = line; *p != ' ' && *p != '\0'; p++) {
            if (!isxdigit(*p))
                break;
            addr1 <<= 4;
            if (*p >= '0' && *p <= '9')
                addr1 += (*p - '0');
            else if (tolower(*p) >= 'a' && tolower(*p) <= 'f')
                addr1 += (*p - 'a') + 0xa;
        }
        if (*p == '\n')
            goto loop;
        /* Grab next address */
        for (i = 0; i < 12 && *p != '\0'; p++) {
            if (*p == '0') {
               addr2 <<= 1;
               i++;
            } else if (*p == '1') {
               addr2 = (addr2 << 1) | 1;
               i++;
            } else if (*p != ' ') {
               break;
            }
        }
        if (*p == '\n')
            goto loop;
        if (i != 12) {
           printf("Address2 not complete %d %d %03x %03x %s", ln, i, addr1, addr2, line);
           goto loop;
        }
        if (addr1 != addr2) {
           printf("Address not match %d %03x %03x %s", ln, addr1, addr2, line);
           goto loop;
        }

        /* Skip a blank */
        while (*p == ' ') p++;
        q = ros_2844[addr1].note;
        if (*p != '-') {
            /* Grab sheet and box */
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            *q++ = '-';
            while (*p == ' ') p++;
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            while (*p == ' ') p++;
        }
        p++;
        *q++ = '\0';
        while (*p == ' ') p++;
        /* Grab rest of line */
        j = b = 0;
        fld = 0;
        parity = 1;
        for (;*p != '\0' && *p != '\n' && j < 18; p++) {
            if (*p == '0') {
               b++;
               fld <<= 1;
            } else if (*p == '1') {
               b++;
               fld = (fld << 1) | 1;
               parity ^= 1;
            } else if (*p == ' ') {
               switch (j) {
               case 0:    ros_2844[addr1].ca = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 1:    ros_2844[addr1].cb = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 2:    j++;
                          break;
               case 3:    ros_2844[addr1].ck = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 4:    ros_2844[addr1].cl = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 5:    ros_2844[addr1].ch = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 6:    ros_2844[addr1].pa = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 7:    ros_2844[addr1].ps = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 8:    ros_2844[addr1].cn = fld << 2;
                          j++;
                          b = fld = 0;
                          break;
               case 9:    ros_2844[addr1].pn = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 10:   ros_2844[addr1].cd = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 11:   ros_2844[addr1].cd |= (fld) ? 0x10 : 0;
                          j++;
                          b = fld = 0;
                          break;
               case 12:   ros_2844[addr1].cv = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 13:   ros_2844[addr1].cc = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 14:   ros_2844[addr1].cs = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 15:   ros_2844[addr1].pc = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 16:   ros_2844[addr1].ca |= (fld) ? 0x10 : 0;
                          j++;
                          b = fld = 0;
                          break;
               case 17:   ros_2844[addr1].bp = fld;
                          j++;
                          b = fld = 0;
                          break;
               }
               /* Skip blanks */
               while (p[1] == ' ') p++;
            } else {
               printf("invalid char %d %c %s\n", ln, *p, line);
            }
        }

        parity = 1;
        for (i = 0; i < 13; i++)
            parity ^= (addr1 >> i) & 1;
        if (parity != ros_2844[addr1].pa)
              printf("PA parity error %d %d %s", parity, ros_2844[addr1].pa, line);
        parity = 1;
        for (i = 0; i < 8; i++) {
            parity ^= (ros_2844[addr1].ck >> i) & 1;
            if (i < 5)
                parity ^= (ros_2844[addr1].ca >> i) & 1;
            if (i < 4) {
                parity ^= (ros_2844[addr1].cl >> i) & 1;
                parity ^= (ros_2844[addr1].ch >> i) & 1;
            }
            if (i < 2)
                parity ^= (ros_2844[addr1].cb >> i) & 1;
        }
        parity ^= ros_2844[addr1].pa;
        if (parity != ros_2844[addr1].ps)
              printf("PS parity error %d %d %s", parity, ros_2844[addr1].ps, line);
        parity = 1;
        for (i = 2; i < 8; i++)
            parity ^= (ros_2844[addr1].cn >> i) & 1;
        if (parity != ros_2844[addr1].pn)
              printf("PN parity error %d %d %x %s", parity, ros_2844[addr1].pn, ros_2844[addr1].cn, line);
        parity = 1;
        for (i = 0; i < 5; i++) {
            parity ^= (ros_2844[addr1].cd >> i) & 1;
            if (i < 4)
                parity ^= (ros_2844[addr1].cs >> i) & 1;
            if (i < 3)
                parity ^= (ros_2844[addr1].cc >> i) & 1;
        }
        parity ^= ros_2844[addr1].cv;
        parity ^= ros_2844[addr1].bp;
        if (parity != ros_2844[addr1].pc)
              printf("PC parity error %d %d %d %s", ros_2844[addr1].bp, parity, ros_2844[addr1].pc, line);
     }

 printf("Finish\n");

    /* Construct sort array */
    for (i = 0; i < 4096; i++) {
        ros_sort[i] = i;
        ros_input[i][0] = 0;
    }

    for (i = 0; i < 4096; i++) {
        int j;
        struct _ros_2844 *r = &ros_2844[i];
        j = r->cn;
        if (r->ch == 8)
           j |= (r->ck & 0xf) << 8;
        else
           j |= (i & 0xf00);
        if (r->ch < 2 || r->ch == 8) {
           if (r->ch == 1)
              j |= 2;
           if (r->cl > 1) {
              add_input(j, i);
              j |= 1;
              add_input(j, i);
           } else {
              if (r->cl == 1)
                  j |= 1;
              add_input(j, i);
           }
        } else if (r->cl < 2) {
              if (r->cl == 1)
                  j |= 1;
              if (r->ch > 1) {
                  add_input(j, i);
                  j |= 2;
                  add_input(j, i);
              } else {
                 if (r->ch == 1)
                    j |= 2;
                 add_input(j, i);
              }
        } else {
              add_input(j, i);
              add_input(j|1, i);
              add_input(j|2, i);
              add_input(j|3, i);
        }
    }

    qsort(ros_sort, 4096, sizeof(int), compare);
    page[0][0] = '\0';

    curr_page = &ros_2844[4095].note[0];
 printf("sorted\n");

     for (addr1 = 0; addr1 < 4096; addr1++) {
        int    addr = ros_sort[addr1];
        int    inputs[10];
        struct _ros_2844 *r = &ros_2844[addr];
        char   *p;
        int    j, k;

        if (addr1 == 4095)
             goto last;
        p = strchr(curr_page, '-');
        if (p == NULL && r->note[0] == '\0') {
           continue;
        }
        if (p == NULL)
            p = strchr(r->note, '-');
        if (curr_page == NULL || strncmp(r->note, curr_page, p - curr_page) != 0) {
last:
           printf(" page\n");
           /* Dump page */
           if (page[0][0] != '\0') {
              for (j = 0; j < 100; j++) {
//                  for(k = 239; k > 0; k--) {
 //                     if (page[j][k] != ' ' || k == 2) {
  //                        page[j][k+1] = '\0';
   //                       break;
    //                  }
     //             }
                  puts(&page[j][0]);
              }
           }
           /* Clear page */
           for (j = 0; j < 100; j++) {
               for(k = 0; k < 220; k++)
                  page[j][k] = ' ';
               page[j][k] = '\0';
           }

           /* Set up top legend */
           for (j = 1; j < 10; j++)
               page[0][(j * 23)] = '0' + (j);
           for (j = 1; j < 19; j++)
               page[(j * 5) - 2][0] = 'A' + (j-1);
           curr_page = &r->note[0];
        }

        printf ("%s %03x: ", r->note, addr);
        printf("%s", ca_name[r->ca]);

        switch (r->cc) {
        case 0:
        case 1:
        case 4:
        case 5:
        case 6:  printf("+"); break;
        case 2:  printf("&"); break;
        case 3:  printf("|"); break;
        case 7:  printf("^"); break;
        }
        if (r->cv == 1)
            printf("-");

        if (r->cb == 2)
           printf("%02x", r->ck);
        else
           printf("%s", cb_name[r->cb]);
        switch (r->cc) {
        case 5:
        case 1:  printf("+1");  break;
        case 6:  printf("+C"); break;
        }
        printf("->%s", cd_name[r->cd]);
        if (r->cc > 3 && r->cc < 7)
            printf("C");
        if (r->bp)
            printf(" BYPASS");

        printf(" %02x %s", r->ck, cs_name[r->cs]);
        if (r->ch == 8)
            printf(" %02x %x>W %s ", r->cn, r->ck & 0xf, cl_name[r->cl]);
        else
            printf(" %02x %s %s ", r->cn, ch_name[r->ch], cl_name[r->cl]);
        addr3 = r->cn;
        if (r->ch == 8)
           addr3 |= (r->ck & 0xf) << 8;
        else
           addr3 |= (addr & 0xf00);
        if (r->ch < 2 || r->ch == 8) {
           if (r->ch == 1)
              addr3 |= 2;
           if (r->cl < 2) {
              if (r->cl == 1)
                 addr3 |= 1;
              printf("%s %03x", ros_2844[addr3].note, addr3);
           } else {
              printf("%s %03x, ", ros_2844[addr3].note, addr3);
              addr3 |= 1;
              printf("%s %03x", ros_2844[addr3].note, addr3);
           }
        } else if (r->cl < 2) {
              if (r->ch > 1) {
                  printf("%s %03x, ", ros_2844[addr3].note, addr3);
                  addr3 |= 1;
                  printf("%s %03x", ros_2844[addr3].note, addr3);
              } else {
                 if (r->cl == 1)
                    addr3 |= 1;
                 printf("%s %03x", ros_2844[addr3].note, addr3);
              }
        } else {
              printf("%s %03x, ", ros_2844[addr3].note, addr3);
              addr3 |= 1;
              printf("%s %03x, ", ros_2844[addr3].note, addr3);
              addr3 &= ~1;
              addr3 |= 2;
              printf("%s %03x, ", ros_2844[addr3].note, addr3);
              addr3 |= 1;
              printf("%s %03x", ros_2844[addr3].note, addr3);
        }
        printf(" from: ");
        for (j = 0; ros_input[addr][j] != 0; j++) {
            printf("%s(%03X), ", ros_2844[ros_input[addr][j]].note, ros_input[addr][j]);
        }

        printf("\n");
        p = strchr(r->note, '-');
        p++;
        x = ((toupper(*p) - 'A') * 5) + 1;
        y = ((p[1] - '0')) * 23;

        y -= 8;

        for(k = 0; k < 14; k++) {
           page[(x)][y + k] = '-';
           page[(x)+6][y + k] = '-';
        }
        page[(x)+6][y + 14 ] = '+';
        for (j = 0; j < 6; j++) {
            page[(x) + j][(y)] = '|';
            page[(x) + j][(y) + 14] = '|';
        }
        page[(x) + 3][(y) + 15] = '*';
        sprintf(line, "%04X", addr);
        page[(x)+6][(y)] = *p;
        page[(x)+6][(y) + 1] = p[1];
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

        /* Print emit field */
        if ((r->ck & 0xff) != 0 || r->cb == 2 || r->ch == 8) {
            page[(x)+1][(y)] = 'E';
            page[(x)+1][(y) + 1] = ' ';
            for (k = 7; k >= 0; k--) {
                page[(x)+1][(y) + 2 + (7-k)] = (r->ck & (1<<(k)))? '1':'0';
            }
        }
        if (r->ca == 0 && (r->cc != 2 && r->cc != 3 && r->cc != 7) && r->cv == 0) {
            line[0] = '\0';
        } else {
            strcpy(line, ca_name[r->ca]);

            switch (r->cc) {
            case 0:
            case 1:
            case 4:
            case 5:
            case 6:  strcat(line, "+"); break;
            case 2:  strcat(line, "&"); break;
            case 3:  strcat(line, "|"); break;
            case 7:  strcat(line, "^"); break;
            }
        }
        if (r->cv == 1)
            strcat(line, "-");
        if (r->cb == 2)
            sprintf(&line[strlen(line)], "%d", r->ck);
        else
            strcat(line, cb_name[r->cb]);
        switch (r->cc) {
        case 5:
        case 1:  strcat(line, "+1");  break;
        case 6:  strcat(line, "+C"); break;
        }
        strcat(line, ">");
        strcat(line, cd_name[r->cd]);
        if (r->cc > 3 && r->cc < 7)
            strcat(line, "C");
        if (line[0] != '\0') {
            page[(x)+2][(y)] = 'A';
            page[(x)+2][(y) + 1] = ' ';
            for(j = 0; line[j] != '\0'; j++)
               page[(x)+2][(y) + 2 + j] = line[j];
            if (r->bp) {
                page[(x)+2][(y) + 11] = 'B';
                page[(x)+2][(y) + 12] = 'Y';
            }
        }
        if (cs_name[r->cs][0] != '\0') {
            page[(x)+4][(y)] = 'C';
            for(j = 0; cs_name[r->cs][j] != '\0'; j++)
                page[(x)+4][(y) + 2 + j] = cs_name[r->cs][j];
        }

        if (r->ch != 8 || r->cl != 0) {
            sprintf(line, "R %s,%s", ch_name[r->ch], cl_name[r->cl]);
            for(j = 0; line[j] != '\0'; j++)
                page[(x)+5][(y) + j] = line[j];
        }

        sprintf(line, "%02XR", r->cn);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+5][(y) + 12 + j] = line[j];
        if (r->ch == 8) {
            sprintf(line, "R %X>W", r->ck & 0xf);
            for(j = 0; line[j] != '\0'; j++)
               page[(x)+4][(y) + j] = line[j];
        }
        page[(x)+6][(y) + 7] = ' ';
        if (r->cd == 13 || r->cd == 14)
            page[(x)+6][(y) + 8] = ((r->cn & 04) != 0) ? '1':'0';
        else
            page[(x)+6][(y) + 8] = ' ';
        if (r->ch == 8) {
            page[(x)+6][(y) + 9] = '0';
        } else if (r->ch < 2) {
            page[(x)+6][(y) + 9] = '0' + r->ch;
        } else {
            page[(x)+6][(y) + 9] = '*';
        }
        if (r->ch == 8) {
            page[(x)+6][(y) + 9] = '0';
        } else if (r->cl < 2) {
            page[(x)+6][(y) + 10] = '0' + r->cl;
        } else {
            page[(x)+6][(y) + 10] = '*';
        }

    }
    return 0;
}

#if 0

    for (i = 0; i < 4096; i++) {

        printf ("%s %03X: ", r->note, addr);
        p = strchr(r->note, ':');
        p++;
        x = (toupper(*p) - 'A') * 5;
        y = ((p[1] - '0') + 1) * 20;

        y -= 14;

        for(k = 0; k < 14; k++) {
           page[(x)][y + k] = '-';
           page[(x)+6][y + k] = '-';
        }
        page[(x)+6][y + 14 ] = '+';
        for (j = 0; j < 6; j++) {
            page[(x) + j][(y)] = '|';
            page[(x) + j][(y) + 14] = '|';
        }
        page[(x) + 3][(y) + 15] = '*';
        sprintf(line, "%04X", addr);
        page[(x)+6][(y)] = *p;
        page[(x)+6][(y) + 1] = p[1];
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
            if (r->PK != 0 || r->CB == 3 || r->CU == 2) {
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
            page[(x)+4][(y)] = 'R';
            for(j = 0; ck_name[r->CK][j] != '\0'; j++)
                page[(x)+4][(y) + 2 + j] = ck_name[r->CK][j];
            printf("%s", ck_name[r->CK]);
            if (r->PK != 0)
               printf(",1");
        }
        line[0] = '\0';
        if (r->CF == 4) {
            printf(" STP");
            strcpy(line, "SP");
        } else if (r->CF == 0 && r->CA == 0) {
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
            case 6:  printf("+"); strcat(line, "+"); break;
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

        if (line[0] != '\0') {
                page[(x)+2][(y)] = 'R';
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
            sprintf(line, "*%02X", 0x88 | ((r->CN & 0x80) >> 2) | ((r->CK & 0x80) << 1) | (r->CK & 0x7));
            printf("  %x(%s) %02x %s %s ", 0x88 | ((r->CN & 0x80) >> 2) | ((r->CK & 0x80) << 1) |
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
            }
            printf("  %s(%s) %02x %s %s ", cm_name[r->CM], (r->CM < 3) ? cu2_name[r->CU]: cu1_name[r->CU],
                       r->CN, ch_name[r->CH], cl_name[r->CL]);
        }

        if (line[0] != '\0') {
                page[(x)+3][(y)] = 'S';
                page[(x)+3][(y) + 1] = ' ';
                for(j = 0; line[j] != '\0'; j++)
                   page[(x)+3][(y) + 2 + j] = line[j];
        }

        sprintf(line, "%02XR", r->CN);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+5][(y) + 12 + j] = line[j];

        if (cs_name[r->CS][0] != '\0') {
            page[(x)+4][(y)] = 'C';
            for(j = 0; cs_name[r->CS][j] != '\0'; j++)
                page[(x)+4][(y) + 2 + j] = cs_name[r->CS][j];
        }

        sprintf(line, "R %s,%s", ch_name[r->CH], cl_name[r->CL]);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+5][(y) + j] = line[j];

        page[(x)+6][(y) + 7] = ' ';
        page[(x)+6][(y) + 8] = ' ';
        if (r->CH < 2) {
            page[(x)+6][(y) + 10] = '0' + r->CH;
        } else {
            page[(x)+6][(y) + 10] = '*';
        }
        if (r->CL < 2) {
            page[(x)+6][(y) + 9] = '0' + r->CL;
        } else {
            page[(x)+6][(y) + 9] = '*';
        }


        if (r->CS != 0)
            printf("%s ", cs_name[r->CS]);
        j = r->CN;
        if (r->CM < 3 && r->CU == 2)
           j |= (r->CK & 0xf) << 8;
        else
           j |= (addr & 0xf00);
        if (r->CH < 2) {
           if (r->CH == 1)
              j |= 2;
           if (r->CL > 1) {
              printf("%s %03x, ", ros_2030[j].note, j);
              j |= 1;
              printf("%s %03x", ros_2030[j].note, j);
           } else {
              if (r->CL == 1)
                  j |= 1;
              printf("%s %03x", ros_2030[j].note, j);
           }
        } else if (r->CL < 2) {
              if (r->CL == 1)
                  j |= 1;
              if (r->CH > 1) {
                  printf("%s %03x, ", ros_2030[j].note, j);
                  j |= 2;
                  printf("%s %03x", ros_2030[j].note, j);
              } else {
                 if (r->CH == 1)
                    j |= 2;
                 printf("%s %03x", ros_2030[j].note, j);
              }
        } else {
              printf("%s %03x, ", ros_2030[j].note, j);
              printf("%s %03x, ", ros_2030[j|1].note, j|1);
              printf("%s %03x, ", ros_2030[j|2].note, j|2);
              printf("%s %03x", ros_2030[j|3].note, j|3);
        }

        printf(" from: ");
        for (j = 0; ros_input[addr][j] != 0; j++) {
            printf("%s(%03X), ", ros_2030[ros_input[addr][j]].note, ros_input[addr][j]);
        }
        sprintf(line, "Input=%d", j);
        for(j = 0; line[j] != '\0'; j++)
            page[(x)+7][(y) + j] = line[j];

        /* Find inputs on same page */
    printf(" On page %s ", curr_page);
        for(j = 0; ros_input[addr][j] != 0; j++) {
            struct _ros_2844 *q = &ros_2030[ros_input[addr][j]];
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


}
#endif


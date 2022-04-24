#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include <sys/types.h>

struct _ros {
    int      io;
    int      lu;
    int      mv;
    int      zp;
    int      zn;
    int      zf;
    int      tr;
    int      zr;
    int      ws;
    int      sf;
    int      iv;
    int      al;
    int      wm;
    int      up;
    int      md;
    int      lb;
    int      mb;
    int      dg;
    int      ul;
    int      ur;
    int      ce;
    int      lx;
    int      tc;
    int      ry;
    int      ad;
    int      ab;
    int      bb;
    int      ux;
    int      ss;
    int      extra;
    uint32_t row1;
    uint32_t row2;
    uint32_t row3;
    uint32_t row4;
    char    note[20];
} ROS[4096];


int
main(int argc, char *argv[])
{
    FILE   *in;
    FILE   *out;
    char   line[200];
    int    ln = 0;
    char   *p;
    int    i, j, b;
    int    io;
    int    addr1, addr2;
    int    max;
    char   *q;
    char   note[20];
    u_int32_t  bits[4];
    int    parity;

    /* Syntax, cros2050 input output */
    /*   or    cros2050 <input >output */
    if (argc > 1) {
       if ((in = fopen(argv[1], "r")) == NULL) {
           fprintf(stderr, "Unable to read: %s, ", argv[1]);
           perror("");
           exit(1);
       }
       if ((out = fopen(argv[2], "w")) == NULL) {
           fprintf(stderr, "Unable to create: %s, ", argv[2]);
           perror("");
           exit(1);
       }
    } else {
       in = stdin;
       out = stdout;
    }


loop:
    while (fgets(line, sizeof(line), in) != NULL) {
        ln++;
        /* Ignore header lines */
        if (strncasecmp(line, "hex", 3) == 0 ||
            strncasecmp(line, "add ", 4) == 0)
            goto loop;
        /* Ignore lines that are blank */
        if (line[0] == ' ' && line[1] == ' ')
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
        for (i = 0; i < 13 && *p != '\0'; p++) {
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
        if (i != 13) {
           fprintf(stderr, "Address2 not complete %d %s\n", ln, line);
           goto loop;
        }
        if (addr1 != addr2) {
           fprintf(stderr, "Address not match %d %03x %03x %s\n", ln, addr1, addr2, line);
           goto loop;
        }

        /* Skip a blank */
        for (j = 0; *p == ' '; j++,p++);
        q = note;
        if (strncmp(p, "- ", 2) != 0 && strncmp(p, "io", 2) != 0) {
            /* Grab sheet and box */
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            *q++ = '-';
            while (*p == ' ') p++;
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
        *q = '\0';
        }
        *q++ = '\0';
        while (*p == ' ') p++;
        if (strncasecmp(p, "roser", 5) == 0) {
            io = 4;
        } else if (strncasecmp(p, "zeros", 5) == 0) {
            io = 3;
        } else if (strncasecmp(p, "ones ", 5) == 0) {
            io = 2;
        } else if (strncasecmp(p, "io   ", 5) == 0) {
            io = 1;
        } else {
            io = 0;
        }
        ROS[addr1].io = io;
        p += 5;
        while (*p == ' ') p++;
        strcpy(&ROS[addr1].note[0], &note[0]);
        /* Grab rest of line */
        j = b = 0;
        parity = 1;
        bits[0] = bits[1] = bits[2] = bits[4] = 0;
        for (;*p != '\0' && *p != '\n'; p++) {
            if (*p == '0') {
               b++;
               bits[j] <<= 1;
            } else if (*p == '1') {
               b++;
               bits[j] = (bits[j] << 1) | 1;
               parity ^= 1;
            } else if (*p != ' ') {
               fprintf(stderr, "invalid char %d %c %s\n", ln, *p, line);
            }
            if (b == 31 && j == 0) {
               if (parity != 0 && io < 4) {
                   fprintf(stderr, "Parity error %d %08x 0 %s\n", ln, bits[0],  line);
               }
               j++;
               b = 0;
               parity = 1;
            } else if (b == 25 && j == 1) {
               if (parity != 0 && io < 4) {
                   fprintf(stderr, "Parity error %d %08x 1 %s\n", ln,  bits[1], line);
               }
               j++;
               b = 0;
               parity = 1;
            } else if (b == 28 && j == 2) {
               j++;
               b = 0;
            }
            
        }
            if (parity != 0 && io < 4) {
               fprintf(stderr, "Parity error %d %08x 2 %s\n", ln,  bits[2], line);
            }
        while (b < 14) {
            bits[j] <<= 1;
            b++;
        }
        if (addr1 > max)
            max=addr1;
        ROS[addr1].lu = (bits[0] >> 27) & 7;
        ROS[addr1].mv = (bits[0] >> 25) & 3;
        ROS[addr1].zp = ((bits[0] >> 19) & 0x3f) << 6;
        ROS[addr1].zf = (bits[0] >> 15) & 0xf;
        ROS[addr1].zn = (bits[0] >> 12) & 7;
        ROS[addr1].tr = (bits[0] >> 7) & 0x1f;
        ROS[addr1].zr = (bits[0] >> 6) & 1;
        ROS[addr1].ws = (bits[0] >> 3) & 7;
        ROS[addr1].sf = bits[0] & 7;
        ROS[addr1].row1 = bits[0];
        ROS[addr1].iv = (bits[1] >> 21) & 7;
        ROS[addr1].al = (bits[1] >> 16) & 0x1f;
        ROS[addr1].wm = (bits[1] >> 12) & 0xf;
        ROS[addr1].up = (bits[1] >> 10) & 0x3;
        ROS[addr1].md = (bits[1] >> 9) & 1;
        ROS[addr1].lb = (bits[1] >> 8) & 1;
        ROS[addr1].mb = (bits[1] >> 7) & 1;
        ROS[addr1].dg = (bits[1] >> 4) & 7;
        ROS[addr1].ul = (bits[1] >> 2) & 3;
        ROS[addr1].ur = bits[1] & 3;
        ROS[addr1].row2 = bits[1];
        ROS[addr1].ce = (bits[2] >> 23) & 0xf;
        ROS[addr1].lx = (bits[2] >> 20) & 0x7;
        ROS[addr1].tc = (bits[2] >> 19) & 0x1;
        ROS[addr1].ry = (bits[2] >> 16) & 0x7;
        ROS[addr1].ad = (bits[2] >> 12) & 0xf;
        ROS[addr1].ab = (bits[2] >> 6) & 0x3f;
        ROS[addr1].bb = (bits[2] >> 1) & 0x1f;
        ROS[addr1].ux = bits[2] & 1;
        ROS[addr1].row3 = bits[2];
        ROS[addr1].ss = (bits[3] >> 8) & 0x3f;
        ROS[addr1].row4 = bits[3];
    }
    for (addr1 = 0; addr1 < 4096; addr1++) {
                          /*       io    lu     mv    zp   zn    zf    tr */
        fprintf(out, "/* %03x */ { 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*   zr    ws    sf     iv   al   wm    up     md    mb  */
                     " 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*   lb    dg    ul   ur   ce     lx     tc    ry   ad   */
                     " 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*  ab     bb     ux    ss   extra row1    row2    row3  */
                     " 0x%x, 0x%x, 0x%x, 0x%x,  0x%x, 0x%08x, 0x%08x, 0x%08x, "
                   /* row4   note */
                     " 0x%08x, \"%s\"},\n",
                  addr1, ROS[addr1].io, ROS[addr1].lu, ROS[addr1].mv,
                         ROS[addr1].zp, ROS[addr1].zn, ROS[addr1].zf, 
                         ROS[addr1].tr, ROS[addr1].zr, ROS[addr1].ws,
                         ROS[addr1].sf, ROS[addr1].iv, ROS[addr1].al,
                         ROS[addr1].wm, ROS[addr1].up, ROS[addr1].md,
                         ROS[addr1].mb, ROS[addr1].lb, ROS[addr1].dg,
                         ROS[addr1].ul, ROS[addr1].ur, ROS[addr1].ce,
                         ROS[addr1].lx, ROS[addr1].tc, ROS[addr1].ry,
                         ROS[addr1].ad, ROS[addr1].ab, ROS[addr1].bb,
                         ROS[addr1].ux, ROS[addr1].ss, ROS[addr1].extra,
                         ROS[addr1].row1, ROS[addr1].row2, ROS[addr1].row3,
                         ROS[addr1].row4, ROS[addr1].note);
    }
    return 0;
}



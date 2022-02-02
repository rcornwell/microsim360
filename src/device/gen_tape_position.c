
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


int
main(int argc, char *argv[])
{
    FILE      *out;
    float     length, radius, cir;
    int       i;
    int       fpi;
    int       f;
    int       xpos;
    int       ypos;
    int       step;
    int       last;

    length = 0.0f;
    f = 0;
    i = 0;
    xpos = 0;
    ypos = 75;
    step = 0;
    last = 0;

    if (argc > 1) {
       if ((out = fopen(argv[1], "w")) == NULL) {
           fprintf(stderr, "Unable to create: %s, ", argv[2]);
           perror("");
           exit(1);
       }
    } else {
       out = stdout;
    }

    fprintf(out, "struct _tape_image tape_position[1300] = {\n\r");

    for(radius = 5.125f; length < (2400.0f * 12.0f); radius += 0.003f) {
        cir = M_PI * radius;
        fpi = (int)(cir * 1600.0f);
        length += cir;
        fprintf(out, "   { %d, %d, %d, %d, %d }, /* %d */\n\r", xpos, ypos, f, fpi, (int)(radius * 3.1f), i);
        f += fpi;
        i++;
        step++;
        if (step > 32) {
           step = 0;
           ypos += 75;
           if (ypos > 597 && xpos < 300) {
              ypos = 0;
              xpos += 75;
           }
           if (ypos > 597)
              ypos -= 75;
        }
//        printf("%8d  R=%f C=%f  L=%f Lft = %f   %d  frames=%10d\n", i, radius, cir, length, length/12.0, fpi, f);
    }
    fprintf(out, "   { %d, %d, %d, %d, %d }, /* %d */\n\r", xpos, ypos, f, 0, (int)(radius * 6.0f), i);
    last = i;
    i++;
    while (i < 1300) {
        fprintf(out, "   { 0, 0, 0, 0 },\n\r");
        i++;
    }
    fprintf(out, "};\r\n");
    fprintf(out, "int max_tape_length = %d;\n\r", f);
    fprintf(out, "int max_tape_pos = %d;\n\r", last);
}



#include <math.h>
#include <stdio.h>


int
main(int argc, char *argv[])
{
    float     length, radius, cir;
    int       i;
    int       x;
    int       fpi;
    int       f;
    int       frame;
    int       rev;
   
    length = 0.0f;
    f = -25761;
    i = 0;
    for(radius = 5.125f; length < (2400.0f * 12.0f); radius += 0.003f) {
        cir = M_PI * radius;
        fpi = (int)(cir * 1600.0f);
        f += fpi;
        length += cir;
        x = 25761;
        frame = f;
        for (rev = 0; frame > x; rev ++) {
            x += 15;
            frame -= x;
        }
        i++;
        printf("%8d  R=%f C=%f  L=%f Lft = %f   %d  frames=%10d rev=%d %d\n", i, radius, cir, length, length/12.0, fpi, f, rev, x);
    }

}

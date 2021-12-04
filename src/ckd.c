

    2311   data rate  156,000 b/s  seek 85 ms avg  25ms/rev

           Cyl to cyl      30ms
           cyl 0 to 202   145ms

            5 cyl  40ms
           10 cyl  55ms
           15 cyl  60ms
           20 cyl  75ms
           40 cyl  85ms
           60 cyl  90ms
           80 cyl  95ms
          100 cyl 105ms

            0 to  5 25ms + 5ms/cyl
            5 to 25 55ms + 1ms/cyl
           25 to -  75ms + 1ms/10cyl

          3694 + bytes per track

          3906 bpt

          index to HA  GAP 36 bytes. (34)0x00 - 0xff 0x0e 
          alpha gap        18 bytes. 0xcc - (15)0x00 0xff 0x0e
          beta gap         18 bytes  0xcc - (9)0xff, (6)0x00, (1)0xff, 0x0e
          AM gap           -  byte   0xcc -(>21) 0xff, (4)0x00, (1)0xff, (2AM)0xff, 0x0e
  
          Data rate 6.8us per byte.

          156,000 = 6.4us per byte
          
          1us step time. 

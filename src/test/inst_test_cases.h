/*
 * microsim360 - Model 2030 CPU instruction test cases.
 *
 * Copyright 2022, Richard Cornwell
 *                 Original test cases by Ken Shirriff
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


  CTEST( instruct, fp_conversion) {
      ASSERT_EQUAL(0, floatToFpreg(0, 0.0));
      ASSERT_EQUAL(0, get_fpreg_s(0));
      ASSERT_EQUAL(0, get_fpreg_s(1));

      /* From Princ Ops page 157 */
      ASSERT_EQUAL(0, floatToFpreg(0, 1.0));
      ASSERT_EQUAL_X(0x41100000, get_fpreg_s(0));
      ASSERT_EQUAL(0, get_fpreg_s(1));

      ASSERT_EQUAL(0, floatToFpreg(0, 0.5));
      ASSERT_EQUAL_X(0x40800000, get_fpreg_s(0));
      ASSERT_EQUAL(0, get_fpreg_s(1));

      ASSERT_EQUAL(0, floatToFpreg(0, 1.0/64.0));
      ASSERT_EQUAL_X(0x3f400000, get_fpreg_s(0));
      ASSERT_EQUAL(0, get_fpreg_s(1));

      ASSERT_EQUAL(0, floatToFpreg(0, -15.0));
      ASSERT_EQUAL_X(0xc1f00000, get_fpreg_s(0));
      ASSERT_EQUAL(0, get_fpreg_s(1));
  }

  CTEST(instruct, fp_32_conversion) {
      int   i;

      ASSERT_EQUAL(0, floatToFpreg(0, 0.0));

      set_fpreg_s(0, 0xff000000);
      ASSERT_EQUAL(0.0, cnvt_32_float(0));

      set_fpreg_s(0, 0x41100000);
      ASSERT_EQUAL(1.0, cnvt_32_float(0));

      set_fpreg_s(0, 0x40800000);
      ASSERT_EQUAL(0.5, cnvt_32_float(0));

      set_fpreg_s(0, 0x3f400000);
      ASSERT_EQUAL(1.0/64.0, cnvt_32_float(0));

      set_fpreg_s(0, 0xc1f00000);
      ASSERT_EQUAL(-15.0, cnvt_32_float(0));

      srand(1);
      for (i = 0; i < 20; i++) {
          double f = rand() / (double)(RAND_MAX);
          int p = (rand() / (double)(RAND_MAX) * 400) - 200;
          double fp, ratio;
          f = f * pow(2, p);
          if (rand() & 1) {
              f = -f;
          }
          (void)floatToFpreg(0, f);
          fp = cnvt_32_float(0);
          /* Compare within tolerance */
          ratio = fabs((fp - f) / f);
          ASSERT_TRUE(ratio < .000001);
      }
  }

  CTEST(instruct, fp_64_conversion) {
      int   i;
      ASSERT_EQUAL(0, floatToFpreg(0, 0.0));

      set_fpreg_s(0, 0xff000000);
      set_fpreg_s(1, 0);
      ASSERT_EQUAL(0.0, cnvt_64_float(0));

      set_fpreg_s(0, 0x41100000);
      set_fpreg_s(1, 0);
      ASSERT_EQUAL(1.0, cnvt_64_float(0));

      set_fpreg_s(0, 0x40800000);
      set_fpreg_s(1, 0);
      ASSERT_EQUAL(0.5, cnvt_64_float(0));

      set_fpreg_s(0, 0x3f400000);
      set_fpreg_s(1, 0);
      ASSERT_EQUAL(1.0/64.0, cnvt_64_float(0));

      set_fpreg_s(0, 0xc1f00000);
      set_fpreg_s(1, 0);
      ASSERT_EQUAL(-15.0, cnvt_64_float(0));

      srand(1);
      for (i = 0; i < 20; i++) {
          double f = rand() / (double)(RAND_MAX);
          int p = (rand() / (double)(RAND_MAX) * 400) - 200;
          double fp;
          f = f * pow(2, p);
          if (rand() & 1) {
              f = -f;
          }
          (void)floatToFpreg(0, f);
          fp = cnvt_64_float(0);
          ASSERT_EQUAL(f, fp);
      }
  }

  /* Roughly test characteristics of random number generator */
  CTEST(instruct, randfloat) {
      int pos = 0, neg = 0;
      int big = 0, small = 0;
      int i;

      srand(5);
      for (int i = 0; i < 100; i++) {
          double f = randfloat(200);
          if (f < 0) {
              neg ++;
          } else {
              pos ++;
          }
          if (fabs(f) > pow(2, 100)) {
              big++;
          } else if (fabs(f) < pow(2, -100)) {
              small++;
          }
      }
      ASSERT_TRUE(pos > 30);
      ASSERT_TRUE(neg > 30);
      ASSERT_TRUE(big > 15);
      ASSERT_TRUE(small > 15);

      /* Test scaling */
      big = 0;
      small = 0;
      for (i = 0; i < 100; i++) {
          double f = randfloat(10);
          if (f < 0) {
              neg ++;
          } else {
              pos ++;
          }
          if (fabs(f) > pow(2, 10)) {
              big++;
          } else if (fabs(f) < pow(2, -10)) {
              small++;
          }
      }
      ASSERT_TRUE(big < 8);
      ASSERT_TRUE(small < 8);
  }

  /* Load LR - LR 3,1 */
  CTEST(instruct, load_reg) {
      init_cpu();
      set_cc(CC3);
      set_mem(0x400, 0x18310000);  /* LR 3,1 */
      set_reg(1, 0x12345678);
      test_inst(0);
      ASSERT_EQUAL_X(0x12345678, get_reg(3));
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Load and test LTR - LTR 3,4 */
  CTEST(instruct, loadtest_reg) {
      init_cpu();
      set_mem(0x400, 0x12340000);  /* LTR 3,4 */
      /* Test negative number */
      set_reg(4, 0xcdef1234);
      test_inst(0);
      ASSERT_EQUAL_X(0xcdef1234, get_reg(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test zero */
      set_reg(4, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x0, get_reg(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test positive number */
      set_reg(4, 0x12345678);
      test_inst(0);
      ASSERT_EQUAL_X(0x12345678, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);
  }

  /* Load complement LCR - LCR 3,4 */
  CTEST(instruct, loadcom_reg) {
      init_cpu();
      set_mem(0x400, 0x13340000); /* LCR 3,4 */
      /* Test positive number */
      set_reg(4, 0x00001000);
      test_inst(0);
      ASSERT_EQUAL_X(0xfffff000, get_reg(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test negative number */
      set_reg(4, 0xffffffff);
      test_inst(0);
      ASSERT_EQUAL_X(0x1, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test zero */
      set_reg(4, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x0, get_reg(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_reg(4, 0x80000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_reg(3));
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Load Positive LPR - LPR 3,4 */
  CTEST(instruct, loadpos_reg) {
      init_cpu();
      set_mem(0x400, 0x10340000);  /* LPR 3,4 */
      set_reg(4, 0xffffffff);
      test_inst(0);
      ASSERT_EQUAL_X(0x00000001, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test positive */
      set_reg(4, 0x00000001);
      test_inst(0);
      ASSERT_EQUAL_X(0x1, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test zero */
      set_reg(4, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x0, get_reg(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_reg(4, 0x80000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_reg(3));
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Load negative LNR - LNR 3,4 */
  CTEST(instruct, loadneg_reg) {
      init_cpu();
      set_mem(0x400, 0x11340000);  /* LNR 3,4 */
      set_reg(4, 0xffffffff);
      test_inst(0);
      ASSERT_EQUAL_X(0xffffffff, get_reg(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test positive */
      set_reg(4, 0x00000001);
      test_inst(0);
      ASSERT_EQUAL_X(0xffffffff, get_reg(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test zero */
      set_reg(4, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x0, get_reg(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_reg(4, 0x80000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_reg(3));
      ASSERT_EQUAL(CC1, CC_REG);
  }

  /* Add register - AR 1,2 */
  CTEST(instruct, add_reg) {
      init_cpu(0);
      set_mem(0x400, 0x1a120000);  /* AR 1,2 */
      set_reg(1, 0x12345678);
      set_reg(2, 0x00000005);
      test_inst(0);
      ASSERT_EQUAL_X(0x1234567d, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);
  }

  /* Test two add instructions */
  CTEST(instruct, twoadd_reg) {
      init_cpu();
      set_mem(0x400, 0x1a121a31); /* AR 1,2; AR 3,1 */
      set_reg(1, 0x12345678);
      set_reg(2, 0x00000001);
      set_reg(3, 0x00000010);
      test_inst2();
      ASSERT_EQUAL_X(0x12345679, get_reg(1));
      ASSERT_EQUAL_X(0x12345689, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);
  }

  /* Add a negative number */
  CTEST(instruct, add_neg_reg) {
      init_cpu();
      set_mem(0x400, 0x1a120000);  /* AR 1,2 */
      set_reg(1, 0x81234567);
      set_reg(2, 0x00000001);
      test_inst(0);
      ASSERT_EQUAL_X(0x81234568, get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);
  }

  /* Add zero */
  CTEST(instruct, add_zero_reg) {
      init_cpu();
      set_mem(0x400, 0x1a120000); /* AR 1,2 */
      set_reg(1, 0x00000002);
      set_reg(2, 0xfffffffe);
      test_inst(0);
      ASSERT_EQUAL_X(0x0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Test over flow flag is set */
  CTEST(instruct, add_over_reg) {
      init_cpu();
      set_mem(0x400, 0x1a120000);  /* AR 1,2 */
      set_reg(1, 0x7fffffff);
      set_reg(2, 0x00000001);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Test that overflow will trap */
  CTEST(instruct, add_overtrap_reg) {
      uint32_t   psw1, psw2;
      init_cpu();
      set_mem(0x400, 0x1a120000); /* AR 1,2 */
      set_reg(1, 0x7fffffff);
      set_reg(2, 0x00000001);
      test_inst(0x8);
      psw1 = get_mem(0x28);
      psw2 = get_mem(0x2c);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(0x8, psw1);
      ASSERT_EQUAL_X(0x78000402, psw2);
      ASSERT_EQUAL_X(0x80000000, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Test add from memory */
  CTEST(instruct, add) {
      init_cpu();
      set_mem(0x400, 0x5a156200); /* A 1,200(5,6) */
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x34567890);
      test_inst(0);
      ASSERT_EQUAL_X(0x12345678 + 0x34567890, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);
  }

  /* Test add half word AH */
  CTEST(instruct, add_half) {
      init_cpu();
      set_mem(0x400, 0x4a156200);  /* AH 1,200(5,6) */
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000202);
      set_mem(0x500, 0x34567890);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 + 0x7890, get_reg(1)); /* -1 */
      ASSERT_EQUAL(CC2, CC_REG); /* Negative */
  }

  /* Test add half word sign extend AH */
  CTEST(instruct, add_half_ext) {
      init_cpu();
      set_mem(0x400, 0x4a156200);  /* AH 1,200(5,6) */
      set_reg(1, 1);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0xfffe1234);  /* Only fffe (-2) used */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffffffff, get_reg(1)); /* -1 */
      ASSERT_EQUAL(CC1, CC_REG); /* Negative */
  }

  /* Add logical, zero no carry */
  CTEST(instruct, add_logic_zero) {
      init_cpu();
      set_mem(0x400, 0x1e120000);   /* ALR 1,2 */
      set_reg(1, 0);
      set_reg(2, 0);
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG); /* zero, no carry */
  }

  /* Add logical, non-zero no carry */
  CTEST(instruct, add_logic_nonzero) {
      init_cpu();
      set_mem(0x400, 0x1e120000);   /* ALR 1,2 */
      set_reg(1, 0xffff0000);
      set_reg(2, 0x00000002);
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffff0002, get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG); /* non zero, no carry */
  }

  /* Add logical, zero and carry */
  CTEST( instruct, add_logic_zero_carry) {
      init_cpu();
      set_mem(0x400, 0x1e120000);   /* ALR 1,2 */
      set_reg(1, 0xfffffffe);
      set_reg(2, 0x00000002);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG); /* zero, carry */
  }

  /* Add logical non-zero and carry */
  CTEST( instruct, add_logic_nonzero_carry) {
      init_cpu();
      set_mem(0x400, 0x1e120000);   /* ALR 1,2 */
      set_reg(1, 0xfffffffe);
      set_reg(2, 0x00000003);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000001, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG); /* not zero, carry */
  }

  /* Add logical */
  CTEST(instruct, add_logic2) {
      init_cpu();
      set_mem(0x400, 0x5e156200);  /* AL 1,200(5,6) */
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0xf0000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x02345678, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG); /* not zero, carry */
  }

  /* Subtract instruction */
  CTEST(instruct, subtract) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x00000001);
      set_mem(0x400, 0x1b120000);  /* SR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345677, get_reg(1));
  }

  /* Subtract instruction */
  CTEST(instruct, subtract2) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x12300000);
      set_mem(0x400, 0x5b156200);  /* S 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00045678, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);   /* Positive */
  }

  /* Subtract instruction */
  CTEST(instruct, subtract3) {
      init_cpu();
      set_reg(1, 0x8fffffff);
      set_mem(0x400, 0x1b110000);  /* SR 1,1 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);   /* Zero */
  }

  /* Subtract instruction */
  CTEST(instruct, subtract4) {
      init_cpu();
      set_reg(1, 0xffffffff);
      set_mem(0x400, 0x1b110000);  /* SR 1,1 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);   /* Zero */
  }

  /* Subtract instruction */
  CTEST(instruct, subtract5) {
      init_cpu();
      set_reg(1, 0x80000000);
      set_mem(0x400, 0x1b110000);  /* SR 1,1 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);   /* Zero */
  }

  /* Subtract half */
  CTEST(instruct, sub_half) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x12300000);
      set_mem(0x400, 0x4b156200);  /* SH 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 - 0x1230, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);   /* Positive */
  }

  /* Subtract logical */
  CTEST(instruct, sub_logical) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x12345678);
      set_mem(0x400, 0x1f120000);  /* SLR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);   /* Difference is zero (carry) */
  }

  /* Subtract logical */
  CTEST(instruct, sub_logical2) {
      init_cpu();
      set_reg(1, 0xffffffff);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x11111111);
      set_mem(0x400, 0x5f156200);   /* SL 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xeeeeeeee, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);    /* Non-zero, carry (no borrow) */
  }

  /* Subtract logical */
  CTEST(instruct, sub_logical3) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x23456789);
      set_mem(0x400, 0x5f156200);   /* SLR 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(((uint32_t)0x12345678 - (uint32_t)0x23456789), get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);    /* Non-zero, no carry (borrow) */
  }

  /* Compare register */
  CTEST(instruct, cp_reg) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x12345678);
      set_mem(0x400, 0x19120000);    /* CR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_reg(1)); /* Unchanged */
      ASSERT_EQUAL(CC0, CC_REG);     /* Operands are equal */
  }

  /* Compare register */
  CTEST(instruct, cp_reg2) {
      init_cpu();
      set_reg(1, 0xfffffffe); /* -2 */
      set_reg(2, 0xfffffffd); /* -3 */
      set_mem(0x400, 0x19120000);    /* CR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xfffffffe, get_reg(1)); /* Unchanged */
      ASSERT_EQUAL(CC2, CC_REG);    /* First operand is high */
  }

  /* Compare register */
  CTEST(instruct, cp_reg3) {
      init_cpu();
      set_reg(1, 2);
      set_reg(2, 3);
      set_mem(0x400, 0x19120000);    /* CR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(2, get_reg(1)); /* Unchanged */
      ASSERT_EQUAL(CC1, CC_REG);     /* First operand is low */
  }

  /* Compare */
  CTEST(instruct, comp) {
      init_cpu();
      set_reg(1, 0xf0000000);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x12345678);
      set_mem(0x400, 0x59156200);  /* C 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);   /* First operand is low */
  }

  /* Multiply register */
  CTEST(instruct, mult) {
      init_cpu();
      set_reg(3, 28);
      set_reg(4, 19);
      set_mem(0x400, 0x1c240000);  /* MR 2,4 */
      test_inst(0x0);
      ASSERT_EQUAL(0, get_reg(2));
      ASSERT_EQUAL(28 * 19, get_reg(3));
  }

  /* Multiply register long result */
  CTEST(instruct, mult_long) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_reg(4, 0x34567890);
      set_mem(0x400, 0x1c240000);  /* MR 2,4 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x3b8c7b8, get_reg(2));
      ASSERT_EQUAL_X(0x3248e380, get_reg(3));
  }

  /* Multiply register longest result */
  CTEST(instruct, mult_longer) {
      init_cpu();
      set_reg(3, 0x7fffffff);
      set_reg(4, 0x7fffffff);
      set_mem(0x400, 0x1c240000); /* MR 2,4 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x3fffffff, get_reg(2));
      ASSERT_EQUAL_X(0x00000001, get_reg(3));
  }

  /* Multiply negative numbers */
  CTEST(instruct, mult_neg) {
      init_cpu();
      set_reg(3, 0xfffffffc);     /* -4 */
      set_reg(4, 0xfffffffb);     /* -5 */
      set_mem(0x400, 0x1c240000); /* MR 2,4 */
      test_inst(0x0);
      ASSERT_EQUAL(0, get_reg(2));
      ASSERT_EQUAL(20, get_reg(3));
  }

  /* Multiply negative with positive */
  CTEST(instruct, mult_negpos) {
      init_cpu();
      set_reg(3, 0xfffffffc);      /* -4 */
      set_reg(4, 0x0000000a);      /* 10 */
      set_mem(0x400, 0x1c240000);  /* MR 2,4 */
      test_inst(0x0);
      ASSERT_EQUAL(0xffffffff, get_reg(2));
      ASSERT_EQUAL((uint32_t)(-40), get_reg(3));
  }

  /* Multiply from memory */
  CTEST(instruct, mult_mem) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x34567890);
      set_mem(0x400, 0x5c256200);   /* M 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x03b8c7b8, get_reg(2)); /* High 32-bits */
      ASSERT_EQUAL_X(0x3248e380, get_reg(3)); /* Low 32-bits */
  }

  /* Test multiply with random values */
  CTEST(instruct, mult_rand) {
      srand(1);
      init_cpu();
      for (int i = 0; i < testcycles; i++) {
          int n1 = (int)((double)rand() / (double)(RAND_MAX) * 1000.0);
          int n2 = (int)((double)rand() / (double)(RAND_MAX) * 1000.0);
          if (n1 * n2 >= 0x10000) continue;
          set_reg(3, n1);
          set_reg(4, n2);
          set_mem(0x400, 0x1c240000);  /* MR 2,4 */
          test_inst(0x0);
          ASSERT_EQUAL(0, get_reg(2));
          ASSERT_EQUAL(n1 * n2, get_reg(3));
      }
  }

  /* Multiply half word */
  CTEST(instruct, mult_half) {
      init_cpu();
      set_reg(3, 4);
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0x00000003);
      set_mem(0x400, 0x4c356202);   /* MH 3,202(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(12, get_reg(3)); /* Low 32-bits */
  }

  /* Multiply half word */
  CTEST(instruct, mult_half2) {
      init_cpu();
      set_reg(3, 0x00000015);       /* 21 */
      set_reg(5, 0x00000100);
      set_reg(6, 0x00000200);
      set_mem(0x500, 0xffd91111);   /* -39 */
      set_mem(0x400, 0x4c356200);   /* MH 3,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(0xfffffccd, get_reg(3)); /* Low 32-bits */
  }

  /* Divide register */
  CTEST(instruct, div_reg) {
      init_cpu();
      set_reg(2, 0x1);
      set_reg(3, 0x12345678);
      set_reg(4, 0x00000234);
      set_mem(0x400, 0x1d240000);    /* DR 2,4 */
      /* divide R2/R3 by R4 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x112345678 % 0x234, get_reg(2)); /* Remainder */
      ASSERT_EQUAL_X(0x112345678 / 0x234, get_reg(3)); /* Quotient */
  }

  /* Divide register negative */
  CTEST(instruct, div_neg) {
      init_cpu();
      set_reg(2, 0x1);
      set_reg(3, 0x12345678);
      set_reg(4, (uint32_t)(-0x00000234));
      set_mem(0x400, 0x1d240000);    /* DR 2,4 */
      /* divide R2/R3 by R4 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x112345678 % 0x234, get_reg(2)); /* Remainder */
      ASSERT_EQUAL_X((uint32_t)(-0x112345678 / 0x234), get_reg(3)); /* Quotient */
  }

  /* Divide big value */
  CTEST(instruct, div_big) {
      init_cpu();
      set_reg(2, 0x00112233);
      set_reg(3, 0x44556677);
      set_reg(4, 0x12345678);      /* 0x1122334455667788 / 0x12345678 */
      set_mem(0x400, 0x1d240000);
      /* divide R2/R3 by R4 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x11b3d5f7, get_reg(2)); /* Remainder */
      ASSERT_EQUAL_X(0x00f0f0f0, get_reg(3)); /* Quotient */
  }

  /* Divide overflow */
  CTEST(instruct, div_over) {
      init_cpu();
      set_reg(2, 0x12345678);
      set_reg(3, 0x9abcdef0);
      set_reg(5, 0x100);
      set_reg(6, 0x200);
      set_mem(0x500, 0x23456789);
      set_mem(0x400, 0x5d256200);   /* D 2,200(5,6) */
      test_inst(0x8);
      ASSERT_TRUE(trap_flag);
  }

  /* Divide memory */
  CTEST(instruct, div_mem) {
      init_cpu();
      set_reg(2, 0x12345678);
      set_reg(3, 0x9abcdef0);
      set_reg(5, 0x100);
      set_reg(6, 0x200);
      set_mem(0x500, 0x73456789);
      set_mem(0x400, 0x5d256200);   /* D 2,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x50c0186a, get_reg(2)); /* Remainder */
      ASSERT_EQUAL_X(0x286dead6, get_reg(3)); /* Quotient */
  }

  /* Store half word */
  CTEST(instruct, sth) {
      init_cpu();
      set_reg(3, 0xaabbccdd);
      set_reg(4, 1);
      set_reg(5, 1);
      set_mem(0x1000, 0x12345678);
      set_mem(0x400, 0x40345ffe);   /* STH 3,ffe(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xccdd5678, get_mem(0x1000));
  }

  /* Store half word */
  CTEST(instruct, sth2) {
      init_cpu();
      set_reg(3, 0xaabbccdd);
      set_reg(4, 1);
      set_reg(5, 3);
      set_mem(0x1000, 0x12345678);
      set_mem(0x400, 0x40345ffe);   /* STH 3,ffe(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x1234ccdd, get_mem(0x1000));
  }

  /* Make sure store half fails if unaligned */
  CTEST(instruct, sth3) {
      init_cpu();
      set_reg(3, 0xaabbccdd);
      set_reg(4, 1);
      set_reg(5, 2);
      set_mem(0x1000, 0x12345678);
      set_mem(0x400, 0x40345ffe);   /* STH 3,ffe(4,5) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Test load halfword */
  CTEST(instruct, lh) {
      init_cpu();
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x200);
      set_mem(0x1b84, 0x87654321);
      set_mem(0x400, 0x48345986); /* LH 3,986(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00004321, get_reg(3));
  }

  /* Test load halfword second halfword address */
  CTEST(instruct, lh2) {
      init_cpu();
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x200);
      set_mem(0x1b84, 0x17654321);
      set_mem(0x400, 0x48345984); /* LH 3,984(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001765, get_reg(3));
  }

  /* Test load half word, make sure sign is extended */
  CTEST(instruct, lh_ext) {
      init_cpu();
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x200);
      set_mem(0x1b84, 0x87654321);
      set_mem(0x400, 0x48345984);  /* LH 3,984(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffff8765, get_reg(3)); /* sign extension */
  }

  /* Test load half word second halfword, make sure sign is extended */
  CTEST(instruct, lh_ext2) {
      init_cpu();
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x202);
      set_mem(0x1b84, 0x07658321);
      set_mem(0x400, 0x48345984); /* LH 3,984(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffff8321, get_reg(3)); /* sign extension */
  }

  /* Test Compare half */
  CTEST(instruct, ch_equal) {
      init_cpu();
      set_reg(3, 0x00005678);
      set_mem(0x100, 0x5678abcd);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* equal */
  }

  /* Test compare half with sign extension */
  CTEST(instruct, ch_equal_ext) {
      init_cpu();
      set_reg(3, 0xffff9678);
      set_mem(0x100, 0x9678abcd);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* equal */
  }

  /* Compare half word high */
  CTEST(instruct, ch_high) {
      init_cpu();
      set_reg(3, 0x00001235);
      set_mem(0x100, 0x1234abcd);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC2, CC_REG);  /* First operand high */
  }

  /* Compare half word sign extended */
  CTEST(instruct, ch_high_ext) {
      init_cpu();
      set_reg(3, 0x00001235);
      set_mem(0x100, 0x8234abcd);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC2, CC_REG);  /* First operand high */
  }

  /* Compare half word low */
  CTEST(instruct, ch_low) {
      init_cpu();
      set_reg(3, 0x80001235);
      set_mem(0x100, 0x1234abcd);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* First operand low */
  }

  /* Compare half lower extended */
  CTEST(instruct, ch_low_ext) {
      init_cpu();
      set_reg(3, 0xfffffffc);
      set_mem(0x100, 0xfffd0000);
      set_mem(0x400, 0x49300100); /* CH 3,100(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* First operand low */
  }

  /* Halfword second operand is sign-extended and added to first register. */
  CTEST(instruct, ah) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_mem(0x200, 0x1234eeee);
      set_mem(0x400, 0x4a300200); /* AH 3,200(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 + 0x1234, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Add halfword extended */
  CTEST(instruct, ah_ext) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_mem(0x200, 0xfffe9999); /* -2 */
      set_mem(0x400, 0x4a300200); /* AH 3,200(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345676, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Add half word */
  CTEST(instruct, ah_two) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_reg(1, 2);
      set_mem(0x200, 0x99991234);
      set_mem(0x400, 0x4a310200); /* AH 3,200(1,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 + 0x1234, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Subtract halfword */
  CTEST(instruct, sh) {
      set_reg(3, 0x12345678);
      set_mem(0x200, 0x1234eeee);
      set_mem(0x400, 0x4b300200); /* SH 3,200(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 - 0x1234, get_reg(3));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Multiply halfword */
  CTEST(instruct, mh) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_mem(0x200, 0x00059999);  /* 5 */
      set_mem(0x400, 0x4c300200);  /* MH 3,200(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 * 5, get_reg(3));
  }

  /* Multiply halfword negative number */
  CTEST(instruct, mh_neg) {
      init_cpu();
      set_reg(3, (uint32_t)(-0x12345678));
      set_mem(0x200, 0xfffb9999); /* -5 */
      set_mem(0x400, 0x4c300200); /* MH 3,200(0,0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 * 5, get_reg(3));
  }

  /* Test LRA */
  CTEST(instruct, lra) {
      init_cpu();
      /* From Princ Ops p147 */
      set_mem(0x400, 0x41100800);   /* LRA 1,800 */
      test_inst(0x0);
      ASSERT_EQUAL_X(2048, get_reg(1));
  }

  /* Test LRA with index */
  CTEST(instruct, lra2) {
      init_cpu();
      /* From Princ Ops p147 */
      set_reg(5, 0x00123456);
      set_mem(0x400, 0x4150500a);  /* LRA 5, 10(5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00123460, get_reg(5));
  }

  /* Test store character at each offset */
  CTEST(instruct, stc) {
      int  i;
      int  shift;
      uint32_t  desired;
      init_cpu();

      for (i = 0; i < 4; i++) { /* Test all 4 offsets */
          set_reg(5, 0xffffff12); /* Only 12 used */
          set_reg(1, i);
          set_mem(0x100, 0xaabbccdd);
          set_mem(0x400, 0x42501100); /* "STC 5,100(0,1)"); */
          test_inst(0x0);
          shift = (3 - i) * 8;
          desired = ((0xaabbccdd & ~(0xffu << shift)) | (0x12 << shift));
          ASSERT_EQUAL_X(desired, get_mem(0x100));
      }
  }

  /* Test insert character at each offset */
  CTEST(instruct, ic) {
      int i;
      uint32_t desired;
      init_cpu();
      for (i = 0; i < 4; i++) { /* Test all 4 offsets */
          set_reg(5, 0xaabbccdd);
          set_reg(1, i);
          set_mem(0x100, 0x00112233);
          set_mem(0x400, 0x43501100); /* IC 5,100(0,1) */
          test_inst(0x0);
          desired = (0xaabbcc00 | (i * 17));
          ASSERT_EQUAL_X(desired, get_reg(5));
      }
  }

  /* Test execute instruction */
  CTEST(instruct, ex) {
      init_cpu();
      set_mem(0x100, 0x1a000000); /* Target instruction AR 0,0 */
      set_reg(1, 0x00000045);     /* Modification: AR 4,5 */
      set_reg(4, 0x100);
      set_reg(5, 0x200);
      set_mem(0x400, 0x44100100); /* EX 1,100(0,0) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst2();
      ASSERT_EQUAL_X(0x300, get_reg(4));
  }

  /* Test that execute of execute is illegal */
  CTEST(instruct, ex_ex) {
      init_cpu();
      set_mem(0x100, 0x44100100); /* Target instruction EX 1,100(0,0) */
      set_reg(1, 0x00000045);     /* Modification: EX 4,100(5,0) */
      set_mem(0x400, 0x44100100); /* EX 1,100(0,0) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst2();
      ASSERT_TRUE(trap_flag);
  }

  /* Test BAL instruction */
  CTEST(instruct, bal) {
      init_cpu();
      set_reg(3, 0x12000000);
      set_reg(4, 0x00005600);
      set_ilc(0);                /* overwritten with 2 */
      set_cc(CC3);
      set_mem(0x400, 0x45134078); /* BAL 1,78(3,4) */
      test_inst(0xa);
      ASSERT_EQUAL_X(0xba000404, get_reg(1)); /* low-order PSW: ILC, CR, PROGMASK, return IAR */
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Test branch count instruction */
  CTEST(instruct, bct) {
      init_cpu();
      set_reg(1, 3);             /* Counter */
      set_reg(2, 0x00005678);    /* Branch destination */
      set_reg(3, 0x00000010);
      set_mem(0x400, 0x46123100); /* BCT 1,100(2,3) */
      test_inst(0x0);
      ASSERT_EQUAL(2, get_reg(1));
      ASSERT_EQUAL_X(0x00005788, IAR);
  }

  /* Test branch on condition with all values of CC */
  CTEST(instruct, bc) {
      int i, j;
      init_cpu();
      set_mem(0x100, 0);
      for (i = 0; i < 16; i++) {
          for (j = 0; j < 4; j++) {
              uint32_t op = 0x47000100 | (i << 20);  /* BC i,100 */
              switch(j) {
                 case 0:   set_cc(CC0); break;
                 case 1:   set_cc(CC1); break;
                 case 2:   set_cc(CC2); break;
                 case 3:   set_cc(CC3); break;
              }
              set_mem(0x400, op);
              test_inst(0x0);
              if (((i & 8) && (CC_REG == CC0)) ||
                  ((i & 4) && (CC_REG == CC1)) ||
                  ((i & 2) && (CC_REG == CC2)) ||
                  ((i & 1) && (CC_REG == CC3))) {
                  /* Taken */
                  ASSERT_EQUAL_X(0x100, IAR);
              } else {
                  ASSERT_EQUAL_X(0x404, IAR);
              }
          }
      }
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxh_high) {
      init_cpu();
      set_key(0);
      set_reg( 1, 0x12345678);     /* Value */
      set_reg( 4, 1);              /* Increment */
      set_reg( 5, 0x12345678);     /* Comparand */
      set_reg( 2, 0x1000);         /* Branch target */
      set_mem(0x400, 0x86142200);  /* BXH 1, 4, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345679, get_reg(1));
      ASSERT_EQUAL_X(0x1200, IAR); /* Branch taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxh_high_decr) {
      init_cpu();
      set_reg( 1, 0x12345678);    /* Value */
      set_reg( 4, 0xffffffff);    /* Increment -1 */
      set_reg( 5, 0x12345678);    /* Comparand */
      set_reg( 2, 0x1000);        /* Branch target */
      set_mem(0x400, 0x86142200); /* BXH 1, 4, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345677, get_reg(1));
      ASSERT_EQUAL_X(0x404, IAR); /* Branch not taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxh_high1) {
      init_cpu();
      set_reg( 1, 1);              /* Value */
      set_reg( 3, 0x12345678);     /* Increment and comparand */
      set_reg( 2, 0x1000);         /* Branch target */
      set_mem(0x400, 0x86132200);  /* BXH 1, 3, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345679, get_reg(1));
      ASSERT_EQUAL_X(0x1200, IAR); /* Branch taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxh_high2) {
      init_cpu();
      set_reg( 1, 0xffffffff);    /* Value */
      set_reg( 3, 0x12345678);    /* Increment and comparand */
      set_reg( 2, 0x1000);        /* Branch target */
      set_mem(0x400, 0x86132200); /* BXH 1, 3, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345677, get_reg(1));
      ASSERT_EQUAL_X(0x404, IAR); /* Branch not taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxle) {
      init_cpu();
      set_reg( 1, 0x12345678);    /* Value */
      set_reg( 4, 1);             /* Increment */
      set_reg( 5, 0x12345678);    /* Comparand */
      set_reg( 2, 0x1000);        /* Branch target */
      set_mem(0x400, 0x87142200); /* BXLE 1, 4, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345679, get_reg(1));
      ASSERT_EQUAL_X(0x404, IAR); /* Branch not taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxle_low) {
      init_cpu();
      set_reg( 1, 0x12345678);     /* Value */
      set_reg( 4, 0xffffffff);     /* Increment -1 */
      set_reg( 5, 0x12345678);     /* Comparand */
      set_reg( 2, 0x1000);         /* Branch target */
      set_mem(0x400, 0x87142200);  /* BXLE 1, 4, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345677, get_reg(1));
      ASSERT_EQUAL_X(0x1200, IAR); /* Branch taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxle_share) {
      init_cpu();
      set_reg( 1, 1);             /* Value */
      set_reg( 3, 0x12345678);    /* Increment and comparand */
      set_reg( 2, 0x1000);        /* Branch target */
      set_mem(0x400, 0x87132200); /* BXLE 1, 3, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345679, get_reg(1));
      ASSERT_EQUAL_X(0x404, IAR); /* Branch not taken */
  }

  /* Add increment to first operand, compare with odd register after R3 */
  CTEST(instruct, bxle_share1) {
      init_cpu();
      set_reg( 1, 0xffffffff);     /* Value */
      set_reg( 3, 0x12345678);     /* Increment and comparand */
      set_reg( 2, 0x1000);         /* Branch target */
      set_mem(0x400, 0x87132200);  /* BXLE 1, 3, 200(2) */
      set_mem(0x1200, 0);          /* Clear branch location */
      test_inst(0x0);
      log_trace("IAR = %08x\n", IAR);
      printf("IAR = %08x\n", IAR);
      ASSERT_EQUAL_X(0x12345677, get_reg(1));
      ASSERT_EQUAL_X(0x1200, IAR); /* Branch taken */
  }

  /* Branch count register */
  CTEST(instruct, bcr) {
      init_cpu();
      set_reg( 1, 0x12005678);    /* Branch destination */
      set_cc(CC0);
      set_mem(0x400, 0x07810000); /* BCR 8,1 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Count and branch register, branch taken */
  CTEST(instruct, bcr_always) {
      init_cpu();
      set_reg( 1, 0x12005678);    /* Branch destination */
      set_cc(CC0);
      set_mem(0x400, 0x07f10000); /* BCR 15,1 */ /* always */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Count and branch register, not taken */
  CTEST(instruct, bcr_not) {
      init_cpu();
      set_reg( 1, 0x12005678);    /* Branch destination */
      set_cc(CC1);
      set_mem(0x400, 0x07810000); /* BCR 8,1 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x402, IAR);
  }

  /* Branch and link */
  CTEST(instruct, balr) {
      init_cpu();
      set_ilc(2);
      set_cc(CC3);
      set_reg( 2, 0x12005678);    /* Branch destination */
      set_mem(0x400, 0x05120000); /* BALR 1,2 */
      test_inst(0xa);
      ASSERT_EQUAL_X(0x7a000402, get_reg(1)); /* low-order PSW: ILC, CR, PROGMASK, return IAR */
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Branch and link, save IC */
  CTEST(instruct, balr_not) {
      init_cpu();
      set_ilc(2);                 /* overwritten with 1 */
      set_cc(CC3);
      set_mem(0x400, 0x05100000); /* BALR 1,0 */
      test_inst(0xa);
      ASSERT_EQUAL_X(0x7a000402, get_reg(1)); /* low-order PSW: ILC, CR, PROGMASK, return IAR */
      ASSERT_EQUAL_X(0x402, IAR);
  }

  /* Branch count to register taken */
  CTEST(instruct, bctr_taken) {
      init_cpu();
      set_reg( 1, 3);             /* Counter */
      set_reg( 2, 0x12005678);    /* Branch destination */
      set_mem(0x400, 0x06120000); /* BCTR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL(2, get_reg( 1));
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Branch count going negative */
  CTEST(instruct, bctr_taken_neg) {
      init_cpu();
      set_reg( 1, 0);             /* Counter */
      set_reg( 2, 0x12005678);    /* Branch destination */
      set_mem(0x400, 0x06120000); /* BCTR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffffffff, get_reg(1));
      ASSERT_EQUAL_X(0x00005678, IAR);
  }

  /* Branch count, not taken */
  CTEST(instruct, bctr_not_taken) {
      init_cpu();
      set_reg( 1, 1);             /* Counter */
      set_reg( 2, 0x12005678);    /* Branch destination */
      set_mem(0x400, 0x06120000); /* BCTR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL_X(0x402, IAR);
  }

  /* Test load instruction */
  CTEST(instruct, load) {
      init_cpu();
      set_reg(4, 0x1000);
      set_reg(5, 0x200);
      set_mem(0x1b84, 0x12345678);
      set_mem(0x400, 0x58345984); /* L 3,984(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_reg(3));
  }

  /* Test compare instruction */
  CTEST(instruct, comp2) {
      init_cpu();
      set_reg(3, 0x12345678);
      set_reg(4, 0x1000);
      set_reg(5, 0x200);
      set_mem(0x1b84, 0x12345678);
      set_mem(0x400, 0x59345984); /* C 3,984(4,5) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* Operands are equal */
  }

  /* Test Add instruction with random numbers */
  CTEST(instruct, add_rand) {
    int i;
    srand(42);
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      int n1 = rand();
      int n2 = rand();
      int sum = (n1) + (n2);
      set_reg(1, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5a100100);  /* A 1,100(0,0) */
      test_inst(0x0);
      if (sum >= 0x80000000 || sum < -(long int)0x80000000) {
        ASSERT_EQUAL(CC3, CC_REG); /* Overflow */
        continue;
      } else if (sum == 0) {
        ASSERT_EQUAL(CC0, CC_REG); /* Zero */
      } else if (sum > 0) {
        ASSERT_EQUAL(CC2, CC_REG); /* Positive */
      } else {
        ASSERT_EQUAL(CC1, CC_REG); /* Negative */
      }
      ASSERT_EQUAL(sum, (int32_t)(get_reg(1)));
    }
  }

  /* Test subtract instruction with random numbers */
  CTEST(instruct, sub_rand) {
    int   i;
    srand(123);
    init_cpu();
    for (i = 0; i < testcycles; i++) {
      int n1 = rand();
      int n2 = rand();
      int result = (n1) - (n2);
      set_reg(1, n1);
      set_mem(0x100, n2);
      set_mem(0x400, 0x5b100100);  /* S 1,100(0,0) */
      test_inst(0x0);
      if (result >= (long int)0x80000000 || result < -(long int)0x80000000) {
        ASSERT_EQUAL(CC3, CC_REG); /* Overflow */
        continue;
      } else if (result == 0) {
        ASSERT_EQUAL(CC0, CC_REG); /* Zero */
      } else if (result > 0) {
        ASSERT_EQUAL(CC2, CC_REG); /* Positive */
      } else {
        ASSERT_EQUAL(CC1, CC_REG); /* Negative */
      }
      ASSERT_EQUAL(result, (int32_t)(get_reg(1)));
    }
  }

  /* Test multiply with random numbers */
  CTEST(instruct, mult_rand2) {
      int  i;
      srand(42);
      init_cpu();
      for (i = 0; i < testcycles; i++) {
        int n1 = rand();
        int n2 = rand();
        int64_t desired = (int64_t)(n1) * (int64_t)(n2);
        int64_t result;
        set_reg(3, n1); /* Note: multiplicand in reg 3 but reg 2 specified. */
        set_mem(0x100, n2);
        set_mem(0x400, 0x5c200100);  /* M 2,100(0,0) */
        test_inst(0x0);
        result = (int64_t)(((uint64_t)get_reg(2) << 32) + ((uint64_t)get_reg(3)));
        ASSERT_EQUAL(desired, result);
        if (result != desired)
            break;
        /* No condition code */
      }
  }

  /* Test divide instruction with random numbers */
  CTEST(instruct, div_rand) {
      int   i;
      int   ov;
      srand(124);
      init_cpu();
      for (i = 0; i < testcycles; i++) {
          int64_t divisor;
          int64_t dividend;
          int64_t quotient;
          int64_t remainder;
          dividend = (int64_t)(((uint64_t)rand() << 32) |
                              (uint64_t)rand())/ 100;
          divisor = ((int64_t)(int32_t)rand()) / 4;
          quotient = dividend / divisor;
          remainder = dividend % divisor;
          set_reg(2, (uint32_t)(dividend >> 32));
          set_reg(3, (uint32_t)(dividend & 0xffffffff));
          set_mem(0x100, divisor);
          set_mem(0x400, 0x5d200100); /* D 2,100(0,0) */
          test_inst(0x0);
          if (dividend < 0)
               remainder = -remainder;
          if ((((quotient & 0x7fffffff) * divisor) + remainder) != dividend)
               ov = 1;
          else
               ov = 0;
          if (ov) {
                ASSERT_TRUE(trap_flag);
          } else {
                ASSERT_EQUAL(quotient, get_reg(3));
                ASSERT_EQUAL(remainder, get_reg(2));
          }
      }
  }

  /* Test add logical with random values */
  CTEST(instruct, add_log_rand) {
      int       i;
      srand(125);
      init_cpu();
      for (i = 0; i < testcycles; i++) {
          uint32_t n1 = rand();
          uint32_t n2 = rand();
          uint64_t result = n1+n2;
          int carry = 0;
          set_reg(2, n1);
          set_mem(0x100, n2);
          set_mem(0x400, 0x5e200100);   /* AL 2,100(0,0) */
          test_inst(0x0);
          if (result & 0x100000000) {
              carry = 1;
              result &= 0x0ffffffff;
          }
          if (carry == 0) {
              if (result == 0) {
                  ASSERT_EQUAL(CC0, CC_REG); /* Zero, no carry */
              } else {
                  ASSERT_EQUAL(CC1, CC_REG); /* Nonzero, no carry */
              }
          } else {
              if (result == 0) {
                  ASSERT_EQUAL(CC2, CC_REG); /* Zero, carry */
              } else {
                  ASSERT_EQUAL(CC3, CC_REG); /* Nonzero, carry */
              }
          }
          ASSERT_EQUAL_X(result, get_reg(2));
      }
  }

  /* Test subtract logical with random values */
  CTEST(instruct, sub_log_rand) {
      int    i;
      srand(44);
      init_cpu();
      for (i = 0; i < testcycles; i++) {
          int64_t   n1 = rand();
          int64_t   n2 = rand();
          int64_t   result = n1 + ((n2 ^ 0xffffffff)) + 1;
          int carry = 0;
          set_reg(2,  ((uint32_t)n1) & 0xffffffff);
          set_mem(0x100,  ((uint32_t)n2) & 0xffffffff);
          set_mem(0x400, 0x5f200100);    /* SL 2,100(0,0) */
          test_inst(0x0);
          if ((result & 0x100000000) != 0) {
              carry = 1;
          }
          if (carry == 0) {
              if (result == 0) {
                  ASSERT_EQUAL(CC0, CC_REG); /* Zero, no carry */
              } else {
                  ASSERT_EQUAL(CC1, CC_REG); /* Nonzero, no carry */
              }
          } else {
              if (result == 0) {
                  ASSERT_EQUAL(CC2, CC_REG); /* Zero, carry */
              } else {
                  ASSERT_EQUAL(CC3, CC_REG); /* Nonzero, carry */
              }
          }
          ASSERT_EQUAL_X((uint32_t)(result & 0xffffffff), get_reg(2));
      }
  }

  /* Test store instruction */
  CTEST(instruct, st) {
      init_cpu();
      set_amwp(0); /* EBCDIC */
      set_reg(1, 0x12345678);
      set_reg(2, 0x100);
      set_reg(3, 0x100);
      set_mem(0x400, 0x50123400); /* ST 1,400(2,3) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_mem(0x600));
  }

  /* Test and instruction */
  CTEST(instruct, and) {
      init_cpu();
      set_reg(1, 0x11223344);
      set_reg(2, 0x200);
      set_reg(3, 0x300);
      set_mem(0x954, 0x12345678);
      set_mem(0x400, 0x54123454); /* N 1,454(2,3) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst(0x0);
      ASSERT_EQUAL_X((0x11223344 & 0x12345678), get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG); /* Not zero */
  }

  /* Test compare logical */
  CTEST(instruct, cl) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x200);
      set_reg(3, 0x300);
      set_mem(0x900, 0x12345678);
      set_mem(0x400, 0x55123400); /* CL 1,400(2,3) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* Equal */
  }

  /* Test or instruction */
  CTEST(instruct, or) {
      init_cpu();
      set_reg(1, 0x11223344);
      set_reg(2, 0x200);
      set_reg(3, 0x300);
      set_mem(0x954, 0x12345678);
      set_mem(0x400, 0x56123454); /* O 1,454(2,3) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst(0x0);
      ASSERT_EQUAL_X((0x11223344 | 0x12345678), get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* Test exclusive or */
  CTEST(instruct, xor) {
      init_cpu();
      set_reg(1, 0x11223344);
      set_reg(2, 0x200);
      set_reg(3, 0x300);
      set_mem(0x954, 0x12345678);
      set_mem(0x400, 0x57123454); /* X 1,454(2,3) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst(0x0);
      ASSERT_EQUAL_X((0x11223344 ^ 0x12345678), get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* Test exclusive or zero result */
  CTEST( instruct, xor_zero) {
      init_cpu();
      set_reg(1, 0x11223344);
      set_reg(2, 0x200);
      set_reg(3, 0x300);
      set_mem(0x954, 0x11223344);
      set_mem(0x400, 0x57123454); /* X 1,454(2,3) */
      set_mem(0x404, 0x00000000); /* Prevent fetch of next instruction */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);  /* Zero */
  }


  /* Shift left single */
  CTEST(instruct, sla) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x00000001);
      set_mem(0x400, 0x8b1f2001);   /* SLA 1,1(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 << 2, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);    /* Positive */
  }

  /* Shift left single extrabits */
  CTEST(instruct, sla2) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_reg(2, 0x00000001);
      set_mem(0x400, 0x8b1f2fc1);   /* SLA 1,fc1(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678 << 2, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);    /* Positive */
  }

  /* Shift left single zero */
  CTEST(instruct, sla_zero) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_mem(0x400, 0x8b100000);   /* SLA 1,0(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);    /* Positive */
  }

  /* Shift left single zero negative */
  CTEST(instruct, sla_zero2) {
      init_cpu();
      set_reg(1, 0x92345678);
      set_mem(0x400, 0x8b1f0000);   /* SLA 1,0(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x92345678, get_reg(1)); /* Should be unchanged */
      ASSERT_EQUAL(CC1, CC_REG);    /* Negative */
  }

  /* Shift left single  zero, zero value */
  CTEST(instruct, sla_zero3) {
      init_cpu();
      set_reg(1, 0);
      set_mem(0x400, 0x8b1f0000);   /* SLA 1,0(0) */
      test_inst(0x0);
      ASSERT_EQUAL(0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);    /* Zero */
  }

  /* Shift left single positive overflow */
  CTEST(instruct, sla_over) {
      init_cpu();
      set_reg(1, 0x10000000);
      set_reg(2, 2);              /* Shift by 2 still fits */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x40000000, get_reg(1));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */

      set_reg(1, 0x10000000);
      set_reg(2, 3);              /* Shift by 3 overflows */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);  /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla4) {
      init_cpu();
      set_reg(1, 0x7fffffff);
      set_reg(2, 0x0000001f);     /* Shift by 31 shifts out entire number */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);  /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla5) {
      init_cpu();
      set_reg(1, 0x7fffffff);
      set_reg(2, 0x00000020);     /* Shift by 32 shifts out entire number */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);  /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla6) {
      init_cpu();
      set_reg(1, 0x80000000);
      set_reg(2, 0x0000001f);     /* Shift by 31 shifts out entire number */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x80000000, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);  /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla7) {
      init_cpu();
      set_reg(1, 0x80000000);
      set_reg(2, 21);             /* Shift by 2 should overflow */
      set_mem(0x400, 0x8b1f2000); /* SLA 1,0(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x80000000, get_reg(1));
      ASSERT_EQUAL(CC3, CC_REG);  /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla8) {
      init_cpu();
      set_reg(1, 0x80000001);
      set_reg(2, 0x00000001);
      set_mem(0x400, 0x8b1f2001);  /* SLA 1,1(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x80000004, get_reg(1)); /* Keep the sign */
      ASSERT_EQUAL(CC3, CC_REG);   /* Overflow */
  }

  /* Shift left single */
  CTEST(instruct, sla9) {
      init_cpu();
      set_reg(1, 0xf0000001);
      set_reg(2, 0x00000001);
      set_mem(0x400, 0x8b1f2001);  /* SLA 1,1(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xc0000004, get_reg(1)); /* Keep the sign */
      ASSERT_EQUAL(CC1, CC_REG);   /* Negative */
  }

  /* Shift left logical instruction */
  CTEST(instruct, sll) {
      init_cpu();
      set_reg( 1, 0x82345678);
      set_reg( 2, 0x12340003);    /* Shift 3 bits */
      set_mem(0x400, 0x891f2100); /* SLL 1,100(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x11a2b3c0, get_reg(1));
  }

  /* Shift right logical */
  CTEST(instruct, srl) {
      init_cpu();
      set_reg( 1, 0x82345678);
      set_reg( 2, 0x12340003);    /* Shift 3 bits */
      set_mem(0x400, 0x881f2100); /* SRL 1,100(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x82345678 >> 3, get_reg(1));
  }

  /* Shift right arithmatic */
  CTEST(instruct, sra) {
      init_cpu();
      set_reg( 2, 0x11223344);
      set_mem(0x400, 0x8a2f0105); /* SRA 2,105(0) */ /* Shift right 5 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0089119a, get_reg(2));
  }

  /* Shift right arithmatic */
  CTEST(instruct, sla3) {
      /* From Princ Ops p143 */
      init_cpu();
      set_reg( 2, 0x007f0a72);
      set_mem(0x400, 0x8b2f0008); /* SLA 2,8(0) */ /* Shift left 8 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x7f0a7200, get_reg(2));
  }

  /* Shift right double logical */
  CTEST(instruct, srdl) {
      init_cpu();
      set_reg( 4, 0x12345678);
      set_reg( 5, 0xaabbccdd);
      set_mem(0x400, 0x8c4f0118); /* SRDL 4,118(0) */ /* Shift right 24 (x18) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000012, get_reg(4));
      ASSERT_EQUAL_X(0x345678aa, get_reg(5));
  }

  /* Shift left double logical */
  CTEST(instruct, sldl) {
      init_cpu();
      set_reg( 4, 0x12345678);
      set_reg( 5, 0xaabbccdd);
      set_reg( 6, 8);
      set_mem(0x400, 0x8d4f6100); /* SLDL 4,100(6)  */ /* Shift left 8 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x345678aa, get_reg(4));
      ASSERT_EQUAL_X(0xbbccdd00, get_reg(5));
  }

  /* Shift left double logical */
  CTEST(instruct, sldl2) {
      init_cpu();
      set_reg( 4, 0x12345678);
      set_reg( 5, 0x00010001);
      set_mem(0x400, 0x8d4f051b); /* SLDL 4,51b(0) */ /* Shift left 27 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xc0000800, get_reg(4));
      ASSERT_EQUAL_X(0x08000000, get_reg(5));
  }

  /* Shift left double logical */
  CTEST(instruct, sldl3) {
      init_cpu();
      set_mem(0x400, 0x8d1f2100); /* SLDL 1,100(2) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Shift double right arithmatic */
  CTEST(instruct, srda) {
      init_cpu();
      set_reg( 4, 0x12345678);
      set_reg( 5, 0xaabbccdd);
      set_mem(0x400, 0x8e4f0118); /* SRDA 4,118(0) */ /* Shift right 24 (x18) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000012, get_reg(4));
      ASSERT_EQUAL_X(0x345678aa, get_reg(5));
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Shift double right arithmatic */
  CTEST(instruct, srda_zero) {
      init_cpu();
      set_reg( 4, 0x02345678);
      set_reg( 5, 0xaabbccdd);
      set_mem(0x400, 0x8e4f013c); /* SRDA 4,13c(0) */ /*  Shift right 60 (x3c) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_reg(4));
      ASSERT_EQUAL_X(0x00000000, get_reg(5));
      ASSERT_EQUAL(CC0, CC_REG);  /* Zero */
  }

  /* Shift double right arithmatic */
  CTEST(instruct, srda2) {
      init_cpu();
      set_reg( 4, 0x92345678);
      set_reg( 5, 0xaabbccdd);
      set_mem(0x400, 0x8e4f0118); /* SRDA 4,118(0) */ /* Shift right 24 (x18) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xffffff92, get_reg(4));
      ASSERT_EQUAL_X(0x345678aa, get_reg(5));
      ASSERT_EQUAL(CC1, CC_REG);  /* Negative */
  }

  /* Shift double right arithmatic */
  CTEST(instruct, slda) {
      /* From Princ Ops p143 */
      init_cpu();
      set_reg( 2, 0x007f0a72);
      set_reg( 3, 0xfedcba98);
      set_mem(0x400, 0x8f2f001f); /* SLDA 2,1f(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x7f6e5d4c, get_reg(2));
      ASSERT_EQUAL_X(0x00000000, get_reg(3));
  }

  CTEST(instruct, slda1) {
      init_cpu();
      set_reg( 2, 0xffffffff);
      set_reg( 3, 0xffffe070);
      set_mem(0x400, 0x8f2f0030); /* SLDA 2,30(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xe0700000, get_reg(2));
      ASSERT_EQUAL_X(0x00000000, get_reg(3));
  }

  CTEST(instruct, slda2) {
      init_cpu();
      set_reg( 2, 0xffffffff);
      set_reg( 3, 0xc0506070);
      set_mem(0x400, 0x8f2f0020); /* SLDA 2,20(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xc0506070, get_reg(2));
      ASSERT_EQUAL_X(0x00000000, get_reg(3));
  }

  CTEST(instruct, slda3) {
      init_cpu();
      set_reg( 2, 0xff902030);
      set_reg( 3, 0x40506070);
      set_mem(0x400, 0x8f2f0008); /* SLDA 2,8(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x90203040, get_reg(2));
      ASSERT_EQUAL_X(0x50607000, get_reg(3));
  }

  CTEST(instruct, slda4) {
      init_cpu();
      set_reg( 2, 0x00000000);
      set_reg( 3, 0x000076f7);
      set_mem(0x400, 0x8f2f0030); /* SLDA 2,30(0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x76f70000, get_reg(2));
      ASSERT_EQUAL_X(0x00000000, get_reg(3));
  }

  /* Load multiple registers */
  CTEST(instruct, lm) {
      init_cpu();
      set_reg( 3, 0x10);
      set_mem(0x110, 0x12345678);
      set_mem(0x114, 0x11223344);
      set_mem(0x118, 0x55667788);
      set_mem(0x11c, 0x99aabbcc);
      set_mem(0x400, 0x98253100); /* LM 2,5,100(3) */
      /* Load registers 2 through 5 starting at 0x110 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_reg(2));
      ASSERT_EQUAL_X(0x11223344, get_reg(3));
      ASSERT_EQUAL_X(0x55667788, get_reg(4));
      ASSERT_EQUAL_X(0x99aabbcc, get_reg(5));
  }

  /* Store multiple registers */
  CTEST(instruct, stm) {
      init_cpu();
      /* From Princ Ops p143 */
      set_reg( 14, 0x00002563);
      set_reg( 15, 0x00012736);
      set_reg( 0, 0x12430062);
      set_reg( 1, 0x73261257);
      set_reg( 6, 0x00004000);
      set_mem(0x400, 0x90e16050); /* STM 14,1,50(6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00002563, get_mem(0x4050));
      ASSERT_EQUAL_X(0x00012736, get_mem(0x4054));
      ASSERT_EQUAL_X(0x12430062, get_mem(0x4058));
      ASSERT_EQUAL_X(0x73261257, get_mem(0x405C));
  }

  /* Test under mask */
  CTEST(instruct, tm) {
      init_cpu();
      /* From Princ Ops p147 */
      set_mem(0x9998, 0xaafbaaaa);
      set_reg( 9, 0x00009990);
      set_mem(0x400, 0x91c39009); /* TM 9(9),c3 */
      test_inst(0x0);
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Test under mask */
  CTEST(instruct, tm2) {
      /* From Princ Ops p147 */
      init_cpu();
      set_mem(0x9998, 0xaa3caaaa);
      set_reg( 9, 0x00009990);
      set_mem(0x400, 0x91c39009); /* TM 9(9),c3 */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Test under mask */
  CTEST(instruct, tm3) {
      /* From Princ Ops p147 */
      init_cpu();
      set_mem(0x9998, 0xaa3caaaa);
      set_reg( 9, 0x00009990);
      set_mem(0x400, 0x91009008); /* TM 9(9),c3 */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Test under mask */
  CTEST(instruct, tm4) {
      /* From Princ Ops p147 */
      init_cpu();
      set_mem(0x9998, 0xf03caaaa);
      set_reg( 9, 0x00009990);
      set_mem(0x400, 0x91f09008); /* TM 9(9),c3 */
      test_inst(0x0);
      ASSERT_EQUAL(CC3, CC_REG);
  }

  /* Test under mask */
  CTEST(instruct, tm5) {
      /* From Princ Ops p147 */
      init_cpu();
      set_mem(0x9998, 0xa0f8aaaa);
      set_reg( 9, 0x00009990);
      set_mem(0x400, 0x910c9009); /* TM 9(9),c3 */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);
  }

  /* Test to convert to binary */
  CTEST(instruct, cvb) {
      /* Example from Principles of Operation p122 */
      init_cpu();
      set_reg(5, 50); /* Example seems to have addresses in decimal? */
      set_reg(6, 900);
      set_mem(1000, 0x00000000);
      set_mem(1004, 0x0025594f);
      set_mem(0x400, 0x4f756032);      /*  CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(25594, get_reg(7)); /* Note: decimal, not hex); */
  }

  /* Test convert to binary with bad sign */
  CTEST(instruct, cvb_bad_sign) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x00000000);
      set_mem(1004, 0x00255941);  /* 1 is not a valid sign */
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Test convert to binary with bad digit */
  CTEST(instruct, cvb_bad_digit) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x00000000);
      set_mem(1004, 0x002a594f);
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Test unaligned access */
  CTEST(instruct, cvb_unalign) {
      init_cpu();
      set_reg(5, 0);
      set_reg(6, 0);
      set_mem(1000, 0x00000000);
      set_mem(1004, 0x002a594f);
      set_mem(0x400, 0x4f756034); /* CVB 7,34(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
      set_mem(0x400, 0x4f756031); /* CVB 7,31(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Test for overflow */
  CTEST(instruct, cvb_overflow) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x00000214);
      set_mem(1004, 0x8000000f);
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(2148000000, get_reg(7)); /* Note: decimal, not hex */
  }

  /* Test for larger overflow */
  CTEST(instruct, cvb_big_overflow) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x12345678);
      set_mem(1004, 0x4800000f);
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Test with large number */
  CTEST(instruct, cvb_large) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x00000021);
      set_mem(1004, 0x2345678f);
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(212345678, (int32_t)get_reg(7)); /* Note: decimal, not hex */
  }

  /* Test negative */
  CTEST(instruct, cvb_neg) {
      init_cpu();
      set_reg(5, 50);
      set_reg(6, 900);
      set_mem(1000, 0x00000000);
      set_mem(1004, 0x0025594d);  /* d is negative */
      set_mem(0x400, 0x4f756032); /* CVB 7,32(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL((-25594), (int32_t)get_reg(7)); /* Note: decimal, not hex */
  }

  /* test model 50 case QE900/073C, CLF 112 */
  CTEST(instruct, cvb2) {
      init_cpu();
      set_reg(5, 0x100);
      set_reg(6, 0x200);
      set_mem(0x500, 0);
      set_mem(0x504, 0x1234567f); /* Decimal 1234567+ */
      set_mem(0x400, 0x4f156200); /* CVB 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL(1234567, get_reg(1)); /* Note: decimal, not hex */
  }

  /* Second test with negative */
  CTEST(instruct, cvb_neg2) {
      init_cpu();
      set_reg(5, 0x100);
      set_reg(6, 0x200);
      set_mem(0x500, 0);
      set_mem(0x504, 0x1234567b); /* Decimal 1234567- */
      set_mem(0x400, 0x4f156200); /* CVB 1,200(5,6) */
      test_inst(0x0);
      ASSERT_EQUAL((-1234567), (int32_t)get_reg(1)); /* Note: decimal, not hex */
  }

  /* Test convert to decimal */
  CTEST(instruct, cvd) {
      init_cpu();
      /* Princ Ops p142 */
      set_reg(1, 0x00000f0f);     /* 3855 dec */
      set_reg(13, 0x00007600);
      set_amwp(0); /* EBCDIC */
      set_mem(0x400, 0x4e10d008); /* CVD 1,8(0,13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_mem(0x7608));
      ASSERT_EQUAL_X(0x0003855c, get_mem(0x760C));
  }

  /* Test convert to decimal with ascii mode  */
  CTEST(instruct, cvd_ascii) {
      init_cpu();
      set_reg(1, 0x00000f0f);     /* 3855 dec */
      set_reg(13, 0x00007600);
      set_amwp(8);                /* ASCII */
      set_mem(0x400, 0x4e10d008); /* CVD 1,8(0,13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_mem(0x7608));
      ASSERT_EQUAL_X(0x0003855a, get_mem(0x760c));
  }

  /* Same test with ebcdic mode */
  CTEST(instruct, cvd2) {
      init_cpu();
      /* Princ Ops p142 */
      set_reg(1, -0x00000f0f);    /* -3855 dec */
      set_reg(13, 0x00007600);
      set_amwp(0);                /* EBCDIC */
      set_mem(0x400, 0x4e10d008); /* CVD 1,8(0,13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_mem(0x7608));
      ASSERT_EQUAL_X(0x0003855d, get_mem(0x760C));
  }

  /* Test negative number */
  CTEST(instruct, cvd_ascii2) {
      init_cpu();
      set_reg(1, -0x00000f0f);    /* -3855 dec */
      set_reg(13, 0x00007600);
      set_amwp(8);                /* ASCII */
      set_mem(0x400, 0x4e10d008); /* CVD 1,8(0,13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000000, get_mem(0x7608));
      ASSERT_EQUAL_X(0x0003855b, get_mem(0x760c));
  }

  /* Move immeditate */
  CTEST(instruct, mvi) {
      init_cpu();
      set_reg( 1, 0x3456);
      set_cc(CC2);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x92421010); /* MVI 10(1),42 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12344278, get_mem(0x3464));
      ASSERT_EQUAL(CC2, CC_REG);  /* Unchanged */
  }

  /* Move immediate */
  CTEST(instruct, mvi2) {
      init_cpu();
      set_mem(0x100, 0x11223344);
      set_reg( 1, 1);
      set_mem(0x400, 0x92551100); /* MVI 100(1),55 */ /* Move byte 55 to location 101 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x11553344, get_mem(0x100));
  }

  /* And immediate */
  CTEST(instruct, ni) {
      init_cpu();
      set_reg( 1, 0x3456);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x94f01010); /* NI 10(1),f0 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345078, get_mem(0x3464));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* And immediate */
  CTEST(instruct, ni2) {
      init_cpu();
      set_reg( 1, 0x3456);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x940f1010); /* NI 10(1),f0 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12340678, get_mem(0x3464));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* And immediate */
  CTEST(instruct, ni3) {
      init_cpu();
      set_reg( 1, 0x3456);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x94aa1010); /* NI 10(1),f0 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12340278, get_mem(0x3464));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* And immediate */
  CTEST(instruct, ni_zero) {
      init_cpu();
      set_reg( 1, 0x3456);
      set_mem(0x3464, 0x12345678);
      set_mem(0x400, 0x94001010); /* NI 10(1),0 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12340078, get_mem(0x3464));
      ASSERT_EQUAL(CC0, CC_REG);  /* Zero */
  }

  /* Compare logical immediate */
  CTEST(instruct, cli_zero) {
      init_cpu();
      set_reg( 1, 0x3452);
      set_mem(0x3460, 0x12345678);
      set_mem(0x400, 0x95561010); /* CLI 10(1),56 */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* Equal */
  }

  /* Compare logical immediate */
  CTEST(instruct, cli_low) {
      init_cpu();
      set_reg( 1, 0x3452);
      set_mem(0x3460, 0x12345678);
      set_mem(0x400, 0x95ff1010); /* CLI 10(1),ff */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* First operand is low */
  }

  /* Compare logical immediate */
  CTEST(instruct, cli_all) {
      int  i;
      init_cpu();
      for (i = 0; i < 256 && i < testcycles * 3; i++) {
          set_reg( 1, 0x3442);
          set_mem(0x3450, 0x12345678);
          set_mem(0x400, 0x95001010 | (i << 16)); /* CLI 10(1),i */
          test_inst(0x0);
          if (i == 0x56) {
              ASSERT_EQUAL(CC0, CC_REG); /* Equal */
          } else if (i < 0x56) {
              ASSERT_EQUAL(CC2, CC_REG); /* First operand is high */
          } else {
              ASSERT_EQUAL(CC1, CC_REG); /* First operand is low */
          }
      }
  }

  /* Or immediate */
  CTEST(instruct, oi) {
      init_cpu();
      set_reg( 1, 2);
      set_mem(0x1000, 0x12345678);
      set_mem(0x400, 0x96421fff); /* OI fff(1),42 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12765678, get_mem(0x1000));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* Or immediate */
  CTEST(instruct, xi) {
      init_cpu();
      set_reg( 0, 0x100);         /* Not used */
      set_mem(0x120, 0x12345678);
      set_mem(0x400, 0x970f0123); /* XI 123(0),f */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345677, get_mem(0x120));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* Move numeric */
  CTEST(instruct, mvn) {
      /* From Princ Ops p144 */
      init_cpu();
      set_mem(0x7090, 0xc1c2c3c4);
      set_mem(0x7094, 0xc5c6c7c8);
      set_mem(0x7040, 0xaaf0f1f2);
      set_mem(0x7044, 0xf3f4f5f6);
      set_mem(0x7048, 0xf7f8aaaa);
      set_reg( 14, 0x00007090);
      set_reg( 15, 0x00007040);
      set_mem(0x400, 0xd103f001);
      set_mem(0x404, 0xe000aaaa); /* MVN 1(4,15),0(14) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xc1c2c3c4, get_mem(0x7090));
      ASSERT_EQUAL_X(0xaaf1f2f3, get_mem(0x7040));
      ASSERT_EQUAL_X(0xf4f4f5f6, get_mem(0x7044));
      ASSERT_EQUAL_X(0xf7f8aaaa, get_mem(0x7048));
  }

  /* Move character */
  CTEST(instruct, mvc) {
      init_cpu();
      set_mem(0x100, 0x12345678);
      set_mem(0x200, 0x11223344);
      set_mem(0x400, 0xd2030100);
      set_mem(0x404, 0x02000000); /* MVC 100(4,0),200(0) */ /* Move 4 bytes from 200 to 100 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x11223344, get_mem(0x100));
      ASSERT_EQUAL_X(0x11223344, get_mem(0x200)); /* Unchanged */
  }

  /* Move character */
  CTEST(instruct, mvc2) {
      init_cpu();
      set_mem(0x100, 0x12345678);
      set_mem(0x104, 0xabcdef01);
      set_reg( 1, 2);
      set_reg( 2, 0);
      set_mem(0x400, 0xd2011100);
      set_mem(0x404, 0x01050000); /* MVC 100(2,1),105(0) */ /* Move 2 bytes from 105 to 102 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x1234cdef, get_mem(0x100));
      ASSERT_EQUAL_X(0xabcdef01, get_mem(0x104)); /* Unchanged */
  }

  /* Move zones */
  CTEST(instruct, mvz) {
      init_cpu();
      /* From Princ Ops page 144 */
      set_mem(0x800, 0xf1c2f3c4);
      set_mem(0x804, 0xf5c6aabb);
      set_reg( 15, 0x00000800);
      set_mem(0x400, 0xd304f001);
      set_mem(0x404, 0xf000aabb);  /* MVZ 1(5,15),0(15) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xf1f2f3f4, get_mem(0x800));
      ASSERT_EQUAL_X(0xf5f6aabb, get_mem(0x804));
  }

  /* And characters */
  CTEST(instruct, nc) {
      init_cpu();
      set_mem(0x358, 0x00001790);
      set_mem(0x360, 0x00001401);
      set_reg( 7, 0x00000358);
      set_mem(0x400, 0xd4037000);
      set_mem(0x404, 0x7008aaaa); /* NC 0(4,7),8(7) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001400, get_mem(0x358));
  }

  /* Compare logical character */
  CTEST(instruct, clc_equal) {
      init_cpu();
      set_reg( 1, 0x100);
      set_reg( 2, 0x100);
      set_mem(0x200, 0x12345633);
      set_mem(0x300, 0x12345644);
      set_mem(0x400, 0xd5021100);
      set_mem(0x404, 0x22000000); /* CLC 100(3,1),200(2) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* equal */
  }

  /* Compare logical character */
  CTEST(instruct, clc) {
      init_cpu();
      set_reg( 1, 0x100);
      set_reg( 2, 0x100);
      set_mem(0x200, 0x12345678);
      set_mem(0x300, 0x12345678);
      /* 123456 vs 345678 because of offset */
      set_mem(0x400, 0xd5021100);
      set_mem(0x404, 0x22010000); /* CLC 100(3,1),201(2) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* first operand is low */
  }

  /* Or character */
  CTEST(instruct, oc) {
      init_cpu();
      set_mem(0x358, 0x00001790);
      set_mem(0x360, 0x00001401);
      set_reg( 7, 0x00000358);
      set_mem(0x400, 0xd6037000);
      set_mem(0x404, 0x7008aaaa); /* OC 0(4,7),8(7) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001791, get_mem(0x358));
  }

  /* exclusive or character */
  CTEST(instruct, xc) {
      /* From Princ Ops p146 */
      init_cpu();
      set_mem(0x358, 0x00001790);
      set_mem(0x360, 0x00001401);
      set_reg( 7, 0x00000358);
      set_mem(0x400, 0xd7037000);
      set_mem(0x404, 0x7008aaaa); /* XC 0(4,7),8(7) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00000391, get_mem(0x358));
      set_mem(0x400, 0xd7037008);
      set_mem(0x404, 0x7000aaaa); /* XC 8(4,7),0(7) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001790, get_mem(0x360));
      set_mem(0x400, 0xd7037000);
      set_mem(0x404, 0x7008aaaa); /* XC 0(4,7),8(7) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001401, get_mem(0x358));
  }

  /* translate */
  CTEST(instruct, tr) {
      unsigned int   i;

      /* Based on Princ Ops p147 */
      init_cpu();
      for (i = 0; i < 256; i += 4) {
          /* Table increments each char by 3. Don"t worry about wrapping. */
          set_mem(0x1000 + i, (((i + 3) << 24) |
                               ((i + 4) << 16) |
                               ((i + 5) << 8) |
                               (i + 6)));
      }
      set_mem(0x2100, 0x12345678);
      set_mem(0x2104, 0xabcdef01);
      set_mem(0x2108, 0x11223344);
      set_mem(0x210c, 0x55667788);
      set_mem(0x2110, 0x99aabbcc);
      set_reg( 12, 0x00002100);
      set_reg( 15, 0x00001000);
      set_mem(0x400, 0xdc13c000);
      set_mem(0x404, 0xf000aaaa); /* TR 0(20,12),0(15) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x1537597b, get_mem(0x2100));
      ASSERT_EQUAL_X(0xaed0f204, get_mem(0x2104));
      ASSERT_EQUAL_X(0x14253647, get_mem(0x2108));
      ASSERT_EQUAL_X(0x58697a8b, get_mem(0x210c));
      ASSERT_EQUAL_X(0x9cadbecf, get_mem(0x2110));
  }

  /* Translate and test */
  CTEST(instruct, trt) {
      int   i;
      /* Based on Princ Ops p147 */
      init_cpu();
      for (i = 0; i < 256; i += 4) {
          set_mem(0x2000 + i, 0);
      }
      set_mem(0x204c, 0x10202500);
      set_mem(0x2050, 0x90000000);
      set_mem(0x2058, 0x00000030);
      set_mem(0x205c, 0x35404500);
      set_mem(0x2060, 0x80850000);
      set_mem(0x2068, 0x00000050);
      set_mem(0x206c, 0x55000000);
      set_mem(0x2078, 0x00000060);
      set_mem(0x207c, 0x65707500);

      set_mem(0x3000, 0x40404040);
      set_mem(0x3004, 0x40e4d5d7);  /*  UNP */
      set_mem(0x3008, 0xd2404040);  /* K    */
      set_mem(0x300c, 0x4040d7d9);  /*   PR */
      set_mem(0x3010, 0xd6e4e34d);  /* OUT( */
      set_mem(0x3014, 0xf95d6be6);  /* 9),W */
      set_mem(0x3018, 0xd6d9c44d);  /* ORD( */
      set_mem(0x301C, 0xf55d0000);  /* 5)   */

      set_reg(1, 0x3000);
      set_reg(2, 0);
      set_reg(15, 0x2000);

      set_mem(0x400, 0xdd1d1000); /* TRT 0(30,1),0(15) */
      set_mem(0x404, 0xf0000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00003013, get_reg(1)); /* Match at 3013 */
      ASSERT_EQUAL_X(0x00000020, get_reg(2)); /* Function value from table */
      ASSERT_EQUAL(CC1, CC_REG); /* not completed */
  }

  /* Translate and test */
  CTEST(instruct, trt1) {
      int   i;
      init_cpu();
      /* Based on Princ Ops p147 */
      for (i = 0; i < 256; i += 4) {
          set_mem(0x1000 + i, 0);
      }
      set_mem(0x2020, 0x10203040);

      set_mem(0x3000, 0x12345621); /* 21 will match table entry 20 */
      set_mem(0x3004, 0x11223344);
      set_mem(0x3008, 0x55667788);
      set_mem(0x300c, 0x99aabbcc);
      set_mem(0x400, 0xdd0f1000);
      set_mem(0x404, 0xf000aaaa);  /* TRT 0(16,1),0(15) */
      set_reg(1, 0x3000);
      set_reg(2, 0);
      set_reg(15, 0x2000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00003003, get_reg(1)); /* Match at 3003 */
      ASSERT_EQUAL_X(0x00000020, get_reg(2)); /* Function value from table */
      ASSERT_EQUAL(CC1, CC_REG);              /* not completed */
  }

  /* Add packed short field */
  DTEST(instruct, ap_small) {
      init_cpu();
      set_mem(0x100, 0x0000002c); /* 2+ */
      set_mem(0x200, 0x00003c00); /* 3+ */
      set_mem(0x400, 0xfa000103); /* AP 103(1,0),202(1,0) */
      set_mem(0x404, 0x02020000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0000005c, get_mem(0x100)); /* 5 */
      ASSERT_EQUAL(CC2, CC_REG);   /* Positive */
  }

  /* Add packed */
  DTEST(instruct, ap_one) {
      init_cpu();
      set_mem(0x100, 0x2888011c); /* 2888011+ */
      set_mem(0x200, 0x1112292c); /* 1112292+ */
      set_mem(0x400, 0xfa330100); /* AP 100(4,0),200(4,0) */
      set_mem(0x404, 0x02000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4000303c, get_mem(0x100)); /* 4000303+ */
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Add packed */
  DTEST(instruct, ap_one2) {
      init_cpu();
      set_mem(0x100, 0x0000002c); /* 2+ */
      set_mem(0x200, 0x0000003c); /* 3+ */
      set_mem(0x400, 0xfa330100); /* AP 100(4,0),200(4,0) */
      set_mem(0x404, 0x02000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0000005c, get_mem(0x100)); /* 5+ */
      ASSERT_EQUAL(CC2, CC_REG); /* Positive */
  }

  /* Add packed with offset */
  DTEST(instruct, ap_offset) {
      init_cpu();
      set_mem(0x100, 0x0043212c); /* 2+ */
      set_mem(0x200, 0x0023413c); /* 3+ */
      set_mem(0x400, 0xfa220101); /* AP 101(3,0),201(3,0) */
      set_mem(0x404, 0x02010000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0066625c, get_mem(0x100)); /* 5+ */
      ASSERT_EQUAL(CC2, CC_REG); /* Positive */
  }

  /* Add packed no offset */
  DTEST(instruct, ap_nooffset) {
      init_cpu();
      set_mem(0x100, 0x0043212c); /* 2+ */
      set_mem(0x200, 0x0023413c); /* 3+ */
      set_mem(0x400, 0xfa330100); /* AP 100(4,0),200(4,0) */
      set_mem(0x404, 0x02000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0066625c, get_mem(0x100)); /* 5+ */
      ASSERT_EQUAL(CC2, CC_REG);  /* Positive */
  }

  /* Add packed offset */
  DTEST(instruct, ap_offset2) {
      /* Example from Princ Ops p136.2 */
      init_cpu();
      set_reg(12, 0x00002000);
      set_reg(13, 0x000004fd);
      set_mem(0x2000, 0x38460d00); /* 38460- */
      set_mem(0x500, 0x0112345c);  /* 112345+ */
      set_mem(0x400, 0xfa23c000);  /* AP 0(3,12),3(4,13) */
      set_mem(0x404, 0xd0030000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x73885c00, get_mem(0x2000)); /* 73885+ */
      ASSERT_EQUAL(CC2, CC_REG);   /* Positive */
  }

  /* Add packed */
  DTEST(instruct, ap) {
      /* PrincOps p 150 */
      init_cpu();
      set_reg( 12, 0x00002000);
      set_reg( 13, 0x000004fd);
      set_mem(0x2000, 0x0038460d);
      set_mem(0x500,  0x0112345c);
      set_mem(0x400, 0xfa33c000);
      set_mem(0x404, 0xd0030000);  /* AP 0(4, 12), 3(4, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0073885c, get_mem(0x2000));
      ASSERT_EQUAL(CC2, CC_REG);   /* Positive */
  }

  /* Edit test */
  DTEST(instruct, ed) {
      /* Princ Ops page 149 */
      init_cpu();
      set_amwp(0);
      set_reg(12, 0x1000);
      set_mem(0x1200, 0x0257426c);
      set_mem(0x1000, 0x4020206b);
      set_mem(0x1004, 0x2020214b);
      set_mem(0x1008, 0x202040c3);
      set_mem(0x100c, 0xd9ffffff);
      set_mem(0x400, 0xde0cc000);
      set_mem(0x404, 0xc200aaaa); /* ED 0(13,12),200(12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4040f26b, get_mem(0x1000));
      ASSERT_EQUAL_X(0xf5f7f44b, get_mem(0x1004));
      ASSERT_EQUAL_X(0xf2f64040, get_mem(0x1008));
      ASSERT_EQUAL_X(0x40ffffff, get_mem(0x100c));
      ASSERT_EQUAL(CC2, CC_REG);  /* Result greater than zero */
  }

  /* Edit test */
  DTEST(instruct, ed2) {
      /* Princ Ops page 149 */
      init_cpu();
      set_reg(12, 0x1000);
      set_mem(0x1200, 0x0000026d);
      set_mem(0x1000, 0x4020206b);
      set_mem(0x1004, 0x2020214b);
      set_mem(0x1008, 0x202040c3);
      set_mem(0x100c, 0xd9ffffff);
      set_mem(0x400, 0xde0cc000);
      set_mem(0x404, 0xc200aaaa); /* ED 0(13,12),200(12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x40404040, get_mem(0x1000));
      ASSERT_EQUAL_X(0x4040404b, get_mem(0x1004));
      ASSERT_EQUAL_X(0xf2f640c3, get_mem(0x1008));
      ASSERT_EQUAL_X(0xd9ffffff, get_mem(0x100c));
      ASSERT_EQUAL(CC1, CC_REG);  /* Result less than zero */
  }

  /* Edit and mark */
  DTEST(instruct, edmk) {
      init_cpu();
      set_reg( 1, 0xaabbccdd);
      set_reg(12, 0x1000);
      set_mem(0x1200, 0x0000026d);
      set_mem(0x1000, 0x4020206b);
      set_mem(0x1004, 0x2020214b);
      set_mem(0x1008, 0x202040c3);
      set_mem(0x100c, 0xd9ffffff);
      set_mem(0x400, 0xdf0cc000);
      set_mem(0x404, 0xc200aaaa); /* ED 0(13,12),200(12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x40404040, get_mem(0x1000));
      ASSERT_EQUAL_X(0x4040404b, get_mem(0x1004));
      ASSERT_EQUAL_X(0xf2f640c3, get_mem(0x1008));
      ASSERT_EQUAL_X(0xd9ffffff, get_mem(0x100c));
      ASSERT_EQUAL(CC1, CC_REG); /* Result less than zero */
      ASSERT_EQUAL_X(0xaabbccdd, get_reg(1)); /* Need to adjust this address */
  }

  /* Edit and mark */
  DTEST(instruct, edmk2) {
      init_cpu();
      set_reg( 1, 0xaabbccdd);
      set_reg(12, 0x1000);
      set_mem(0x1200, 0x0000026d);
      set_mem(0x1000, 0x4020206b);
      set_mem(0x1004, 0x2020204b);
      set_mem(0x1008, 0x202040c3);
      set_mem(0x100c, 0xd9ffffff);
      set_mem(0x400, 0xdf0cc000);
      set_mem(0x404, 0xc200aaaa); /* ED 0(13,12),200(12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x40404040, get_mem(0x1000));
      ASSERT_EQUAL_X(0x40404040, get_mem(0x1004));
      ASSERT_EQUAL_X(0xf2f640c3, get_mem(0x1008));
      ASSERT_EQUAL_X(0xd9ffffff, get_mem(0x100c));
      ASSERT_EQUAL(CC1, CC_REG); /* Result less than zero) */
      ASSERT_EQUAL_X(0xaa001008, get_reg(1)); /* Need to adjust this address) */
  }

  /* Edit and mark */
  DTEST(instruct, edmk3) {
      init_cpu();
      set_reg( 1, 0xaabbccdd);
      set_reg(12, 0x1000);
      set_mem(0x1200, 0x0257426d);
      set_mem(0x1000, 0x4020206b);
      set_mem(0x1004, 0x2020204b);
      set_mem(0x1008, 0x202040c3);
      set_mem(0x100c, 0xd9ffffff);
      set_mem(0x400, 0xdf0cc000);
      set_mem(0x404, 0xc200aaaa); /* ED 0(13,12),200(12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4040f26b, get_mem(0x1000));
      ASSERT_EQUAL_X(0xf5f7f44b, get_mem(0x1004));
      ASSERT_EQUAL_X(0xf2f640c3, get_mem(0x1008));
      ASSERT_EQUAL_X(0xd9ffffff, get_mem(0x100c));
      ASSERT_EQUAL(CC1, CC_REG); /* Result less than zero */
      ASSERT_EQUAL_X(0xaa001002, get_reg(1)); /* Need to adjust this address */
  }

  /* Move offset */
  CTEST(instruct, mvo) {
      /* Princ Ops 152 */
      set_reg( 12, 0x00005600);
      set_reg( 15, 0x00004500);
      set_mem(0x5600, 0x7788990c);
      set_mem(0x4500, 0x123456ff);
      set_mem(0x400, 0xf132c000);
      set_mem(0x404, 0xf0000000); /* MVO 0(4, 12), 0(3, 15) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0123456c, get_mem(0x5600));
  }

  /* Pack instruction */
  CTEST(instruct, pack) {
      /* Princ Ops p151 */
      init_cpu();
      set_reg( 12, 0x00001000);
      set_mem(0x1000, 0xf1f2f3f4);
      set_mem(0x1004, 0xc5000000);
      set_mem(0x400, 0xf244c000);
      set_mem(0x404, 0xc0000000);  /* PACK 0(5, 12), 0(5, 12) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00001234, get_mem(0x1000));
      ASSERT_EQUAL_X(0x5c000000, get_mem(0x1004));
  }

  /* Unpack */
  CTEST(instruct, unpk) {
      /* Princ Ops p151 */
      init_cpu();
      set_reg( 12, 0x00001000);
      set_reg( 13, 0x00002500);
      set_mem(0x2500, 0xaa12345d);
      set_mem(0x1000, 0xffffffff);
      set_mem(0x1004, 0xffffffff);
      set_mem(0x400, 0xf342c000);
      set_mem(0x404, 0xd0010000); /* UNPK 0(5, 12), 1(3, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xf1f2f3f4, get_mem(0x1000));
      ASSERT_EQUAL_X(0xd5ffffff, get_mem(0x1004));
  }

  /* Zap */
  DTEST(instruct, zap) {
      /* Princ Ops p150 */
      init_cpu();
      set_reg( 9, 0x00004000);
      set_mem(0x4000, 0x12345678);
      set_mem(0x4004, 0x90aaaaaa);
      set_mem(0x4500, 0x38460dff);
      set_mem(0x400, 0xf8429000);
      set_mem(0x404, 0x95000000); /* ZAP 0(5, 9), 500(3, 9) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x00003846, get_mem(0x4000));
      ASSERT_EQUAL_X(0x0daaaaaa, get_mem(0x4004));
      ASSERT_EQUAL(CC1, CC_REG); /* Result less than zero */
  }

  /* Zap short field */
  DTEST(instruct, zap_short) {
      init_cpu();
      set_amwp(8);                /* ASCII */
      set_mem(0x100, 0x2a000000); /* 2+ */
      set_mem(0x200, 0x3a000000); /* 3+ */
      set_mem(0x400, 0xf8000100);
      set_mem(0x404, 0x02000000); /* ZAP 100(1, 0), 200(1, 0) */
      test_inst(0x0);
      ASSERT_EQUAL(0x3a000000, get_mem(0x100)); /* 3+ */
  }

  /* Zap with offset */
  DTEST(instruct, zap_offest) {
      init_cpu();
      set_amwp(8);                 /* ASCII */
      set_mem(0x100, 0x002a0000);  /* 2+ */
      set_mem(0x200, 0x00003a00);  /* 3+ */
      set_mem(0x400, 0xf8000101);
      set_mem(0x404, 0x02020000);  /* ZAP 101(1, 0), 202(1, 0) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x003a0000, get_mem(0x100)); /* 3+ */
  }

  /* Compare packed */
  DTEST(instruct, cp) {
      /* Princ Op page 150 */
      init_cpu();
      set_reg( 12, 0x00000600);
      set_reg( 13, 0x00000400);
      set_mem(0x700, 0x1725356d);
      set_mem(0x500, 0x0672142d);
      set_mem(0x400, 0xf933c100);
      set_mem(0x404, 0xd1000000);  /* CP 100(4, 12), 100(4, 13) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);   /* First lower */
  }

  /* Compare packed  equal */
  DTEST(instruct, cp0) {
      init_cpu();
      set_reg( 12, 0x00000600);
      set_reg( 13, 0x00000400);
      set_mem(0x700, 0x1725356d);
      set_mem(0x500, 0x00172535);
      set_mem(0x504, 0x6d000000);
      set_mem(0x400, 0xf933c100);
      set_mem(0x404, 0xd1010000);   /* CP 100(4, 12), 101(4, 13) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);    /* Equal */
  }

  /* Compare packed first higher */
  DTEST(instruct, cp3) {
      init_cpu();
      set_reg( 12, 0x00000600);
      set_reg( 13, 0x00000400);
      set_mem(0x700, 0x1725346d);
      set_mem(0x500, 0x00172535);
      set_mem(0x504, 0x6d000000);
      set_mem(0x400, 0xf933c100);
      set_mem(0x404, 0xd1010000);  /* CP 100(4, 12), 101(4, 13) */
      test_inst(0x0);
      ASSERT_EQUAL(CC2, CC_REG);   /* First higher */
  }

  /* Subtract packed */
  DTEST(instruct, sp) {
      init_cpu();
      set_reg( 12, 0x00002000);
      set_reg( 13, 0x000004fc);
      set_mem(0x2000,  0x0038460c);
      set_mem(0x500,   0x0112345c);
      set_mem(0x400, 0xfb33c000);
      set_mem(0x404, 0xd0040000);  /* SP 0(4, 12), 3(4, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x0073885d, get_mem(0x2000));
      ASSERT_EQUAL(CC1, CC_REG);   /* Negative */
  }

  /* Multiply packed */
  DTEST(instruct, mp) {
      /* PrincOps p 151 */
      init_cpu();
      set_reg( 4, 0x00001200);
      set_reg( 6, 0x00000500);
      set_mem(0x1300, 0x00003846);
      set_mem(0x1304, 0x0cffffff);
      set_mem(0x500, 0x321dffff);
      set_mem(0x400, 0xfc414100);
      set_mem(0x404, 0x60000000); /* MP 100(5, 4), 0(2, 6) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x01234566, get_mem(0x1300));
      ASSERT_EQUAL_X(0x0dffffff, get_mem(0x1304));
      ASSERT_EQUAL(CC1, CC_REG);  /* Negative */
  }

  /* Divide packed */
  DTEST(instruct, dp) {
      /* PrincOps p 151 */
      init_cpu();
      set_reg( 12, 0x00002000);
      set_reg( 13, 0x00003000);
      set_mem(0x2000, 0x01234567);
      set_mem(0x2004, 0x8cffffff);
      set_mem(0x3000, 0x321dffff);
      set_mem(0x400, 0xfd41c000);
      set_mem(0x404, 0xd0000000);  /* DP 0(5, 12), 0(2, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x38460d01, get_mem(0x2000));
      ASSERT_EQUAL_X(0x8cffffff, get_mem(0x2004));
  }

  /* Compare logical register */
  CTEST(instruct, clr) {
      init_cpu();
      set_reg( 1, 0x12345678);
      set_reg( 2, 0x12345678);
      set_mem(0x400, 0x15120000); /* CLR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* equal */

      set_reg( 1, 0x12345678);
      set_reg( 2, 0x12345679);
      set_mem(0x400, 0x15120000); /* CLR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* first operand is low */

      set_reg( 1, 0x12345679);
      set_reg( 2, 0x12345678);
      set_mem(0x400, 0x15120000); /* CLR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL(CC2, CC_REG);  /* first operand is high */

      set_reg( 1, 0x7fffffff);
      set_reg( 2, 0x8fffffff);
      set_mem(0x400, 0x15120000); /* CLR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG); /* first operand is low */
  }

  /* Compare logical */
  CTEST(instruct, cl2) {
      init_cpu();
      set_reg( 1, 0x12345678);
      set_reg( 2, 0x100);
      set_reg( 3, 0x100);
      set_mem(0x300, 0x12345678);
      set_mem(0x400, 0x55123100); /* CL 1,100(2,3) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* equal */
  }

  /* And register */
  CTEST(instruct, nr) {
      init_cpu();
      set_reg( 1, 0xff00ff00);
      set_reg( 2, 0x12345678);
      set_mem(0x400, 0x14120000); /* NR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X( 0x12005600, get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* And register zero result */
  CTEST(instruct, nr0) {
      init_cpu();
      set_reg( 1, 0x12345678);
      set_reg( 2, 0xedcba987);
      set_mem(0x400, 0x14120000); /* NR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0, get_reg(1));
      ASSERT_EQUAL(CC0, CC_REG);  /* Zero */
  }

  /* Or register */
  CTEST(instruct, or2) {
      init_cpu();
      set_reg( 1, 0xff00ff00);
      set_reg( 2, 0x12345678);
      set_mem(0x400, 0x16120000); /* OR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xff34ff78, get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  /* Exclusive or register */
  CTEST(instruct, xr) {
      init_cpu();
      set_reg( 1, 0xff00ff00);
      set_reg( 2, 0x12345678);
      set_mem(0x400, 0x17120000); /* XR 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(0xed34a978, get_reg(1));
      ASSERT_EQUAL(CC1, CC_REG);  /* Not zero */
  }

  CTEST(instruct, sll2) {
    int i;
    init_cpu();
    for (i = 0; i < 31; i++) {
      set_reg( 1, 1);
      set_reg( 2, 0x12340000 + i); /* Shift i bits */
      set_mem(0x400, 0x891f2100); /* SLL 1,100(2) */
      test_inst(0x0);
      ASSERT_EQUAL_X((uint32_t)(1 << i), get_reg(1));
    }
  }


  CTEST(instruct, spm) {
    init_cpu();
    set_reg( 1, 0x12345678); /* Mask 2 */
    set_mem(0x400, 0x041f0000); /* SPM 1 */
      test_inst(0x0);
    ASSERT_EQUAL(CC1, CC_REG);
    ASSERT_EQUAL_X(0x2, PM);
  }

  /* Test ssm instruction */
  CTEST(instruct, ssm) {
      init_cpu();
      set_mask(0xff);
      set_key(3);
      set_amwp(0x8); /* Privileged */
      set_cc(CC1);
      set_reg(3, 0x11);
      set_mem(0x110, 0xaabbccdd); /* Access byte 1 */
      set_mem(0x400, 0x80ee3100); /* "SSM 100(3); */
      test_inst(0xa);
      ASSERT_EQUAL_X(0xBB, get_mask());
      ASSERT_EQUAL_X(3, get_key());
      ASSERT_EQUAL_X(0x8, get_amwp());
      ASSERT_EQUAL(CC1, CC_REG);
      ASSERT_EQUAL_X(0xa, PM);
      ASSERT_EQUAL_X(0x404, IAR);
      set_key(0);
  }

  /* Test the ssm does not work in problem state */
  CTEST(instruct, ssm_unpriv) {
      init_cpu();
      set_key(0);
      set_mask(0xff);
      set_amwp(0x1); /* problem state */
      set_cc(CC1);
      set_mem(0x400, 0x80ee3100); /* SSM 100(3) */
      test_inst(0xa);
      ASSERT_TRUE(trap_flag);
      set_amwp(0);  /* Privileged */
  }

  /* Test lpsw instruction */
  CTEST(instruct, lpsw) {
      init_cpu();
      set_key(0);
      set_amwp(0);  /* Privileged */
      set_reg(3, 0x10);
      set_mem(0x110, 0xE1345678);
      set_mem(0x114, 0x9a003450); /* Branch to 123450 */
      set_mem(0x400, 0x82003100); /* LPSW 100(3) */
      set_mem(0x3450, 0x00000000);  /* Nop in case things are executed */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x3, get_key());
      ASSERT_EQUAL_X(0x4, get_amwp());
      ASSERT_EQUAL(CC1, CC_REG);
      ASSERT_EQUAL_X(0xa, PM);
      ASSERT_EQUAL_X(0x003450, IAR);
      ASSERT_EQUAL_X(0xE1, get_mask());
      set_key(0);
  }

  /* Supervisory call */
  CTEST(instruct, svc) {
      init_cpu();
      set_key(0);
      set_amwp(1);
      set_mask(0xE1);
      set_cc(CC1);
      set_mem(0x60, 0xE1345678);
      set_mem(0x64, 0x9a003450);  /* Branch to 3450 */
      set_mem(0x400, 0x0a120000); /* SVC 12 */
      set_mem(0x3450, 0x00000000);  /* Nop in case things are executed */
      test_inst(0x4);
      ASSERT_EQUAL_X(0x3, get_key());  /* Validate that PSW got set */
      ASSERT_EQUAL_X(0x4, get_amwp());
      ASSERT_EQUAL(CC1, CC_REG);
      ASSERT_EQUAL_X(0xa, PM);
      ASSERT_EQUAL_X(0x003450, IAR);
      ASSERT_EQUAL_X(0xE1, get_mask());
      ASSERT_EQUAL_X(0xE1010012, get_mem(0x20)); /* Validate OPSW */
      ASSERT_EQUAL_X(0x54000402, get_mem(0x24));
      set_key(0);
  }

  /* Set storage key */
  CTEST(instruct, ssk) {
      init_cpu();
      set_amwp(1);                /* Privileged */
      set_reg( 1, 0x11223344);    /* Key */
      set_reg( 2, 0x00005600);    /* Address: last 4 bits must be 0 */
      set_mem_key(0x5600, 0);
      set_mem(0x400, 0x08120000); /* SSK 1,2 */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Set storage key */
  CTEST(instruct, ssk2) {
      init_cpu();
      set_amwp(0);                /* Privileged */
      set_mem_key(0x5600, 0);
      set_reg( 1, 0x11223344);    /* Key */
      set_reg( 2, 0x00005600);    /* Address: last 4 bits must be 0 */
      set_mem(0x400, 0x08120000); /* SSK 1,2 */
      test_inst(0x0);
      ASSERT_EQUAL_X(4, get_mem_key(0x00005678));
  }

  /* Set storage key unaligned access */
  CTEST(instruct, ssk3) {
      init_cpu();
      set_amwp(0);                /* Privileged */
      set_mem_key(0x5600, 0);
      set_reg( 1, 0x11223344);    /* Key */
      set_reg( 2, 0x12345674);    /* Unaligned: last 4 bits not 0 */
      set_mem(0x400, 0x08120000); /* SSK 1,2 */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* ISK reads the storage key */
  CTEST(instruct, isk) {
      init_cpu();
      set_amwp(0);                /* Privileged */
      set_mem_key(0x5600, 2);
      set_reg( 1, 0x89abcdef);
      set_reg( 2, 0x00005600);    /* Aligned: last 4 bits 0 */
      set_mem(0x400, 0x09120000); /* ISK 1,2 */
      test_inst(0x0);
      ASSERT_FALSE(trap_flag);
      ASSERT_EQUAL_X(0x89abcd20, get_reg(1));
  }

  /* ISK reads the storage key */
  CTEST(instruct, isk2) {
      init_cpu();
      set_amwp(1);                /* Unprivileged */
      set_mem_key(0x5600, 2);
      set_reg( 1, 0xaabbccdd);
      set_reg( 2, 0x00005600);    /* Aligned: last 4 bits 0 */
      set_mem(0x400, 0x09120000); /* ISK 1,2 */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* ISK reads the storage key */
  CTEST(instruct, isk3) {
      init_cpu();
      set_amwp(0);                /* Privileged */
      set_mem_key(0x5600, 2);
      set_reg( 1, 0xaabbccdd);
      set_reg( 2, 0x00005678);    /* Unaligned: last 4 bits not 0 */
      set_mem(0x400, 0x09120000); /* ISK 1,2 */
      test_inst(0x0);
      ASSERT_TRUE(trap_flag);
  }

  /* Protection check. unmatched key */
  CTEST(instruct, prot_check) {
      init_cpu();
      set_amwp(1);                /* unpriv */
      set_key(2);
      set_reg( 1, 0x11223344);
      set_reg( 2, 0x00005670);
      set_mem( 0x5678, 0x0);
      set_mem_key(0x5600, 4);
      set_mem(0x400, 0x50102008); /* st 1,0(2) */
      test_inst(0x0);
      set_key(2);
      ASSERT_EQUAL_X(0, get_mem(0x5678)); /* Make sure memory not changed */
      ASSERT_TRUE(trap_flag);
  }

  /* Protection check. Keys match */
  CTEST(instruct, prot_check2) {
      init_cpu();
      set_amwp(1);                 /* unpriv */
      set_key(4);
      set_reg( 1, 0x11223344);
      set_reg( 2, 0x00005670);
      set_mem( 0x5678, 0x0);
      set_mem_key(0x5600, 4);
      set_mem(0x400, 0x50102008); /* st 1,0(2) */
      test_inst(0x0);
      set_key(4);
      ASSERT_EQUAL_X(0x11223344, get_mem(0x5678)); /* Make sure updated */
  }

  /* Protection check. Keys mismatch, read */
  CTEST(instruct, prot_check3) {
      init_cpu();
      set_amwp(1);                 /* unpriv */
      set_key(2);
      set_reg( 1, 0x11223344);
      set_reg( 2, 0x00005670);
      set_mem( 0x5678, 0x12345678);
      set_mem_key(0x5600, 4);
      set_mem(0x400, 0x58102008); /* l 1,0(2) */
      test_inst(0x0);
      set_key(2);
      ASSERT_EQUAL_X(0x12345678, get_reg(1)); /* Read should work */
      ASSERT_FALSE(trap_flag);
  }

  /* Protection check. Keys match, read */
  CTEST(instruct, prot_check4) {
      init_cpu();
      set_amwp(1);                /* unpriv */
      set_key(4);
      set_reg( 1, 0x11223344);
      set_reg( 2, 0x00005670);
      set_mem( 0x5678, 0x12345678);
      set_mem_key(0x5600, 4);
      set_mem(0x400, 0x58102008); /* l 1,0(2) */
      test_inst(0x0);
      set_key(0);
      ASSERT_EQUAL_X(0x12345678, get_reg(1));
      ASSERT_FALSE(trap_flag);
  }

  /* Protection check. CPU zero, memory not zero */
  CTEST(instruct, prot_check5) {
      init_cpu();
      set_amwp(1);                 /* unpriv */
      set_key(4);
      set_reg( 1, 0x11223344);
      set_reg( 2, 0x00005670);
      set_mem( 0x5678, 0x0);
      set_mem_key(0x5600, 4);
      set_mem(0x400, 0x50102008); /* st 1,0(2) */
      test_inst(0x0);
      set_key(0);
      ASSERT_EQUAL_X(0x11223344, get_mem(0x5678)); /* Make sure updated */
  }

  /* Test and set */
  CTEST(instruct, ts) {
      init_cpu();
      set_amwp(0);                /* unpriv */
      set_reg( 2, 2);             /* Index */
      set_mem(0x100, 0x83857789); /* 102 top bit not set */
      set_mem(0x400, 0x93002100); /* TS 100(2) */
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);  /* Not set */
      ASSERT_EQUAL_X(0x8385ff89, get_mem(0x100));
  }

  /* Test and set */
  CTEST(instruct, ts2) {
      init_cpu();
      set_reg( 2, 2);             /* Index */
      set_mem(0x100, 0x8385c789); /* 102 top bit set */
      set_mem(0x400, 0x93002100); /* TS 100(2) */
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);  /* Set */
      ASSERT_EQUAL_X(0x8385ff89, get_mem(0x100));
  }

  /* Test load halfword */
  CTEST(instruct, lh_iva) {
      uint32_t   psw1, psw2;
      init_cpu();
      set_amwp(0);
      set_cc(CC1);
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x201);
      set_mem(0x28, 0xffffffff);
      set_mem(0x2c, 0xffffffff);
      set_mem(0x1b84, 0x87654321);
      set_mem(0x400, 0x48345986); /* LH 3,986(4,5) */
      test_inst(0x0);
      psw1 = get_mem(0x28);
      psw2 = get_mem(0x2c);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(0x00000006, psw1);
      ASSERT_EQUAL_X(0x90000404, psw2);
  }

  /* Test load word */
  CTEST(instruct, l_iva1) {
      uint32_t   psw1, psw2;
      init_cpu();
      set_amwp(0);
      set_cc(CC1);
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x201);
      set_mem(0x28, 0xffffffff);
      set_mem(0x2c, 0xffffffff);
      set_mem(0x1b84, 0x68654321);
      set_mem(0x400, 0x58345984); /* L 3,984(4,5) */
      test_inst(0x0);
      psw1 = get_mem(0x28);
      psw2 = get_mem(0x2c);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(0x00000006, psw1);
      ASSERT_EQUAL_X(0x90000404, psw2);
  }

  /* Test load word */
  CTEST(instruct, l_iva2) {
      uint32_t   psw1, psw2;
      init_cpu();
      set_amwp(0);
      set_cc(CC1);
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x202);
      set_mem(0x28, 0xffffffff);
      set_mem(0x2c, 0xffffffff);
      set_mem(0x1b84, 0x68654321);
      set_mem(0x400, 0x58345984); /* L 3,984(4,5) */
      test_inst(0x0);
      psw1 = get_mem(0x28);
      psw2 = get_mem(0x2c);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(0x00000006, psw1);
      ASSERT_EQUAL_X(0x90000404, psw2);
  }

  /* Test load word */
  CTEST(instruct, l_iva3) {
      uint32_t   psw1, psw2;
      init_cpu();
      set_amwp(0);
      set_cc(CC1);
      set_reg(3, 0xffffffff);
      set_reg(4, 0x1000);
      set_reg(5, 0x203);
      set_mem(0x28, 0xffffffff);
      set_mem(0x2c, 0xffffffff);
      set_mem(0x1b84, 0x68654321);
      set_mem(0x400, 0x58345984); /* L 3,984(4,5) */
      test_inst(0x0);
      psw1 = get_mem(0x28);
      psw2 = get_mem(0x2c);
      ASSERT_TRUE(trap_flag);
      ASSERT_EQUAL_X(0x00000006, psw1);
      ASSERT_EQUAL_X(0x90000404, psw2);
  }

#define ED  0xde
#define MVO 0xf1
#define ZAP 0xf8
#define CP  0xf9
#define AP  0xfa
#define SP  0xfb
#define MP  0xfc
#define DP  0xfd

char hex[] = "0123456789abcdef";

struct _dec_case {
    uint8_t           op;   /* Opcode */
    char             *i1;   /* Input one string */
    char             *i2;   /* Input two string */
    char             *out;  /* Output */
    uint8_t          cc;    /* Condition code */
    uint8_t          ex;    /* exception code */
} dec_cases[] = {
     { AP,  "2c", "3c", "5c", CC2},
     { SP,  "1c", "7c", "6d", CC1},
     { AP,  "1c", "7c", "8c", CC2},
     { SP,  "9c", "5c", "4c", CC2},
     { AP,  "9c", "5c", "4c", CC3, 10},
     { SP,  "009c", "5d", "014c", CC2},
     { SP,  "1d", "1d", "0c", CC0},
     { AP,  "12345c", "54321c", "66666c", CC2},
     { SP,  "12345c", "54321c", "41976d", CC1},
     { SP,  "54321c", "12345c", "41976c", CC2},
     { SP,  "54321c", "01234d", "55555c", CC2},
     { SP,  "12345c", "54321d", "66666c", CC2},
     { AP,  "12345d", "54321d", "66666d", CC1},
     { AP,  "012c", "052c", "064c", CC2},
     { AP,  "072c", "012c", "084c", CC2},
     { AP,  "095c", "023c", "118c", CC2},
     { SP,  "095c", "023d", "118c", CC2},
     { SP,  "012c", "532c", "520d", CC1},
     { AP,  "171c", "053c", "224c", CC2},
     { SP,  "171d", "053c", "224d", CC1},
     { AP,  "053d", "171d", "224d", CC1},
     { AP,  "1c", "2c", "3c", CC2},
     { AP,  "072c", "025d", "047c", CC2},
     { AP,  "072d", "080c", "008c", CC2},
     { SP,  "77532c", "12345c", "65187c", CC2},
     { AP,  "9c", "018d", "9d", CC1},
     { SP,  "6c", "014c", "8d", CC1},
     { SP,  "8d", "019d", "1c", CC3, 10},
     { AP,  "7d", "016c", "9c", CC2},
     { MP,  "0000125c", "752c", "0094000c", CC2},
     { MP,  "012345", "654321", "012345", CC0, 7},
     { MP,  "5c", "5c", "5c", CC0, 6},
     { MP,  "005c", "5c", "025c", CC0},
     { MP,  "005c", "012c", "005c", CC0, 6},
     { MP,  "006c", "013c", "006c", CC0, 6},
     { MP,  "00004c", "017c", "00068c", CC0},
     { MP,  "005c", "215c", "005c", CC0, 6},
     { MP,  "00006c", "135c", "00810c", CC0},
     { MP,  "00004c", "023c", "00092c", CC0},
     { MP,  "007c", "9c", "063c", CC0},
     { MP,  "009d", "8c", "072d", CC0},
     { MP,  "018c", "2c", "036c", CC0, 7},
     { MP,  "008d", "3d", "024c", CC0},
     { MP,  "001d", "0c", "000d", CC0},
     { MP,  "000c", "052d", "000c", CC0, 6},
     { MP,  "00000014142c", "14142c", "00199996164c", CC0},
     { MP,  "00000017320c", "17320c", "00299982400c", CC0},
     { MP,  "0000000223607d", "0223607c", "0000000223607d", CC0, 7},
     { MP,  "002236067977499c", "3d", "006708203932497d", CC0},
     { MP,  "001414213562373d", "2d", "002828427124746c", CC0},
     { MP,  "022360679774997c", "3d", "022360679774997c", CC0, 7},
     { MP,  "014142135623730d", "2d", "014142135623730d", CC0, 7},
     { MP,  "002236067977499c", "029d", "002236067977499c", CC0, 7},
     { MP,  "001414213562373d", "021d", "001414213562373d", CC0, 7},
     { MP,  "000223606797749c", "029d", "000223606797749c", CC0, 7},
     { MP,  "000141421356237d", "021d", "000141421356237d", CC0, 7},
     { MP,  "022360697774997c", "9d", "022360697774997c", CC0, 7},
     { MP,  "074142315623730d", "8d", "074142315623730d", CC0, 7},
     { MP,  "000000000000005c", "0123456c", "000000000617280c", CC0},
     { MP,  "000000000000005c", "1234567c", "000000006172835c", CC0},
     { MP,  "000000000000003c", "012345678c", "000000037037034c", CC0},
     { MP,  "000000000000015c", "0123456c", "000000001851840c", CC0},
     { MP,  "000000000000025c", "1234567c", "000000030864175c", CC0},
     { MP,  "000000000000093c", "012345678c", "000001148148054c", CC0},
     { MP,  "000000001234567c", "1234567c", "001524155677489c", CC0},
     { MP,  "000000001234567c", "012345678c", "000000001234567c", CC0, 7},
     { MP,  "000000001234567c", "123456789c", "000000001234567c", CC0, 7},
     { MP,  "0001234c", "025c", "0001234c", CC0, 7},
     { MP,  "0001243d", "017c", "0001243d", CC0, 7},
     { MP,  "0005432c", "071d", "0005432c", CC0, 7},
     { MP,  "0000123d", "176d", "0021648c", CC0},
     { MP,  "0000512c", "01068c", "0000512c", CC0, 7},
     { MP,  "002c", "2c", "004c", CC0},
     { MP,  "004c", "4c", "016c", CC0},
     { MP,  "008c", "8c", "064c", CC0},
     { MP,  "00016c", "016c", "00016c", CC0, 7},
     { MP,  "0000032c", "032c", "0001024c", CC0},
     { MP,  "0000064c", "064c", "0004096c", CC0},
     { MP,  "0000128c", "128c", "0016384c", CC0},
     { MP,  "0000256c", "256c", "0065536c", CC0},
     { MP,  "0000512c", "512c", "0262144c", CC0},
     { MP,  "00000001024c", "01024c", "00001048576c", CC0},
     { MP,  "00000002048c", "02048c", "00004194304c", CC0},
     { MP,  "00000004096c", "04096c", "00016777216c", CC0},
     { MP,  "00000008192c", "08192c", "00067108864c", CC0},
     { MP,  "00000016384c", "16384c", "00268435456c", CC0},
     { MP,  "00000032768c", "32768c", "01073741824c", CC0},
     { MP,  "00000065536c", "65536c", "04294967296c", CC0},
     { MP,  "000000000131072c", "0131072c", "000017179869184c", CC0},
     { MP,  "000000000524288c", "0524288c", "000274877906944c", CC0},
     { MP,  "000000002097152c", "0131072c", "000274877906944c", CC0},
     { MP,  "000000002097152c", "65536c", "000137438953472c", CC0},
     { MP,  "000000002097152c", "2097152c", "004398046511104c", CC0},
     { MP,  "000002147483646c", "512c", "001099511626752c", CC0},
     { MP,  "000002147483646c", "08192c", "000002147483646c", CC0, 7},
     { MP,  "000002147483646c", "16384c", "000002147483646c", CC0, 7},
     { MP,  "000002147483646c", "65536c", "000002147483646c", CC0, 7},
     { MP,  "004398046511104c", "8c", "035184372088832c", CC0},
     { MP,  "004398046511104c", "064c", "004398046511104c", CC0, 7},
     { MP,  "000549755813888c", "08192c", "000549755813888c", CC0, 7},
     { MP,  "000549755813888c", "512c", "000549755813888c", CC0, 7},
     { MP,  "000549755813888c", "064c", "000549755813888c", CC0, 7},
     { MP,  "000549755813888c", "8c", "004398046511104c", CC0},
     { MP,  "000068719476736c", "16384c", "000068719476736c", CC0, 7},
     { MP,  "000068719476736c", "04096c", "000068719476736c", CC0, 7},
     { MP,  "000068719476736c", "512c", "035184372088832c", CC0},
     { MP,  "7c", "7d", "7c", CC0, 6},
     { MP,  "025c", "3d", "025c", CC0, 7},
     { MP,  "7d", "8d", "7d", CC0, 6},
     { DP,  "77325c", "025c", "77325c", CC0, 11},
     { DP,  "066c", "1c", "066c", CC0, 11},
     { DP,  "072c", "3d", "072c", CC0, 11},
     { DP,  "066d", "2c", "066d", CC0, 11},
     { DP,  "072c", "1c", "072c", CC0, 11},
     { DP,  "072c", "0c", "072c", CC0, 11},
     { DP,  "000077325c", "025c", "03093c000c", CC0},
     { DP,  "0000066c", "2c", "00033c0c", CC0},
     { DP,  "00066c", "2c", "033c0c", CC0},
     { DP,  "00066c", "2c", "033c0c", CC0},
     { DP,  "066c", "2c", "066c", CC0, 11},
     { DP,  "0123456c", "072c", "0123456c", CC0, 11},
     { DP,  "0123456c", "072c", "0123456c", CC0, 11},
     { DP,  "000123456c", "072c", "01714c048c", CC0},
     { DP,  "000123456c", "072c", "01714c048c", CC0},
     { DP,  "00000123456c", "072c", "0001714c048c", CC0},
     { DP,  "00004398046511104c", "064c", "0068719476736c000c", CC0},
     { DP,  "00004398046511104c", "064c", "0068719476736c000c", CC0},
     { DP,  "004398046511104c", "064c", "68719476736c000c", CC0},
     { DP,  "004398046511104c", "064c", "68719476736c000c", CC0},
     { DP,  "00000043980465111c", "653c", "0000067351401c258c", CC0},
     { DP,  "00000439804651110c", "653c", "0000673514013c621c", CC0},
     { DP,  "00004398046511104c", "653c", "0006735140139c337c", CC0},
     { DP,  "00004398046511104c", "653c", "0006735140139c337c", CC0},
     { DP,  "004398046511104c", "653c", "06735140139c337c", CC0},
     { DP,  "043980465111040c", "653c", "67351401395c105c", CC0},
     { DP,  "439804651110400c", "653c", "439804651110400c", CC0, 11},
     { DP,  "0000435d", "7c", "00062d1d", CC0},
     { DP,  "0000435c", "7d", "00062d1c", CC0},
     { DP,  "0000435d", "7d", "00062c1d", CC0},
     { DP,  "0000251d", "7d", "00035c6d", CC0},
     { DP,  "0000252d", "7d", "00036c0d", CC0},
     { DP,  "0000253d", "7d", "00036c1d", CC0},
     { DP,  "00000d", "1c", "000d0d", CC0},
     { DP,  "00001d", "1c", "001d0d", CC0},
     { DP,  "00001c", "1c", "001c0c", CC0},
     { DP,  "00000c", "1d", "000d0c", CC0},
     { DP,  "00000c", "1c", "000c0c", CC0},
     { DP,  "00000c", "0c", "00000c", CC0, 11},
     { DP,  "0000000000725c", "1234567c", "00000c0000725c", CC0},
     { DP,  "0000000000725c", "012345678c", "000c000000725c", CC0},
     { DP,  "1234567c", "1234567c", "1234567c", CC0, 6},
     { DP,  "012345678c", "1234567c", "012345678c", CC0, 11},
     { DP,  "000000008c", "1234567c", "0c0000008c", CC0},
     { DP,  "000000008c", "0123456c", "0c0000008c", CC0},
     { DP,  "000000008c", "12345c", "000c00008c", CC0},
     { DP,  "0000000000000006543210987654321c", "123456789012345c", "000000000000053c000001170000036c", CC0},
     { DP,  "0000000000006543210987654321000c", "123456789012345c", "000000000053000c001170000036000c", CC0},
     { DP,  "0000000006543210987654321000111c", "123456789012345c", "000000053000009c058888934889006c", CC0},
     { DP,  "0000006543210987654321000111222c", "123456789012345c", "000053000009477c000046530117657c", CC0},
     { DP,  "0000043210987654321000111222333c", "123456789012345c", "000350009003150c010253617335583c", CC0},
     { DP,  "0000543210987654321000111222333c", "123456789012345c", "004400009039600c013044117360333c", CC0},
     { DP,  "0006543210987654321000111222333c", "123456789012345c", "053000009477000c046530117657333c", CC0},
     { DP,  "0076543210987654321000111222333c", "123456789012345c", "620000014580003c066829754085298c", CC0},
     { DP,  "0876543210987654321000111222333c", "123456789012345c", "0876543210987654321000111222333c", CC0, 11},
     { DP,  "6543210987654321000111222333444c", "123456789012345c", "6543210987654321000111222333444c", CC0, 11},
     { DP,  "0000000000000000000000000000000c", "123456789012345c", "000000000000000c000000000000000c", CC0},
     { DP,  "0000000000000000000000000000000c", "01234567890123456c", "0000000000000000000000000000000c", CC0, 6},
     { MVO, "512c", "001068", "068c", CC0},
     { MVO, "7788990c", "123456", "0123456c", CC0},
     { MVO, "0001234c", "025c", "000025cc", CC0},
     { MVO, "0001243d", "017c", "000017cd", CC0},
     { MVO, "0005432c", "071d", "000071dc", CC0},
     { MVO, "0000123d", "176d", "000176dd", CC0},
     { MVO, "0000512c", "01068c", "001068cc", CC0},
     { MVO, "002c", "2c", "02cc", CC0},
     { MVO, "004c", "4c", "04cc", CC0},
     { MVO, "008c", "8c", "08cc", CC0},
     { MVO, "512c", "00068c", "68cc", CC0},
     { ZAP, "0001234c", "025c", "0000025c", CC2},
     { ZAP, "0001243d", "017c", "0000017c", CC2},
     { ZAP, "0005432c", "071d", "0000071d", CC1},
     { ZAP, "0000123d", "176d", "0000176d", CC1},
     { ZAP, "0000512c", "01068c", "0001068c", CC2},
     { ZAP, "002c", "2c", "002c", CC2},
     { ZAP, "004c", "4c", "004c", CC2},
     { ZAP, "008c", "8c", "008c", CC2},
     { ZAP, "512c", "01068c", "068c", CC3, 10},
     { ZAP, "512c", "00068c", "068c", CC2},
     { CP,  "0c", "000d", "0c", CC0},
     { CP,  "1c", "5c", "1c", CC1},
     { CP,  "9c", "9c", "9c", CC0},
     { CP,  "9c", "9d", "9c", CC2},
     { CP,  "017c", "4d", "017c", CC2},
     { CP,  "1c", "034d", "1c", CC2},
     { CP,  "027c", "000000235d", "027c", CC2},
     { CP,  "5c", "000000235d", "5c", CC2},
     { CP,  "12345c", "54321c", "12345c", CC1},
     { ED,  "ee2020202120", "00023c", "eeeeeeeef2f3", CC2},
     { ED,  "ee2020202120", "0c1c012c", "eeeef1eef1f2", CC2},
     { ED,  "ee2020202120", "0d1d012d", "eeeef1f0f1f2", CC1},
     { ED,  "ee202022202120", "0c1c012e", "eeeef1eeeef1f2", CC2},
     { ED,  "ee202020", "00b0", "eeeeee20", CC0, 7},
     { ED,  "ee202020", "00c0", "eeeeee20", CC0, 7},
     { ED,  "ee212020", "000f", "eeeef0f0", CC0},
     { ED,  "ee2020202020202020202020202020", "013b026c00129c789a", "eeeef1f3f0f2f6eeeef1f2f9f7f8f9", CC2},
     { AP,  "3c", "5c", "8c", CC2},
     { 0,  "", "", "", CC0}
};

  /* Run group of decimal test cases */
  DTEST(instruct, decimal_test) {
      struct _dec_case *test;
      char      result[30];
      int       i;
      int       l1, l2;
      uint8_t   data;
      int       addr;

      init_cpu();
      data = 0;
      for (test = &dec_cases[0]; test->op != 0; test++) {
          l1 = l2 = 0;
          addr = 0x1000;
          for (i = 0; i < strlen(test->i1); i++) {
               if (i & 1) {
                   data |= (strchr(hex, test->i1[i]) - hex);
                   l1++;
                   set_mem_b(addr, data);
                   addr++;
               } else {
                   data = (strchr(hex, test->i1[i]) - hex) << 4;
               }
          }
          addr = 0x2000;
          for (i = 0; i < strlen(test->i2); i++) {
               if (i & 1) {
                   data |= (strchr(hex, test->i2[i]) - hex);
                   l2++;
                   set_mem_b(addr, data);
                   addr++;
               } else {
                   data = (strchr(hex, test->i2[i]) - hex) << 4;
               }
          }
          set_reg(10, 0x1000);
          set_reg(12, 0x2000);
          if (test->op == 0xde) {
             set_mem(0x400, (test->op << 24) | (((l1) -1) << 16) | 0xa000);
          } else {
             set_mem(0x400, (test->op << 24) | ((l1 -1) << 20) | ((l2 -1) << 16) | 0xa000);
          }
          set_mem(0x404, 0xc0000000);
          set_mem(0x28, 0);
          test_inst(0x4);
          addr = 0x1000;
          for (i = 0; i < strlen(test->out); i++) {
               if (i & 1) {
                   data |= (strchr(hex, test->out[i]) - hex);
                   l1++;
                   addr++;
                   result[i] = hex[data & 0xf];
               } else {
                   uint8_t data2 = get_mem_b(addr);
                   result[i] = hex[(data2 >> 4) & 0xf];
                   data = (strchr(hex, test->out[i]) - hex) << 4;
               }
          }
          result[i] = '\0';
printf("DEC %x %s , %s => %s\n", test->op, test->i1, test->i2, result);
          if (test->ex) {
              ASSERT_TRUE(trap_flag);
              ASSERT_EQUAL_X(test->ex, get_mem(0x28) & 0xffff);
          } else {
          ASSERT_STR(test->out, result);
              ASSERT_EQUAL(test->cc, CC_REG);
              ASSERT_FALSE(trap_flag);
              ASSERT_EQUAL_X(test->ex, get_mem(0x28) & 0xffff);
          }
      }
  }

  /* Test floating point store double */
  FTEST(instruct, std) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x400, 0x60012100);  /* STD 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_mem(0x500));
      ASSERT_EQUAL_X(0xaabbccdd, get_mem(0x504));
  }

  /* Test floating point load double */
  FTEST(instruct, ld) {
      init_cpu();
      set_mem(0x100, 0x12345678);
      set_mem(0x104, 0xaabbccdd);
      set_mem(0x400, 0x68000100);  /*  LD 0,100(0,0) */
      set_fpreg_s(0, 0xffffffff);
      set_fpreg_s(1, 0xffffffff);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_fpreg_s(0));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(1));
  }

  /* Test floating point load double */
  FTEST(instruct, ld2) {
      init_cpu();
      set_mem(0x100, 0x44000000);
      set_mem(0x104, 0xaabbccdd);
      set_mem(0x400, 0x68000100);  /*  LD 0,100(0,0) */
      set_fpreg_s(0, 0xffffffff);
      set_fpreg_s(1, 0xffffffff);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x44000000, get_fpreg_s(0)); /* Stays unnormalized */
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(1));
  }

  /* Load complement LCDR - LCDR 2,4 */
  FTEST(instruct, lcdr) {
      init_cpu();
      set_mem(0x400, 0x23240000); /* LCDR 2,4 */
      /* Test positive number */
      set_fpreg_s(4, 0x12345678);
      set_fpreg_s(5, 0xaabbccdd);
      test_inst(0);
      ASSERT_EQUAL_X(0x92345678, get_fpreg_s(2));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test negative number */
      set_fpreg_s(4, 0x92345678);
      set_fpreg_s(5, 0xaabbccdd);
      test_inst(0);
      ASSERT_EQUAL_X(0x12345678, get_fpreg_s(2));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test zero */
      set_fpreg_s(4, 0x00000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_fpreg_s(4, 0x80000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Load Positive LPDR - LPDR 3,4 */
  FTEST(instruct, lpdr) {
      init_cpu();
      set_mem(0x400, 0x20240000);  /* LPDR 2,4 */
      set_fpreg_s(4, 0xffffffff);
      set_fpreg_s(5, 0xffffffff);
      test_inst(0);
      ASSERT_EQUAL_X(0x7fffffff, get_fpreg_s(2));
      ASSERT_EQUAL_X(0xffffffff, get_fpreg_s(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test positive */
      set_fpreg_s(4, 0x12345678);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x12345678, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC2, CC_REG);
      /* Test zero */
      set_fpreg_s(4, 0x00000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_fpreg_s(4, 0x80000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Load negative LNDR - LNDR 3,4 */
  FTEST(instruct, lndr) {
      init_cpu();
      set_mem(0x400, 0x21240000);  /* LNDR 2,4 */
      set_fpreg_s(4, 0xffffffff);
      set_fpreg_s(5, 0xffffffff);
      test_inst(0);
      ASSERT_EQUAL_X(0xffffffff, get_fpreg_s(2));
      ASSERT_EQUAL_X(0xffffffff, get_fpreg_s(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test positive */
      set_fpreg_s(4, 0x12345678);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x92345678, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC1, CC_REG);
      /* Test zero */
      set_fpreg_s(4, 0x00000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
      /* Test overflow */
      set_fpreg_s(4, 0x80000000);
      set_fpreg_s(5, 0x00000000);
      test_inst(0);
      ASSERT_EQUAL_X(0x80000000, get_fpreg_s(2));
      ASSERT_EQUAL_X(0x00000000, get_fpreg_s(3));
      ASSERT_EQUAL(CC0, CC_REG);
  }

  /* Test compare double */
  FTEST(instruct, cd) {
      init_cpu();
      set_fpreg_s(0, 0x43000000);
      set_fpreg_s(1, 0x00000000);
      set_mem(0x100, 0x32123456);
      set_mem(0x104, 0x789ABCDE);
      set_mem(0x400, 0x69000100);  /* CD 0,100(0,0) */
      set_mem(0x404, 0x00000000);
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);   /* Less */
  }

  /* Test compare double */
  FTEST(instruct, cd2) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_mem(0x100, 0x44000000);
      set_mem(0x104, 0xaabbccdd);
      set_mem(0x400, 0x69000100);  /* CD 0,100(0,0) */
      set_mem(0x404, 0x00000000);
      test_inst(0x0);
      ASSERT_EQUAL(CC1, CC_REG);   /* Less */
  }

  /* Test compare double */
  FTEST(instruct, cd3) {
      init_cpu();
      set_fpreg_s(0, 0x43082100);
      set_fpreg_s(1, 0xaabbccdd);
      set_mem(0x100, 0x43082100);
      set_mem(0x104, 0xaabbccdd);
      set_mem(0x400, 0x69000100);  /* CD 0,100(0,0) */
      set_mem(0x404, 0x00000000);
      test_inst(0x0);
      ASSERT_EQUAL(CC0, CC_REG);   /* Equal */
  }

  /* Half instruct rand */
  FTEST(instruct, hd_rand) {
      int i;
      double f1;
      double result;
      double desired;
      double ratio;
      int did = 0;

      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          if (floatToFpreg(2, f1) != 0)
              continue;
          desired = f1/2.0;
          set_mem(0x400, 0x24020000);  /* HDR 0,2 */
          test_inst(0x0);
          result = cnvt_64_float(0);
          ratio = fabs((result - desired) / desired);
          ASSERT_TRUE(ratio < .000001);
          did++;
      }
  }

  /* Add double */
  FTEST(instruct, ad) {
      /* Princ Ops 153 */
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6a60d000);  /* AD 6,0(0, 13) */
      set_mem(0x404, 0x00000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x42833345, get_fpreg_s(6));
      ASSERT_EQUAL_X(0x60000000, get_fpreg_s(7));
  }

  FTEST(instruct, ad_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;

      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          desired = f1 + f2;
          set_mem(0x400, 0x2a020000);  /* ADR 0,2 */
          test_inst(0x0);
          result = cnvt_64_float(0);
          ratio = fabs((result - desired) / desired);
          ASSERT_TRUE(ratio < .000001);
          did++;
      }
  }

  /* Subtract double */
  FTEST(instruct, sd) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6b60d000);  /* SD 6,0(0, 13) */
      set_mem(0x404, 0x00000000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4280ECBA, get_fpreg_s(6));
      ASSERT_EQUAL_X(0xA0000000, get_fpreg_s(7));
  }

  FTEST(instruct, sd_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          desired = f1 - f2;
          set_mem(0x400, 0x2b020000);  /* SDR 0,2 */
          test_inst(0x0);
          result = cnvt_64_float(0);
          ratio = fabs((result - desired) / desired);
          ASSERT_TRUE(ratio < .000001);
          did++;
      }
  }

  /* Multiply double */
  FTEST(instruct, md) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6c60d000);  /* MD 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4293fb6f, get_fpreg_s(6));
      ASSERT_EQUAL_X(0x16000000, get_fpreg_s(7));
  }

  FTEST(instruct, md_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          desired = f1 * f2;
          set_mem(0x400, 0x2c020000);  /* MDR 0,2 */
          test_inst(0x2);
          result = cnvt_64_float(0);
          if (floatToFpreg(4, desired) != 0)
              continue;
          ratio = fabs((result - desired) / desired);
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75) {
              ASSERT_TRUE(trap_flag);
          } else {
              ASSERT_TRUE(ratio < .000001);
          }
          did++;
      }
      ASSERT_TRUE(did > 80);
  }

  /* Divide double */
  FTEST(instruct, dd) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6d60d000);  /* DD 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x42725012, get_fpreg_s(6));
      ASSERT_EQUAL_X(0xf5527d99, get_fpreg_s(7));
  }

  FTEST(instruct, dd_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(1);
      for (i = 0; i < 100; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          desired = f1 / f2;
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75)
              continue;
          set_mem(0x400, 0x2d020000);  /* DDR 0,2 */
          test_inst(0x0);
          result = cnvt_64_float(0);
          ratio = fabs((result - desired) / desired);
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75) {
              ASSERT_TRUE(trap_flag);
          } else {
              ASSERT_TRUE(ratio < .000001);
          }
          did++;
      }
      ASSERT_TRUE(did > 80);
  }

  /* Add double unnormalized */
  FTEST(instruct, aw) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6e60d000);  /* AU 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x43083334, get_fpreg_s(6));
      ASSERT_EQUAL_X(0x56000000, get_fpreg_s(7));
  }

  /* Subtract double unnormalized */
  FTEST(instruct, sw) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x6f60d000);  /* SU 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x43080ecb, get_fpreg_s(6));
      ASSERT_EQUAL_X(0xaa000000, get_fpreg_s(7));
  }

  /* Store float point */
  FTEST(instruct, ste) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x404, 0x11223344);
      set_mem(0x400, 0x70012100);  /* STE 0,100(1,2) */
      set_mem(0x500, 0xaabbccdd);
      set_mem(0x505, 0x11223344);
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_mem(0x500));
      ASSERT_EQUAL_X(0x11223344, get_mem(0x504));
  }

  /* Load floating point */
  FTEST(instruct, le) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x500, 0x11223344);
      set_mem(0x505, 0x11223344);
      set_mem(0x400, 0x78012100);  /* LE 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x11223344, get_fpreg_s(0));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(1));
  }

  /* Compare floating point */
  FTEST(instruct, ce) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x500, 0x11223344);
      set_mem(0x400, 0x79012100);  /* CE 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12345678, get_fpreg_s(0));
      ASSERT_EQUAL(CC2, CC_REG);
  }

  /* Half instruct rand */
  FTEST(instruct, he_rand) {
      int i;
      double f1;
      double result;
      double desired;
      double ratio;
      int did = 0;

      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          if (floatToFpreg(2, f1) != 0)
              continue;
          desired = f1/2.0;
          set_mem(0x400, 0x34020000);  /* HER 0,2 */
          test_inst(0x0);
          result = cnvt_32_float(0);
          ratio = fabs((result - desired) / desired);
          ASSERT_TRUE(ratio < .000001);
          did++;
      }
  }

  /* Add floating point */
  FTEST(instruct, ae) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x500, 0x11223344);
      set_mem(0x400, 0x7a012100);  /* AE 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x123679ac, get_fpreg_s(0));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(1));
  }

  FTEST(instruct, ae_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < 100; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          if (floatToFpreg(4, f1) != 0)
              continue;
          desired = f1 + f2;
          if (floatToFpreg(6, desired) != 0)
              continue;
          set_mem(0x400, 0x3a020000);  /* AER 0,2 */
          test_inst(0x2);
          result = cnvt_32_float(0);
          ratio = fabs((result - desired) / desired);
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75) {
              ASSERT_TRUE(trap_flag);
          } else {
              ASSERT_TRUE(ratio < .000001);
          }
          did++;
      }
  }

  /* Subtract floating point */
  FTEST(instruct, se) {
      init_cpu();
      set_fpreg_s(0, 0x12345678);
      set_fpreg_s(1, 0xaabbccdd);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x500, 0x11223344);
      set_mem(0x400, 0x7b012100);  /* SE 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x12323343, get_fpreg_s(0));
      ASSERT_EQUAL_X(0xaabbccdd, get_fpreg_s(1));
  }

  FTEST(instruct, se_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          f1 = cnvt_32_float(0);
          f2 = cnvt_32_float(2);
          desired = f1 - f2;
          set_mem(0x400, 0x3b020000);  /* SER 0,2 */
          test_inst(0x0);
          result = cnvt_32_float(0);
          ratio = fabs((result - desired) / desired);
          ASSERT_TRUE(ratio < .000001);
          did++;
      }
  }

  /* Multiply floating point */
  FTEST(instruct, me) {
      init_cpu();
      set_fpreg_s(0, 0x43082100);
      set_fpreg_s(1, 0xaabbccdd);
      set_mem(0x500, 0x41123456);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x400, 0x7c012100);  /* ME 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x4293fb6f, get_fpreg_s(0));
  }

  FTEST(instruct, me_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          f1 = cnvt_32_float(0);
          f2 = cnvt_32_float(2);
          desired = f1 * f2;
          set_mem(0x400, 0x3c020000);  /* MER 0,2 */
          test_inst(0x2);
          result = cnvt_32_float(0);
          ratio = fabs((result - desired) / desired);
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75) {
              ASSERT_TRUE(trap_flag);
          } else {
              ASSERT_TRUE(ratio < .000001);
          }
          did++;
      }
      ASSERT_TRUE(did > 80);
  }

  /* Divide floating point */
  FTEST(instruct, de) {
      init_cpu();
      set_fpreg_s(0, 0x43082100);
      set_fpreg_s(1, 0xaabbccdd);
      set_mem(0x500, 0x41123456);
      set_reg(1, 0x100);
      set_reg(2, 0x300);
      set_mem(0x400, 0x7d012100);  /* DE 0,100(1,2) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x42725012, get_fpreg_s(0));
  }

  FTEST(instruct, de_rand) {
      int i;
      double f1;
      double f2;
      double result;
      double desired;
      double ratio;
      int did = 0;
      srand(5);
      for (i = 0; i < testcycles; i++) {
          f1 = randfloat(200);
          f2 = randfloat(200);
          if (floatToFpreg(0, f1) != 0)
              continue;
          if (floatToFpreg(2, f2) != 0)
              continue;
          f1 = cnvt_32_float(0);
          f2 = cnvt_32_float(2);
          desired = f1 / f2;
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75)
              continue;
          set_mem(0x400, 0x3d020000);  /* DER 0,2 */
          test_inst(0x2);
          result = cnvt_32_float(0);
          ratio = fabs((result - desired) / desired);
          if (fabs(desired) < 5.4e-79 || fabs(desired) > 7.2e75) {
              ASSERT_TRUE(trap_flag);
          } else {
              ASSERT_TRUE(ratio < .000001);
          }
          did++;
      }
      ASSERT_TRUE(did > 80);
  }

  /* Add floating point unnormalized */
  FTEST(instruct, au) {
      /* Princ Ops 153 */
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x7e60d000);  /* AU 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x43083334, get_fpreg_s(6));
  }

  /* Subtract floating point unnormalized */
  FTEST(instruct, su) {
      init_cpu();
      set_fpreg_s(6, 0x43082100);
      set_fpreg_s(7, 0x00000000);
      set_mem(0x2000, 0x41123456);
      set_mem(0x2004, 0x00000000);
      set_reg(13, 0x00002000);
      set_mem(0x400, 0x7f60d000);  /* SU 6,0(0, 13) */
      test_inst(0x0);
      ASSERT_EQUAL_X(0x43080ecb, get_fpreg_s(6));
  }

  /* Add packed with offset */
  DTEST(instruct, ap_lr) {
      init_cpu();
      set_reg(1, 0x12345678);
      set_mem(0x100, 0x0043212c); /* 2+ */
      set_mem(0x200, 0x0023413c); /* 3+ */
      set_mem(0x400, 0xfa220101); /* AP 101(3,0),201(3,0) */
      set_mem(0x404, 0x02011831);
      set_mem(0x408, 0x00000000);
      test_io_inst(0);
      ASSERT_EQUAL_X(0x0066625c, get_mem(0x100)); /* 5+ */
//      ASSERT_EQUAL(CC2, CC_REG); /* Positive */
      ASSERT_EQUAL_X(0x12345678, get_reg(3));
 //     ASSERT_EQUAL(CC3, CC_REG);
  }

  MTEST(instruct, mvin) {
      init_cpu();
      log_trace("MVIN\n");
      set_mem(0x200, 0xC1C2C3C4);
      set_mem(0x204, 0xC5C6C7C8);
      set_mem(0x208, 0xC9CACB00);
      set_mem(0x300, 0xF1F2F3F4);
      set_mem(0x304, 0xF5F6F7F8);
      set_mem(0x308, 0xF9000000);
      set_mem(0x400, 0xe8070200);  /* MVINV 200(7),300 */
      set_mem(0x404, 0x03070000);
      test_inst(0x0);
      ASSERT_EQUAL_X(0xF8F7F6F5, get_mem(0x200));
      ASSERT_EQUAL_X(0xF4F3F2F1, get_mem(0x204));
      ASSERT_EQUAL_X(0xC9CACB00, get_mem(0x208));
  }

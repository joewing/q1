
; GCD
; Compute B = GCD(B, C)
gcd:
   stb   gcd_x
   stc   gcd_y
gcd_loop:
   ldb   gcd_y
   not
   mab
   inc
   mab
   ldc   gcd_x
   add
   jz    gcd_result_x
   jc    gcd_x_greater
   mab
   not
   mab
   inc
   sta   gcd_y
   j     gcd_loop
gcd_x_greater:
   sta   gcd_x
   j     gcd_loop
gcd_result_x:
   ldb   gcd_x
   ret
gcd_x:
   db    0
gcd_y:
   db    0


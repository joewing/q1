
; Multiplication subroutine.
; Compute B = B * C
mult:
   stb   mult_x
   stc   mult_y
   clr
   sta   mult_result
mult_loop:
   ldb   mult_x
   shr
   sta   mult_x
   ldb   mult_y
   jc    mult_bit_set
   jz    mult_done
   j     mult_not_set
mult_bit_set:
   ldc   mult_result
   add
   sta   mult_result
mult_not_set:
   shl
   sta   mult_y
   j     mult_loop
mult_done:
   ldb   mult_result
   ret
mult_x:
   db    0
mult_y:
   db    0
mult_result:
   db    0


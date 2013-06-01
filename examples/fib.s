; Compute the largest Fibonacci number that fits in 8 bits.
; 32 bytes
fib:
   ldb   fib_init
   stb   fib_n1
   stb   fib_n2
fib_loop:
   ldb   fib_n1
   ldc   fib_n2
   stb   fib_n2
   add
   jc    fib_done
   sta   fib_n1
   j     fib_loop
fib_done:
   hlt
fib_n1:
   db    1
fib_n2:
   db    1
fib_init:
   db    1

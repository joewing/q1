; Sieve of Eratosthenes
; Compute the largest prime number that fits in 8 bits.
; 85 + 256 bytes
sieve:
   ldb   sieve_zero
   stb   sieve_init_index + 2
sieve_init_loop:
   ldb   sieve_zero
sieve_init_index:
   stb   sieve_primes
   ldb   sieve_init_index + 2
   inc
   sta   sieve_init_index + 2
   jc    sieve_init_done
   j     sieve_init_loop
sieve_init_done:
   mab
   inc
   sta   sieve_next_test + 2
sieve_next_num:
   ldb   sieve_next_test + 2
   inc
   jc    sieve_done
   sta   sieve_next_test + 2
sieve_next_test:
   ldb   sieve_primes
   inc
   jc    sieve_next_num
   ldb   sieve_next_test + 2
   stb   sieve_largest
   stb   sieve_index + 2
sieve_loop:
   ldb   sieve_index + 2
   ldc   sieve_largest
   add
   jc    sieve_next_num
   sta   sieve_index + 2
   ldb   sieve_neg1
sieve_index:
   stb   sieve_primes
sieve_zero:
   j     sieve_loop
sieve_done:
   ldb   sieve_largest
   hlt
sieve_neg1:
   db    255
sieve_largest:
   db    0
   org   256
sieve_primes:

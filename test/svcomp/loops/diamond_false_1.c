/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assert(int);
extern unsigned int __VERIFIER_nondet_uint(void);


int main(void) {
  unsigned int x = 0;
  unsigned int y = __VERIFIER_nondet_uint();

  while (x < 99) {
    if (y % 2 == 0) {
      x++;
    } else {
      x += 2;
    }
  }

  __VERIFIER_assert((x % 2) == (y % 2)); /* ERROR */
}

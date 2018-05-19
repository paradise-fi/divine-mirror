/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym big c */
extern unsigned int __VERIFIER_nondet_uint(void);
extern void __VERIFIER_error(void);

unsigned int id(unsigned int x);
unsigned int id2(unsigned int x);

unsigned int id(unsigned int x) {
  if (x==0) return 0;
  unsigned int ret = id2(x-1) + 1;
  if (ret > 2) return 2;
  return ret;
}

unsigned int id2(unsigned int x) {
  if (x==0) return 0;
  unsigned int ret = id(x-1) + 1;
  if (ret > 2) return 2;
  return ret;
}

int main(void) {
  unsigned int input = __VERIFIER_nondet_uint();
  unsigned int result = id(input);
  if (result == 3) {
    ERROR: __VERIFIER_error();
  }
}

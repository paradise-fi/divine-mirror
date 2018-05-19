/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym big c */
extern int __VERIFIER_nondet_int(void);
extern void __VERIFIER_error(void);

int id(int x) {
  if (x==0) return 0;
  int ret = id((unsigned int)x-1) + 1;
  if (ret > 3) return 3;
  return ret;
}

int main(void) {
  int input = __VERIFIER_nondet_int();
  int result = id(input);
  if (result == 5) {
    ERROR: __VERIFIER_error();
  }
}

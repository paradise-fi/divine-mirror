/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
extern void __VERIFIER_error(void);

int sum(int n, int m) {
    if (n <= 0) {
      return m + n;
    } else {
      return sum(n - 1, m + 1);
    }
}

int main(void) {
  int a = 15;
  int b = 0;
  int result = sum(a, b);
  if (result != a + b) {
    ERROR: __VERIFIER_error();
  }
}

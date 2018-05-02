/* VERIFY_OPTS: --symbolic --sequential */
/* TAGS: sym todo c */
extern void __VERIFIER_error() __attribute__ ((__noreturn__));

/*
 * Recursive implementation integer addition.
 * 
 * Author: Matthias Heizmann
 * Date: 2013-07-13
 * 
 */

extern int __VERIFIER_nondet_int(void);

long long addition(long long m, long long n) {
    if (n == 0) {
        return m;
    }
    if (n > 0) {
        return addition(m+1, n-1);
    }
    if (n < 0) {
        return addition(m-1, n+1);
    }
}


int main() {
    int m = __VERIFIER_nondet_int();
    int n = __VERIFIER_nondet_int();
    long long result = addition(m,n);
    if (m < 100 || n < 100 || result >= 200) {
        return 0;
    } else {
        ERROR: __VERIFIER_error();
    }
}
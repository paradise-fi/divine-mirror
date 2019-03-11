/* TAGS: c */
/* VERIFY_OPTS: -o nofail:malloc */
// Source: Shaz Qadeer, Dinghao Wu: "KISS: Keep It Simple and Sequential",
// PLDI 2004
// Simplified to remove heap accesses

#include <pthread.h>
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error(); /* ERROR */
  }
  return;
}
extern void __VERIFIER_atomic_begin();
extern void __VERIFIER_atomic_end();

volatile int stoppingFlag;
volatile int pendingIo;
volatile int stoppingEvent;
volatile int stopped;

int BCSP_IoIncrement() {
    if (stoppingFlag) {
	return -1;
    } else {
	pendingIo = pendingIo + 1;
    }
    return 0;
}

int dec() {
    __VERIFIER_atomic_begin();
    pendingIo--;
    int tmp = pendingIo;
    __VERIFIER_atomic_end();
    return tmp;
}

void BCSP_IoDecrement() {
    int pending;
    pending = dec();
    if (pending == 0) {
	stoppingEvent = 1;
    }
}

void* BCSP_PnpAdd(void* arg) {
    int status;
    status = BCSP_IoIncrement();
    if (status == 0) {
	__VERIFIER_assert(!stopped);
    }
    BCSP_IoDecrement();
}

void* BCSP_PnpStop(void* arg) {
    stoppingFlag = 1;
    BCSP_IoDecrement();
    __VERIFIER_assume(stoppingEvent);
    stopped = 1;
}

int main() {
    pthread_t t;
    pendingIo = 1;
    stoppingFlag = 0;
    stoppingEvent = 0;
    stopped = 0;
    pthread_create(&t, 0, BCSP_PnpStop, 0);
    BCSP_PnpAdd(0);
    return 0;
}

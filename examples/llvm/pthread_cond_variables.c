/*
 * This program simulates producer-consument(s) pattern, implemented using
 * conditional variables. Instead of spinning, consumers are sleeping
 * until some item is enqueued into the shared queue.
 * If compiled with -DBUG, condition is not re-checked after the
 * pthread_cond_wait returns. But this is obviously incorrect because
 * when a thread receives a signal it is not necessarily the first one that
 * gets scheduled when the signalling thread unlocks the mutex.
 * For example we could have the following series of operations:
 *  1. Cons_0 locks queue_mutex, sees that enqueued == 0, and waits (releases mutex).
 *  2. Prod locks queue_mutex, produces new item and signals about new item being added.
 *  3. Cons_1 tries to lock queue_mutex, but fails and has to wait.
 *  4. Prod unclocks queue_mutex and the scheduler decides to schedule Cons_1
       instead of Cons_0.
 *  5. Cons_1 locks queue_mutex, sees that enqueued != 0, and removes the item
 *     (but note that Prod already signalled Cons_0 that it should get this item).
 *  6. Cons_1 unlocks queue_mutex and the scheduler resumes Cons_0.
 *  7. Cons_0 now tries to remove from an empty queue.
 *
 * Thus at least two consumers and two items to process are needed to have
 * assertion violated in some run of the program.
 *
 * Current implementation of conditional variables in DiVinE - LLVM interpreter
 * also takes into consideration that pthread_cond_signal can send signal to more
 * than one thread (as it is defined in the POSIX specifications).
 * However spurious wakeup is not yet implemented.
 */

#include "divine-llvm.h"

#ifndef DIVINE
#include "unistd.h"
#endif

#define NUM_CONSUMERS         2
#define NUM_ITEMS_TO_PROCESS  2

volatile int enqueued = 0;
volatile int dequeued = 0;

pthread_mutex_t queue_mutex;
pthread_cond_t queue_emptiness_cv;

void *consumer(void *arg)
{
    int id = (int) arg;
    while (1) {

        pthread_mutex_lock(&queue_mutex);
        
        #ifdef BUG
        if (dequeued < NUM_ITEMS_TO_PROCESS && enqueued == 0) {
        #else // correct
        while (dequeued < NUM_ITEMS_TO_PROCESS && enqueued == 0) {
        #endif
            trace( "Consumer ID = %d is going to sleep.", id );
            pthread_cond_wait(&queue_emptiness_cv, &queue_mutex);
            trace( "Consumer ID = %d was woken up.", id );
        }

        if (dequeued == NUM_ITEMS_TO_PROCESS) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        } 

        // dequeue
        assert(enqueued>0);
        ++dequeued;
        --enqueued;
        trace( "Consumer ID = %d dequeued an item.", id );

        pthread_mutex_unlock(&queue_mutex);

        // here the thread would do some work with dequeued item (in his local space)
        #ifndef DIVINE
        sleep(1);
        #endif
    }
    return NULL;
}

int main (void)
{
  int i;
  pthread_t threads[NUM_CONSUMERS];

  // initialize mutex and condition variable objects
  pthread_mutex_init(&queue_mutex, NULL);
  pthread_cond_init (&queue_emptiness_cv, NULL);

  // create and start all consumers
  for (i=0; i<NUM_CONSUMERS; i++) {
      pthread_create(&threads[i], 0, consumer, (void*)(i+1));
  }

  // producer:
  for (i=0; i<NUM_ITEMS_TO_PROCESS; i++) {
      // here the main thread would produce some item to process
      #ifndef DIVINE
      sleep(1);
      #endif

      pthread_mutex_lock(&queue_mutex);

      //enqueue
      enqueued++;

      // wake up at least one consumer
      pthread_cond_signal(&queue_emptiness_cv);

      pthread_mutex_unlock(&queue_mutex);
  }

  // when it is finnished, wake up all consumers
  pthread_cond_broadcast(&queue_emptiness_cv);

  // wait for all threads to complete
  for (i=0; i<NUM_CONSUMERS; i++) {
      pthread_join(threads[i], NULL);
  }

  assert(enqueued == 0);
  assert(dequeued == NUM_ITEMS_TO_PROCESS);  

  // clean up and exit
  pthread_mutex_destroy(&queue_mutex);
  pthread_cond_destroy(&queue_emptiness_cv);
  pthread_exit(NULL);

  return 0;
}

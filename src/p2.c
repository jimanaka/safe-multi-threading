/* p2.c: Jake Imanaka, 2023, ICS 612 Project 2
 * Original by Edoardo Biagioni, 2021
 * 
 * This program was compiled and tested on a x86_64 GNU/Linux (kernel 6.1.8-arch) machine.
 * The program compiles properly and implements the 4 requirements outlined
 * by the project description.
 *
 * --------------------------------------------------------------
 * 
 * this program demonstrates that incrementing a variable is not an
 * atomic operation -- if the increment happens in multiple threads,
 * the final total will not be as expected.  This is true when the
 * number of threads and loops is large enough, whereas for small
 * numbers of loops and threads (or in case of a single thread),
 * the computation gives the expected result. 
 *
 * to understand why, consider that to increment a variable in memory,
 * the CPU must read the value from memory into a register, increment
 * the value, then store the new value back into memory.  If two threads
 * are repeatedly doing this at the same time, sometimes they will get
 * the same value v from memory, and both will store back the value v+1,
 * instead of one of them storing v+1 and the other one v+2.  So one of
 * the increments is lost.
 *
 * this program takes up to two optional command-line arguments: the
 * number of threads to start, and the number of loops for each thread.
 * If no command-line arguments are provided, this program starts 2
 * threads, each doing 10 million loops.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#define LOOPS	10 * 1000 * 1000
#define THREADS	2

// Locks the state->counter variable
// NOTE: This program only uses 1 c file so <static> keyword does not change anything here
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

// Locks the state->threads variable
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

// Locks the state->start variable
static pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;

// Used for signaling main thread when all threads are finished
static pthread_cond_t  thread_cond = PTHREAD_COND_INITIALIZER;

// Used for signaling threads to start
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;


struct state_struct {
  int start;             // 0 until ready to start
  volatile long counter; // volatile: always read the value from memory
  volatile long threads;
  long num_loops;        // set in main, not modified in the threads
};

/* after busy-waiting for the start variable to be set, increment the
 * state counter variable the given number of times.  Once that
 * is finished, print that we are finished, taking the thread number
 * from the number of threads that haven't finished yet. */
static void * thread(void * arg)
{
  struct state_struct * state = (struct state_struct *) arg;

  // waits for the main thread to signal for threads to start without busy-waiting
  pthread_mutex_lock(&start_mutex);
  if (state->start == 0) {
    pthread_cond_wait(&start_cond, &start_mutex);
  }
  pthread_mutex_unlock(&start_mutex);
  
  for (int i = 0; i < state->num_loops; i++) {
    // critical section to safely increment state->counter using count_mutex
    pthread_mutex_lock(&count_mutex);
    state->counter++;
    pthread_mutex_unlock(&count_mutex);
  }

  // critical section to access and decrement state->threads using thread_mutex
  pthread_mutex_lock(&thread_mutex);
  printf ("thread %ld finishing\n", state->threads);
  state->threads--;
  // sends signal to main thread that all threads are done using thread_cond if thread count is 0.
  if (state->threads == 0) {
    pthread_cond_signal(&thread_cond);
  }
  pthread_mutex_unlock(&thread_mutex);
  return NULL;
}

/* print to a string the elapsed time and the CPU time relative
 * to the start (elapsed time) and startc (CPU time) variables
 *
 * The string returned is in a static array, so if multiple threads
 * call this function at the same, the results may be inaccurate.
 */
static char * all_times (struct timeval start, clock_t startc)
{
// all computations done in microseconds
#define US_PER_S	(1000 * 1000)
  struct timeval finish;
  gettimeofday (&finish, NULL);
  long delta = (finish.tv_sec - start.tv_sec) * US_PER_S +
               finish.tv_usec - start.tv_usec;
  long delta_cpu = (clock() - startc) * US_PER_S / CLOCKS_PER_SEC;
  static char result [1000];
  snprintf (result, sizeof (result), "%ld.%06lds, cpu time %ld.%06ld",
            delta / US_PER_S, delta % US_PER_S,
            delta_cpu / US_PER_S, delta_cpu % US_PER_S);
  return result;
}

int main (int argc, char ** argv)
{
  long num_threads = (argc <= 1) ? THREADS : atoi (argv[1]);
  struct state_struct state =
    { .start = 0, .counter = 0, .threads = 0,
      .num_loops = (argc <= 2) ? LOOPS : atoi (argv[2]) };

  while (state.threads < num_threads) {
    pthread_t t;
    int i = pthread_create (&t, NULL, thread, (void *)&state);
    if (i != 0) {
      printf ("pthread_create failed with error %d\n", i);
      printf ("  (EAGAIN %d, EINVAL %d, EPERM %d)\n", EAGAIN, EINVAL, EPERM);
      printf ("  EAGAIN may mean too many threads for your system\n");
      exit (1);
    }
    state.threads++;
  }

  struct timeval start;
  gettimeofday (&start, NULL);
  clock_t startc = clock();

  // broadcast to all waiting threads to start.
  // any thread that is created after the broadcast will not wait b/c state.start = 1
  pthread_mutex_lock(&start_mutex);
  state.start = 1;   // start all the threads
  pthread_cond_broadcast(&start_cond);
  pthread_mutex_unlock(&start_mutex);

  // Wait for all threads to finish without busy-waiting
  pthread_mutex_lock(&thread_mutex);
  pthread_cond_wait(&thread_cond, &thread_mutex);
  pthread_mutex_unlock(&thread_mutex);

  // at this point, all threads (except for main) are finished
  printf ("%ld total count, expected %ld, time %ss\n",
          state.counter, state.num_loops * num_threads,
          all_times (start, startc));
}


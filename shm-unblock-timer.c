#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "timer.h"
#include "utils.h"

#define NUM_TEST_ITERATIONS     1000

/*
 * Test that attempts to time how long it takes a child process that is
 * blocked on a lock (held by the parent process) to become unblocked after
 * the lock is released in the parent process.
 *
 * The test loops for <NUM_TEST_ITERATIONS> iterations and uses no sleeps
 * between tests. To reduce the chance that the parent drops lock A and
 * acquires it again before the child process is able to acquire it, the
 * test uses a second lock, lock B. Lock B is optional and not needed for
 * correctness/liveness.
 *
 * The parent and child share a shared memory buffer. The parent records a
 * timestamp just before unlocking a shared mutex which the child process
 * blocks on. The child records a timestamp after it has been unblocked. The
 * delta is logged by the parent process and considered the unblock latency.
 *
 * parent:
 *   enter B
 *   loop:
 *      enter A
 *      exit B
 *      set timestamp_parent_release=tick();
 *      exit A
 *      enter B
 *
 * child:
 *   loop:
 *     enter B
 *     enter A
 *     set timestamp_child_acquire=tick();
 *     exit A
 *     exit B
 */

typedef struct {
  pthread_mutex_t       a;
  pthread_mutex_t       b;
  volatile uint64_t     timestamp_parent_release;
  volatile uint64_t     timestamp_child_acquire;
  volatile int          child_should_exit;
} shared_memory_t;

int child_process(shared_memory_t *shm);
int parent_process(shared_memory_t *shm, int iterations);
int logging_enabled = 0;
int random_sleep_microseconds = 0;

int
main(int argc, char** argv)
{
  int                   rv;
  pid_t                 fork_pid;
  shared_memory_t       *shm;
  pthread_mutexattr_t   attr = {};
  char                  option;
  int                   iterations = NUM_TEST_ITERATIONS;

  timer_init();

  rv = get_args(argc, argv,
      &random_sleep_microseconds,
      &logging_enabled,
      &iterations);
  if (rv != 0) {
    exit (rv);
  }

  shm = (shared_memory_t*) create_shared_memory(sizeof (shared_memory_t));
  bzero(shm, sizeof (shared_memory_t));

  rv = pthread_mutexattr_init(&attr);
  rv = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  rv = pthread_mutex_init(&shm->a, &attr);
  rv = pthread_mutex_init(&shm->b, &attr);

  fork_pid = fork();
  if (fork_pid == -1) {
    LOG_ERR("fork() failed\n");
    rv = -1;
  } else if (fork_pid == 0) {
    LOG("child PID: %d\n", getpid());
    rv = child_process(shm);
  } else {
    LOG("parent PID: %d\n", getpid());
    rv = parent_process(shm, iterations);
  }

  exit(rv);
}

int
parent_process(shared_memory_t *shm, int iterations)
{
  pthread_mutex_t *a, *b;
  int i = 0;
  uint64_t total_delta = 0, min_delta = UINT64_MAX, max_delta = 0;

  a = &shm->a;
  b = &shm->b;

  pthread_mutex_lock(b);

  while (i < iterations) {
    uint64_t delta;

    pthread_mutex_lock(a);
    pthread_mutex_unlock(b);

    if (random_sleep_microseconds)
      random_usleep(random_sleep_microseconds);

    if (shm->timestamp_parent_release == 0 &&
        shm->timestamp_child_acquire == 0) {
      shm->timestamp_parent_release = tick();
    } else if (shm->timestamp_child_acquire &&
               !shm->timestamp_parent_release) {
      assert(0);
    } else if (shm->timestamp_child_acquire) {
      delta = tick_delta_to_nanoseconds(shm->timestamp_child_acquire -
                                        shm->timestamp_parent_release);
      total_delta += delta;
      if (delta < min_delta)
        min_delta = delta;
      if (delta > max_delta)
        max_delta = delta;
      LOG("%" PRIu64 " nanoseconds\n", delta);
      i++;
      shm->timestamp_child_acquire = 0;
      shm->timestamp_parent_release = tick();
    } else {
      // The child didn't get a chance to run during the time we had
      // dropped the locks and reacquired them. Loop until the child
      // has filled in timestamp_child_acquire.
      assert(shm->timestamp_parent_release && !shm->timestamp_child_acquire);
    }

    pthread_mutex_unlock(a);
    pthread_mutex_lock(b);
  }

  pthread_mutex_unlock(b);

  pthread_mutex_lock(a);
  shm->child_should_exit = 1;
  pthread_mutex_unlock(a);

  PRINT("average over %d iterations: %" PRIu64 " nanoseconds\n",
      iterations, total_delta / iterations);
  PRINT("    max over %d iterations: %" PRIu64 " nanoseconds\n",
      iterations, max_delta);
  PRINT("    min over %d iterations: %" PRIu64 " nanoseconds\n",
      iterations, min_delta);

  return 0;
}

int
child_process(shared_memory_t *shm)
{
  pthread_mutex_t *a, *b;

  a = &shm->a;
  b = &shm->b;

  while (1) {
    pthread_mutex_lock(b);
    pthread_mutex_lock(a);

    if (shm->child_should_exit) {
      pthread_mutex_unlock(a);
      pthread_mutex_unlock(b);
      return 0;
    }

    if (!shm->timestamp_child_acquire && shm->timestamp_parent_release) {
      shm->timestamp_child_acquire = tick();
    }

    pthread_mutex_unlock(a);
    pthread_mutex_unlock(b);
  }
}

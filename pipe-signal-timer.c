#include <assert.h>
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
#define PRINT(args...)          logging(1, stdout, args)
#define LOG_ERR(args...)        logging(1, stderr, args)
#define LOG(args...)            logging(logging_enabled, stdout, args)

#define PIPE_RD_END                     0
#define PIPE_WR_END                     1

#define MSG_POKE_READY                  1
#define MSG_POKE                        2
#define MSG_POKE_REPLY                  3

typedef struct {
  pthread_t             wait_thread;
  pthread_cond_t        wait_cv;
  pthread_mutex_t       wait_lock;
  int                   wait_thread_ready;

  int                   send_fd;
  int                   recv_poke_fd;
  int                   child_should_exit;
} child_state_t;

typedef struct {
  int                   send_poke_fd;
  int                   recv_fd;
  int                   iterations;
} parent_state_t;

typedef struct {
  int                   type;
} poke_ready_msg_t;

typedef struct {
  int                   type;
  int                   child_should_exit;
} poke_msg_t;

typedef struct {
  int                   type;
  uint64_t              tick;
} poke_reply_msg_t;

int parent_process(parent_state_t *pstatep);
int parent_do_poke_test(parent_state_t *pstate);
void parent_do_shutdown(parent_state_t *pstatep);
int child_process(child_state_t *cstatep);
void* child_recv_poke_thread_func(void* data);
void* child_wait_thread_func(void* data);
int logging_enabled = 0;
int random_sleep_microseconds = 0;

int
main(int argc, char** argv)
{
  int           rv;
  int           pipe1[2], pipe2[2];
  pid_t         fork_pid;
  char          option;
  int           iterations = NUM_TEST_ITERATIONS;

  timer_init();

  rv = get_args(argc, argv,
      &random_sleep_microseconds,
      &logging_enabled,
      &iterations);
  if (rv != 0) {
    exit (rv);
  }

  rv = pipe(pipe1);
  if (rv == -1) {
    LOG_ERR("pipe() failed\n");
    exit(-1);
  }
  rv = pipe(pipe2);
  if (rv == -1) {
    LOG_ERR("pipe() failed\n");
    exit(-1);
  }

  fork_pid = fork();
  if (fork_pid == -1) {
    LOG_ERR("fork() failed\n");
    rv = -1;
  } else if (fork_pid == 0) {
    child_state_t cstate = {};

    LOG("child PID: %d\n", getpid());

    // pipe1: parent->child (poke)
    // pipe2: child->parent (poke reply)
    close(pipe1[PIPE_WR_END]);
    close(pipe2[PIPE_RD_END]);

    cstate.recv_poke_fd = pipe1[PIPE_RD_END];
    cstate.send_fd      = pipe2[PIPE_WR_END];

    rv = child_process(&cstate);
  } else {
    parent_state_t pstate = {};

    LOG("parent PID: %d\n", getpid());

    // pipe1: parent->child (poke)
    // pipe2: child->parent (poke reply)
    close(pipe1[PIPE_RD_END]);
    close(pipe2[PIPE_WR_END]);

    pstate.send_poke_fd       = pipe1[PIPE_WR_END];
    pstate.recv_fd            = pipe2[PIPE_RD_END];
    pstate.iterations         = iterations;

    rv = parent_process(&pstate);
  }

  exit(rv);
}

void*
child_wait_thread_func(void *data)
{
  child_state_t *cstatep = (child_state_t *)data;

  pthread_mutex_lock(&cstatep->wait_lock);

  while (1) {
    int                 rv;
    uint64_t            wakeup_tick;
    poke_reply_msg_t    poke_reply = {};

    cstatep->wait_thread_ready = 1;
    pthread_cond_signal(&cstatep->wait_cv);

    (void) pthread_cond_wait(&cstatep->wait_cv, &cstatep->wait_lock);

    poke_reply.tick = tick();
    poke_reply.type = MSG_POKE_REPLY;

    if (cstatep->child_should_exit) {
      break;
    }

    rv = write_bytes(cstatep->send_fd, sizeof (poke_reply), &poke_reply);
    if (rv != 0) {
      break;
    }
  }

  pthread_mutex_unlock(&cstatep->wait_lock);

  return NULL;
}

int
child_process(child_state_t *cstatep)
{
  int                   rv, *thread_status;

  rv = pthread_cond_init(&cstatep->wait_cv, NULL);
  if (rv != 0) {
    LOG_ERR("child_process: pthread_cond_init() failed\n");
    return rv;
  }

  rv = pthread_mutex_init(&cstatep->wait_lock, NULL);
  if (rv != 0) {
    LOG_ERR("child_process: pthread_mutex_init() failed\n");
    return rv;
  }

  rv = pthread_create(&cstatep->wait_thread, NULL,
                      child_wait_thread_func, cstatep);
  if (rv != 0) {
    LOG_ERR("child_process: pthread_create() failed\n");
    return rv;
  }

  while (1) {
    poke_msg_t          poke_msg = {};
    poke_ready_msg_t    poke_ready = {};

    // wait until wait_thread is ready
    pthread_mutex_lock(&cstatep->wait_lock);
    while (!cstatep->wait_thread_ready) {
      (void) pthread_cond_wait(&cstatep->wait_cv, &cstatep->wait_lock);
    }

    // tell parent we are ready for poke
    poke_ready.type = MSG_POKE_READY;
    rv = write_bytes(cstatep->send_fd, sizeof (poke_ready), &poke_ready);
    if (rv != 0) {
      break;
    }

    // wait for poke message
    rv = read_bytes(cstatep->recv_poke_fd, sizeof (poke_msg), &poke_msg);
    if (rv != 0) {
      LOG_ERR("%s: error: read_bytes returned %d\n", __FUNCTION__, rv);
      break;
    }

    if (poke_msg.child_should_exit) {
      cstatep->child_should_exit = 1;
      pthread_cond_signal(&cstatep->wait_cv);
      pthread_mutex_unlock(&cstatep->wait_lock);
      break;
    }

    cstatep->wait_thread_ready = 0;
    pthread_cond_signal(&cstatep->wait_cv);
    pthread_mutex_unlock(&cstatep->wait_lock);
  }

  (void) pthread_join(cstatep->wait_thread, (void**)&thread_status);
  (void) pthread_cond_destroy(&cstatep->wait_cv);
  (void) pthread_mutex_destroy(&cstatep->wait_lock);

  return 0;
}

int
parent_process(parent_state_t *pstatep)
{
  int                   rv;
  pthread_mutexattr_t   attr;
  uint64_t              total_delta = 0, min_delta = UINT64_MAX, max_delta = 0;

  for (int i = 0; i <= pstatep->iterations; i++) {
    poke_ready_msg_t    poke_ready = {};
    poke_msg_t          poke = {};
    poke_reply_msg_t    poke_reply = {};
    uint64_t            poke_start_time, delta;

    if (random_sleep_microseconds)
      random_usleep(random_sleep_microseconds);

    rv = read_bytes(pstatep->recv_fd, sizeof (poke_ready), &poke_ready);
    if (rv == 0) {
      assert(poke_ready.type = MSG_POKE_READY);
    } else {
      break;
    }

    if (i == pstatep->iterations) {
      // we're done
      poke.child_should_exit = 1;
    }

    poke.type = MSG_POKE;
    poke_start_time = tick();
    rv = write_bytes(pstatep->send_poke_fd, sizeof (poke), &poke);
    if (rv != 0 || i == pstatep->iterations) {
      break;
    }

    rv = read_bytes(pstatep->recv_fd, sizeof (poke_reply), &poke_reply);
    if (rv == 0) {
      assert(poke_ready.type = MSG_POKE_REPLY);
    } else {
      break;
    }

    delta = tick_delta_to_nanoseconds(poke_reply.tick - poke_start_time);
    LOG("%" PRIu64 " nanoseconds\n", delta);

    total_delta += delta;
    if (delta < min_delta)
      min_delta = delta;
    if (delta > max_delta)
      max_delta = delta;
  }

  PRINT("average over %d iterations: %" PRIu64 " nanoseconds\n",
      pstatep->iterations, total_delta / pstatep->iterations);
  PRINT("    max over %d iterations: %" PRIu64 " nanoseconds\n",
      pstatep->iterations, max_delta);
  PRINT("    min over %d iterations: %" PRIu64 " nanoseconds\n",
      pstatep->iterations, min_delta);

  return 0;
}

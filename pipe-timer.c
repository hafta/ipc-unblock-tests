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

#define MSG_POKE_READY          1
#define MSG_POKE                2
#define MSG_POKE_REPLY          3

typedef struct {
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

int
child_process(child_state_t *cstatep)
{
  int rv;

  while (1) {
    poke_msg_t          poke_msg = {};
    poke_reply_msg_t    poke_reply = {};

    rv = read_bytes(cstatep->recv_poke_fd, sizeof (poke_msg), &poke_msg);
    if (rv != 0) {
      LOG_ERR("%s: error: read_bytes returned %d\n", __FUNCTION__, rv);
      break;
    }

    poke_reply.tick = tick();
    assert(poke_msg.type == MSG_POKE);

    if (poke_msg.child_should_exit) {
      break;
    }

    poke_reply.type = MSG_POKE_REPLY;
    rv = write_bytes(cstatep->send_fd, sizeof (poke_reply), &poke_reply);
    if (rv != 0) {
      break;
    }
  }

  return rv;
}

int
parent_process(parent_state_t *pstatep)
{
  int                   rv;
  uint64_t              total_delta = 0, min_delta = UINT64_MAX, max_delta = 0;

  for (int i = 0; i <= pstatep->iterations; i++) {
    poke_msg_t          poke = {};
    poke_reply_msg_t    poke_reply = {};
    uint64_t            poke_start_time, delta;

    if (i == pstatep->iterations) {
      // we're done
      poke.child_should_exit = 1;
    }

    if (random_sleep_microseconds)
      random_usleep(random_sleep_microseconds);

    poke.type = MSG_POKE;
    poke_start_time = tick();
    rv = write_bytes(pstatep->send_poke_fd, sizeof (poke), &poke);
    if (rv != 0 || i == pstatep->iterations) {
      break;
    }

    rv = read_bytes(pstatep->recv_fd, sizeof (poke_reply), &poke_reply);
    if (rv != 0) {
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

  return rv;
}

These tests attempt to measure the time it takes for a thread blocked in one
process to become unblocked and run when signalled by another process.

The shm-unblock-timer test uses a pthread mutex in shared memory shared between
two processes. It measures the time between the parent process unlocking the
mutex and the child process being woken up.

pipe-timer measures the time it takes when the child process is blocked on a
pipe read and is woken up by the parent process sending a poke message on the
pipe.

pipe-signal-timer is similar to pipe-timer, but includes a context switch in
the child process. The child process blocks on a pipe read, but when it is
woken up by receiving the poke message, it wakes up a thread in the child
process blocked on a pthread condition variable. The test measures the time
between the parent process sending the pipe message and the child process thread
blocked on the condition variable being woken up.

To build:

```
$ make
```

To run the tests:

```
$ make test
```

Options:
```
-l                 enables logging
-i <ITERATIONS>    specify the number of test iterations
-s <MICROSECONDS>  enables random sleeps up to MICROSECONDS
```

Examples:

```
$ TEST_ARGS="-s 1000000 -i 10 -l" make test
Set TEST_ARGS to pass arguments to the tests.
./shm-unblock-timer -s 1000000 -i 10 -l
parent PID: 67960
child PID: 67961
39985 nanoseconds
75229 nanoseconds
62833 nanoseconds
45997 nanoseconds
49180 nanoseconds
38887 nanoseconds
71988 nanoseconds
75625 nanoseconds
71083 nanoseconds
47735 nanoseconds
average over 10 iterations: 57854 nanoseconds
    max over 10 iterations: 75625 nanoseconds
    min over 10 iterations: 38887 nanoseconds
./pipe-timer -s 1000000 -i 10 -l
parent PID: 67962
child PID: 67963
39196 nanoseconds
57187 nanoseconds
32155 nanoseconds
28985 nanoseconds
40042 nanoseconds
34950 nanoseconds
32591 nanoseconds
51357 nanoseconds
33967 nanoseconds
33827 nanoseconds
average over 10 iterations: 38425 nanoseconds
    max over 10 iterations: 57187 nanoseconds
    min over 10 iterations: 28985 nanoseconds
./pipe-signal-timer -s 1000000 -i 10 -l
parent PID: 67965
child PID: 67966
122126 nanoseconds
54624 nanoseconds
73360 nanoseconds
54567 nanoseconds
47158 nanoseconds
64621 nanoseconds
55525 nanoseconds
50120 nanoseconds
103792 nanoseconds
55278 nanoseconds
average over 10 iterations: 68117 nanoseconds
    max over 10 iterations: 122126 nanoseconds
    min over 10 iterations: 47158 nanoseconds
```

```
$ TEST_ARGS="-s 1000 -i 1000" make test
Set TEST_ARGS to pass arguments to the tests.
./shm-unblock-timer -s 1000 -i 1000
average over 1000 iterations: 25901 nanoseconds
    max over 1000 iterations: 72716 nanoseconds
    min over 1000 iterations: 5483 nanoseconds
./pipe-timer -s 1000 -i 1000
average over 1000 iterations: 21692 nanoseconds
    max over 1000 iterations: 65376 nanoseconds
    min over 1000 iterations: 4192 nanoseconds
./pipe-signal-timer -s 1000 -i 1000
average over 1000 iterations: 30728 nanoseconds
    max over 1000 iterations: 80860 nanoseconds
    min over 1000 iterations: 6852 nanoseconds
```

```
$ TEST_ARGS="-i 100000" make test
Set TEST_ARGS to pass arguments to the tests.
./shm-unblock-timer -i 100000
average over 100000 iterations: 4498 nanoseconds
    max over 100000 iterations: 292312 nanoseconds
    min over 100000 iterations: 70 nanoseconds
./pipe-timer -i 100000
average over 100000 iterations: 2775 nanoseconds
    max over 100000 iterations: 141333 nanoseconds
    min over 100000 iterations: 919 nanoseconds
./pipe-signal-timer -i 100000
average over 100000 iterations: 6098 nanoseconds
    max over 100000 iterations: 102353 nanoseconds
    min over 100000 iterations: 3806 nanoseconds
```

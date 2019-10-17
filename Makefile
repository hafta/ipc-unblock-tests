SHELL = /bin/sh

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CCFLAGS += -D LINUX
endif
ifeq ($(UNAME_S),Darwin)
	CCFLAGS += -D MACOS
endif

all: shm-unblock-timer pipe-timer pipe-signal-timer

shm-unblock-timer: utils.c timer.c shm-unblock-timer.c
	$(CC) $(CCFLAGS) $? -pthread -o $@

pipe-timer: utils.c timer.c pipe-timer.c
	$(CC) $(CCFLAGS) $? -pthread -o $@

pipe-signal-timer: utils.c timer.c pipe-signal-timer.c
	$(CC) $(CCFLAGS) $? -pthread -o $@

test:
	./shm-unblock-timer $(TEST_ARGS)
	./pipe-timer $(TEST_ARGS)
	./pipe-signal-timer $(TEST_ARGS)

clean:
	rm -f shm-unblock-timer pipe-timer pipe-signal-timer
	rm -f -r *.dSYM

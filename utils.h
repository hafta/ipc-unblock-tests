#include <stdint.h>

#define PRINT(args...)          logging(1, stdout, args)
#define LOG_ERR(args...)        logging(1, stderr, args)
#define LOG(args...)            logging(logging_enabled, stdout, args)

#define PIPE_RD_END             0
#define PIPE_WR_END             1

int read_bytes(int fd, uint32_t bytes_to_read, void *buf);
int write_bytes(int fd, uint32_t bytes_to_write, void *buf);
void logging(int logging_enabled, FILE *fp, const char *format, ...);
void *create_shared_memory(size_t shm_size);
void random_usleep(uint64_t max_microseconds);
void usage(int argc, char **argv);
int get_args(int argc, char **argv,
    int *sleepp, int *loggingp, int *iterationsp);

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>

#include "utils.h"

void
usage(int argc, char **argv)
{
  PRINT("usage: %s [-l] [-i <iterations>]\n\n", argv[0]);
  PRINT("  -l                 enables logging\n");
  PRINT("  -i <ITERATIONS>    specify the number of test iterations\n");
  PRINT("  -s <MICROSECONDS>  enables random sleeps up to MICROSECONDS\n");
}

int
get_args(int argc, char **argv, int *sleepp, int *loggingp, int *iterationsp)
{
  char option;

  while ((option = getopt(argc, argv, "s:li:")) != -1) {
    switch (option)
    {
    case 's':
      *sleepp = atoi(optarg);
      if (*sleepp <= 0) {
        LOG_ERR("Option -%c should be a positive integer.\n", optopt);
        return -1;
      }
      break;
    case 'l':
      *loggingp = 1;
      break;
    case 'i':
      *iterationsp = atoi(optarg);
      if (*iterationsp <= 0) {
        LOG_ERR("Option -%c should be a positive integer.\n", optopt);
        return -1;
      }
      break;
    default:
      usage(argc, argv);
      return -1;
    }
  }

  return 0;
}

void
random_usleep(uint64_t max_microseconds)
{
    usleep(random() % max_microseconds);
}

void*
create_shared_memory(size_t shm_size)
{
  void *shared_memory;
  shared_memory = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANON, -1, 0);
  return shared_memory;
}

void
logging(int logging_enabled, FILE *fp, const char *format, ...)
{
  if (logging_enabled) {
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fflush(fp);
  }
}

// Return 0 on success
int
read_bytes(int fd, uint32_t bytes_to_read, void *buf)
{
  uint32_t bytes_remaining = bytes_to_read;
  char *read_location = (char *)buf;

  while (bytes_remaining > 0) {
    uint32_t bytes_read;

    bytes_read = read(fd, read_location, bytes_remaining);

    if (bytes_read == -1) {
      //hlog("read error: errno: %d - %s\n", errno, strerror(errno));
      return -1;
    }

    if (bytes_read < bytes_remaining) {
      bytes_remaining -= bytes_read;
      read_location += bytes_read;
      continue;
    }

    if (bytes_read == bytes_remaining) {
      return 0;
    }

    if (bytes_read > bytes_remaining) {
      //hlog("read more bytes than expected\n");
      return -1;
    }
  }
  return 0;
}

// Returns 0 on success
int
write_bytes(int fd, uint32_t bytes_to_write, void* buf)
{
  uint32_t bytes_remaining = bytes_to_write;
  char *write_location = (char *)buf;

  while (bytes_remaining > 0) {
    uint32_t bytes_written;

    bytes_written = write(fd, write_location, bytes_remaining);

    if (bytes_written < bytes_remaining) {
      bytes_remaining -= bytes_written;
      write_location += bytes_written;
      continue;
    }

    if (bytes_written == bytes_remaining) {
      return 0;
    }

    if (bytes_written > bytes_remaining) {
      //hlog("wrote more bytes than expected\n");
      return -1;
    }
  }
  return 0;
}

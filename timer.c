#include <stdint.h>

#if defined(MACOS)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreServices/CoreServices.h>
#endif

#if defined(LINUX)
#include <time.h>
#endif

#if defined(MACOS)
static mach_timebase_info_data_t mach_time_info;

void
timer_init(void)
{
  (void) mach_timebase_info(&mach_time_info);
}

uint64_t
tick(void)
{
  return mach_absolute_time();
}

uint64_t
tick_delta_to_nanoseconds(uint64_t delta)
{
  return delta * mach_time_info.numer / mach_time_info.denom;
}

#elif defined(LINUX)

void
timer_init(void)
{
}

uint64_t
tick(void)
{
  int rv;
  struct timespec tp = {};

  rv = clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
  if (rv == -1) {
    return 0;
  }

  return (tp.tv_sec * 1000000000) + tp.tv_nsec;
}

uint64_t
tick_delta_to_nanoseconds(uint64_t delta)
{
  return delta;
}

#endif

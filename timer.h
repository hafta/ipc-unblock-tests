#include <stdint.h>

void timer_init(void);
uint64_t tick(void);
uint64_t tick_delta_to_nanoseconds(uint64_t delta);

#include <time.h>

inline uint64_t get_time_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (((uint64_t) ts.tv_sec) * 1000000000) + ts.tv_nsec;
}

inline uint64_t get_us() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (((uint64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

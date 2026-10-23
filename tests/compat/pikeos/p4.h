#ifndef PIKEOS_P4_H
#define PIKEOS_P4_H 1

#include <stdint.h>

#define P4_H 1
#define P4_E_OK 0
#define P4_E_STATE (-1)
#define P4_THR_MYSELF ((P4_thr_t)0)
#define P4_NUM_PRIOS 256

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t P4_time_t;
typedef uint32_t P4_thr_t;
typedef int32_t P4_prio_t;
typedef int32_t P4_e_t;

// Standard PikeOS P4 API declaration:
// In SYSGO PikeOS microkernel, p4_get_time returns the current nanosecond
// timestamp directly.
P4_time_t p4_get_time(void);

// Test helper setters
void syscape_test_set_pikeos_time(P4_time_t time);
void syscape_test_reset_pikeos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // PIKEOS_P4_H

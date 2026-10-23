#ifndef PIKEOS_P4_H
#define PIKEOS_P4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t P4_time_t;
typedef uint32_t P4_task_t;
typedef int32_t P4_prio_t;
typedef uint32_t P4_cpu_t;
typedef int32_t P4_e_t;

#define P4_E_OK 0
#define P4_E_STATE (-1)

// PikeOS P4 API declarations
P4_e_t p4_get_time(P4_time_t* time);
P4_task_t p4_task_self(void);
P4_prio_t p4_task_get_prio(P4_task_t task);
P4_cpu_t p4_cpu_count(void);

// Test helper setters
void syscape_test_set_pikeos_time(P4_time_t time);
void syscape_test_set_pikeos_time_error(P4_e_t err);
void syscape_test_set_pikeos_task_null(int is_null);
void syscape_test_set_pikeos_task_prio(P4_prio_t prio);
void syscape_test_set_pikeos_cpu_count(P4_cpu_t count);
void syscape_test_reset_pikeos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // PIKEOS_P4_H

#ifndef OS_H
#define OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MYNEWT_VAL
#define MYNEWT_VAL(x) MYNEWT_VAL_##x
#endif

#ifndef MYNEWT_NO_VERSION_MACRO
#define MYNEWT_VERSION_STRING "1.10.0"
#define MYNEWT_VERSION_MAJOR 1
#define MYNEWT_VERSION_MINOR 10
#define MYNEWT_VERSION_REVISION 0
#endif

#ifndef MYNEWT_NO_TICKS_PER_SEC
#ifndef OS_TICKS_PER_SEC
#define OS_TICKS_PER_SEC 1000U
#endif
#ifndef MYNEWT_VAL_OS_TICKS_PER_SEC
#define MYNEWT_VAL_OS_TICKS_PER_SEC 1000U
#endif
#endif

#ifndef MYNEWT_VAL_OS_CORES
#define MYNEWT_VAL_OS_CORES 1U
#endif

#ifndef MYNEWT_TOTAL_HEAP_SIZE
#define MYNEWT_TOTAL_HEAP_SIZE 65536U
#endif

#ifndef MYNEWT_VAL_OS_HEAP_SIZE
#define MYNEWT_VAL_OS_HEAP_SIZE 65536U
#endif

typedef uint32_t os_time_t;

struct os_task {
    struct os_task* t_next;
    const char* t_name;
    uint8_t t_prio;
    uint8_t t_taskid;
    uint8_t t_state;
};

os_time_t os_time_get(void);
struct os_task* os_sched_get_current_task(void);
uint8_t os_task_count(void);

size_t mynewt_mock_get_free_heap(void);
#ifndef MYNEWT_FREE_HEAP_SIZE
#define MYNEWT_FREE_HEAP_SIZE mynewt_mock_get_free_heap()
#endif

void mynewt_mock_set_time(os_time_t t);
void mynewt_mock_set_task_prio(uint8_t prio);
void mynewt_mock_set_free_heap(size_t free_bytes);
void mynewt_mock_set_task_count(uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // OS_H

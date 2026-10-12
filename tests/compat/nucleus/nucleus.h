#ifndef NUCLEUS_H
#define NUCLEUS_H

#include <stddef.h>
#include <stdint.h>

#ifndef PLUS_VERSION_NUMBER
#define PLUS_VERSION_NUMBER 3
#endif

#ifndef NU_PLUS
#define NU_PLUS 1
#endif

#ifndef TICKS_PER_SECOND
#define TICKS_PER_SECOND 100U
#endif

#ifndef NU_SUCCESS
#define NU_SUCCESS 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NU_TASK_STRUCT {
    char tc_name[8];
    uint8_t tc_priority;
    uint32_t tc_id;
} NU_TASK;

// Official Nucleus PLUS C API declarations
char* NU_Release_Information(void);
uint32_t NU_Get_Time_Stamp(void);
uint32_t NU_Retrieve_Clock(void);
NU_TASK* NU_Current_Task_Pointer(void);
int NU_Task_Information(NU_TASK* task, char* name, uint8_t* status,
                        uint32_t* scheduled_count, uint8_t* priority,
                        void** stack_base, uint32_t* stack_size,
                        uint32_t* minimum_stack);
uint32_t NU_Established_Tasks(void);
uint32_t NU_Core_Count(void);

// Test helper setters
void syscape_test_set_nucleus_ticks(uint32_t ticks);
void syscape_test_set_nucleus_task_priority(uint8_t prio);
void syscape_test_set_nucleus_current_task_null(int is_null);
void syscape_test_set_nucleus_established_tasks(uint32_t count);
void syscape_test_set_nucleus_core_count(uint32_t count);
void syscape_test_set_nucleus_version_string(const char* ver);
void syscape_test_reset_nucleus_mock(void);

#ifdef __cplusplus
}
#endif

#endif // NUCLEUS_H

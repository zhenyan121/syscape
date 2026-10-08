#ifndef TI_SYSBIOS_KNL_TASK_H
#define TI_SYSBIOS_KNL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* Task_Handle;

Task_Handle Task_self(void);
int Task_getPri(Task_Handle task);

void syscape_test_set_ti_rtos_task_priority(int prio);
void syscape_test_set_ti_rtos_task_self_null(int is_null);
void syscape_test_reset_ti_rtos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // TI_SYSBIOS_KNL_TASK_H

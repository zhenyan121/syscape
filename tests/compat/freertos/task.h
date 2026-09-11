#ifndef INC_TASK_H
#define INC_TASK_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define tskKERNEL_VERSION_NUMBER "V10.5.1"
#define tskKERNEL_VERSION_MAJOR 10
#define tskKERNEL_VERSION_MINOR 5
#define tskKERNEL_VERSION_BUILD 1

TickType_t xTaskGetTickCount(void);
UBaseType_t uxTaskPriorityGet(TaskHandle_t xTaskToQuery);
UBaseType_t uxTaskGetNumberOfTasks(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

#include <stddef.h>
#include <stdint.h>
#include "FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef void* TaskHandle_t;

size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);

#ifdef __cplusplus
}
#endif

#endif

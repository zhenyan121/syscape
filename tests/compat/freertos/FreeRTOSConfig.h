#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define configTICK_RATE_HZ 1000
#define portTICK_PERIOD_MS (1000 / configTICK_RATE_HZ)
#define configTOTAL_HEAP_SIZE ((size_t)(64 * 1024))
#define configNUM_CORES 1

#endif

#ifndef KERNEL_DEFINES_H
#define KERNEL_DEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef RIOT_NO_VERSION_MACRO
#ifndef RIOT_VERSION
#define RIOT_VERSION "2026.01"
#endif

#ifndef RIOT_VERSION_STRING
#define RIOT_VERSION_STRING "2026.01"
#endif
#endif

#ifndef RIOT_CPU_COUNT
#define RIOT_CPU_COUNT 1U
#endif

#ifndef RIOT_TOTAL_HEAP_SIZE
#define RIOT_TOTAL_HEAP_SIZE 65536U
#endif

#ifndef RIOT_HEAP_SIZE
#define RIOT_HEAP_SIZE 65536U
#endif

#ifndef RIOT_FREE_HEAP_SIZE
#define RIOT_FREE_HEAP_SIZE 32768U
#endif

#ifdef __cplusplus
}
#endif

#endif // KERNEL_DEFINES_H

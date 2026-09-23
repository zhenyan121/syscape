#ifndef MALLOC_MONITOR_H
#define MALLOC_MONITOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t get_mem_usage(void);
void riot_mock_set_available_memory(size_t bytes);

#ifdef __cplusplus
}
#endif

#endif // MALLOC_MONITOR_H

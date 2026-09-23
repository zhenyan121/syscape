#ifndef CPU_H
#define CPU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t cpu_get_ram_size(void);
void riot_mock_set_ram_size(size_t bytes);

#ifdef __cplusplus
}
#endif

#endif // CPU_H

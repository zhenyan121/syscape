#ifndef XTIMER_H
#define XTIMER_H

#include "kernel_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t xtimer_now_usec(void);
uint64_t xtimer_now_usec64(void);

void riot_mock_set_xtimer_usec(uint64_t usec);

#ifdef __cplusplus
}
#endif

#endif // XTIMER_H

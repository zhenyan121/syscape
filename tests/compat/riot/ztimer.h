#ifndef ZTIMER_H
#define ZTIMER_H

#include "kernel_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ztimer_clock ztimer_clock_t;

extern ztimer_clock_t* const _ztimer_msec;
extern ztimer_clock_t* const _ztimer_usec;
extern ztimer_clock_t* const _ztimer_sec;

#ifndef ZTIMER_MSEC
#define ZTIMER_MSEC _ztimer_msec
#endif

#ifndef ZTIMER_USEC
#define ZTIMER_USEC _ztimer_usec
#endif

#ifndef ZTIMER_SEC
#define ZTIMER_SEC _ztimer_sec
#endif

uint32_t ztimer_now(ztimer_clock_t* clock);

void riot_mock_set_time_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // ZTIMER_H

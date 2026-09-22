#ifndef ZTIMER64_H
#define ZTIMER64_H

#include "ztimer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ztimer64_clock ztimer64_clock_t;

extern ztimer64_clock_t* const _ztimer64_msec;
extern ztimer64_clock_t* const _ztimer64_usec;
extern ztimer64_clock_t* const _ztimer64_sec;

#ifndef ZTIMER64_MSEC
#define ZTIMER64_MSEC _ztimer64_msec
#endif

#ifndef ZTIMER64_USEC
#define ZTIMER64_USEC _ztimer64_usec
#endif

#ifndef ZTIMER64_SEC
#define ZTIMER64_SEC _ztimer64_sec
#endif

uint64_t ztimer64_now(ztimer64_clock_t* clock);

void riot_mock_set_time64_ms(uint64_t ms);

#ifdef __cplusplus
}
#endif

#endif // ZTIMER64_H

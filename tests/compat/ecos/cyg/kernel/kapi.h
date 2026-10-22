#ifndef CYG_KERNEL_KAPI_H
#define CYG_KERNEL_KAPI_H

#include <cyg/infra/cyg_type.h>

#ifndef __ECOS__
#define __ECOS__ 1
#endif

#ifndef __ECOS
#define __ECOS 1
#endif

#ifndef CYGNUM_KERNEL_SCHED_PRIORITIES
#define CYGNUM_KERNEL_SCHED_PRIORITIES 32
#endif

#ifndef CYGNUM_HAL_RTC_NUMERATOR
#define CYGNUM_HAL_RTC_NUMERATOR 1000000000ULL
#endif

#ifndef CYGNUM_HAL_RTC_DENOMINATOR
#define CYGNUM_HAL_RTC_DENOMINATOR 100ULL
#endif

#ifndef CYGNUM_HAL_RTC_PERIOD
#define CYGNUM_HAL_RTC_PERIOD 10000ULL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Official eCos C API declarations
cyg_tick_count_t cyg_current_time(void);
cyg_handle_t cyg_thread_self(void);
cyg_priority_t cyg_thread_get_priority(cyg_handle_t thread);
cyg_priority_t cyg_thread_get_current_priority(cyg_handle_t thread);

// Test helper setters
void syscape_test_set_ecos_ticks(cyg_tick_count_t ticks);
void syscape_test_set_ecos_thread_priority(cyg_priority_t prio);
void syscape_test_set_ecos_thread_null(int is_null);
void syscape_test_reset_ecos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // CYG_KERNEL_KAPI_H

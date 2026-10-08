#ifndef TI_SYSBIOS_KNL_CLOCK_H
#define TI_SYSBIOS_KNL_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern uint32_t Clock_tickPeriod;

uint32_t Clock_getTicks(void);

void syscape_test_set_ti_rtos_ticks(uint32_t ticks);
void syscape_test_set_ti_rtos_tick_period(uint32_t period_us);

#ifdef __cplusplus
}
#endif

#endif // TI_SYSBIOS_KNL_CLOCK_H

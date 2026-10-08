#ifndef TI_SYSBIOS_HAL_CORE_H
#define TI_SYSBIOS_HAL_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern uint32_t ti_sysbios_hal_Core_numCores;

#ifndef Core_numCores
#define Core_numCores ti_sysbios_hal_Core_numCores
#endif

void syscape_test_set_ti_rtos_core_count(uint32_t count);
void syscape_test_reset_ti_rtos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // TI_SYSBIOS_HAL_CORE_H

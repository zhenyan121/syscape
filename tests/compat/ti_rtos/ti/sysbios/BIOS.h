#ifndef TI_SYSBIOS_BIOS_H
#define TI_SYSBIOS_BIOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef ti_sysbios_BIOS__
#define ti_sysbios_BIOS__ 1
#endif

#ifndef xdc_target__
#define xdc_target__ 1
#endif

#ifndef ti_sysbios_BIOS_version
#define ti_sysbios_BIOS_version 0x06830000U
#endif

#ifndef ti_sysbios_BIOS_versionStr
#define ti_sysbios_BIOS_versionStr "6.83.00.00"
#endif

#ifndef BIOS_version
#define BIOS_version ti_sysbios_BIOS_version
#endif

void syscape_test_reset_ti_rtos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // TI_SYSBIOS_BIOS_H

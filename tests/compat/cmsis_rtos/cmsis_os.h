#ifndef CMSIS_OS_H
#define CMSIS_OS_H

#ifndef osCMSIS_RTX
#define osCMSIS_RTX 1
#endif

#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef osKernelSysTickFrequency
#define osKernelSysTickFrequency 1000U
#endif

uint32_t osKernelSysTick(void);

#ifdef __cplusplus
}
#endif

#endif // CMSIS_OS_H

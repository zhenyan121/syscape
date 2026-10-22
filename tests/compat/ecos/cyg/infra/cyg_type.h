#ifndef CYG_INFRA_CYG_TYPE_H
#define CYG_INFRA_CYG_TYPE_H

#include <stddef.h>
#include <stdint.h>

#ifndef CYG_VERSION_INIT
#define CYG_VERSION_INIT 3
#endif

#ifndef CYG_VERSION_CURRENT
#define CYG_VERSION_CURRENT 30000
#endif

typedef uint64_t cyg_tick_count_t;
typedef uint32_t cyg_handle_t;
typedef uint32_t cyg_priority_t;
typedef uint32_t cyg_addrword_t;
typedef int cyg_bool_t;

#endif // CYG_INFRA_CYG_TYPE_H

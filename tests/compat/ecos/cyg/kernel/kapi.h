#ifndef CYG_KERNEL_KAPI_H
#define CYG_KERNEL_KAPI_H

#include <cyg/infra/cyg_type.h>

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
void syscape_test_set_ecos_thread_current_priority(cyg_priority_t prio);
void syscape_test_set_ecos_thread_null(int is_null);
void syscape_test_reset_ecos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // CYG_KERNEL_KAPI_H

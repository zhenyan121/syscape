#ifndef TESTS_COMPAT_RTTHREAD_RTDEF_H
#define TESTS_COMPAT_RTTHREAD_RTDEF_H

#include <stddef.h>
#include <stdint.h>

#if !defined(RT_VERSION_MAJOR)
#define RT_VERSION_MAJOR 5
#endif
#if !defined(RT_VERSION_MINOR)
#define RT_VERSION_MINOR 1
#endif
#if !defined(RT_VERSION_PATCH)
#define RT_VERSION_PATCH 0
#endif

#if !defined(RT_VERSION)
#define RT_VERSION 5
#endif
#if !defined(RT_SUBVERSION)
#define RT_SUBVERSION 1
#endif
#if !defined(RT_REVISION)
#define RT_REVISION 0
#endif

#if !defined(RT_TICK_PER_SECOND) && !defined(RT_NO_TICK_RATE)
#define RT_TICK_PER_SECOND 1000
#endif

#if !defined(RT_THREAD_PRIORITY_MAX)
#define RT_THREAD_PRIORITY_MAX 32
#endif

#if !defined(RT_USING_HEAP)
#define RT_USING_HEAP 1
#endif

typedef uint32_t rt_tick_t;
typedef size_t rt_size_t;
typedef uint8_t rt_uint8_t;
typedef uint32_t rt_uint32_t;

struct rt_thread {
    rt_uint8_t current_priority;
    rt_uint8_t init_priority;
    const char* name;
};

typedef struct rt_thread* rt_thread_t;

#endif

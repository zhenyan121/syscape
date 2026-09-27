#ifndef TESTS_COMPAT_RTTHREAD_RTDEF_H
#define TESTS_COMPAT_RTTHREAD_RTDEF_H

#include <stddef.h>
#include <stdint.h>

#if !defined(RT_LEGACY_VERSION)
#if !defined(RT_VERSION_MAJOR)
#define RT_VERSION_MAJOR 5
#endif
#if !defined(RT_VERSION_MINOR)
#define RT_VERSION_MINOR 1
#endif
#if !defined(RT_VERSION_PATCH)
#define RT_VERSION_PATCH 0
#endif
#endif

#if !defined(RT_VERSION)
#define RT_VERSION 4
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

#if !defined(RT_NO_HEAP)
#if !defined(RT_USING_HEAP)
#define RT_USING_HEAP 1
#endif
#endif

typedef uint32_t rt_tick_t;
typedef size_t rt_size_t;
typedef uint8_t rt_uint8_t;
typedef uint32_t rt_uint32_t;
typedef int32_t rt_int32_t;
typedef int16_t rt_int16_t;

enum rt_object_class_type {
    RT_Object_Class_Null = 0x00,
    RT_Object_Class_Thread = 0x01,
    RT_Object_Class_Semaphore = 0x02,
    RT_Object_Class_Mutex = 0x03,
    RT_Object_Class_Event = 0x04,
    RT_Object_Class_MailBox = 0x05,
    RT_Object_Class_MessageQueue = 0x06,
    RT_Object_Class_MemHeap = 0x07,
    RT_Object_Class_MemPool = 0x08,
    RT_Object_Class_Device = 0x09,
    RT_Object_Class_Timer = 0x0a,
    RT_Object_Class_Module = 0x0b,
    RT_Object_Class_Memory = 0x0c,
    RT_Object_Class_Channel = 0x0d,
    RT_Object_Class_Custom = 0x0e,
    RT_Object_Class_Unknown = 0x0f,
    RT_Object_Class_Static = 0x80
};

struct rt_thread {
    rt_uint8_t current_priority;
    rt_uint8_t init_priority;
    const char* name;
};

typedef struct rt_thread* rt_thread_t;

#endif

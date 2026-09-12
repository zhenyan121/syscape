#ifndef INTEGRITY_H
#define INTEGRITY_H

#ifdef __cplusplus
extern "C" {
#endif

#define INTEGRITY_MAJOR_VERSION 11
#define INTEGRITY_MINOR_VERSION 7
#define INTEGRITY_PATCH_VERSION 8
#define INTEGRITY_VERSION 1178U
#define INTEGRITY_NUM_PROCESSORS 4U
#define INTEGRITY_TOTAL_HEAP_SIZE 65536U
#define INTEGRITY_TASK_COUNT 5U

typedef int Error;
typedef unsigned long long Time;
typedef unsigned int ProcessorCount;
typedef unsigned int Task;
typedef unsigned int Priority;
typedef unsigned int MemorySize;

enum { Success = 0, Failure = 1 };

Error GetTime(Time* time);
Error GetProcessorCount(ProcessorCount* count);
Error CurrentTask(Task* task);
Error GetPriority(Task task, Priority* priority);
MemorySize GetMemoryPoolFreeSpace(void);

#ifdef __cplusplus
}
#endif

#endif

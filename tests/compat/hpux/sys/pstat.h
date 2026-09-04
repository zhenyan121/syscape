#ifndef _SYS_PSTAT_H
#define _SYS_PSTAT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SW_ENABLED 0x01
#define SW_BLOCK 0x02
#define SW_FS 0x04

#define CP_USER 0
#define CP_NICE 1
#define CP_SYS 2
#define CP_IDLE 3
#define CP_WAIT 4
#define CP_SSYS 5

#if defined(_ICOD_BASE_INFO)
#define PSP_SPU_ENABLED 1
#endif

#if defined(_PSTAT64)
typedef int64_t _T_LONG_T;
typedef uint64_t _T_ULONG_T;
#else
typedef long _T_LONG_T;
typedef unsigned long _T_ULONG_T;
#endif

struct pst_static {
    _T_LONG_T page_size;
    _T_LONG_T physical_memory;
    _T_LONG_T pst_maxmem;
    /* NOTE: HP-UX 11i struct pst_static does NOT have model member */
};

struct pst_dynamic {
    _T_LONG_T psd_free;
    _T_LONG_T psd_proc_cnt;
    _T_LONG_T psd_max_proc_cnt;
    _T_LONG_T psd_activeprocs;
    double psd_avg_1_min;
    double psd_avg_5_min;
    double psd_avg_15_min;
    _T_LONG_T psd_cpu_time[6];
    _T_LONG_T psd_mp_cpu_time[6];
};

struct pst_processor {
    _T_LONG_T psp_cpu_time[6];
    _T_LONG_T psp_iticksperclktick;
#if defined(_ICOD_BASE_INFO)
    int psp_processor_state;
#endif
    /* NOTE: HP-UX 11i struct pst_processor does NOT have psp_cpu_frequency */
};

struct pst_status {
    _T_LONG_T pst_pid;
    _T_LONG_T pst_ppid;
    _T_LONG_T pst_uid;
    _T_LONG_T pst_gid;
    char pst_cmd[64];
    char pst_ucomm[64];
    time_t pst_start;
    _T_LONG_T pst_rssize;
    _T_LONG_T pst_shmsize;
    _T_LONG_T pst_mmsize;
    _T_LONG_T pst_usize;
    _T_LONG_T pst_iosize;
    _T_LONG_T pst_vtsize;
    _T_LONG_T pst_vdsize;
    _T_LONG_T pst_vssize;
    _T_LONG_T pst_vshmsize;
    _T_LONG_T pst_vmmsize;
    _T_LONG_T pst_vusize;
    _T_LONG_T pst_viosize;
#if defined(__ia64) || defined(__ia64__)
    _T_LONG_T pst_vrsesize;
#endif
    _T_LONG_T pst_idx;
    /* NOTE: HP-UX 11i struct pst_status does NOT have pst_vsize or pst_nlwp */
};

struct pst_swapinfo {
    _T_ULONG_T pss_idx;
    unsigned int pss_flags;
    _T_ULONG_T pss_nblksenabled;
    _T_ULONG_T pss_nfpgs;
    _T_ULONG_T pss_limit;
    _T_ULONG_T pss_swapchunk;
    char pss_mntpt[256];
};

int pstat_getstatic(struct pst_static* buf, size_t elemsize, size_t elemcount,
                    int index);
int pstat_getdynamic(struct pst_dynamic* buf, size_t elemsize, size_t elemcount,
                     int index);
int pstat_getprocessor(struct pst_processor* buf, size_t elemsize,
                       size_t elemcount, int index);
int pstat_getproc(struct pst_status* buf, size_t elemsize, size_t elemcount,
                  int index);
int pstat_getswap(struct pst_swapinfo* buf, size_t elemsize, size_t elemcount,
                  int index);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PSTAT_H */

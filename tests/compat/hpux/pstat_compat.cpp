#include <sys/pstat.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern "C" {

bool syscape_mock_zero(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

int pstat_getstatic(struct pst_static* buf, size_t elemsize, size_t elemcount,
                    int /*index*/) {
    if (!buf || elemsize < sizeof(struct pst_static) || elemcount == 0) {
        errno = EINVAL;
        return -1;
    }
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_STATIC_ZERO")) {
        return 0;
    }
    std::memset(buf, 0, sizeof(struct pst_static));
    buf->page_size = 4096;
    buf->physical_memory = 1048576; // 4 GB (in 4KB pages)
    buf->pst_maxmem = 1048576;
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PHYSICAL_INVALID")) {
        buf->physical_memory = 0;
    }
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PAGE_SIZE_INVALID")) {
        buf->page_size = 0;
    }
    return 1;
}

int pstat_getdynamic(struct pst_dynamic* buf, size_t elemsize, size_t elemcount,
                     int /*index*/) {
    if (!buf || elemsize < sizeof(struct pst_dynamic) || elemcount == 0) {
        errno = EINVAL;
        return -1;
    }
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_DYNAMIC_ZERO")) {
        return 0;
    }
    std::memset(buf, 0, sizeof(struct pst_dynamic));
    buf->psd_free = 524288; // 2 GB free (in 4KB pages)
    buf->psd_proc_cnt = 4;  // 4 active processors
    buf->psd_max_proc_cnt = 8;
    buf->psd_activeprocs = 42; // 42 active processes in system table
    buf->psd_avg_1_min = 0.50;
    buf->psd_avg_5_min = 0.40;
    buf->psd_avg_15_min = 0.30;
    buf->psd_cpu_time[CP_USER] = 1000;
    buf->psd_cpu_time[CP_NICE] = 100;
    buf->psd_cpu_time[CP_SYS] = 500;
    buf->psd_cpu_time[CP_IDLE] = 8000;
    buf->psd_cpu_time[CP_WAIT] = 200;
    buf->psd_cpu_time[CP_SSYS] = 50;
    buf->psd_mp_cpu_time[CP_USER] = 1000;
    buf->psd_mp_cpu_time[CP_NICE] = 100;
    buf->psd_mp_cpu_time[CP_SYS] = 500;
    buf->psd_mp_cpu_time[CP_IDLE] = 8000;
    buf->psd_mp_cpu_time[CP_WAIT] = 200;
    buf->psd_mp_cpu_time[CP_SSYS] = 50;
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_DYNAMIC_INVALID")) {
        buf->psd_free = -1;
    }
    return 1;
}

int pstat_getprocessor(struct pst_processor* buf, size_t elemsize,
                       size_t elemcount, int /*index*/) {
    if (!buf || elemsize < sizeof(struct pst_processor) || elemcount == 0) {
        errno = EINVAL;
        return -1;
    }
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROCESSOR_ZERO")) {
        return 0;
    }
    for (size_t i = 0; i < elemcount; ++i) {
        std::memset(&buf[i], 0, sizeof(struct pst_processor));
        const bool disabled = elemcount > 1 && i == 0;
        buf[i].psp_cpu_time[CP_USER] = disabled ? 123456 : 500;
        buf[i].psp_cpu_time[CP_NICE] = disabled ? 654321 : 50;
        buf[i].psp_cpu_time[CP_SYS] = disabled ? 111111 : 250;
        buf[i].psp_cpu_time[CP_IDLE] = disabled ? 222222 : 4000;
        buf[i].psp_cpu_time[CP_WAIT] = disabled ? 333333 : 100;
        buf[i].psp_cpu_time[CP_SSYS] = disabled ? 444444 : 25;
        buf[i].psp_iticksperclktick = 1000;
#if defined(_ICOD_BASE_INFO)
        buf[i].psp_processor_state = disabled ? 0 : PSP_SPU_ENABLED;
#endif
    }
    return static_cast<int>(elemcount);
}

int pstat_getproc(struct pst_status* buf, size_t elemsize, size_t elemcount,
                  int index) {
    if (!buf || elemsize < sizeof(struct pst_status)) {
        errno = EINVAL;
        return -1;
    }
    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROC_ZERO")) {
        return 0;
    }
    // Single process targeted query (elemcount == 0, target PID in index)
    if (elemcount == 0) {
        if (index <= 0 ||
            (index != static_cast<int>(::getpid()) && index != 1)) {
            errno = ESRCH;
            return -1;
        }
        std::memset(buf, 0, sizeof(struct pst_status));
        buf->pst_pid = static_cast<_T_LONG_T>(index);
        buf->pst_ppid = 1;
        buf->pst_uid = 1000;
        buf->pst_gid = 1000;
        if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROC_ID_OVERFLOW")) {
            buf->pst_pid = static_cast<_T_LONG_T>(UINT64_C(4294967296)) + index;
        }
        buf->pst_rssize = 1024;
        buf->pst_shmsize = 128;
        buf->pst_mmsize = 64;
        buf->pst_usize = 4;
        buf->pst_iosize = 0;
        buf->pst_vtsize = 1024;
        buf->pst_vdsize = 1024;
        buf->pst_vssize = 512;
        buf->pst_vshmsize = 256;
        buf->pst_vmmsize = 128;
        buf->pst_vusize = 4;
        buf->pst_viosize = 0;
#if defined(__ia64) || defined(__ia64__)
        buf->pst_vrsesize = 0;
#endif
        buf->pst_start = 1700000000;
        buf->pst_idx = 0;
        std::strncpy(buf->pst_cmd, "/usr/bin/syscape_proc --test-flag",
                     sizeof(buf->pst_cmd) - 1);
        std::strncpy(buf->pst_ucomm, "syscape_proc",
                     sizeof(buf->pst_ucomm) - 1);
        return 1;
    }

    // Enumeration query (elemcount > 0, burst starting at index)
    if (index == 0) {
        const int count = (elemcount >= 2) ? 2 : 1;
        std::memset(buf, 0,
                    sizeof(struct pst_status) * static_cast<size_t>(count));

        buf[0].pst_pid = 0;
        buf[0].pst_ppid = 0;
        buf[0].pst_uid = 0;
        buf[0].pst_gid = 0;
        buf[0].pst_rssize = 512;
        buf[0].pst_shmsize = 64;
        buf[0].pst_mmsize = 32;
        buf[0].pst_usize = 4;
        buf[0].pst_iosize = 0;
        buf[0].pst_vtsize = 512;
        buf[0].pst_vdsize = 512;
        buf[0].pst_vssize = 256;
        buf[0].pst_vshmsize = 128;
        buf[0].pst_vmmsize = 64;
        buf[0].pst_vusize = 4;
        buf[0].pst_viosize = 0;
#if defined(__ia64) || defined(__ia64__)
        buf[0].pst_vrsesize = 0;
#endif
        buf[0].pst_start = 1700000000;
        buf[0].pst_idx = 0;
        std::strncpy(buf[0].pst_cmd, "swapper", sizeof(buf[0].pst_cmd) - 1);
        std::strncpy(buf[0].pst_ucomm, "swapper", sizeof(buf[0].pst_ucomm) - 1);

        if (count > 1) {
            buf[1].pst_pid = static_cast<_T_LONG_T>(::getpid());
            buf[1].pst_ppid = 1;
            buf[1].pst_uid = 1000;
            buf[1].pst_gid = 1000;
            buf[1].pst_rssize = 1024;
            buf[1].pst_shmsize = 128;
            buf[1].pst_mmsize = 64;
            buf[1].pst_usize = 4;
            buf[1].pst_iosize = 0;
            buf[1].pst_vtsize = 1024;
            buf[1].pst_vdsize = 1024;
            buf[1].pst_vssize = 512;
            buf[1].pst_vshmsize = 256;
            buf[1].pst_vmmsize = 128;
            buf[1].pst_vusize = 4;
            buf[1].pst_viosize = 0;
#if defined(__ia64) || defined(__ia64__)
            buf[1].pst_vrsesize = 0;
#endif
            buf[1].pst_start = 1700000000;
            buf[1].pst_idx = 1;
            std::strncpy(buf[1].pst_cmd, "/usr/bin/syscape_proc --test-flag",
                         sizeof(buf[1].pst_cmd) - 1);
            std::strncpy(buf[1].pst_ucomm, "syscape_proc",
                         sizeof(buf[1].pst_ucomm) - 1);
        }
        if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROC_ID_OVERFLOW")) {
            buf[0].pst_uid = static_cast<_T_LONG_T>(UINT64_C(4294967296));
        }
        if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROC_ID_NEGATIVE")) {
            buf[0].pst_gid = -1;
        }
        return count;
    }

    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_PROC_BACKWARD")) {
        std::memset(buf, 0, sizeof(struct pst_status));
        buf[0].pst_pid = 1;
        buf[0].pst_uid = 0;
        buf[0].pst_gid = 0;
        buf[0].pst_idx = static_cast<_T_LONG_T>(index - 1);
        return 1;
    }

    if (index == 1) {
        std::memset(buf, 0, sizeof(struct pst_status));
        buf[0].pst_pid = static_cast<_T_LONG_T>(::getpid());
        buf[0].pst_ppid = 1;
        buf[0].pst_uid = 1000;
        buf[0].pst_gid = 1000;
        buf[0].pst_rssize = 1024;
        buf[0].pst_shmsize = 128;
        buf[0].pst_mmsize = 64;
        buf[0].pst_usize = 4;
        buf[0].pst_iosize = 0;
        buf[0].pst_vtsize = 1024;
        buf[0].pst_vdsize = 1024;
        buf[0].pst_vssize = 512;
        buf[0].pst_vshmsize = 256;
        buf[0].pst_vmmsize = 128;
        buf[0].pst_vusize = 4;
        buf[0].pst_viosize = 0;
#if defined(__ia64) || defined(__ia64__)
        buf[0].pst_vrsesize = 0;
#endif
        buf[0].pst_start = 1700000000;
        buf[0].pst_idx = 1;
        std::strncpy(buf[0].pst_cmd, "/usr/bin/syscape_proc --test-flag",
                     sizeof(buf[0].pst_cmd) - 1);
        std::strncpy(buf[0].pst_ucomm, "syscape_proc",
                     sizeof(buf[0].pst_ucomm) - 1);
        return 1;
    }

    return 0;
}

int pstat_getswap(struct pst_swapinfo* buf, size_t elemsize, size_t elemcount,
                  int index) {
    if (!buf || elemsize < sizeof(struct pst_swapinfo) || elemcount == 0) {
        errno = EINVAL;
        return -1;
    }
    if (index == 0) {
        const int count = (elemcount >= 2) ? 2 : 1;
        std::memset(buf, 0,
                    sizeof(struct pst_swapinfo) * static_cast<size_t>(count));

        // Device 0: Block swap (1 GB = 1048576 1KB blocks, 512 MB free =
        // 131072 4KB pages).
        buf[0].pss_idx = 0;
        buf[0].pss_flags = SW_ENABLED | SW_BLOCK;
        buf[0].pss_nblksenabled = 1048576;
        buf[0].pss_nfpgs = 131072;
        std::strncpy(buf[0].pss_mntpt, "/dev/vg00/lvol2",
                     sizeof(buf[0].pss_mntpt) - 1);

        if (count > 1) {
            // Device 1: Filesystem swap (SW_FS) (2 GB = 1024 limit * 2048 KB
            // swapchunk * 1024 B, 1 GB free = 262144 4KB pages)
            buf[1].pss_idx = 1;
            buf[1].pss_flags = SW_ENABLED | SW_FS;
            buf[1].pss_limit = 1024;
            buf[1].pss_swapchunk = 2048;
            buf[1].pss_nfpgs = 262144;
            std::strncpy(buf[1].pss_mntpt, "/var/paging",
                         sizeof(buf[1].pss_mntpt) - 1);
        }
        return count;
    }

    if (syscape_mock_zero("SYSCAPE_TEST_PSTAT_SWAP_BACKWARD")) {
        std::memset(buf, 0, sizeof(struct pst_swapinfo));
        buf[0].pss_idx = static_cast<_T_ULONG_T>(index - 1);
        buf[0].pss_flags = SW_ENABLED | SW_BLOCK;
        return 1;
    }

    if (index == 1) {
        std::memset(buf, 0, sizeof(struct pst_swapinfo));
        buf[0].pss_idx = 1;
        buf[0].pss_flags = SW_ENABLED | SW_FS;
        buf[0].pss_limit = 1024;
        buf[0].pss_swapchunk = 2048;
        buf[0].pss_nfpgs = 262144;
        std::strncpy(buf[0].pss_mntpt, "/var/paging",
                     sizeof(buf[0].pss_mntpt) - 1);
        return 1;
    }

    return 0;
}

} // extern "C"

#include "tk/tkernel.h"

namespace {

SYSTIM s_mock_tim = {0, 42000}; // 42,000 ms
T_RVER s_mock_ver = {
    0x0001,       // maker
    0x0001,       // prid
    (3 << 8) | 0, // prver: 3.0
    {1, 0, 0, 0}  // prno: patch 1
};
T_RTSK s_mock_rtsk = {
    12, // tskpri
    12, // tskbpri
    2   // tskstat (TTS_RUN)
};
T_RMPL s_mock_rmpl = {
    0,     // wtskid
    32768, // frsz: 32 KiB total free memory
    16384  // maxsz: 16 KiB largest contiguous free block (maxsz <= frsz)
};

} // namespace

extern "C" {

ER tk_get_tim(SYSTIM* pk_tim) {
    if (pk_tim != nullptr) {
        *pk_tim = s_mock_tim;
        return E_OK;
    }
    return E_SYS;
}

ER tk_ref_ver(T_RVER* pk_rver) {
    if (pk_rver != nullptr) {
        *pk_rver = s_mock_ver;
        return E_OK;
    }
    return E_SYS;
}

ER tk_ref_tsk(ID tskid, T_RTSK* pk_rtsk) {
    if (tskid == TSK_SELF && pk_rtsk != nullptr) {
        *pk_rtsk = s_mock_rtsk;
        return E_OK;
    }
    return E_SYS;
}

ER tk_ref_mpl(ID mplid, T_RMPL* pk_rmpl) {
    if ((mplid == MPL_SELF || mplid == 1) && pk_rmpl != nullptr) {
        *pk_rmpl = s_mock_rmpl;
        return E_OK;
    }
    return E_SYS;
}

} // extern "C"

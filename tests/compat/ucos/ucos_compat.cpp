#include "os.h"

namespace {

OS_TCB s_mock_tcb = {12, 12};
OS_TICK s_mock_ticks = 42000;
CPU_INT32U s_mock_free_heap = 32768U;

} // namespace

extern "C" {

OS_TCB* OSTCBCurPtr = &s_mock_tcb;
OS_TCB* OSTCBCur = &s_mock_tcb;
OS_OBJ_QTY OSTaskQty = 6;
CPU_INT08U OSTaskCtr = 6;

CPU_INT16U OSVersion(OS_ERR* p_err) {
    if (p_err != nullptr) {
        *p_err = 0;
    }
    return OS_VERSION;
}

OS_TICK OSTimeGet(OS_ERR* p_err) {
    if (p_err != nullptr) {
        *p_err = 0;
    }
    return s_mock_ticks;
}

CPU_INT32U OS_GetFreeHeapSpace(void) {
    return s_mock_free_heap;
}

} // extern "C"

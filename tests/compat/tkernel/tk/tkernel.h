#ifndef TK_TKERNEL_H
#define TK_TKERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TK_NO_VERSION_MACRO
#define TK_VERSION 3001U
#endif
#define TK_NUM_CORES 4U
#define TK_TOTAL_HEAP_SIZE 65536U
#define TK_TASK_COUNT 5U

#define TSK_SELF 0
#define MPL_SELF 1

typedef int ID;
typedef int PRI;
typedef int ER;
typedef unsigned int UINT;
typedef unsigned int SZ;
typedef unsigned short UH;
typedef unsigned int UW;
typedef int W;

typedef struct {
    W hi;
    UW lo;
} SYSTIM;

typedef struct t_rver {
    UH maker;
    UH prid;
    UH prver;
    UH prno[4];
} T_RVER;

typedef struct t_rtsk {
    PRI tskpri;
    PRI tskbpri;
    UINT tskstat;
} T_RTSK;

typedef struct t_rmpl {
    ID wtskid;
    SZ frsz;
    SZ maxsz;
} T_RMPL;

enum { E_OK = 0, E_SYS = -5, E_NOEXS = -52 };

ER tk_get_tim(SYSTIM* pk_tim);
ER tk_ref_ver(T_RVER* pk_rver);
ER tk_ref_tsk(ID tskid, T_RTSK* pk_rtsk);
ER tk_ref_mpl(ID mplid, T_RMPL* pk_rmpl);

#ifdef __cplusplus
}
#endif

#endif

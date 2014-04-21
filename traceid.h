//
// traceid.h
// 20-Feb-99
//
// trace on section done only if #define TRACE_nametotrace is defined
// can just do individual tracePeft() calls, too (but still need TRACE_WANTED defined)

#define TRACE_WANTED

// no point defining any of these if trace isn't wanted

#ifdef TRACE_WANTED

 #define TRACE_IN             0
 #define TRACE_OUT       0x8000

#define TRACE_CALIBRATE         "calibrate"
 #define TRACE_CALIBRATE_IN      1 | TRACE_IN
 #define TRACE_CALIBRATE_OUT     1 | TRACE_OUT

#define TRACE_IRQ               "irq"
 #define TRACE_IRQ_IN            64 | TRACE_IN
 #define TRACE_IRQ_OUT           64 | TRACE_OUT

 //#define TRACE_PROCESS           "process"
 #define TRACE_PROCESS_IN        65 | TRACE_IN
 #define TRACE_PROCESS_OUT       65 | TRACE_OUT

 //#define TRACE_RETBUFFER         "retbuffer"
 #define TRACE_RETBUFFER_IN      66 | TRACE_IN
 #define TRACE_RETBUFFER_OUT     66 | TRACE_OUT

 //#define TRACE_FILLAUDIOBUFFER   "fillaudiobuffer"
 #define TRACE_FILLAUDIOBUFFER_IN  67 | 0
 #define TRACE_FILLAUDIOBUFFER_OUT 67 | 0x8000

#define TRACE_IDC_DDCMD         "idc_cmd"
 #define TRACE_IDC_DDCMD_IN      68 | 0
 #define TRACE_IDC_DDCMD_OUT     68 | 0x8000

#define TRACE_STRAT             "strat"
 #define TRACE_STRAT_IN          69 | 0
 #define TRACE_STRAT_OUT         69 | 0x8000

#define TRACE_STRAT_GENIOCTL    "stratgenioctl"
 // no IN since uses cat/func codes only (may get non-audio cat!)
 // no OUT since uses cat | TRACE_OUT/func codes only

#endif


//
// strat.c
// 25-Jan-99
//
// VOID __far stratMode2(REQPACK __far *rpPtr);


#include "cs40.h"

static VOID stratInitComplete(REQPACK __far *rpPtr);
static VOID stratOpen(REQPACK __far *rpPtr);
static VOID stratClose(REQPACK __far *rpPtr);
static VOID stratDeinstall(REQPACK __far *rpPtr);

static USHORT P5 = 0;          // updated at strat entry (once, set to 1 if can RDTSC)
static USHORT R3 = 3;          // updated at strat entry (usually once, set to 0 if ring0)

#ifdef TRACE_WANTED // ===================================================

 static ULONG calibrate = -1;
 static USHORT trace = 1;       // true if should call to trace support


 // ----------------------
 // in: minor event number
 //out: tPeft.rdtsc.lo (for calibrartion use, right at startup)
 //nts: needs P5 to get RDTSC, else uses timer0
 //     if P5 .lo always has bit0=1
 //     if not P5 .lo always has bit0=0
 //
 //     code being traced is surrounded by
 //
 //       #ifdef TRACE_XXX
 //        tracePerf(TRACE_XXX, _IF());
 //       #endif
 //
 //     where this is defined in my_h\os2\traceid.h

 ULONG tracePerf(USHORT minor, ULONG data) {

  USHORT rc = 7734;

  typedef struct _TPERF {
   ULONG count;
   RDTSC_COUNT rdtsc;
   ULONG data;
  } TPERF;

  static TPERF tPerf = {0,0};

  if (trace == 0) return 0;

  P5 = 0;  // just simpler to compare with non-p5 times on mytrace.exe reports

  tPerf.count++;
  tPerf.data = data;

  if (P5) {
     rdtsc(&tPerf.rdtsc);
     tPerf.rdtsc.lo = tPerf.rdtsc.lo | 1;     // bit0=1 always if P5
  }
  else {
     // 838.1  ns per tick
     // 0.8381 us per tick
     // 50 ticks = 50 x 0.8381 us = 41.91 us

     tPerf.rdtsc.lo = (ULONG)ReadTimer0();
     tPerf.rdtsc.lo = tPerf.rdtsc.lo & ~1;    // bit0=0 always if not P5
  }

  if (R3) {
     rc = DosSysTrace(255,sizeof(TPERF),minor,(PCHAR)&tPerf);
  }
  else {
     rc = DevHelp_RAS(255,minor,sizeof(TPERF),(PSZ)&tPerf);
  }

  return tPerf.rdtsc.lo;  // used only just after boot so will always be still in lo, or if not,
 }                        // hopefully won't be unlucky enough to straddle lo/hi in split second


 // ---------------------
 // in: n/a
 //out: n/a
 //nts: called at strat init complete (and play start when debugging)

 VOID traceCalibrate(VOID) {

  ULONG ti, to;

  _cli_();
  ti = tracePerf(TRACE_CALIBRATE_IN,0);   // results in 50-100 ticks on box2 (timer0)
  to = tracePerf(TRACE_CALIBRATE_OUT,0);  // ring0
  _sti_();

  if (P5) {
     if (to > ti) {
        calibrate = to - ti;    // usual, means did not wrap to rdtsc.hi
     }
     else {
        calibrate = (0xFFFFFFFF - ti) + to + 1; // wrapped, so take pre-wrap and add to post wrap
     }
  }
  else {
     if (ti > to) {          // expected for timer0, unless rollunder (can't be expected to be =)
        calibrate = ti - to; // ti=40000  to=39950  calibrate=50
     }
     else {                  // rollunder (ti=25  to=65530  calibrate=50)
        calibrate = ti + (65536-to);
     }
  }

  return;
 }

#endif  // #ifdef TRACE_WANTED ===================================================


// -------------------------------------
// in:
//out:
//nts: have to remember to think __far for all pointers
//
//     be aware that rpPtr and es:bx contents probably aren't the same, even if not calling out
//     this because compiler sets a local rpPtr (pointer) to es:bx on entry, then uses local ptr
//
//     also be aware that watcom __saveregs doesn't do what it says it does (!) so be sure to
//     look at listing file if __saveregs is used (also check how __interrupt looks)

// need to __loadds?

VOID __far stratMode2(REQPACK __far *rpPtr) {
#pragma aux stratMode2 parm [es bx];

// int3();
// ddprintf("@stratMode2, es:bx=%p\n",rpPtr);

 if (R3) {
    R3 = IsRing3();                  // inits in ring3, once gets to 0 always 0, even if multi hdrs
    if (CanCPUID()) P5 = CanRDTSC(); // also only need to do this once (assumes not a BASEDEV)
 }

#ifdef TRACE_STRAT
 tracePerf(TRACE_STRAT_IN, rpPtr->command);
#endif

 rpPtr->status = RPDONE;

 switch (rpPtr->command) {
 case STRATEGY_INIT:
    stratMode2Init(rpPtr);
    break;
 case STRATEGY_OPEN:
    stratOpen(rpPtr);
    break;
 case STRATEGY_CLOSE:
    stratClose(rpPtr);
    break;
 case STRATEGY_GENIOCTL:
    ioctlStrat(rpPtr, 0);
    break;
 case STRATEGY_DEINSTALL:
    stratDeinstall(rpPtr);
    break;
 case STRATEGY_INITCOMPLETE:
    stratInitComplete(rpPtr);
    break;
 default:
    rpPtr->status = RPDONE | RPERR | RPGENFAIL;
 }

#ifdef TRACE_STRAT
 tracePerf(TRACE_STRAT_OUT, rpPtr->status);
#endif

 return;
}

// ---------
// in:
//out:
//nts: resets timer0 to max, and count-by-1 (mode2), so only do once, and then really only when testing
//     does a trace calibrate (always), also done at play start
//     seems like clock01.sys already sets to mode2, so can probably skip (am)

static VOID stratInitComplete(REQPACK __far *rpPtr) {

#ifdef TRACE_CALIBRATE
 // ResetTimer0();
 traceCalibrate();
#endif

 return;
 rpPtr;
}


// ---------
// in:
//out:
//nts: resources not allocated until ioctlAudioInit()

static VOID stratOpen(REQPACK __far *rpPtr) {

 // check if VDD has hardware and if so return busy, as in:
 // rpPtr->status = RPDONE | RPERR | RPBUSY;
 // if not, then inc inUseCounter to prevent a VDD from getting it

 return;
 rpPtr;
}


// ---------
// in:
//out:
//nts: may want to verify pid?

extern void dumpheap(void);

static VOID stratClose(REQPACK __far *rpPtr) {

 STREAM *streamPtr;

 streamPtr = streamFindStreamSFN(rpPtr->open.SFN);
 if (streamPtr) {
    wavestreamDeinit(streamPtr->wsParentPtr);  // this frees streamPtr and wsPtr (wsParentPtr)

    // dec inUseCounter
 }

// !!!
// dump heap

// dumpheap();

 return;
 rpPtr;
}


// ---------
// in:
//out:
//nts:

static VOID stratDeinstall(REQPACK __far *rpPtr) {

 // org code:
 //  while (pAudioHWList->IsElements())
 //     pAudioHWList->DestroyElement(pAudioHWList->PopHead());

 return;
 rpPtr;
}



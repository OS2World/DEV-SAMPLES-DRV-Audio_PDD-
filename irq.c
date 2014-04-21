//
// irq.c
// 30-Jan-99
//
// VOID __far __loadds irqHandler(VOID);
// USHORT irqEnable(USHORT irqNo);
// USHORT irqDisable(USHORT irqNo);

#include "cs40.h"

static USHORT irqEnabled = 0;   // count enables, grab IRQ when 0->1, release IRQ when back to 0
static USHORT irqLevel = 0;

// -----------
// in: n/a
//out: clc=okay, stc=bad
//nts: gets called by kernel to handle interrupt (interrupt context)
//
//     A new logical device might post an interrupt request while we are
//     in service (such as a CAPTURE that is running along with a PLAY).
//
//     OS/2 2.1 and later save all regs (so documented, not 100% sure on extended regs (eax))
//     interrupts are disabled on entry since not a shared IRQ
//     probably should not be using cli/sti here, and

VOID __far __loadds irqHandler(VOID) {

 WAVESTREAM *wsPtr;
 STREAM *streamPtr;
 USHORT times;

// !!! ---------
//static USHORT cnt = 0;
// ddprintf("at IRQ, cnt=%u\n",cnt);


 sti();         // sure, let higher-priority interrupts through

//cnt++;
//if (cnt > 99) {
//   if ((cnt % 100) == 0) ddprintf("at IRQ, cnt=%u\n",cnt);
//   if (cnt > 1023) cnt = 0;
//   chipsetIntReset(AUDIOHW_WAVE_PLAY);
//   cli();
//   DevHelp_EOI(irqLevel);
//   clc();
//   return;
//}

#ifdef TRACE_IRQ
 tracePerf(TRACE_IRQ_IN,_IF());
#endif

 for (times = 0; times < irqEnabled; times++) {

    if (chipsetIntPending(AUDIOHW_WAVE_PLAY)) {
       streamPtr = streamFindActive(STREAM_WAVE_PLAY);
       if (streamPtr) {
          wsPtr = streamPtr->wsParentPtr;

#ifdef TRACE_PROCESS
 tracePerf(TRACE_PROCESS_IN,_IF());
#endif

          wavestreamProcess(wsPtr);

#ifdef TRACE_PROCESS
 tracePerf(TRACE_PROCESS_OUT,_IF());
#endif

       }
       chipsetIntReset(AUDIOHW_WAVE_PLAY);
    }

    if (chipsetIntPending(AUDIOHW_WAVE_CAPTURE)) {
//ddprintf("at IRQ, capture, cnt=%u\n",cnt++);
       streamPtr = streamFindActive(STREAM_WAVE_CAPTURE);
       if (streamPtr) {
          wsPtr = streamPtr->wsParentPtr;
          wavestreamProcess(wsPtr);
       }
       chipsetIntReset(AUDIOHW_WAVE_CAPTURE);
    }

    if (chipsetIntPending(AUDIOHW_TIMER)) {
//ddprintf("at IRQ, timer, cnt=%u\n",cnt++);
       chipsetIntReset(AUDIOHW_TIMER);
    }


    // check if anymore

    if (chipsetIntPending(-1) == 0) {

#ifdef TRACE_IRQ
 tracePerf(TRACE_IRQ_OUT,_IF());
#endif

       cli();
       DevHelp_EOI(irqLevel);
       clc();
       return;
    }

 }
//ddprintf("at IRQ, stc()\n");

#ifdef TRACE_IRQ
 tracePerf(TRACE_IRQ_OUT,0x9999);
#endif

 stc();

 return;        // back to kernel with bad news
}


// ---------
// in: irqNo
//out: 0=okay 1=fail (or other rc error)
//nts: if 0->1 grab IRQ
//     ds must be set correctly since checked by kernel when unsetting
//     if irq already set, inc count (error if irqNo differs from irqLevel)

USHORT irqEnable(USHORT irqNo) {

 USHORT rc = 0;

 if (irqEnabled && (irqNo != irqLevel)) return 1;

 if (irqEnabled == 0) {
    irqLevel = irqNo;
    rc = DevHelp_SetIRQ((NPFN)irqHandler, irqLevel, 0); // rc =1 if already owned

// !!!
//ddprintf("@SetIRQ (#%u), rc=%u\n",irqNo,rc);

 }
 if (rc == 0) irqEnabled++;

 return rc;
}


// ---------
// in: irqNo
//out: 0=okay, 1=errro
//nts: if 1->0 release IRQ
//     ds must be set to owner

USHORT irqDisable(USHORT irqNo) {

 USHORT rc = 0;

 if (irqEnabled == 0) return 1;
 if (irqNo != irqLevel) return 1;
 if (irqEnabled == 1) {
    rc = DevHelp_UnSetIRQ(irqNo);  // rc=1 if not owner of IRQ based on DS

// !!!
//ddprintf("@UnSetIRQ (#%u), rc=%u\n",irqNo,rc);

 }
 if (rc == 0) irqEnabled--;

 return 0;
}



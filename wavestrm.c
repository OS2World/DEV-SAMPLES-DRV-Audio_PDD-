//
// wavestrm.c
// 27-Jan-99
//
// 19-Feb-99 removed bogus reference/use of controlPtr->
//
// VOID    wavestreamProcess(WAVESTREAM *wsPtr);
// ULONG   wavestreamGetCurrentTime(WAVESTREAM *wsPtr);
// VOID    wavestreamSetCurrentTime(ULONG time, WAVESTREAM *wsPtr);
// USHORT  wavestreamStart(WAVESTREAM *wsPtr);
// USHORT  wavestreamStop(WAVESTREAM *wsPtr);
// USHORT  wavestreamPause(WAVESTREAM *wsPtr);
// USHORT  wavestreamResume(WAVESTREAM *wsPtr);
// STREAM *wavestreamInit(USHORT streamType, MCI_AUDIO_INIT __far *mciInitPtr, WAVESTREAM *wsPtr);
// USHORT  wavestreamDeinit(WAVESTREAM *wsPtr);

#include "cs40.h"

static USHORT RealignBuffer(ULONG endPos, STREAM_BUFFER *sbPtr);
static VOID RealignPausedBuffers(WAVESTREAM *wsPtr);
static VOID ResetAudioBuffer(WAVESTREAM *wsPtr);
static VOID FillAudioBuffer(WAVESTREAM *wsPtr);
static ULONG WriteAudioBuffer(WAVESTREAM *wsPtr);
static ULONG ReadAudioBuffer(WAVESTREAM *wsPtr);

// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: n/a
//nts: process irq
//     called at interrupt time from the hardware objects' handler (waveplay handler, etc.)
//     if buffers on proc queue get/put them ...
//     (was Process)
//     original notes follow (there was no _vUpdateProcessed() routine):
//     --first call _vUpdateProcessed() to update the dma amd audio buffer related
//     --stuff. Next if we have buffers on the primary queue try to read/write them
//     --to the audiobuffer. Look at the buffers on the done queue and see if they
//     --can be returned and finally process any events pending.

VOID wavestreamProcess(WAVESTREAM *wsPtr) {

 STREAM_BUFFER *sbPtr;
 STREAM *streamPtr = wsPtr->streamPtr;
 USHORT streamMode = wsPtr->streamPtr->streamType & STREAM_RW_MODE;

 wsPtr->bytesProcessed = abUpdate(1, &wsPtr->waPtr->ab);  // get 'final' stream byte consumed/produced count

 if (streamMode == STREAM_WRITE) {           // playback

// even undoing this only brought it down to 58%!
// so pretty-much rem'ed out all of the irq (just resets int and bye!)...it runs at 58%
// don't know what the hell's going on

    if (sbNotEmpty(&streamPtr->sbaProc)) {   // if buffer in proc queue...
       FillAudioBuffer(wsPtr);               // keep audio buffer full
    }

    // if there are buffers that have been completely written to the audio buffer
    // check the first one on the done queue to see if the hardware has consumed it
    // and if so return it

// rem'ing out the return-buffer stuff below only went from 66% down to 60% or so...hm
// now checking the FillAudioBuffer section above (rem'ing it out--will get back tomorrow
// in any case) ... if that's not it, have to check ssm_idc stuff next


// should see about doing this out of interrupt context, via arm context hook (18-Feb-99)
// since it calls back to SHDD and ints are enabled there

    if (sbNotEmpty(&streamPtr->sbaDone)) {   
       sbPtr = sbHead(&streamPtr->sbaDone);
       if ((wsPtr->bytesProcessed + wsPtr->waveConfig.bytesPerIRQ) >= sbPtr->bufferDonePos) {
          streamReturnBuffer(streamPtr);
       }
    }

 }
 else { // STREAM_READ capture
    ReadAudioBuffer(wsPtr);
    while(sbNotEmpty(&streamPtr->sbaDone)) {
       streamReturnBuffer(streamPtr);
    }
 }

// streamProcessEvents(streamPtr); // stub routine for now

 return;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: current time
//nts: this calcs stream time in milliseconds based on...(?)
//     -- time = bytes consumed / consume rate
//
//     IMPORTANT: often the use of this data is to not be returned directly, but
//     a pointer to a variable that has its value (eg, wsPtr->currTime, or wsPtr->timeBase
//     for control stop/pause) is returned instead (all DDCMD calls, but not SHD returns)
//     -- if instead return the value directly all sorts of crap (silently) happens (19-Feb-99)

ULONG wavestreamGetCurrentTime(WAVESTREAM *wsPtr) {

 ULONG msecs = 0, processed;
 USHORT flag = 0;
 STREAM *streamPtr = wsPtr->streamPtr;

 if (streamPtr->streamState == STREAM_STREAMING) flag = 1; // check w/hardware only if running
 processed = abUpdate(flag, &wsPtr->waPtr->ab);

 if (processed) {
    ULONG c_rate = wsPtr->waveConfig.consumeRate;
    ULONG secs = processed / c_rate;
    ULONG ov =   processed - (secs * c_rate);
    msecs = ((ov * 1000) / c_rate) + (secs * 1000);
 }

 return msecs + wsPtr->timeBase;  // if nothing yet processed, effectively returns just time base
}


// -----------------------------------
// in: time = ms time to set time base
//     wsPtr -> WAVESTREAM structure
//out: n/a
//nts: mmpm/2 sends stream starting time (may not be 0) so use this as the time base

VOID wavestreamSetCurrentTime(ULONG time, WAVESTREAM *wsPtr) {

 wsPtr->timeBase = time;

 return;
}


// -----------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0
//nts:

USHORT wavestreamStart(WAVESTREAM *wsPtr) {

 USHORT rc = 0;

 // waveConfig.bitsPerSample, channels, sampleRate, and dataType, must be setup
 // (which is done in wavestreamInit()) before calling waConfigDev()

 waConfigDev(wsPtr);
 ResetAudioBuffer(wsPtr);

 if (wsPtr->waPtr->devType == AUDIOHW_WAVE_PLAY) {
    rc = waveplayStart(wsPtr);
 }
 else {
    rc = waverecStart(wsPtr);
 }

 if (rc) {
    rc = ERROR_START_STREAM;
 }
 else {
    wsPtr->streamPtr->streamState = STREAM_STREAMING;
 }

 return rc;
}


// -----------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0
//nts:

USHORT wavestreamStop(WAVESTREAM *wsPtr) {

 USHORT rc = 0;

 if (wsPtr->waPtr->devType == AUDIOHW_WAVE_PLAY) {
    rc = waveplayStop(wsPtr);
 }
 else {
    rc = waverecStop(wsPtr);
 }

 streamReturnBuffers(wsPtr->streamPtr);
 wsPtr->streamPtr->streamState = STREAM_STOPPED;

 wsPtr->timeBase = wavestreamGetCurrentTime(wsPtr); // local

 abReset(wsPtr->streamPtr->streamType & STREAM_RW_MODE, &wsPtr->waPtr->ab);

 return rc;
}


// -----------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0
//nts:

USHORT wavestreamPause(WAVESTREAM *wsPtr) {

 USHORT rc = 0;

 if (wsPtr->waPtr->devType == AUDIOHW_WAVE_PLAY) {
    rc = waveplayStop(wsPtr);
 }
 else {
    rc = waverecStop(wsPtr);
 }

 wsPtr->streamPtr->streamState = STREAM_PAUSED;
 RealignPausedBuffers(wsPtr);

 wsPtr->timeBase = wavestreamGetCurrentTime(wsPtr); // local

 abReset(wsPtr->streamPtr->streamType & STREAM_RW_MODE, &wsPtr->waPtr->ab);

// !!!
// testing new dma stuff (can just change logical dma buffer size at will)
//
//if (wsPtr->audioBufferSize == 0) {
//   wsPtr->audioBufferSize = 8192;
//}

 return rc;
}


// -----------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0
//nts:

USHORT wavestreamResume(WAVESTREAM *wsPtr) {

 USHORT rc;

 // since the resume code is exactly the same as the start, just call start

 rc = wavestreamStart(wsPtr);  // this gets all of start inlined...

 return rc;
}


// -----------------------------------
// in: streamType = AUDIOHW_WAVE_PLAY or _CAPTURE
//     wsPtr -> WAVESTREAM structure, already allocated
//out: n/a
//nts:
//     this called by "void IoctlAudioInit(PREQPACKET prp, USHORT LDev)"
//     and should (I think) return a (STREAM *), which means the last
//     thing this should do is call streamInit(...)

STREAM *wavestreamInit(USHORT streamType, MCI_AUDIO_INIT __far *mciInitPtr, WAVESTREAM *wsPtr) {

 USHORT rc = 0;
 STREAM *tstreamPtr = malloc(sizeof(STREAM));   // this gets free'd in wavestreamDeinit()

 if (tstreamPtr) {

    MEMSET(tstreamPtr,0,sizeof(STREAM));

    rc = streamInit(streamType, tstreamPtr);
    if (rc == 0) {

       tstreamPtr->wsParentPtr = wsPtr;

       // update WAVESTREAM

       wsPtr->streamPtr = tstreamPtr;

       // WAVEAUDIO waPtr-> data already setup (incl. dma, audiobuffer)
       // but still need to set WAVESTREAM.waPtr -> wap (or war) structure (6-Feb-99)

       if (streamType == AUDIOHW_WAVE_PLAY) {
          wsPtr->waPtr = &wap;
       }
       else {
          wsPtr->waPtr = &war;
       }

       //WAVECONFIG. data already setup, some of it? like silence

       abReset(streamType & STREAM_RW_MODE, &wsPtr->waPtr->ab);
       wsPtr->waveConfig.sampleRate = (USHORT)mciInitPtr->lSRate;
       wsPtr->waveConfig.bitsPerSample = (UCHAR)mciInitPtr->lBitsPerSRate;
       wsPtr->waveConfig.channels = (UCHAR)mciInitPtr->sChannels;
       wsPtr->waveConfig.dataType = mciInitPtr->sMode;

       wsPtr->audioBufferSize = 0;  // 0=use default logical DMA size
       wsPtr->bytesProcessed = 0;
       wsPtr->timeBase = 0;

       mciInitPtr->sSlotNumber = -1;
       mciInitPtr->lResolution = 10;

       mciInitPtr->ulFlags = mciInitPtr->ulFlags | FIXED | LEFT_ALIGNED;

       // original had this if flags == 8...snafu (pas16 has 8 also)

       if (mciInitPtr->ulFlags == 16) {
          mciInitPtr->ulFlags = mciInitPtr->ulFlags | TWOS_COMPLEMENT;
       }
    }

    // since rc only possible if streamInit() failed, don't need to use streamDeinit()
    // but...streamInit() currently never fails...

    if (rc) {
       free(tstreamPtr);
       tstreamPtr = 0;
    }
 }

 return tstreamPtr;
}

// ------------------
// in:
//out:
//nts: deinit used rather than just doing this in streamDeinit
//     just to reverse wavestreamInit() concept

USHORT wavestreamDeinit(WAVESTREAM *wsPtr) {

 USHORT rc = 0;

 streamDeinit(wsPtr->streamPtr);
 free(wsPtr->streamPtr);
 free(wsPtr);

 return rc;
}


// --------------------------
// in: endPos
//     sbPtr -> stream buffer
//out:
//nts:
//
//  _vRealignBuffer
//  called just after a wave stream pause on a playback.
//  Gets the end position of the stream when paused and a pointer to a
//  STREAMBUFFER. Basicly this function looks at the streambuffer and if
// there is any unplayed data in it it adjusts the bufpos counter.
// the donepos counter is ALWAYS set to zero. It will return 0 if all
// the data has been played and 1 if there is still some data left.
//
// note that sbPtr->sizes are being aligned with 0xFFFFFFFC, which should do fine (don't use ALIGN_FILL_...)

static USHORT RealignBuffer(ULONG endPos, STREAM_BUFFER *sbPtr) {

 USHORT rc = 1;
 ULONG sbStart, consumed;

 sbStart = sbPtr->bufferDonePos - sbPtr->bufferCurrPos;

 if (endPos <= sbStart) {       // none of the data in this stream buffer has been consumed yet
    sbPtr->bufferDonePos = 0;   
    sbPtr->bufferCurrPos = 0;   
    // rc = 1; already is
 }
 else {                         // some or all of the data has been consumed
    consumed = endPos - sbStart;
    if (consumed <= sbPtr->bufferSize) {        // some has been...
       sbPtr->bufferDonePos = 0;

// !!!
// not sure if this should be ALIGN_FILL_PLAY instead of 0xFFFFFFFC
// but either this or the one in RealignBuffers() caused problem at pause if used FFFFFFFC

       //sbPtr->bufferCurrPos = (sbPtr->bufferSize-consumed) & 0xFFFFFFFC; //always keep dword-aligned
       sbPtr->bufferCurrPos = (sbPtr->bufferSize-consumed) & ALIGN_FILL_PLAY;
       // rc = 1; already is
    }
    else {                      // all has been...
       sbPtr->bufferDonePos = 0;
       sbPtr->bufferCurrPos = sbPtr->bufferSize;
       rc = 0;
    }
 }

 return rc;
}


// ----------------------------------
// in: wsPtr -> wave stream structure
//out:
//nts:
//
// _vRealignPausedBuffers(void)
// When a stream is paused, have to "realign" the data in the audio buffer with reality, since:
// - on playback, not all the data in the audio buffer has been consumed
// - on capture, not all the good data in the audio buffer has been copied out
//
// After getting DDCMDCONTROL PAUSE cmd, this routine is called to line the MMPM buffers back up:
// - for a capture stream: copy any data still in the audio buffer to an MMPM buffer
// - for a playback stream:
// -- check the STREAMBUFFER at proc queue to see if any unconsumed data is in the audio buffer
//    if yes, back up bufferCurrPos (was ulBuffpos) in the STREAMBUFFER
// -- check any STREAMBUFFERs on done queue, starting with the last one (the tail).
//    if necessary, back up bufferCurrPos (was ulBuffpos) and put the STREAMBUFFER on the Head queue

static VOID RealignPausedBuffers(WAVESTREAM *wsPtr) {

 STREAM *streamPtr = wsPtr->streamPtr;

 if ((streamPtr->streamType & STREAM_RW_MODE) == STREAM_READ) {
    ReadAudioBuffer(wsPtr);  // capture/recording
 }
 else {

    static STREAM_BUFFER_ANCHOR sbaTemp; // have to use static else gets "pointer truncated" warning
                                         // which is probably a compiler bug (!) anyway, check later
    STREAM_BUFFER *tsbPtr;
    ULONG endPos;
    USHORT rc;

    sbaTemp.headPtr = 0;
    sbaTemp.tailPtr = 0;

// !!!
// not sure if this should be ALIGN_FILL_PLAY instead of 0xFFFFFFFC
// but either this or the one in RealignBuffer() caused problem at pause if used FFFFFFFC

    //endPos = (abUpdate(1, &wsPtr->waPtr->ab)) & 0xFFFFFFFC; // get 'final' stream byte consumed/produced count
    endPos = (abUpdate(1, &wsPtr->waPtr->ab)) & ALIGN_FILL_PLAY; // get 'final' stream byte consumed count

    if (sbNotEmpty(&streamPtr->sbaProc)) {   // if buffer in proc queue...
       tsbPtr = sbHead(&streamPtr->sbaProc); // ...only care about first since rest are waiting...
       if (tsbPtr->bufferDonePos) {          // if any data has been written from this SB, realign
          RealignBuffer(endPos, tsbPtr);
       }
    }

    // if buffers on the done queue, pop off head and push them on sbaTemp head
    // this is to re-order them, putting the more recently used ones at the front of the queue
    //
    // pass all those (on sbaTemp queue) to RealignBuffer():
    // -- if rc = 0: no unprocessed data in the buffer (ready to be returned as-is)
    //    so it gets put on the tail of the done queue
    // -- if rc = 1: put it on head of in-process queue

    while (sbNotEmpty(&streamPtr->sbaDone)) {
       tsbPtr = sbPopHead(&streamPtr->sbaDone);
       sbPushOnHead(tsbPtr, &sbaTemp);
    }

    while (sbNotEmpty(&sbaTemp)) {
       tsbPtr = sbHead(&sbaTemp);
       rc = RealignBuffer(endPos, tsbPtr);
       tsbPtr = sbPopHead(&sbaTemp);
       if (rc == 0) {   
          sbPushOnTail(tsbPtr, &streamPtr->sbaDone); // buffer is done
       }
       else {           
          sbPushOnHead(tsbPtr, &streamPtr->sbaProc); // buffer has more
       }
    }
 }

 return;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: n/a
//nts: prepare to stream
//     (was _vInitAudioBuf)

static VOID ResetAudioBuffer(WAVESTREAM *wsPtr) {

 AUDIOBUFFER *abPtr = &wsPtr->waPtr->ab;
 USHORT streamMode = wsPtr->streamPtr->streamType & STREAM_RW_MODE;

 abReset(streamMode, abPtr);            // reset audiobuffer
 if (streamMode == STREAM_WRITE) {
    FillAudioBuffer(wsPtr);
 }
 else {
    abFill(wsPtr->waveConfig.silence, abPtr);
 }

 return;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: n/a
//nts: fill audio buffer with as much data as possible (for playback)
//     align on 256 bytes sizes though, using ALIGN_FILL_PLAY
//     was _vFillAudioBuf

static VOID FillAudioBuffer(WAVESTREAM *wsPtr) {

 ULONG space;
 ULONG bytesWritten = 0;        // just for tracePerf
 STREAM *streamPtr = wsPtr->streamPtr;
 AUDIOBUFFER *abPtr = &wsPtr->waPtr->ab;

 // while have process buffers and there is space in the audio buffer, write data to the audio buffer

#ifdef TRACE_FILLAUDIOBUFFER
 tracePerf(TRACE_FILLAUDIOBUFFER_IN, _IF());
#endif

 space = abSpace(abPtr) & ALIGN_FILL_PLAY;

 while(space && sbNotEmpty(&streamPtr->sbaProc)) {
    bytesWritten = bytesWritten + WriteAudioBuffer(wsPtr);  // bytesWritten just for tracePerf
    space = abSpace(abPtr) & ALIGN_FILL_PLAY;
 }

#ifdef TRACE_FILLAUDIOBUFFER
 tracePerf(TRACE_FILLAUDIOBUFFER_OUT, (bytesWritten << 16) | _IF());  //
#endif

 return;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: n/a
//nts: write one buffer to the audio buffer
//     caller must ensure that it's okay to write to the audio buffer
//     -- this assumes there's room and that there's a buffer on proc queue
//     (was _vAudioBufWrite)

static ULONG WriteAudioBuffer(WAVESTREAM *wsPtr) {

 UCHAR __far *dataBufferPtr;
 ULONG bufferLeft, bytesWritten, startPos;

 STREAM_BUFFER *sbPtr;
 STREAM *streamPtr  = wsPtr->streamPtr;
 AUDIOBUFFER *abPtr = &wsPtr->waPtr->ab;

 sbPtr =    sbHead(&streamPtr->sbaProc);
 startPos = abBytes(abPtr);   // could just do "startPos = abPtr->bufferBytes;" (all routine does)

 dataBufferPtr = sbPtr->bufferPtr + sbPtr->bufferCurrPos;
 bufferLeft    = sbPtr->bufferSize - sbPtr->bufferCurrPos;

 bytesWritten  = abWrite(dataBufferPtr, bufferLeft, abPtr);  // write to the audio buffer

 sbPtr->bufferDonePos = startPos + bytesWritten;  // store bytes written (updated in pause if needed)
 sbPtr->bufferCurrPos = sbPtr->bufferCurrPos + bytesWritten; // update position according to Garp

 // check if this emptied the buffer (check on dword align in case last buffer, which may not be)
 // if it's empty put it on the tail of the Done queue

 if (sbPtr->bufferCurrPos >= (sbPtr->bufferSize & 0xFFFFFFFC)) {
    STREAM_BUFFER *tsbPtr = sbPopHead(&streamPtr->sbaProc);
    sbPushOnTail(tsbPtr, &streamPtr->sbaDone);
 }

 return bytesWritten; // just so can use it for tracePerf
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: n/a
//nts: read recorded data from the audio buffer
//     called at interrupt time (via wavestreamProcess())
//     (was _vReadAudioBuf)

static ULONG ReadAudioBuffer(WAVESTREAM *wsPtr) {

 UCHAR __far *dataBufferPtr;
 ULONG bufferLeft, space, bytesRead;
 ULONG totBytes = 0;  // need separate var since bytesRead used in loop

 STREAM_BUFFER *sbPtr;
 STREAM *streamPtr  = wsPtr->streamPtr;
 AUDIOBUFFER *abPtr = &wsPtr->waPtr->ab;

 space = abSpace(abPtr) & ALIGN_FILL_CAPTURE;   // was not here at all before
 while(space && sbNotEmpty(&streamPtr->sbaProc)) {

    sbPtr = sbHead(&streamPtr->sbaProc);

    dataBufferPtr = sbPtr->bufferPtr + sbPtr->bufferCurrPos;
    bufferLeft    = sbPtr->bufferSize - sbPtr->bufferCurrPos;

    bytesRead = abRead(dataBufferPtr, bufferLeft, abPtr);  // read from the audio buffer
    totBytes = totBytes + bytesRead;  // for tracePerf

    sbPtr->bufferCurrPos = sbPtr->bufferCurrPos + bytesRead; // update position according to Garp

    // check if this emptied the buffer (check on dword align in case last buffer, which may not be)
    // if it's empty put it on the tail of the Done queue

    if (sbPtr->bufferCurrPos >= (sbPtr->bufferSize & 0xFFFFFFFC)) {
       STREAM_BUFFER *tsbPtr = sbPopHead(&streamPtr->sbaProc);
       sbPushOnTail(tsbPtr, &streamPtr->sbaDone);
    }

    space = abSpace(abPtr) & ALIGN_FILL_CAPTURE;  // was & 0xFFFFFFFC;;
 }

 return totBytes;
}




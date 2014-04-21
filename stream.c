//
// stream.c
// 27-Jan-99
//
// VOID    streamReturnBuffer(STREAM *streamPtr);
// VOID    streamReturnBuffers(STREAM *streamPtr);
// USHORT  streamPauseTime(STREAM *streamPtr);
// USHORT  streamResumeTime(STREAM *streamPtr);
// USHORT  streamRegister(DDCMDREGISTER __far *cmdPtr, STREAM *streamPtr);
// VOID    streamDeregister(STREAM *streamPtr);
// USHORT  streamRead(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr);
// USHORT  streamWrite(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr);
// USHORT  streamInit(USHORT streamType, STREAM *streamPtr);
// USHORT  streamDeinit(STREAM *streamPtr);
// STREAM *streamFindActive(USHORT streamType);
// STREAM *streamFindStreamSFN(ULONG SFN);
// STREAM *streamFindStreamHandle(HSTREAM streamH);
// STREAM_BUFFER *sbHead(STREAM_BUFFER_ANCHOR *sbaPtr);
// STREAM_BUFFER *sbTail(STREAM_BUFFER_ANCHOR *sbaPtr);
// VOID    sbPushOnHead(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
// VOID    sbPushOnTail(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
// STREAM_BUFFER *sbPopHead(STREAM_BUFFER_ANCHOR *sbaPtr);
// STREAM_BUFFER *sbPopTail(STREAM_BUFFER_ANCHOR *sbaPtr);
// USHORT  sbDestroyElement(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
// STREAM_BUFFER *sbPopElement(STREAM_BUFFER *match_sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
// USHORT  sbNotEmpty(STREAM_BUFFER_ANCHOR *sbaPtr);
// STREAM *streamHead(VOID);
// STREAM *streamTail(VOID);
// VOID    streamPushOnHead(STREAM *sPtr);
// VOID    streamPushOnTail(STREAM *sPtr);
// STREAM *streamPopHead(VOID);
// STREAM *streamPopTail(VOID);
// USHORT  streamDestroyElement(STREAM *sPtr);
// STREAM *streamPopElement(STREAM *match_sPtr);
// USHORT  streamNotEmpty(VOID);
//
// to init queue set headPtr/tailPtr = 0
// which is accomplished by simply MEMSET()'ing the allocation
// ie, the contructor (doesn't seem to be a destructor for the QUEUE class)
//
// ----------------------------------------- NOTE -------------------------------------------
// the following event-support routines are not yet done (stubs, all now have STREAM * as parm)
// parm list subject change (:
//
//VOID streamProcessEvents(STREAM *streamPtr) ;
//VOID streamEnableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr);
//VOID streamDisableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr);
//VOID streamSetNextEvent(STREAM *streamPtr);


#include "cs40.h"

static STREAM_ANCHOR streamList = {0,0}; // head of list (has .headPtr and .tailPtr to STREAM structures)

// ---------------------------------------------------------------------
// in: streamPtr -> stream from which to remove head buffer of done list
//out: n/a
//nts: frees stream buffer that was at head of done list

VOID streamReturnBuffer(STREAM *streamPtr) {

 static SHD_REPORTINT shdri = {SHD_REPORT_INT,0,0,0,0,0};  // structure to return buffers to SHD

 STREAM_BUFFER *tsbPtr = sbPopHead(&streamPtr->sbaDone);  // was STREAM_BUFFER *tsbPtr = (PSTREAMBUFFER)qhDone.PopHead();

// !!!
//ddprintf("@streamReturnBuffer, streamPtr=%x\n",streamPtr);

 if (tsbPtr) {

    if (streamPtr->streamType & STREAM_WRITE) {

       // this is a write (playback): tell stream handler that we played all of the buffer

       shdri.ulFlag = SHD_WRITE_COMPLETE;
       shdri.ulStatus = tsbPtr->bufferSize;
    }
    else {

       // this is a capture: tell stream hamdler how much data we wrote to the buffer

       shdri.ulFlag = SHD_READ_COMPLETE;
       shdri.ulStatus = tsbPtr->bufferCurrPos;
    }

    shdri.hStream = streamPtr->streamH;
    shdri.pBuffer = tsbPtr->bufferPtr;

    // wavestreamGetCurrentTime() needs to access stream's parent WAVESTREAM structure --
    // this is the only case (so far) where a stream operation needs to look at what's
    // in WAVSTREAM (specifically, abPtr, needed by this get time call) -- had to add
    // this member to the STREAM structure (1-Feb-99), and set it up in streamInit(),
    // which is called by wavestreamInit() (WAVESTREAM constructor) by IoctlAudioInit()...
    // ugh, the driver docs just plain suck

    shdri.ulStreamTime = wavestreamGetCurrentTime(streamPtr->wsParentPtr);

#ifdef TRACE_RETBUFFER
 tracePerf(TRACE_RETBUFFER_IN, _IF());
#endif

    streamPtr->pfnSHD(&shdri);  // function call

#ifdef TRACE_RETBUFFER
 tracePerf(TRACE_RETBUFFER_OUT,_IF());
#endif

    free(tsbPtr); // this is what lots and lots of allocations come from

// !!!
//ddprintf("~streamReturnBuffer:free:%x\n",tsbPtr);

 }

 return;
}


// --------------------------------------------------------
// in: streamPtr -> stream from which to remove head buffer
//out: n/a
//nts: returns (and frees) all stream buffers

VOID streamReturnBuffers(STREAM *streamPtr) {

 STREAM_BUFFER *tsbPtr;

 while(sbNotEmpty(&streamPtr->sbaProc)) {       // likely gets inlined so use sbNotEmpty()
    tsbPtr = sbPopHead(&streamPtr->sbaProc);    // rather than sbPopHead() until it returns 0
    sbPushOnTail(tsbPtr, &streamPtr->sbaDone);  // pop from proc list and push on done list since...
 }

 while(sbNotEmpty(&streamPtr->sbaDone)) {       // stream buffers were moved to the done list
    streamReturnBuffer(streamPtr);              // because that's what streamReturnBuffer() uses
 }                                              // namely, the head of the done list

 return;
}


// ---------------------------------------------------
// in: streamPtr -> stream to pause/resume time counter
//out: always 0, for okay 
//nts: returns USHORT though caller (ssm_idc.c) expects ULONG to return back to MMPM

USHORT streamPauseTime(STREAM *streamPtr) {
 streamPtr->counterIncFlag = 0;
 return 0;
}

USHORT streamResumeTime(STREAM *streamPtr) {
 streamPtr->counterIncFlag = 1;
 return 0;
}


// ---------------------------------------------------
// in: cmdPtr -> DDCMDREGISTER pack (far ptr)
//     streamPtr -> stream working on
//out: always 0, for okay 
//nts: returns USHORT though caller (ssm_idc.c) expects ULONG to return back to MMPM
//
// ulBufSize: Specifies VSD or PDD output buffer size in bytes that will be sent to
//            the driver. The caller will fill in a default value. If the device driver
//            does not override this value, it will be used to determine the buffer size.
//
// Note:  The device driver sets this field to indicate the buffer size that it would like to
// receive from the streaming subsystem. For MIDI, this field is usually set to 512. For DMA-based
// audio or other systems where the interrupt rate equals the buffer rate, this field should
// be set to about 1/30 the quantity of data consumed or produced per second. This field supplies
// the system with a minimal buffer size for wave audio. Because other system elements also
// negotiate to determine buffer size and for software motion video, the system might provide
// buffers smaller than the size requested.
  
USHORT streamRegister(DDCMDREGISTER __far *cmdPtr, STREAM *streamPtr) {

 streamPtr->streamH = cmdPtr->hStream;
 streamPtr->pfnSHD =  (T_PFNSHD) cmdPtr->pSHDEntryPoint;  // need cast since using a __cdecl cc

 cmdPtr->ulAddressType = ADDRESS_TYPE_VIRTUAL;  // type of buffer pointer want to get from SSM
 cmdPtr->mmtimePerUnit = 0;     // not used
 cmdPtr->ulBytesPerUnit = 0;    // not used

 // suggestions:
 cmdPtr->ulNumBufs = 0x00000006;  // 6 default buffers, at 60K each, is about 2 secs
 cmdPtr->ulBufSize = 0x0000F000;  // see Note above

 return 0;
}


// ---------------------------------------------------
// in: streamPtr -> stream to deregister
//out: n/a
//nts:

VOID streamDeregister(STREAM *streamPtr) {

 streamPtr->streamH = 0;
 streamPtr->pfnSHD = 0;

 return;
}


// ---------------------------------------------
// in: bufferPtr -> stream buffer data (far ptr)
//     bufferSize = size of empty buffer (passed down as ULONG from kernel, org was short)
//     wsPtr -> WAVESTREAM structure
//out: 0 if okay, else rc (no memory, eg)
//nts: called by DDCMD_READ
//     original was getting length as "unsigned" as in USHORT
//     similar to streamWrite()
//     was called Read(), was also in wavestrm.c but why?  so moved here, to stream.c
//     (when it was in wavestrm.c, it was using &wsPtr->streamPtr->sbaProc... otherwise same)

USHORT streamRead(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr) {

 USHORT rc = 0;
 STREAM_BUFFER *newsbPtr = malloc(sizeof(STREAM_BUFFER));

// !!!
//ddprintf("@streamRead:newsbPtr=%x\n",newsbPtr);

 if (newsbPtr) {
    newsbPtr->nextPtr = 0;
    newsbPtr->rsv2 = 0;
    newsbPtr->bufferPtr = bufferPtr;
    newsbPtr->bufferSize = length;
    newsbPtr->bufferCurrPos = 0;
    newsbPtr->bufferDonePos = 0;

    sbPushOnTail(newsbPtr, &streamPtr->sbaProc);
 }
 else {
    rc = 8;
 }

 return rc;
}


// ---------------------------------------------------
// in: bufferPtr -> streaming buffer to add
//     length = size of streaming buffer to add
//     streamPtr -> stream to add stream buffer to
//out: 0 if okay, else rc (no memory, eg)
//nts: original was getting length as "unsigned" as in USHORT
//     org was also always in stream.c, unlike Read() which was in wavestrm.c

USHORT streamWrite(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr) {

 USHORT rc = 0;
 STREAM_BUFFER *newsbPtr = malloc(sizeof(STREAM_BUFFER));

// !!!
//ddprintf("@streamWrite:newsbPtr=%x\n",newsbPtr);

 if (newsbPtr) {
    newsbPtr->nextPtr = 0;
    newsbPtr->rsv2 = 0;
    newsbPtr->bufferPtr = bufferPtr;
    newsbPtr->bufferSize = length;
    newsbPtr->bufferCurrPos = 0;
    newsbPtr->bufferDonePos = 0;

    sbPushOnTail(newsbPtr, &streamPtr->sbaProc);
 }
 else {
    rc = 8;
 }

 return rc;
}


// ---------------------------------------------------
// in: streamType = eg, STREAM_WAVE_CAPTURE (==AUDIOHW_WAVE_CAPTURE)(org ULONG)
//     streamPtr -> stream structure
//out: always 0 for now
//nts:
//
// actually, STREAM constructor is called as part of WAVESTREAM constructor (STREAM
// constructor is done first, though) so this (streamInit()) will be called by
// wavestreamInit()...

USHORT streamInit(USHORT streamType, STREAM *streamPtr) {

 streamPushOnTail(streamPtr);
 streamPtr->streamType = streamType;

 // already have verified that streamType is a supported type in IOCtl audio init
 // so ahwPtr will be valid (so don't need to check here)

 streamPtr->streamH = 0;        // ensure won't match yet on FindStream()
 streamPtr->SFN = 0;            // ditto (since these are not yet operational streams)
 streamPtr->counterIncFlag = 1;
 streamPtr->currentTime = 0;    // this is updated when DDCMD_STATUS gotten
 streamPtr->streamState = STREAM_STOPPED;

 return 0;
}


// ---------------------------------------------------
// in: streamPtr -> stream structure
//out: always 0 for now
//nts: stream destructor (does not free(streamPtr) though...that's done in wavestreamDeinit)

USHORT streamDeinit(STREAM *streamPtr) {

 USHORT rc = 0;
 STREAM_BUFFER *tsbPtr;

 if (streamPtr->streamState == STREAM_STREAMING) {

    // this has been replaced...
    // streamPtr->wsParentPtr->waPtr->ahwPtr->xStop(); // yipes, talk about an indirect function call
    // by this:

    if (streamPtr->wsParentPtr->waPtr->devType == AUDIOHW_WAVE_PLAY) {
       rc = waveplayStop(streamPtr->wsParentPtr);
    }
    else {
       rc = waverecStop(streamPtr->wsParentPtr);
    }
 }

 // destroy all stream buffers and events still associated with this stream

 while(sbNotEmpty(&streamPtr->sbaProc)) {
    tsbPtr = sbHead(&streamPtr->sbaProc);
    sbDestroyElement(tsbPtr, &streamPtr->sbaProc);
 }

 while(sbNotEmpty(&streamPtr->sbaDone)) {
    tsbPtr = sbHead(&streamPtr->sbaDone);
    sbDestroyElement(tsbPtr, &streamPtr->sbaDone);
 }

 // can do this event processing code now since won't do anything (always empty for now)

 while(sbNotEmpty(&streamPtr->sbaEvent)) {
    tsbPtr = sbHead(&streamPtr->sbaEvent);
    sbDestroyElement(tsbPtr, &streamPtr->sbaEvent);
 }

 streamPopElement(streamPtr);   // remove this stream from list

 return 0;
}


// ---------------------------------------------------
// in: streamType (was ULONG)
//out: streamPtr -> active stream
//nts: finds first active stream of matching type

STREAM *streamFindActive(USHORT streamType) {

 STREAM *streamPtr = streamList.headPtr;

 while (streamPtr) {
    if (streamPtr->streamType == streamType && streamPtr->streamState == STREAM_STREAMING) break;
    streamPtr = streamPtr->nextPtr;
 }

 return streamPtr;
}


// ---------------------------------------------------
// in: SFN (was ULONG)
//out: streamPtr -> stream with this SFN
//nts: map SFN to stream object

STREAM *streamFindStreamSFN(USHORT SFN) {

 STREAM *streamPtr = streamList.headPtr;

 while (streamPtr) {
    if (streamPtr->SFN == SFN) break;
    streamPtr = streamPtr->nextPtr;
 }

 return streamPtr;
}


// ---------------------------------------------------
// in: streamH
//out: streamPtr -> stream with this handle
//nts: map stream handle to stream object

STREAM *streamFindStreamHandle(HSTREAM streamH) {

 STREAM *streamPtr = streamList.headPtr;

 while (streamPtr) {
    if (streamPtr->streamH == streamH) break;
    streamPtr = streamPtr->nextPtr;
 }

 return streamPtr;
}


// -----------------------------------------------------------------------------
// Queue support for STREAM_BUFFER objects (STREAM objects follows this, below)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// in: streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: stream_buffer pointer -> head
//nts: sort of like PopHead() but doesn't remove head
//
// eg,
// STREAM_BUFFER *DoneHeadPtr = sbHead(&streamPtr->sbaDone);

STREAM_BUFFER *sbHead(STREAM_BUFFER_ANCHOR *sbaPtr) {

 STREAM_BUFFER *tempPtr = sbaPtr->headPtr;

 return tempPtr;
}


// -------------------------------------------------------------------
// in: streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: stream_buffer pointer -> tail
//nts: sort of like PopTail() but doesn't remove tail
//
// eg,
// STREAM_BUFFER *DoneHeadPtr = sbTail(&streamPtr->sbaDone);

STREAM_BUFFER *sbTail(STREAM_BUFFER_ANCHOR *sbaPtr) {

 STREAM_BUFFER *tempPtr = sbaPtr->tailPtr;

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: stream buffer ptr -> stream buffer item to put on head of queue
//     streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: n/a
//nts:
//
// eg,
// sbPushOnHead(sbPtr, &streamPtr->sbaDone);

VOID sbPushOnHead(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr) {

 if (sbaPtr->headPtr == 0) {            // empty list so just put it on
    sbPtr->nextPtr = 0;
    sbaPtr->headPtr = sbPtr;
    sbaPtr->tailPtr = sbPtr;
 }
 else {
    sbPtr->nextPtr = sbaPtr->headPtr;   // set buffer's nextPtr to current head
    sbaPtr->headPtr = sbPtr;            // and make this buffer the new head
 }

 return;
}


// -----------------------------------------------------------------------------
// in: stream buffer ptr -> stream buffer item to put on tail of queue
//     streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: n/a
//nts:
//
// eg,
// sbPushOnTail(sbPtr, &streamPtr->sbaDone);

VOID sbPushOnTail(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr) {

 sbPtr->nextPtr = 0;
 if (sbaPtr->headPtr == 0) {            // empty list so just put it on
    sbaPtr->headPtr = sbPtr;
    sbaPtr->tailPtr = sbPtr;
 }
 else {
    sbaPtr->tailPtr->nextPtr = sbPtr;   // set current tail's nextPtr to new tail
    sbaPtr->tailPtr = sbPtr;            // and make this buffer the new tail
 }

 return;
}


// -----------------------------------------------------------------------------
// in: streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: stream_buffer pointer -> what was at head of the queue
//nts:
//
// eg,
// STREAM_BUFFER *DoneHeadPtr = sbPopHead(&streamPtr->sbaDone);

STREAM_BUFFER *sbPopHead(STREAM_BUFFER_ANCHOR *sbaPtr) {

 STREAM_BUFFER *tempPtr = sbaPtr->headPtr;

 if (tempPtr) {
    sbaPtr->headPtr = tempPtr->nextPtr;
    if (sbaPtr->headPtr == 0) sbaPtr->tailPtr = 0;
    tempPtr->nextPtr = 0;
 }

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: stream_buffer pointer -> what was at tail of the queue
//nts:
//
// eg,
// STREAM_BUFFER *DoneTailPtr = sbPopTail(&streamPtr->sbaDone);

STREAM_BUFFER *sbPopTail(STREAM_BUFFER_ANCHOR *sbaPtr) {

 STREAM_BUFFER *tempPtr = sbaPtr->tailPtr;
 STREAM_BUFFER *temp2Ptr = sbaPtr->headPtr;

 if (tempPtr == temp2Ptr) {

    // only one item in list, or it's empty

    sbaPtr->headPtr = 0;
    sbaPtr->tailPtr = 0;
    if (tempPtr) tempPtr->nextPtr = 0;  // set the old tail's nextPtr to 0
 }
 else {
    if (tempPtr) {
       while (temp2Ptr->nextPtr != tempPtr) { 
          temp2Ptr = temp2Ptr->nextPtr; // scan down list to just before tail
       }
       sbaPtr->tailPtr = temp2Ptr;      // set the new tail to be that one

       if (sbaPtr->tailPtr == 0) {      // hm, can this happen (seem like has to only
          sbaPtr->headPtr = 0;          // have one item for it to, and that's covered above)
       }
       else {
          sbaPtr->tailPtr->nextPtr = 0; // mark new tail's nextPtr as 0 (since it's the tail!)
       }
       tempPtr->nextPtr = 0;            // set the old tail's nextPtr to 0
    }
 }

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: sbPtr -> stream buffer to destroy
//     streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: 1 if destroyed it, else not there
//nts: frees memory allocated at sbPtr
//
// eg,
// destroyedItFlag = sbDestroyElement(sbPtr, &streamPtr->sbaDone);

USHORT sbDestroyElement(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr) {

 USHORT rc = 0;

 if (sbPopElement(sbPtr,sbaPtr)) {
    free(sbPtr);

// !!!
//ddprintf("@sbDestroyElement:free:%x\n",sbPtr);

    rc = 1;
 }

 return rc;
}


// -----------------------------------------------------------------------------
// in: match_sbPtr -> stream buffer to remove for queue (based on address)
//     streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: returns match_sbPtr (same as entry) if found/removed, else 0 if not there
//nts: see below
//
// eg,
// sbPtr = sbPopElement(sbPtr, &streamPtr->sbaDone);

STREAM_BUFFER *sbPopElement(STREAM_BUFFER *match_sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr) {

 STREAM_BUFFER *tempPtr = 0;

 if (sbaPtr->headPtr == 0) {            // empty queue
    return 0;
 }

 if (match_sbPtr == sbaPtr->headPtr) {  // if matches head, pop head
    tempPtr = sbPopHead(sbaPtr);
    return tempPtr;
 }

 if (match_sbPtr == sbaPtr->tailPtr) {  // if matches tail, pop tail
    tempPtr = sbPopTail(sbaPtr);
    return tempPtr;
 }

 // this next section scans items, check the nextPtrs of each -- when a nextPtr points
 // to the match_sbPtr it's known that the item is there, so set that nextPtr to point
 // to the item's nextPtr rather than the item itself (item is match_sbPtr in this case)
 // effectively removing the item from the list

 tempPtr = sbaPtr->headPtr;
 while (tempPtr->nextPtr != match_sbPtr && tempPtr->nextPtr != 0) {
    tempPtr = tempPtr->nextPtr;         // scan elements until find it in someone's nextPtr...
 }
 if (tempPtr == 0) return 0;            // not found at all

 tempPtr->nextPtr = match_sbPtr->nextPtr; // now set links so the item is removed from the list

 return match_sbPtr;
}


// -----------------------------------------------------------------------------
// in: streamBufferAnchorPtr -> queue anchor to either proc, done, or event queues
//out: 0 if the list is empty, otherwise 1
//nts: original returned a ULONG for some reason
//
// eg,
// notEmpty = sbNotEmpty(&streamPtr->sbaDone);

USHORT sbNotEmpty(STREAM_BUFFER_ANCHOR *sbaPtr) {

 USHORT rc = 0;

 if (sbaPtr->headPtr) rc = 1;

 return rc;
}

// -------------------------------------
// to init queue set headPtr/tailPtr = 0
// which is accomplished by simply MEMSET()'ing the allocation
// ie, the contructor (doesn't seem to be a destructor for the QUEUE class)


// -----------------------------------------------------------------------------
// Queue support for STREAM objects
// -----------------------------------------------------------------------------
// note: unlike STREAM_BUFFER queue support, don't need to pass anchor pointer
//       since it's known to be streamList


// -----------------------------------------------------------------------------
// in: n/a
//out: stream pointer -> head
//nts: sort of like PopHead() but doesn't remove head
//
// eg,
// STREAM *HeadPtr = streamHead();

STREAM *streamHead(VOID) {

 STREAM *tempPtr = streamList.headPtr;

 return tempPtr;
}


// -------------------------------------------------------------------
// in: n/a
//out: stream pointer -> tail
//nts: sort of like PopTail() but doesn't remove tail
//
// eg,
// STREAM *TailPtr = streamTail();

STREAM *streamTail(VOID) {

 STREAM *tempPtr = streamList.tailPtr;

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: stream ptr -> stream item to put on head of queue
//out: n/a
//nts:
//
// eg,
// streamPushOnHead(sPtr);

VOID streamPushOnHead(STREAM *sPtr) {

 if (streamList.headPtr == 0) {         // empty list so just put it on
    sPtr->nextPtr = 0;
    streamList.headPtr = sPtr;
    streamList.tailPtr = sPtr;
 }
 else {
    sPtr->nextPtr = streamList.headPtr; // set stream's nextPtr to current head
    streamList.headPtr = sPtr;          // and make this buffer the new head
 }

 return;
}


// -----------------------------------------------------------------------------
// in: stream ptr -> stream item to put on tail of queue
//out: n/a
//nts:
//
// eg,
// streamPushOnTail(sPtr);

VOID streamPushOnTail(STREAM *sPtr) {

 sPtr->nextPtr = 0;
 if (streamList.headPtr == 0) {         // empty list so just put it on
    streamList.headPtr = sPtr;
    streamList.tailPtr = sPtr;
 }
 else {
    streamList.tailPtr->nextPtr = sPtr; // set current tail's nextPtr to new tail
    streamList.tailPtr = sPtr;          // and make this buffer the new tail
 }

 return;
}


// -----------------------------------------------------------------------------
// in: n/a
//out: stream pointer -> what was at head of the queue
//nts:
//
// eg,
// STREAM *HeadPtr = streamPopHead();

STREAM *streamPopHead(VOID) {

 STREAM *tempPtr = streamList.headPtr;

 if (tempPtr) {
    streamList.headPtr = tempPtr->nextPtr;
    if (streamList.headPtr == 0) streamList.tailPtr = 0;
    tempPtr->nextPtr = 0;
 }

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: n/a
//out: stream pointer -> what was at tail of the queue
//nts:
//
// eg,
// STREAM *TailPtr = streamPopTail();

STREAM *streamPopTail(VOID) {

 STREAM *tempPtr = streamList.tailPtr;
 STREAM *temp2Ptr = streamList.headPtr;

 if (tempPtr == temp2Ptr) {

    // only one item in list, or it's empty

    streamList.headPtr = 0;
    streamList.tailPtr = 0;
    if (tempPtr) tempPtr->nextPtr = 0;  // set the old tail's nextPtr to 0
 }
 else {
    if (tempPtr) {
       while (temp2Ptr->nextPtr != tempPtr) { 
          temp2Ptr = temp2Ptr->nextPtr; // scan down list to just before tail
       }
       streamList.tailPtr = temp2Ptr;   // set the new tail to be that one

       if (streamList.tailPtr == 0) {   // hm, can this happen (seem like has to only
          streamList.headPtr = 0;       // have one item for it to, and that's covered above)
       }
       else {
          streamList.tailPtr->nextPtr = 0; // mark new tail's nextPtr as 0 (since it's the tail!)
       }
       tempPtr->nextPtr = 0;            // set the old tail's nextPtr to 0
    }
 }

 return tempPtr;
}


// -----------------------------------------------------------------------------
// in: sPtr -> stream to destroy
//out: 1 if destroyed it, else not there
//nts: frees memory allocated at sPtr
//
// eg,
// destroyedItFlag = streamDestroyElement(sPtr);

USHORT streamDestroyElement(STREAM *sPtr) {

 USHORT rc = 0;

 if (streamPopElement(sPtr)) {
    free(sPtr);

// !!!
//ddprintf("@streamDestroyElement:free:%x\n",sPtr);

    rc = 1;
 }

 return rc;
}


// -----------------------------------------------------------------------------
// in: match_sPtr -> stream to remove for queue (based on address)
//out: returns match_sPtr (same as entry) if found/removed, else 0 if not there
//nts: see below
//
// eg,
// sPtr = streamPopElement(sPtr);

STREAM *streamPopElement(STREAM *match_sPtr) {

 STREAM *tempPtr = 0;

 if (streamList.headPtr == 0) {         // empty queue
    return 0;
 }

 if (match_sPtr == streamList.headPtr) { // if matches head, pop head
    tempPtr = streamPopHead();
    return tempPtr;
 }

 if (match_sPtr == streamList.tailPtr) { // if matches tail, pop tail
    tempPtr = streamPopTail();
    return tempPtr;
 }

 // this next section scans items, check the nextPtrs of each -- when a nextPtr points
 // to the match_sPtr it's known that the item is there, so set that nextPtr to point
 // to the item's nextPtr rather than the item itself (item is match_sPtr in this case)
 // effectively removing the item from the list

 tempPtr = streamList.headPtr;
 while (tempPtr->nextPtr != match_sPtr && tempPtr->nextPtr != 0) {
    tempPtr = tempPtr->nextPtr;         // scan elements until find it in someone's nextPtr...
 }
 if (tempPtr == 0) return 0;            // not found at all

 tempPtr->nextPtr = match_sPtr->nextPtr; // now set links so the item is removed from the list

 return match_sPtr;
}


// -----------------------------------------------------------------------------
// in: n/a
//out: 0 if the list is empty, otherwise 1
//nts: original returned a ULONG for some reason
//
// eg,
// notEmpty = streamNotEmpty();

USHORT streamNotEmpty(VOID) {

 USHORT rc = 0;

 if (streamList.headPtr) rc = 1;

 return rc;
}


// ---------------------------------
// in: streamPtr -> stream structure
//out:
//nts:

VOID streamProcessEvents(STREAM *streamPtr) {

 return;
 streamPtr;
}


// --------------------------------------
// in: ddcmdPtr -> DDCMDCONTROL structure (far ptr)
//     streamPtr -> stream structure
//out:
//nts:

VOID streamEnableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr) {

 return;
 ddcmdPtr;
 streamPtr;
}


// --------------------------------------
// in: ddcmdPtr -> DDCMDCONTROL structure (far ptr)
//     streamPtr -> stream structure
//out:
//nts:

VOID streamDisableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr) {

 return;
 ddcmdPtr;
 streamPtr;
}


// --------------------------------------
// in: streamPtr -> stream structure
//out:
//nts:

VOID streamSetNextEvent(STREAM *streamPtr) {

 return;
 streamPtr;
}


#if 0

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// the following event-support routines are not yet done
//  void STREAM::ProcessEvents(void)
//  ULONG STREAM::EnableEvent(PDDCMDCONTROL pControl)
//  ULONG STREAM::DisableEvent(PDDCMDCONTROL pControl)
//  void STREAM::SetNextEvent(void)


class EVENT;


// ProcessEvents
// called by the Process at interrupt time to see if there are
// any events that have timed out
//
void STREAM::ProcessEvents(void)
{
   if (qhEvent.IsElements()) {
      PEVENT pnextevent = (PEVENT)qhEvent.Head();
      ULONG time = GetCurrentTime();
      ULONG eventtime = pnextevent->GetEventTime();
      if (eventtime <= time)
         pnextevent->Report(time);
   }

}
ULONG STREAM::EnableEvent(PDDCMDCONTROL pControl)
{

   // see if the event already exists on the event queue
   // call FindEvent if we get back an address (event exists)
   // call the UpdateEvent member function and get the event info updated.
   // if Findevent returns NULL (no event on queue) then call the construct
   // a new event and put it on the tail of the event queue. then call
   // SetNextEvent to update the next event to time out....

   PEVENT pevent = FindEvent(pControl->hEvent, &qhEvent);
   if (pevent)
      pevent->UpdateEvent(this,pControl->hEvent,(PCONTROL_PARM)pControl->pParm);
   else {
      pevent= new EVENT(this,pControl->hEvent,(PCONTROL_PARM)pControl->pParm);
   }
   if (!pevent)
      return ERROR_TOO_MANY_EVENTS;

   SetNextEvent();
   return NO_ERROR;
}

ULONG STREAM::DisableEvent(PDDCMDCONTROL pControl)
{
   pTrace->vLog(DDCMD_DisableEvent,pControl->hEvent);
   PEVENT pevent = FindEvent(pControl->hEvent, &qhEvent);
   if (!pevent)
      return ERROR_INVALID_EVENT;

    // destroying an event may change things that get referenced in the ISR
    // so we really need to cli/sti around the call to DestroyElement
   _cli_();
   qhEvent.DestroyElement((PQUEUEELEMENT)pevent);
   if (qhEvent.Head() != qhEvent.Tail())
      SetNextEvent();
   _sti_();
   return NO_ERROR;
}



/**@internal SetNextEvent
 * @param    None
 * @return   None
 * @notes
 * the function walks the event list and finds the next event to timeout.
 * the event is moved to the head of the event queue.
 *
 */
void STREAM::SetNextEvent(void)
{

   // if there are no events or only one event on the
   // queue just return
   if ((qhEvent.Head()) == (qhEvent.Tail()))
       return;

   PQUEUEELEMENT pele1 = qhEvent.Head();
   PQUEUEELEMENT pele2 = NULL;
   ULONG ulTimeToBeat = -1;     // -1 equals 0xFFFFFFFF the maximum time

   while (pele1) {
      if (((PEVENT)pele1)->GetEventTime() <= ulTimeToBeat) {
         pele2 = pele1;
         ulTimeToBeat = ((PEVENT)pele1)->GetEventTime();
      }
      pele1 = pele1->pNext;
   }
   // pele2 should now contain the address of the next
   // event to time out.. if it is not already on
   // the head of the Event queue then put it there
   if (pele2 != qhEvent.Head()) {
      _cli_();
      qhEvent.PopElement(pele2);
      qhEvent.PushOnHead(pele2);
      _sti_();
   }
}


#endif



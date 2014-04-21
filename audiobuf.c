//
// audiobuf.c
// 27-Jan-99
//
//
// static USHORT AllocMem(ULONG bufferSize, AUDIOBUFFER *audioBufferPtr);
// static ULONG GetStartOffset(AUDIOBUFFER *audioBufferPtr);
// VOID   abReset(USHORT mode, AUDIOBUFFER *audioBufferPtr);
// ULONG  abSpace(AUDIOBUFFER *audioBufferPtr);
// ULONG  abBytes(AUDIOBUFFER *audioBufferPtr);
// ULONG  abUpdate(USHORT flags, AUDIOBUFFER *audioBufferPtr);
// ULONG  abWrite(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr);
// ULONG  abRead(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr);
// VOID   abFill(USHORT fillWith, AUDIOBUFFER *audioBufferPtr);
// VOID   abDeinit(AUDIOBUFFER *audioBufferPtr);
// USHORT abInit(ULONG bufferSize, ULONG pageSize, USHORT dmaChannel, AUDIOBUFFER *audioBufferPtr);

#include "cs40.h"

PFN Device_Help = 0;

// ---------------
// in: bufferSize, amount of physical memory to allocate (not all likely will be usuable)
//out: rc (PDD says only rc=87 is possible error code)
//nts: seems they all use this, and request above 1MB first, then under 1MB if none there
//     may always be < 16MB here?  since DMA won't work if it's past 16MB mark
//     only allocates raw physical memory, still need to massage it before using

static USHORT AllocMem(ULONG bufferSize, AUDIOBUFFER *audioBufferPtr) {

 USHORT rc;
 ULONG physAddr;

 rc = DevHelp_AllocPhys(bufferSize, MEMTYPE_ABOVE_1M, &physAddr);
 if (rc) rc = DevHelp_AllocPhys(bufferSize, MEMTYPE_BELOW_1M ,&physAddr);
 if (rc == 0) {
    audioBufferPtr->bufferPhysAddrRaw = physAddr; // won't change
    audioBufferPtr->bufferPhysAddr = physAddr;    // will be forced to next 64K alignment
 }

 return rc;
}


// ------------------
// in: audioBufferPtr
//out: offset of next start
//nts: gets offset in audio buffer of where next read/write operation should start

static ULONG GetStartOffset(AUDIOBUFFER *audioBufferPtr) {

 ULONG t = audioBufferPtr->bufferBytes % audioBufferPtr->bufferSize;

 return t;
}


// ------------------
// in: mode = read or write
//     audioBufferPtr
//out: n/a
//nts: reset the audio buffer (was named InitBuffer)

VOID abReset(USHORT mode, AUDIOBUFFER *audioBufferPtr) {

 audioBufferPtr->mode = mode;
 audioBufferPtr->deviceBytes = 0;
 audioBufferPtr->bufferBytes = 0;

 return;
}


// ------------------
// in: audioBufferPtr
//out: good bytes
//nts: gets bytes available in write audio buffer / valid bytes in read audio buffer
//     original had _fGetSpace() called only by BufferStatus() (no point), so just coding
//     BufferStatus() -- i.e., this returns the "good data" in the buffer (on a read or
//     capture) or the "room left" in the buffer (on a write or play)
//     original name was also BufferStatus()

ULONG abSpace(AUDIOBUFFER *audioBufferPtr) {

 ULONG t;

 if (audioBufferPtr->mode == AUDIOBUFFER_WRITE) {
    t = audioBufferPtr->bufferSize - (audioBufferPtr->bufferBytes - audioBufferPtr->deviceBytes); // bytes available in buffer (write to dev, play)
 }
 else {
    t = audioBufferPtr->deviceBytes - audioBufferPtr->bufferBytes;  // valid data bytes in buffer (read from dev, capture)
 }

 return t;
}


// ------------------
// in: audioBufferPtr
//out: bytes written|read
//nts: returns total number of bytes written to (playback mode) or read from (record mode) device

ULONG abBytes(AUDIOBUFFER *audioBufferPtr) {

 ULONG t = audioBufferPtr->bufferBytes;

 return t;
}


// ---------------
// in: flags (was ULONG)
//     audioBufferPtr
//out: data consumed or produced
//nts: called by the stream to get the "latest" number of bytes consumed|produced by device
//     Flags is either 0 or not 0: if not 0 the audio buffer will call into the hardware
//     (in this case dma object) and get the latest number of bytes value

ULONG abUpdate(USHORT flags, AUDIOBUFFER *audioBufferPtr) {

 if (flags) {
    audioBufferPtr->deviceBytes = audioBufferPtr->deviceBytes + dmaQueryDelta(audioBufferPtr);
 }

 return audioBufferPtr->deviceBytes;
}


// ---------------------------
// in: dataPtr -> data to play
//     dataSize = bytes in buffer
//     audioBufferPtr
//out: bytes actually written to buffer
//nts: (was called WriteBuffer())
//     writes data into the audio buffer (for it to be played)
//     This should be called by a stream that is doing a playback -- this will only
//     do the write based on the data returned by GetSpace/GetStartOffset
//     BufferUpdate should be called before this routine

ULONG abWrite(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr) {

 ULONG abSize = audioBufferPtr->bufferSize;
 ULONG startOffset = GetStartOffset(audioBufferPtr);
 ULONG bytes = min(dataSize, abSpace(audioBufferPtr));

 if (bytes >= abSize) bytes = abSize;  // max limit is physical buffer size
 bytes = bytes & ALIGN_FILL_PLAY;      // align now, after above
 if (bytes == 0) goto ExitNow;         // shouldn't happen, unless abSpace() is 0, which it won't

 // if this write will wrap the buffer, split it up and do 2 writes

 if ((startOffset + bytes) > abSize) {

    ULONG diff = abSize - startOffset;

    MEMCPY(audioBufferPtr->bufferPtr+startOffset, dataPtr, diff);
    MEMCPY(audioBufferPtr->bufferPtr, dataPtr+diff, bytes-diff);

// !!!
// won't get here when doing ALIGN_FILL_PLAY size that's an even multiple of buffer

//tracePerf(128,(ULONG)audioBufferPtr->bufferPtr+startOffset);
//tracePerf(129,(ULONG)dataPtr);

 }
 else {
    MEMCPY(audioBufferPtr->bufferPtr+startOffset, dataPtr, bytes);
 }

 audioBufferPtr->bufferBytes = audioBufferPtr->bufferBytes + bytes;

ExitNow:

 return bytes;
}


// ---------------------------
// in: dataPtr -> data to read
//     dataSize = bytes in buffer
//     audioBufferPtr
//out: bytes actually read from buffer
//nts: (was called ReadBuffer())
//     reads data from the audio buffer (it was recorded)
//     This should be called by a stream that is doing a record -- this will only
//     do the read based on the data returned by GetSpace/GetStartOffset
//     BufferUpdate should be called before this routine

ULONG abRead(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr) {

 ULONG abSize = audioBufferPtr->bufferSize;
 ULONG startOffset = GetStartOffset(audioBufferPtr);
 ULONG bytes = min(dataSize, abSpace(audioBufferPtr));

 if (bytes >= abSize) bytes = abSize;  // max limit is physical buffer size
 bytes = bytes & ALIGN_FILL_CAPTURE;   // align now, after above
 if (bytes == 0) goto ExitNow;         // shouldn't happen, unless abSpace() is 0, which it won't

 if ((startOffset + bytes) > abSize) {

    ULONG diff = abSize - startOffset;

    MEMCPY(dataPtr, audioBufferPtr->bufferPtr+startOffset, diff);
    MEMCPY(dataPtr+diff, audioBufferPtr->bufferPtr, bytes-diff);
 }
 else {
    MEMCPY(dataPtr, audioBufferPtr->bufferPtr+startOffset, bytes);
 }

 audioBufferPtr->bufferBytes = audioBufferPtr->bufferBytes + bytes;

ExitNow:
 return bytes;
}


// -------------------------------------
// in: fillWith = word to use as filler
//     audioBufferPtr
//out: n/a
//nts: seems to be no case where filler is not byte-symetrical, nevertheless,
//     MEMSET() will write full words to buffer so -could- send 7FFF as a 16-bit filler
//     idea here is to fill buffer with silence data, for as much as there is room

VOID abFill(USHORT fillWith, AUDIOBUFFER *audioBufferPtr) {

 ULONG bytes = abSpace(audioBufferPtr);
 ULONG startOffset = GetStartOffset(audioBufferPtr);

 // if doing a capture the value returned by abSpace() is the data in the buffer ready to
 // be copied out -- therefore, the amount to fill is that amount subtracted from the buffer size

 if (audioBufferPtr->mode == AUDIOBUFFER_READ) bytes = audioBufferPtr->bufferSize - bytes;

 MEMSET(audioBufferPtr->bufferPtr+startOffset, fillWith, bytes);

 return;
}


// -------------------------------------
// in: audioBufferPtr
//out: n/a
//nts: destructor
//     free the GDT first?  probably doesn't matter

VOID abDeinit(AUDIOBUFFER *audioBufferPtr) {

 DevHelp_FreePhys(audioBufferPtr->bufferPhysAddrRaw);
 DevHelp_FreeGDTSelector(audioBufferPtr->sel);

 return;
}


#pragma code_seg ("_INITTEXT");
#pragma data_seg ("_INITDATA","ENDDS");

// -------------------------------------
// in: bufferSize = size of DMA to allocate
//     pageSize = size of page for hardware (DMA or PCI, so probably different if PCI?)
//     dma channel = for this audio buffer
//     audioBufferPtr
//out: n/a
//nts: constructor
//     sets things up ... give her a feather she's a cheroke
//     careful not to call this when really mean to call abReset()
//     originally passed flags as arg, not dmaChannel

USHORT abInit(ULONG bufferSize, ULONG pageSize, USHORT dmaChannel, AUDIOBUFFER *audioBufferPtr) {

 USHORT rc = 0;
 USHORT flags;
 ULONG tBufferSize, tBufferStart, tBufferEnd;

 audioBufferPtr->bufferSize = bufferSize;    // initial use, can vary per stream (ab.bufferSize)

 switch(dmaChannel) {
 case 0:
 case 1:
 case 3:
    flags = AUDIOBUFFER_ISA_DMA8;
    tBufferSize = bufferSize + bufferSize;   // double size
    break;
 case 5:
 case 6:
 case 7:
    flags = AUDIOBUFFER_ISA_DMA16;
    tBufferSize = bufferSize + 0x20000;      // 128KB + requested size
    break;
 case 99:
    flags = AUDIOBUFFER_DDMA;
    tBufferSize = bufferSize;                // actual size
    break;
 default:
    rc = 1;
 }

 if (rc == 0) {

    rc = AllocMem(tBufferSize, audioBufferPtr);

    if (rc == 0) {

       tBufferStart = audioBufferPtr->bufferPhysAddr;

       if (flags == AUDIOBUFFER_ISA_DMA8) { // check if fits wholly in 64K page
          tBufferEnd = tBufferStart + bufferSize;
          if ((tBufferEnd >> 16) != (tBufferStart >> 16)) tBufferStart = (tBufferEnd & 0xFFFF0000);
          audioBufferPtr->bufferPhysAddr = tBufferStart;
       }
       else if (flags == AUDIOBUFFER_ISA_DMA16) { // force start on a 128K page
          audioBufferPtr->bufferPhysAddr = (audioBufferPtr->bufferPhysAddr + bufferSize) & 0xFFFE0000;
       }
       //else if (flags == AUDIOBUFFER_DDMA) {
       //   audioBufferPtr->bufferPhysAddr already set okay
       //}

       // allocate  a GDT

       rc = DevHelp_AllocGDTSelector(&audioBufferPtr->sel,1);

       if (rc == 0) {

          // map the physical memory to the GDT

          rc = DevHelp_PhysToGDTSelector(audioBufferPtr->bufferPhysAddr,
                                         (USHORT)audioBufferPtr->bufferSize,
                                         audioBufferPtr->sel);

          if (rc == 0) audioBufferPtr->bufferPtr = MAKEP(audioBufferPtr->sel,0);
       }
    }
 }

 if (rc) {
    if (audioBufferPtr->bufferPhysAddrRaw) {
       DevHelp_FreePhys(audioBufferPtr->bufferPhysAddrRaw);
       audioBufferPtr->bufferPhysAddrRaw = 0;
    }
    if (audioBufferPtr->sel) {
       DevHelp_FreeGDTSelector(audioBufferPtr->sel);
       audioBufferPtr->sel = 0;
    }
 }

 audioBufferPtr->deviceBytes = 0; // should already be
 audioBufferPtr->bufferBytes = 0; // should already be

 return rc;
 pageSize;
}



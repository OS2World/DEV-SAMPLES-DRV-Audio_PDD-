//
// dma.c
// 25-Jan-99
//
// ULONG  dmaQueryDelta(AUDIOBUFFER *audioBufferPtr);
// VOID   dmaWaitForChannel(USHORT count, AUDIOBUFFER *audioBufferPtr);
// VOID   dmaStart(AUDIOBUFFER *audioBufferPtr);
// VOID   dmaStop(AUDIOBUFFER *audioBufferPtr);
// USHORT dmaSetModeType(AUDIOBUFFER *audioBufferPtr, USHORT mode);
// USHORT dmaInit(USHORT dmaChannel, USHORT dmaType, AUDIOBUFFER *audioBufferPtr);
// USHORT dmaDeinit(AUDIOBUFFER *audioBufferPtr);

#include "cs40.h"

//typedef struct _DMACHANNEL_INFO {
// USHORT portAddr;       //  0
// USHORT portCount;      //  2
// USHORT portPage;       //  4
// USHORT portMask;       //  6
// USHORT portMode;       //  8
// USHORT portFlipFlop;   // 10
// USHORT portStatus;     // 12
// UCHAR  maskStatus;     // 14
// UCHAR  maskEnable;     // 15
// UCHAR  maskDisable;    // 16
// UCHAR  typeFdma;       // 17
// USHORT rsv18;          // 18
//} DMACHANNEL_INFO;      // 20 bytes
//
//typedef struct _DMACHANNEL {
// USHORT status;         //  0
// USHORT ch;             //  2 0-3, 5-7
// USHORT type;           //  4 DMA_TYPE_*
// UCHAR  mode;           //  6 data for DMA mode register
// UCHAR  page;           //  7
// UCHAR  addrHi;         //  8
// UCHAR  addrLo;         //  9
// UCHAR  countHi;        // 10
// UCHAR  countLo;        // 11
// DMACHANNEL_INFO chInfo;// 12 setup data (20 bytes)
// ULONG  audioBufferSize;// 32
// ULONG  lastPosition;   // 36
//} DMACHANNEL;           // 40 bytes

static DMACHANNEL_INFO dmaInfo[8] = {
 {0x00,0x01,0x87,0x0A,0x0B,0x0C,0x08, 0x10,0x00,0x04,0,0},
 {0x02,0x03,0x83,0x0A,0x0B,0x0C,0x08, 0x20,0x01,0x05,0,0},
 {0x04,0x05,0x81,0x0A,0x0B,0x0C,0x08, 0x40,0x02,0x06,0,0},
 {0x06,0x07,0x82,0x0A,0x0B,0x0C,0x08, 0x80,0x03,0x07,0,0},
 {0xC0,0xC2,0x8F,0xD4,0xD6,0xD8,0xD0, 0x10,0x00,0x04,0,0},
 {0xC4,0xC6,0x8B,0xD4,0xD6,0xD8,0xD0, 0x20,0x01,0x05,0,0},
 {0xC8,0xCA,0x89,0xD4,0xD6,0xD8,0xD0, 0x40,0x02,0x06,0,0},
 {0xCC,0xCE,0x8A,0xD4,0xD6,0xD8,0xD0, 0x80,0x03,0x07,0,0},
};


// type-F DMA has bits7/6 = 0, so instead of 0x5x for dma???Mode[ch], use 0x1x
// selectively modified at runtime

static UCHAR dmaPlayMode[]   = {0x58,0x59,0x5A,0x5B,0x58,0x59,0x5A,0x5B};
static UCHAR dmaCaptureMode[]= {0x54,0x55,0x56,0x57,0x54,0x55,0x56,0x57};


//-------------------
// in: audioBufferPtr
//out: change in data produced/consumed
//nts: .lastPosition is set to 0 at start

ULONG dmaQueryDelta(AUDIOBUFFER *audioBufferPtr) {

 ULONG delta, lastPos, pos;
 USHORT count;
 USHORT countLo, countHi;  // use USHORT rather than UCHAR so can << 8

 lastPos = audioBufferPtr->dmaCh.lastPosition;

 //NOTE: if autoinit mode then count register will NEVER read FFFF since it gets
 //      reloaded with the base count -- that means count reaches 0 as "lowest"
 //      -- here's a sample frequency distribution over 8192 trips through a 32KB
 //      dma buffer, where for x=n, x is the count value and n is the freq of that count
 //      (so, count reg=0 came up 60 times out of 8192...interesting that 7FFE is 0...
 //      ...which is probably caused by IRQ handler being active, see second freqdist)
 //
 //      irq on:  0=60, 1=72, 2=45, 3=259, 7FFE=0,  7FFF=45, 8000=0, FFFF=0
 //      irq off: 0=62, 1=72, 2=46, 3=259, 7FFE=55, 7FFF=248, 8000=0, FFFF=0
 //
 //      if it were a random event (it's not!), expect count for each
 //      would be 0.25 (8192 div 32768) -- see bottom of this file for count test tracker
 //
 // this gets the current position of the channel, from 0 to size of dma buffer-1
 // the port count register is decremented after each xfer
 // it goes from 0 to FFFF at the last byte to xfer (that's why it's setup with count-1)

 _cli_();
 outp(audioBufferPtr->dmaCh.chInfo.portFlipFlop, 0);  // set flipflop
 countLo = inp(audioBufferPtr->dmaCh.chInfo.portCount);
 countHi = inp(audioBufferPtr->dmaCh.chInfo.portCount);
 _sti_();

 count = (countHi << 8) + countLo;

 if (audioBufferPtr->dmaCh.ch > 3) count = count + count;  // count as words if 16-bit channel

 pos = audioBufferPtr->dmaCh.audioBufferSize - (ULONG)count;

 if (pos >= lastPos) {
    delta = pos - lastPos;
 }
 else {
    delta = (audioBufferPtr->dmaCh.audioBufferSize - lastPos) + pos;
 }

 audioBufferPtr->dmaCh.lastPosition = pos;

 return delta;
}


// ----------------------------------------
// in: wait = uSecs to wait  (micro seconds)
//     audioBufferPtr
//out: n/a
//nts: wait for the DRQ bit to turn off (de-assert) on this DMA channel
//     since some DMA devices won't de-assert their DRQ line if they are stopped while active
//
//     might want to count number of loops spent here...?  but won't solve anything

VOID dmaWaitForChannel(USHORT count, AUDIOBUFFER *audioBufferPtr) {

 USHORT tPort = audioBufferPtr->dmaCh.chInfo.portStatus;
 UCHAR tMask = audioBufferPtr->dmaCh.chInfo.maskStatus;
 UCHAR tByte = inp(tPort);

 while ((tByte & tMask) && count) {
    count--;
    iodelay(WAIT_1US);  // was DMA_IO_WAIT (which is the same thing: 2)
    tByte = inp(tPort);
 }

 return;
}


// ----------------------------------------
// in: audioBufferPtr
//out: n/a
//nts: starts the DMA channel (info setup in InitDMA())

VOID dmaStart(AUDIOBUFFER *audioBufferPtr) {

 UCHAR mode;

 audioBufferPtr->dmaCh.lastPosition = 0;

 mode = audioBufferPtr->dmaCh.mode;
 if (audioBufferPtr->dmaCh.chInfo.typeFdma) mode = mode & 0x3F;  // use demand mode if typeF DMA

 _cli_();
 outp(audioBufferPtr->dmaCh.chInfo.portMask, audioBufferPtr->dmaCh.chInfo.maskDisable); // disable channel
 outp(audioBufferPtr->dmaCh.chInfo.portMode, mode);  // set mode
 outp(audioBufferPtr->dmaCh.chInfo.portPage, audioBufferPtr->dmaCh.page); // set page
 outp(audioBufferPtr->dmaCh.chInfo.portFlipFlop, 0); // set flip-flop
 outp(audioBufferPtr->dmaCh.chInfo.portAddr, audioBufferPtr->dmaCh.addrLo); // set low address
 outp(audioBufferPtr->dmaCh.chInfo.portAddr, audioBufferPtr->dmaCh.addrHi); // set high
 outp(audioBufferPtr->dmaCh.chInfo.portFlipFlop, 0); // set flip-flop
 outp(audioBufferPtr->dmaCh.chInfo.portCount, audioBufferPtr->dmaCh.countLo); // set low count
 outp(audioBufferPtr->dmaCh.chInfo.portCount, audioBufferPtr->dmaCh.countHi); // set high
 outp(audioBufferPtr->dmaCh.chInfo.portMask, audioBufferPtr->dmaCh.chInfo.maskEnable); // enable channel
 _sti_();

 return;
}


// ----------------------------------------
// in: audioBufferPtr
//out: n/a
//nts: stops the DMA channel

VOID dmaStop(AUDIOBUFFER *audioBufferPtr) {

 outp(audioBufferPtr->dmaCh.chInfo.portMask, audioBufferPtr->dmaCh.chInfo.maskDisable); // disable channel

 return;
}


// ----------------------------------------
// in: audioBufferPtr
//     mode  0=single, 1=demand
//out: last mode type
//nts: sets DMA mode type (demand mode or single mode)

USHORT dmaSetModeType(AUDIOBUFFER *audioBufferPtr, USHORT mode) {

 USHORT lastType = audioBufferPtr->dmaCh.chInfo.typeFdma;

 if (mode) {
    audioBufferPtr->dmaCh.chInfo.typeFdma = 1;
 }
 else {
    audioBufferPtr->dmaCh.chInfo.typeFdma = 0;
 }

 return lastType;
}


// ----------------------------------------
// in: channel = 0-3,5-7
//     dmaType = see cs40.h (also comes in with special flag here: bit8=1 to use type F dma)
//     audioBufferPtr
//out:
//nts: dma constructor
//     must be sure to always have dmaType piggy-backed with DMA_TYPE_FTYPE bit flag set as needed
//     called more than just from init

USHORT dmaInit(USHORT dmaChannel, USHORT dmaType, AUDIOBUFFER *audioBufferPtr) {

 ULONG bufferPhysAddr, count;
 USHORT typeFdma;

 typeFdma = dmaType & DMA_TYPE_FTYPE;   // bit8=1
 dmaType = dmaType & ~DMA_TYPE_FTYPE;

 if ((dmaType & DMA_TYPE_ISA) == 0) return 87;  // whatever
 if (dmaChannel > 7 || dmaChannel == 2 || dmaChannel == 4) return 87;

 audioBufferPtr->dmaCh.ch = dmaChannel;
 audioBufferPtr->dmaCh.type = dmaType;

 audioBufferPtr->dmaCh.chInfo = dmaInfo[dmaChannel]; // whoa! structure copy

 if ((dmaType & DMA_TYPE_DIRECTION) == DMA_TYPE_CAPTURE) {
    audioBufferPtr->dmaCh.mode = dmaCaptureMode[dmaChannel];
 }
 else {
    audioBufferPtr->dmaCh.mode = dmaPlayMode[dmaChannel];
 }

 // doing demand mode DMA instead of single improves performance a lot:
 //    55% CPU use to 28% on 486
 // this picked up each time dma is started, so can be changed at runtime
 // just by chaning abPtr->dmaCh.chInfo.typeFdma (via dmaSetModeType())
 // may also want to look at chipsetSetDTM(USHORT dtmFlag)

 dmaSetModeType(audioBufferPtr, typeFdma);

 // while it seems redundant to have both dmaCh.audioBufferSize and ab.bufferSize (and ws.bufferSize)
 // it serves its purpose since dmaCh.audioBufferSize is only set here and so
 // I can call here to re-size logical buffer size

 count = audioBufferPtr->bufferSize;
 audioBufferPtr->dmaCh.audioBufferSize = count;

 if (dmaChannel > 3) {
    count = count >> 1;  // word count (apparently don't need to -1 as do with 8 bit DMA channel)
 }
 else {
    count = count - 1;
 }

 audioBufferPtr->dmaCh.countHi = (UCHAR)(count >> 8);
 audioBufferPtr->dmaCh.countLo = (UCHAR)count;

 bufferPhysAddr = audioBufferPtr->bufferPhysAddr;

 audioBufferPtr->dmaCh.page = (UCHAR)(bufferPhysAddr >> 16);
 audioBufferPtr->dmaCh.addrHi = (UCHAR)(bufferPhysAddr >> 8);
 audioBufferPtr->dmaCh.addrLo = (UCHAR)bufferPhysAddr;

 return 0;
}


// ----------------------------------------
// in: audioBufferPtr
//out:
//nts: dma destructor
//     assumes is running...probably doesn't matter if it isn't

USHORT dmaDeinit(AUDIOBUFFER *audioBufferPtr) {

 dmaStop(audioBufferPtr);

 return 0;
}


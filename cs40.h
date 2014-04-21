//
// cs40.h
// 25-Jan-99
//

#ifndef CS40_INCL

//xx #define NO_INT3   // define to change int3() into a nop

#define  INCL_BASE
#define  INCL_NOPMAPI

#include <os2.h>
#include <os2medef.h>
#include <meerror.h>
#include <ssm.h>
#include <shdd.h>
#include <audio.h>

#include "wdevhelp.h"
#include "whelper.h"
#include "include.h"
#include "traceid.h"

#define DRIVER_VERSION 0x01000001  // 1.0 build 0001 (MMmmBBBB) returned in audio init ioctl



// -------------------------------------
// structures, types, defines
// -------------------------------------

typedef struct _REQPACK {
 UCHAR   length;                // 00 request packet length
 UCHAR   unit;                  // 01 unit code for block DD only
 UCHAR   command;               // 02 command code
 USHORT  status;                // 03 status word
 UCHAR   rsv5[4];               // 05 reserved bytes
 ULONG   qLink;                 // 09 queue linkage
 union {                        //    command-specific data
  UCHAR  avail[19];             // 13 available space, in 32-byte packet

  struct {                      //    INIT Packet (one for entry, one for exit)
   UCHAR  units;                // 13 number of units
   union {
    struct {
     PFN Device_Help;           // 14 used at entry
    };
    struct {
     USHORT sizeCS;             // 14 final code offset (used at exit)
     USHORT sizeDS;             // 16 final data offset
    };
   };
   UCHAR  __far *argsPtr;       // 18 -> args
   UCHAR  drive;                // 22 drive number
  } init;

  struct {                      //    OPEN/CLOSE
   USHORT SFN;                  // 13 sfn
  } open;

  struct {                      //    IOCTL
   UCHAR  category;             // 13 category code
   UCHAR  function;             // 14 function code
   VOID __far *parmPtr;         // 15 -> parm data
   VOID __far *dataPtr;         // 19 -> data
   USHORT SFN;                  // 23 from kernel, pass back in Audio_Init
   USHORT parmBuffLen;          // 25
   USHORT dataBuffLen;          // 27
  } ioctl;
 }; // unnamed (was s;)
} REQPACK;


// ------------------------------------
// header.c, structures, defines, types

#define RPDONE    0x0100         // return successful, must be set
#define RPBUSY    0x0200         // device is busy (or has no data to return)
#define RPDEV     0x4000         // user-defined error
#define RPERR     0x8000         // return error

// List of error codes, from chapter 8 of PDD reference
#define RPNOTREADY  0x0002
#define RPBADCMD    0x0003
#define RPGENFAIL   0x000c
#define RPDEVINUSE  0x0014
#define RPINITFAIL  0x0015

// list of Strategy commands from PDD reference
// Note this is only the list of commands audio device drivers care about

#define STRATEGY_INIT          0x00
#define STRATEGY_OPEN          0x0D
#define STRATEGY_CLOSE         0x0E
#define STRATEGY_GENIOCTL      0x10
#define STRATEGY_DEINSTALL     0x14
#define STRATEGY_INITCOMPLETE  0x1F

#define DA_CHAR         0x8000   // Character PDD
#define DA_IDCSET       0x4000   // IDC entry point set
#define DA_BLOCK        0x2000   // Block device driver
#define DA_SHARED       0x1000   // Shared device (filesystem share-aware)
#define DA_NEEDOPEN     0x800    // Open/Close required

#define DA_OS2DRVR      0x0080   // Standard OS/2 driver
#define DA_IOCTL2       0x0100   // Supports IOCTL2
#define DA_USESCAP      0x0180   // Uses Capabilities bits

#define DA_CLOCK        8        // Clock device
#define DA_NULL         4        // NULL device
#define DA_STDOUT       2        // STDOUT device
#define DA_STDIN        1        // STDIN device

#define DC_INITCPLT     0x10     // Supports Init Complete
#define DC_ADD          8        // ADD driver
#define DC_PARALLEL     4        // Supports parallel ports
#define DC_32BIT        2        // Supports 32-bit addressing (rather than 24-bit)
#define DC_IOCTL2       1        // Supports DosDevIOCtl2 and Shutdown (1C)

typedef VOID (__near *PFNENTRY)(VOID);  // strat/IDC entry cc

#pragma pack(1); // not needed

typedef struct _DEV_HEADER {
 ULONG nextDD;
 USHORT attr;
 PFNENTRY pfnStrategy;
 PFNENTRY pfnIDC;
 CHAR name[8];
 ULONG reserved[2];
 ULONG caps;
} DEV_HEADER;

#pragma pack();

// pseudo-variable that points to device header
//#define phdr ((DEV_HEADER *) 0)


// -------------------------------------
// chipset.c, structures, types, defines


// ---------------------------------
// dma.c, structures, types, defines
//

#define DMA_TYPE_PLAY      0x0000  // (dma-read)
#define DMA_TYPE_CAPTURE   0x0001  // (dma-write)
#define DMA_TYPE_DIRECTION 0x0001  // used to AND with PLAY/CAPTURE types
#define DMA_TYPE_WORD_SZ   0x0006
#define DMA_TYPE_8BIT      0x0000  // 8 bit DMA channel
#define DMA_TYPE_16BIT     0x0002  // 16 bit DMA channel
#define DMA_TYPE_ISA       0x0008  // DMA Channel of an 8237A
#define DMA_TYPE_DDMA      0x0010  // Distributed DMA Channel
#define DMA_TYPE_FTYPE     0x0100  // hardware supports F type DMA (demand mode for the 8237)

//#define DMA_IO_WAIT 2    // IO wait time between IO DMA regs (2 * 500ns = 1us) (just use WAIT_1US)

typedef struct _DMACHANNEL_INFO {
 USHORT portAddr;       //  0
 USHORT portCount;      //  2
 USHORT portPage;       //  4
 USHORT portMask;       //  6
 USHORT portMode;       //  8
 USHORT portFlipFlop;   // 10
 USHORT portStatus;     // 12
 UCHAR  maskStatus;     // 14
 UCHAR  maskEnable;     // 15
 UCHAR  maskDisable;    // 16
 UCHAR  typeFdma;       // 17
 USHORT rsv18;          // 18
} DMACHANNEL_INFO;      // 20 bytes

typedef struct _DMACHANNEL {
 USHORT status;         //  0
 USHORT ch;             //  2 0-3, 5-7
 USHORT type;           //  4 DMA_TYPE_*
 UCHAR  mode;           //  6 data for DMA mode register
 UCHAR  page;           //  7
 UCHAR  addrHi;         //  8
 UCHAR  addrLo;         //  9
 UCHAR  countHi;        // 10
 UCHAR  countLo;        // 11
 DMACHANNEL_INFO chInfo;// 12 setup data (20 bytes)
 ULONG  audioBufferSize;// 32 set to AUDIOBUFFER.bufferSize at dmaInit()
 ULONG  lastPosition;   // 36
} DMACHANNEL;           // 40 bytes


// --------------------------------------
// audiobuf.c, structures, types, defines

// values for the buffer mode

#define AUDIOBUFFER_READ  0             // the device is doing a record
#define AUDIOBUFFER_WRITE 1             // the device is doing a playback
//#define AUDIOBUFFER_BAD_INIT -1         // Error during constructor

#define AUDIOBUFFER_ALIGN_PAGE 0x01     // the buffer Must be ailgned on a page
#define AUDIOBUFFER_CROSS_PAGE 0x02     // the buffer May cross a page boundary
#define AUDIOBUFFER_NO_PAGE    0x04     // the buffer has no page requirements
#define AUDIOBUFFER_ISA_DMA8   0x08     // the buffer will be used by an 8 bit DMA
#define AUDIOBUFFER_ISA_DMA16  0x10     // the buffer will be used by a 16 bit DMA
#define AUDIOBUFFER_DDMA       0x20     // Distributed DMA Buffer


typedef struct _AUDIOBUFFER {
 USHORT mode;                // 00 status mode (success or not)
 SEL    sel;                 // 02 selector from Phys->GDT
 UCHAR __far *bufferPtr;     // 04 16:16 GDT pointer to buffer (MAKEP() on the Phys->GDT selector:0
 ULONG  bufferPhysAddrRaw;   // 08 unaligned base address (used to free it)
 ULONG  bufferPhysAddr;      // 12 aligned base address (actually used)
 ULONG  bufferSize;          // 16 size of DMA buffer currently using (logical size, <= max size)
 ULONG  bufferSizeMax;       // 20 MAX size of DMA buffer (original allocation)
 ULONG  deviceBytes;         // 24 bytes consumed or produced by device
 ULONG  bufferBytes;         // 28 bytes read or written from buffer
 DMACHANNEL dmaCh;           // 32 dma channel (40 bytes)
} AUDIOBUFFER;               // 72 bytes


// --------------------------------------
// audiohw.c, structures, types, defines

#define AUDIOHW_INVALID_DEVICE     0xFFFF  // audio hardware device types (was ULONG)
#define AUDIOHW_READ_DEVICE        0x0000
#define AUDIOHW_WRITE_DEVICE       0x0001
#define AUDIOHW_WAVE_CAPTURE       0x0010
#define AUDIOHW_WAVE_PLAY          0x0011
#define AUDIOHW_FMSYNTH_CAPTURE    0x0020
#define AUDIOHW_FMSYNTH_PLAY       0x0021
#define AUDIOHW_MPU401_CAPTURE     0x0040
#define AUDIOHW_MPU401_PLAY        0x0041
#define AUDIOHW_TIMER              0x0080

// regarding lDev: MMPM/2 will not send requests for two like devices to
// the same driver, but it can be "fooled" into thinking there are two drivers,
// each capable of one of the said devices.. (pft!)

typedef struct _AUDIOHW_T2DLIST {
 USHORT devType;        // 00 device type (audiohw.c device type, that is (see above))
 USHORT lDev;           // 02 strategy entry that this request came from
 USHORT dataType;       // 04 data type from MMPM/2 (see audio/h/os2medef.h)
 USHORT opType;         // 06 the operation type (see audio.h AUDIO_INIT.ulOperation)
} AUDIOHW_T2DLIST;      //  8 bytes


// ----------------------------------
// init.c, structures, types, defines


// -----------------------------------
// ioctl.c, structures, types, defines


// ---------------------------------
// irq.c, structures, types, defines


// ------------------------------------
// strat.c, structures, types, defines
// see traceid.h include above


// ------------------------------------
// stream.c, structures, types, defines

#define STREAM_RW_MODE  (STREAM_WRITE | STREAM_READ)  // STREAM_WRITE = 1, STREAM_READ = 0

typedef ULONG (__far __cdecl *T_PFNSHD)(VOID __far *);

// the point of this is to map a stream type to a hardware type (double-plus good)
// so use the STREAM_* define in stream-related searches and the AUDIOHW_* define if hw related

#define STREAM_READ             AUDIOHW_READ_DEVICE
#define STREAM_WRITE            AUDIOHW_WRITE_DEVICE
#define STREAM_WAVE_CAPTURE     AUDIOHW_WAVE_CAPTURE
#define STREAM_WAVE_PLAY        AUDIOHW_WAVE_PLAY
#define STREAM_FMSYNTH_CAPTURE  AUDIOHW_FMSYNTH_CAPTURE
#define STREAM_FMSYNTH_PLAY     AUDIOHW_FMSYNTH_PLAY
#define STREAM_MPU401_CAPTURE   AUDIOHW_MPU401_CAPTURE
#define STREAM_MPU401_PLAY      AUDIOHW_MPU401_PLAY

#define STREAM_STOPPED     0x00   // stream states (originally was ULONG)
#define STREAM_STREAMING   0x01
#define STREAM_PAUSED      0x02
#define STREAM_IDLE        0x04
#define STREAM_NOT_IDLE    0xFB

typedef struct _STREAM_BUFFER {
 struct _STREAM_BUFFER *nextPtr;// 00
 USHORT rsv2;                   // 02
 UCHAR __far *bufferPtr;        // 04 -> stream buffer
 ULONG bufferSize;              // 08 size of stream buffer (-not- dma related)
 ULONG bufferCurrPos;           // 12 current position
 ULONG bufferDonePos;           // 16 position at which buffer can be returned
} STREAM_BUFFER;                // 20 bytes
typedef STREAM_BUFFER *PSTREAM_BUFFER;

typedef struct _STREAM_BUFFER_ANCHOR { // was "STREAM_BUFFER_HEAD" but anchor makes more sense
 STREAM_BUFFER *headPtr;
 STREAM_BUFFER *tailPtr;
} STREAM_BUFFER_ANCHOR;
typedef STREAM_BUFFER_ANCHOR *PSTREAM_BUFFER_ANCHOR;

typedef struct _STREAM {
 struct _STREAM *nextPtr;       // 00 next stream on queue
 USHORT rsv2;                   // 02
 HSTREAM streamH;               // 04
 T_PFNSHD pfnSHD;               // 08 is "typedef ULONG (__far __cdecl *PFN_SHD)(VOID __far *)"
 USHORT SFN;                    // 12
 USHORT rsv14;                  // 14 reserved (SFN is 16-bit value down here, in reqpack too)
 STREAM_BUFFER_ANCHOR sbaProc;  // 16 head for STREAM_BUFFER in-process queue anchor (2 x 2 bytes)
 STREAM_BUFFER_ANCHOR sbaDone;  // 20 head for STREAM_BUFFER done queue anchor
 STREAM_BUFFER_ANCHOR sbaEvent; // 24 head for STREAM_BUFFER event queue anchor
 USHORT streamType;             // 28 eg,STREAM_WAVE_CAPTURE (==AUDIOHW_WAVE_CAPTURE)(org ULONG)
 UCHAR  streamState;            // 30
 UCHAR  counterIncFlag;         // 31 true if current time should be inc'ed every tick
 ULONG  currentTime;            // 32 current time...
 struct _WAVESTREAM *wsParentPtr;//36 pointer to parent wavestream (1-Feb-99, see below[1])
} STREAM;                       // 38 bytes
typedef STREAM *PSTREAM;
// [1] need this link to the parent (*wsParentPtr) since stream.c calls to
// wavestreamGetCurrentTime() which needs WAVESTREAM, not STREAM, pointer ... simple enough
// -- this is also the only place to track wavestream pointers...since not stored anywhere else
// see wavestreamInit() for more

typedef struct _STREAM_ANCHOR { // was "STREAM_HEAD" but anchor makes more sense
 STREAM *headPtr;
 STREAM *tailPtr;
} STREAM_ANCHOR;
typedef STREAM_ANCHOR *PSTREAM_ANCHOR;


// --------------------------------------
// wavaudio.c, structures, types, defines

#define DMA_BUFFER_SIZE ((ULONG)0xF000) // physical DMA buffer size (always 60KB)

// cs4232 specific defines

#define FORMAT_BIT0     0x40
#define CL_BIT          0x20
#define STEREO_BIT      0x10

typedef struct _WAVECONFIG {
 USHORT sampleRate;     // 00 I:sample rate (was ULONG)
 USHORT silence;        // 02 I:data for silence
 UCHAR  bitsPerSample;  // 04 I:bits per sample (was ULONG)
 UCHAR  channels;       // 05 I:channels (was ULONG)
 USHORT dataType;       // 06 I:data type (PCM, muLaw...) (was ULONG)
 ULONG  consumeRate;    // 08 O:bytes consumed/produced each sec
 ULONG  bytesPerIRQ;    // 12 O:bytes consumed/produced per IRQ
} WAVECONFIG;           // 16 bytes
typedef WAVECONFIG *PWAVECONFIGINFO;

#define FLAGS_WAVEAUDIO_FULLDUPLEX  1   // can do full-duplex (has separate DMA for play/rec)
#define FLAGS_WAVEAUDIO_DMA16       2   // dma channel is 16-bit
#define FLAGS_WAVEAUDIO_FTYPEDMA    4   // hardware support demand-mode dma

// WAVEAUDIO has just about everything needed for a device
// (one WAVEAUDIO for a PLAY device, another for a RECORD device)
// removed dmaCh/dmaChSec since don't really need them (dmaCh is already in AUDIOBUFFER *abPtr)
// and since that makes the code just more hardware specific

// AUDIOHW *ahwPtr;       // 02 has devType, and function pointers (xx bytes)
// AUDIOBUFFER *abPtr;    // 08 yup, -> ab structure  (72 bytes) alloc in waSetup()

typedef struct _WAVEAUDIO {
 USHORT flags;          // 00 see FLAGS_* above
 USHORT devType;        // 02 AUDIOHW_* device type (was  AUDIOHW *ahwPtr;)
 USHORT irq;            // 04 irq level
 NPFN irqHandlerPtr;    // 06 irq handler function pointer (uses offset only, always CS-relative)
 UCHAR  clockSelectData;// 08 hardware-specific clock select data (ix8)
 UCHAR  formatData;     // 09 hardware-specific format data (ix8/28)
 USHORT countData;      // 10 hardware-specific count data (ix14-15/30-31)
 AUDIOBUFFER ab;        // 12 (72 bytes) (was  AUDIOBUFFER *abPtr, once allocated in waSetup()
} WAVEAUDIO;            // 84 bytes
typedef WAVEAUDIO *PWAVEAUDIO;


// --------------------------------------
// wavestrm.c, structures, types, defines

#define ALIGN_FILL_PLAY    0xFFFFFC00   // original was 0xFFFFFFFC
#define ALIGN_FILL_CAPTURE 0xFFFFFC00   // original was 0xFFFFFFFC

typedef struct _WAVESTREAM {    // streamPtr->wsParentPtr is only way to find this
 STREAM *streamPtr;             // 00
 WAVEAUDIO *waPtr;              // 02
 WAVECONFIG waveConfig;         // 04 (16 bytes)
 USHORT audioBufferSize;        // 20 logical DMA buffer size for THIS wavestream! (tbi)
 USHORT audioBufferInts;        // 22 interrupts per buffer (def=0, which is two per buffer)
 ULONG  bytesProcessed;         // 24
 ULONG  timeBase;               // 28 in ms, MMPM/2 send for stream time...
} WAVESTREAM;                   // 32 bytes
typedef WAVESTREAM *PWAVESTREAM;


// --------------------------------------
// waveplay.c, structures, types, defines

extern WAVEAUDIO wap;


// -------------------------------------
// waverec.c, structures, types, defines

extern WAVEAUDIO war;


// -------------------------------------
// protos
// -------------------------------------


// -----------------
// audiobuf.c protos

VOID   abReset(USHORT mode, AUDIOBUFFER *audioBufferPtr);
ULONG  abSpace(AUDIOBUFFER *audioBufferPtr);
ULONG  abBytes(AUDIOBUFFER *audioBufferPtr);
ULONG  abUpdate(USHORT flags, AUDIOBUFFER *audioBufferPtr);
ULONG  abWrite(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr);
ULONG  abRead(UCHAR __far *dataPtr, ULONG dataSize, AUDIOBUFFER *audioBufferPtr);
VOID   abFill(USHORT fillWith, AUDIOBUFFER *audioBufferPtr);
VOID   abDeinit(AUDIOBUFFER *audioBufferPtr);
USHORT abInit(ULONG bufferSize, ULONG pageSize, USHORT dmaChannel, AUDIOBUFFER *audioBufferPtr);


// ----------------
// audiohw.c protos

USHORT   hwGetType(USHORT dataType, USHORT opType, USHORT lDev);
USHORT   hwSetType(USHORT devType, USHORT dataType, USHORT opType, USHORT lDev);


// ----------------
// chipset.c protos

UCHAR  chipsetGET(USHORT type, USHORT reg);  // upper-case GET to spot from chipsetSet
VOID   chipsetSet(USHORT type, USHORT reg, UCHAR data);
VOID   chipsetMCE(USHORT mode);
USHORT chipsetSetDTM(USHORT dtmFlag);
USHORT chipsetInit(USHORT bp, USHORT cp, USHORT mode, USHORT make);
USHORT chipsetIntPending(USHORT type);
VOID   chipsetIntReset(USHORT type);
UCHAR  chipsetWaitInit(VOID);
UCHAR  chipsetWaitACI(VOID);


// ------------
// dma.c protos

ULONG  dmaQueryDelta(AUDIOBUFFER *audioBufferPtr);
VOID   dmaWaitForChannel(USHORT count, AUDIOBUFFER *audioBufferPtr);
VOID   dmaStart(AUDIOBUFFER *audioBufferPtr);
VOID   dmaStop(AUDIOBUFFER *audioBufferPtr);
USHORT dmaSetModeType(AUDIOBUFFER *audioBufferPtr, USHORT mode);
USHORT dmaInit(USHORT dmaChannel, USHORT dmaType, AUDIOBUFFER *audioBufferPtr);
USHORT dmaDeinit(AUDIOBUFFER *audioBufferPtr);


// -------------
// init.c protos

USHORT stratMode2Init(REQPACK __far *rpPtr);


// ---------------
// ioctl.c protos

VOID ioctlStrat(REQPACK __far *rpPtr, USHORT lDev);


// ------------
// irq.c protos

VOID __far __loadds irqHandler(VOID);
USHORT irqEnable(USHORT irqNo);
USHORT irqDisable(USHORT irqNo);


// --------------
// strat.c protos

ULONG tracePerf(USHORT minor, ULONG data);
VOID  traceCalibrate(VOID);

VOID __far stratMode2(REQPACK __far *rpPtr);


// ----------------
// ssm_idc.c protos

ULONG __far __loadds __cdecl idcDDCMD(DDCMDCOMMON __far *commonPtr);


// ---------------
// stream.c protos

VOID    streamReturnBuffer(STREAM *streamPtr);
VOID    streamReturnBuffers(STREAM *streamPtr);
USHORT  streamPauseTime(STREAM *streamPtr);
USHORT  streamResumeTime(STREAM *streamPtr);
USHORT  streamRegister(DDCMDREGISTER __far *cmdPtr, STREAM *streamPtr);
VOID    streamDeregister(STREAM *streamPtr);
USHORT  streamRead(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr);
USHORT  streamWrite(UCHAR __far *bufferPtr, ULONG length, STREAM *streamPtr);
USHORT  streamInit(USHORT streamType, STREAM *streamPtr);
USHORT  streamDeinit(STREAM *streamPtr);
STREAM *streamFindActive(USHORT streamType);
STREAM *streamFindStreamSFN(USHORT SFN);
STREAM *streamFindStreamHandle(HSTREAM streamH);

STREAM_BUFFER *sbHead(STREAM_BUFFER_ANCHOR *sbaPtr);  // stream buffer queue support
STREAM_BUFFER *sbTail(STREAM_BUFFER_ANCHOR *sbaPtr);
VOID           sbPushOnHead(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
VOID           sbPushOnTail(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
STREAM_BUFFER *sbPopHead(STREAM_BUFFER_ANCHOR *sbaPtr);
STREAM_BUFFER *sbPopTail(STREAM_BUFFER_ANCHOR *sbaPtr);
USHORT         sbDestroyElement(STREAM_BUFFER *sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
STREAM_BUFFER *sbPopElement(STREAM_BUFFER *match_sbPtr, STREAM_BUFFER_ANCHOR *sbaPtr);
USHORT         sbNotEmpty(STREAM_BUFFER_ANCHOR *sbaPtr);

STREAM *streamHead(VOID);  // stream queue support
STREAM *streamTail(VOID);
VOID    streamPushOnHead(STREAM *sPtr);
VOID    streamPushOnTail(STREAM *sPtr);
STREAM *streamPopHead(VOID);
STREAM *streamPopTail(VOID);
USHORT  streamDestroyElement(STREAM *sPtr);     // doesn't seem to be used
STREAM *streamPopElement(STREAM *match_sPtr);
USHORT  streamNotEmpty(VOID);                   // doesn't seem to be used

// not yet done in stream.c are several event routines...these are temporary stubs

VOID streamProcessEvents(STREAM *streamPtr) ;
VOID streamEnableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr);
VOID streamDisableEvent(DDCMDCONTROL __far *ddcmdPtr, STREAM *streamPtr);
VOID streamSetNextEvent(STREAM *streamPtr);


// -----------------
// wavaudio.c protos

VOID   waDevCaps(MCI_AUDIO_CAPS __far *devCapsPtr);
VOID   waConfigDev(WAVESTREAM *wsPtr);
USHORT waPause(VOID);
USHORT waResume(VOID);
USHORT waSetup(USHORT dmaChannel, WAVEAUDIO *waPtr);


// -----------------
// wavestrm.c protos

VOID    wavestreamProcess(WAVESTREAM *wsPtr);
ULONG   wavestreamGetCurrentTime(WAVESTREAM *wsPtr);
VOID    wavestreamSetCurrentTime(ULONG time, WAVESTREAM *wsPtr);
USHORT  wavestreamStart(WAVESTREAM *wsPtr);
USHORT  wavestreamStop(WAVESTREAM *wsPtr);
USHORT  wavestreamPause(WAVESTREAM *wsPtr);
USHORT  wavestreamResume(WAVESTREAM *wsPtr);
STREAM *wavestreamInit(USHORT streamType, MCI_AUDIO_INIT __far *mciInitPtr, WAVESTREAM *wsPtr);
USHORT  wavestreamDeinit(WAVESTREAM *wsPtr);


// -----------------
// waveplay.c protos

USHORT waveplayStart(WAVESTREAM *wsPtr);
USHORT waveplayStop(WAVESTREAM *wsPtr);
USHORT waveplayEnable(WAVESTREAM *wsPtr);
USHORT waveplayInit(USHORT dmaChannel, USHORT flags, USHORT irq);


// ----------------
// waverec.c protos

USHORT waverecStart(WAVESTREAM *wsPtr);
USHORT waverecStop(WAVESTREAM *wsPtr);
USHORT waverecEnable(WAVESTREAM *wsPtr);
USHORT waverecInit(USHORT dmaChannel, USHORT flags, USHORT irq);


// -----------------
// ddprintf.c protos
// Note: if "%s" then string pointer in the ,... section must be a FAR POINTER!
//       eg, ddprintf(dest,"%s",(char __far *)"a string")  see ras40 source for more

VOID __cdecl ddprintf (char far *St , ...);  // max buffer output is (probably) set to 260 bytes
//VOID ddputstring (char far *St);


// -----------------
// malloc.c protos

VOID _near *malloc(USHORT bytes);
VOID        free(VOID __near *ptr);
VOID _near *realloc(VOID __near *ptr, USHORT bytes);
unsigned    _msize(VOID __near *ptr);
unsigned    _memfree(VOID);
unsigned    HeapInit(USHORT);
void        dumpheap(VOID);


// ----------------
// segments.asm

VOID iodelay(USHORT count);
#pragma aux iodelay parm nomemory [cx] modify nomemory exact [ax cx];



// -------------------------
// hw/chip defines/constants

#define WAIT_1US              2  // 500 ns waits for 1 us
#define WAIT_10US            20  // 500 ns waits for 10 us
#define WAIT_1MS           2000  // 500 ns waits for 1 ms
#define WAIT_2MS           4000  // 500 ns waits for 1 ms
#define WAIT_5MS          10000  // 500 ns waits for 5 ms

#define DMA_WAIT 362

#define ADDRESS_MASK    0xE0
#define IREG_MASK       0x1F
#define MCE_BIT         0x40
#define MCE_ON          MCE_BIT
#define MCE_OFF         ~(MCE_ON)
#define INIT_BIT        0x80
#define ACI_BIT         0x20

#define CALIBRATE_NONE       0x00
#define CALIBRATE_CONVERTERS 0x08
#define CALIBRATE_DACS       0x10
#define CALIBRATE_ALL        0x18

#define PLAY_INTERRUPT       0x10
#define CAPTURE_INTERRUPT    0x20
#define TIMER_INTERRUPT      0x40
#define ALL_INTERRUPTS       0x70
#define CLEAR_PI             0x6F
#define CLEAR_CI             0x5F
#define CLEAR_TI             0x3F

#define ADDRESS_REG     0
#define DATA_REG        1
#define STATUS_REG      2

#define LEFT_ADC_INPUT_CONTROL   0x00
#define RIGHT_ADC_INPUT_CONTROL  0x01
#define LEFT_AUX1_INPUT_CONTROL  0x02
#define RIGHT_AUX1_INPUT_CONTROL 0x03
#define LEFT_AUX2_INPUT_CONTROL  0x04
#define RIGHT_AUX2_INPUT_CONTROL 0x05
#define LEFT_DAC_OUTPUT_CONTROL  0x06
#define RIGHT_DAC_OUTPUT_CONTROL 0x07
#define PLAYBACK_DATA_FORMAT_REG 0x08
#define INTERFACE_CONFIG_REG     0x09
#define PIN_CONTROL_REG          0x0A
#define ERROR_STATUS_INIT_REG    0x0B
#define MODE_AND_ID_REG          0x0C
#define LOOPBACK_CONTROL_REG     0x0D
#define UPPER_PLAYBACK_COUNT     0x0E
#define LOWER_PLAYBACK_COUNT     0x0F
#define ALT_FEATURE_ENABLE_1     0x10
#define ALT_FEATURE_ENABLE_2     0x11
#define LEFT_LINE_INPUT_CONTROL  0x12
#define RIGHT_LINE_INPUT_CONTROL 0x13
#define LOWER_TIMER_COUNT        0x14
#define UPPER_TIMER_COUNT        0x15
#define ALT_SAMPLE_FREQ_SELECT   0x16
#define ALT_FEATURE_ENABLE_3     0x17
#define ALT_FEATURE_STATUS       0x18
#define VERSION_ID_REG           0x19
#define MONO_IO_CONTROL          0x1A
#define LEFT_OUTPUT_CONTROL      0x1B
#define CAPTURE_DATA_FORMAT_REG  0x1C
#define RIGHT_OUTPUT_CONTROL     0x1D
#define UPPER_CAPTURE_COUNT      0x1E
#define LOWER_CAPTURE_COUNT      0x1F


#define CS40_INCL
#endif






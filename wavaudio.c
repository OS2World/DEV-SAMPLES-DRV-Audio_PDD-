//
// wavaudio.c
// 27-Jan-99
//
// VOID   waDevCaps(MCI_AUDIO_CAPS __far *devCapsPtr, WAVEAUDIO *waPtr);
// VOID   waConfigDev(WAVECONFIG *waveConfigPtr, WAVEAUDIO *waPtr);
// USHORT waPause(VOID);
// USHORT waResume(VOID);
// USHORT waSetup(USHORT dmaChannel, WAVEAUDIO *waPtr);
//
// _usfind_matching_sample_rate() and
// _vset_clock_info() only called here, once, so done at caller rather than separate routines

#include "cs40.h"

// local defines for PCM, muLAW, and aLAW tables used by virtual void DevCaps(PAUDIO_CAPS pCaps)

#define NUMFREQS   4
#define BPSTYPES   2
#define MONOSTEREO 2

// local sample rate to clock-select bits

typedef struct _SRT {
 USHORT sampleRate;     // 00 (was ULONG)
 UCHAR  clockSelect;    // 02
 UCHAR  rsv3;           // 03 reserved (can be used as multiplier flag for freq if need freq > 65535)
} SRT;

// This table holds all the sample rates the CS4232 can operate at and the data required
// in bits 0-3 of the FS and Playback Data Format Reg, (indexed register 8) to set them up

static SRT srt[] = {
  5512,0x01,0,  6615,0x0f,0,  8000,0x00,0,  9600,0x0e,0, 11025,0x03,0,
 16000,0x02,0, 18900,0x05,0, 22050,0x07,0, 27428,0x04,0, 32000,0x06,0,
 33075,0x0D,0, 37800,0x09,0, 44100,0x0B,0, 48000,0x0C,0};

#define SRT_COUNT (sizeof(srt)/sizeof(SRT))

// the following 3-D array defines the subtypes for DATATYPE_WAVEFORM
// The array is 4x2x2 and is indexed using frequency index, bits per
// sample being 8 or 16 represented by 0 or 1 resp. and mono or stereo
// mode represented by 0 or 1 respectively. For eg. to find out the
// subtype for 22050Hz sampling frequency, using 16 bits per sample
// and stereo mode, we use waveSubtypes[FREQ22KHZ][BPS16][1].

static USHORT waveSubtypes[NUMFREQS][BPSTYPES][MONOSTEREO] = { // was ULONG
 WAVE_FORMAT_1M08,    // 11.025kHz, 8-bit  Mono
 WAVE_FORMAT_1S08,    // 11.025kHz, 8-bit  Stereo
 WAVE_FORMAT_1M16,    // 11.025kHz, 16-bit Mono
 WAVE_FORMAT_1S16,    // 11.025kHz, 16-bit Stereo
 WAVE_FORMAT_2M08,    // 22.05kHz , 8-bit  Mono
 WAVE_FORMAT_2S08,    // 22.05kHz , 8-bit  Stereo
 WAVE_FORMAT_2M16,    // 22.05kHz , 16-bit Mono
 WAVE_FORMAT_2S16,    // 22.05kHz , 16-bit Stereo
 WAVE_FORMAT_4M08,    // 44.1kHz  , 8-bit  Mono
 WAVE_FORMAT_4S08,    // 44.1kHz  , 8-bit  Stereo
 WAVE_FORMAT_4M16,    // 44.1kHz  , 16-bit Mono
 WAVE_FORMAT_4S16,    // 44.1kHz  , 16-bit Stereo
 WAVE_FORMAT_8M08,    //  8.0kHz  , 8-bit  Mono
 WAVE_FORMAT_8S08,    //  8.0kHz  , 8-bit  Stereo
 WAVE_FORMAT_8M16,    //  8.0kHz  , 16-bit Mono
 WAVE_FORMAT_8S16     //  8.0kHz  , 16-bit Stereo
};

// the following 2-D array defines the subtypes for DATATYPE_ALAW  it is indexed by the
// sampling rate ordinal (from _usfind_matching_sample_rate) and the number of channels

static USHORT aLaw[NUMFREQS][MONOSTEREO] = { // was ULONG
 ALAW_8B11KM,      // 8bit 11kHz mono
 ALAW_8B11KS,      // 8bit 11kHz stereo
 ALAW_8B22KM,      // 8bit 22kHz mono
 ALAW_8B22KS,      // 8bit 22kHz stereo
 ALAW_8B44KM,      // 8bit 44kHz mono
 ALAW_8B44KS,      // 8bit 44kHz stereo
 ALAW_8B8KM ,      // 8bit 8kHz mono
 ALAW_8B8KS        // 8bit 8kHz stereo
};

// the following 2-D array defines the subtypes for DATATYPE_MULAW it is indexed by the
// sampling rate ordinal (from _usfind_matching_sample_rate) and the number of channels

static USHORT muLaw[NUMFREQS][MONOSTEREO] = { // was ULONG
 MULAW_8B11KM,     // 8bit 11kHz mono
 MULAW_8B11KS,     // 8bit 11kHz stereo
 MULAW_8B22KM,     // 8bit 22kHz mono
 MULAW_8B22KS,     // 8bit 22kHz stereo
 MULAW_8B44KM,     // 8bit 44kHz mono
 MULAW_8B44KS,     // 8bit 44kHz stereo
 MULAW_8B8KM ,     // 8bit 8kHz mono
 MULAW_8B8KS       // 8bit 8kHz stereo
};

// sampleRates[] used by _usfind_matching_sample_rate() to determine sampling rate ordinal

USHORT sampleRates[] = {11025, 22050, 44100, 8000};  // all that's supported!? (yes, weird order)

#define SR_COUNT (sizeof(sampleRates)/sizeof(USHORT))

// following takes positive arguments and returns the absolute difference between the two args

#define DELTA(x,y) ((x > y) ? x - y : y - x)


// --------------------
// in: devCapsPtr -> audio caps structure passed from IoctlAudioCapability() call (was PAUDIO_CAPS, in audio.h)
//out: n/a
//nts:

VOID waDevCaps(MCI_AUDIO_CAPS __far *devCapsPtr) {

 USHORT sampleRate;
 USHORT sampleRateIndex = 0;
 USHORT tbps, tch, isFD;

 if (devCapsPtr->ulOperation == OPERATION_RECORD) {
    isFD = war.flags & FLAGS_WAVEAUDIO_FULLDUPLEX;
 }
 else if (devCapsPtr->ulOperation == OPERATION_PLAY) {
    isFD = wap.flags & FLAGS_WAVEAUDIO_FULLDUPLEX;
 }
 else {
    devCapsPtr->ulSupport = UNSUPPORTED_OPERATION;      // (since can't record+play on SAME stream)
    return;
 }

 if (devCapsPtr->ulChannels > 2) {
    devCapsPtr->ulSupport = UNSUPPORTED_CHANNELS;
    return;
 }

 if (devCapsPtr->ulBitsPerSample != 8 && devCapsPtr->ulBitsPerSample != 16) {
    devCapsPtr->ulSupport = UNSUPPORTED_BPS;
    return;
 }

 sampleRate = (USHORT)devCapsPtr->ulSamplingRate;
 if (1) {
    USHORT i, diff, minDiff = 65535;
    for (i=0; i < (sizeof(sampleRates)/sizeof(sampleRates[0])); i++) {
       diff = DELTA(sampleRate, sampleRates[i]);
       if (diff < minDiff) {
          minDiff = diff;
          sampleRateIndex = i;
       }
    }
    sampleRate = sampleRates[sampleRateIndex];
    if (sampleRate != (USHORT)devCapsPtr->ulSamplingRate) {
       devCapsPtr->ulSamplingRate = sampleRate;                      // yup, update original
       devCapsPtr->ulFlags = devCapsPtr->ulFlags | BESTFIT_PROVIDED; // ugh!

// !!!
//ddprintf("waDevCaps: using BESTFIT sampleRate\n");

    }
 }

 // get ulDataSubType and update any format-specific flags
 // note: all data types have more than one value

 tbps = (devCapsPtr->ulBitsPerSample-8)/8;      // 0 or 1 (for 8 or 16)
 tch  = devCapsPtr->ulChannels - 1;             // 0 or 1 (for mono or stereo)

 switch (devCapsPtr->ulDataType) {
 case DATATYPE_WAVEFORM:
 case PCM:
    devCapsPtr->ulDataSubType = (ULONG)waveSubtypes[sampleRateIndex][tbps][tch];
    //if (tbps) devCapsPtr->ulFlags = devCapsPtr->ulFlags | TWOS_COMPLEMENT | SIGNED; //16-bit is alway 2's-comp
// !!! see if this matters
    if (tbps) devCapsPtr->ulFlags = devCapsPtr->ulFlags | TWOS_COMPLEMENT;
    break;

 case DATATYPE_ALAW:
 case DATATYPE_RIFF_ALAW:
 case A_LAW:
    devCapsPtr->ulDataSubType = (ULONG)aLaw[sampleRateIndex][tch];
    break;

 case DATATYPE_MULAW:
 case DATATYPE_RIFF_MULAW:
 case MU_LAW:
    devCapsPtr->ulDataSubType = (ULONG)muLaw[sampleRateIndex][tch];
    break;

 default:
    devCapsPtr->ulSupport = UNSUPPORTED_DATATYPE;
    return;
 }

 // original had this overwrite the ulFlags above! (TWOS_COMP and BESTFIT_!)
 // also, original (tropez) sets TWOS_COMP for 8-bit! data, in WAVESTREAM constructor
 // also, original sets BIG_ENDIAN (4232 says 16-bit is little endian)
 // ... not sure if this is saying what should be used, or what -can- be used (as in BIG_ENDIAN supported)

// !!!
 devCapsPtr->ulFlags = devCapsPtr->ulFlags |
                              BIG_ENDIAN |      // !!! that's what tropez has
                              FIXED        |  // fixed length data
                              LEFT_ALIGNED |  // left align bits on byte (always is if 8/16 bps)
                              INPUT        |  // input select is supported
                              OUTPUT       |  // output select is supported
                              MONITOR      |  // monitor is supported
                              VOLUME;         // volume control is supported

 // full-duplex info:
 // The number of resource units is described in the MMPM2.INI -- this can be thought of as
 // the number of active streams the driver can manage at one time.   Use 2 (one play, one rec).
 //
 // Tell MMPM how many of these units THIS stream will consume.  If full-duplex allowed, this
 // stream consumes 1 stream unit.  If not allowed, indicate that this stream consumes 2 units
 // (or all the available units).
 //
 // Along with the resource units, 2 resources classes are defined:  one for playback and one
 // for capture.  Tell MMPM (in the MMPM2.INI) that driver can deal with 1 playback and 1 capture
 // stream, OR 1 capture and 1 playback stream, at the same time. This is indicated in the
 // valid resource combos in MMPM2.INI
 //
 // So, check if this is a playback or capture and set the correct resource class
 // (playback = 1, capture = 2)
 //
 // no longer have WAVEAUDIO.usDmaChan/usSecDmaChan, but instead just WAVEAUDIO.flags bit0=1 is F-D

 if (isFD) {
    devCapsPtr->ulResourceUnits = 1;
 }
 else {
    devCapsPtr->ulResourceUnits = 2;   // full-duplex not allowed so allocate both resource units
 }

 if (devCapsPtr->ulOperation == OPERATION_RECORD) {
    devCapsPtr->ulResourceClass = 2;
 }
 else {
    devCapsPtr->ulResourceClass = 1;
 }

 devCapsPtr->fCanRecord = 1;
 devCapsPtr->ulBlockAlign = 1;
 devCapsPtr->ulSupport = SUPPORT_SUCCESS;

// !!!
//ddprintf("@waDevCaps: devCapsPtr->ulFlags=%lx\n", devCapsPtr->ulFlags);

 return;
}


// ---------------------------------
// in: wsPtr -> wavestream structure
//out:
//nts: also reinits dma hardware (esp. for logical dma buffer size, interrupts/buffer)

VOID waConfigDev(WAVESTREAM *wsPtr) {

 ULONG i, count, consumeRate;
 USHORT tbps, tch, tBytesPS, tIs16, tIsST;
 USHORT bufferInts;
 USHORT dmaType;

 WAVECONFIG *waveConfigPtr = &wsPtr->waveConfig;
 WAVEAUDIO *waPtr = wsPtr->waPtr;

// !!! new dma stuff

 waPtr->ab.bufferSize = wsPtr->audioBufferSize; // dma buffer size for this stream (def=0, 16KB)
 if (waPtr->ab.bufferSize < 1024) waPtr->ab.bufferSize = 0x4000;

 bufferInts = wsPtr->audioBufferInts;           // interrupts per dma buffer (def=0, two per buffer)
 if (bufferInts < 2) bufferInts = 2;            // at least two (must be a power of 2)

// !!!
bufferInts = 8;

 tbps = waveConfigPtr->bitsPerSample;
 tBytesPS = tbps/8;
 tIs16 = tBytesPS >> 1;

 tch  = waveConfigPtr->channels;
 tIsST = tch >> 1;

 // set clock select bits, using first SRT with sample rate >= requested rate
 // eg, if req=44000 then uses 44100 (if req=2000 then uses 5512)
 // this was done in a call to _vset_clock_info(ULONG freq)

 for (i=0; i < SRT_COUNT; i++) {
    if (srt[i].sampleRate >= waveConfigPtr->sampleRate) {
       waPtr->clockSelectData = srt[i].clockSelect;
       break; // out of for
    }
    waPtr->clockSelectData = srt[SRT_COUNT-1].clockSelect;  // requested is > last srt, so use last
 }

 // set up WAVEAUDIO.formatData, set silence data, set mono/stereo

 switch (waveConfigPtr->dataType) {
 case DATATYPE_WAVEFORM:
 case PCM:
    if (tIs16) {
       waPtr->formatData = FORMAT_BIT0;
       waveConfigPtr->silence = 0x0000;
    }
    else {
       waPtr->formatData = 0;
       waveConfigPtr->silence = 0x8080;
    }
    break;

 case DATATYPE_ALAW:
 case DATATYPE_RIFF_ALAW:
 case A_LAW:
    waPtr->formatData = FORMAT_BIT0 | CL_BIT;
    waveConfigPtr->silence = 0x5555;
    break;

 case DATATYPE_MULAW:
 case DATATYPE_RIFF_MULAW:
 case MU_LAW:
    waPtr->formatData = CL_BIT;
    waveConfigPtr->silence = 0x7F7F;
    break;
 }

 if (tIsST) waPtr->formatData = waPtr->formatData | STEREO_BIT;  // set stereo bit

 count = waPtr->ab.bufferSize/bufferInts;// bytes per interrupt
 waveConfigPtr->bytesPerIRQ = count;    // grab it while it's still bytes
 if (tIs16) count = count >> 1;         // if 16-bit then half as many as that in samples
 if (tIsST) count = count >> 1;         // if stereo then half as many as that in samples again
 waPtr->countData = count-1;            // samples per interrupt

// !!!
ddprintf("@waConfigDev:count=%lxh, bytes/IRQ=%lxh\n",count,waveConfigPtr->bytesPerIRQ);

 // calc PCM consume rate
 // The consume rate is the number of bytes consumed by this data format per second, based on:
 //    rate = sampleRate * bps/8 * channels
 // this is returned to the WAVESTREAM and used to calculate stream time

 consumeRate = waveConfigPtr->sampleRate;
 if (tIs16) consumeRate = consumeRate + consumeRate;
 if (tIsST) consumeRate = consumeRate + consumeRate;
 waveConfigPtr->consumeRate = consumeRate;

 // reinit dma
 // (dmaType has extra meaning: typeFdma is at bit8, so add it in (set in original dmaInit())

 dmaType = waPtr->ab.dmaCh.type | ((waPtr->ab.dmaCh.chInfo.typeFdma & 1) << 8);
 dmaInit(waPtr->ab.dmaCh.ch, dmaType, &waPtr->ab);

 return;
}


// ---------------
// in: n/a (maybe)
//out: 1 always
//nts: not used (use Stop stream instead)

USHORT waPause(VOID) {

 return 1;
}


// ---------------
// in: n/a (maybe)
//out: 1 always
//nts: not used (use Start stream instead)

USHORT waResume(VOID) {

 return 1;
}


#pragma code_seg ("_INITTEXT");
#pragma data_seg ("_INITDATA","ENDDS");


// -----------------------------
// in: dma channel = dma channel
//     waPtr -> WAVEAUDIO structure for this object
//out: 0 if okay, else error
//nts: called by wpInit() and wrInit(), each of which have their own, single WAVEAUDIO structure
//     and then only once, at driver init
//     sets up:
//      1. audioBuffer allocation and init
//      2. dma init
//     on entry, waPtr->flags:
//      FLAGS_WAVEAUDIO_FULLDUPLEX  1   // can do full-duplex (has separate DMA for play/rec)
//      FLAGS_WAVEAUDIO_DMA16       2   // dma channel is 16-bit (and waPtr->dmaPtr->ch itself)
//      FLAGS_WAVEAUDIO_FTYPEDMA    4   // hardware support demand-mode dma
//     passing dmaChannel to simplify
//     was _vSetup()

USHORT waSetup(USHORT dmaChannel, WAVEAUDIO *waPtr) {

 USHORT rc = 0;
 USHORT typeDMA = DMA_TYPE_ISA; // will be OR'ed with DMA_TYPE_PLAY(dma-read)/CAPTURE(dma-write)
 ULONG  sizeDMA = DMA_BUFFER_SIZE;  // physical size, will be xxKB always (after testing)
 ULONG  sizePage = 0; // 0 because...dunno, not used I suppose

 rc = abInit(sizeDMA, sizePage, dmaChannel, &waPtr->ab);
 if (rc) goto ExitNow;

 if (waPtr->devType == AUDIOHW_WAVE_PLAY) {
    typeDMA = typeDMA | DMA_TYPE_PLAY;
 }
 else {
    typeDMA = typeDMA | DMA_TYPE_CAPTURE;
 }

 if (waPtr->flags & FLAGS_WAVEAUDIO_FTYPEDMA) {
    typeDMA = typeDMA | DMA_TYPE_FTYPE;
 }

 rc = dmaInit(dmaChannel, typeDMA, &waPtr->ab);
 if (rc) goto ExitNow;

 waPtr->irqHandlerPtr = (NPFN)irqHandler;  // assign near function pointer to IRQ handler (always CS:offset)

ExitNow:
 return rc;
}



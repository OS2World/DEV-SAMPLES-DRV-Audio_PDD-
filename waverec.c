//
// waverec.c
// 1-Feb-99
//
// USHORT waverecStart(WAVESTREAM *wsPtr);
// USHORT waverecStop(WAVESTREAM *wsPtr);
// USHORT waverecEnable(WAVESTREAM *wsPtr);
// USHORT waverecInit(USHORT dmaChannel, USHORT flags, USHORT irq) {

#include "cs40.h"


WAVEAUDIO war;  // ** global **  The heart of it all, hardware-wise

// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts:

USHORT waverecStart(WAVESTREAM *wsPtr) {

 USHORT rc = 0;
 UCHAR data;
 STREAM *tstreamPtr = 0; // original typed this as WAVESTREAM*, but FindActiveStream() is STREAM
 USHORT isFD = wsPtr->waPtr->flags & FLAGS_WAVEAUDIO_FULLDUPLEX;

// !!!
ddprintf("@ waverecStart, isFD=%u\n",isFD);

 // need to know if playing now (only care if full duplex capable)

 if (isFD) tstreamPtr = streamFindActive(STREAM_WAVE_PLAY);

 // whoa! now this is hardware specific -- ensure record not active, and if not FD, off play
 // this will be moved into a hardware-specific module eventually

 data = chipsetGET('i', INTERFACE_CONFIG_REG);
 if (isFD) {
    data = data & 0xFD;  // off CEN
 }
 else {
    data = data & 0xFC;  // off CEN, and PEN since not full duplex
 }

 _cli_();
 dmaWaitForChannel(DMA_WAIT, &wsPtr->waPtr->ab);
 chipsetSet('i', INTERFACE_CONFIG_REG, data);
 _sti_();

 // mute the dac output
 // MIXER_SOURCE_WAVE is the correct mixer line to send the
 // query and set controls command to...
 // copy the current mute and volume values to temp variables then
 // set the volume to 0 and mute the output

 //- pDACOutput->QueryControl(MIX_MUTE, (ULONG FAR *)&DACMute);
 //- pDACOutput->QueryControl(MIX_VOLUME, (ULONG FAR *)&DACVolume);
 //- pDACOutput->SetControl(MIX_MUTE, MUTE_ON);
 //- pDACOutput->SetControl(MIX_VOLUME,0);
 //
 // need to ensure digital loopback muted (...) here, too, eventually

 chipsetMCE(1); // turn on MCE

 // set up interface config reg

 if (isFD == 0) {
    data = 0x44;        // capture gets dma play into pio
 }
 else {
    data = chipsetGET('i', INTERFACE_CONFIG_REG) & 1; // preserve PEN
 }
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 // set the Clock and data format reg
 // full-duplex concern:
 // "to set the clock info we need to read the Fs and Playback Data Format
 // reg. We must preserve the playback format bits or we could switch
 // the playback data format on the fly.... very bad.."
 // (see waveplay.c for the concern for playback since this is done differently than playback)

 data = chipsetGET('i', PLAYBACK_DATA_FORMAT_REG) & 0xF0; // preserve playback format
 data = data | wsPtr->waPtr->clockSelectData;
 chipsetSet('i', PLAYBACK_DATA_FORMAT_REG, data);
 chipsetSet('i', CAPTURE_DATA_FORMAT_REG, wsPtr->waPtr->formatData);

 // wait for the chip to settle down

 chipsetWaitInit();
 chipsetMCE(0); // turn MCE off

 // Just in case the CAL0 and CAL1 bits are set to anything other than 0b00
 // or if this is the first start since we reset the codec
 // wait for init complete and aci in process to turn off

 chipsetWaitInit();
 chipsetWaitACI();

 iodelay(WAIT_2MS);

 // if this is a dual dma environment (full-duplex) and a
 // playback stream is active turn PEN back on

 if (isFD && tstreamPtr) {
    data = chipsetGET('i', INTERFACE_CONFIG_REG) | 1;  // turn on PEN
    chipsetSet('i', INTERFACE_CONFIG_REG, data);
 }

 // set the base count registers (to *_PLAYBACK_* if not full-duplex)

 if (isFD) {
    chipsetSet('i', LOWER_CAPTURE_COUNT, (UCHAR)wsPtr->waPtr->countData);
    chipsetSet('i', UPPER_CAPTURE_COUNT, (UCHAR)(wsPtr->waPtr->countData >> 8));
 }
 else {
    chipsetSet('i', LOWER_PLAYBACK_COUNT, (UCHAR)wsPtr->waPtr->countData);
    chipsetSet('i', UPPER_PLAYBACK_COUNT, (UCHAR)(wsPtr->waPtr->countData >> 8));
 }

 // enable the ISR via DevHelp_SetIRQ

 rc = irqEnable(wsPtr->waPtr->irq);
 if (rc) goto ExitNow;

 // start up DMA

 dmaStart(&wsPtr->waPtr->ab);

 // ### unmute the DAC
 //- pDACOutput->SetControl(MIX_MUTE, DACMute);
 //- pDACOutput->SetControl(MIX_VOLUME,DACVolume);

// original didn't toggle MCE when turning on CEN, but it did in waveplay.c...check that out..

 chipsetMCE(1); // turn MCE on (can probably remove this for just setting CEN!)

 data = chipsetGET('i', INTERFACE_CONFIG_REG) | 2;  // turn on CEN
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 chipsetMCE(0); // turn MCE off (see above)

ExitNow:
 return rc;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts:

USHORT waverecStop(WAVESTREAM *wsPtr) {

 USHORT rc = 0;
 UCHAR data;

 // if CEN is on turn it off

 data = chipsetGET('i', INTERFACE_CONFIG_REG);

// !!!
//ddprintf("@ waverecStop, data=%u\n",(USHORT)data);

 if (data & 2) {
    data = data & 0xFE;
    _cli_();
    dmaWaitForChannel(DMA_WAIT, &wsPtr->waPtr->ab);
    chipsetSet('i', INTERFACE_CONFIG_REG, data);
    _sti_();
 }

 // stop DMA and disable irq (irq gets released only after last disable)

 dmaStop(&wsPtr->waPtr->ab);
 rc = irqDisable(wsPtr->waPtr->irq);

 chipsetSet('i', ALT_FEATURE_STATUS, CLEAR_CI); // turn int off (do before disabling irq?)

 return rc;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts: just adds WaveRec_Irq_Handler() to routine to call from interrupt handler
//     since already is known just return 0 (see Dis/EnableIrqHandler() for more)

USHORT waverecEnable(WAVESTREAM *wsPtr) {

 USHORT rc = 0;

 return rc;
 wsPtr;
}


// ------------------
// in: dmaChannel
//     flags:
//      FLAGS_WAVEAUDIO_FULLDUPLEX  1   // can do full-duplex (has separate DMA for play/rec)
//      FLAGS_WAVEAUDIO_DMA16       2   // dma channel is 16-bit
//      FLAGS_WAVEAUDIO_FTYPEDMA    4   // hardware support demand-mode dma
//     irq = number of IRQ (only one...)
//out: 0 if okay, else error
//nts:

#pragma code_seg ("_INITTEXT");
#pragma data_seg ("_INITDATA","ENDDS");

USHORT waverecInit(USHORT dmaChannel, USHORT flags, USHORT irq) {

 USHORT rc;

 MEMSET(&war,0,sizeof(war));

 war.devType = AUDIOHW_WAVE_CAPTURE;
 wap.flags = flags;
 wap.irq = irq;

 // waSetup():
 // sets up DMACHANNEL
 // sets up war.irqHandlerPtr

 rc = waSetup(dmaChannel, &war);

 return rc;
}


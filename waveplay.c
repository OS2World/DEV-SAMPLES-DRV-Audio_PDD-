//
// waveplay.c
// 1-Feb-99
//
// USHORT waveplayStart(WAVESTREAM *wsPtr) {
// USHORT waveplayStop(WAVESTREAM *wsPtr) {
// USHORT waveplayEnable(WAVESTREAM *wsPtr) {
// USHORT waveplayInit(USHORT dmaChannel, USHORT flags, USHORT irq) {

#include "cs40.h"


WAVEAUDIO wap;  // ** global **  The heart of it all, hardware-wise

// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts:

USHORT waveplayStart(WAVESTREAM *wsPtr) {

 USHORT rc = 0;
 UCHAR data;
 STREAM *tstreamPtr = 0; // original typed this as WAVESTREAM*, but FindActiveStream() is STREAM
 USHORT isFD = wsPtr->waPtr->flags & FLAGS_WAVEAUDIO_FULLDUPLEX;

#ifdef TRACE_CALIBRATE
 traceCalibrate();
#endif

// !!!
//ddprintf("@ waveplayStart, isFD=%u\n",isFD);

 // need to know if recording now (only care if full duplex capable)

 if (isFD) tstreamPtr = streamFindActive(STREAM_WAVE_CAPTURE);

 // whoa! now this is hardware specific -- ensure playback not active, and if not FD, off record
 // this will be moved into a hardware-specific module eventually

 data = chipsetGET('i', INTERFACE_CONFIG_REG);
 if (isFD) {
    data = data & 0xFE;  // off PEN
 }
 else {
    data = data & 0xFC;  // off PEN, and CEN since not full duplex
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

 //- pDACOutput->QueryControl(MIX_VOLUME, (ULONG FAR *)&DACVolume);
 //- pDACOutput->SetControl(MIX_MUTE, MUTE_ON);
 //- pDACOutput->SetControl(MIX_VOLUME,0);

 chipsetMCE(1); // turn MCE on

 // set up interface config reg

 if (isFD == 0) {
    data = 0x84;        // playback gets dma capture into pio
 }
 else {
    data = chipsetGET('i', INTERFACE_CONFIG_REG) & (0x18 | 0x02); // preserve CEN (and CAL1,0)
 }
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 // set the Clock and data format reg
 // full-duplex conserns:
 // The 4232 only has 1 clock so the playback and capture sample rates
 // have to be the same. Because of this we have to make some value judgments
 // about how to set up the sampling rate. One could an comeup with many
 // possable rules on how to manage the sampling rate... I have one:
 // THE CAPTURE ALWAYS WINS !!
 // With this in mind we now set up the Fs and Playback Data Format Reg.

 if (isFD && tstreamPtr) {
    data = chipsetGET('i', PLAYBACK_DATA_FORMAT_REG) & 15; // preserve the clock config bits..
    data = data | wsPtr->waPtr->formatData;
 }
 else {
    data = wsPtr->waPtr->formatData | wsPtr->waPtr->clockSelectData;
 }
 chipsetSet('i', PLAYBACK_DATA_FORMAT_REG, data);

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
 // capture stream is active, make sure CEN is on.

 if (isFD && tstreamPtr) {
    data = chipsetGET('i', INTERFACE_CONFIG_REG) | 2;   // turn on CEN
    chipsetSet('i', INTERFACE_CONFIG_REG, data);
 }

 // set the playback base count registers

 chipsetSet('i', LOWER_PLAYBACK_COUNT,(UCHAR)wsPtr->waPtr->countData);
 chipsetSet('i', UPPER_PLAYBACK_COUNT,(UCHAR)(wsPtr->waPtr->countData >> 8));

 // enable the ISR via DevHelp_SetIRQ

 rc = irqEnable(wsPtr->waPtr->irq);
 if (rc) goto ExitNow;

 // start up DMA

 dmaStart(&wsPtr->waPtr->ab);

 // ### unmute the DAC
 //- pDACOutput->SetControl(MIX_MUTE, MUTE_OFF);
 //- pDACOutput->SetControl(MIX_VOLUME,DACVolume);

 // why toggle MCE?  Would disrupt and record, see waverec.c for more since it doesn't do this now

 chipsetMCE(1); // turn MCE on

 data = chipsetGET('i', INTERFACE_CONFIG_REG) | 1; // turn on PEN (enable playback)
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 chipsetMCE(0); // turn MCE off

ExitNow:
 return rc;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts:

USHORT waveplayStop(WAVESTREAM *wsPtr) {

 USHORT rc = 0;
 UCHAR data;

 // if PEN is on turn it off

 data = chipsetGET('i', INTERFACE_CONFIG_REG);

// !!!
//ddprintf("@ waveplayStop, data=%u\n",(USHORT)data);

 if (data & 1) {
    data = data & 0xFE;
    _cli_();
    dmaWaitForChannel(DMA_WAIT, &wsPtr->waPtr->ab);
    chipsetSet('i', INTERFACE_CONFIG_REG, data);
    _sti_();
 }

 // stop DMA and disable irq (irq gets released only after last disable)

 dmaStop(&wsPtr->waPtr->ab);
 rc = irqDisable(wsPtr->waPtr->irq);

 chipsetSet('i', ALT_FEATURE_STATUS, CLEAR_PI); // turn int off (do before disabling irq?)

 return rc;
}


// ---------------------------------
// in: wsPtr -> WAVESTREAM structure
//out: 0 if okay, else error
//nts:

USHORT waveplayEnable(WAVESTREAM *wsPtr) {

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

USHORT waveplayInit(USHORT dmaChannel, USHORT flags, USHORT irq) {

 USHORT rc;

 MEMSET(&wap,0,sizeof(wap));

 wap.devType = AUDIOHW_WAVE_PLAY;
 wap.flags = flags;
 wap.irq = irq;

 // waSetup():
 // sets up DMACHANNEL
 // sets up wap.irqHandlerPtr

 rc = waSetup(dmaChannel, &wap);

 return rc;
}


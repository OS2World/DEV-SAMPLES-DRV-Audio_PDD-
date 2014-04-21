//
// init.c
// 3-Feb-99
//
// USHORT stratMode2Init(REQPACK __far *rpPtr);
//
// will make call into slam basedev to get resources (won't use RM... but have to
// write a slam, with IDC hook, probably use assembly like wacker...?)

#pragma code_seg ("_INITTEXT");
#pragma data_seg ("_INITDATA","ENDDS");

#include "cs40.h"

extern USHORT endCode;
extern USHORT endData;

// -------------------------------------
// in:
//out:
//nts:

USHORT stratMode2Init(REQPACK __far *rpPtr) {

 USHORT rc = 0;
 USHORT heapSize;
 USHORT dmaPlayChannel, dmaRecChannel, irq;
 USHORT bp, cp, mode, make = 0;
 USHORT flags = 0;
 USHORT typeFdmaPlay = 0, typeFdmaRec = 0, dtmFlag = 0;

 Device_Help = rpPtr->init.Device_Help;

 rpPtr->status = RPDONE | RPERR | RPGENFAIL;
 rpPtr->init.sizeCS = 0;
 rpPtr->init.sizeDS = 0;

 heapSize = HeapInit(8192);

 // do any log setup here...

 // handle cmd line here...

 // temp for now, knows about -x only

 {
  UCHAR __far *tPtr = rpPtr->init.argsPtr;

  while (*tPtr && *tPtr != ' ') { // skip over filename
     tPtr++;
  }

  while (*tPtr && *tPtr != '-') { // look for a -
     tPtr++;
  }
  if (*tPtr) {                    // will have a - or a -0-
     tPtr++;
     if (*tPtr == 'x') make = 0x101;
  }

  if (make == 0) {

     typeFdmaPlay = 1;
     typeFdmaRec = 1;
     dmaPlayChannel = 1;
     dmaRecChannel = 0;
     flags = FLAGS_WAVEAUDIO_FULLDUPLEX;
     dtmFlag = 1;

     irq = 5;
     bp = 0x534;
     cp = 0x538;
     mode = 2;
     make = 0;   // 0=CS, 0x100=Yamaha, 0x101=3DXG
  }
  else {

     typeFdmaPlay = 1;
     typeFdmaRec = 1;
     dmaPlayChannel = 3;
     dmaRecChannel = 0;
     flags = FLAGS_WAVEAUDIO_FULLDUPLEX;
     dtmFlag = 1;

     irq = 11;
     bp = 0x534;
     cp = 0x390;
     mode = 2;
     make = 0x101;  // 0=CS, 0x100=Yamaha, 0x101=3DXG
  }
 }


 // resource manager access here (create, detect PnP, etc.) here...

 // had created IRQ object for timer here...
 // had setup more hardware types here (FMSYNTH or MPU...)

 if (dmaPlayChannel >= 4) flags = flags | FLAGS_WAVEAUDIO_DMA16;
 if (typeFdmaPlay)        flags = flags | FLAGS_WAVEAUDIO_FTYPEDMA;

 rc = waveplayInit(dmaPlayChannel, flags, irq);
 if (rc) {
ddprintf("waveplayInit() failed, rc=%u\n");
    goto ExitNow;
 }

 flags = flags & ~(FLAGS_WAVEAUDIO_DMA16 | FLAGS_WAVEAUDIO_FTYPEDMA);
 if (dmaRecChannel >= 4) flags = flags | FLAGS_WAVEAUDIO_DMA16;
 if (typeFdmaRec)        flags = flags | FLAGS_WAVEAUDIO_FTYPEDMA;

 rc = waverecInit(dmaRecChannel, flags, irq);
 if (rc) {
ddprintf("waverecInit() failed, rc=%u\n");
    goto ExitNow;
 }

 rc = chipsetInit(bp, cp, mode, make);
 if (rc) goto ExitNow;

 chipsetSetDTM(dtmFlag);


 // had init'ed mixer here...
 //InitMixer();

 // had done VDD setup here...
 // fill in the ADAPTERINFO
 //codec_info.ulNumPorts = NUMIORANGES;
 //codec_info.Range[0].ulPort  =  pResourcesWSS->uIOBase[0];
 //codec_info.Range[0].ulRange =  pResourcesWSS->uIOLength[0];
 //...
 //// set up the addressing to the codec data for the vdd
 //pfcodec_info = (ADAPTERINFO __far *)&codec_info;
 //DevHelp_VirtToLin (SELECTOROF(pfcodec_info), (ULONG)(OFFSETOF(pfcodec_info)),(PLIN)&pLincodec);
 // copy the pdd name out of the header.
 //for (i = 0; i < sizeof(szPddName)-1 ; i++) {
 //   if (phdr->abName[i] <= ' ')
 //      break;
 //   szPddName[i] = phdr->abName[i];
 //}
 //// register the VDD IDC entry point..
 //DevHelp_RegisterPDD ((NPSZ)szPddName, (PFN)IDCEntry_VDD);

 if (rc) goto ExitNow;

 rpPtr->status = RPDONE;
 rpPtr->init.sizeCS = (USHORT)&endCode;
 rpPtr->init.sizeDS = (USHORT)&endData;

ddprintf("stratMode2Init:endCode=%x, endData=%x, ds=%x\n",rpPtr->init.sizeCS,rpPtr->init.sizeDS,_DS());

ExitNow:
 return rc;
}


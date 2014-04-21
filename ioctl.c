//
// ioctl.c
// 2-Feb-99
//
// VOID ioctlStrat(REQPACK __far *rpPtr, USHORT lDev);

#include "cs40.h"

#define VSD_RCID  99    // as in VSD RCID


// mixer/line stuff not yet done


// -------------------------
// in: rpPtr -> request pack
//     lDev = logical device
//out: n/a
//nts: this checks that the SFN is not already a registered stream, and
//     then

static VOID ioctlAudioInit(REQPACK __far *rpPtr, USHORT lDev) {

 WAVESTREAM *wsPtr;
 STREAM *streamPtr;
 USHORT devType;
 USHORT rc;

 MCI_AUDIO_INIT __far *mciInitPtr = rpPtr->ioctl.dataPtr;
 USHORT SFN = rpPtr->ioctl.SFN;

// !!!
//ddprintf("@ioctlAudioInit...\n");

 // if idle or deinit request, get the stream object based on SFN and set state to idle...

 if (mciInitPtr->sMode == IDLE) {
    streamPtr = streamFindStreamSFN(SFN);
    if (streamPtr) streamPtr->streamState = streamPtr->streamState | STREAM_IDLE;
    //mciInitPtr->pvReserved = (VOID __far *)((ULONG)SFN);  // have to... done at good exit
    rc = 0;
    goto ExitNow;
 }
 else {

    // check for stream with same SFN already -- if so and is idle, reset idle bit
    // this has to be done because mmpm passes the idle state down if the stream is
    // losing the hardware to another app (usually to another app) -- if not now idle,
    // then the stream is either not registered or is being re-initialized (o) in another
    // mode or with different file attributes (quote) ... this reinit is "a total hack on
    // the part of mmpm since it should instead deregister the stream, then init a new
    // stream, but mmpm doesn't do it that way" (at least, the VSD doesn't, supposedly)
    // anyway, if this is the case, delete the current stream and make a new one based on
    // this req pack

    streamPtr = streamFindStreamSFN(SFN);
    if (streamPtr) {
       if (streamPtr->streamState & STREAM_IDLE) {
          streamPtr->streamState = streamPtr->streamState & STREAM_NOT_IDLE;
          //mciInitPtr->pvReserved = (VOID __far *)((ULONG)SFN);  // have to... done at good exit
          rc = 0;
          goto ExitNow;
       }
       else {
          //streamDeinit(streamPtr);      // was "delete pstream" (use wavestreamDeinit() instead)
          wavestreamDeinit(streamPtr->wsParentPtr);
       }
    }
 }

 devType = hwGetType(mciInitPtr->sMode, (USHORT)mciInitPtr->ulOperation, lDev);  // AUDIOHW_WAVE_PLAY/CAPTURE
 if (devType == AUDIOHW_INVALID_DEVICE) {
    rc = INVALID_REQUEST;
    goto ExitNow;
 }

 mciInitPtr->ulFlags = 0;

 if (devType == AUDIOHW_WAVE_PLAY || devType == AUDIOHW_WAVE_CAPTURE) {

    wsPtr = malloc(sizeof(WAVESTREAM));   // wsPtr will be stored/tracked in streamPtr->wsParentPtr
    if (wsPtr) {
       MEMSET(wsPtr,0,sizeof(WAVESTREAM));
       streamPtr = wavestreamInit(devType, mciInitPtr, wsPtr);  // wavestreamInit() allocates streamPtr
       if (streamPtr == 0) {
          rc = INVALID_REQUEST;
          goto ExitNow;
       }
    }
    else {
       rc = INVALID_REQUEST;    // rc = 8; probably not a valid rc, but could use it in rp.status...
       goto ExitNow;            // see audio.h for valid return codes...
    }
 }
 else {
    rc = INVALID_REQUEST;
    goto ExitNow;
 }

 mciInitPtr->ulFlags = mciInitPtr->ulFlags | VOLUME | INPUT | OUTPUT | MONITOR; // have to modify
 mciInitPtr->sDeviceID = VSD_RCID; // reported in VSD DLL (I'll have to come up with my own, say 40)

 streamPtr->SFN = SFN;
 rc = 0;


ExitNow:

// !!!
//ddprintf("...mciInit.flags=%lx\n",mciInitPtr->ulFlags);

 if (rc == 0) {
    mciInitPtr->pvReserved = (VOID __far *)((ULONG)SFN);  // since common in all rc==0
    // think this is for sending in, as in driver version needed?
    // mciInitPtr->ulVersionLevel = DRIVER_VERSION;
 }

 mciInitPtr->sReturnCode = rc;
 if (rc) rpPtr->status = rpPtr->status | RPERR;

 return;
}


// -------------------------
// in: rpPtr -> request pack
//     lDev = logical device
//out: n/a
//nts:

static VOID ioctlAudioCaps(REQPACK __far *rpPtr, USHORT lDev) {

 USHORT devType;
 USHORT rc = 0;

 MCI_AUDIO_CAPS __far *devCapsPtr = rpPtr->ioctl.dataPtr;

 devType = hwGetType((USHORT)devCapsPtr->ulDataType, (USHORT)devCapsPtr->ulOperation, lDev);
 if (devType == AUDIOHW_INVALID_DEVICE) {
    rc = UNSUPPORTED_DATATYPE;
    goto ExitNow;
 }

 waDevCaps(devCapsPtr);
 rc = devCapsPtr->ulSupport;

ExitNow:
 devCapsPtr->ulSupport = rc;
 if (rc) rpPtr->status = rpPtr->status | RPERR;

 return;
}


// -------------------------
// in: rpPtr -> request pack
//out: n/a
//nts:
// * if it's AUDIO_CHANGE, just report success, otherwise report failure
// * this is because we don't support volume, balance, multiple in/out devices,
// * etc.  Also, START, STOP, RESUME, and PAUSE are redundant, so we don't
// * support those either.

static VOID ioctlAudioControl(REQPACK __far *rpPtr) {

 MCI_AUDIO_CONTROL __far *controlPtr = rpPtr->ioctl.dataPtr;

 if (controlPtr->usIOCtlRequest == AUDIO_CHANGE) {
    controlPtr->sReturnCode = 0;
 }
 else {
    controlPtr->sReturnCode = INVALID_REQUEST;
 }

 return;
}


// -------------------------
// in: rpPtr -> request pack (.status RPDONE bit already set)
//     lDev = logical device (first header device is 0, second (if any) is 1, and so on)
//out: n/a
//nts: generic ioctl strategy entry
//     cat80 is all that's supported...

VOID ioctlStrat(REQPACK __far *rpPtr, USHORT lDev) {

// !!!

#ifdef TRACE_STRAT_GENIOCTL
 tracePerf(rpPtr->ioctl.category | TRACE_IN, rpPtr->ioctl.function);
#endif

 if (rpPtr->ioctl.category == AUDIO_IOCTL_CAT) {

    switch(rpPtr->ioctl.function) {
    case AUDIO_INIT:
       ioctlAudioInit(rpPtr,lDev);
       break;
    case AUDIO_CONTROL:
       ioctlAudioControl(rpPtr);
       break;
    case AUDIO_CAPABILITY:
       ioctlAudioCaps(rpPtr,lDev);
       break;
    case MIX_GETCONNECTIONS:
    case MIX_SETCONNECTIONS:
    case MIX_GETLINEINFO:
    case MIX_GETCONTROL:
    case MIX_SETCONTROL:

// !!!
ddprintf("ioctl mixer calls, %u\n",rpPtr->ioctl.function);

       break;
    default:
       rpPtr->status = rpPtr->status | RPERR | RPBADCMD;
       break;
    }
 }
 else {
    rpPtr->status = rpPtr->status | RPERR | RPBADCMD;

// !!!

#ifdef TRACE_STRAT_GENIOCTL
 tracePerf(rpPtr->ioctl.category | TRACE_OUT, rpPtr->ioctl.function);
#endif

 }

 return;
}


//
// ssm_idc.c
// 3-Feb-99
//
// ULONG __far __loadds __cdecl idcDDCMD(DDCMDCOMMON __far *commonPtr);

#include "cs40.h"

ULONG __far __loadds __cdecl idcDDCMD(DDCMDCOMMON __far *commonPtr) {

 ULONG rc = NO_ERROR;
 USHORT function;
 STREAM *streamPtr;

//static USHORT cnt=0;

 if (commonPtr == 0) {
#ifdef TRACE_IDC_DDCMD
    tracePerf(TRACE_IDC_DDCMD_IN, 0xA5A5); // can't get function since commonPtr = 0
#endif
    return(ERROR_INVALID_BLOCK);
 }

 function = commonPtr->ulFunction;
 streamPtr = streamFindStreamHandle(commonPtr->hStream);

//cnt++;
//ddprintf("idcDDCMD, cnt=%u, func=%u, streamPtr=%x \n",cnt,function, streamPtr);


#ifdef TRACE_IDC_DDCMD
 tracePerf(TRACE_IDC_DDCMD_IN, ((ULONG)function << 16) | _IF());  // high-word is function is USHORT, 0-6
#endif

 switch(function) {
 case DDCMD_SETUP:      // 0

    if (streamPtr) {

       DDCMDSETUP __far *setupPtr = (DDCMDSETUP __far *)commonPtr;
       SETUP_PARM __far *setupParmPtr = setupPtr->pSetupParm;

       // here, yes, setupParmPtr is passed in, and am to set timeBase to its ulStreamTime value

       wavestreamSetCurrentTime(setupParmPtr->ulStreamTime, streamPtr->wsParentPtr);

       // if flags' field (based on size) then indicate to mmpm that recurring events
       // are supported...only they're not just yet

       if (setupPtr->ulSetupParmSize > sizeof(ULONG)) {
          //setupParmPtr->ulFlags = SETUP_RECURRING_EVENTS;
          setupParmPtr->ulFlags = 0;
       }
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;

 case DDCMD_READ:       // 1

    if (streamPtr) {

       DDCMDREADWRITE __far *rwPtr = (DDCMDREADWRITE __far *)commonPtr;

       // rc = 8 if can't allocate memory for STREAM_BUFFER

       rc = streamRead(rwPtr->pBuffer, rwPtr->ulBufferSize, streamPtr);
       if (rc == 8) rc = ERROR_TOO_MANY_BUFFERS;  // 5901;
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;

 case DDCMD_WRITE:      // 2

    if (streamPtr) {

       DDCMDREADWRITE __far *rwPtr = (DDCMDREADWRITE __far *)commonPtr;

       // rc = 8 if can't allocate memory for STREAM_BUFFER

       rc = streamWrite(rwPtr->pBuffer, rwPtr->ulBufferSize, streamPtr);
       if (rc == 8) rc = ERROR_TOO_MANY_BUFFERS;  // 5901;
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;

 case DDCMD_STATUS:     // 3

    if (streamPtr) {

       DDCMDSTATUS __far *statusPtr = (DDCMDSTATUS __far *)commonPtr;

       streamPtr->currentTime = wavestreamGetCurrentTime(streamPtr->wsParentPtr);

       // yes, give it a -pointer- to this stream's current time var (NOT the time itself)

       statusPtr->pStatus = &streamPtr->currentTime;    // should be a far pointer
       statusPtr->ulStatusSize = 4;     // or use sizeof(STATUS_PARM)
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;

 case DDCMD_CONTROL:    // 4

    if (streamPtr) {

       DDCMDCONTROL __far *controlPtr = (DDCMDCONTROL __far *)commonPtr;
       WAVESTREAM *wsPtr = streamPtr->wsParentPtr;
       USHORT cmd = (USHORT)controlPtr->ulCmd;

       switch(cmd) {
       case DDCMD_START: // 1
          rc = wavestreamStart(wsPtr);
          break;
       case DDCMD_STOP:  // 2
          rc = wavestreamStop(wsPtr);
          controlPtr->pParm = &wsPtr->timeBase;   // since wavestreamStop updated .timeBase
          controlPtr->ulParmSize = sizeof(ULONG); // give it a pointer to that var
          break;
       case DDCMD_PAUSE: // 3
          rc = wavestreamPause(wsPtr);
          controlPtr->pParm = &wsPtr->timeBase;   // since wavestreamPause updated .timeBase
          controlPtr->ulParmSize = sizeof(ULONG); // give it a pointer to that var
          break;
       case DDCMD_RESUME: // 4
          rc = wavestreamResume(wsPtr);
          break;
       case DDCMD_ENABLE_EVENT:  // 5
       case DDCMD_DISABLE_EVENT: // 6
          rc = ERROR_INVALID_REQUEST;
          break;

       case DDCMD_PAUSE_TIME: // 7 (newish)
          rc = streamPauseTime(streamPtr);
          break;
       case DDCMD_RESUME_TIME: // 8 (newish)
          rc = streamResumeTime(streamPtr);
          break;
       default:
          rc = ERROR_INVALID_REQUEST;
       }
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;
 case DDCMD_REG_STREAM: // 5


    if (streamPtr) {
       rc = ERROR_HNDLR_REGISTERED;
    }
    else {
       streamPtr = streamFindStreamSFN(((DDCMDREGISTER __far *)commonPtr)->ulSysFileNum);
       if (streamPtr == 0) {
          rc = ERROR_STREAM_CREATION;
       }
       else {
          rc = streamRegister((DDCMDREGISTER __far *)commonPtr, streamPtr);
       }
    }
    break;

 case DDCMD_DEREG_STREAM: // 6

    if (streamPtr) {
       streamDeregister(streamPtr);
       rc = 0;
    }
    else {
       rc = ERROR_INVALID_STREAM;
    }
    break;

 default:
    rc = ERROR_INVALID_FUNCTION;
 }

#ifdef TRACE_IDC_DDCMD
 tracePerf(TRACE_IDC_DDCMD_OUT, ((ULONG)function << 16) | _IF());  // high-word is function is USHORT, 0-6
#endif

 return rc;
}

// con printfs (moved down here to get them out of the way)
//ddprintf("~@ idcDDCMD, function=%u, streamPtr=%x\n", function,streamPtr);
//ddprintf("@idcDDCMD, function=DDCMD_SETUP, setupParmPtr=%p, setupSize=%lu, streamPtr=%x\n",
//           setupPtr->pSetupParm, setupPtr->ulSetupParmSize,streamPtr);
//ddprintf("@ idcDDCMD, function=DDCMD_READ,  streamPtr=%x\n", streamPtr);
//ddprintf("~@ idcDDCMD, function=DDCMD_WRITE, streamPtr=%x\n", streamPtr);
//int3();
//ddprintf("@idcDDCMD, function=DDCMD_STATUS,streamPtr=%x\n", streamPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, pParm=%p, ulParmSize=%lu, streamPtr=%x wsPtr=%x\n",
//           controlPtr->pParm, controlPtr->ulParmSize, streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=START, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=STOP,  streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=PAUSE, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=RESUME, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=EVENTxxx, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=PAUSETIME, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, cmd=RESUMETIME, streamPtr=%x wsPtr=%x\n", streamPtr, wsPtr);
//ddprintf("@idcDDCMD, function=DDCMD_CONTROL, streamPtr=%x\n", streamPtr);
//ddprintf("@idcDDCMD, function=DDCMD_REG_STREAM, streamPtr=%x\n", streamPtr);
//ddprintf("@idcDDCMD, function=DDCMD_DEREG_STREAM, streamPtr=%x\n", streamPtr);
//ddprintf("* leaving idcDDCMD, (rc=%lx)\n", rc);



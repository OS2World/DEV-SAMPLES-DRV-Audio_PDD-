//
// audiohw.c
// 27-Jan-99
//
//
// USHORT hwGetType(USHORT dataType, USHORT opType, USHORT lDev);
// USHORT hwSetType(USHORT devType, USHORT dataType, USHORT opType, USHORT lDev);

#include "cs40.h"

// datatype/optype to device list

AUDIOHW_T2DLIST t2dList[] = {
 {AUDIOHW_WAVE_PLAY,0,DATATYPE_WAVEFORM,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,PCM,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,DATATYPE_ALAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,DATATYPE_RIFF_ALAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,A_LAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,DATATYPE_MULAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,DATATYPE_RIFF_MULAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_PLAY,0,MU_LAW,OPERATION_PLAY},
 {AUDIOHW_WAVE_CAPTURE,0,DATATYPE_WAVEFORM,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,PCM,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,DATATYPE_ALAW,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,DATATYPE_RIFF_ALAW,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,A_LAW,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,DATATYPE_MULAW,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,DATATYPE_RIFF_MULAW,OPERATION_RECORD},
 {AUDIOHW_WAVE_CAPTURE,0,MU_LAW,OPERATION_RECORD},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},
 {AUDIOHW_INVALID_DEVICE,-1,-1,-1},  // last device is a no-device, always AUDIOHW_INVALID_DEVICE
};

#define MAX_T2D_ENTRIES ((sizeof(t2dList) / sizeof(AUDIOHW_T2DLIST)) -1) // -1 to leave last untouched


// ----------------------------------
// in: dataType |
//     opType   | to match
//     lDev     |
//out: devType of matching entry
//nts: original had devType as ULONG all around, I use USHORT
//     devType == 0 is a valid type (AUDIOHW_READ_DEVICE) though not used here, so -1 is rc on error

USHORT hwGetType(USHORT dataType, USHORT opType, USHORT lDev) {

 USHORT i;
 USHORT devType = AUDIOHW_INVALID_DEVICE;

 // allow invalid device to be mixed in t2dList, so check all 32 entries in t2d
 // though that is not a useful condition since that data/op type can just not be entered in list

 for (i=0; i < MAX_T2D_ENTRIES; i++) {

    if (t2dList[i].devType != AUDIOHW_INVALID_DEVICE  &&
        t2dList[i].dataType == dataType  &&
        t2dList[i].opType == opType      &&
        t2dList[i].lDev == lDev) {

       devType = t2dList[i].devType;
    }
 }

 return devType;
}

// ----------------------------------
// in: devType  |
//     dataType |
//     opType   | to match or add
//     lDev     |
//out: 0 if okay, else 87
//nts: sets matching record, or if no exact match adds entry (if room)

USHORT hwSetType(USHORT devType, USHORT dataType, USHORT opType, USHORT lDev) {

 USHORT i;
 USHORT rc = 0;
 USHORT match = 0xFFFF, firstAvail = 0xFFFF;

 // check if matches current entry

 for (i=0; i < MAX_T2D_ENTRIES; i++) {

    if (t2dList[i].devType != AUDIOHW_INVALID_DEVICE) {
       if (t2dList[i].dataType==dataType && t2dList[i].opType==opType && t2dList[i].lDev==lDev) {
          match = i;
       }
    }
    else {
       if (firstAvail == 0xFFFF) firstAvail = i;  // track first open slot in case need to insert
    }
 }

 // check if no match, and if not, check if room available

 if (match == 0xFFFF) {
    if (firstAvail >= MAX_T2D_ENTRIES) rc = 68; // CP error, "Too Many Names" (close enough)
 }

 // update or insert entry

 if (rc == 0) {
    t2dList[i].devType = devType;
    t2dList[i].dataType = dataType;
    t2dList[i].opType = opType;
    t2dList[i].lDev = lDev;
 }

 return rc;
}



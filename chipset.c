//
// chipset.c
// 4-Feb-99
//
// 27-Feb-99: to use type-F DMA:
//            pin control reg (i10): DTM (bit2) set to 1
//            also, use DEMAND mode for dma channel, rather than single mode
//
// UCHAR  chipsetGET(USHORT type, USHORT reg);
// VOID   chipsetSet(USHORT type, USHORT reg, UCHAR data);
// VOID   chipsetMCE(USHORT mode);
// USHORT chipsetSetDTM(USHORT dtm);
// USHORT chipsetInit(USHORT bp, USHORT cp, USHORT mode, USHORT make);
// USHORT chipsetIntPending(USHORT type);
// USHORT chipsetIntReset(USHORT type);
// UCHAR  chipsetWaitInit(VOID);
// UCHAR  chipsetWaitACI(VOID);

#include "cs40.h"

// local protos

static UCHAR iGET(USHORT reg);
static VOID  iSet(USHORT reg, UCHAR data);
static UCHAR xGET(USHORT reg);
static VOID  xSet(USHORT reg, UCHAR data);
static UCHAR ciGET(USHORT reg);
static VOID  ciSet(USHORT reg, UCHAR data);

// local data

static UCHAR  chipMake = 0;     // 0=CS, 0x100=Yamaha, 0x101=3DXG
static UCHAR  chipVer = 0;      // 715,4231; 4232(4236non-B); 4235,4236,4237,4238,4239
static USHORT chipMode = 0;     // 2 or 3

static USHORT basePort = 0;     // codec base (WSS base+4, though WSS used only in old WSS lingo)
static USHORT dataPort = 0;     // data (basePort+1)
static USHORT statusPort = 0;   // status (basePort+2)

static USHORT controlPort= 0;   // control port
static USHORT cindexPort = 0;   // control index port
static USHORT cdataPort = 0;    // control data port


// ---------------------
// in: index reg to read
//out: data at reg
//nts: get index-indirect reg
//     compiler is typically more efficent when using dataPort instead of basePort+1

static UCHAR iGET(USHORT reg) {

 UCHAR data;
 UCHAR mt;              // MCE and TRD bits

 _cli_();
 mt = inp(basePort) & 0x60;
 outp(basePort,mt|reg);
 data = inp(dataPort);
 _sti_();

 return data;
}


// ----------------------
// in: index reg to write
//     data to write
//out: n/a
//nts: set index-indirect reg

static VOID iSet(USHORT reg, UCHAR data) {

 UCHAR mt;              // MCE and TRD bits

 _cli_();
 mt = inp(basePort) & 0x60;
 outp(basePort,mt|reg);
 outp(dataPort,data);
 _sti_();

 return;
}


// -----------------------
// in: x-index reg to read
//out: data at reg
//nts: get extended reg
//     requires mode3
//     ; d7   d6   d5   d4   d3    d2  d1  d0
//     ;XA3  XA2  XA1  XA0  XRAE  XA4 rez  ACF
//     ACF is not in 4235/9 anymore (for ADPCM capture freeze)

static UCHAR xGET(USHORT reg) {

 USHORT tReg;
 UCHAR data;
 UCHAR mt;              // MCE and TRD bits

 tReg = (reg >> 2) & 4; // XA4 to d2 (only want d2 bit)
 reg = reg << 4;        // XA3-0 to d7-4 (d3-0 cleared by this)
 reg = reg|0x10|tReg;   // set XRAE, and merge d7-d4,d2

 _cli_();
 mt = inp(basePort) & 0x60;
 outp(basePort,mt|23);  // select I23, extended reg access (and ACF bit) (R1 is now x-addr reg)
 data = inp(dataPort);  // read first to get ACF
 reg = reg|(data & 1);  // x-reg address data
 outp(dataPort,reg);    // after this write, R1 is now x-data reg
 data = inp(dataPort);  // read data of x-reg
 _sti_();

 return data;
}


// ------------------------
// in: x-index reg to write
//     data to write
//out: n/a
//nts: set extended reg
//     requires mode3
//     ; d7   d6   d5   d4   d3    d2  d1  d0
//     ;XA3  XA2  XA1  XA0  XRAE  XA4 rez  ACF

static VOID xSet(USHORT reg, UCHAR data) {

 USHORT tReg;
 UCHAR tData;
 UCHAR mt;              // MCE and TRD bits

 tReg = (reg >> 2) & 4; // XA4 to d2 (only want d2 bit)
 reg = reg << 4;        // XA3-0 to d7-4 (d3-0 cleared by this)
 reg = reg|0x10|tReg;   // set XRAE, and merge d7-d4,d2

 _cli_();
 mt = inp(basePort) & 0x60;
 outp(basePort,mt|23);  // select I23, extended reg access (and ACF bit) (R1 is now x-addr reg)
 tData = inp(dataPort); // read first to get ACF
 reg = reg|(tData & 1); // x-reg address data
 outp(dataPort,reg);    // after this write, R1 is now x-data reg
 outp(dataPort,data);   // write data to x-reg
 _sti_();

 return;
}


// -----------------------------
// in: control index reg to read
//out: data at reg
//nts: get index-indirect control reg

static UCHAR ciGET(USHORT reg) {

 UCHAR data;

 _cli_();
 outp(cindexPort,reg);
 data = inp(cdataPort);
 _sti_();

 return data;
}


// ------------------------------
// in: control index reg to write
//out: n/a
//nts: set index-indirect control reg

static VOID ciSet(USHORT reg, UCHAR data) {

 _cli_();
 outp(cindexPort,reg);
 outp(cdataPort, data);
 _sti_();

 return;
}


// -------------------------------------------------------------------------------
// in: type ='i' for index
//           'x' for extended
//           'c' for index-control
//           'C' for direct control
//           'd' for direct (not usually used)
//     reg to read
//out: reg data
//nts: for 's' use chipsetStatus(READ_STATUS)
//     'C' (direct control access) is not used by 715

UCHAR chipsetGET(USHORT type, USHORT reg) {

 UCHAR data = 0xFF;

 if (type == 'i') {
    data = iGET(reg);
 }
 else if (type == 'x') {
    data = xGET(reg);
 }
 else if (type == 'c') {
    data = ciGET(reg);
 }
 else if (type == 'C') {
    data = inp(controlPort+reg);
 }
 else if (type == 'd') {
    data = inp(basePort+reg);  // typically not used (specific chipsetGETInit() used instead)
 }

 return data;
}


// -------------------------------------------------------------------------------
// in: type ='i' for index
//           'x' for extended
//           'c' for index-control
//           'C' for direct control
//           'd' for direct (not usually used)
//     reg to write
//     data to write
//out: n/a
//nts: for 's' use chipsetStatus(RESET_STATUS)
//     'C' (direct control access) is not used by 715

VOID chipsetSet(USHORT type, USHORT reg, UCHAR data) {

 if (type == 'i') {
    iSet(reg,data);
 }
 else if (type == 'x') {
    xSet(reg,data);
 }
 else if (type == 'c') {
    ciSet(reg,data);
 }
 else if (type == 'C') {
    outp(controlPort+reg,data);
 }
 else if (type == 'd') {
    outp(basePort+reg,data);    // typically not used
 }

 return;
}


// -------------------------------------
// in: state to set dtm (1=DTM, 0=no DTM)
//out: previous state (1 or 0)
//nts:

USHORT chipsetSetDTM(USHORT dtmFlag) {

 USHORT lastState;
 UCHAR data;

 data = (chipsetGET('i', PIN_CONTROL_REG));
 lastState = (data & 4) != 0;

 if (lastState != dtmFlag) {
    data = data & ~4;
    if (dtmFlag) data = data | 4;
    chipsetSet('i', PIN_CONTROL_REG, data);
 }

 return lastState;
}


// -------------------------------------
// in: bp = base port
//     cp = control port (can be 0)
//     mode = mode (2, 3 also for cs)
//     make = chip make (0=CS, 0x100=Yamaha, 0x101=3DXG)
//out:
//nts:

USHORT chipsetInit(USHORT bp, USHORT cp, USHORT mode, USHORT make) {

 USHORT rc = 0;
 UCHAR data;

 if ((bp < 0x100) || (cp && (cp < 0x100)) || (mode < 2) || (mode > 2)) {
    rc = 1;
    goto ExitNow;
 }

 basePort   = bp;
 dataPort   = bp+1;
 statusPort = bp+2;
 controlPort= cp;

 chipMake = make;
 chipVer = 0;   // ref it for now

 if (cp) {
    if (make & 0x100) {
       cindexPort = cp;       // 715
       cdataPort = cp+1;
    }
    else {
       cindexPort = cp+3;     // CS
       cdataPort = cp+4;
    }
 }
 chipMode = mode;

 rc = chipsetWaitInit();
 if (rc) {
ddprintf("1:chipsetWaitInit()=%u\n",rc);
    goto ExitNow;
 }

 chipsetSet('i', MODE_AND_ID_REG, 0x40);        // set mode 2

 data = chipsetGET('i',INTERFACE_CONFIG_REG);
 data = data & 0xDC;                            // disable PEN/CEN
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 chipsetSet('i', LEFT_OUTPUT_CONTROL, 0x8F);    // output mute and down
 chipsetSet('i', RIGHT_OUTPUT_CONTROL, 0x8F);

 chipsetSet('i', MONO_IO_CONTROL, 0xCF);        // mom mute and down
 chipsetSet('i', LOOPBACK_CONTROL_REG, 0xFC);   // monitor mute and down

 chipsetSet('i', LEFT_DAC_OUTPUT_CONTROL, 0xBF); // DAC output mute and down
 chipsetSet('i', RIGHT_DAC_OUTPUT_CONTROL, 0xBF);

 chipsetMCE(1);

 // full calibration uses bits 4/3, available in 4232+ (incl.4235/9)
 // difference between full and just convertor (4231, and org source) is
 // the analog mixer is not calibrated -- see the benefits of each calibration
 // mode type in the docs (CS4232, p.53 talks about why might want to use no calibration)
 // note that a full calibration is always done at power-up, and this is the init code...

 data = chipsetGET('i', INTERFACE_CONFIG_REG) | 0x18; // full calibration
 chipsetSet('i', INTERFACE_CONFIG_REG, data);

 chipsetMCE(0);

 rc = chipsetWaitInit();                // yes, have to wait on INIT bit after calibrate (by spec)
 if (rc) {
ddprintf("2:chipsetWaitInit()=%u\n",rc);
    goto ExitNow;
 }

 rc = chipsetWaitACI();
 if (rc) {
ddprintf("chipsetWaitACI()=%u\n",rc);
    goto ExitNow;
 }

 // playback/capture mode change enables off (PMCE/CMCE, not for 4231 but yes 715),
 // also timer feature disable

 chipsetSet('i', ALT_FEATURE_ENABLE_1, 0);      

 // mute and down inputs to 0 dB gain

 chipsetSet('i', LEFT_AUX1_INPUT_CONTROL,  0x88);
 chipsetSet('i', RIGHT_AUX1_INPUT_CONTROL, 0x88);
 chipsetSet('i', LEFT_AUX2_INPUT_CONTROL,  0x88);
 chipsetSet('i', RIGHT_AUX2_INPUT_CONTROL, 0x88);
 chipsetSet('i', LEFT_LINE_INPUT_CONTROL,  0x88);
 chipsetSet('i', RIGHT_LINE_INPUT_CONTROL, 0x88);

 // tropez code did the monitor and DAC output controls AGAIN... I don't

 outp(statusPort,0);    // clear all int flags before enabling interrupt pin (done next)

 // enable interrupts (better not get spurious ints since handler won't own IRQ until start)

 data = 2 | (chipsetGET('i', PIN_CONTROL_REG) & ~3);
 chipsetSet('i', PIN_CONTROL_REG, data);

ExitNow:
 return rc;
}


// -------------------------------------
// in: INT type to check
//out: 0=not pending, 1=pending
//nts:

USHORT chipsetIntPending(USHORT type) {

 UCHAR data = chipsetGET('i',ALT_FEATURE_STATUS);

 switch(type) {
 case AUDIOHW_WAVE_PLAY:
    data = data & PLAY_INTERRUPT;
    break;
 case AUDIOHW_WAVE_CAPTURE:
    data = data & CAPTURE_INTERRUPT;
    break;
 case AUDIOHW_TIMER:
    data = data & TIMER_INTERRUPT;
    break;
 case -1:
    data = data & ALL_INTERRUPTS;
    break;
 default:
    data = 0;
 }

 return (data != 0);
}


// -------------------------------------
// in: INT type to clear
//out: n/a
//nts:

VOID chipsetIntReset(USHORT type) {

 UCHAR data = 0;

 switch(type) {
 case AUDIOHW_WAVE_PLAY:
    data = CLEAR_PI;
    break;
 case AUDIOHW_WAVE_CAPTURE:
    data = CLEAR_CI;
    break;
 case AUDIOHW_TIMER:
    data = CLEAR_TI;
    break;
 }

 if (data) chipsetSet('i',ALT_FEATURE_STATUS,data);

 return;
}


// -------------------------------------
// in: n/a
//out: 0=okay, else timeout error
//nts: wait for INIT bit to clear
//     tropez code first looked/waited for INIT bit to go from 0 -> 1, which seems pointless
//     and not well supported for doing it, so I don't (it also read it once more before returning)
//
//     had this timeout at first use once (twice actually) but went away after power cycle
//     may have been a messed up config since it was during initial testing (which I'm still
//     doing -- 7-Feb-99 00:51 -- maybe the DMA isn't running, will be checking right after this)
//
//     above (timeout) was likely related to the fact tat I was using dataPort instead of basePort!
//     ... still checking out no sound problem (might be this?)

UCHAR chipsetWaitInit(VOID) {

 USHORT cnt = 0;
 UCHAR  data = inp(basePort);

 while (data & 0x80) {
    iodelay(WAIT_1US);
    data = inp(basePort);
    cnt++;
    if (cnt == 65535) break;  // 65.5 millseconds is way too long, timeout
 }

 return (data & 0x80); // just return bit7
}


// -------------------------------------
// in: n/a
//out: 0=okay, else timeout error
//nts: wait for ACI bit to clear (auto-calibration)
//     full calibration can take 450 sample periods:
//     - at 44.1kHz that's 450/44100=10.2 milliseconds (4235+ always run internally at 44.1kHz)
//     - at  5.5kHz that's 450/5512 =81.6 milliseconds
//     since quite long, using WAIT_1MS (1 ms) wait rather than lots of WAIT_1US (1 us)

UCHAR chipsetWaitACI(VOID) {

 USHORT cnt = 0;
 UCHAR  data = iGET(ERROR_STATUS_INIT_REG);  // i11

 while (data & ACI_BIT) {
    iodelay(WAIT_1MS);
    data = iGET(ERROR_STATUS_INIT_REG);
    cnt++;
    if (cnt > 162) break;  // 81.64 milliseconds is longest is should ever be, timeout if longer
 }                         // using double since WAIT_1MS is 2000 loops, which may be off a bit

 return (data & ACI_BIT);  // just return bit5
}


// -------------------------------------
// in: mode 0=off, else on
//out:
//nts:

VOID chipsetMCE(USHORT mode) {

 UCHAR data;

 data = inp(basePort);
 if (mode) {
    data = data | MCE_ON;
 }
 else {
    data = data & MCE_OFF;
 }
 outp(basePort, data);

 return;
}


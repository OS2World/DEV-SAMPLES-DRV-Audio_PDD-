//
// whelper.h
// 25-Jan-99
// helper pragma aux

// ---------------
// use _cli_/_sti_
// instead of cli()/sti() in case ever need to manage SMP,
// but must always use in pairs since pushf/popf
// can still use cli()/sti()

 void _cli_(void);
 #pragma aux _cli_ = \
   "pushf" \
   "cli"   \
  parm nomemory \
  modify nomemory exact [];

 void _sti_(void);
 #pragma aux _sti_ = \
   "popf" \
  parm nomemory \
  modify nomemory exact [];


// -------------------------------------------------------------------
// MEMSET() is a 16-bit version of the memset()
// MEMCPY() is new, based on the MEMSET() done here (will have a 386 CPU)
// cld expected to be in force since not done here
// -------------------------------------------------------------------
// ASM_MEMSET() dword-aligns on destination
// ASM_MEMCPY() dword-aligns on destination (overlapped is not allowed)
// -------------------------------------------------------------------

 VOID ASM_MEMSET(CHAR _far *destPtr, USHORT value, USHORT length);
 #define MEMSET(strg,value,length)  ASM_MEMSET((CHAR _far *)strg,(USHORT)value,(USHORT)length)

// will use value as a 16-bit value so can write 7FFF,
// so send 16-bit value (unless count is odd AND count >=32 then only byte is used)
// if count is < 32 then WORD is always used (unless only 1 byte!)

 #pragma aux ASM_MEMSET=\
         "cld"          \
         "test cx,cx"   \
         "jz SX"        \
         "cmp cx,32"    /* 1-31 bytes then by words */ \
         "jb SW"        \
         "test cx,1"    \  
         "jz L16"       \
         "mov ah,al"     /* odd then only byte is used */ \
 "L16:    mov bx,ax"    \
         "shl eax,16"   \
         "mov ax,bx"    \
         "test di,3"    \
         "jz LX"        /* dest is dword aligned */ \
         "test di,1"    \
         "jz LW"        /* dest is word aligned */  \
 "LB:     stosb"        /* dest is byte aligned */  \
         "dec cx"       \
         "test di,2"    \  
         "jz LX"        /* dest is dword aligned */ \
 "LW:     stosw"        \
         "sub cx,2"     \
 "LX:     mov bl,cl"    \
         "shr cx,2"     \
         "rep stosd"    \
         "and bl,3"     \
         "jz SX"        /* all data written */ \
         "mov cl,bl"    \
 "SW:     shr cx,1"     \
         "rep stosw"    /* write word, if available (not done if cx=0)*/ \
         "jnc SX"       \
 "SB:     stosb"        /* write last byte */ \
 "SX:" \
  parm nomemory [es di][ax][cx] \
  modify [bx];

 // ----------------------------------------------------------------
 // have to use [dx si] since [ds si] not valid for this memory model
 // note: source is probably more likely to be aligned...and since always
 // dwording bytes, destination should also be!

 VOID ASM_MEMCPY(CHAR _far *destPtr, CHAR _far *srcPtr, USHORT length);
 #define MEMCPY(dPtr,sPtr,length) ASM_MEMCPY((CHAR _far *)dPtr,(CHAR _far *)sPtr,(USHORT)length)

 #pragma aux ASM_MEMCPY=\
         "cld"          \
         "push ds"      \
         "mov ds,dx"    \
         "test cx,cx"   \
         "jz SX"        \
         "test cx,32"   \
         "jb SB"        \
         "test di,3"    \
         "jz LX"        \
         "test di,1"    \
         "jz LW"        \
 "LB:     movsb"        \
         "dec cx"       \
         "test di,2"    \
         "jz LX"        \
 "LW:     movsw"        \
         "sub cx,2"     \
 "LX:     mov bl,cl"    \
         "shr cx,2"     \
         "rep movsd"    \
         "and bl,3"     \
         "jz SX"        \
         "mov cl,bl"    \
 "SB:     rep movsb"    \
 "SX:     pop ds"       \
  parm nomemory [es di][dx si][cx] \
  modify [bx];


USHORT IsRing3(VOID);
#pragma aux IsRing3=\
       "push cs"  \
       "pop ax "  \
       "and ax,3" \
 value [ax]       \
 parm nomemory    \
 modify nomemory;

USHORT CanCPUID(VOID);
#pragma aux CanCPUID=\
       "push ebx"    \
       "pushfd"      \
       "pop eax"     \
       "mov ebx,eax" \
       "xor eax,200000h" /* toggle bit21 */ \
       "push eax"    \
       "popfd"       \
       "pushfd"      \
       "pop eax"     \
       "cmp eax,ebx" \
       "mov ax,0"    \
       "jz NoID"     \
       "inc ax"      \
"NoID:  pop ebx"     \
 parm nomemory       \
 value [ax]          \
 modify nomemory;

USHORT CanRDTSC(VOID);
#pragma aux CanRDTSC = \
       "push ebx"   \
       "push ecx"   \
       "push edx"   \
       "sub eax,eax"\
       "db 0Fh,0A2h"\
       "cmp eax,1"  \
       "mov ax,0"   \
       "jb NoGo"    \
       "mov eax,1"  \
       "db 0Fh,0A2h"\
       "test dl,10h"\
       "mov ax,0"   \
       "jz NoGo"    \
       "inc ax"     \
"NoGo:  pop edx"    \
       "pop ecx"    \
       "pop ebx"    \
 parm nomemory      \
 value [ax]         \
 modify nomemory;


// set ticks to max (have to), but especially to count-by-1 (mode 2)
// only call once, at driver init (suppose that's safe), but probably only really when testing

USHORT ResetTimer0(VOID);
#pragma aux ResetTimer0 = \
       "pushf"       \
       "cli"         \
       "mov al,34h"  \
       "out 43h,al"  \
       "sub al,al"   \
       "out 40h,al"  \
       "out 40h,al"  \
       "popf"        \
 parm nomemory       \
 value [ax]          \
 modify nomemory;


USHORT ReadTimer0(VOID);
#pragma aux ReadTimer0 = \
       "sub al,al"   \
       "push cx"     \
       "pushf"       \
       "cli"         \
       "out 43h,al"  \
       "in  al,40h"  \
       "mov ah,al"   \
       "in  al,40h"  \
       "popf"        \
       "pop cx"      \
       "xchg al,ah"  \
 parm nomemory       \
 value [ax]          \
 modify nomemory;


void pushAX(void);
#pragma aux pushAX = \
   "push ax" \
   parm nomemory \
   modify nomemory exact [];

void pushBX(void);
#pragma aux pushBX = \
   "push bx" \
   parm nomemory \
   modify nomemory exact [];

void pushCX(void);
#pragma aux pushCX = \
   "push cx" \
   parm nomemory \
   modify nomemory exact [];

void pushDX(void);
#pragma aux pushDX = \
   "push dx" \
   parm nomemory \
   modify nomemory exact [];

void pushSI(void);
#pragma aux pushSI = \
   "push si" \
   parm nomemory \
   modify nomemory exact [];

void pushDI(void);
#pragma aux pushDI = \
   "push di" \
   parm nomemory \
   modify nomemory exact [];

void pushBP(void);
#pragma aux pushBP = \
   "push bp" \
   parm nomemory \
   modify nomemory exact [];


void popAX(void);
#pragma aux popAX = \
   "pop ax" \
   parm nomemory \
   modify nomemory exact [];

void popBX(void);
#pragma aux popBX = \
   "pop bx" \
   parm nomemory \
   modify nomemory exact [];

void popCX(void);
#pragma aux popCX = \
   "pop cx" \
   parm nomemory \
   modify nomemory exact [];

void popDX(void);
#pragma aux popDX = \
   "pop dx" \
   parm nomemory \
   modify nomemory exact [];

void popSI(void);
#pragma aux popSI = \
   "pop si" \
   parm nomemory \
   modify nomemory exact [];

void popDI(void);
#pragma aux popDI = \
   "pop di" \
   parm nomemory \
   modify nomemory exact [];

void popBP(void);
#pragma aux popBP = \
   "pop bp" \
   parm nomemory \
   modify nomemory exact [];

void nop(void);
#pragma aux nop = \
   "nop" \
   parm nomemory \
   modify nomemory exact [];




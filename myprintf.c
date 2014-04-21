//
// myprintf()
// 25-Jan-99
//
// see ras40's stuff for perhaps a more recent version
//
// Note: if "%s" then string pointer in the ,... section must be a FAR POINTER! in the , ...
//       part of the string:  ddprintf(dest,"%s","a string"); will fail because compiler
//       only passes near pointer for "a string" -- either pass a far pointer, or use
//       ddprintf(dest,"%s",(char __far *)"a string"), or don't use strings in the arg list,
//       but instead actual (far) pointers
//
// static VOID PrintCom(USHORT basePort, UCHAR byte);
// static VOID ddputstring (char far *St);
// static char far *ddprintf_DecWordToASCII(char far *StrPtr, WORD wDecVal, WORD Option);
// static char far *ddprintf_DecLongToASCII(char far *StrPtr, DWORD lDecVal,WORD Option);
// static char far *ddprintf_HexWordToASCII(char far *StrPtr, WORD wHexVal, WORD Option);
// static char far *ddprintf_HexLongToASCII(char far *StrPtr, DWORD wHexVal, WORD Option);
// VOID __cdecl ddprintf (char far *DbgStr , ...);
//
// writes to com port 2 a ddprintf() string (up to MAX_STR_SIZE bytes)
// (or could write to screen at init (DosPutMessage) or basedev (devhelp save_message)
//
// 16-bit version, so %u limited to word-size data (use %lh for 32-bit data (%lx, too)
// though %p always goes after seg:off so expects 32-bit data
//
// should only include in debug builds (probably, or at least move data and code to INIT segment)

//#define DRV_16 (can only find it used in a PAS16 mixer thing)

//#define FAST_OUT  // only output strings starting with ~ if defined

#include "cs40.h"

#pragma code_seg ("_TEXT");
#pragma data_seg ("_DATA","DATA");

#define MAX_STR_SIZE 260

#define LEADING_ZEROES          0x8000
#define SIGNIFICANT_FIELD       0x0007

static char ddhextab[]="0123456789ABCDEF";
static char dd_BuildString[MAX_STR_SIZE];

// Following declarations are need for string display from basedev
//#define MSG_REPLACEMENT_STRING 1178   /* replacement string */
//UCHAR szStringOut [CCHMAXPATH];
//static MSGTABLE StringOutMsg = {MSG_REPLACEMENT_STRING, 1, NULL};

// dx+5 is LSR register
// dx is baseport (also data register)

VOID PrintCom(USHORT basePort, UCHAR byte);  // aux asm macro
#pragma aux PrintCom =\
        "add dx,5"    \
"wait4:  in al,dx"    \
        "test al,20h" \
        "jz wait4"    \
        "sub dx,5"    \
        "mov al,ah"   \
        "out dx,al"   \
 parm caller nomemory [dx] [ah]  \
 modify exact [ax dx];


static VOID ddputstring (char far *St) {

// putmessage
   //int      iMsgLength;
   //char far *TempSt;
   //
   //TempSt = St;
   //iMsgLength = 0;
   //while (*TempSt != '\0')
   //   {
   //   TempSt++;
   //   iMsgLength++;
   //   }               // Should strcat a \r\n to the string.
   //DosPutMessage (1, iMsgLength, St);         // For ring-3 initialization

// savemessage
   //chhTrace_Save_Message (St);
   //chhStringOutMsg.MsgStrings[0] = St;
   //chhDevHelp_Save_Message ((NPBYTE)&StringOutMsg);

   //dd_strcpy (szStringOut, (NPBYTE)St);      // For ring-0 initialization
   //szStringOut [CCHMAXPATH-1] = '\0';


// added this to write to com2 (25-Jan-99)
// commessage
   char al = 0;
   while (*St) {
      al = *St++;
      PrintCom(0x2F8,al);
   }
//   if (al) {
//      PrintCom(0x2F8,13);
//      PrintCom(0x2F8,10);
//   }
}

static char far *ddprintf_DecWordToASCII(char far *StrPtr, WORD wDecVal, WORD Option) {

  BOOL fNonZero=FALSE;
  WORD Digit;
  WORD Power=10000;

  while (Power)
     {
     Digit=0;
     while (wDecVal >=Power)                   //Digit=wDecVal/Power;
        {
        Digit++;
        wDecVal-=Power;
        }

     if (Digit)
        fNonZero=TRUE;

     if (Digit ||
         fNonZero ||
         (Option & LEADING_ZEROES) ||
         ((Power==1) && (fNonZero==FALSE)))
         {
         *StrPtr=(char)('0'+Digit);
         StrPtr++;
         }

     if (Power==10000)
        Power=1000;
     else if (Power==1000)
        Power=100;
     else if (Power==100)
        Power=10;
     else if (Power==10)
        Power=1;
     else
        Power=0;
     } // end while

  return (StrPtr);
}


static char far *ddprintf_DecLongToASCII(char far *StrPtr, DWORD lDecVal,WORD Option) {

   BOOL  fNonZero=FALSE;
   DWORD Digit;
   DWORD Power=1000000000;                      // 1 billion

   while (Power)
      {
      Digit=0;                                                                        // Digit=lDecVal/Power
      while (lDecVal >=Power)                   // replaced with while loop
         {
         Digit++;
         lDecVal-=Power;
         }

      if (Digit)
         fNonZero=TRUE;

      if (Digit ||
          fNonZero ||
          (Option & LEADING_ZEROES) ||
          ((Power==1) && (fNonZero==FALSE)))
         {
         *StrPtr=(char)('0'+Digit);
         StrPtr++;
         }

      if (Power==1000000000)                    // 1 billion
         Power=100000000;
      else if (Power==100000000)
         Power=10000000;
      else if (Power==10000000)
         Power=1000000;
      else if (Power==1000000)
         Power=100000;
      else if (Power==100000)
         Power=10000;
      else if (Power==10000)
         Power=1000;
      else if (Power==1000)
         Power=100;
      else if (Power==100)
         Power=10;
      else if (Power==10)
         Power=1;
      else
         Power=0;
      }
   return (StrPtr);
}


static char far *ddprintf_HexWordToASCII(char far *StrPtr, WORD wHexVal, WORD Option) {

   BOOL fNonZero=FALSE;
   WORD Digit;
   WORD Power=0xF000;
   WORD ShiftVal=12;

   while (Power)
      {
      Digit=(wHexVal & Power)>>ShiftVal;
      if (Digit)
         fNonZero=TRUE;

      if (Digit ||
          fNonZero ||
          (Option & LEADING_ZEROES) ||
          ((Power==0x0F) && (fNonZero==FALSE)))
         //*StrPtr++=(char)('0'+Digit);
         *StrPtr++=ddhextab[Digit];

      Power>>=4;
      ShiftVal-=4;
      } // end while

   return (StrPtr);
}


static char far *ddprintf_HexLongToASCII(char far *StrPtr, DWORD wHexVal, WORD Option) {

   BOOL  fNonZero=FALSE;
   DWORD Digit;
   DWORD Power=0xF0000000;
   DWORD ShiftVal=28;

   while (Power)
      {
      Digit=(wHexVal & Power)>>ShiftVal;
      if (Digit)
         fNonZero=TRUE;

      if (Digit ||
          fNonZero ||
          (Option & LEADING_ZEROES) ||
          ((Power==0x0F) && (fNonZero==FALSE)))
          *StrPtr++=ddhextab[Digit];

      if (Power==0xF0000000)                  // 1 billion
         Power=0xF000000;
      else if (Power==0xF000000)
         Power=0xF00000;
      else if (Power==0xF00000)
         Power=0xF0000;
      else if (Power==0xF0000)
         Power=0xF000;
      else if (Power==0xF000)
         Power=0xF00;
      else if (Power==0xF00)
         Power=0xF0;
      else if (Power==0xF0)
         Power=0xF;
      else Power=0;

      ShiftVal-=4;
      } // end while

   return (StrPtr);
}

// saveregs is not saving all regs! so have to do it myself to be sure all are really saved
// esp. es:bx (can't use pusha/popa since need bp)
// diff between __saveregs and not is just es (probably all that's called for for __cdecl)
// si/di is already saved

VOID __cdecl ddprintf (char far *DbgStr , ...) {

   char far *BuildPtr; //=dd_BuildString;
   char far *pStr;     //=(char far *) DbgStr;
   char far *SubStr;
   union {
    VOID    far *VoidPtr;
    WORD    far *WordPtr;
    DWORD   far *LongPtr;
    DWORD far *StringPtr;
   } Parm;
   WORD wBuildOption;

// !!!
// fast out

#ifdef FAST_OUT
 if (*DbgStr != '~') return;  // only let strings starting with ~ through
#endif

   BuildPtr = dd_BuildString;
   pStr = DbgStr;

   Parm.VoidPtr=(VOID far *) &DbgStr;
   Parm.StringPtr++;                            // skip size of string pointer

   while (*pStr) {
      
      if (BuildPtr >= (char far *) &dd_BuildString[MAX_STR_SIZE-2]) break; // don't overflow target

      switch (*pStr)
      {
         case '%':
            wBuildOption=0;
            pStr++;
            if (*pStr=='0')
               {
               wBuildOption|=LEADING_ZEROES;
               pStr++;
               }

            //if (*pStr=='u')    // always unsigned
            //   pStr++;        // this means %u won't work!  buggy crap

            switch(*pStr)
               {
               case 'x':
               case 'X':
                  BuildPtr=ddprintf_HexWordToASCII(BuildPtr, *Parm.WordPtr++,wBuildOption);
                  pStr++;
                  continue;

               case 'u':
               case 'd':
                  BuildPtr=ddprintf_DecWordToASCII(BuildPtr, *Parm.WordPtr++,wBuildOption);
                  pStr++;
                  continue;

               case 's':
                  SubStr=(char far *)*Parm.StringPtr;
                  while (*BuildPtr++ = *SubStr++);
                  Parm.StringPtr++;
                  BuildPtr--;  // remove the \0
                  pStr++;
                  continue;

               case 'p':
                  BuildPtr=ddprintf_HexLongToASCII(BuildPtr, *Parm.LongPtr++,wBuildOption);
                  pStr++;
                  continue;

               case 'l':
                  pStr++;
                  switch (*pStr)
                  {
                  case 'x':
                  case 'X':
                     BuildPtr=ddprintf_HexLongToASCII(BuildPtr, *Parm.LongPtr++,wBuildOption);
                     pStr++;
                     continue;

                  case 'u':
                  case 'd':
                     BuildPtr=ddprintf_DecLongToASCII(BuildPtr, *Parm.LongPtr++,wBuildOption);
                     pStr++;
                     continue;

                  } // end switch
                  continue;    // dunno what he wants

               case 0:
                  continue;
               } // end switch
            break;

         case '\\':   // that means a literal '\' is in the stream, not just a '\n' (binary 10)
         case '\r':   // CR, already is literal (assumed "msg\n" only stores a binary 10)
            break;

         case '\n':
            *BuildPtr++=13;
            *BuildPtr++=10;
            pStr++;
            continue;

      } // end switch

      *BuildPtr++=*pStr++;
   } // end while

   *BuildPtr=0;                                 // cauterize the string
   ddputstring((char far *) dd_BuildString);    // display

   return;
}



//
// header.c
// 25-Jan-99
//

#pragma code_seg ("_INITTEXT");
#pragma data_seg ("_HEADER","DATA");

#include "cs40.h"


DEV_HEADER header = {
 -1,
 DA_CHAR | DA_IDCSET | DA_NEEDOPEN | DA_USESCAP,
 (PFNENTRY) stratMode2,
 (PFNENTRY) idcDDCMD,
 {'C','S','A','U','D','1','$',' '},
 0,0,
 DC_INITCPLT | DC_IOCTL2
};


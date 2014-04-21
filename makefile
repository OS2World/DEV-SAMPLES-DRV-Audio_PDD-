# cs40.sys makefile watcom 10a
# 25-Jan-98
#
#- Set the environmental variables for compiling
#
.ERASE

.SUFFIXES:
.SUFFIXES: .sys .obj .asm .inc .def .lrf .ref .lst .sym .map .c .cpp .h .lib

NAME   = cs40
WMAPNAME = mapfile

.BEFORE
!ifndef %WATCOM
   set WATCOM=d:\dev\comp\wc
!endif

   set INCLUDE=\dev\ddk\base\h;$(%WATCOM)\H

#########################################
# Options for Watcom 16-bit C compiler
#########################################
#  -bt=os2   = Build target OS is OS/2
#  -ms       = Memory model small
#  -3        = Enable use of 80386 instructions
#  -4        = Optimize for 486 (assumes -3)
#  -5        = Optimize for Pentium (assumes -3)
#  -j        = char default is unsigned
#  -d1       = Include line number info in object
#              (necessary to produce assembler listing)
#  -d2       = Include debugging info for ICAT
#              (necessary to produce assembler listing)
#  -o        = Optimization - i = enable inline intrinsic functions
#                             r = optimize for 80486 and pentium pipes
#                             s = space is preferred to time
#                             l = enable loop optimizations
#                             a = relax aliasing constraints
#                             n = allow numerically unstable optimizations
#  -od to disable optimazations (use this and -d2 -hc for icat)
#  -s        = Omit stack size checking from start of each function
#  -zl       = Place no library references into objects
#  -wx       = Warning level set to maximum (vs 1..4)
#  -zfp      = Prevent use of FS selector
#  -zgp      = Prevent use of GS selector
#  -zq       = Operate quietly
#  -zm       = Put each function in its own segment
#  -zu       = Do not assume that SS contains segment of DGROUP
# -hc = codeview (-hw=watcom  -hd=dwarf)
CC=wcc
##CFLAGS=-ms -5 -bt=os2 -d2 -oi -s -j -wx -zl -zfp -zgp -zq -zu -hc
#CFLAGS=-ms -5 -bt=os2 -od -d2 -s -j -wx -zl -zfp -zgp -zq -zu -hc

CFLAGS=-ms -5 -bt=os2 -d1 -oi -otexan -s -j -wx -zl -zfp -zgp -zq -zu

#########################################
# Options for Watcom assembler
#########################################
#  -bt=os2   = Build target OS is OS/2
#  -d1       = Include line number info in object
#              (necessary to produce assembler listing)
#  -i        = Include list
#  -zq       = Operate quietly
#  -3p       = 80386 protected-mode instructions
#
ASM=wasm
AFLAGS=-d1 -zq -5p
LINK=wlink

#########################################
# Inference rules
#########################################

.c.obj: .AUTODEPEND
     $(CC) $(CFLAGS) $*.c
     wdisasm -l -s -b -e -p $*

.asm.obj: .AUTODEPEND
     $(ASM) $(AFLAGS) $*.asm
     wdisasm -l -s -b -e -p $*

#########################################
# Object file list
#########################################

OBJS1=segsetup.obj header.obj strat.obj
OBJS2=audiobuf.obj audiohw.obj dma.obj irq.obj
OBJS3=wavaudio.obj wavestrm.obj stream.obj
OBJS4=waveplay.obj waverec.obj
OBJS5=ioctl.obj ssm_idc.obj
OBJS7=chipset.obj
OBJS8=malloc.obj
OBJS9=myprintf.obj init.obj

OBJS=$(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(OBJS5) $(OBJS7) $(OBJS8) $(OBJS9)

all: $(NAME).sys $(NAME).sym

$(NAME).lrf: makefile
   @%write $^@ system os2 dll
   @%write $^@ option quiet
   @%write $^@ option verbose
   @%write $^@ option caseexact
   @%write $^@ option symfile=$(NAME).dbg
   @%write $^@ debug codeview
   @%write $^@ option cache
   @%write $^@ option map=$(WMAPNAME).
   @%write $^@ option description 'CS40...'
   @%write $^@ name $(NAME).sys
   @for %f in ($(OBJS)) do @%append $^@ file %f
   @%write $^@ import DOSIODELAYCNT DOSCALLS.427
   @%write $^@ library d:\dev\ddk\base\lib\os2286.lib
   @%write $^@ library d:\dev\ddk\base\lib\rmcalls.lib

#$(NAME).sys: $(OBJS) makefile $(NAME).lrf d:\dev\ddk\base\lib\os2286.lib d:\dev\ddk\base\lib\rmcalls.lib
$(NAME).sys: $(OBJS) makefile $(NAME).lrf 
   $(LINK) @$(NAME).lrf

$(NAME).sym: $(WMAPNAME)
   wat2map $(WMAPNAME) $(NAME).MAP
   mapsym $(NAME).MAP

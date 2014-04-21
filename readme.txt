16-May-1999
This implementaton is Copyright (C)1999 Acme


This is a sample OS/2 audio PDD.  It works fine for straight OS/2 support
(made esp. for DART full-duplex).  Chipsets supported are CS4231, 4232,
4236, 4236B, 4237B, 4238B, Yamaha OPL3-SA3 (YMF715), and other compatibles.
It works in mode 2.  The chipset dependency is restricted to just a
module or two, and can be extended if you have any idea on what's going on.

This source is to be used only as a guide for making OS/2 audio drivers.

There is no DOS VDM handler.

There is no WinOS2 driver.

I've been using this driver since February 1999, everyday, probably with
over 500 hours of use.  It took the whole month to code and test, with two
of those weeks spent on one pesky problem.  No problems at all.  It does
full duplex perfectly on my hardware.

There is no mmpm2.ini DDINSTALL setup program, but the following can be
used as a guide.  No resource DLL is needed.

[IBMWaveBusAudio01]
 VERSIONNUMBER=1.00.
 PRODUCTINFO=whatever
 MCDDRIVER=AUDIOMCD
 VSDDRIVER=AUDIOIF
 PDDNAME=CSAUD1$
 MCDTABLE=MDM
 RESOURCENAME=BusAudioW01
 DEVICEFLAG=1
 DEVICETYPE=7
 SHARETYPE=3
 RESOURCEUNITS=2
 RESOURCECLASSES=2,1,2
 VALIDCOMBINATIONS=1,2,2,1
 CONNECTORS=1,3,IBMAmpMixBusAudio01,1
 PARMSTRING=FORMAT=1,SAMPRATE=44100,BPS=16,CHANNELS=2,DIRECTION=PLAY
 EXTNAMES=2,WAV,VOC
 EATYPES=Digital Audio
 ALIASNAME=Digital Audio
[IBMWaveBusAudio01-PLAY]
 NUMDEVICES=1
 NUMCONNECTIONS=1
 DEVICE01=AMPMIX
 CONNECTION01=0,3,1,1,3,1
[IBMWaveBusAudio01-RECORD]
 NUMDEVICES=1
 NUMCONNECTIONS=1
 DEVICE01=AMPMIX
 CONNECTION01=1,3,1,0,3,1
[IBMAmpMixBusAudio01]
 VERSIONNUMBER=1.00.
 PRODUCTINFO=whatever
 MCDDRIVER=AMPMXMCD
 VSDDRIVER=AUDIOIF
 PDDNAME=CSAUD1$
 MCDTABLE=MDM
 RESOURCENAME=BusAudioA01
 DEVICEFLAG=2
 DEVICETYPE=9
 SHARETYPE=3
 RESOURCEUNITS=2
 RESOURCECLASSES=2,1,2
 VALIDCOMBINATIONS=1,2,2,1
 CONNECTORS=0

Some of the comments are based on early knowledge.  I did not write
the ddprint stuff, just fixed it up a bit.  MM was done by Tabi (maybe).
Some of the comments are from others (unknown others).


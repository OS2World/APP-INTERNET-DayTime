# Makefile for daytime server and client
# Copyright (C) 1994 Kai Uwe Rommel <rommel@jonas>
#
# $Id: Makefile,v 1.6 1999/06/13 12:00:58 rommel Exp rommel $
#
# $Log: Makefile,v $
# Revision 1.6  1999/06/13 12:00:58  rommel
# fixed SNTP bitfield alignment error
#
# Revision 1.5  1998/10/18 19:37:25  rommel
# only IBM compiler is supported from now on
#
# Revision 1.4  1998/08/11 12:47:31  rommel
# fixed getdate dependency
#
# Revision 1.3  1998/07/30 06:46:09  rommel
# added Win32 port
# added SNTP support
# fixed many bugs
# prepared for modem support
#
# Revision 1.2  1995/08/20 08:17:12  rommel
# minor fixes
#
# Revision 1.1  1995/08/20 08:16:18  rommel
# Initial revision

win32:
	$(MAKE) all CC="icc -q" O=".obj" OUT="-Fe" \
	CFLAGS="-Sp1 -O -DWIN32" LDFLAGS="-B/ST:0x10000" \
	LIBS="wsock32.lib advapi32.lib"
win32-debug:
	$(MAKE) all CC="icc -q -Ti" O=".obj" OUT="-Fe" \
	CFLAGS="-Sp1 -DWIN32" LDFLAGS="-B/ST:0x10000" \
	LIBS="wsock32.lib advapi32.lib"
os2:
	$(MAKE) all CC="icc -q" O=".obj" OUT="-Fe" \
	CFLAGS="-Sp1 -Ss -O -DOS2" LDFLAGS="-B/ST:0x10000"
#	LIBS="tcp32dll.lib so32dll.lib"
os2-debug:
	$(MAKE) all CC="icc -q -Ti" O=".obj" OUT="-Fe" \
	CFLAGS="-Sp1 -Ss -DOS2" LDFLAGS="-B/ST:0x10000"
#	LIBS="tcp32dll.lib so32dll.lib"

YACC = bison

.SUFFIXES: .y .c $O

.c$O:
	$(CC) $(CFLAGS) -I. -c $<

.y.c:
	$(YACC) -o $*.c $<

all: daytimed.exe daytime.exe

daytimed.exe: daytimed$O getopt$O os$O
	$(CC) $(LDFLAGS) $(OUT) $@ daytimed$O getopt$O os$O $(LIBS)

daytime.exe: daytime$O getdate$O os$O getopt$O
	$(CC) $(LDFLAGS) $(OUT) $@ daytime$O getdate$O os$O getopt$O $(LIBS)

daytimed$O daytime$O getdate$O os$O getopt$O: daytime.h os.h

getdate.c: getdate.y

getdate$O: getdate.c
	$(CC) $(CFLAGS) -I. -c -O- getdate.c

# end of Makefile

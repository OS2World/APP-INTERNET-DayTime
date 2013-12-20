CC=gcc
YACC = bison
O=.o
CFLAGS=-O1 -DOS2
LDFLAGS=-B/ST:0x10000
LIBS=U:\usr\lib\libsocket.a

.SUFFIXES: .y .c $O

.c$O:
	$(CC) $(CFLAGS) -I. -c $<

.y.c:
	$(YACC) --verbose --debug -o $*.c $<

all: daytimed.exe daytime.exe

daytimed.exe: daytimed$O os$O
	$(CC) $(LDFLAGS) $(OUT) daytimed$O os$O $(LIBS)

daytime.exe: daytime$O getdate$O os$O
	$(CC) $(LDFLAGS) $(OUT) daytime$O getdate$O os$O $(LIBS)

daytimed$O daytime$O getdate$O os$O: daytime.h os.h

getdate.c: getdate.y

getdate$O: getdate.c


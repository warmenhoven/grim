CC = gcc
LDLIBS = -lcurses -lnbio
CFLAGS += -g3 -O3 -I/usr/include/libnbio -Wall

OBJS = config.o display.o list.o main.o

ifneq "$(PERF)" ""
CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifneq "$(JABBER)" ""
LDLIBS += -lexpat
CFLAGS += -DJABBER
OBJS += jabber.o xml.o
else
LDLIBS += -lfaim
CFLAGS += -I/usr/include/libfaim
OBJS += faim.o
endif

ifneq "$(SOUND)" ""
LDLIBS += -lesd
CFLAGS += -DSOUND
OBJS += sound.o
endif

TARGET = grim

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(OBJS): main.h list.h xml.h

sound.o: sound.h

sound.h: au2h
	./au2h Receive.au sound.h

clean:
	@rm -rf $(TARGET) au2h *.o core sound.h $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile *.c main.h list.h xml.h Receive.au grim.1 tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

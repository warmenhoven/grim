CC = gcc
LDLIBS = -lcurses -lnbio
CFLAGS += -g -I/usr/include/libnbio -Wall

OBJS = config.o display.o list.o main.o

ifneq "$(PERF)" ""
CFLAGS += -fprofile-arcs -ftest-coverage -O0
else
CFLAGS += -O3
endif

ifneq "$(AIM)" ""
LDLIBS += -lfaim
CFLAGS += -I/usr/include/libfaim
OBJS += faim.o
else
LDLIBS += -lexpat
CFLAGS += -DJABBER
OBJS += jabber.o xml.o
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
	mkdir -p tmp/$(TARGET)
	cp Makefile *.c main.h list.h Receive.au grim.1 tmp/$(TARGET)
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)
	rm -rf tmp

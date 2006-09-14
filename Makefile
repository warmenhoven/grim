TOPDIR = /usr
CC = gcc
LDLIBS = -lcurses $(TOPDIR)/lib/libnbio.a
CFLAGS += -g3 -O3 -I$(TOPDIR)/include/libnbio -I$(TOPDIR)/include/libfaim -Wall

OBJS = config.o display.o list.o main.o

ifneq "$(PERF)" ""
CFLAGS += -fprofile-arcs -ftest-coverage
endif

AIMLDLIBS += $(TOPDIR)/lib/libfaim.a
AIMOBJS += faim.o

JABBERLDLIBS += -lexpat
JABBEROBJS += jabber.o sha1.o xml.o

TARGET = grim

all:: $(TARGET).aim $(TARGET).jabber

$(TARGET).aim: $(OBJS) $(AIMOBJS)
	$(CC) $^ $(LDLIBS) $(AIMLDLIBS) -o $@

$(TARGET).jabber: $(OBJS) $(JABBEROBJS)
	$(CC) $^ $(LDLIBS) $(JABBERLDLIBS) -o $@

$(OBJS): main.h list.h
$(JABBEROBJS): xml.h

clean:
	@rm -rf $(TARGET).aim $(TARGET).jabber *.o core $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile *.c list.h main.h sha1.h xml.h grim.1 tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

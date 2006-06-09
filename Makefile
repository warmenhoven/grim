TOPDIR = /usr
CC = gcc
LDLIBS = -lcurses $(TOPDIR)/lib/libnbio.a
CFLAGS += -g3 -O3 -I$(TOPDIR)/include/libnbio -Wall

OBJS = config.o display.o list.o main.o

ifneq "$(PERF)" ""
CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifeq "$(JABBER)" ""
LDLIBS += $(TOPDIR)/lib/libfaim.a
CFLAGS += -I$(TOPDIR)/include/libfaim
OBJS += faim.o
else
LDLIBS += -lexpat
OBJS += jabber.o sha1.o xml.o
endif

TARGET = grim

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(OBJS): main.h list.h xml.h

clean:
	@rm -rf $(TARGET) *.o core $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile *.c list.h main.h sha1.h xml.h grim.1 tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

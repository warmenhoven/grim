LDLIBS = -lcurses -lfaim -lnbio -lesd
CFLAGS += -g -I/usr/local/include/libfaim -I/usr/local/include/libnbio -Wall

OBJS = config.o display.o faim.o list.o main.o sound.o

TARGET = grim

$(TARGET): $(OBJS) sound.o
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(OBJS): main.h list.h sound.h

sound.h: au2h
	./au2h Receive.au sound.h

clean:
	@rm -rf $(TARGET) au2h *.o core sound.h

LDLIBS = -lcurses -lfaim -lnbio # -lesd
CFLAGS += -g -I/usr/include/libfaim -I/usr/include/libnbio -Wall

OBJS = config.o display.o faim.o list.o main.o

# if you want sound uncomment these three lines
#LDLIBS += -lesd
#CFLAGS += -DSOUND
#OBJS += sound.o

TARGET = grim

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(OBJS): main.h list.h

sound.o: sound.h

sound.h: au2h
	./au2h Receive.au sound.h

clean:
	@rm -rf $(TARGET) au2h *.o core sound.h $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)
	cp Makefile *.c main.h list.h Receive.au tmp/$(TARGET)
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)
	rm -rf tmp

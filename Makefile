LDLIBS = -lcurses -lfaim -lnbio
CFLAGS += -g -I/usr/local/include/libfaim -I/usr/local/include/libnbio -Wall

OBJS = config.o display.o faim.o main.o

TARGET = grim

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(OBJS): main.h

clean:
	@rm -rf $(TARGET) *.o core

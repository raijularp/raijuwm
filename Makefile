
CC = gcc
CFLAGS = -std=c11 -O2 -Wall
LIBS = -lX11 -lXinerama
TARGET = raijuwm
SRC = raijuwm.c
BAR = bar
BAR_SRC = bar.c

all: $(TARGET) $(BAR)

$(TARGET): config.h $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

$(BAR): config.h $(BAR_SRC)
	$(CC) $(CFLAGS) -o $(BAR) $(BAR_SRC) -lX11 -lXinerama

config.h: config.def.h
	@cp config.def.h config.h

clean:
	rm -f $(TARGET) $(BAR) config.h

install: all
	install -d /usr/local/bin
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)
	install -m 0755 $(BAR) /usr/local/bin/$(BAR)

uninstall:
	rm -f /usr/local/bin/$(TARGET) /usr/local/bin/$(BAR)

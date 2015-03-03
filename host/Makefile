
CC=gcc
TARGETCC=mipsel-openwrt-linux-uclibc-gcc

CFLAGS += -std=c11 -Wall -Ipaho/src -Lpaho/build/output -lpaho-mqtt3c -lpthread

OBJECTS := $(patsubst %.c,%.o,$(wildcard *.c))

all: debug

debug: CFLAGS += -g
debug: stepd.host

release: CC=$(TARGETCC)
release: LD=$(TARGETLD)
release: stepd.mipsel

stepd.host stepd.mipsel: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: debug release


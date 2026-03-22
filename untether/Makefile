CC = $(shell xcrun --sdk iphoneos --find clang)
STRIP = $(shell xcrun --sdk iphoneos --find strip)
LDID = $(shell which ldid)
TAR = $(shell which tar)

SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
SRC = $(wildcard ./src/*.c) $(wildcard ../common/src/*.c) $(wildcard ../common/src/*.s)

CFLAGS = -isysroot $(SDK) -arch armv7 -I./include -I../common/include
LDFLAGS = -L$(SDK)/usr/lib -L../resources/lib -lc -lSystem -framework CoreFoundation -framework IOKit

untether_5_7: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -miphoneos-version-min=5.0 -o aquila_5_7 $^
	$(STRIP) aquila_5_7
	$(LDID) -S../resources/ents.xml aquila_5_7

untether_4: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -miphoneos-version-min=4.0 -o aquila_4 $^
	$(STRIP) aquila_4
	$(LDID) -S../resources/ents.xml aquila_4

all: untether_5_7 untether_4

clean:
	@rm -rf staging
	@rm -rf aquila_4
	@rm -rf aquila_5_7
	@rm -rf *.o

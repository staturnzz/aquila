CC = $(shell xcrun --sdk iphoneos --find clang)
STRIP = $(shell xcrun --sdk iphoneos --find strip)
LDID = $(shell which ldid)
TAR = $(shell which tar)

SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
SRC = $(wildcard ./src/*.c) $(wildcard ../common/src/*.c) $(wildcard ../common/src/*.s)

CFLAGS = -isysroot $(SDK) -miphoneos-version-min=5.0 -arch armv7 -I./include -I../common/include
LDFLAGS = -L$(SDK)/usr/lib -L../resources/lib -lc -lSystem -framework CoreFoundation -framework IOKit

all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o aquila $^
	$(STRIP) aquila
	$(LDID) -S../resources/ents.xml aquila

clean:
	@rm -rf aquila
	@rm -rf *.o

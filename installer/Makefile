CC = $(shell xcrun --sdk iphoneos --find clang)
STRIP = $(shell xcrun --sdk iphoneos --find strip)
LDID = $(shell which ldid)

SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
SRC = $(wildcard ./src/*.c) $(wildcard ../common/src/*.c) $(wildcard ../common/src/*.s)

CFLAGS = -isysroot $(SDK) -miphoneos-version-min=5.0 -arch armv7 -I./include -I../common/include
LDFLAGS = -L$(SDK)/usr/lib -L../resources/lib -lc -lSystem -framework CoreFoundation -framework IOKit -framework ImageIO
LDFLAGS += -framework CoreGraphics ../resources/IOMobileFramebuffer.tbd

all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o installer $^
	$(STRIP) installer
	$(LDID) -S../resources/ents.xml installer

clean:
	@rm -rf installer
	@rm -rf *.o

CC = $(shell xcrun --sdk iphoneos --find clang)
STRIP = $(shell xcrun --sdk iphoneos --find strip)
LDID = $(shell which ldid)

SDK = $(shell xcrun --sdk iphoneos --show-sdk-path)
SRC = $(wildcard ./src/*.c) $(wildcard ../common/src/*.c) $(wildcard ../common/src/*.s)

CFLAGS = -isysroot $(SDK) -arch armv7 -I./include -I../common/include
LDFLAGS = -L$(SDK)/usr/lib -L../resources/lib -lc -lSystem -framework CoreFoundation -framework IOKit -framework ImageIO
LDFLAGS += -framework CoreGraphics ../resources/IOMobileFramebuffer.tbd -llockdown

_installer_5_7: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -miphoneos-version-min=5.0 -o installer_5_7 $^
	$(STRIP) installer_5_7
	$(LDID) -S../resources/ents.xml installer_5_7

_installer_4: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -miphoneos-version-min=4.0 -o installer_4 $^
	$(STRIP) installer_4
	$(LDID) -S../resources/ents.xml installer_4

all: _installer_5_7 _installer_4

clean:
	@rm -rf installer_5_7
	@rm -rf installer_4
	@rm -rf *.o

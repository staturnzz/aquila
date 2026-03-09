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

deb: 
	@rm -rf ./staging
	@mkdir -p ./staging/DEBIAN

	@umask u=rwx,g=rx,o=rx && mkdir -p ./staging/tmp/aquila
	@cp -a ./aquila_4 ./staging/tmp/aquila/aquila_4
	@cp -a ./aquila_5_7 ./staging/tmp/aquila/aquila_5_7
	@cp -a ../resources/amfi_bypass_4_5.dylib ./staging/tmp/aquila/amfi_bypass_4_5.dylib
	@cp -a ../resources/amfi_bypass_6.dylib ./staging/tmp/aquila/amfi_bypass_6.dylib
	@cp -a ../resources/amfi_bypass_7.dylib ./staging/tmp/aquila/amfi_bypass_7.dylib

	@cp -a ./control ./staging/DEBIAN/control
	@cp -a ./preinst ./staging/DEBIAN/preinst
	@cp -a ./postinst ./staging/DEBIAN/postinst

	@chmod 0755 ./staging/DEBIAN/preinst
	@chmod 0755 ./staging/DEBIAN/postinst

	@find ./staging -name '.DS_Store' -type f -delete
	COPYFILE_DISABLE=1 dpkg-deb -Znone --root-owner-group --build ./staging ./com.staturnz.aquila.deb

all: untether_5_7 untether_4 deb

clean:
	@rm -rf com.staturnz.aquila.deb
	@rm -rf staging
	@rm -rf aquila_4
	@rm -rf aquila_5_7
	@rm -rf *.o

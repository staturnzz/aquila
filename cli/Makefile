CC = $(shell xcrun --sdk macosx --find clang)
SDK = $(shell xcrun --sdk macosx --show-sdk-path)
LDID = $(shell which ldid)
LIBS = $(SDK)/usr/lib

CFLAGS := -I./include -isysroot $(SDK) -arch x86_64 -DMACOS_BUILD=1
LDFLAGS += -F$(SDK)/System/Library/PrivateFrameworks -F/Library/Apple/System/Library/PrivateFrameworks
LDFLAGS +=  -L$(SDK)/usr/lib -lSystem -lc -framework CoreFoundation -framework MobileDevice
LDFLAGS += -Wl,-sectcreate,__DATA,__payload_dmg,../resources/payload.dmg
LDFLAGS += -Wl,-sectcreate,__DATA,__stock_dmg,../resources/stock.dmg
LDFLAGS += -Wl,-sectcreate,__DATA,__signature,../resources/stock.signature
LDFLAGS += -Wl,-sectcreate,__DATA,__bootstrap,../resources/bootstrap.tar
LDFLAGS += -Wl,-sectcreate,__DATA,__launchd_conf,../resources/launchd.conf
LDFLAGS += -Wl,-sectcreate,__DATA,__splashscreen,../resources/splashscreen.jp2
LDFLAGS += -Wl,-sectcreate,__DATA,__amfi_6,../resources/amfi_bypass_6.dylib
LDFLAGS += -Wl,-sectcreate,__DATA,__amfi_4_5,../resources/amfi_bypass_4_5.dylib
LDFLAGS += -Wl,-sectcreate,__DATA,__tar,../resources/tar
LDFLAGS += -Wl,-sectcreate,__DATA,__tar_4,../resources/tar_4
LDFLAGS += -Wl,-sectcreate,__DATA,__truststore,../resources/truststore.tar
LDFLAGS += -Wl,-sectcreate,__DATA,__safemode_5,../resources/safemode5.deb
LDFLAGS += -Wl,-sectcreate,__DATA,__substrate_5,../resources/substrate5.deb
LDFLAGS += -Wl,-sectcreate,__DATA,__aquila,../untether/aquila
LDFLAGS += -Wl,-sectcreate,__DATA,__installer,../installer/installer

SRC = $(wildcard src/*.c)

all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o aquila $^
	$(LDID) -Sents.xml aquila

clean:
	@rm -rf aquila
	@rm -rf *.o

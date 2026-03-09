
payload1:
	@rm -rf ./staging
	@rm -rf ./payload1.dmg
	@mkdir -p ./staging/Library/Lockdown/ServiceAgents

	@cp -a ../resources/plist/haxx.aquila.remount.plist ./staging/Library/Lockdown/ServiceAgents/haxx.aquila.remount.plist
	@cp -a ../resources/plist/haxx.aquila.afc2.plist ./staging/Library/Lockdown/ServiceAgents/haxx.aquila.afc2.plist
	@cp -a ../resources/plist/haxx.aquila.housekeeping.plist ./staging/Library/Lockdown/ServiceAgents/haxx.aquila.housekeeping.plist

	@chmod -R 0777 staging
	@find ./staging -name ".DS_Store" -delete
	@hdiutil create -srcfolder staging -volname "root" -fs HFS+ -format UDRW -imagekey bps=512 -imagekey appendable=true -imagekey burnable=true -ov -layout NONE ./payload1.dmg


payload1_7:
	@rm -rf ./staging
	@rm -rf ./payload1_7.dmg
	@rm -rf ./temp.dmg
	@diskutil unmount /Volumes/temp | true
	@diskutil unmount /Volumes/DeveloperDiskImage | true
	@diskutil unmount /Volumes/DeveloperLib | true
	@diskutil unmount /Volumes/DeveloperCaches | true

	@mkdir -p ./staging/DeveloperDiskImage
	@mkdir ./staging/DeveloperLib
	@mkdir ./staging/DeveloperCaches

	hdiutil create -size 20m -layout GPTSPUD -fs HFS+ -volname temp temp.dmg
	hdiutil attach temp.dmg

	diskutil partitionDisk /dev/`diskutil info -plist /Volumes/temp | plutil -extract ParentWholeDisk raw -` GPT \
		HFS+ DeveloperDiskImage 4m \
		HFS+ DeveloperLib 8m \
		HFS+ DeveloperCaches R

	@mkdir ./staging/DeveloperCaches/com.apple.dyld
	@touch ./staging/DeveloperCaches/com.apple.dyld/enable-dylibs-to-override-cache
	@cp -a ../resources/amfi_bypass_7.dylib ./staging/DeveloperLib/libmis.dylib

	@chmod -R 0777 staging
	@find ./staging -name ".DS_Store" -delete
	@rsync -a ./staging/DeveloperLib/* /Volumes/DeveloperLib
	@rsync -a ./staging/DeveloperCaches/* /Volumes/DeveloperCaches

	hdiutil detach /dev/`diskutil info -plist /Volumes/DeveloperDiskImage | plutil -extract ParentWholeDisk raw -`
	hdiutil convert temp.dmg -format UDZO -imagekey zlib-level=9 -o payload1_7.dmg

payload2_7:
	@rm -rf ./staging
	@rm -rf ./payload2_7.dmg
	@mkdir -p ./staging/Library/Lockdown/ServiceAgents
	@cp -a ../resources/plist/com.apple* ./staging/Library/Lockdown/ServiceAgents
	@cp -a ../installer/installer_5_7 ./staging/installer
	@cp -a ../resources/amfi_bypass_7.dylib ./staging/amfi_bypass_7.dylib
	@cp -a ../resources/tar ./staging/tar

	@chmod -R 0777 staging
	@find ./staging -name ".DS_Store" -delete
	@hdiutil create -srcfolder staging -volname "root" -fs HFS+ -format UDRW -imagekey bps=512 -imagekey appendable=true -imagekey burnable=true -ov -layout NONE ./payload2_7.dmg

all: payload1 payload1_7 payload2_7

clean:
	@rm -rf *.dmg


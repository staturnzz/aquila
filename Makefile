clean:
	$(MAKE) -C untether clean
	$(MAKE) -C installer clean
	$(MAKE) -C cli clean
	@rm -rf ./resources/payload.dmg

all: clean
	$(MAKE) -C untether
	$(MAKE) -C installer

	cd ./resources && ./create_payload.sh
	$(MAKE) -C cli

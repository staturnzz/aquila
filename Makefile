clean:
	$(MAKE) -C untether clean
	$(MAKE) -C installer clean
	$(MAKE) -C payload clean
	$(MAKE) -C cli clean
	@rm -rf ./resources/payload.dmg

all: clean
	$(MAKE) -C untether all
	$(MAKE) -C installer all
	$(MAKE) -C payload all
	$(MAKE) -C cli

fast:
	$(MAKE) -C cli

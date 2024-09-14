.PHONY: all release debug install clean

all: release

release: clean
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
	$(MAKE) -C build

debug: clean
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local
	$(MAKE) -C build

install:
	$(MAKE) -C build install

clean:
	rm -rf build
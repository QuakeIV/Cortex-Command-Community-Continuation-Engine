
# shitty make wrapper so that i can just type 'make' to build

build: FORCE
	meson build
	ninja -C build

FORCE:

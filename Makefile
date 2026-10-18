# Native 3DS build: devkitARM + libctru + 3ds-tools, no extra libraries.
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB ?= $(DEVKITPRO)/libctru
TOOLS := $(DEVKITPRO)/tools/bin
CC3DS := $(DEVKITARM)/bin/arm-none-eabi-gcc
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS3DS := $(ARCH) -std=gnu11 -O2 -g -Wall -Wextra -Werror -D__3DS__ -mword-relocations -ffunction-sections -fdata-sections -Iinclude -I$(CTRULIB)/include
SOURCES := $(wildcard source/*.c)
OBJECTS := $(patsubst source/%.c,build/%.o,$(SOURCES))
.PHONY: all clean test package
all: dist/3dsDS.3dsx dist/3dsDS.smdh
build dist:
	mkdir -p $@
build/%.o: source/%.c | build
	$(CC3DS) $(CFLAGS3DS) -MMD -MP -c $< -o $@
dist/3dsDS.elf: $(OBJECTS) | dist
	$(CC3DS) $(ARCH) -specs=3dsx.specs -L$(CTRULIB)/lib $(OBJECTS) -Wl,--gc-sections,-Map,build/3dsDS.map -lctru -lm -o $@
dist/3dsDS.smdh: | dist
	$(TOOLS)/smdhtool --create "3dsDS" "roboRIO driver station" "3dsDS contributors" $(CTRULIB)/default_icon.png $@
dist/3dsDS.3dsx: dist/3dsDS.elf dist/3dsDS.smdh
	$(TOOLS)/3dsxtool $< $@ --smdh=dist/3dsDS.smdh
test: | build
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -Iinclude source/protocol.c source/safety.c source/link.c tests/test_core.c -o build/test_core
	./build/test_core
package: all
	python3 tools/package.py
clean:
	rm -rf build dist
-include $(OBJECTS:.o=.d)

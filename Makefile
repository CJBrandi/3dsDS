# Native 3DS build: devkitARM, libctru, Citro2D/Citro3D and 3ds-tools.
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB ?= $(DEVKITPRO)/libctru
PORTLIBS ?= $(DEVKITPRO)/portlibs/3ds
TOOLS := $(DEVKITPRO)/tools/bin
CIA_TOOLS ?= build/cia-tools
VERSION_MAJOR ?= 0
VERSION_MINOR ?= 3
VERSION_MICRO ?= 1
CC3DS := $(DEVKITARM)/bin/arm-none-eabi-gcc
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS3DS := $(ARCH) -std=gnu11 -O2 -g -Wall -Wextra -Werror -D__3DS__ -mword-relocations -ffunction-sections -fdata-sections -Iinclude -I$(CTRULIB)/include -I$(PORTLIBS)/include
SOURCES := $(wildcard source/*.c)
OBJECTS := $(patsubst source/%.c,build/%.o,$(SOURCES))
.PHONY: all clean test package preview cia cia-tools release
all: dist/3dsDS.elf dist/3dsDS.smdh
build dist:
	mkdir -p $@
build/%.o: source/%.c | build
	$(CC3DS) $(CFLAGS3DS) -MMD -MP -c $< -o $@
dist/3dsDS.elf: $(OBJECTS) | dist
	$(CC3DS) $(ARCH) -specs=3dsx.specs -L$(CTRULIB)/lib -L$(PORTLIBS)/lib $(OBJECTS) -Wl,--gc-sections,-Map,build/3dsDS.map -lcitro2d -lcitro3d -lctru -lmbedcrypto -lm -o $@
dist/3dsDS.smdh: assets/icon.png | dist
	$(TOOLS)/smdhtool --create "3dsDS" "roboRIO driver station" "3dsDS contributors" assets/icon.png $@
dist/3dsDS.3dsx: dist/3dsDS.elf dist/3dsDS.smdh
	$(TOOLS)/3dsxtool $< $@ --smdh=dist/3dsDS.smdh
test: | build
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -Itests/fakes -Iinclude source/wifi_scan.c source/wifi_scan_parse.c tests/test_wifi_scan_service.c -o build/test_wifi_scan_service
	./build/test_wifi_scan_service
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -Iinclude source/wifi_scan_parse.c tests/test_wifi_scan.c -o build/test_wifi_scan
	./build/test_wifi_scan
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -Iinclude source/protocol.c source/safety.c source/link.c tests/test_core.c -o build/test_core
	./build/test_core
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -Iinclude source/ui.c source/wifi_scan_parse.c tests/test_ui.c -o build/test_ui
	./build/test_ui
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -Itests/fakes -Iinclude source/wifi.c source/wifi_settings.c tests/test_wifi.c -lmbedcrypto -o build/test_wifi
	./build/test_wifi
package: release
clean:
	rm -rf build dist
-include $(OBJECTS:.o=.d)

preview: | build
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude source/ui.c source/wifi_scan_parse.c tools/preview_ui.c -o build/preview_ui
	./build/preview_ui docs

cia-tools:
	python3 tools/fetch_cia_tools.py $(CIA_TOOLS)
build/banner.bnr: assets/banner.png assets/silence.wav | build
	$(CIA_TOOLS)/bannertool makebanner -i assets/banner.png -a assets/silence.wav -o $@
dist/3dsDS.cia: dist/3dsDS.elf dist/3dsDS.smdh build/banner.bnr assets/app.rsf
	$(CIA_TOOLS)/makerom -f cia -o $@ -target t -rsf assets/app.rsf -elf dist/3dsDS.elf -icon dist/3dsDS.smdh -banner build/banner.bnr -major $(VERSION_MAJOR) -minor $(VERSION_MINOR) -micro $(VERSION_MICRO)
cia: dist/3dsDS.cia
release: all cia
	python3 tools/check_artifacts.py

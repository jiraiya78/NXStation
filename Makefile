# NXStation — devkitPro Docker build
# Usage (PowerShell):
#   function dmake { docker run --rm -v ${PWD}:/src --workdir /src devkitpro/devkita64 make $args }
#   dmake
#   dmake clean
#   dmake -j8

.PHONY: all configure clean NXStation.nro

BUILD_DIR  ?= build_switch
CMAKE_FLAGS ?= -DPLATFORM_SWITCH=ON -DSF_ENABLE_FFMPEG=ON -DCMAKE_BUILD_TYPE=Release
JOBS       ?= $(shell nproc 2>/dev/null || echo 4)
SHELL := /bin/bash
SWITCH_ENV = source /opt/devkitpro/switchvars.sh

all: NXStation.nro

configure:
	@$(SWITCH_ENV) && cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

$(BUILD_DIR)/Makefile:
	@$(MAKE) configure

NXStation.nro: $(BUILD_DIR)/Makefile
	@$(SWITCH_ENV) && cmake --build $(BUILD_DIR) --target NXStation.nro -j$(JOBS)

clean:
	rm -rf $(BUILD_DIR)

# Build both independently loadable modules through the shared CMake setup.
BUILD_DIR ?= build
DMOD_CPU_FAMILY ?= stm32f7
CMAKE_ARGS ?=

.PHONY: all configure clean

all: configure
	cmake --build $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DDMOD_CPU_FAMILY=$(DMOD_CPU_FAMILY) $(CMAKE_ARGS)

clean:
	cmake --build $(BUILD_DIR) --target clean

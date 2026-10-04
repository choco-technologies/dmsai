# CMake builds both independently loadable modules and resolves SDK packages.
BUILD_DIR ?= build
DMOD_CPU_FAMILY ?= stm32f7
CMAKE_ARGS ?=
.PHONY: all configure test clean
all: configure
	cmake --build $(BUILD_DIR)
configure:
	cmake -S . -B $(BUILD_DIR) -DDMOD_CPU_FAMILY=$(DMOD_CPU_FAMILY) $(CMAKE_ARGS)
test:
	cmake -S tests/native -B $(BUILD_DIR)-native
	cmake --build $(BUILD_DIR)-native
	ctest --test-dir $(BUILD_DIR)-native --output-on-failure
clean:
	cmake --build $(BUILD_DIR) --target clean

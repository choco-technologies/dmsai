# dmsai

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmsai/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmsai/actions/workflows/ci.yml)

dmsai DMOD library module.

## Description

TODO: describe what this module does.

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

Tests are built automatically alongside the module (see `tests/`). Once built,
run them with `ctest`:

```bash
cd build
ctest --output-on-failure
```

`ctest` installs the test module's dependencies with `dmf-get` and then runs
it through `dmod_loader`. To run it manually instead:

```bash
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d ${DMOD_DMF_DIR}/test_dmsai-local.dmd -y
dmod_loader build/dmf/test_dmsai.dmf
```

## Usage

<TBD>

This library module provides functions that can be used by other modules:

```c
#include "dmsai.h"
```

## API

| Function | Description |
|----------|-------------|
| `dmsai_create()` | Create a new `dmsai_t` instance. |
| `dmsai_destroy()` | Destroy an instance created by `_create()`. |
| `dmsai_is_valid()` | Check whether a handle is a valid instance. |

See [include/dmsai.h](include/dmsai.h) for the full
declarations and [docs/api-reference.md](docs/api-reference.md) for the
complete reference.

## Documentation

See the `docs/` directory:

- **[api-reference.md](docs/api-reference.md)** - Complete API documentation

View documentation using `dmf-man dmsai`.

## Hardware Port

This module ships two DMOD modules: the architecture-independent
`dmsai` and `dmsai_port`, which contains the
architecture-specific implementation. The active architecture is selected via
`DMOD_CPU_FAMILY` (default: `stm32f7`):

```bash
cmake .. -DDMOD_CPU_FAMILY=stm32f7
```

See [docs/port-implementation.md](docs/port-implementation.md) for how to add
another architecture. Port-specific files:

```
├── include/dmsai_port.h
├── src/port/
│   ├── CMakeLists.txt
│   └── stm32f7/
│       ├── config.cmake
│       └── port.c
└── dmsai_port.dmr
```
## Project Structure

```
dmsai/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dmsai.h
├── src/
│   └── dmsai.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmsai_test.c
├── CMakeLists.txt
├── Makefile
├── dmsai.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT

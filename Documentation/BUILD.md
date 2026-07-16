# Building PixelDisplay Pro

## Prerequisites
- CMake ≥ 3.24
- A C++20 compiler: MSVC 2022, Clang ≥ 14, or GCC ≥ 12
- Ninja (recommended) or your platform's native generator
- (Optional) Adobe After Effects SDK — only needed to build the plugin itself.
  The engine and tests build without it.

## Quick start (engine + tests)

```sh
cd PixelDisplay
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure    # or: ./build/Tests/pdtests
```

This builds:
- `pdengine` — the standalone rendering engine (static library)
- `pdtests` — headless engine tests (CPU backend)

## Building the After Effects plugin

Point CMake at your local AE SDK. The plugin target only configures when the
path exists, so CI stays green without the SDK.

```sh
cmake -S PixelDisplay -B build -G Ninja \
      -DPD_AE_SDK_ROOT=/path/to/AfterEffectsSDK
cmake --build build
```

## Platform notes

- **macOS** builds a universal binary (`x86_64;arm64`) by default. The Metal GPU
  backend is enabled by `-DPD_WITH_METAL=ON` (default ON on Apple).
- **Windows** enables the Direct3D 12 backend by default (`-DPD_WITH_D3D12=ON`).
- **OpenGL** backend is opt-in (`-DPD_WITH_OPENGL=ON`) for portability/parity.
- The **CPU backend is always built** and is the golden reference for tests.

## Useful options

| Option | Default | Meaning |
|--------|---------|---------|
| `PD_BUILD_TESTS` | ON | Build `pdtests` |
| `PD_WITH_METAL`  | ON (Apple) | Metal GPU backend |
| `PD_WITH_D3D12`  | ON (Windows) | Direct3D 12 GPU backend |
| `PD_WITH_OPENGL` | OFF | OpenGL GPU backend |
| `PD_AE_SDK_ROOT` | (unset) | Path to AE SDK; enables the plugin target |

## Sanitizers (development)

```sh
cmake -S PixelDisplay -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"
cmake --build build-asan && ./build-asan/Tests/pdtests

# Thread races (validates the MFR / thread-pool contract):
cmake -S PixelDisplay -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
cmake --build build-tsan && ./build-tsan/Tests/pdtests
```

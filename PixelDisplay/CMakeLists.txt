# PixelDisplay Pro — top-level build
#
# Targets:
#   pdengine          static library — the standalone rendering engine (no Adobe)
#   pdtests           engine unit / golden-image tests (headless, CPU backend)
#   PixelDisplayPro   the After Effects plugin — built ONLY when the AE SDK is
#                     found, so CI and engine development work without it.
#
# Platforms: Windows (x64), macOS Intel, macOS Apple Silicon (universal2).
cmake_minimum_required(VERSION 3.24)

project(PixelDisplayPro
        VERSION 0.1.0
        DESCRIPTION "Procedural display-hardware simulation for After Effects"
        LANGUAGES CXX)

# ---- Global settings -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# macOS: build universal binaries by default (Intel + Apple Silicon).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
endif()

option(PD_BUILD_TESTS "Build engine unit tests" ON)
option(PD_WITH_METAL  "Build the Metal GPU backend (macOS)" ${APPLE})
option(PD_WITH_D3D12  "Build the Direct3D 12 GPU backend (Windows)" ${WIN32})
option(PD_WITH_OPENGL "Build the OpenGL GPU backend" OFF)

add_subdirectory(Engine)

if(PD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# The AE plugin target is gated on the SDK being present (design decision:
# engine + tests always build; the host adapter compiles only with the SDK).
set(PD_AE_SDK_ROOT "" CACHE PATH "Path to the Adobe After Effects SDK root")
if(PD_AE_SDK_ROOT AND EXISTS "${PD_AE_SDK_ROOT}")
    message(STATUS "After Effects SDK found — building PixelDisplayPro plugin")
    add_subdirectory(Host/AfterEffects)
else()
    message(STATUS "After Effects SDK not set (PD_AE_SDK_ROOT) — skipping plugin target; "
                   "engine and tests will still build")
endif()

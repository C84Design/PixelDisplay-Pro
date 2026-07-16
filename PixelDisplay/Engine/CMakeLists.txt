# PixelDisplay Pro — Engine (Layer 2)
#
# `pdengine` is a standalone static library with ZERO dependency on the Adobe
# SDK. Its only include root is the PixelDisplay/ directory, so all engine
# includes are of the form  #include "Engine/..."  — this makes the boundary
# obvious and lets host adapters consume the engine headers unchanged.

add_library(pdengine STATIC
    Core/Engine.cpp
    Core/FrameGraph.cpp
    Renderer/CPU/CpuBackend.cpp
)

target_include_directories(pdengine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # so "Engine/..." resolves
)

target_compile_features(pdengine PUBLIC cxx_std_20)

# Threading for the CPU backend's thread pool.
find_package(Threads REQUIRED)
target_link_libraries(pdengine PUBLIC Threads::Threads)

# Warnings-as-quality (not fatal, to keep third-party host builds resilient).
if(MSVC)
    target_compile_options(pdengine PRIVATE /W4 /permissive-)
else()
    target_compile_options(pdengine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- GPU backends (added in later milestones; scaffolding gated here) ------
if(PD_WITH_METAL AND APPLE)
    target_compile_definitions(pdengine PUBLIC PD_WITH_METAL=1)
    # Metal sources (.mm) and shader compilation are wired up in Milestone 2/9.
    # target_sources(pdengine PRIVATE Renderer/GPU/Metal/MetalBackend.mm)
    # target_link_libraries(pdengine PUBLIC "-framework Metal" "-framework Foundation")
endif()
if(PD_WITH_D3D12 AND WIN32)
    target_compile_definitions(pdengine PUBLIC PD_WITH_D3D12=1)
    # target_sources(pdengine PRIVATE Renderer/GPU/D3D12/D3D12Backend.cpp)
    # target_link_libraries(pdengine PUBLIC d3d12 dxgi dxguid)
endif()
if(PD_WITH_OPENGL)
    target_compile_definitions(pdengine PUBLIC PD_WITH_OPENGL=1)
    # target_sources(pdengine PRIVATE Renderer/GPU/OpenGL/OpenGLBackend.cpp)
endif()

add_library(PixelDisplay::Engine ALIAS pdengine)

// PixelDisplay Pro — Engine/Renderer/GPU/OpenGL/OpenGLBackend.cpp
//
// OpenGL backend skeleton. Mirrors MetalBackend.mm using glDispatchCompute,
// SSBOs and image load/store. Returns nullptr until the GLSL kernels are ported
// and pass the golden-image parity suite. See GPU_PIPELINE.md.
#include "Engine/Renderer/GPU/OpenGL/OpenGLBackend.hpp"

namespace pd::gl {

std::unique_ptr<Backend> createOpenGLBackend() {
    // TODO(M9-gl): create a GL context/compute program and return a backend once
    // the GLSL kernels are validated against the CPU reference.
    return nullptr;
}

}  // namespace pd::gl


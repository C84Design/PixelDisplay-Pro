// PixelDisplay Pro — Engine/Renderer/GPU/OpenGL/OpenGLBackend.hpp
//
// Factory for the OpenGL 4.3+ / GLES 3.1 backend (portability & parity path).
// STATUS: skeleton — the GLSL compute mirror of the CPU stages, SSBOs for
// geometry/LUTs and image-load/store textures are completed and parity-checked
// before this returns a usable backend. See GPU_PIPELINE.md.
#pragma once

#include <memory>

#include "Engine/Renderer/Backend.hpp"

namespace pd::gl {

/// Create an OpenGL backend, or nullptr if unavailable / not yet completed.
std::unique_ptr<Backend> createOpenGLBackend();

}  // namespace pd::gl

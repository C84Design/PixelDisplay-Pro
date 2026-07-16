// PixelDisplay Pro — Engine/Renderer/GPU/Metal/MetalBackend.hpp
//
// Factory for the Metal backend (macOS, primary GPU path). Declared in plain
// C++ so the engine facade can call it without importing any Objective-C or
// Metal headers; the implementation lives in MetalBackend.mm.
//
// STATUS: built and validated on macOS only (see GPU_PIPELINE.md). Returns
// nullptr when Metal is unavailable so BackendPreference::Auto falls back.
#pragma once

#include <memory>

#include "Engine/Renderer/Backend.hpp"

namespace pd::metal {

/// Create a Metal backend, or nullptr if no usable Metal device is present.
std::unique_ptr<Backend> createMetalBackend();

}  // namespace pd::metal

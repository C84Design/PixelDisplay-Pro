// PixelDisplay Pro — Engine/Renderer/GPU/D3D12/D3D12Backend.hpp
//
// Factory for the Direct3D 12 backend (Windows, secondary GPU path). Same plain
// C++ surface as the Metal factory. STATUS: skeleton — built on Windows; the
// compute PSOs (HLSL mirror of the CPU stages), root signature, descriptor
// heaps and command allocators are completed and validated against the CPU
// reference before this factory returns a usable backend. See GPU_PIPELINE.md.
#pragma once

#include <memory>

#include "Engine/Renderer/Backend.hpp"

namespace pd::d3d12 {

/// Create a D3D12 backend, or nullptr if unavailable / not yet completed.
std::unique_ptr<Backend> createD3D12Backend();

}  // namespace pd::d3d12

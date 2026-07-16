// PixelDisplay Pro — Engine/Renderer/GPU/D3D12/D3D12Backend.cpp
//
// D3D12 backend skeleton. The full implementation mirrors MetalBackend.mm:
//   - device + command queue + timeline fence
//   - root signature (CBV for GpuParams, SRV/UAV tables for textures)
//   - compute PSOs from the HLSL mirror of the CPU stages, cached to disk
//   - placed resources on a transient heap; per-thread command allocators
//   - dispatch the graph stages, then read back to the host ImageView
//
// Until the PSOs are ported and pass the golden-image parity suite on Windows,
// the factory returns nullptr so BackendPreference::Auto falls back to the CPU
// reference. This keeps the tree honest: no silently-partial GPU output.
#include "Engine/Renderer/GPU/D3D12/D3D12Backend.hpp"

namespace pd::d3d12 {

std::unique_ptr<Backend> createD3D12Backend() {
    // TODO(M9-windows): construct the device/queue/PSOs and return a backend
    // once the HLSL kernels are validated against the CPU reference.
    return nullptr;
}

}  // namespace pd::d3d12

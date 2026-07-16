// PixelDisplay Pro — Engine/Noise/Hash.hpp
//
// Deterministic, coordinate-derived hashing. ALL "randomness" in the engine
// comes from hashing (coordinate, seed) — never a stateful RNG — so results are
// identical on CPU and GPU, across runs, and safe under Multi-Frame Rendering
// (no shared generator state). See DESIGN.md §10.2.
#pragma once

#include <cstdint>

namespace pd::noise {

/// PCG-style integer hash. Good avalanche, cheap, trivially portable to shaders.
inline std::uint32_t hashU32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

inline std::uint32_t hash2(std::int32_t x, std::int32_t y, std::uint32_t seed) {
    std::uint32_t h = seed * 0x9e3779b1u;
    h = hashU32(h ^ static_cast<std::uint32_t>(x) * 0x85ebca6bu);
    h = hashU32(h ^ static_cast<std::uint32_t>(y) * 0xc2b2ae35u);
    return h;
}

/// Uniform float in [0,1) from integer coordinates + seed.
inline float hash2f(std::int32_t x, std::int32_t y, std::uint32_t seed) {
    return static_cast<float>(hash2(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}

/// Two independent uniforms in [0,1) from the same coordinate (different lanes).
inline void hash2f2(std::int32_t x, std::int32_t y, std::uint32_t seed,
                    float& a, float& b) {
    a = hash2f(x, y, seed);
    b = hash2f(x, y, seed ^ 0x68bc21ebu);
}

}  // namespace pd::noise

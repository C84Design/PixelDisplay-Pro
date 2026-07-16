// PixelDisplay Pro — Engine/Noise/Noise.hpp
//
// Value noise + fractal Brownian motion built on the deterministic hash. Used
// for panel non-uniformity (mura), fingerprints/smudges, temporal grain, and
// other organic imperfections. Deterministic and coordinate-derived, so it is
// identical across CPU/GPU and safe under Multi-Frame Rendering.
#pragma once

#include <cmath>

#include "Engine/Math/Vec.hpp"
#include "Engine/Noise/Hash.hpp"

namespace pd::noise {

/// Smooth value noise in [0,1], continuous, lattice period 1.
inline float valueNoise(float x, float y, std::uint32_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - xi;
    float yf = y - yi;
    // Quintic smoothing for C2 continuity.
    float u = xf * xf * xf * (xf * (xf * 6 - 15) + 10);
    float v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);

    float a = hash2f(xi,     yi,     seed);
    float b = hash2f(xi + 1, yi,     seed);
    float c = hash2f(xi,     yi + 1, seed);
    float d = hash2f(xi + 1, yi + 1, seed);
    float ab = a + (b - a) * u;
    float cd = c + (d - c) * u;
    return ab + (cd - ab) * v;
}

/// Fractal Brownian motion: sum of octaves. Returns roughly [0,1].
inline float fbm(float x, float y, std::uint32_t seed, int octaves = 4) {
    float sum = 0.0f, amp = 0.5f, freq = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * valueNoise(x * freq, y * freq, seed + static_cast<std::uint32_t>(o) * 131u);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return norm > 0 ? sum / norm : 0.0f;
}

/// Signed fbm centered on 0, range roughly [-1,1].
inline float fbmSigned(float x, float y, std::uint32_t seed, int octaves = 4) {
    return fbm(x, y, seed, octaves) * 2.0f - 1.0f;
}

}  // namespace pd::noise

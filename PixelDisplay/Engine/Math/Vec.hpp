// PixelDisplay Pro — Engine/Math/Vec.hpp
//
// Small, header-only vector/matrix types shared by CPU kernels and (by
// convention) mirrored in shader code. Kept intentionally minimal and
// constexpr-friendly; this is not a general linear-algebra library.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pd::math {

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator*(Vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec3 rgb() const { return {x, y, z}; }
};

// Common scalar helpers (mirrored in shader convention).
constexpr float saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
constexpr float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float fract(float v) { return v - std::floor(v); }

/// Smoothstep with analytic edge for resolution-independent anti-aliasing.
inline float smoothstepf(float edge0, float edge1, float x) {
    if (edge0 == edge1) return x < edge0 ? 0.0f : 1.0f;
    float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// 2x2 rotation applied to a Vec2 (used by grid/pixel rotation).
inline Vec2 rotate(Vec2 v, float radians) {
    float c = std::cos(radians), s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

}  // namespace pd::math

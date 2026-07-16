// PixelDisplay Pro — Engine/DisplayLayouts/Grid.hpp
//
// Shared mapping between output-pixel coordinates and virtual display-cell
// indices. Single source of truth used by both the synthesis stage and the
// artifact stage so per-cell defects (dead/stuck/hot pixels) align exactly with
// the emitted grid.
#pragma once

#include <cmath>

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::layout {

using math::Vec2;

struct GridMapping {
    Vec2 center{0, 0};
    float pitch = 1.0f;
    float gridRot = 0.0f;      // radians (already negated for the transform)
    Vec2 gridOffset{0, 0};

    static GridMapping from(const DisplayParams& d, int w, int h) {
        GridMapping m;
        m.center = {w * 0.5f, h * 0.5f};
        float scale = std::max(d.resolutionScale, 0.01f);
        m.pitch = std::max(d.pixelSize * scale, 1.0f);
        m.gridRot = -d.gridRotationDeg * 3.14159265358979f / 180.0f;
        m.gridOffset = {d.gridOffsetX, d.gridOffsetY};
        return m;
    }

    /// Grid-space coordinate for a pixel center (x+0.5, y+0.5).
    Vec2 toGrid(int x, int y) const {
        Vec2 p{x + 0.5f, y + 0.5f};
        return math::rotate(p - center, gridRot) + center + gridOffset;
    }

    void cell(int x, int y, int& cx, int& cy) const {
        Vec2 g = toGrid(x, y);
        cx = static_cast<int>(std::floor(g.x / pitch));
        cy = static_cast<int>(std::floor(g.y / pitch));
    }

    /// Total number of whole cells spanning the image (for defect densities).
    long cellCount(int w, int h) const {
        long nx = static_cast<long>(std::ceil(w / pitch)) + 1;
        long ny = static_cast<long>(std::ceil(h / pitch)) + 1;
        return nx * ny;
    }
};

}  // namespace pd::layout

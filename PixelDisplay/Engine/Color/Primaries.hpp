// PixelDisplay Pro — Engine/Color/Primaries.hpp
//
// RGB primaries conversions. The engine's working space is scene-linear with
// Rec.709/sRGB primaries (D65). Input signals in other gamuts (Display P3,
// Rec.2020) are converted into the working space; the output is converted back
// to the requested gamut before encoding. Conversions go through CIE XYZ.
//
// Matrices are the standard Bradford-adapted D65 RGB<->XYZ matrices. All are
// constexpr so a compile-time product yields a single input->working or
// working->output matrix per colour space.
#pragma once

#include "Engine/Core/Types.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::color {

using math::Mat3;

// RGB -> XYZ (D65) for each gamut.
inline constexpr Mat3 kRec709ToXYZ{{
    0.4123908f, 0.3575843f, 0.1804808f,
    0.2126390f, 0.7151687f, 0.0721923f,
    0.0193308f, 0.1191948f, 0.9505322f}};

inline constexpr Mat3 kXYZToRec709{{
     3.2409699f, -1.5373832f, -0.4986108f,
    -0.9692436f,  1.8759675f,  0.0415551f,
     0.0556301f, -0.2039770f,  1.0569715f}};

inline constexpr Mat3 kP3ToXYZ{{
    0.4865709f, 0.2656677f, 0.1982173f,
    0.2289746f, 0.6917385f, 0.0792869f,
    0.0000000f, 0.0451134f, 1.0439444f}};

inline constexpr Mat3 kXYZToP3{{
     2.4934969f, -0.9313836f, -0.4027108f,
    -0.8294890f,  1.7626641f,  0.0236247f,
     0.0358458f, -0.0761724f,  0.9568845f}};

inline constexpr Mat3 kRec2020ToXYZ{{
    0.6369580f, 0.1446169f, 0.1688810f,
    0.2627002f, 0.6779981f, 0.0593017f,
    0.0000000f, 0.0280727f, 1.0609851f}};

inline constexpr Mat3 kXYZToRec2020{{
     1.7166512f, -0.3556708f, -0.2533663f,
    -0.6666844f,  1.6164812f,  0.0157685f,
     0.0176399f, -0.0427706f,  0.9421031f}};

inline Mat3 gamutToXYZ(ColorSpace s) {
    switch (s) {
        case ColorSpace::DisplayP3: return kP3ToXYZ;
        case ColorSpace::Rec2020:   return kRec2020ToXYZ;
        default:                    return kRec709ToXYZ;  // sRGB/Rec709/Linear
    }
}

/// Matrix converting linear RGB in `from` primaries to the working space
/// (linear Rec.709). Identity for sRGB/Rec.709 inputs.
inline Mat3 inputToWorking(ColorSpace from) {
    if (from == ColorSpace::sRGB || from == ColorSpace::Rec709 ||
        from == ColorSpace::Linear)
        return Mat3{};  // identity
    return kXYZToRec709 * gamutToXYZ(from);
}

/// Matrix converting working-space linear RGB (Rec.709) to `to` primaries.
inline Mat3 workingToOutput(ColorSpace to) {
    switch (to) {
        case ColorSpace::DisplayP3: return kXYZToP3 * kRec709ToXYZ;
        case ColorSpace::Rec2020:   return kXYZToRec2020 * kRec709ToXYZ;
        default:                    return Mat3{};  // identity
    }
}

}  // namespace pd::color

// PixelDisplay Pro — Engine/Core/ParamSnapshot.hpp
//
// The complete, flat, copyable description of an effect configuration for ONE
// render. The host adapter builds this from AE parameters; the engine reads it
// and nothing else. It is a value type (no pointers into host memory except
// owned mask blobs, which are copied in). Passed by const-ref into render().
//
// Grouping mirrors the UI groups in Documentation/DESIGN.md §8. Defaults are
// chosen so that a default-constructed snapshot with display.enable == false is
// an exact passthrough (output == input).
#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Core/Types.hpp"

namespace pd {

/// Procedurally-generated display panel types. Never bitmap textures.
enum class DisplayType {
    LcdRgbStripe, LcdBgrStripe, Oled, PentileOled, DiamondOled,
    MiniLed, MicroLed, CrtShadowMask, CrtApertureGrille, LedBillboard,
    GameBoyLcd, NintendoDs, RetinaLcd, StudioDisplay, MacBookMiniLed,
    SamsungAmoled, RgbLedMatrix, Hexagonal, Circular, Square,
    RoundedSquare, Diamond,
};

enum class SubpixelOrder { RGB, BGR, RGBG_Pentile, Custom };
enum class CaDirection { Radial, Horizontal, Vertical };
enum class StuckPixelMode { RandomRGB, Green, Blue, Red, White };
enum class SensorDirection { TopToBottom, BottomToTop, LeftToRight, RightToLeft };

// ---- Parameter groups ------------------------------------------------------

struct DisplayParams {
    bool  enable = false;            // master gate; false => passthrough
    float pixelSize = 8.0f;          // virtual display cell pitch, in source px
    float dotSize = 0.85f;           // emitter footprint fraction of the cell
    float spacing = 0.15f;           // inter-cell gap fraction
    float resolutionScale = 1.0f;
    float softness = 0.0f;
    float brightnessCompensation = 1.0f;
    float edgeSoftening = 0.05f;
    float pixelRoundness = 0.2f;     // 0 = square, 1 = circle
    float pixelRotationDeg = 0.0f;
    float pixelAspect = 1.0f;
    float gridOffsetX = 0.0f;
    float gridOffsetY = 0.0f;
    float gridRotationDeg = 0.0f;
    float pixelRandomness = 0.0f;    // seeded per-cell jitter [0..1]
    std::uint32_t randomSeed = 1u;
};

struct SubpixelParams {
    bool  enable = true;
    float size = 1.0f;
    float gap = 0.1f;
    float softness = 0.1f;
    float brightness = 1.0f;
    float gamma = 1.0f;
    float scaleR = 1.0f, scaleG = 1.0f, scaleB = 1.0f;
    SubpixelOrder order = SubpixelOrder::RGB;
};

struct ColorParams {
    bool  linearWorkflow = true;
    ColorSpace inputSpace = ColorSpace::sRGB;
    ColorSpace outputSpace = ColorSpace::sRGB;

    bool  caEnable = false;
    float caAmount = 0.0f;
    CaDirection caDirection = CaDirection::Radial;

    math::Vec2 offsetR{0, 0}, offsetG{0, 0}, offsetB{0, 0};  // independent RGB offset

    float contrast = 1.0f;
    float brightness = 0.0f;
    float exposure = 0.0f;
    float gamma = 1.0f;
    float saturation = 1.0f;
    float vibrance = 0.0f;
    float whiteBalance = 0.0f;  // -1 cool .. +1 warm
    float tint = 0.0f;          // -1 green .. +1 magenta
    float highlightCompression = 0.0f;
    float shadowLift = 0.0f;
};

struct DisplayCharacteristics {
    float glowRadius = 0.0f, glowIntensity = 0.0f;
    float bloomThreshold = 1.0f, bloomIntensity = 0.0f;
    float blackLevel = 0.0f;
    float backlightBleed = 0.0f;
    float diffusion = 0.0f;
    float displayNoise = 0.0f;
    float pixelFlicker = 0.0f;
    float pixelAging = 0.0f;
    float responseTimeMs = 0.0f;
    float imagePersistence = 0.0f;
};

struct ArtifactParams {
    // Dead / stuck / hot pixels
    std::int32_t deadPixelCount = 0;   // 0..10000
    std::uint32_t deadPixelSeed = 1u;
    float deadPixelBrightness = 0.0f;
    math::Vec3 deadPixelColor{0, 0, 0};
    bool  deadPixelClusters = false;
    std::int32_t stuckPixelCount = 0;
    StuckPixelMode stuckPixelMode = StuckPixelMode::RandomRGB;
    std::int32_t hotPixelCount = 0;

    // Panel-scale non-uniformity
    float mura = 0.0f;
    float panelUniformity = 0.0f;
    float brightnessDrift = 0.0f;
    float columnDefects = 0.0f;
    float rowDefects = 0.0f;
    float banding = 0.0f;

    // Dirty screen
    float dust = 0.0f, hair = 0.0f, microScratches = 0.0f;
    float fingerprints = 0.0f, pressureMarks = 0.0f;

    float lightLeakage = 0.0f;
    float vignetting = 0.0f;
};

struct BurnInParams {
    bool  enable = false;
    float intensity = 0.0f;
    float age = 0.0f;             // 0..1 panel wear
    float persistence = 0.0f;
    float recoverySpeed = 0.0f;
    float ghosting = 0.0f;
    bool  logo = false, statusBar = false, window = false, taskbar = false;
    // Optional user-imported grayscale mask (copied in; engine-owned).
    std::vector<float> customMask;   // row-major, size = maskW*maskH, empty if unused
    std::int32_t maskW = 0, maskH = 0;
};

struct AnimationParams {
    float scanlineThickness = 0.0f, scanlineOpacity = 0.0f, scanlineMovement = 0.0f;
    bool  rollingRefreshEnable = false;
    SensorDirection rollingDirection = SensorDirection::TopToBottom;
    float rollingSpeed = 0.0f;
    float refreshRateHz = 60.0f;   // 24/30/60/90/120/144/165/240
    float pwmFrequency = 0.0f, pwmDutyCycle = 1.0f, pwmIntensity = 0.0f;
    float randomFlicker = 0.0f;
    float pixelTwinkle = 0.0f;
    float temporalNoise = 0.0f;
    float pixelWarmUp = 0.0f;
    float lcdResponseDelay = 0.0f;
    bool  oledInstantMode = true;
};

struct LensParams {
    bool  rollingShutterEnable = false;
    float readoutTimeMs = 0.0f;
    SensorDirection sensorDirection = SensorDirection::TopToBottom;
    float rollingShutterOffset = 0.0f;
    float cameraSyncHz = 0.0f;

    float chromaticAberration = 0.0f;
    float lensBlur = 0.0f;
    float reflection = 0.0f;
    float refraction = 0.0f;
    float moire = 0.0f;
    float cameraDefocus = 0.0f;
    float screenCurvature = 0.0f;
    float glassThickness = 0.0f;
    float polarizer = 0.0f;
    float antiReflectiveCoating = 0.0f;
};

struct PerformanceParams {
    bool  gpuEnable = true;
    bool  cpuEnable = true;   // allow CPU even if GPU present (parity/testing)
    bool  adaptiveQuality = false;
    bool  draftMode = false;
    float previewResolution = 1.0f;
    float finalResolution = 1.0f;
    bool  tileRendering = true;
    bool  patternCache = true, shaderCache = true, maskCache = true;
    bool  showMemoryUsage = false, showStatistics = false;
};

// ---- The snapshot ----------------------------------------------------------

struct ParamSnapshot {
    static constexpr std::uint32_t kSchemaVersion = 1;
    std::uint32_t schemaVersion = kSchemaVersion;

    DisplayType displayType = DisplayType::LcdRgbStripe;

    DisplayParams          display;
    SubpixelParams         subpixel;
    ColorParams            color;
    DisplayCharacteristics characteristics;
    ArtifactParams         artifacts;
    BurnInParams           burnIn;
    AnimationParams        animation;
    LensParams             lens;
    PerformanceParams      performance;

    /// True when the effect should output the source unchanged.
    bool isPassthrough() const { return !display.enable; }
};

}  // namespace pd

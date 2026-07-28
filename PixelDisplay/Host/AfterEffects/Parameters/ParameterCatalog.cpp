// PixelDisplay Pro — Host/AfterEffects/Parameters/ParameterCatalog.cpp
#include "Host/AfterEffects/Parameters/ParameterCatalog.hpp"

namespace pd::host {

namespace {

using P = ParamType;
using G = Group;

// Small builders keep the catalog declarative and readable.
ParamInfo fl(G g, const char* id, const char* label, float def, float lo, float hi,
             const char* unit = "") {
    return {g, id, label, P::Float, def, lo, hi, unit, {}};
}
ParamInfo in(G g, const char* id, const char* label, float def, float lo, float hi) {
    return {g, id, label, P::Int, def, lo, hi, "", {}};
}
ParamInfo bl(G g, const char* id, const char* label, bool def) {
    return {g, id, label, P::Bool, def ? 1.0f : 0.0f, 0, 1, "", {}};
}
ParamInfo col(G g, const char* id, const char* label) {
    return {g, id, label, P::Color, 0, 0, 1, "", {}};
}
ParamInfo btn(G g, const char* id, const char* label) {
    return {g, id, label, P::Button, 0, 0, 0, "", {}};
}
ParamInfo en(G g, const char* id, const char* label, int def, std::vector<EnumOption> opts) {
    return {g, id, label, P::Enum, static_cast<float>(def), 0,
            static_cast<float>(opts.size() - 1), "", std::move(opts)};
}

std::vector<ParamInfo> build() {
    std::vector<ParamInfo> c;

    // Display
    c.push_back(btn(G::Display, "reset.display", "Reset Section"));
    c.push_back(bl(G::Display, "display.enable", "Enable", false));
    c.push_back(fl(G::Display, "display.pixelSize", "Pixel Size", 8, 1, 256, "px"));
    c.push_back(fl(G::Display, "display.dotSize", "Dot Size", 0.85f, 0, 1.5f));
    c.push_back(fl(G::Display, "display.spacing", "Spacing", 0.15f, 0, 0.95f));
    c.push_back(fl(G::Display, "display.resolutionScale", "Resolution Scale", 1, 0.1f, 4));
    c.push_back(fl(G::Display, "display.softness", "Softness", 0, 0, 1));
    c.push_back(fl(G::Display, "display.brightnessCompensation", "Brightness Compensation", 1, 0, 4));
    c.push_back(fl(G::Display, "display.edgeSoftening", "Edge Softening", 0.05f, 0, 1));
    c.push_back(fl(G::Display, "display.pixelRoundness", "Pixel Roundness", 0.2f, 0, 1));
    c.push_back(fl(G::Display, "display.pixelRotationDeg", "Pixel Rotation", 0, -180, 180, "°"));
    c.push_back(fl(G::Display, "display.pixelAspect", "Pixel Aspect Ratio", 1, 0.25f, 4));
    c.push_back(fl(G::Display, "display.gridOffsetX", "Grid Offset X", 0, -256, 256, "px"));
    c.push_back(fl(G::Display, "display.gridOffsetY", "Grid Offset Y", 0, -256, 256, "px"));
    c.push_back(fl(G::Display, "display.gridRotationDeg", "Grid Rotation", 0, -180, 180, "°"));
    c.push_back(fl(G::Display, "display.pixelRandomness", "Pixel Randomness", 0, 0, 1));

    // Pattern (display type)
    c.push_back(btn(G::Pattern, "reset.pattern", "Reset Section"));
    c.push_back(en(G::Pattern, "displayType", "Display Type", 0, {
        {0, "LCD RGB Stripe"}, {1, "LCD BGR Stripe"}, {2, "OLED"}, {3, "Pentile OLED"},
        {4, "Diamond OLED"}, {5, "MiniLED"}, {6, "MicroLED"}, {7, "CRT Shadow Mask"},
        {8, "CRT Aperture Grille"}, {9, "LED Billboard"}, {10, "GameBoy LCD"},
        {11, "Nintendo DS"}, {12, "Retina LCD"}, {13, "Studio Display"},
        {14, "MacBook MiniLED"}, {15, "Samsung AMOLED"}, {16, "RGB LED Matrix"},
        {17, "Hexagonal"}, {18, "Circular"}, {19, "Square"}, {20, "Rounded Square"},
        {21, "Diamond"}}));

    // Subpixels
    c.push_back(btn(G::Subpixels, "reset.subpixels", "Reset Section"));
    c.push_back(bl(G::Subpixels, "subpixel.enable", "Enable Subpixels", true));
    c.push_back(fl(G::Subpixels, "subpixel.size", "Subpixel Size", 1, 0.1f, 1.5f));
    c.push_back(fl(G::Subpixels, "subpixel.gap", "Subpixel Gap", 0.1f, 0, 0.9f));
    c.push_back(fl(G::Subpixels, "subpixel.softness", "Subpixel Softness", 0.1f, 0, 1));
    c.push_back(fl(G::Subpixels, "subpixel.brightness", "Subpixel Brightness", 1, 0, 4));
    c.push_back(fl(G::Subpixels, "subpixel.gamma", "Subpixel Gamma", 1, 0.2f, 4));
    c.push_back(fl(G::Subpixels, "subpixel.scaleR", "Red Scale", 1, 0, 2));
    c.push_back(fl(G::Subpixels, "subpixel.scaleG", "Green Scale", 1, 0, 2));
    c.push_back(fl(G::Subpixels, "subpixel.scaleB", "Blue Scale", 1, 0, 2));
    c.push_back(en(G::Subpixels, "subpixel.order", "RGB Ordering", 0,
                   {{0, "RGB"}, {1, "BGR"}, {2, "RGBG (PenTile)"}, {3, "Custom"}}));

    // Color
    c.push_back(btn(G::Color, "reset.color", "Reset Section"));
    c.push_back(bl(G::Color, "color.linearWorkflow", "Linear Workflow", true));
    c.push_back(en(G::Color, "color.inputSpace", "Input Color Space", 1,
                   {{0, "Linear"}, {1, "sRGB"}, {2, "Display P3"}, {3, "Rec.709"}, {4, "Rec.2020"}}));
    c.push_back(en(G::Color, "color.outputSpace", "Output Color Space", 1,
                   {{0, "Linear"}, {1, "sRGB"}, {2, "Display P3"}, {3, "Rec.709"}, {4, "Rec.2020"}}));
    c.push_back(bl(G::Color, "color.caEnable", "Chromatic Aberration", false));
    c.push_back(fl(G::Color, "color.caAmount", "CA Amount", 0, 0, 50, "px"));
    c.push_back(en(G::Color, "color.caDirection", "CA Direction", 0,
                   {{0, "Radial"}, {1, "Horizontal"}, {2, "Vertical"}}));
    c.push_back(fl(G::Color, "color.contrast", "Contrast", 1, 0, 3));
    c.push_back(fl(G::Color, "color.brightness", "Brightness", 0, -1, 1));
    c.push_back(fl(G::Color, "color.exposure", "Exposure", 0, -5, 5, "stops"));
    c.push_back(fl(G::Color, "color.gamma", "Gamma", 1, 0.2f, 4));
    c.push_back(fl(G::Color, "color.saturation", "Saturation", 1, 0, 3));
    c.push_back(fl(G::Color, "color.vibrance", "Vibrance", 0, -1, 1));
    c.push_back(fl(G::Color, "color.whiteBalance", "White Balance", 0, -1, 1));
    c.push_back(fl(G::Color, "color.tint", "Tint", 0, -1, 1));
    c.push_back(fl(G::Color, "color.highlightCompression", "Highlight Compression", 0, 0, 1));
    c.push_back(fl(G::Color, "color.shadowLift", "Shadow Lift", 0, 0, 1));

    // Display Characteristics
    c.push_back(btn(G::DisplayCharacteristics, "reset.chr", "Reset Section"));
    c.push_back(fl(G::DisplayCharacteristics, "chr.glowRadius", "Glow Radius", 0, 0, 64, "px"));
    c.push_back(fl(G::DisplayCharacteristics, "chr.glowIntensity", "Glow Intensity", 0, 0, 4));
    c.push_back(fl(G::DisplayCharacteristics, "chr.bloomThreshold", "Bloom Threshold", 1, 0, 4));
    c.push_back(fl(G::DisplayCharacteristics, "chr.bloomIntensity", "Bloom", 0, 0, 4));
    c.push_back(fl(G::DisplayCharacteristics, "chr.blackLevel", "Black Level", 0, 0, 0.5f));
    c.push_back(fl(G::DisplayCharacteristics, "chr.backlightBleed", "Backlight Bleed", 0, 0, 1));
    c.push_back(fl(G::DisplayCharacteristics, "chr.diffusion", "Display Diffusion", 0, 0, 1));
    c.push_back(fl(G::DisplayCharacteristics, "chr.displayNoise", "Display Noise", 0, 0, 1));
    c.push_back(fl(G::DisplayCharacteristics, "chr.pixelFlicker", "Pixel Flicker", 0, 0, 1));
    c.push_back(fl(G::DisplayCharacteristics, "chr.pixelAging", "Pixel Aging", 0, 0, 1));
    c.push_back(fl(G::DisplayCharacteristics, "chr.responseTimeMs", "Pixel Response Time", 0, 0, 50, "ms"));
    c.push_back(fl(G::DisplayCharacteristics, "chr.imagePersistence", "Image Persistence", 0, 0, 1));

    // Artifacts
    c.push_back(btn(G::Artifacts, "reset.artifacts", "Reset Section"));
    c.push_back(in(G::Artifacts, "art.deadPixelCount", "Dead Pixels", 0, 0, 10000));
    c.push_back(in(G::Artifacts, "art.deadPixelSeed", "Dead Pixel Random Seed", 1, 0, 100000));
    c.push_back(fl(G::Artifacts, "art.deadPixelBrightness", "Dead Pixel Brightness", 0, 0, 1));
    c.push_back(col(G::Artifacts, "art.deadPixelColor", "Dead Pixel Color"));
    c.push_back(bl(G::Artifacts, "art.deadPixelClusters", "Dead Pixel Clusters", false));
    c.push_back(in(G::Artifacts, "art.stuckPixelCount", "Stuck Pixels", 0, 0, 10000));
    c.push_back(en(G::Artifacts, "art.stuckPixelMode", "Stuck Pixel Color", 0,
                   {{0, "Random RGB"}, {1, "Green"}, {2, "Blue"}, {3, "Red"}, {4, "White"}}));
    c.push_back(in(G::Artifacts, "art.hotPixelCount", "Hot Pixels", 0, 0, 10000));
    c.push_back(fl(G::Artifacts, "art.mura", "Mura Effect", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.panelUniformity", "Panel Uniformity", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.brightnessDrift", "Brightness Drift", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.columnDefects", "Column Defects", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.rowDefects", "Row Defects", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.banding", "Banding", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.dust", "Dust", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.hair", "Hair", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.microScratches", "Micro Scratches", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.fingerprints", "Fingerprints", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.pressureMarks", "Pressure Marks", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.lightLeakage", "Light Leakage", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "art.vignetting", "Vignetting", 0, 0, 1));
    c.push_back(bl(G::Artifacts, "burn.enable", "Burn-In Enable", false));
    c.push_back(fl(G::Artifacts, "burn.intensity", "Burn-In Intensity", 0, 0, 2));
    c.push_back(fl(G::Artifacts, "burn.age", "Burn-In Age", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "burn.persistence", "Burn-In Persistence", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "burn.recoverySpeed", "Recovery Speed", 0, 0, 1));
    c.push_back(fl(G::Artifacts, "burn.ghosting", "Ghosting", 0, 0, 1));
    c.push_back(bl(G::Artifacts, "burn.logo", "Logo Burn-In", false));
    c.push_back(bl(G::Artifacts, "burn.statusBar", "Status Bar Burn-In", false));
    c.push_back(bl(G::Artifacts, "burn.window", "Window Burn-In", false));
    c.push_back(bl(G::Artifacts, "burn.taskbar", "Taskbar Burn-In", false));

    // Animation
    c.push_back(btn(G::Animation, "reset.animation", "Reset Section"));
    c.push_back(fl(G::Animation, "anim.scanlineThickness", "Scanline Thickness", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.scanlineOpacity", "Scanline Opacity", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.scanlineMovement", "Scanline Movement", 0, -1, 1));
    c.push_back(bl(G::Animation, "anim.rollingRefreshEnable", "Rolling Refresh", false));
    c.push_back(fl(G::Animation, "anim.rollingSpeed", "Refresh Speed", 0, 0, 10));
    c.push_back(en(G::Animation, "anim.refreshRateHz", "Refresh Rate", 2,
                   {{0, "24 Hz"}, {1, "30 Hz"}, {2, "60 Hz"}, {3, "90 Hz"}, {4, "120 Hz"},
                    {5, "144 Hz"}, {6, "165 Hz"}, {7, "240 Hz"}}));
    c.push_back(fl(G::Animation, "anim.pwmFrequency", "PWM Frequency", 0, 0, 2000, "Hz"));
    c.push_back(fl(G::Animation, "anim.pwmDutyCycle", "PWM Duty Cycle", 1, 0.02f, 1));
    c.push_back(fl(G::Animation, "anim.pwmIntensity", "PWM Intensity", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.randomFlicker", "Random Flicker", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.pixelTwinkle", "Pixel Twinkle", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.temporalNoise", "Temporal Noise", 0, 0, 1));
    c.push_back(fl(G::Animation, "anim.pixelWarmUp", "Pixel Warm-Up", 0, 0, 1));
    c.push_back(bl(G::Animation, "anim.oledInstantMode", "OLED Instant Mode", true));

    // Lens / Camera
    c.push_back(btn(G::Lens, "reset.lens", "Reset Section"));
    c.push_back(bl(G::Lens, "lens.rollingShutterEnable", "Rolling Shutter", false));
    c.push_back(fl(G::Lens, "lens.readoutTimeMs", "Readout Time", 0, 0, 100, "ms"));
    c.push_back(en(G::Lens, "lens.sensorDirection", "Sensor Direction", 0,
                   {{0, "Top→Bottom"}, {1, "Bottom→Top"}, {2, "Left→Right"}, {3, "Right→Left"}}));
    c.push_back(fl(G::Lens, "lens.rollingShutterOffset", "Shutter Offset", 0, -1, 1));
    c.push_back(fl(G::Lens, "lens.cameraSyncHz", "Camera Sync", 0, 0, 240, "Hz"));
    c.push_back(fl(G::Lens, "lens.chromaticAberration", "Lens Chromatic Aberration", 0, 0, 20));
    c.push_back(fl(G::Lens, "lens.lensBlur", "Lens Blur", 0, 0, 32, "px"));
    c.push_back(fl(G::Lens, "lens.reflection", "Display Reflection", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.refraction", "Display Refraction", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.moire", "Moiré", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.cameraDefocus", "Camera Defocus", 0, 0, 32, "px"));
    c.push_back(fl(G::Lens, "lens.screenCurvature", "Screen Curvature", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.glassThickness", "Glass Thickness", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.polarizer", "Polarizer Simulation", 0, 0, 1));
    c.push_back(fl(G::Lens, "lens.antiReflectiveCoating", "Anti-Reflective Coating", 0, 0, 1));

    // Performance
    c.push_back(btn(G::Performance, "reset.performance", "Reset Section"));
    c.push_back(bl(G::Performance, "perf.gpuEnable", "GPU Enable", true));
    c.push_back(bl(G::Performance, "perf.cpuEnable", "CPU Enable", true));
    c.push_back(bl(G::Performance, "perf.adaptiveQuality", "Adaptive Quality", false));
    c.push_back(bl(G::Performance, "perf.draftMode", "Draft Mode", false));
    c.push_back(fl(G::Performance, "perf.previewResolution", "Preview Resolution", 1, 0.1f, 1));
    c.push_back(fl(G::Performance, "perf.finalResolution", "Final Resolution", 1, 0.1f, 1));
    c.push_back(bl(G::Performance, "perf.tileRendering", "Tile Rendering", true));
    c.push_back(bl(G::Performance, "perf.patternCache", "Pattern Cache", true));
    c.push_back(bl(G::Performance, "perf.shaderCache", "Shader Cache", true));
    c.push_back(bl(G::Performance, "perf.maskCache", "Mask Cache", true));
    c.push_back(bl(G::Performance, "perf.showMemoryUsage", "Memory Usage Display", false));
    c.push_back(bl(G::Performance, "perf.showStatistics", "Render Statistics", false));

    // Presets (popup filled by the preset system) + Utilities
    c.push_back(en(G::Presets, "preset.select", "Preset", 0, {{0, "(none)"}}));
    c.push_back(btn(G::Utilities, "util.reset", "Reset"));
    c.push_back(btn(G::Utilities, "util.resetCategory", "Reset Current Category"));
    c.push_back(btn(G::Utilities, "util.randomize", "Randomize"));
    c.push_back(btn(G::Utilities, "util.copy", "Copy Settings"));
    c.push_back(btn(G::Utilities, "util.paste", "Paste Settings"));
    c.push_back(btn(G::Utilities, "util.importPreset", "Import Preset"));
    c.push_back(btn(G::Utilities, "util.exportPreset", "Export Preset"));
    c.push_back(btn(G::Utilities, "util.saveUser", "Save User Preset"));
    c.push_back(btn(G::Utilities, "util.loadUser", "Load User Preset"));

    return c;
}

}  // namespace

const std::vector<ParamInfo>& catalog() {
    static const std::vector<ParamInfo> c = build();
    return c;
}

const char* groupName(Group g) {
    switch (g) {
        case Group::Display:                return "Display";
        case Group::Pattern:                return "Pattern";
        case Group::Subpixels:              return "Subpixels";
        case Group::Color:                  return "Color";
        case Group::DisplayCharacteristics: return "Display Characteristics";
        case Group::Artifacts:              return "Artifacts";
        case Group::Animation:              return "Animation";
        case Group::Lens:                   return "Lens";
        case Group::Performance:            return "Performance";
        case Group::Presets:                return "Presets";
        case Group::Utilities:              return "Utilities";
    }
    return "";
}

}  // namespace pd::host

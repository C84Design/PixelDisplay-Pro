// PixelDisplay Pro — Host/AfterEffects/Presets/Presets.cpp
#include "Host/AfterEffects/Presets/Presets.hpp"

#include <functional>
#include <sstream>
#include <unordered_map>

#include "Engine/Noise/Hash.hpp"

namespace pd::host {

// ---- Field visitor: single source of truth for (de)serialization ----------
//
// Each field is exposed as a (key, get, set) triple in double precision. Using
// one list for both directions guarantees serialize/deserialize are symmetric,
// so export→import round-trips exactly.
namespace {

struct Field {
    const char* key;
    std::function<double()> get;
    std::function<void(double)> set;
};

std::vector<Field> fieldsOf(ParamSnapshot& s) {
    std::vector<Field> f;
    auto F = [&](const char* k, std::function<double()> g, std::function<void(double)> st) {
        f.push_back({k, std::move(g), std::move(st)});
    };
    auto enumF = [&](const char* k, auto& e) {
        using E = std::remove_reference_t<decltype(e)>;
        F(k, [&e] { return double(static_cast<int>(e)); },
          [&e](double v) { e = static_cast<E>(static_cast<int>(v + 0.5)); });
    };
    auto boolF = [&](const char* k, bool& b) {
        F(k, [&b] { return b ? 1.0 : 0.0; }, [&b](double v) { b = v != 0.0; });
    };
    auto fl = [&](const char* k, float& x) {
        F(k, [&x] { return double(x); }, [&x](double v) { x = float(v); });
    };
    auto iF = [&](const char* k, std::int32_t& x) {
        F(k, [&x] { return double(x); }, [&x](double v) { x = std::int32_t(v + 0.5); });
    };
    auto uF = [&](const char* k, std::uint32_t& x) {
        F(k, [&x] { return double(x); }, [&x](double v) { x = std::uint32_t(v + 0.5); });
    };

    enumF("displayType", s.displayType);
    // Display
    boolF("display.enable", s.display.enable);
    fl("display.pixelSize", s.display.pixelSize);
    fl("display.dotSize", s.display.dotSize);
    fl("display.spacing", s.display.spacing);
    fl("display.resolutionScale", s.display.resolutionScale);
    fl("display.softness", s.display.softness);
    fl("display.brightnessCompensation", s.display.brightnessCompensation);
    fl("display.edgeSoftening", s.display.edgeSoftening);
    fl("display.pixelRoundness", s.display.pixelRoundness);
    fl("display.pixelRotationDeg", s.display.pixelRotationDeg);
    fl("display.pixelAspect", s.display.pixelAspect);
    fl("display.gridOffsetX", s.display.gridOffsetX);
    fl("display.gridOffsetY", s.display.gridOffsetY);
    fl("display.gridRotationDeg", s.display.gridRotationDeg);
    fl("display.pixelRandomness", s.display.pixelRandomness);
    uF("display.randomSeed", s.display.randomSeed);
    // Subpixel
    boolF("subpixel.enable", s.subpixel.enable);
    fl("subpixel.size", s.subpixel.size);
    fl("subpixel.gap", s.subpixel.gap);
    fl("subpixel.softness", s.subpixel.softness);
    fl("subpixel.brightness", s.subpixel.brightness);
    fl("subpixel.gamma", s.subpixel.gamma);
    fl("subpixel.scaleR", s.subpixel.scaleR);
    fl("subpixel.scaleG", s.subpixel.scaleG);
    fl("subpixel.scaleB", s.subpixel.scaleB);
    enumF("subpixel.order", s.subpixel.order);
    // Color
    boolF("color.linearWorkflow", s.color.linearWorkflow);
    enumF("color.inputSpace", s.color.inputSpace);
    enumF("color.outputSpace", s.color.outputSpace);
    boolF("color.caEnable", s.color.caEnable);
    fl("color.caAmount", s.color.caAmount);
    enumF("color.caDirection", s.color.caDirection);
    fl("color.contrast", s.color.contrast);
    fl("color.brightness", s.color.brightness);
    fl("color.exposure", s.color.exposure);
    fl("color.gamma", s.color.gamma);
    fl("color.saturation", s.color.saturation);
    fl("color.vibrance", s.color.vibrance);
    fl("color.whiteBalance", s.color.whiteBalance);
    fl("color.tint", s.color.tint);
    fl("color.highlightCompression", s.color.highlightCompression);
    fl("color.shadowLift", s.color.shadowLift);
    // Characteristics
    fl("chr.glowRadius", s.characteristics.glowRadius);
    fl("chr.glowIntensity", s.characteristics.glowIntensity);
    fl("chr.bloomThreshold", s.characteristics.bloomThreshold);
    fl("chr.bloomIntensity", s.characteristics.bloomIntensity);
    fl("chr.blackLevel", s.characteristics.blackLevel);
    fl("chr.backlightBleed", s.characteristics.backlightBleed);
    fl("chr.diffusion", s.characteristics.diffusion);
    fl("chr.displayNoise", s.characteristics.displayNoise);
    fl("chr.pixelFlicker", s.characteristics.pixelFlicker);
    fl("chr.pixelAging", s.characteristics.pixelAging);
    fl("chr.responseTimeMs", s.characteristics.responseTimeMs);
    fl("chr.imagePersistence", s.characteristics.imagePersistence);
    // Artifacts
    iF("art.deadPixelCount", s.artifacts.deadPixelCount);
    uF("art.deadPixelSeed", s.artifacts.deadPixelSeed);
    fl("art.deadPixelBrightness", s.artifacts.deadPixelBrightness);
    boolF("art.deadPixelClusters", s.artifacts.deadPixelClusters);
    iF("art.stuckPixelCount", s.artifacts.stuckPixelCount);
    enumF("art.stuckPixelMode", s.artifacts.stuckPixelMode);
    iF("art.hotPixelCount", s.artifacts.hotPixelCount);
    fl("art.mura", s.artifacts.mura);
    fl("art.panelUniformity", s.artifacts.panelUniformity);
    fl("art.brightnessDrift", s.artifacts.brightnessDrift);
    fl("art.columnDefects", s.artifacts.columnDefects);
    fl("art.rowDefects", s.artifacts.rowDefects);
    fl("art.banding", s.artifacts.banding);
    fl("art.dust", s.artifacts.dust);
    fl("art.hair", s.artifacts.hair);
    fl("art.microScratches", s.artifacts.microScratches);
    fl("art.fingerprints", s.artifacts.fingerprints);
    fl("art.pressureMarks", s.artifacts.pressureMarks);
    fl("art.lightLeakage", s.artifacts.lightLeakage);
    fl("art.vignetting", s.artifacts.vignetting);
    // Burn-in
    boolF("burn.enable", s.burnIn.enable);
    fl("burn.intensity", s.burnIn.intensity);
    fl("burn.age", s.burnIn.age);
    fl("burn.persistence", s.burnIn.persistence);
    fl("burn.recoverySpeed", s.burnIn.recoverySpeed);
    fl("burn.ghosting", s.burnIn.ghosting);
    boolF("burn.logo", s.burnIn.logo);
    boolF("burn.statusBar", s.burnIn.statusBar);
    boolF("burn.window", s.burnIn.window);
    boolF("burn.taskbar", s.burnIn.taskbar);
    // Animation
    fl("anim.scanlineThickness", s.animation.scanlineThickness);
    fl("anim.scanlineOpacity", s.animation.scanlineOpacity);
    fl("anim.scanlineMovement", s.animation.scanlineMovement);
    boolF("anim.rollingRefreshEnable", s.animation.rollingRefreshEnable);
    fl("anim.rollingSpeed", s.animation.rollingSpeed);
    fl("anim.refreshRateHz", s.animation.refreshRateHz);
    fl("anim.pwmFrequency", s.animation.pwmFrequency);
    fl("anim.pwmDutyCycle", s.animation.pwmDutyCycle);
    fl("anim.pwmIntensity", s.animation.pwmIntensity);
    fl("anim.randomFlicker", s.animation.randomFlicker);
    fl("anim.pixelTwinkle", s.animation.pixelTwinkle);
    fl("anim.temporalNoise", s.animation.temporalNoise);
    fl("anim.pixelWarmUp", s.animation.pixelWarmUp);
    boolF("anim.oledInstantMode", s.animation.oledInstantMode);
    // Lens
    boolF("lens.rollingShutterEnable", s.lens.rollingShutterEnable);
    fl("lens.readoutTimeMs", s.lens.readoutTimeMs);
    enumF("lens.sensorDirection", s.lens.sensorDirection);
    fl("lens.rollingShutterOffset", s.lens.rollingShutterOffset);
    fl("lens.cameraSyncHz", s.lens.cameraSyncHz);
    fl("lens.chromaticAberration", s.lens.chromaticAberration);
    fl("lens.lensBlur", s.lens.lensBlur);
    fl("lens.reflection", s.lens.reflection);
    fl("lens.refraction", s.lens.refraction);
    fl("lens.moire", s.lens.moire);
    fl("lens.cameraDefocus", s.lens.cameraDefocus);
    fl("lens.screenCurvature", s.lens.screenCurvature);
    fl("lens.glassThickness", s.lens.glassThickness);
    fl("lens.polarizer", s.lens.polarizer);
    fl("lens.antiReflectiveCoating", s.lens.antiReflectiveCoating);
    return f;
}

}  // namespace

std::string serialize(const ParamSnapshot& s) {
    ParamSnapshot copy = s;  // fieldsOf needs a mutable ref for get/set closures
    std::ostringstream os;
    os << "PixelDisplayPro preset v" << ParamSnapshot::kSchemaVersion << "\n";
    for (const auto& f : fieldsOf(copy)) os << f.key << '=' << f.get() << '\n';
    return os.str();
}

bool deserialize(const std::string& text, ParamSnapshot& out) {
    std::unordered_map<std::string, double> kv;
    std::istringstream is(text);
    std::string line;
    while (std::getline(is, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        try {
            kv[line.substr(0, eq)] = std::stod(line.substr(eq + 1));
        } catch (...) { /* skip malformed lines */ }
    }
    if (kv.empty()) return false;
    for (auto& f : fieldsOf(out)) {
        auto it = kv.find(f.key);
        if (it != kv.end()) f.set(it->second);
    }
    return true;
}

ParamSnapshot randomize(const ParamSnapshot& baseline, std::uint32_t seed) {
    ParamSnapshot s = baseline;
    s.display.enable = true;
    auto r = [&](int i) { return noise::hash2f(i, 0, seed); };
    s.displayType = static_cast<DisplayType>(
        static_cast<int>(r(1) * static_cast<float>(static_cast<int>(DisplayType::Diamond) + 1)));
    s.display.pixelSize = 4.0f + r(2) * 20.0f;
    s.display.spacing = r(3) * 0.4f;
    s.display.dotSize = 0.6f + r(4) * 0.4f;
    s.display.pixelRoundness = r(5);
    s.characteristics.glowIntensity = r(6) * 1.5f;
    s.characteristics.glowRadius = r(7) * 10.0f;
    s.artifacts.mura = r(8) * 0.5f;
    s.artifacts.vignetting = r(9) * 0.6f;
    s.artifacts.banding = r(10) * 0.4f;
    s.lens.chromaticAberration = r(11) * 4.0f;
    return s;
}

// ---- The shipped presets ---------------------------------------------------

const char* presetName(PresetId id) {
    switch (id) {
        case PresetId::AppleStudioDisplay: return "Apple Studio Display";
        case PresetId::MacBookProMiniLed:  return "MacBook Pro MiniLED";
        case PresetId::AppleRetina:        return "Apple Retina";
        case PresetId::SamsungAmoled:      return "Samsung AMOLED";
        case PresetId::SonyOled:           return "Sony OLED";
        case PresetId::SonyTrinitron:      return "Sony Trinitron";
        case PresetId::DellIps:            return "Dell IPS";
        case PresetId::CheapTnPanel:       return "Cheap TN Panel";
        case PresetId::LedBillboard:       return "LED Billboard";
        case PresetId::AirportDisplay:     return "Airport Display";
        case PresetId::CrtTelevision:      return "CRT Television";
        case PresetId::ArcadeCrt:          return "Arcade CRT";
        case PresetId::BrokenLcd:          return "Broken LCD";
        case PresetId::OldLaptop:          return "Old Laptop";
        case PresetId::GameBoy:            return "GameBoy";
        case PresetId::NintendoDs:         return "Nintendo DS";
        case PresetId::RetroLcd:           return "Retro LCD";
        case PresetId::BrokenOled:         return "Broken OLED";
        case PresetId::BurnedOled:         return "Burned OLED";
        case PresetId::DamagedDisplay:     return "Damaged Display";
        default:                           return "Unknown";
    }
}

ParamSnapshot makePreset(PresetId id) {
    ParamSnapshot s;
    s.display.enable = true;
    s.subpixel.enable = true;

    switch (id) {
        case PresetId::AppleStudioDisplay:
            s.displayType = DisplayType::StudioDisplay; s.display.pixelSize = 3.0f;
            s.display.spacing = 0.06f; s.color.outputSpace = ColorSpace::DisplayP3;
            s.characteristics.glowRadius = 2; s.characteristics.glowIntensity = 0.15f;
            break;
        case PresetId::MacBookProMiniLed:
            s.displayType = DisplayType::MacBookMiniLed; s.display.pixelSize = 3.0f;
            s.color.outputSpace = ColorSpace::DisplayP3; s.characteristics.bloomIntensity = 0.2f;
            s.characteristics.bloomThreshold = 0.85f; s.characteristics.backlightBleed = 0.03f;
            break;
        case PresetId::AppleRetina:
            s.displayType = DisplayType::RetinaLcd; s.display.pixelSize = 2.5f;
            s.display.spacing = 0.05f; break;
        case PresetId::SamsungAmoled:
            s.displayType = DisplayType::SamsungAmoled; s.display.pixelSize = 4.0f;
            s.color.saturation = 1.2f; s.characteristics.glowIntensity = 0.2f; break;
        case PresetId::SonyOled:
            s.displayType = DisplayType::Oled; s.display.pixelSize = 4.0f;
            s.characteristics.blackLevel = 0.0f; s.characteristics.bloomIntensity = 0.25f;
            s.characteristics.bloomThreshold = 0.8f; break;
        case PresetId::SonyTrinitron:
            s.displayType = DisplayType::CrtApertureGrille; s.display.pixelSize = 6.0f;
            s.animation.scanlineOpacity = 0.25f; s.animation.scanlineThickness = 0.5f;
            s.lens.screenCurvature = 0.25f; s.characteristics.glowRadius = 3;
            s.characteristics.glowIntensity = 0.3f; break;
        case PresetId::DellIps:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 4.0f;
            s.characteristics.backlightBleed = 0.06f; break;
        case PresetId::CheapTnPanel:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 5.0f;
            s.color.contrast = 0.85f; s.artifacts.panelUniformity = 0.3f;
            s.artifacts.mura = 0.25f; s.color.saturation = 0.85f; break;
        case PresetId::LedBillboard:
            s.displayType = DisplayType::LedBillboard; s.display.pixelSize = 20.0f;
            s.display.spacing = 0.35f; s.characteristics.glowRadius = 6;
            s.characteristics.glowIntensity = 0.6f; s.display.brightnessCompensation = 1.4f;
            break;
        case PresetId::AirportDisplay:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 6.0f;
            s.artifacts.deadPixelCount = 12; s.artifacts.mura = 0.2f;
            s.characteristics.backlightBleed = 0.08f; break;
        case PresetId::CrtTelevision:
            s.displayType = DisplayType::CrtShadowMask; s.display.pixelSize = 7.0f;
            s.animation.scanlineOpacity = 0.35f; s.lens.screenCurvature = 0.5f;
            s.characteristics.glowRadius = 4; s.characteristics.glowIntensity = 0.4f;
            s.artifacts.vignetting = 0.3f; break;
        case PresetId::ArcadeCrt:
            s.displayType = DisplayType::CrtApertureGrille; s.display.pixelSize = 5.0f;
            s.animation.scanlineOpacity = 0.4f; s.color.saturation = 1.25f;
            s.characteristics.glowIntensity = 0.5f; s.characteristics.glowRadius = 4;
            s.lens.screenCurvature = 0.35f; break;
        case PresetId::BrokenLcd:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 5.0f;
            s.artifacts.deadPixelCount = 300; s.artifacts.stuckPixelCount = 150;
            s.artifacts.columnDefects = 0.4f; s.artifacts.lightLeakage = 0.4f;
            s.artifacts.mura = 0.4f; break;
        case PresetId::OldLaptop:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 5.0f;
            s.artifacts.panelUniformity = 0.4f; s.artifacts.brightnessDrift = 0.3f;
            s.characteristics.backlightBleed = 0.12f; s.color.contrast = 0.9f; break;
        case PresetId::GameBoy:
            s.displayType = DisplayType::GameBoyLcd; s.display.pixelSize = 10.0f;
            s.display.spacing = 0.15f; s.subpixel.enable = false;
            s.color.saturation = 0.0f; s.color.tint = 0.4f; s.color.whiteBalance = 0.2f; break;
        case PresetId::NintendoDs:
            s.displayType = DisplayType::NintendoDs; s.display.pixelSize = 6.0f; break;
        case PresetId::RetroLcd:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 8.0f;
            s.display.spacing = 0.25f; s.color.saturation = 0.9f; break;
        case PresetId::BrokenOled:
            s.displayType = DisplayType::PentileOled; s.display.pixelSize = 5.0f;
            s.artifacts.columnDefects = 0.5f; s.artifacts.deadPixelCount = 200;
            s.burnIn.enable = true; s.burnIn.intensity = 0.5f; s.burnIn.age = 0.6f; break;
        case PresetId::BurnedOled:
            s.displayType = DisplayType::SamsungAmoled; s.display.pixelSize = 4.0f;
            s.burnIn.enable = true; s.burnIn.intensity = 1.0f; s.burnIn.age = 0.9f;
            s.burnIn.statusBar = true; s.burnIn.taskbar = true; s.burnIn.ghosting = 0.6f; break;
        case PresetId::DamagedDisplay:
            s.displayType = DisplayType::LcdRgbStripe; s.display.pixelSize = 5.0f;
            s.artifacts.deadPixelCount = 500; s.artifacts.stuckPixelCount = 250;
            s.artifacts.rowDefects = 0.5f; s.artifacts.columnDefects = 0.5f;
            s.artifacts.lightLeakage = 0.5f; s.artifacts.mura = 0.5f;
            s.artifacts.microScratches = 0.4f; s.lens.refraction = 0.2f; break;
        default: break;
    }
    return s;
}

}  // namespace pd::host

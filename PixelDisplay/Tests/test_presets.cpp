// PixelDisplay Pro — Tests/test_presets.cpp
//
// Milestones 10–11 acceptance: the UI parameter catalog is complete and sane,
// and the preset system builds/serializes/randomizes correctly and renders end
// to end through the engine.
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Host/AfterEffects/Parameters/ParameterCatalog.hpp"
#include "Host/AfterEffects/Presets/Presets.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;
using namespace pd::host;

PD_TEST("parameter catalog covers all groups with valid ranges") {
    const auto& cat = catalog();
    PD_CHECK(cat.size() > 90);   // full control surface
    std::set<int> groups;
    for (const auto& p : cat) {
        groups.insert(static_cast<int>(p.group));
        if (p.type == ParamType::Float || p.type == ParamType::Int) {
            PD_CHECK(p.minValue <= p.defaultValue);
            PD_CHECK(p.defaultValue <= p.maxValue);
        }
        if (p.type == ParamType::Enum) PD_CHECK(!p.options.empty());
        PD_CHECK(!p.id.empty());
        PD_CHECK(!p.label.empty());
    }
    PD_CHECK_EQ(groups.size(), std::size_t(11));   // all UI groups present
}

PD_TEST("all 20 presets enable the display and are distinct") {
    std::set<std::string> seen;
    for (int i = 0; i < static_cast<int>(PresetId::Count); ++i) {
        ParamSnapshot s = makePreset(static_cast<PresetId>(i));
        PD_CHECK(s.display.enable);
        PD_CHECK(std::string(presetName(static_cast<PresetId>(i))) != "Unknown");
        seen.insert(serialize(s));
    }
    PD_CHECK_EQ(seen.size(), std::size_t(PresetId::Count));  // no two identical
}

PD_TEST("serialize/deserialize round-trips exactly") {
    for (int i = 0; i < static_cast<int>(PresetId::Count); ++i) {
        ParamSnapshot s = makePreset(static_cast<PresetId>(i));
        std::string text = serialize(s);
        ParamSnapshot back;
        PD_CHECK(deserialize(text, back));
        PD_CHECK(serialize(back) == text);   // stable round-trip
    }
}

PD_TEST("deserialize rejects garbage and survives partial input") {
    ParamSnapshot s;
    PD_CHECK(!deserialize("", s));
    PD_CHECK(!deserialize("no equals signs here\njust text", s));
    // Partial input applies only the keys it recognises.
    ParamSnapshot p; p.display.pixelSize = 99.0f;
    PD_CHECK(deserialize("display.pixelSize=12.5\n", p));
    PD_CHECK_NEAR(p.display.pixelSize, 12.5f, 1e-4f);
}

PD_TEST("randomize is deterministic per seed") {
    ParamSnapshot base;
    ParamSnapshot a = randomize(base, 123);
    ParamSnapshot b = randomize(base, 123);
    ParamSnapshot c = randomize(base, 999);
    PD_CHECK(serialize(a) == serialize(b));
    PD_CHECK(serialize(a) != serialize(c));
    PD_CHECK(a.display.enable);
}

PD_TEST("a preset renders end-to-end through the engine") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    ParamSnapshot p = makePreset(PresetId::BurnedOled);

    const int w = 64, h = 64;
    std::vector<std::uint8_t> in(static_cast<std::size_t>(w) * h * 4, 200), out(in.size(), 0);
    ImageView vin{in.data(), w, h, (std::ptrdiff_t)w * 4, PixelFormat::ARGB8, ColorSpace::sRGB};
    ImageView vout = vin; vout.data = out.data();
    RenderRequest req{vin, vout, &p, {}, RenderQuality::Final, {}, nullptr};
    req.time.layerTimeSeconds = 10.0;
    PD_CHECK(eng->render(req).ok());
    PD_CHECK(in != out);   // the preset visibly transformed the frame
}

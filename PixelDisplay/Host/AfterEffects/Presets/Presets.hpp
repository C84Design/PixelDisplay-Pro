// PixelDisplay Pro — Host/AfterEffects/Presets/Presets.hpp
//
// The shipped preset library plus (de)serialization and utility operations
// (randomize / reset / copy-paste). Presets are plain ParamSnapshot data, so
// they are engine-side and unit-tested without the After Effects SDK.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Engine/Core/ParamSnapshot.hpp"

namespace pd::host {

/// The 20 professionally-tuned presets (DESIGN.md §14), in menu order.
enum class PresetId {
    AppleStudioDisplay, MacBookProMiniLed, AppleRetina, SamsungAmoled, SonyOled,
    SonyTrinitron, DellIps, CheapTnPanel, LedBillboard, AirportDisplay,
    CrtTelevision, ArcadeCrt, BrokenLcd, OldLaptop, GameBoy, NintendoDs,
    RetroLcd, BrokenOled, BurnedOled, DamagedDisplay,
    Count
};

const char* presetName(PresetId id);

/// Build the tuned ParamSnapshot for a preset.
ParamSnapshot makePreset(PresetId id);

/// Serialize a snapshot to a stable, human-readable key=value text block, and
/// parse it back. Round-trips exactly for all scalar/enum parameters (the
/// imported custom burn-in mask is handled separately by the host).
std::string serialize(const ParamSnapshot& s);
bool deserialize(const std::string& text, ParamSnapshot& out);

/// Deterministically randomize the creative parameters from a seed (leaves
/// performance/colour-management settings alone). Same seed => same result.
ParamSnapshot randomize(const ParamSnapshot& baseline, std::uint32_t seed);

}  // namespace pd::host

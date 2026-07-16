// PixelDisplay Pro — Host/AfterEffects/Parameters/ParameterCatalog.hpp
//
// The single, SDK-independent description of every UI control: its group, label,
// type, default, and range. This catalog drives BOTH the After Effects
// parameter registration and the collapsible UI panel layout, so the UI and the
// engine can never drift out of sync. It contains no Adobe headers and is unit-
// tested without the SDK.
//
// The host adapter walks this catalog to (a) register PF parameters in order and
// (b) build a ParamSnapshot from the current parameter values. Group order here
// is the panel order (DESIGN.md §8 / UI groups).
#pragma once

#include <string>
#include <vector>

namespace pd::host {

/// Collapsible UI groups, in panel order.
enum class Group {
    Display, Pattern, Subpixels, Color, DisplayCharacteristics,
    Artifacts, Animation, Lens, Performance, Presets, Utilities,
};

enum class ParamType { Bool, Float, Int, Enum, Color, Button, Group };

struct EnumOption { int value; std::string label; };

struct ParamInfo {
    Group group;
    std::string id;         // stable identifier
    std::string label;      // UI label
    ParamType type;
    float defaultValue = 0; // for Float/Int/Bool(0/1)/Enum(index)
    float minValue = 0;
    float maxValue = 0;
    std::string unit;       // e.g. "px", "%", "°"
    std::vector<EnumOption> options;  // for Enum
};

/// The complete ordered catalog of controls.
const std::vector<ParamInfo>& catalog();

/// Human-readable name of a group (panel header).
const char* groupName(Group g);

}  // namespace pd::host

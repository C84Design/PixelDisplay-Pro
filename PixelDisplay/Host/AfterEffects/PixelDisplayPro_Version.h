// PixelDisplay Pro — Host/AfterEffects/PixelDisplayPro_Version.h
//
// Single source of the plugin's identity and version. Included by BOTH the C++
// entry point and the PiPL resource (.r) — both are run through the C
// preprocessor, so these plain #defines are safe in each. Keeping them here
// guarantees the compiled code and the resource AE reads at scan time agree.
#pragma once

#define PDP_NAME        "PixelDisplay Pro"
#define PDP_CATEGORY    "PixelDisplay"
#define PDP_MATCH_NAME  "CUBE84 PixelDisplay Pro"   // globally-unique, never change
#define PDP_DESCRIPTION "Procedural display-hardware simulation (CUBE84)."

#define PDP_MAJOR_VERSION 1
#define PDP_MINOR_VERSION 0
#define PDP_BUG_VERSION   0
#define PDP_STAGE_VERSION PF_Stage_DEVELOP
#define PDP_BUILD_VERSION 1

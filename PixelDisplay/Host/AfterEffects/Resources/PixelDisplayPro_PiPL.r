/* PixelDisplay Pro — PiPL resource
 *
 * The Plug-In Property List that After Effects reads at scan time to register
 * the effect: its menu name, category, match-name, entry-point symbol, spec
 * version, and global out-flags. Without this resource AE never lists the
 * plugin. Compiled by Rez (macOS) or PiPLtool (Windows) — see CMakeLists.txt.
 *
 * The out-flags below MUST match those set in GlobalSetup(); they are written
 * with the named PF_OutFlag_* constants so the two can never diverge.
 */

#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
    #include <AE_General.r>
#endif

#include "PixelDisplayPro_Version.h"

/* Rez compiles this resource WITHOUT AE_Effect.h, so the PF_VERSION macro and
 * the PF_Stage_* constants used by AE_Effect_Version below are not defined here
 * (they are only needed to fold the version fields into a single long). Provide
 * Rez-safe fallbacks with the SDK's bit layout. Guarded with #ifndef so the C++
 * entry point — which does include AE_Effect.h — always uses the real ones. */
#ifndef PF_Stage_DEVELOP
    #define PF_Stage_DEVELOP  0
    #define PF_Stage_ALPHA    1
    #define PF_Stage_BETA     2
    #define PF_Stage_RELEASE  3
#endif
#ifndef PF_VERSION
    #define PF_VERSION(MAJOR, MINOR, BUG, STAGE, BUILD) \
        (((MAJOR) << 19) | ((MINOR) << 15) | ((BUG) << 11) | ((STAGE) << 9) | (BUILD))
#endif

resource 'PiPL' (16000) {
    {
        /* [0] */
        Kind {
            AEEffect
        },
        /* [1] */
        Name {
            PDP_NAME
        },
        /* [2] */
        Category {
            PDP_CATEGORY
        },

        /* [3] platform-specific code entry points (all name the same symbol) */
#ifdef AE_OS_WIN
    #ifdef AE_PROC_INTELx64
        CodeWin64X86 {"EffectMain"},
    #endif
#else
    #ifdef AE_OS_MAC
        CodeMacIntel64 {"EffectMain"},
        CodeMacARM64 {"EffectMain"},
    #endif
#endif

        /* [6] */
        AE_PiPL_Version {
            2,
            0
        },
        /* [7] */
        AE_Effect_Spec_Version {
            PF_PLUG_IN_VERSION,
            PF_PLUG_IN_SUBVERS
        },
        /* [8] */
        AE_Effect_Version {
            PF_VERSION( PDP_MAJOR_VERSION,
                        PDP_MINOR_VERSION,
                        PDP_BUG_VERSION,
                        PDP_STAGE_VERSION,
                        PDP_BUILD_VERSION )
        },
        /* [9] */
        AE_Effect_Info_Flags {
            0
        },
        /* [10] global out-flags — keep in sync with GlobalSetup() */
        AE_Effect_Global_OutFlags {
            PF_OutFlag_DEEP_COLOR_AWARE |
            PF_OutFlag_PIX_INDEPENDENT |
            PF_OutFlag_NON_PARAM_VARY
        },
        AE_Effect_Global_OutFlags_2 {
            PF_OutFlag2_SUPPORTS_SMART_RENDER |
            PF_OutFlag2_FLOAT_COLOR_AWARE |
            PF_OutFlag2_SUPPORTS_THREADED_RENDERING
        },
        /* [11] */
        AE_Effect_Match_Name {
            PDP_MATCH_NAME
        },
        /* [12] */
        AE_Reserved_Info {
            0
        }
    }
};

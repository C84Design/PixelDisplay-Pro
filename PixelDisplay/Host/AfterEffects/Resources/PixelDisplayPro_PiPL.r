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
        /* [8] Folded version integer. Rez already defines PF_VERSION differently
         * from the C macro, so the value is written as a literal to match what
         * GlobalSetup() computes: PF_VERSION(1, 0, 0, PF_Stage_DEVELOP, 1) =
         * (1<<19) | (0<<15) | (0<<11) | (0<<9) | 1 = 524289. Keep in sync with
         * PixelDisplayPro_Version.h. */
        AE_Effect_Version {
            524289
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

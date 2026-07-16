# PixelDisplay Pro — After Effects plugin target (SDK-gated)
#
# Configured only when PD_AE_SDK_ROOT points at a valid Adobe AE SDK. Produces
# PixelDisplayPro.aex (Windows) / PixelDisplayPro.plugin (macOS). Links the
# engine + host-core; the engine target itself never sees the AE SDK includes.
#
# A working AE plugin needs TWO things beyond the code:
#   1. the PiPL resource compiled and embedded (so AE lists it in the menu), and
#   2. the correct package packaging (.aex on Windows, a .plugin bundle on mac).
# Both are handled below using the SDK's own tools (Rez / PiPLtool).

set(PDP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PDP_PIPL    ${PDP_SRC_DIR}/Resources/PixelDisplayPro_PiPL.r)

set(AE_HEADERS   ${PD_AE_SDK_ROOT}/Examples/Headers)
set(AE_HEADERS_SP ${PD_AE_SDK_ROOT}/Examples/Headers/SP)
set(AE_UTIL      ${PD_AE_SDK_ROOT}/Examples/Util)
set(AE_RESOURCES ${PD_AE_SDK_ROOT}/Examples/Resources)

add_library(PixelDisplayPro MODULE
    EntryPoint/PixelDisplayPro.cpp
)

target_include_directories(PixelDisplayPro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..   # repo root: "Engine/...", "Host/..."
    ${CMAKE_CURRENT_SOURCE_DIR}         # PixelDisplayPro_Version.h
    ${AE_HEADERS}
    ${AE_HEADERS_SP}
    ${AE_UTIL}
)

target_link_libraries(PixelDisplayPro PRIVATE pdengine pdhostcore)
target_compile_features(PixelDisplayPro PRIVATE cxx_std_20)
set_target_properties(PixelDisplayPro PROPERTIES PREFIX "")   # no "lib" prefix

# ---------------------------------------------------------------------------
# macOS: .plugin bundle + PiPL embedded via Rez into the executable's data fork
# ---------------------------------------------------------------------------
if(APPLE)
    set_target_properties(PixelDisplayPro PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        SUFFIX ""
        MACOSX_BUNDLE_INFO_PLIST ${PDP_SRC_DIR}/Resources/Info.plist.in
        MACOSX_BUNDLE_EXECUTABLE_NAME "PixelDisplayPro")
    target_compile_definitions(PixelDisplayPro PRIVATE MSWindows=0 AE_OS_MAC=1)

    find_program(REZ_EXECUTABLE Rez REQUIRED)

    # Write the classic 'eFKTFXTC' PkgInfo and compile+embed the PiPL resource.
    add_custom_command(TARGET PixelDisplayPro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "eFKTFXTC" >
                "$<TARGET_BUNDLE_DIR:PixelDisplayPro>/Contents/PkgInfo"
        COMMAND ${REZ_EXECUTABLE}
                -d __MACH__ -useDF
                -i "${AE_HEADERS}" -i "${AE_RESOURCES}" -i "${PDP_SRC_DIR}"
                -o "$<TARGET_FILE:PixelDisplayPro>"
                "${PDP_PIPL}"
        COMMENT "Embedding PiPL resource (Rez) and writing PkgInfo")

# ---------------------------------------------------------------------------
# Windows: .aex + PiPL compiled by PiPLtool into a .rc linked into the module
# ---------------------------------------------------------------------------
elseif(WIN32)
    set_target_properties(PixelDisplayPro PROPERTIES SUFFIX ".aex")
    target_compile_definitions(PixelDisplayPro PRIVATE MSWindows=1 AE_OS_WIN=1)

    find_program(PIPLTOOL_EXECUTABLE
        NAMES PiPLtool PiPLtool.exe
        PATHS "${AE_RESOURCES}" "${PD_AE_SDK_ROOT}/Resources"
        REQUIRED)

    set(PIPL_RR  ${CMAKE_CURRENT_BINARY_DIR}/PixelDisplayPro_PiPL.rr)
    set(PIPL_RC  ${CMAKE_CURRENT_BINARY_DIR}/PixelDisplayPro_PiPL.rc)

    # Documented AE PiPL build: preprocess the .r, then PiPLtool -> .rc, then let
    # the resource compiler/linker embed it in the .aex.
    add_custom_command(
        OUTPUT ${PIPL_RC}
        COMMAND ${CMAKE_CXX_COMPILER} /nologo /EP /I "${AE_HEADERS}"
                /I "${PDP_SRC_DIR}" "${PDP_PIPL}" > "${PIPL_RR}"
        COMMAND ${PIPLTOOL_EXECUTABLE} "${PIPL_RR}" "${PIPL_RC}"
        DEPENDS ${PDP_PIPL} ${PDP_SRC_DIR}/PixelDisplayPro_Version.h
        COMMENT "Compiling PiPL resource (PiPLtool)"
        VERBATIM)
    target_sources(PixelDisplayPro PRIVATE ${PIPL_RC})

# ---------------------------------------------------------------------------
# Other platforms (e.g. Linux CI): build the module so the code is verified,
# but the PiPL packaging step is macOS/Windows-only.
# ---------------------------------------------------------------------------
else()
    message(STATUS "PixelDisplayPro: building module for verification only "
                   "(PiPL packaging is macOS/Windows-only).")
endif()

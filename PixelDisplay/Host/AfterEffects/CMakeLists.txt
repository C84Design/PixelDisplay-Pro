# PixelDisplay Pro — After Effects plugin target (SDK-gated)
#
# Configured only when PD_AE_SDK_ROOT points at a valid Adobe AE SDK. Produces
# PixelDisplayPro.aex (Windows) / .plugin (macOS). Links the engine + host-core;
# the engine target itself never sees the AE SDK include dirs.

add_library(PixelDisplayPro MODULE
    EntryPoint/PixelDisplayPro.cpp
)

target_include_directories(PixelDisplayPro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${PD_AE_SDK_ROOT}/Examples/Headers
    ${PD_AE_SDK_ROOT}/Examples/Headers/SP
    ${PD_AE_SDK_ROOT}/Examples/Util
)

target_link_libraries(PixelDisplayPro PRIVATE pdengine pdhostcore)
target_compile_features(PixelDisplayPro PRIVATE cxx_std_20)

if(APPLE)
    set_target_properties(PixelDisplayPro PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        SUFFIX "")
elseif(WIN32)
    set_target_properties(PixelDisplayPro PROPERTIES SUFFIX ".aex")
endif()

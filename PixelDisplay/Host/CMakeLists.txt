# PixelDisplay Pro — Host layer
#
# `pdhostcore` holds the SDK-independent host logic: the UI parameter catalog and
# the preset system + serialization. It links the engine but pulls in NO Adobe
# headers, so it builds and unit-tests everywhere (the actual After Effects
# plugin/entry point is a separate, SDK-gated target).

add_library(pdhostcore STATIC
    AfterEffects/Parameters/ParameterCatalog.cpp
    AfterEffects/Presets/Presets.cpp
)

target_include_directories(pdhostcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(pdhostcore PUBLIC pdengine)
target_compile_features(pdhostcore PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(pdhostcore PRIVATE /W4)
else()
    target_compile_options(pdhostcore PRIVATE -Wall -Wextra)
endif()

add_library(PixelDisplay::HostCore ALIAS pdhostcore)

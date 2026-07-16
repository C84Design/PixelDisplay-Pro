# PixelDisplay Pro — Engine tests (headless, no Adobe SDK required)

add_executable(pdtests
    test_main.cpp
    test_passthrough.cpp
    test_synthesis.cpp
    test_subpixels.cpp
)

target_link_libraries(pdtests PRIVATE pdengine)
target_include_directories(pdtests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(MSVC)
    target_compile_options(pdtests PRIVATE /W4)
else()
    target_compile_options(pdtests PRIVATE -Wall -Wextra)
endif()

add_test(NAME pdtests COMMAND pdtests)

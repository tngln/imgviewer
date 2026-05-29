cmake_minimum_required(VERSION 3.29)

project(imgviewer LANGUAGES CXX)

add_executable(imgviewer
    src/image.decoder.cc
    src/main.cc
    src/renderer.cc
    src/surface.manager.cc
    src/ui.a11y.cc
    src/ui.cc
)

target_include_directories(imgviewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/wil/include"
)

target_compile_features(imgviewer PRIVATE cxx_std_20)

target_compile_definitions(imgviewer PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(imgviewer PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(MSVC)
    target_compile_options(imgviewer PRIVATE
        /W4
        /permissive-
        /utf-8
    )

    target_link_options(imgviewer PRIVATE
        /SUBSYSTEM:WINDOWS
        /ENTRY:mainCRTStartup
    )
endif()

target_link_libraries(imgviewer PRIVATE
    d2d1
    dcomp
    d3d11
    dwrite
    dxgi
    ole32
    shell32
    uiautomationcore
    user32
    windowscodecs
    gdi32
)

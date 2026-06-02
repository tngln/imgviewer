cmake_minimum_required(VERSION 3.29)

project(imgviewer LANGUAGES CXX)

add_executable(imgviewer
    src/app.cc
    src/app.action.cc
    src/app.config.cc
    src/app.host.cc
    src/app.keybindings.cc
    src/image.decoder.cc
    src/image.sequence.cc
    src/image.viewer.cc
    src/main.cc
    src/path.util.cc
    src/ui.a11y.cc
    src/ui.button.cc
    src/ui.draw.cc
    src/ui.element.cc
    src/ui.layout.cc
    src/ui.renderer.cc
    src/ui.surface.cc
    src/ui.text.cc
    src/ui.tooltip.cc
    src/ui.cc
    src/win32.dialog.cc
    src/win32.util.cc
)

target_include_directories(imgviewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/nlohmann_json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/wil/include"
)

target_compile_features(imgviewer PRIVATE cxx_std_20)

target_compile_definitions(imgviewer PRIVATE
    NOMINMAX
    UNICODE
    WIN32_LEAN_AND_MEAN
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
    comctl32
    d2d1
    dcomp
    d3d11
    dwmapi
    dwrite
    dxgi
    ole32
    shell32
    shlwapi
    uiautomationcore
    user32
    windowscodecs
    gdi32
    imm32
)

cmake_minimum_required(VERSION 3.29)

project(imgviewer LANGUAGES CXX RC)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(IMGVIEWER_DEVELOPER_WINDOW_CONFIGS "Debug" CACHE STRING
    "Semicolon-separated build configurations that show the Developer window.")

add_executable(imgviewer
    src/experimental/ui.bind.cc
    src/experimental/ui.decl.cc
    src/imgviewer.cc
    src/imgviewer.about.cc
    src/imgviewer.action.cc
    src/imgviewer.config.cc
    src/imgviewer.developer.cc
    src/imgviewer.edit.cc
    src/imgviewer.host.app_messages.cc
    src/imgviewer.host.chrome.cc
    src/imgviewer.host.cc
    src/imgviewer.host.keyboard.cc
    src/imgviewer.host.lifecycle.cc
    src/imgviewer.host.pointer.cc
    src/imgviewer.host.pointer_router.cc
    src/imgviewer.interaction.cc
    src/imgviewer.keybindings.cc
    src/imgviewer.renderer.cc
    src/imgviewer.settings.cc
    src/imgviewer.strings.cc
    src/imgviewer.ui.color_picker_toolstrip.cc
    src/imgviewer.ui.edit_toolbar.cc
    src/imgviewer.ui.floating_toolbar.cc
    src/imgviewer.ui.info_panel.cc
    src/imgviewer.ui.pen_toolstrip.cc
    src/imgviewer.ui.selection_toolstrip.cc
    src/imgviewer.ui.shape_toolstrip.cc
    src/imgviewer.ui.text_toolstrip.cc
    src/imgviewer.ui.toolstrip.cc
    src/imgviewer.ui.animation_toolbar.cc
    src/image.analysis.cc
    src/image.animation_decoder.cc
    src/image.decoder.cc
    src/image.encoder.cc
    src/image.metadata.cc
    src/image.sequence.cc
    src/image.stb_decoder.cc
    src/imgviewer.viewer.cc
    src/imgviewer.ui.cc
    src/imgviewer.ui.titlebar.cc
    src/imgviewer.ui.toolbar.cc
    src/main.cc
    src/ui.a11y.cc
    src/ui.button.cc
    src/ui.button_behavior.cc
    src/ui.host_accessibility.cc
    src/ui.host_effects.cc
    src/ui.host_ime.cc
    src/ui.host_popup.cc
    src/ui.label.cc
    src/ui.draw.cc
    src/ui.element.cc
    src/ui.events.cc
    src/ui.graphics_device.cc
    src/ui.layout.cc
    src/ui.menu.cc
    src/ui.panel.cc
    src/ui.popup.cc
    src/ui.renderer.cc
    src/ui.selection.cc
    src/ui.slider.cc
    src/ui.surface.cc
    src/ui.table.cc
    src/ui.text.cc
    src/ui.text_edit_state.cc
    src/ui.textbox.cc
    src/ui.toast.cc
    src/ui.tooltip.cc
    src/ui.window.cc
    src/ui.cc
    src/util.format.cc
    src/experimental/util.signal.cc
    src/win32.clipboard.cc
    src/win32.dialog.cc
    src/imgviewer.rc
    src/win32.screen_capture.cc
    src/win32.util.cc
    src/win32.window.cc
)

target_include_directories(imgviewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/nlohmann_json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/wil/include"
)

target_compile_features(imgviewer PRIVATE cxx_std_20)

target_compile_definitions(imgviewer PRIVATE
    NOMINMAX
    UNICODE
    WIN32_LEAN_AND_MEAN
    _UNICODE
)

foreach(developer_window_config IN LISTS IMGVIEWER_DEVELOPER_WINDOW_CONFIGS)
    if(NOT developer_window_config STREQUAL "")
        target_compile_definitions(imgviewer PRIVATE
            $<$<CONFIG:${developer_window_config}>:IMGVIEWER_ENABLE_DEVELOPER_WINDOW>
        )
    endif()
endforeach()

set_target_properties(imgviewer PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<CONFIG:Release>:MultiThreaded>"
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

# ---------------------------------------------------------------------------
# imgviewer_tests: minimal console regression harness for the pure modules.
# Safety net for the signal-driven/declarative refactor (see refactor.md).
# ---------------------------------------------------------------------------
add_executable(imgviewer_tests
    tests/imgviewer_tests.cc
    src/experimental/util.signal.cc
    src/imgviewer.action.cc
    src/imgviewer.host.pointer_router.cc
    src/imgviewer.interaction.cc
    src/imgviewer.keybindings.cc
    src/imgviewer.strings.cc
    src/ui.button_behavior.cc
    src/ui.element.cc
    src/ui.layout.cc
)

target_include_directories(imgviewer_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/nlohmann_json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/wil/include"
)

target_compile_features(imgviewer_tests PRIVATE cxx_std_20)

target_compile_definitions(imgviewer_tests PRIVATE
    NOMINMAX
    UNICODE
    WIN32_LEAN_AND_MEAN
    _UNICODE
)

set_target_properties(imgviewer_tests PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<CONFIG:Release>:MultiThreaded>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(MSVC)
    target_compile_options(imgviewer_tests PRIVATE
        /W4
        /permissive-
        /utf-8
    )
endif()

target_link_libraries(imgviewer_tests PRIVATE
    d2d1
    user32
)

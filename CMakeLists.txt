cmake_minimum_required(VERSION 3.29)

project(imgviewer LANGUAGES CXX RC)

if(WIN32 AND CMAKE_CXX_COMPILER)
    set(CMAKE_C_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "C compiler" FORCE)
endif()
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
enable_language(C)

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "Disable QuickJS-NG install rules" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "Do not build QuickJS-NG libc into qjs" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "Do not build QuickJS-NG examples" FORCE)
set(QJS_BUILD_WERROR OFF CACHE BOOL "Do not build QuickJS-NG with warnings as errors" FORCE)
add_subdirectory(third_parties/quickjs EXCLUDE_FROM_ALL)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(IMGVIEWER_DEVELOPER_WINDOW_CONFIGS "Debug" CACHE STRING
    "Semicolon-separated build configurations that show the Developer window.")
set(IMGVIEWER_DEVELOPER_SOURCE src/v2/imgviewer.developer.cc)
set(IMGVIEWER_SETTINGS_SOURCE src/v2/imgviewer.settings.cc)
set(IMGVIEWER_UI_SOURCE src/v2/imgviewer.ui.cc)

add_executable(imgviewer
    src/imgviewer.cc
    src/imgviewer.about.cc
    src/imgviewer.action.cc
    src/imgviewer.config.cc
    ${IMGVIEWER_DEVELOPER_SOURCE}
    src/imgviewer.edit.cc
    src/imgviewer.host.app_messages.cc
    src/imgviewer.host.chrome.cc
    src/imgviewer.host.cc
    src/imgviewer.host.ime.cc
    src/imgviewer.host.keyboard.cc
    src/imgviewer.host.lifecycle.cc
    src/imgviewer.host.pointer.cc
    src/imgviewer.host.pointer_router.cc
    src/imgviewer.interaction.cc
    src/imgviewer.keybindings.cc
    src/imgviewer.renderer.cc
    ${IMGVIEWER_SETTINGS_SOURCE}
    src/v2/imgviewer.script_engine.cc
    src/v2/imgviewer.script_ui.cc
    src/v2/imgviewer.script_window_root.cc
    src/imgviewer.strings.cc
    ${IMGVIEWER_UI_SOURCE}
    src/image.analysis.cc
    src/image.animation_decoder.cc
    src/image.decoder.cc
    src/image.encoder.cc
    src/image.metadata.cc
    src/image.sequence.cc
    src/image.stb_decoder.cc
    src/imgviewer.viewer.cc
    src/main.cc
    src/script.canvas_color.cc
    src/script.quickjs_runtime.cc
    src/ui.host_effects.cc
    src/ui.host_ime.cc
    src/ui.host_input.cc
    src/ui.host_popup.cc
    src/ui.draw.cc
    src/ui.events.cc
    src/ui.graphics_device.cc
    src/ui.popup.cc
    src/ui.renderer.cc
    src/ui.surface.cc
    src/ui.text.cc
    src/ui.text_edit_state.cc
    src/ui.toast.cc
    src/ui.window.cc
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
    VER2
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

set_target_properties(qjs PROPERTIES
    MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<CONFIG:Release>:MultiThreaded>"
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
    advapi32
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
    user32
    windowscodecs
    gdi32
    imm32
    qjs
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
    src/script.canvas_color.cc
    src/script.quickjs_runtime.cc
    src/ui.draw.cc
    src/v2/imgviewer.script_engine.cc
    src/v2/imgviewer.script_ui.cc
    src/v2/imgviewer.script_window_root.cc
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
    advapi32
    d2d1
    dwmapi
    user32
    qjs
)

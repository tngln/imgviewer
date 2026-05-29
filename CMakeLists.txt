cmake_minimum_required(VERSION 3.29)

project(imgviewer LANGUAGES CXX)

add_executable(imgviewer
    src/main.cc
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
    user32
    gdi32
)

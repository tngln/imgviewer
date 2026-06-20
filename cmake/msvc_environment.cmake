function(imgviewer_enable_msvc_environment)
    if(NOT WIN32 OR DEFINED ENV{VCINSTALLDIR})
        return()
    endif()

    find_program(_imgviewer_cl cl.exe)
    find_program(_imgviewer_rc rc.exe)
    if(_imgviewer_cl AND _imgviewer_rc)
        return()
    endif()

    set(_imgviewer_vswhere "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
    if(NOT EXISTS "${_imgviewer_vswhere}")
        message(STATUS "vswhere.exe was not found; keeping current compiler environment")
        return()
    endif()

    execute_process(
        COMMAND "${_imgviewer_vswhere}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC/Auxiliary/Build/vcvarsall.bat
        OUTPUT_VARIABLE _imgviewer_vcvarsall
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _imgviewer_vcvarsall OR NOT EXISTS "${_imgviewer_vcvarsall}")
        message(STATUS "MSVC x64 tools were not found; keeping current compiler environment")
        return()
    endif()

    set(_imgviewer_vcvars_script "${CMAKE_BINARY_DIR}/CMakeFiles/imgviewer_vcvars_env.bat")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/CMakeFiles")
    file(WRITE "${_imgviewer_vcvars_script}"
        "@echo off\n"
        "call \"${_imgviewer_vcvarsall}\" x64 >nul\n"
        "if errorlevel 1 exit /b %errorlevel%\n"
        "set\n"
    )

    execute_process(
        COMMAND cmd /d /c "${_imgviewer_vcvars_script}"
        OUTPUT_VARIABLE _imgviewer_vcvars_env
        RESULT_VARIABLE _imgviewer_vcvars_result
        ERROR_QUIET
    )
    if(NOT _imgviewer_vcvars_result EQUAL 0)
        message(STATUS "vcvarsall.bat failed; keeping current compiler environment")
        return()
    endif()

    string(REPLACE "\r\n" "\n" _imgviewer_vcvars_env "${_imgviewer_vcvars_env}")
    string(REPLACE "\r" "\n" _imgviewer_vcvars_env "${_imgviewer_vcvars_env}")
    string(REPLACE ";" "__IMGVIEWER_SEMICOLON__" _imgviewer_vcvars_env "${_imgviewer_vcvars_env}")
    string(REPLACE "\n" ";" _imgviewer_vcvars_lines "${_imgviewer_vcvars_env}")
    foreach(_imgviewer_line IN LISTS _imgviewer_vcvars_lines)
        string(REPLACE "__IMGVIEWER_SEMICOLON__" ";" _imgviewer_line "${_imgviewer_line}")
        if(_imgviewer_line MATCHES "^([^=]+)=(.*)$")
            set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
            if(CMAKE_MATCH_1 STREQUAL "Path")
                set(ENV{PATH} "${CMAKE_MATCH_2}")
            endif()
        endif()
    endforeach()

    if(CMAKE_GENERATOR MATCHES "Ninja" AND NOT CMAKE_MAKE_PROGRAM)
        find_program(_imgviewer_ninja ninja.exe)
        if(_imgviewer_ninja)
            set(CMAKE_MAKE_PROGRAM "${_imgviewer_ninja}" CACHE FILEPATH "Ninja build tool")
        endif()
    endif()

    if(NOT CMAKE_CXX_COMPILER)
        find_program(_imgviewer_cl_after_vcvars cl.exe)
        if(_imgviewer_cl_after_vcvars)
            set(CMAKE_CXX_COMPILER "${_imgviewer_cl_after_vcvars}" CACHE FILEPATH "CXX compiler")
        endif()
    endif()
    if(NOT CMAKE_C_COMPILER)
        find_program(_imgviewer_c_after_vcvars cl.exe)
        if(_imgviewer_c_after_vcvars)
            set(CMAKE_C_COMPILER "${_imgviewer_c_after_vcvars}" CACHE FILEPATH "C compiler")
        endif()
    endif()
    if(NOT CMAKE_RC_COMPILER)
        find_program(_imgviewer_rc_after_vcvars rc.exe)
        if(_imgviewer_rc_after_vcvars)
            set(CMAKE_RC_COMPILER "${_imgviewer_rc_after_vcvars}" CACHE FILEPATH "RC compiler")
        endif()
    endif()

    if(DEFINED ENV{INCLUDE})
        set(_imgviewer_msvc_include_dirs "$ENV{INCLUDE}")
        include_directories(SYSTEM BEFORE ${_imgviewer_msvc_include_dirs})
    endif()
    if(DEFINED ENV{LIB})
        set(_imgviewer_msvc_lib_dirs "$ENV{LIB}")
        link_directories(BEFORE ${_imgviewer_msvc_lib_dirs})
    endif()

    message(STATUS "Loaded MSVC x64 environment from ${_imgviewer_vcvarsall}")
endfunction()

imgviewer_enable_msvc_environment()

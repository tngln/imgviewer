@echo off
setlocal

taskkill /F /IM imgviewer.exe >nul 2>nul

set "ROOT=%~dp0"
set "CONFIG=%~1"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined CONFIG set "CONFIG=Debug"

if /I "%CONFIG%"=="Debug" (
    set "CONFIG=Debug"
) else if /I "%CONFIG%"=="Release" (
    set "CONFIG=Release"
) else (
    echo Usage: build.bat [Debug^|Release]
    exit /b 1
)

if not exist "%VSWHERE%" (
    echo vswhere.exe was not found.
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL=%%I"
)

if not defined VS_INSTALL (
    echo Visual Studio with MSVC x64 tools was not found.
    exit /b 1
)

set "VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo vcvars64.bat was not found: %VCVARS%
    exit /b 1
)

call "%VCVARS%"
if errorlevel 1 exit /b %errorlevel%

where cmake >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
    ) else (
        echo cmake.exe was not found in PATH or C:\Program Files\CMake\bin.
        exit /b 1
    )
) else (
    set "CMAKE_EXE=cmake"
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo ninja.exe was not found in PATH.
    exit /b 1
)

"%CMAKE_EXE%" --preset "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build --preset "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%

echo Built %ROOT%build\%CONFIG%\imgviewer.exe

@echo off
setlocal

taskkill /F /IM imgviewer.exe >nul 2>nul

set "ROOT=%~dp0"
set "CONFIG=%~1"
set "ARCH=%~2"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined CONFIG set "CONFIG=Debug"
if not defined ARCH set "ARCH=x64"

if /I "%CONFIG%"=="Debug" (
    set "CONFIG=Debug"
) else if /I "%CONFIG%"=="Release" (
    set "CONFIG=Release"
) else (
    echo Usage: build.bat [Debug^|Release] [x64^|arm64]
    exit /b 1
)

if /I "%ARCH%"=="x64" (
    set "ARCH=x64"
    set "VS_TOOLSET_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    set "VCVARS_ARG=amd64"
) else if /I "%ARCH%"=="arm64" (
    set "ARCH=arm64"
    set "VS_TOOLSET_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.ARM64"
    set "VCVARS_ARG=amd64_arm64"
) else (
    echo Usage: build.bat [Debug^|Release] [x64^|arm64]
    exit /b 1
)

if not exist "%VSWHERE%" (
    echo vswhere.exe was not found.
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires %VS_TOOLSET_COMPONENT% -property installationPath`) do (
    set "VS_INSTALL=%%I"
)

if not defined VS_INSTALL (
    echo Visual Studio with MSVC %ARCH% tools was not found.
    exit /b 1
)

set "VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo vcvarsall.bat was not found: %VCVARS%
    exit /b 1
)

call "%VCVARS%" %VCVARS_ARG%
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

"%CMAKE_EXE%" --preset "%CONFIG%-%ARCH%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build --preset "%CONFIG%-%ARCH%"
if errorlevel 1 exit /b %errorlevel%

echo Built %ROOT%build\%CONFIG%-%ARCH%\imgviewer.exe

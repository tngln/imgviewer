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
where bun >nul 2>nul
if errorlevel 1 (
    echo bun.exe was not found in PATH.
    exit /b 1
)

if not exist "%ROOT%build\%CONFIG%\scripts" mkdir "%ROOT%build\%CONFIG%\scripts"
bun build "%ROOT%scripts\ui\src\settings_ui.ts" --target=browser --format=iife --outdir "%ROOT%build\%CONFIG%\scripts" --entry-naming settings_ui.js --sourcemap=external
if errorlevel 1 exit /b %errorlevel%
bun build "%ROOT%scripts\ui\src\main_ui.ts" --target=browser --format=iife --outdir "%ROOT%build\%CONFIG%\scripts" --entry-naming main_ui.js --sourcemap=external
if errorlevel 1 exit /b %errorlevel%
bun build "%ROOT%scripts\ui\src\popup_ui.ts" --target=browser --format=iife --outdir "%ROOT%build\%CONFIG%\scripts" --entry-naming popup_ui.js --sourcemap=external
if errorlevel 1 exit /b %errorlevel%
bun build "%ROOT%scripts\ui\src\about_ui.ts" --target=browser --format=iife --outdir "%ROOT%build\%CONFIG%\scripts" --entry-naming about_ui.js --sourcemap=external
if errorlevel 1 exit /b %errorlevel%

set "BUILD_LOG=%TEMP%\imgviewer-build-%RANDOM%-%RANDOM%.log"

"%CMAKE_EXE%" --preset "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build --preset "%CONFIG%" >"%BUILD_LOG%" 2>&1
set "BUILD_EXIT=%ERRORLEVEL%"
python "%ROOT%tools\filter_build_output.py" "%BUILD_LOG%"
set "FILTER_EXIT=%ERRORLEVEL%"
del "%BUILD_LOG%" >nul 2>nul
if not "%FILTER_EXIT%"=="0" exit /b %FILTER_EXIT%
if not "%BUILD_EXIT%"=="0" exit /b %BUILD_EXIT%

echo Built %ROOT%build\%CONFIG%\imgviewer.exe

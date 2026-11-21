@echo off
setlocal enabledelayedexpansion

echo ======================================================================
echo   Grand Theft Auto V — Windows Native Port (Direct3D 11) Builder
echo ======================================================================

where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake is not found in PATH. Please install CMake and add it to your PATH.
    exit /b 1
)

:: Check for MSVC environment or locate vcvarsall.bat
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Looking for Visual Studio installation...
    for %%V in (Enterprise Professional Community BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvars64.bat" (
            echo Initializing MSVC x64 build environment...
            call "C:\Program Files\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvars64.bat"
            goto :vcfound
        )
    )
    echo [WARNING] MSVC compiler not initialized in current console. CMake will attempt auto-detection.
)
:vcfound

echo.
echo [1/3] Setting up dependencies & stubs...
if not exist "windows\third_party\webview2\build\native\x64\WebView2Loader.dll" (
    echo [INFO] Fetching WebView2 SDK package...
    powershell -Command "Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.2903.40/microsoft.web.webview2.1.0.2903.40.nupkg' -OutFile 'webview2.zip' -TimeoutSec 120; New-Item -ItemType Directory -Force -Path 'windows/third_party/webview2' | Out-Null; tar -xf webview2.zip -C 'windows/third_party/webview2'; Remove-Item 'webview2.zip' -Force"
)
where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    if not exist "app\Generated\game.h" (
        python app\Scripts\stub_asi_natives.py
        echo [INFO] AOT game engine not compiled in app\Generated.
        echo [INFO] Building native host shell. For full ~300 MB AOT native engine, run build_win_aot.bat.
    ) else (
        echo [INFO] Found AOT game engine in app\Generated. Building full native binary.
    )
)

echo.
echo [2/3] Configuring CMake for Windows x64...
if not exist "build_win" mkdir build_win

cmake -B build_win -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Compiling GTA5Port.exe (Release)...
cmake --build build_win --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ======================================================================
echo   Build Successful!
echo   Executable: build_win\Release\GTA5Port.exe (or build_win\GTA5Port.exe)
echo ======================================================================

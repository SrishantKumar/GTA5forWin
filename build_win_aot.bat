@echo off
setlocal enabledelayedexpansion

echo ======================================================================
echo   Grand Theft Auto V — Windows Native Port Full AOT Engine Builder
echo ======================================================================

:: Accept optional game folder argument: e.g. build_win_aot.bat D:\GTA5
set GAME_ARG=%~1
if "%GAME_ARG%"=="" (
    if exist "data" if exist "b\8b0b5899ed\game.wasm" (
        set GAME_ARG=.
    )
)

:: Step 1: Check Python and run AOT preparation if needed
if not exist "app\Generated\game.h" (
    echo.
    echo [1/3] Running Ahead-of-Time (AOT) engine preparation...
    where python >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Python 3 is required to run wasm2c and prepare AOT engine files.
        echo Please install Python from https://www.python.org/ or Microsoft Store.
        exit /b 1
    )
    if not "%GAME_ARG%"=="" (
        python app\Scripts\prepare_aot.py --mirror "%GAME_ARG%"
    ) else (
        python app\Scripts\prepare_aot.py
    )
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] AOT engine preparation failed.
        exit /b %ERRORLEVEL%
    )
) else (
    echo [INFO] Found existing AOT generated engine files in app\Generated.
)

:: Step 2: Configure CMake
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
echo [2/3] Configuring CMake for Windows x64 (Full Native AOT Engine)...
if not exist "build_win" mkdir build_win

cmake -B build_win -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Compiling Full Native GTA5Port.exe (Release)...
cmake --build build_win --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ======================================================================
echo   Build Successful! Full Native AOT Game Engine Compiled!
echo   Executable: build_win\Release\GTA5Port.exe (or build_win\GTA5Port.exe)
echo ======================================================================

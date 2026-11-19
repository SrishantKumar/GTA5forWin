# Grand Theft Auto V — 100% Native Windows (x64) Port

This port is a **true 100% native C/C++ application** for Windows 10 and 11 (x64), matching the exact native architecture of the macOS/iOS port (`gta5-macos-port`).

It contains **ZERO browser, Electron, Node.js, WebView, or web runtime dependencies**:
- **Engine Execution:** Direct execution of the ahead-of-time (AOT) compiled engine (`EngineHost.c` + `Generated/game.c`).
- **Graphics Pipeline (`WinGraphicsHost.cpp`):** Direct hardware **Direct3D 11** (`d3d11.lib`, `dxgi.lib`) consuming the engine's graphics command ring directly on your GPU with native swapchain presentation, vertex/constant buffer binding, and render target management.
- **Input Subsystem (`WinInput.cpp`):** High-precision **Win32 Raw Input (`WM_INPUT`)** with hardware pointer locking (`ClipCursor`, hidden cursor), mouse wheel delta routing, and **XInput (`xinput.h`)** for native Xbox and PC gamepads.
- **Audio Subsystem (`WinAudio.cpp`):** Low-latency **WASAPI (`IAudioClient`)** stream feeding 48,000 Hz 32-bit floating-point stereo audio directly from `PGFillAudio`.
- **Filesystem & State (`WinFiles.cpp`):** Native Windows directory layout (`%APPDATA%\GTA5Port\Data`) and asset verification against `data/manifest.json`.

---

## System Requirements

| Component | Minimum | Recommended |
|---|---|---|
| **OS** | Windows 10 (x64, Version 1903+) or Windows 11 | Windows 11 (64-bit) |
| **CPU** | Intel Core i5-3470 / AMD FX-8350 or equivalent quad-core | Intel Core i7-8700 / AMD Ryzen 5 3600 or newer |
| **GPU** | DirectX 11 compatible GPU (NVIDIA GT 730 / Intel Iris Xe) | NVIDIA GTX 1060 / AMD RX 580 or newer |
| **RAM** | 8 GB RAM | 16 GB RAM |
| **Storage** | ~25 GB free space (SSD recommended) | High-speed NVMe SSD |
| **Game Files** | Web build `8b0b5899ed` (`data/` and `b/8b0b5899ed/`) | Same game files used by the iPhone/Mac port |

---

## 1. Prerequisites (For Building on Windows)

1. **Visual Studio 2022** (Community, Professional, or Build Tools):
   - Install the **"Desktop development with C++"** workload.
   - Includes MSVC C++ compiler (`cl.exe`), Windows 10/11 SDK, and CMake.
2. **CMake 3.20+** (included with Visual Studio or standalone from [cmake.org](https://cmake.org/download/)).
3. **Git for Windows** ([git-scm.com](https://git-scm.com/)).

---

## 2. One-Click Build (`build_win.bat`)

Clone the repository and run the Windows build batch script:

```cmd
git clone https://github.com/kevinerikjs/gta5-macos-port.git
cd gta5-macos-port
build_win.bat
```

The script automatically detects your Visual Studio installation, configures CMake, compiles the native executable, and outputs `GTA5Port.exe` into `build_win\Release\`.

---

## 3. Manual CMake Build Commands

If you prefer building manually from the command line:

```cmd
# Open "x64 Native Tools Command Prompt for VS 2022"
cd gta5-macos-port

# Configure CMake
cmake -B build_win -S . -DCMAKE_BUILD_TYPE=Release

# Build executable (using all CPU cores)
cmake --build build_win --config Release --parallel
```

Run the built binary:
```cmd
build_win\Release\GTA5Port.exe
```

---

## 4. Game Files Setup

Like the macOS and iPhone versions, **this repository contains no copyrighted game files**. You provide your own copy of game build `8b0b5899ed`.

### Folder Layout:
```
YourGameFolder/
├── b/
│   └── 8b0b5899ed/
│       ├── game.wasm
│       └── shaders/
│           ├── index.json
│           └── pack_*.bin
└── data/
    ├── manifest.json
    ├── common/
    └── x64/
```

### Launching:
1. Run `GTA5Port.exe`.
2. On first launch, a native Windows folder picker dialog will prompt you to select your game directory.
3. The port verifies `b/8b0b5899ed/game.wasm`, `data/manifest.json`, and shaders, saving your choice to `%APPDATA%\GTA5Port\settings.ini`.
4. The game starts immediately with native Direct3D 11 rendering and WASAPI audio.

---

## 5. Controls & Features

- **Fullscreen:** Press `Alt+Enter` or `F11` to toggle borderless fullscreen.
- **Mouse & Keyboard:** Raw Input captures the mouse for 360-degree aiming. `WASD` moves, `Space` jumps/handbrakes, `F` enters/exits vehicles, `E` interacts, and scroll wheel selects weapons.
- **Pause Menu & Cursor Release:** Press `ESC` to toggle the pause menu and release mouse capture.
- **Xbox / XInput Controllers:** Automatically detected. Left stick moves, right stick aims, `RT`/`LT` triggers fire/aim, and `A`/`B`/`X`/`Y` handle game actions.
- **Self Radio:** Copy songs (`.mp3`, `.wav`, `.flac`) into `%APPDATA%\GTA5Port\Data\Self Radio`. With 3 or more songs, Self Radio is unlocked in the radio wheel.
- **Saves:** Stored natively in `%APPDATA%\GTA5Port\Data\userdata\Documents\Rockstar Games\GTA V\`.

---

## 6. Architecture & Platform Comparison

| Layer | macOS Port (`gta5-macos-port`) | Windows Port (`windows/`) |
|---|---|---|
| **Language** | Swift 5 + Obj-C / C | Pure C17 & C++20 |
| **Windowing** | AppKit / Mac Catalyst (`UIWindow`) | Pure Win32 (`RegisterClassExW`, `CreateWindowExW`) |
| **Graphics** | Apple Metal (`MetalGraphicsHost.mm`) | Direct3D 11 (`WinGraphicsHost.cpp`) |
| **Input** | `GameController.framework` + `MacCursor.swift` | Win32 Raw Input (`WM_INPUT`) + XInput |
| **Audio** | `AVFAudio` + `AudioToolbox` | Windows WASAPI (`IAudioClient`) |
| **Build System** | XcodeGen (`project.yml`) + Xcode | CMake (`CMakeLists.txt`) + MSVC / Clang |

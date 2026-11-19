# GTA5forWin

[![Windows 10/11](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6?logo=windows&logoColor=white)](#)
[![Direct3D 11](https://img.shields.io/badge/Graphics-Direct3D%2011-blue?logo=directx&logoColor=white)](#)
[![Audio WASAPI](https://img.shields.io/badge/Audio-WASAPI%20(48kHz)-green)](#)
[![Input](https://img.shields.io/badge/Input-RawInput%20%2B%20XInput-orange)](#)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](#)

> **Grand Theft Auto V — Standalone 100% Native Windows 10 & 11 (x64) Desktop Port**  
> **Port by [Srishant Kumar](https://github.com/srishantkumar)**

A true native C17 / C++20 Windows desktop port adapted from `gta5-macos-port` and the `playgta5` engine architecture.

---

## Highlights

- **100% Native C/C++:** Absolutely **NO** Electron, Chromium, WebView2, Node.js, HTML/JS, or local HTTP servers. Runs directly as a pure 64-bit Windows PE executable (`GTA5Port.exe`).
- **Direct3D 11 Hardware Graphics:** Dedicated DirectX 11 backend (`windows/WinGraphicsHost.cpp`) consuming the engine's graphics command ring directly on your GPU with native swapchain presentation, vertex/constant buffers, and BC texture sampling.
- **Raw Input Mouse & Aiming:** Hardware-level Win32 Raw Input (`WM_INPUT`) with automatic cursor clipping (`ClipCursor`) and hidden mouse pointer for 360-degree aiming without mouse escapes or stutter.
- **XInput Gamepad Passthrough:** Native Xbox Series / Xbox One / Xbox 360 and PC controller support with analog deadzone filtering.
- **Low-Latency WASAPI Audio:** Dedicated audio worker thread streaming 48,000 Hz, 32-bit floating-point stereo audio directly from the engine (`PGFillAudio`).
- **Native Windows State & Saves:** User documents and save files reside in `%APPDATA%\GTA5Port\Data\`.
- **One-Click GitHub Actions:** Automated GitHub Actions build workflow configured to **only run on manual click (`workflow_dispatch`)**, never cluttering your commit history.

---

## System Requirements

| Component | Minimum | Recommended |
|---|---|---|
| **OS** | Windows 10 (x64, Version 1903+) or Windows 11 | Windows 11 (64-bit) |
| **Processor** | Intel Core i5-3470 / AMD FX-8350 or equivalent | Intel Core i7-8700 / AMD Ryzen 5 3600 or newer |
| **Graphics** | DirectX 11 GPU (Intel HD 520 / GeForce GT 730) | NVIDIA GTX 1060 / AMD RX 580 or newer |
| **Memory** | 8 GB RAM | 16 GB RAM |
| **Storage** | ~25 GB free disk space | High-speed NVMe SSD |
| **Game Files** | Web build `8b0b5899ed` (`data/` and `b/8b0b5899ed/`) | Same game files used by the iPhone/Mac port |

---

## 1. Quick Build on Windows

### Prerequisites
- **Visual Studio 2022** (Community, Professional, or Build Tools) with the **"Desktop development with C++"** workload.
- **CMake 3.20+** (included with Visual Studio or available at [cmake.org](https://cmake.org/download/)).
- **Git for Windows** ([git-scm.com](https://git-scm.com/)).

### Option A: One-Click Build Script
```cmd
git clone https://github.com/SrishantKumar/GTA5forWin.git
cd GTA5forWin
build_win.bat
```
The script will locate your MSVC compiler, configure CMake, and compile `GTA5Port.exe` in `build_win\Release\`.

### Option B: Manual CMake Build
```cmd
cmake -B build_win -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build_win --config Release --parallel
```

Run the built executable:
```cmd
build_win\Release\GTA5Port.exe
```

---

## 2. GitHub Actions Automated Build

This repository includes a preconfigured GitHub Actions workflow: [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml).

- **Manual Trigger Only (`workflow_dispatch`):** The workflow **only** runs when you manually click **"Run workflow"** in the GitHub Actions tab. It will **not** trigger on every git push or commit.
- **Artifact Output:** Compiles on `windows-latest` with MSVC and CMake, publishing a downloadable zip containing `GTA5Port.exe`.

---

## 3. Game Files Setup

**This repository contains no copyrighted game files.** You must provide your own copy of game build `8b0b5899ed`.

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

### First Launch:
1. Run `GTA5Port.exe`.
2. A native Windows folder picker dialog will ask you to locate your game directory.
3. The port verifies `b/8b0b5899ed/game.wasm`, `data/manifest.json`, and shaders, saving your selection to `%APPDATA%\GTA5Port\settings.ini`.
4. The game launches with native Direct3D 11 rendering and WASAPI sound.

---

## 4. Controls & Shortcuts

- **Fullscreen:** `Alt+Enter` or `F11` toggles borderless fullscreen.
- **Mouse & Keyboard:**
  - `WASD`: Movement
  - `Mouse`: 360-degree look & aim
  - `Left Click`: Shoot / Attack
  - `Right Click`: Aim down sights
  - `Scroll Wheel`: Switch weapons
  - `Space`: Jump / Handbrake
  - `F`: Enter / Exit vehicle
  - `E`: Context interaction / Horn
  - `ESC`: Pause game & release mouse cursor
- **Xbox / XInput Controller:**
  - `Left Stick`: Movement
  - `Right Stick`: Camera look
  - `RT / LT`: Throttle & Brake (Vehicle) / Fire & Aim (Foot)
  - `A / B / X / Y`: Standard action buttons
  - `Start`: Pause menu
- **Self Radio:**
  - Place `.mp3`, `.wav`, or `.flac` audio files into `%APPDATA%\GTA5Port\Data\Self Radio`. With 3 or more tracks, Self Radio will automatically appear in your radio wheel.

---

## 5. Credits & Acknowledgements

- **Windows Native Port:** **[Srishant Kumar](https://github.com/srishantkumar)**
- **Original macOS / iOS Port:** [Kevin Erik](https://github.com/kevinerikjs/gta5-macos-port)
- **Original Browser / WASM Engine Exploration:** [TomerGamerTV](https://github.com/TomerGamerTV/playgta5)
- **Grand Theft Auto V and RAGE Engine:** © Rockstar Games & Take-Two Interactive Software, Inc. This project is an independent educational native port and is not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.

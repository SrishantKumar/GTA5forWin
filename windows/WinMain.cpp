#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <thread>
#include <chrono>
#include <string>
#include <cstdio>
#include <filesystem>

#include "WinGraphicsHost.h"
#include "WinInput.h"
#include "WinAudio.h"
#include "WinFiles.h"
#include "WinUI.h"
#include "WinNativeUI.h"
#include "../app/Source/EngineHost.h"

extern "C" void pg_log(const char *message);
extern "C" void PGSetSelectedGameLanguage(const char *lang);

namespace fs = std::filesystem;

static HWND gMainWindow = nullptr;
static bool gIsFullscreen = false;
static WINDOWPLACEMENT gPrevPlacement = { sizeof(gPrevPlacement) };

static void EngineLogCallback(const char *message, void *) {
    WinUI::Log(message, 3);
    if (message) {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
        static std::string logPath = (fs::path(WinFiles::GetAppDataFolder()) / "engine.log").string();
        FILE *f = fopen(logPath.c_str(), "a");
        if (f) {
            fprintf(f, "%s\n", message);
            fclose(f);
        }
    }
}

static void EnableDarkModeTitleBar(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
    #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
    #endif
    #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
    #define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
    #endif
    if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode)))) {
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &useDarkMode, sizeof(useDarkMode));
    }
}

static void ToggleFullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (!gIsFullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &gPrevPlacement) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            gIsFullscreen = true;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &gPrevPlacement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        gIsFullscreen = false;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // 1. Process message through Pure Native Direct2D UI layer first
    if (WinNativeUI::ProcessMessage(hwnd, uMsg, wParam, lParam)) {
        return 0;
    }

    // 2. Process message through ImGui UI layer (in-game overlay)
    if (WinUI::ProcessMessage(hwnd, uMsg, wParam, lParam)) {
        return 0;
    }

    // 3. Process message through Game Input layer
    if (WinInputProcessMessage(hwnd, uMsg, wParam, lParam)) {
        return 0;
    }

    switch (uMsg) {
        case WM_PAINT: {
            if (WinNativeUI::IsVisible()) {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                WinNativeUI::Render();
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                PGResizeD3D11Buffers(width, height);
                PGSetBackBufferSize(width, height);
                WinNativeUI::Resize(width, height);
            }
            return 0;
        }

        case WM_SYSKEYDOWN: {
            if (wParam == VK_RETURN && (lParam & (1 << 29))) { // Alt + Enter
                ToggleFullscreen(hwnd);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_F11) {
                ToggleFullscreen(hwnd);
                return 0;
            }
            if (wParam == VK_F1 && WinUI::IsGameRunning()) {
                WinUI::ToggleOverlay();
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            WinNativeUI::Shutdown();
            WinUI::Shutdown();
            WinAudioShutdown();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static void OnLaunchGame(const WinUIConfig& config) {
    WinUI::Log("Starting Grand Theft Auto V Engine initialization...");

    // Format graphics configuration options
    char optShader[64], optTex[64], optShadow[64], optRefl[64], optLod[64];
    snprintf(optShader, sizeof(optShader), "-shaderQuality=%d", config.shaderQuality);
    snprintf(optTex, sizeof(optTex), "-textureQuality=%d", config.textureQuality);
    snprintf(optShadow, sizeof(optShadow), "-shadowQuality=%d", config.shadowQuality);
    snprintf(optRefl, sizeof(optRefl), "-reflectionQuality=%d", config.reflectionQuality);
    snprintf(optLod, sizeof(optLod), "-lodScale=%.2f", config.lodScale);

    const char* qualityArgs[] = { optShader, optTex, optShadow, optRefl, optLod };
    int width = 1280;
    int height = 720;
    switch (config.resolutionPreset) {
        case 0: width = 1280; height = 720; break;
        case 1: width = 1600; height = 900; break;
        case 2: width = 1920; height = 1080; break;
        case 3: width = 2560; height = 1440; break;
    }

    PGConfigureEngine(qualityArgs, 5, width, height, config.frameLimit);

    if (config.memoryCapMB > 0) {
        PGSetMemoryCap(config.memoryCapMB);
    }

    PGSetStartMode(config.startMode);
    PGSetModsOff(!config.modsEnabled);
    PGSetSelfRadioMode(config.selfRadioMode);

    // Apply HUD layout
    PGSetMinimapLayout(config.minimapDX, config.minimapDY, config.minimapScale);
    PGSetHelpTextLayout(config.helpTextX, config.helpTextY, config.helpTextScale);
    PGSetHudPiece(0, config.wheelDX, config.wheelDY, config.wheelScale);
    PGSetHudPiece(1, config.phoneDX, config.phoneDY, config.phoneScale);
    PGSetHudPiece(2, config.feedDX, config.feedDY, config.feedScale);
    PGSetSubtitleRaise(config.subtitleRaise);

    // Set locale
    PGSetSelectedGameLanguage(WinUI::GetCurrentLanguageCode());

    // Configure game directories
    PGSetLocalGameFolder(config.gameFolder.c_str());

    std::string manifestPath = (fs::path(config.gameFolder) / "data" / "manifest.json").string();
    std::string documentsRoot = WinFiles::GetDocumentsFolder();

    // Prepare User Music if any
    PGUserMusicPrepare(WinFiles::GetUserMusicFolder().c_str(), documentsRoot.c_str());

    // Spawn Engine Host on background thread
    std::thread engineThread([manifestPath, documentsRoot]() {
        WinUI::Log("Engine host thread launched.");
        PGStartEngine(manifestPath.c_str(), documentsRoot.c_str(), EngineLogCallback, nullptr);
    });
    engineThread.detach();

    // D2D launcher is already shut down by WM_NATIVEUI_LAUNCH before we get here.
    // Flush the D3D11 swap chain so the first game frame replaces the launcher background.
    PGD3D11Present(0);

    WinUI::SetGameRunning(true);
    WinUI::SetOverlayOpen(false);

    // Lock cursor for game input after engine thread starts
    WinInputSetCursorLock(gMainWindow, true);

    WinUI::Log("Game launched. Press [F1] to toggle the in-game overlay, [ESC] to unlock cursor.");
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Enable Per-Monitor High-DPI Awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Register Window Class
    const wchar_t CLASS_NAME[] = L"GTA5PortWindowClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"GTA V Port Error", MB_ICONERROR);
        return 1;
    }

    // 16:9 High-DPI Widescreen Sizing (1600x900 DIPs)
    UINT dpi = GetDpiForSystem();
    float scale = (dpi > 0) ? ((float)dpi / 96.0f) : 1.0f;
    int targetClientW = (int)(1600.0f * scale);
    int targetClientH = (int)(900.0f * scale);

    // Clamp to 90% of primary monitor work area
    RECT workArea = {};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        int maxW = (workArea.right - workArea.left) * 9 / 10;
        int maxH = (workArea.bottom - workArea.top) * 9 / 10;
        if (targetClientW > maxW || targetClientH > maxH) {
            targetClientW = maxW;
            targetClientH = maxW * 9 / 16;
            if (targetClientH > maxH) {
                targetClientH = maxH;
                targetClientW = maxH * 16 / 9;
            }
        }
    }
    if (targetClientW < 1280) targetClientW = 1280;
    if (targetClientH < 720) targetClientH = 720;

    RECT rc = { 0, 0, targetClientW, targetClientH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    int winWidth = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;

    // Center window on primary monitor
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winWidth) / 2;
    int posY = (screenH - winHeight) / 2;
    if (posX < 0) posX = CW_USEDEFAULT;
    if (posY < 0) posY = CW_USEDEFAULT;

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Grand Theft Auto V — Windows Native Port (Direct3D 11) | Port by Srishant Kumar",
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        winWidth, winHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create main application window.", L"GTA V Port Error", MB_ICONERROR);
        return 1;
    }
    gMainWindow = hwnd;

    // Enable Sleek Dark Mode Title Bar on Windows 10/11
    EnableDarkModeTitleBar(hwnd);

    // Initialize Subsystems
    WinFiles::PrepareEnvironment(WinFiles::GetDocumentsFolder());
    WinInputInit(hwnd);
    WinAudioInit();
    PGAttachWindow(hwnd);
    WinAudioStart();

    // Initialize ImGui Native UI layer (fallback & in-game overlay)
    WinUI::Init(hwnd, PGGetD3D11Device(), PGGetD3D11Context());
    WinUI::SetLaunchCallback(OnLaunchGame);

    // Initialize Pure Native Direct2D / DirectWrite / WIC UI (1:1 Replica of macOS AppKit Launcher)
    bool nativeUiReady = WinNativeUI::Init(hwnd, [](const WinNativeLaunchArgs& args) {
        WinUIConfig config = WinUI::GetConfig();
        std::string savedFolder = WinFiles::LoadSavedGameFolder();
        if (!savedFolder.empty()) {
            config.gameFolder = savedFolder;
        }
        if (args.mode == "story") {
            config.startMode = 1;
        } else if (args.mode == "sandbox") {
            config.startMode = 2;
        } else if (args.mode == "performance") {
            config.startMode = 1;
            config.graphicsPreset = 0;
            config.frameLimit = 0;
        }
        config.graphicsPreset = args.preset;
        OnLaunchGame(config);
    });

    WinUI::Log("======================================================================");
    WinUI::Log("  Grand Theft Auto V — Windows Native Port (Direct3D 11)");
    WinUI::Log("  Port by Srishant Kumar (https://github.com/srishantkumar)");
    if (nativeUiReady) {
        WinUI::Log("  UI Engine: Direct2D / DirectWrite / WIC (Pure Native C++ 1:1 macOS Replica)");
    } else {
        WinUI::Log("  UI Engine: Direct3D 11 / ImGui Native Overlay");
    }
    WinUI::Log("======================================================================");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main Loop
    MSG msg = {};
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            if (WinNativeUI::IsVisible()) {
                // When Native Direct2D launcher is visible, render and yield lightly
                WinNativeUI::Render();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            } else if (!WinUI::IsGameRunning()) {
                // In Launcher Mode without Native UI: Render ImGui Launcher Frame
                WinUI::RenderLauncherFrame(hwnd, PGGetD3D11Context(), PGGetD3D11BackBufferRTV(), PGGetD3D11SwapChain());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                // In Game Mode: Poll input
                auto currentFrameTime = std::chrono::high_resolution_clock::now();
                double deltaSec = std::chrono::duration<double>(currentFrameTime - lastFrameTime).count();
                lastFrameTime = currentFrameTime;

                WinInputPoll(deltaSec);

                // Render in-game HUD or overlay if open
                if (WinUI::IsOverlayOpen()) {
                    WinUI::RenderInGameOverlay(hwnd, PGGetD3D11Context(), PGGetD3D11BackBufferRTV());
                }

                // Yield slightly
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }
    }

    WinUI::Shutdown();
    CoUninitialize();
    return (int)msg.wParam;
}

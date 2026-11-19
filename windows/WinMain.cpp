#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellscalingapi.h>
#include <chrono>
#include <string>
#include <cstdio>
#include <filesystem>

#include "WinGraphicsHost.h"
#include "WinInput.h"
#include "WinAudio.h"
#include "WinFiles.h"
#include "../app/Source/EngineHost.h"

namespace fs = std::filesystem;

static HWND gMainWindow = nullptr;
static bool gIsFullscreen = false;
static WINDOWPLACEMENT gPrevPlacement = { sizeof(gPrevPlacement) };

static void EngineLogCallback(const char *message, void *) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    printf("[GTA5 Engine] %s\n", message);
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
    if (WinInputProcessMessage(hwnd, uMsg, wParam, lParam)) {
        return 0;
    }

    switch (uMsg) {
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                PGResizeD3D11Buffers(width, height);
                PGSetBackBufferSize(width, height);
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
            break;
        }

        case WM_DESTROY: {
            WinAudioShutdown();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Enable Per-Monitor High-DPI Awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

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

    int defaultWidth = 1280;
    int defaultHeight = 720;

    RECT rc = { 0, 0, defaultWidth, defaultHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Grand Theft Auto V — Windows Native Port (Direct3D 11) | Port by Srishant Kumar (github.com/srishantkumar)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create main application window.", L"GTA V Port Error", MB_ICONERROR);
        return 1;
    }
    gMainWindow = hwnd;

    // Load or prompt for game folder
    std::string gameFolder = WinFiles::LoadSavedGameFolder();
    WinGameFolderReport report = WinFiles::VerifyGameFolder(gameFolder);

    if (!report.isValid) {
        std::string chosen;
        if (WinFiles::PromptChooseGameFolder(hwnd, chosen)) {
            report = WinFiles::VerifyGameFolder(chosen);
            if (report.isValid) {
                gameFolder = chosen;
                WinFiles::SaveGameFolder(gameFolder);
            } else {
                std::string err = "Selected folder is missing required game files:\n" + report.errorMessage;
                MessageBoxA(hwnd, err.c_str(), "GTA V Port — Incomplete Game Files", MB_ICONWARNING);
            }
        }
    }

    // Initialize Subsystems
    WinFiles::PrepareEnvironment(WinFiles::GetDocumentsFolder());
    WinInputInit(hwnd);
    WinAudioInit();
    PGAttachWindow(hwnd);
    WinAudioStart();

    pg_log("======================================================================");
    pg_log("  Grand Theft Auto V — Windows Native Port (Direct3D 11)");
    pg_log("  Port by Srishant Kumar (https://github.com/srishantkumar)");
    pg_log("======================================================================");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Configure and start engine
    if (report.isValid) {
        const char *quality[] = {
            "-shaderQuality=1", "-textureQuality=1", "-shadowQuality=1",
            "-reflectionQuality=1", "-lodScale=1.0"
        };
        PGConfigureEngine(quality, 5, defaultWidth, defaultHeight, 0); // 0 = unlocked frame rate
        PGSetStartMode(2); // 2 = Sandbox / Story launch
        PGSetLocalGameFolder(gameFolder.c_str());

        std::string manifestPath = (fs::path(gameFolder) / "data" / "manifest.json").string();
        std::string documentsRoot = WinFiles::GetDocumentsFolder();

        PGStartEngine(manifestPath.c_str(), documentsRoot.c_str(), EngineLogCallback, nullptr);
    } else {
        pg_log("Running in preflight mode without game files. Direct3D 11 and audio initialized.");
    }

    // Main Game Loop
    MSG msg = {};
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            auto currentFrameTime = std::chrono::high_resolution_clock::now();
            double deltaSec = std::chrono::duration<double>(currentFrameTime - lastFrameTime).count();
            lastFrameTime = currentFrameTime;

            WinInputPoll(deltaSec);

            // Yield slightly to prevent 100% spin if waiting on D3D11 present
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}

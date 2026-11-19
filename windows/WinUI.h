#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <functional>

struct WinUIConfig {
    std::string gameFolder;
    int startMode = 2;         // 1 = Story Mode, 2 = Sandbox Mode, 3 = Test Level
    int languageIndex = 0;     // 0 = en-US, 1 = fr-FR, 2 = de-DE, etc.

    // Graphics Settings
    int graphicsPreset = 2;    // 0 = Ultra Perf, 1 = Balanced, 2 = High, 3 = Ultra
    int resolutionPreset = 0;  // 0 = 1280x720, 1 = 1600x900, 2 = 1920x1080, 3 = 2560x1440, 4 = Native/Current
    int frameLimit = 0;        // 0 = Unlocked, 1 = 60 FPS, 2 = 30 FPS, 3 = 40 FPS
    bool vsync = true;
    bool fullscreen = false;
    int shaderQuality = 1;     // 0 = Normal, 1 = High, 2 = Very High
    int textureQuality = 1;    // 0 = Normal, 1 = High, 2 = Very High
    int shadowQuality = 1;     // 0 = Normal, 1 = High, 2 = Very High
    int reflectionQuality = 1; // 0 = Normal, 1 = High, 2 = Very High
    float lodScale = 1.0f;
    int memoryCapMB = 0;       // 0 = Auto

    // HUD Layout Adjustments
    float minimapScale = 1.0f;
    float minimapDX = 0.0f;
    float minimapDY = 0.0f;
    float helpTextScale = 1.0f;
    float helpTextX = 0.0f;
    float helpTextY = 0.0f;
    float wheelScale = 1.0f;
    float wheelDX = 0.0f;
    float wheelDY = 0.0f;
    float phoneScale = 1.0f;
    float phoneDX = 0.0f;
    float phoneDY = 0.0f;
    float feedScale = 1.0f;
    float feedDX = 0.0f;
    float feedDY = 0.0f;
    float subtitlesScale = 1.0f;
    float subtitleRaise = 0.0f;

    // Audio & Radio
    int masterVolume = 100;
    int sfxVolume = 100;
    int musicVolume = 100;
    int selfRadioMode = 0;     // 0 = Radio Station, 1 = Sequential, 2 = Shuffle

    // Mods & Scripts
    bool modsEnabled = true;

    // Persistent Cheats
    bool godMode = false;
    bool neverWanted = false;

    // Overlay & HUD
    bool showPerfHud = true;
    int perfHudPosition = 0;   // 0 = Top-Right, 1 = Top-Left
};

struct WinUILogEntry {
    std::string timestamp;
    std::string text;
    int level; // 0 = Info, 1 = Warning, 2 = Error, 3 = Engine
};

namespace WinUI {
    // Initialization & Lifecycle
    void Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    bool ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Frame rendering
    void RenderLauncherFrame(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv, IDXGISwapChain* swapChain);
    void RenderInGameOverlay(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);

    // Logging & Diagnostics
    void Log(const char* message, int level = 0);
    const std::vector<WinUILogEntry>& GetLogs();
    void ClearLogs();

    // State Accessors
    bool IsGameRunning();
    void SetGameRunning(bool running);
    bool IsOverlayOpen();
    void SetOverlayOpen(bool open);
    void ToggleOverlay();

    WinUIConfig& GetConfig();
    void SaveConfig();
    void LoadConfig();

    // Launch callback hook
    using LaunchCallback = std::function<void(const WinUIConfig&)>;
    void SetLaunchCallback(LaunchCallback cb);

    // Helper to query language code
    const char* GetCurrentLanguageCode();
}

// C-linkable hooks
extern "C" {
    void WinUILog(const char* message);
    void WinUIRenderInGameOverlay(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);
    bool WinUIIsGameRunning(void);
    bool WinUIIsOverlayOpen(void);
    void WinUISetOverlayOpen(bool open);
    void WinUIToggleOverlay(void);
}

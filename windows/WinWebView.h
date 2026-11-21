#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <functional>

struct WinWebViewLaunchArgs {
    std::string mode;
    int sandboxMap = 0;
    int preset = 4;
};

namespace WinWebView {
    using LaunchHandler = std::function<void(const WinWebViewLaunchArgs&)>;

    // Lifecycle
    bool Init(HWND hwnd, LaunchHandler onLaunch);
    void Shutdown();

    // Window Management
    void Resize(int width, int height);
    void SetVisible(bool visible);
    bool IsVisible();
    bool IsInitialized();

    // Data synchronization
    void SendFileStatus(bool ready, const std::string& folder, int missingCount, int corruptCount, const std::string& problemsJson);
    void SendSystemInfo(const std::string& gpu, const std::string& cpu, const std::string& ram);
    void RefreshStatus();
}

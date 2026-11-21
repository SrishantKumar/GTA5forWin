#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct WinNativeLaunchArgs {
    std::string mode;
    int sandboxMap = 0;
    int preset = 4;
};

namespace WinNativeUI {
    using LaunchHandler = std::function<void(const WinNativeLaunchArgs&)>;

    // Lifecycle
    bool Init(HWND hwnd, LaunchHandler onLaunch);
    void Shutdown();

    // Rendering & Frame Update
    void Render();
    bool ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void Resize(int width, int height);

    // Visibility & Overlay
    void SetVisible(bool visible);
    bool IsVisible();

    // Data Sync & Directory Scanning
    void RefreshStatus();
}

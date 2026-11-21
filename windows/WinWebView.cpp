#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "WinWebView.h"
#include "WinFiles.h"

#if __has_include(<WebView2.h>)
#include <WebView2.h>
#elif __has_include("third_party/webview2/build/native/include/WebView2.h")
#include "third_party/webview2/build/native/include/WebView2.h"
#endif

#pragma comment(lib, "shlwapi.lib")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

namespace {

// Function pointer typedef for CreateCoreWebView2EnvironmentWithOptions
typedef HRESULT(STDAPICALLTYPE *CreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions *environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environment_created_handler);

static HWND gHwnd = nullptr;
static WinWebView::LaunchHandler gLaunchHandler = nullptr;
static ComPtr<ICoreWebView2Environment> gEnv = nullptr;
static ComPtr<ICoreWebView2Controller> gController = nullptr;
static ComPtr<ICoreWebView2> gWebView = nullptr;
static HMODULE gWebViewLoaderLib = nullptr;
static bool gIsInitialized = false;
static bool gIsVisible = true;

// String conversion helpers
static std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], count);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

static std::string WideToUtf8(const std::wstring &wstr) {
    if (wstr.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

static std::string EscapeJsonString(const std::string &input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '\\') ss << "\\\\";
        else if (c == '"') ss << "\\\"";
        else if (c == '\n') ss << "\\n";
        else if (c == '\r') ss << "\\r";
        else if (c == '\t') ss << "\\t";
        else ss << c;
    }
    return ss.str();
}

static std::string FindJsonValue(const std::string &json, const std::string &key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.length()) return "";

    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    } else {
        size_t end = json.find_first_of(",}\n\r", pos);
        if (end == std::string::npos) end = json.length();
        return json.substr(pos, end - pos);
    }
}

// Find local launcher folder
static std::wstring GetLauncherFolderPath() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring exeDir = exePath;
    std::vector<std::wstring> candidates = {
        exeDir + L"\\launcher",
        exeDir + L"\\windows\\launcher",
        exeDir + L"\\..\\windows\\launcher",
        L"windows\\launcher",
        L".\\launcher"
    };

    for (const auto &cand : candidates) {
        std::wstring indexHtml = cand + L"\\index.html";
        DWORD attribs = GetFileAttributesW(indexHtml.c_str());
        if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            wchar_t fullPath[MAX_PATH] = {};
            GetFullPathNameW(cand.c_str(), MAX_PATH, fullPath, nullptr);
            return fullPath;
        }
    }
    return exeDir + L"\\launcher";
}

// System Hardware Diagnostics
static void GetHardwareInfo(std::string &gpu, std::string &cpu, std::string &ram) {
    gpu = "Direct3D 11 Hardware Graphics";
    cpu = "x86_64 Multi-Core Processor";
    ram = "16 GB RAM";

    // CPU from registry
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpuName[256] = {};
        DWORD size = sizeof(cpuName);
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, (LPBYTE)cpuName, &size) == ERROR_SUCCESS) {
            cpu = cpuName;
            // Trim leading spaces
            size_t first = cpu.find_first_not_of(" ");
            if (first != std::string::npos) cpu = cpu.substr(first);
        }
        RegCloseKey(hKey);
    }

    // RAM
    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (GlobalMemoryStatusEx(&mem)) {
        int totalGb = (int)((mem.ullTotalPhys + (1024 * 1024 * 1024 - 1)) / (1024 * 1024 * 1024));
        ram = std::to_string(totalGb) + " GB Physical RAM";
    }
}

static void PostMessageToJS(const std::string &type, const std::string &jsonPayload) {
    if (!gWebView) return;
    std::string script = "if (window.handleNativeMessage) { window.handleNativeMessage('" + type + "', " + jsonPayload + "); }";
    std::wstring wscript = Utf8ToWide(script);
    gWebView->ExecuteScript(wscript.c_str(), nullptr);
}

static void SendInitialData() {
    std::string gpu, cpu, ram;
    GetHardwareInfo(gpu, cpu, ram);
    WinWebView::SendSystemInfo(gpu, cpu, ram);
    WinWebView::RefreshStatus();
}

static void HandleIncomingMessage(const std::string &msg) {
    std::string type = FindJsonValue(msg, "type");
    if (type == "ready") {
        SendInitialData();
    } else if (type == "play") {
        std::string mode = FindJsonValue(msg, "mode");
        std::string mapStr = FindJsonValue(msg, "sandboxMap");
        std::string presetStr = FindJsonValue(msg, "preset");

        WinWebViewLaunchArgs args;
        args.mode = mode.empty() ? "story" : mode;
        args.sandboxMap = mapStr.empty() ? 0 : std::atoi(mapStr.c_str());
        args.preset = presetStr.empty() ? 4 : std::atoi(presetStr.c_str());

        WinWebView::SetVisible(false);
        if (gLaunchHandler) {
            gLaunchHandler(args);
        }
    } else if (type == "browse_folder") {
        std::string chosen;
        if (WinFiles::PromptChooseGameFolder(gHwnd, chosen)) {
            WinFiles::SaveGameFolder(chosen);
            WinWebView::RefreshStatus();
        }
    } else if (type == "verify_files") {
        WinWebView::RefreshStatus();
    } else if (type == "open_url") {
        std::string url = FindJsonValue(msg, "url");
        if (!url.empty()) {
            ShellExecuteW(nullptr, L"open", Utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } else if (type == "open_folder") {
        std::string folder = FindJsonValue(msg, "folder");
        if (folder == "saves") {
            WinFiles::OpenFolderInExplorer(WinFiles::GetDocumentsFolder());
        } else if (folder == "mods") {
            WinFiles::OpenFolderInExplorer(WinFiles::GetModsFolder());
        } else if (folder == "music") {
            WinFiles::OpenFolderInExplorer(WinFiles::GetUserMusicFolder());
        }
    }
}

} // namespace

namespace WinWebView {

bool Init(HWND hwnd, LaunchHandler onLaunch) {
    gHwnd = hwnd;
    gLaunchHandler = onLaunch;

    // 1. Locate and dynamically load WebView2Loader.dll
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring exeDir = exePath;

    std::vector<std::wstring> loaderCandidates = {
        exeDir + L"\\WebView2Loader.dll",
        exeDir + L"\\windows\\third_party\\webview2\\build\\native\\x64\\WebView2Loader.dll",
        L"WebView2Loader.dll"
    };

    for (const auto &path : loaderCandidates) {
        gWebViewLoaderLib = LoadLibraryW(path.c_str());
        if (gWebViewLoaderLib) break;
    }

    if (!gWebViewLoaderLib) {
        OutputDebugStringA("[WinWebView] WebView2Loader.dll not found; WebView2 UI unavailable.\n");
        return false;
    }

    auto pfnCreateEnv = (CreateCoreWebView2EnvironmentWithOptionsFn)GetProcAddress(
        gWebViewLoaderLib, "CreateCoreWebView2EnvironmentWithOptions");

    if (!pfnCreateEnv) {
        OutputDebugStringA("[WinWebView] CreateCoreWebView2EnvironmentWithOptions symbol missing.\n");
        return false;
    }

    // 2. Set user data directory in %LOCALAPPDATA%\GTA5forWin\WebView2Data
    std::string localApp = WinFiles::GetLocalAppDataFolder();
    std::wstring userDataFolder = Utf8ToWide(localApp) + L"\\GTA5forWin\\WebView2Data";
    CreateDirectoryW((Utf8ToWide(localApp) + L"\\GTA5forWin").c_str(), nullptr);
    CreateDirectoryW(userDataFolder.c_str(), nullptr);

    // 3. Create Environment
    HRESULT hr = pfnCreateEnv(
        nullptr,
        userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                if (FAILED(result) || !env) {
                    OutputDebugStringA("[WinWebView] Failed to create WebView2 environment.\n");
                    return S_OK;
                }
                gEnv = env;

                // Create Controller
                gEnv->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT res, ICoreWebView2Controller *controller) -> HRESULT {
                            if (FAILED(res) || !controller) {
                                OutputDebugStringA("[WinWebView] Failed to create WebView2 controller.\n");
                                return S_OK;
                            }
                            gController = controller;
                            gController->get_CoreWebView2(&gWebView);

                            if (gWebView) {
                                // Match window bounds
                                RECT bounds = {};
                                GetClientRect(hwnd, &bounds);
                                gController->put_Bounds(bounds);
                                gController->put_IsVisible(TRUE);

                                // Settings
                                ComPtr<ICoreWebView2Settings> settings;
                                gWebView->get_Settings(&settings);
                                if (settings) {
                                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                                    settings->put_AreDevToolsEnabled(TRUE);
                                    settings->put_IsStatusBarEnabled(FALSE);
                                }

                                // Message receiver
                                gWebView->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                                            LPWSTR rawMsg = nullptr;
                                            if (SUCCEEDED(args->TryGetWebMessageAsString(&rawMsg)) && rawMsg) {
                                                std::string msgUtf8 = WideToUtf8(rawMsg);
                                                CoTaskMemFree(rawMsg);
                                                HandleIncomingMessage(msgUtf8);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    nullptr);

                                // Map launcher folder to virtual host name
                                std::wstring launcherFolder = GetLauncherFolderPath();
                                ComPtr<ICoreWebView2_3> webView3;
                                if (SUCCEEDED(gWebView.As(&webView3))) {
                                    webView3->SetVirtualHostNameToFolderMapping(
                                        L"gta5launcher.local",
                                        launcherFolder.c_str(),
                                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                    gWebView->Navigate(L"https://gta5launcher.local/index.html");
                                } else {
                                    std::wstring fileUrl = L"file:///" + launcherFolder + L"\\index.html";
                                    std::replace(fileUrl.begin(), fileUrl.end(), L'\\', L'/');
                                    gWebView->Navigate(fileUrl.c_str());
                                }

                                gIsInitialized = true;
                                gIsVisible = true;
                                OutputDebugStringA("[WinWebView] WebView2 Launcher initialized successfully!\n");
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    return SUCCEEDED(hr);
}

void Shutdown() {
    if (gController) {
        gController->Close();
        gController = nullptr;
    }
    gWebView = nullptr;
    gEnv = nullptr;
    if (gWebViewLoaderLib) {
        FreeLibrary(gWebViewLoaderLib);
        gWebViewLoaderLib = nullptr;
    }
    gIsInitialized = false;
}

void Resize(int width, int height) {
    if (gController) {
        RECT bounds = { 0, 0, width, height };
        gController->put_Bounds(bounds);
    }
}

void SetVisible(bool visible) {
    gIsVisible = visible;
    if (gController) {
        gController->put_IsVisible(visible ? TRUE : FALSE);
    }
}

bool IsVisible() {
    return gIsVisible && gController != nullptr;
}

bool IsInitialized() {
    return gIsInitialized;
}

void SendFileStatus(bool ready, const std::string &folder, int missingCount, int corruptCount, const std::string &problemsJson) {
    std::ostringstream ss;
    ss << "{"
       << "\"ready\":" << (ready ? "true" : "false") << ","
       << "\"folder\":\"" << EscapeJsonString(folder) << "\","
       << "\"missingCount\":" << missingCount << ","
       << "\"corruptCount\":" << corruptCount << ","
       << "\"problems\":" << (problemsJson.empty() ? "[]" : problemsJson)
       << "}";
    PostMessageToJS("file_status", ss.str());
}

void SendSystemInfo(const std::string &gpu, const std::string &cpu, const std::string &ram) {
    std::ostringstream ss;
    ss << "{"
       << "\"gpu\":\"" << EscapeJsonString(gpu) << "\","
       << "\"cpu\":\"" << EscapeJsonString(cpu) << "\","
       << "\"ram\":\"" << EscapeJsonString(ram) << "\""
       << "}";
    PostMessageToJS("sys_info", ss.str());
}

void RefreshStatus() {
    std::string folder = WinFiles::LoadSavedGameFolder();
    if (folder.empty()) {
        folder = WinFiles::AutoDetectGameFolder();
        if (!folder.empty()) {
            WinFiles::SaveGameFolder(folder);
        }
    }

    WinGameFolderReport rep = WinFiles::VerifyGameFolder(folder);

    std::ostringstream probJson;
    probJson << "[";
    bool first = true;
    for (const auto &item : rep.missingItems) {
        if (!first) probJson << ",";
        first = false;
        probJson << "{\"path\":\"" << EscapeJsonString(item) << "\",\"reason\":\"Missing archive or file\"}";
    }
    probJson << "]";

    int missing = (int)rep.missingItems.size();
    SendFileStatus(rep.isValid, folder, missing, 0, probJson.str());
}

} // namespace WinWebView

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WinUI.h"
#include "WinFiles.h"
#include "WinGraphicsHost.h"
#include "WinInput.h"
#include "../app/Source/EngineHost.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <xinput.h>

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

// External Win32 message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// External language setter from wasm-rt-stubs.c
extern "C" void PGSetSelectedGameLanguage(const char *lang);

namespace {

// UI State
static bool gInitialized = false;
static bool gIsGameRunning = false;
static bool gIsOverlayOpen = false;
static int gActiveTab = 0; // 0 = Play, 1 = Graphics, 2 = Controls/HUD, 3 = Mods, 4 = Audio, 5 = Cheats/Spawn, 6 = Diagnostics
static WinUIConfig gConfig;
static WinUI::LaunchCallback gLaunchCallback;

// Direct3D 11 Adapter Description Cache
static std::string gGpuName = "Direct3D 11 Hardware Graphics Device";
static std::string gGpuDriverLevel = "Direct3D 11.1";

// Thread-safe Logging
static std::mutex gLogMutex;
static std::vector<WinUILogEntry> gLogs;
static std::ofstream gLogFile;

// Game folder verification cache
static WinGameFolderReport gCachedReport;
static bool gReportNeedsRefresh = true;

// Language metadata
static const char* kLanguageCodes[] = {
    "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "ja-JP", "ko-KR",
    "zh-TW", "zh-CN", "ru-RU", "pl-PL", "pt-BR", "es-MX"
};

static const char* kLanguageNames[] = {
    "English (US)", "Français (French)", "Deutsch (German)", "Italiano (Italian)",
    "Español (Spanish)", "日本語 (Japanese)", "한국어 (Korean)", "繁體中文 (Traditional Chinese)",
    "简体中文 (Simplified Chinese)", "Русский (Russian)", "Polski (Polish)",
    "Português do Brasil (Portuguese)", "Español de México (Mexican Spanish)"
};

// Curated GTA V Vehicle Presets (available immediately for launcher & in-game)
struct VehiclePreset {
    const char* category;
    const char* name;
    const char* model;
    uint32_t modelHash; // JoAAT hash
};

static const VehiclePreset kVehiclePresets[] = {
    // Supercars
    { "Super", "Truffade Adder", "adder", 0xB779A091 },
    { "Super", "Pegassi Zentorno", "zentorno", 0x98EF77E0 },
    { "Super", "Pegassi Infernus", "infernus", 0x47BFF561 },
    { "Super", "Grotti Turismo R", "turismor", 0x3DE9F564 },
    { "Super", "Grotti Cheetah", "cheetah", 0x8C1B79E7 },
    { "Super", "Overflod Entity XF", "entityxf", 0x1B06D672 },
    { "Super", "Pegassi Osiris", "osiris", 0x5D8408B0 },
    { "Super", "Progen T20", "t20", 0x6322DC75 },
    // Sports
    { "Sports", "Karin Kuruma (Armored)", "kuruma2", 0x0CD146B8 },
    { "Sports", "Elegy RH8", "elegy2", 0x80854423 },
    { "Sports", "Benefactor Feltzer", "feltzer2", 0x65F09B05 },
    { "Sports", "Annis Elegy Retro", "elegy", 0x933099C7 },
    { "Sports", "Dinka Jester", "jester", 0x76882C74 },
    { "Sports", "Bravado Banshee", "banshee", 0x9EC05C1E },
    // Muscle
    { "Muscle", "Bravado Gauntlet", "gauntlet", 0x992B9217 },
    { "Muscle", "Vapid Dominator", "dominator", 0x367F688B },
    { "Muscle", "Imponte Duke O'Death", "dukes2", 0x7292AA9C },
    { "Muscle", "Declasse Sabre Turbo", "sabregt", 0x01198A44 },
    // Motorcycles & Bikes
    { "Bikes", "Pegassi Bati 801", "bati", 0xF31D082A },
    { "Bikes", "Dinka Akuma", "akuma", 0x2A37424C },
    { "Bikes", "Shitzu Hakuchou", "hakuchou", 0xF7244BF5 },
    { "Bikes", "Western Sanchez (Dirt)", "sanchez", 0xA74DF84E },
    { "Bikes", "Pegassi Oppressor", "oppressor", 0x487A2946 },
    // Helicopters & Planes
    { "Aircraft", "Nagasaki Buzzard Attack", "buzzard", 0x2F035640 },
    { "Aircraft", "Western Maverick", "maverick", 0x9D0450DE },
    { "Aircraft", "Cargobob", "cargobob", 0xFCFCB68B },
    { "Aircraft", "P-996 LAZER (Fighter Jet)", "lazer", 0xB39B0AE6 },
    { "Aircraft", "Mammoth Hydra (VTOL Jet)", "hydra", 0x39D6E83F },
    { "Aircraft", "Mallard (Stunt Plane)", "stunt", 0xE7B2D2B0 },
    { "Aircraft", "Mammoth Titan (Cargo)", "titan", 0x76DA243A },
    // Off-Road & SUVs
    { "Off-Road", "HVY Insurgent", "insurgent", 0x8A79B3D3 },
    { "Off-Road", "Karin Rebel", "rebel", 0x431C2D3A },
    { "Off-Road", "BF Bifta", "bifta", 0x7A9D6998 },
    { "Off-Road", "Canis Mesa (Merryweather)", "mesa3", 0xD6A55EC7 },
    // Military & Emergency
    { "Emergency", "Rhino Tank", "rhino", 0x2DA22C37 },
    { "Emergency", "Police Cruiser", "police", 0x79FDE6C9 },
    { "Emergency", "Police Buffalo (FIB)", "fbi2", 0x66B011DE },
    { "Emergency", "Ambulance", "ambulance", 0x45D56CAD },
    { "Emergency", "Fire Truck", "firetruk", 0x73920F8E },
    // Boats
    { "Boats", "Shitzu Tropic", "tropic", 0x16464522 },
    { "Boats", "Pegassi Speeder", "speeder", 0xCE77A5B9 },
    { "Boats", "Nagasaki Dinghy", "dinghy", 0x107F392C },
    { "Boats", "Submersible", "submersible", 0x2B1A6EE3 }
};

static void ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Dimensions & Rounding
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(12, 7);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 13.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // Palette: Sleek Dark Slate + Rockstar Gold/Amber (#F5A623)
    const ImVec4 cGold       = ImVec4(0.96f, 0.65f, 0.14f, 1.00f);
    const ImVec4 cGoldHover  = ImVec4(1.00f, 0.74f, 0.28f, 1.00f);
    const ImVec4 cGoldActive = ImVec4(0.85f, 0.55f, 0.08f, 1.00f);
    const ImVec4 cBgDark     = ImVec4(0.07f, 0.08f, 0.10f, 0.98f);
    const ImVec4 cBgCard     = ImVec4(0.12f, 0.13f, 0.17f, 0.70f);
    const ImVec4 cBorder     = ImVec4(0.24f, 0.27f, 0.35f, 0.50f);
    const ImVec4 cTextWhite  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    const ImVec4 cTextMuted  = ImVec4(0.60f, 0.64f, 0.72f, 1.00f);

    colors[ImGuiCol_Text]                  = cTextWhite;
    colors[ImGuiCol_TextDisabled]          = cTextMuted;
    colors[ImGuiCol_WindowBg]              = cBgDark;
    colors[ImGuiCol_ChildBg]               = cBgCard;
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.11f, 0.14f, 0.98f);
    colors[ImGuiCol_Border]                = cBorder;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.23f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.29f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.11f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.11f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.25f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.39f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = cGold;
    colors[ImGuiCol_CheckMark]             = cGold;
    colors[ImGuiCol_SliderGrab]            = cGold;
    colors[ImGuiCol_SliderGrabActive]      = cGoldActive;
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.25f, 0.28f, 0.37f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = cGold;
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.21f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.28f, 0.37f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = cGold;
    colors[ImGuiCol_Separator]             = cBorder;
    colors[ImGuiCol_SeparatorHovered]      = cGoldHover;
    colors[ImGuiCol_SeparatorActive]       = cGoldActive;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.28f, 0.32f, 0.40f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = cGoldHover;
    colors[ImGuiCol_ResizeGripActive]      = cGold;
    colors[ImGuiCol_Tab]                   = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.25f, 0.33f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.23f, 0.31f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_PlotLines]             = cGold;
    colors[ImGuiCol_PlotLinesHovered]      = cGoldHover;
    colors[ImGuiCol_PlotHistogram]         = cGold;
    colors[ImGuiCol_PlotHistogramHovered]  = cGoldHover;
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.14f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = cBorder;
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.23f, 0.30f, 0.40f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.96f, 0.65f, 0.14f, 0.35f);
    colors[ImGuiCol_NavHighlight]          = cGold;
}

static void CacheGpuInfo(ID3D11Device* device) {
    if (!device) return;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf()))) {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                char buffer[256] = {};
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buffer, sizeof(buffer), nullptr, nullptr);
                gGpuName = buffer;
            }
        }
    }
}

} // anonymous namespace

namespace WinUI {

void Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (gInitialized) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // We use our own config.ini persistence

    ApplyDarkTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);

    CacheGpuInfo(device);

    // Initialize Log File next to executable and in AppData
    std::string logPath = "GTA5Port.log";
    gLogFile.open(logPath, std::ios::out | std::ios::app);

    // Load persisted settings
    LoadConfig();

    // If game folder was empty, try auto-detection
    if (gConfig.gameFolder.empty()) {
        std::string detected = WinFiles::AutoDetectGameFolder();
        if (!detected.empty()) {
            gConfig.gameFolder = detected;
            gReportNeedsRefresh = true;
        }
    }

    gInitialized = true;
    Log("Five Star Windows Direct3D 11 Port UI Initialized.");
}

void Shutdown() {
    if (!gInitialized) return;
    SaveConfig();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (gLogFile.is_open()) {
        gLogFile.close();
    }
    gInitialized = false;
}

bool ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!gInitialized) return false;

    // Give ImGui Win32 backend first crack at messages
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) {
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (!gIsGameRunning || gIsOverlayOpen) {
        // In launcher mode or when in-game overlay is open, ImGui captures mouse & keyboard
        if (io.WantCaptureMouse) {
            switch (uMsg) {
                case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                case WM_MOUSEWHEEL:  case WM_MOUSEMOVE:
                    return true;
            }
        }
        if (io.WantCaptureKeyboard) {
            switch (uMsg) {
                case WM_KEYDOWN: case WM_KEYUP:
                case WM_CHAR:    case WM_SYSKEYDOWN:
                    return true;
            }
        }
    }
    return false;
}

void Log(const char* message, int level) {
    if (!message || !*message) return;

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

    WinUILogEntry entry;
    entry.timestamp = timeBuf;
    entry.text = message;
    entry.level = level;

    {
        std::lock_guard<std::mutex> lock(gLogMutex);
        if (gLogs.size() > 2500) {
            gLogs.erase(gLogs.begin(), gLogs.begin() + 500);
        }
        gLogs.push_back(entry);

        if (gLogFile.is_open()) {
            gLogFile << "[" << timeBuf << "] " << message << std::endl;
            gLogFile.flush();
        }
    }

    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

const std::vector<WinUILogEntry>& GetLogs() {
    return gLogs;
}

void ClearLogs() {
    std::lock_guard<std::mutex> lock(gLogMutex);
    gLogs.clear();
}

bool IsGameRunning() { return gIsGameRunning; }
void SetGameRunning(bool running) { gIsGameRunning = running; }
bool IsOverlayOpen() { return gIsOverlayOpen; }
void SetOverlayOpen(bool open) { gIsOverlayOpen = open; }
void ToggleOverlay() { gIsOverlayOpen = !gIsOverlayOpen; }

WinUIConfig& GetConfig() { return gConfig; }

void SaveConfig() {
    std::string path = WinFiles::GetConfigFilePath();
    fs::create_directories(fs::path(path).parent_path());

    auto writeStr = [&](const char* sec, const char* key, const std::string& val) {
        WritePrivateProfileStringA(sec, key, val.c_str(), path.c_str());
    };
    auto writeInt = [&](const char* sec, const char* key, int val) {
        WritePrivateProfileStringA(sec, key, std::to_string(val).c_str(), path.c_str());
    };
    auto writeFloat = [&](const char* sec, const char* key, float val) {
        char buf[32]; snprintf(buf, sizeof(buf), "%.2f", val);
        WritePrivateProfileStringA(sec, key, buf, path.c_str());
    };

    writeStr("Launcher", "GameFolder", gConfig.gameFolder);
    writeInt("Launcher", "StartMode", gConfig.startMode);
    writeInt("Launcher", "LanguageIndex", gConfig.languageIndex);

    writeInt("Graphics", "Preset", gConfig.graphicsPreset);
    writeInt("Graphics", "ResolutionPreset", gConfig.resolutionPreset);
    writeInt("Graphics", "FPSCap", gConfig.frameLimit);
    writeInt("Graphics", "VSync", gConfig.vsync ? 1 : 0);
    writeInt("Graphics", "Fullscreen", gConfig.fullscreen ? 1 : 0);
    writeInt("Graphics", "ShaderQuality", gConfig.shaderQuality);
    writeInt("Graphics", "TextureQuality", gConfig.textureQuality);
    writeInt("Graphics", "ShadowQuality", gConfig.shadowQuality);
    writeInt("Graphics", "ReflectionQuality", gConfig.reflectionQuality);
    writeFloat("Graphics", "LodScale", gConfig.lodScale);
    writeInt("Graphics", "MemoryCapMB", gConfig.memoryCapMB);

    writeFloat("HUD", "MinimapScale", gConfig.minimapScale);
    writeFloat("HUD", "MinimapDX", gConfig.minimapDX);
    writeFloat("HUD", "MinimapDY", gConfig.minimapDY);
    writeFloat("HUD", "HelpTextScale", gConfig.helpTextScale);
    writeFloat("HUD", "HelpTextX", gConfig.helpTextX);
    writeFloat("HUD", "HelpTextY", gConfig.helpTextY);
    writeFloat("HUD", "WheelScale", gConfig.wheelScale);
    writeFloat("HUD", "PhoneScale", gConfig.phoneScale);
    writeFloat("HUD", "FeedScale", gConfig.feedScale);
    writeFloat("HUD", "SubtitlesScale", gConfig.subtitlesScale);
    writeFloat("HUD", "SubtitleRaise", gConfig.subtitleRaise);

    writeInt("Audio", "MasterVolume", gConfig.masterVolume);
    writeInt("Audio", "SFXVolume", gConfig.sfxVolume);
    writeInt("Audio", "MusicVolume", gConfig.musicVolume);
    writeInt("Audio", "SelfRadioMode", gConfig.selfRadioMode);

    writeInt("Mods", "Enabled", gConfig.modsEnabled ? 1 : 0);
    writeInt("Cheats", "GodMode", gConfig.godMode ? 1 : 0);
    writeInt("Cheats", "NeverWanted", gConfig.neverWanted ? 1 : 0);

    writeInt("UI", "ShowPerfHud", gConfig.showPerfHud ? 1 : 0);
    writeInt("UI", "PerfHudPos", gConfig.perfHudPosition);
}

void LoadConfig() {
    std::string path = WinFiles::GetConfigFilePath();
    if (!fs::exists(path)) {
        // Fallback check in AppData Roaming
        path = fs::path(WinFiles::GetAppDataFolder()) / "settings.ini";
    }

    char buf[MAX_PATH] = {};
    GetPrivateProfileStringA("Launcher", "GameFolder", "", buf, MAX_PATH, path.c_str());
    if (buf[0]) gConfig.gameFolder = buf;

    gConfig.startMode = GetPrivateProfileIntA("Launcher", "StartMode", gConfig.startMode, path.c_str());
    gConfig.languageIndex = GetPrivateProfileIntA("Launcher", "LanguageIndex", gConfig.languageIndex, path.c_str());

    gConfig.graphicsPreset = GetPrivateProfileIntA("Graphics", "Preset", gConfig.graphicsPreset, path.c_str());
    gConfig.resolutionPreset = GetPrivateProfileIntA("Graphics", "ResolutionPreset", gConfig.resolutionPreset, path.c_str());
    gConfig.frameLimit = GetPrivateProfileIntA("Graphics", "FPSCap", gConfig.frameLimit, path.c_str());
    gConfig.vsync = GetPrivateProfileIntA("Graphics", "VSync", 1, path.c_str()) != 0;
    gConfig.fullscreen = GetPrivateProfileIntA("Graphics", "Fullscreen", 0, path.c_str()) != 0;
    gConfig.shaderQuality = GetPrivateProfileIntA("Graphics", "ShaderQuality", gConfig.shaderQuality, path.c_str());
    gConfig.textureQuality = GetPrivateProfileIntA("Graphics", "TextureQuality", gConfig.textureQuality, path.c_str());
    gConfig.shadowQuality = GetPrivateProfileIntA("Graphics", "ShadowQuality", gConfig.shadowQuality, path.c_str());
    gConfig.reflectionQuality = GetPrivateProfileIntA("Graphics", "ReflectionQuality", gConfig.reflectionQuality, path.c_str());
    gConfig.memoryCapMB = GetPrivateProfileIntA("Graphics", "MemoryCapMB", gConfig.memoryCapMB, path.c_str());

    gConfig.masterVolume = GetPrivateProfileIntA("Audio", "MasterVolume", gConfig.masterVolume, path.c_str());
    gConfig.sfxVolume = GetPrivateProfileIntA("Audio", "SFXVolume", gConfig.sfxVolume, path.c_str());
    gConfig.musicVolume = GetPrivateProfileIntA("Audio", "MusicVolume", gConfig.musicVolume, path.c_str());
    gConfig.selfRadioMode = GetPrivateProfileIntA("Audio", "SelfRadioMode", gConfig.selfRadioMode, path.c_str());

    gConfig.modsEnabled = GetPrivateProfileIntA("Mods", "Enabled", 1, path.c_str()) != 0;
    gConfig.godMode = GetPrivateProfileIntA("Cheats", "GodMode", 0, path.c_str()) != 0;
    gConfig.neverWanted = GetPrivateProfileIntA("Cheats", "NeverWanted", 0, path.c_str()) != 0;

    gConfig.showPerfHud = GetPrivateProfileIntA("UI", "ShowPerfHud", 1, path.c_str()) != 0;
    gConfig.perfHudPosition = GetPrivateProfileIntA("UI", "PerfHudPos", 0, path.c_str());

    // Sync selected language to engine stub
    if (gConfig.languageIndex >= 0 && gConfig.languageIndex < 13) {
        PGSetSelectedGameLanguage(kLanguageCodes[gConfig.languageIndex]);
    }
}

void SetLaunchCallback(LaunchCallback cb) {
    gLaunchCallback = cb;
}

const char* GetCurrentLanguageCode() {
    if (gConfig.languageIndex >= 0 && gConfig.languageIndex < 13) {
        return kLanguageCodes[gConfig.languageIndex];
    }
    return "en-US";
}

// -------------------------------------------------------------------------------------------------
// UI Tab Renderers
// -------------------------------------------------------------------------------------------------

static void RenderPlayTab(HWND hwnd) {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ GAME LAUNCH & ASSET VERIFICATION");
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Folder Selection Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("FolderCard", ImVec2(0, 165), true);
    {
        ImGui::TextUnformatted("Grand Theft Auto V Game Directory:");
        char folderBuf[MAX_PATH];
        strncpy(folderBuf, gConfig.gameFolder.c_str(), sizeof(folderBuf) - 1);
        folderBuf[sizeof(folderBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 360);
        if (ImGui::InputText("##GameFolderPath", folderBuf, sizeof(folderBuf))) {
            gConfig.gameFolder = folderBuf;
            gReportNeedsRefresh = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Browse Folder...", ImVec2(110, 0))) {
            std::string chosen;
            if (WinFiles::PromptChooseGameFolder(hwnd, chosen)) {
                gConfig.gameFolder = chosen;
                gReportNeedsRefresh = true;
                WinFiles::SaveGameFolder(chosen);
                SaveConfig();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Auto-Detect", ImVec2(100, 0))) {
            std::string detected = WinFiles::AutoDetectGameFolder();
            if (!detected.empty()) {
                gConfig.gameFolder = detected;
                gReportNeedsRefresh = true;
                WinFiles::SaveGameFolder(detected);
                SaveConfig();
                Log("Auto-detected GTA V game folder.");
            } else {
                Log("Auto-detect: No standard Steam/Epic/Rockstar GTA V installation found.", 1);
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Open Folder", ImVec2(100, 0))) {
            WinFiles::OpenFolderInExplorer(gConfig.gameFolder);
        }

        ImGui::Spacing();

        // Refresh verification if needed
        if (gReportNeedsRefresh) {
            gCachedReport = WinFiles::VerifyGameFolder(gConfig.gameFolder);
            gReportNeedsRefresh = false;
        }

        // Verification Badges
        if (gCachedReport.isValid) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "[OK] Game Files Verified and Valid!");
            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Found: %s (RPF Archives: %d)",
                gCachedReport.folderPath.c_str(), gCachedReport.foundRpfCount);
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.2f, 1.0f), "[!] Game Directory Incomplete or Missing Assets");
            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f),
                "Please select your legitimate Grand Theft Auto V folder (Steam, Epic, or Rockstar Games).");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // 2. Launch Options Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("OptionsCard", ImVec2(0, 150), true);
    {
        ImGui::Columns(2, "LaunchOptionsCols", false);
        ImGui::SetColumnWidth(0, 360);

        ImGui::TextUnformatted("Launch Mode:");
        if (ImGui::RadioButton("Sandbox Mode (Free Roam Los Santos)", gConfig.startMode == 2)) {
            gConfig.startMode = 2;
        }
        if (ImGui::RadioButton("Story Mode (Prologue / Newest Save)", gConfig.startMode == 1)) {
            gConfig.startMode = 1;
        }
        if (ImGui::RadioButton("Developer Test Level (env_test)", gConfig.startMode == 3)) {
            gConfig.startMode = 3;
        }

        ImGui::NextColumn();

        ImGui::TextUnformatted("Game Language:");
        ImGui::SetNextItemWidth(260);
        if (ImGui::Combo("##LanguageCombo", &gConfig.languageIndex, kLanguageNames, IM_ARRAYSIZE(kLanguageNames))) {
            PGSetSelectedGameLanguage(kLanguageCodes[gConfig.languageIndex]);
            SaveConfig();
        }

        ImGui::Spacing();
        ImGui::Checkbox("Enable Script Mods & Add-ons (.asi)", &gConfig.modsEnabled);
        ImGui::Checkbox("Show In-Game Performance HUD (FPS / Frame Time)", &gConfig.showPerfHud);

        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Spacing();

    // 3. Huge Launch Action Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.65f, 0.14f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.74f, 0.28f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.55f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.06f, 1.00f));

    std::string launchLabel = gCachedReport.isValid
        ? "▶  LAUNCH GRAND THEFT AUTO V  (DIRECT3D 11)"
        : "▶  LAUNCH GTA V  (PREFLIGHT DIAGNOSTIC MODE)";

    if (ImGui::Button(launchLabel.c_str(), ImVec2(-1, 56))) {
        SaveConfig();
        if (gLaunchCallback) {
            gLaunchCallback(gConfig);
        }
    }

    ImGui::PopStyleColor(4);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f),
        "Tip: In-game press [F1] or [Insert] to toggle overlay settings, cheats, vehicle spawner, and live logs.");
}

static void RenderGraphicsTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ GRAPHICS & DISPLAY SETTINGS");
    ImGui::Separator();
    ImGui::Spacing();

    // Presets
    ImGui::TextUnformatted("Quality Preset:");
    const char* presets[] = { "Ultra Performance (Low)", "Balanced (Medium)", "High Quality", "Ultra / Max Detail" };
    if (ImGui::Combo("##PresetCombo", &gConfig.graphicsPreset, presets, IM_ARRAYSIZE(presets))) {
        switch (gConfig.graphicsPreset) {
            case 0: // Ultra Perf
                gConfig.shaderQuality = 0; gConfig.textureQuality = 0;
                gConfig.shadowQuality = 0; gConfig.reflectionQuality = 0;
                gConfig.lodScale = 0.8f;
                break;
            case 1: // Balanced
                gConfig.shaderQuality = 1; gConfig.textureQuality = 1;
                gConfig.shadowQuality = 1; gConfig.reflectionQuality = 1;
                gConfig.lodScale = 1.0f;
                break;
            case 2: // High
                gConfig.shaderQuality = 2; gConfig.textureQuality = 2;
                gConfig.shadowQuality = 1; gConfig.reflectionQuality = 1;
                gConfig.lodScale = 1.2f;
                break;
            case 3: // Ultra
                gConfig.shaderQuality = 2; gConfig.textureQuality = 2;
                gConfig.shadowQuality = 2; gConfig.reflectionQuality = 2;
                gConfig.lodScale = 1.5f;
                break;
        }
        SaveConfig();
    }

    ImGui::Spacing();

    // Display & Framing
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("GraphicsSub", ImVec2(0, 320), true);
    {
        ImGui::Columns(2, "GfxCols", false);

        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Display & Window");
        ImGui::Spacing();

        const char* resOptions[] = { "1280 × 720 (HD)", "1600 × 900", "1920 × 1080 (FHD)", "2560 × 1440 (2K)", "Native / Current Window" };
        ImGui::TextUnformatted("Resolution Scale:");
        ImGui::SetNextItemWidth(200);
        ImGui::Combo("##ResScale", &gConfig.resolutionPreset, resOptions, IM_ARRAYSIZE(resOptions));

        const char* fpsCaps[] = { "Unlocked (Maximum)", "60 FPS (Standard)", "30 FPS", "40 FPS (OLED / Handheld)" };
        ImGui::TextUnformatted("Frame Rate Limit:");
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("##FPSCap", &gConfig.frameLimit, fpsCaps, IM_ARRAYSIZE(fpsCaps))) {
            PGSetFrameLimit(gConfig.frameLimit);
        }

        ImGui::Checkbox("Vertical Sync (VSync)", &gConfig.vsync);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            PGSetPresentInterval(gConfig.vsync ? 0.0166 : 0.0);
        }

        ImGui::Checkbox("Borderless Fullscreen", &gConfig.fullscreen);
        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "Shortcut: Alt + Enter or F11");

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Detail & Engine Memory");
        ImGui::Spacing();

        const char* qLevels[] = { "Normal", "High", "Very High" };
        ImGui::TextUnformatted("Shader Quality:");
        ImGui::SetNextItemWidth(180);
        ImGui::Combo("##ShaderQ", &gConfig.shaderQuality, qLevels, IM_ARRAYSIZE(qLevels));

        ImGui::TextUnformatted("Texture Quality:");
        ImGui::SetNextItemWidth(180);
        ImGui::Combo("##TexQ", &gConfig.textureQuality, qLevels, IM_ARRAYSIZE(qLevels));

        ImGui::TextUnformatted("Shadow Quality:");
        ImGui::SetNextItemWidth(180);
        ImGui::Combo("##ShadowQ", &gConfig.shadowQuality, qLevels, IM_ARRAYSIZE(qLevels));

        ImGui::TextUnformatted("Reflection Quality:");
        ImGui::SetNextItemWidth(180);
        ImGui::Combo("##ReflQ", &gConfig.reflectionQuality, qLevels, IM_ARRAYSIZE(qLevels));

        ImGui::TextUnformatted("Level of Detail (LOD) Scale:");
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("##LodScale", &gConfig.lodScale, 0.5f, 2.0f, "%.1fx");

        const char* memCaps[] = { "Automatic (OS Managed)", "4096 MB", "6144 MB", "8192 MB", "12288 MB" };
        int memCapIndex = 0;
        if (gConfig.memoryCapMB == 4096) memCapIndex = 1;
        else if (gConfig.memoryCapMB == 6144) memCapIndex = 2;
        else if (gConfig.memoryCapMB == 8192) memCapIndex = 3;
        else if (gConfig.memoryCapMB == 12288) memCapIndex = 4;

        ImGui::TextUnformatted("Engine Memory Cap:");
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("##MemCap", &memCapIndex, memCaps, IM_ARRAYSIZE(memCaps))) {
            const int capValues[] = { 0, 4096, 6144, 8192, 12288 };
            gConfig.memoryCapMB = capValues[memCapIndex];
            PGSetMemoryCap(gConfig.memoryCapMB);
        }

        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderControlsHudTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ CONTROLS & HUD LAYOUT EDITOR");
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Controller Status
    XINPUT_STATE xstate;
    bool controllerConnected = (XInputGetState(0, &xstate) == ERROR_SUCCESS);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("ControlsCard", ImVec2(0, 100), true);
    {
        ImGui::Columns(2, "CtrlCols", false);
        ImGui::TextUnformatted("Primary Controller (XInput):");
        if (controllerConnected) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "[CONNECTED] Xbox / XInput Gamepad Detected");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.75f, 1.0f), "[NONE] Keyboard & Mouse Active (Gamepad Disconnected)");
        }

        ImGui::NextColumn();

        const char* targetingModes[] = { "Traditional Lock-On", "Assisted Aiming", "Assisted Free Aim", "Free Aim (PC)" };
        int curTargeting = PGTargetingMode();
        if (curTargeting < 0 || curTargeting > 3) curTargeting = 0;
        ImGui::TextUnformatted("Weapon Targeting Mode:");
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("##TargetingMode", &curTargeting, targetingModes, IM_ARRAYSIZE(targetingModes))) {
            PGSetTargetingMode(curTargeting);
        }

        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // 2. HUD Layout Sliders
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("HudCard", ImVec2(0, 310), true);
    {
        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Live HUD Element Positions & Sizing");
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "Adjust HUD pieces to fit widescreen or ultrawide monitors.");
        ImGui::Spacing();

        if (ImGui::SliderFloat("Minimap Scale", &gConfig.minimapScale, 0.5f, 2.0f, "%.2fx")) {
            PGSetMinimapLayout(gConfig.minimapDX, gConfig.minimapDY, gConfig.minimapScale);
        }
        if (ImGui::SliderFloat("Help Messages Scale", &gConfig.helpTextScale, 0.5f, 2.0f, "%.2fx")) {
            PGSetHelpTextLayout(gConfig.helpTextX, gConfig.helpTextY, gConfig.helpTextScale);
        }
        if (ImGui::SliderFloat("Character Switch Wheel Scale", &gConfig.wheelScale, 0.5f, 2.0f, "%.2fx")) {
            PGSetHudPiece(0, gConfig.wheelDX, gConfig.wheelDY, gConfig.wheelScale);
        }
        if (ImGui::SliderFloat("Mobile Phone Scale", &gConfig.phoneScale, 0.5f, 2.0f, "%.2fx")) {
            PGSetHudPiece(1, gConfig.phoneDX, gConfig.phoneDY, gConfig.phoneScale);
        }
        if (ImGui::SliderFloat("Notification Feed Scale", &gConfig.feedScale, 0.5f, 2.0f, "%.2fx")) {
            PGSetHudPiece(2, gConfig.feedDX, gConfig.feedDY, gConfig.feedScale);
        }
        if (ImGui::SliderFloat("Subtitles Position Raise", &gConfig.subtitleRaise, 0.0f, 0.3f, "%.2f")) {
            PGSetSubtitleRaise(gConfig.subtitleRaise);
        }

        ImGui::Spacing();
        if (ImGui::Button("Reset All HUD Elements to Default", ImVec2(240, 0))) {
            gConfig.minimapScale = 1.0f; gConfig.minimapDX = 0.0f; gConfig.minimapDY = 0.0f;
            gConfig.helpTextScale = 1.0f; gConfig.helpTextX = 0.0f; gConfig.helpTextY = 0.0f;
            gConfig.wheelScale = 1.0f; gConfig.wheelDX = 0.0f; gConfig.wheelDY = 0.0f;
            gConfig.phoneScale = 1.0f; gConfig.phoneDX = 0.0f; gConfig.phoneDY = 0.0f;
            gConfig.feedScale = 1.0f; gConfig.feedDX = 0.0f; gConfig.feedDY = 0.0f;
            gConfig.subtitlesScale = 1.0f; gConfig.subtitleRaise = 0.0f;

            PGSetMinimapLayout(0, 0, 1.0);
            PGSetHelpTextLayout(0, 0, 1.0);
            for (int i = 0; i < 6; i++) PGSetHudPiece(i, 0, 0, 1.0);
            PGSetSubtitleRaise(0.0);
            SaveConfig();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderModsTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ MODS & SCRIPT HOOK V MANAGER");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("ModsCard", ImVec2(0, 420), true);
    {
        if (ImGui::Checkbox("Enable Script Hook V (.asi) and Custom Game Mods", &gConfig.modsEnabled)) {
            PGSetModsOff(!gConfig.modsEnabled);
            SaveConfig();
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 170);
        if (ImGui::Button("Open Mods Folder", ImVec2(160, 0))) {
            WinFiles::OpenFolderInExplorer(WinFiles::GetModsFolder());
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f),
            "Drop your GTA V PC mods into the Mods folder:\n"
            " • Script mods: .asi files (e.g. Simple Trainer, Enhanced Native Trainer)\n"
            " • Package mods: .oiv files or unpacked assembly folders\n"
            " • Custom vehicles and peds: .yft / .ytd files\n"
            " • Data files: handling.meta, vehicles.meta, etc.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Detected Mods in Mods Folder:");

        std::string modsDir = WinFiles::GetModsFolder();
        std::error_code ec;
        int count = 0;
        if (fs::exists(modsDir, ec)) {
            for (const auto& entry : fs::directory_iterator(modsDir, ec)) {
                std::string fname = entry.path().filename().string();
                if (fname == "README.txt") continue;
                count++;
                bool isAsi = (entry.path().extension() == ".asi");
                if (isAsi) {
                    ImGui::BulletText("%s [Script Hook V .asi Module]", fname.c_str());
                } else if (entry.is_directory()) {
                    ImGui::BulletText("%s [Addon / Replacement Mod Directory]", fname.c_str());
                } else {
                    ImGui::BulletText("%s", fname.c_str());
                }
            }
        }

        if (count == 0) {
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f),
                "(No mods installed yet. Click 'Open Mods Folder' to install mods.)");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderAudioTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ AUDIO & SELF RADIO (CUSTOM TRACKS)");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("AudioCard", ImVec2(0, 380), true);
    {
        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Master Volume Levels");
        ImGui::Spacing();

        ImGui::SliderInt("Master Volume", &gConfig.masterVolume, 0, 100, "%d%%");
        ImGui::SliderInt("Sound Effects (SFX)", &gConfig.sfxVolume, 0, 100, "%d%%");
        ImGui::SliderInt("Music & Radio Volume", &gConfig.musicVolume, 0, 100, "%d%%");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Self Radio (Custom Music Station)");
        ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f),
            "Add your own audio tracks (.mp3, .wav, .m4a, .flac) to the Self Radio folder.");

        ImGui::Spacing();
        if (ImGui::Button("Open Self Radio Music Folder", ImVec2(220, 0))) {
            WinFiles::OpenFolderInExplorer(WinFiles::GetUserMusicFolder());
        }

        ImGui::Spacing();
        const char* radioModes[] = { "Radio Station (Midway join, realistic)", "Sequential (In order)", "Shuffle (Randomized)" };
        ImGui::TextUnformatted("Playback Order:");
        ImGui::SetNextItemWidth(260);
        if (ImGui::Combo("##RadioMode", &gConfig.selfRadioMode, radioModes, IM_ARRAYSIZE(radioModes))) {
            PGSetSelfRadioMode(gConfig.selfRadioMode);
            SaveConfig();
        }

        if (gIsGameRunning) {
            ImGui::Spacing();
            bool audible = PGSelfRadioAudible();
            ImGui::Text("Self Radio Audible Right Now: %s", audible ? "YES" : "NO");
            if (ImGui::Button("Next Song Track", ImVec2(140, 0))) {
                PGSelfRadioNext();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderCheatsTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ CHEATS & VEHICLE SPAWNER");
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Persistent Cheats
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("CheatsCard", ImVec2(0, 140), true);
    {
        ImGui::Columns(2, "CheatCols", false);

        if (ImGui::Checkbox("God Mode (Invulnerability)", &gConfig.godMode)) {
            PGSetPersistentCheatEnabled(PGCheatGodMode, gConfig.godMode);
            SaveConfig();
        }
        if (ImGui::Checkbox("Never Wanted (Police Ignore Crimes)", &gConfig.neverWanted)) {
            PGSetPersistentCheatEnabled(PGCheatNeverWanted, gConfig.neverWanted);
            SaveConfig();
        }

        static int cashAmount = 1000000;
        ImGui::SetNextItemWidth(140);
        ImGui::InputInt("##CashAmount", &cashAmount, 50000);
        ImGui::SameLine();
        if (ImGui::Button("Set Character Cash ($)")) {
            PGSetPlayerCash(cashAmount);
            Log("Player cash set cheat triggered.");
        }

        ImGui::NextColumn();

        if (ImGui::Button("Repair Vehicle", ImVec2(130, 0))) {
            PGQueueCheat(PGCheatRepairVehicle, 0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clean Vehicle", ImVec2(130, 0))) {
            PGQueueCheat(PGCheatCleanVehicle, 0);
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Time:");
        if (ImGui::Button("06:00")) PGQueueCheat(PGCheatSetTime, 6);
        ImGui::SameLine();
        if (ImGui::Button("12:00")) PGQueueCheat(PGCheatSetTime, 12);
        ImGui::SameLine();
        if (ImGui::Button("18:00")) PGQueueCheat(PGCheatSetTime, 18);
        ImGui::SameLine();
        if (ImGui::Button("00:00")) PGQueueCheat(PGCheatSetTime, 0);

        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // 2. Vehicle Spawner
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("VehicleCard", ImVec2(0, 280), true);
    {
        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "Vehicle Spawner");
        static char searchFilter[64] = {};
        static bool enterSpawned = true;

        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##VehSearch", "Search vehicles...", searchFilter, sizeof(searchFilter));
        ImGui::SameLine();
        ImGui::Checkbox("Enter Vehicle Immediately", &enterSpawned);

        ImGui::Spacing();

        std::string searchLower = searchFilter;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        ImGui::BeginChild("VehList", ImVec2(0, 200), false);
        for (const auto& v : kVehiclePresets) {
            std::string nameLower = v.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (!searchLower.empty() && nameLower.find(searchLower) == std::string::npos &&
                std::string(v.category).find(searchLower) == std::string::npos) {
                continue;
            }

            char btnLabel[128];
            snprintf(btnLabel, sizeof(btnLabel), "Spawn##%s", v.model);
            if (ImGui::Button(btnLabel, ImVec2(70, 0))) {
                PGSpawnVehicle(v.modelHash, enterSpawned);
                Log((std::string("Spawned vehicle: ") + v.name).c_str());
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "[%s]", v.category);
            ImGui::SameLine();
            ImGui::Text("%s (%s)", v.name, v.model);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderDiagnosticsTab() {
    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "★ SYSTEM DIAGNOSTICS & LOG OUTPUT");
    ImGui::Separator();
    ImGui::Spacing();

    // Hardware Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
    ImGui::BeginChild("HwCard", ImVec2(0, 90), true);
    {
        ImGui::Columns(2, "HwCols", false);
        ImGui::Text("Graphics Device: %s", gGpuName.c_str());
        ImGui::Text("Render Backend: Direct3D 11.1 Native Hardware");

        ImGui::NextColumn();
        ImGui::Text("FPS: %.1f FPS (%.2f ms)", PGRenderedFramesPerSecond(), PGGPUFrameMilliseconds());
        ImGui::Text("Target OS: Windows 10/11 x64");

        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Log Drawer
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.11f, 0.95f));
    ImGui::BeginChild("LogViewer", ImVec2(0, 320), true);
    {
        static char logSearch[64] = {};
        static bool autoScroll = true;

        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##LogSearch", "Filter log...", logSearch, sizeof(logSearch));
        ImGui::SameLine();
        ImGui::Checkbox("Auto-Scroll", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Clear Logs")) {
            ClearLogs();
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Log File in Notepad")) {
            ShellExecuteA(nullptr, "open", "notepad.exe", "GTA5Port.log", nullptr, SW_SHOWNORMAL);
        }

        ImGui::Separator();

        ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(gLogMutex);
            std::string filter = logSearch;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            for (const auto& entry : gLogs) {
                if (!filter.empty()) {
                    std::string textLower = entry.text;
                    std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
                    if (textLower.find(filter) == std::string::npos) continue;
                }

                ImVec4 color = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);
                if (entry.level == 1) color = ImVec4(0.95f, 0.75f, 0.20f, 1.0f); // Warning
                else if (entry.level == 2) color = ImVec4(0.95f, 0.35f, 0.30f, 1.0f); // Error

                ImGui::TextColored(ImVec4(0.50f, 0.55f, 0.65f, 1.0f), "[%s]", entry.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextColored(color, "%s", entry.text.c_str());
            }

            if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// -------------------------------------------------------------------------------------------------
// Launcher & In-Game Frame Rendering
// -------------------------------------------------------------------------------------------------

void RenderLauncherFrame(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv, IDXGISwapChain* swapChain) {
    if (!gInitialized || !context || !rtv || !swapChain) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("GTA5LauncherRoot", nullptr, windowFlags);
    {
        // Left Navigation Sidebar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.10f, 0.13f, 0.98f));
        ImGui::BeginChild("Sidebar", ImVec2(240, 0), true);
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "GRAND THEFT AUTO V");
            ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Windows Native Port");
            ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "Direct3D 11 Backend");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const char* tabNames[] = {
                "▶  Play / Launch",
                "⚙  Graphics & Display",
                "🎮  Controls & HUD",
                "🧩  Mods & Scripts",
                "📻  Audio & Radio",
                "⚡  Cheats & Vehicles",
                "📊  Diagnostics & Log"
            };

            for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
                bool selected = (gActiveTab == i);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.65f, 0.14f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
                }

                if (ImGui::Button(tabNames[i], ImVec2(-1, 38))) {
                    gActiveTab = i;
                }

                if (selected) {
                    ImGui::PopStyleColor(2);
                }
                ImGui::Spacing();
            }

            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 65);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.50f, 0.55f, 0.65f, 1.0f), "Build 8b0b5899ed");
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.40f, 1.0f), "● D3D11 Pipeline Ready");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Right Content Area
        ImGui::BeginChild("MainContentArea", ImVec2(0, 0), false);
        {
            switch (gActiveTab) {
                case 0: RenderPlayTab(hwnd); break;
                case 1: RenderGraphicsTab(); break;
                case 2: RenderControlsHudTab(); break;
                case 3: RenderModsTab(); break;
                case 4: RenderAudioTab(); break;
                case 5: RenderCheatsTab(); break;
                case 6: RenderDiagnosticsTab(); break;
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // Render to swapchain
    ImGui::Render();
    const float clearColor[4] = { 0.07f, 0.08f, 0.10f, 1.0f };
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->ClearRenderTargetView(rtv, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain->Present(1, 0);
}

void RenderInGameOverlay(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv) {
    if (!gInitialized || !context || !rtv) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    // 1. Minimal Performance HUD in corner
    if (gConfig.showPerfHud) {
        ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav;
        ImVec2 hudPos = (gConfig.perfHudPosition == 0)
            ? ImVec2(io.DisplaySize.x - 190, 15)
            : ImVec2(15, 15);
        ImGui::SetNextWindowPos(hudPos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.65f);

        if (ImGui::Begin("PerfHUD", nullptr, hudFlags)) {
            ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.14f, 1.0f), "GTA V PORT  %s", gGpuDriverLevel.c_str());
            ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.35f, 1.0f), "FPS: %.1f  (%.2f ms)",
                PGRenderedFramesPerSecond(), PGGPUFrameMilliseconds());
            ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.80f, 1.0f), "[F1] In-Game Menu");
        }
        ImGui::End();
    }

    // 2. Full In-Game Overlay Menu (Toggled with F1 or Insert)
    if (gIsOverlayOpen) {
        ImVec2 centerPos = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        ImVec2 menuSize = ImVec2(std::min(io.DisplaySize.x - 40.0f, 960.0f), std::min(io.DisplaySize.y - 40.0f, 640.0f));
        ImGui::SetNextWindowPos(centerPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(menuSize, ImGuiCond_Appearing);

        if (ImGui::Begin("GTA V In-Game Overlay Menu", &gIsOverlayOpen, ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginTabBar("InGameTabs")) {
                if (ImGui::BeginTabItem("⚡ Cheats & Vehicles")) {
                    RenderCheatsTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("⚙ Graphics")) {
                    RenderGraphicsTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("🎮 HUD Layout")) {
                    RenderControlsHudTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("🧩 Mods")) {
                    RenderModsTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("📊 Logs")) {
                    RenderDiagnosticsTab();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

} // namespace WinUI

// C Linkage Wrappers
extern "C" {
    void WinUILog(const char* message) {
        WinUI::Log(message);
    }

    void WinUIRenderInGameOverlay(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv) {
        WinUI::RenderInGameOverlay(hwnd, context, rtv);
    }

    bool WinUIIsGameRunning(void) {
        return WinUI::IsGameRunning();
    }

    bool WinUIIsOverlayOpen(void) {
        return WinUI::IsOverlayOpen();
    }

    void WinUISetOverlayOpen(bool open) {
        WinUI::SetOverlayOpen(open);
    }

    void WinUIToggleOverlay(void) {
        WinUI::ToggleOverlay();
    }
}

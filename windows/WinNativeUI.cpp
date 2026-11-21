#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <xinput.h>

#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <sstream>

#include "WinNativeUI.h"
#include "WinFiles.h"
#include "WinUI.h"
#include "resources/resource.h"

#ifdef DrawText
#undef DrawText
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "xinput.lib")

using Microsoft::WRL::ComPtr;

namespace {

// Application Screens
enum Screen {
    SCREEN_PLAY = 0,
    SCREEN_SETTINGS = 1,
    SCREEN_GAMEFILES = 2
};

static HWND gHwnd = nullptr;
static WinNativeUI::LaunchHandler gLaunchHandler = nullptr;
static bool gIsVisible = true;
static bool gInitialized = false;

// Direct2D / DirectWrite / WIC device resources
static ComPtr<ID2D1Factory> gD2DFactory;
static ComPtr<IDWriteFactory> gDWriteFactory;
static ComPtr<IWICImagingFactory> gWicFactory;
static ComPtr<ID2D1HwndRenderTarget> gRenderTarget;

// Bitmaps decoded from binary resources
static ComPtr<ID2D1Bitmap> gBmpStartBg;
static ComPtr<ID2D1Bitmap> gBmpSettingsBg;
static ComPtr<ID2D1Bitmap> gBmpAppIcon;
static ComPtr<ID2D1Bitmap> gBmpCardStory;
static ComPtr<ID2D1Bitmap> gBmpCardSandbox;
static ComPtr<ID2D1Bitmap> gBmpCardPerf;
static ComPtr<ID2D1Bitmap> gBmpCardMods;
static ComPtr<ID2D1Bitmap> gBmpCardRadio;

// Solid color brushes
static ComPtr<ID2D1SolidColorBrush> gBrushWhite;
static ComPtr<ID2D1SolidColorBrush> gBrushBlack;
static ComPtr<ID2D1SolidColorBrush> gBrushAccentOrange;
static ComPtr<ID2D1SolidColorBrush> gBrushAccentBlue;
static ComPtr<ID2D1SolidColorBrush> gBrushGoldBeta;
static ComPtr<ID2D1SolidColorBrush> gBrushOk;
static ComPtr<ID2D1SolidColorBrush> gBrushWarn;
static ComPtr<ID2D1SolidColorBrush> gBrushProblem;
static ComPtr<ID2D1SolidColorBrush> gBrushDarkNight;
static ComPtr<ID2D1SolidColorBrush> gBrushCardBg;
static ComPtr<ID2D1SolidColorBrush> gBrushCardActiveBg;
static ComPtr<ID2D1SolidColorBrush> gBrushPanelStroke;
static ComPtr<ID2D1SolidColorBrush> gBrushMuted;
static ComPtr<ID2D1SolidColorBrush> gBrushDim;
static ComPtr<ID2D1LinearGradientBrush> gVignetteBrush;

// Text Formats
static ComPtr<IDWriteTextFormat> gFormatHeroTitle;
static ComPtr<IDWriteTextFormat> gFormatSectionTitle;
static ComPtr<IDWriteTextFormat> gFormatNavTab;
static ComPtr<IDWriteTextFormat> gFormatNavTabActive;
static ComPtr<IDWriteTextFormat> gFormatBody;
static ComPtr<IDWriteTextFormat> gFormatBodySemibold;
static ComPtr<IDWriteTextFormat> gFormatSub;
static ComPtr<IDWriteTextFormat> gFormatMicro;
static ComPtr<IDWriteTextFormat> gFormatKeycap;

// UI State
static Screen gCurrentScreen = SCREEN_PLAY;
static int gSelectedMode = 0;       // 0: Story, 1: Sandbox, 2: Performance, 3: Mods, 4: Radio
static int gSandboxMap = 0;         // 0: Los Santos, 1: GTA VI Test Map
static int gActiveSettingsTab = 0;  // 0..8
static int gActivePreset = 4;       // Quality (recommended)
static bool gShowProblemsModal = false;
static float gSettingsScrollY = 0.0f;
static float gProblemsScrollY = 0.0f;

// Mouse & Input
static float gMouseX = -1.0f;
static float gMouseY = -1.0f;
static bool gMouseDown = false;

// Hardware & Verification info
static std::string gFolderDisplay = "C:\\Games\\Grand Theft Auto V";
static WinGameFolderReport gReport;
static std::string gGpuName = "Direct3D 11 Hardware Graphics";
static std::string gCpuName = "x86_64 Multi-Core Processor";
static std::string gRamInfo = "16 GB Physical RAM";

// Settings options and keys
struct OptionGroup {
    const wchar_t* name;
    std::vector<std::wstring> options;
    int currentIndex;
};

static OptionGroup gOptResolution = { L"Resolution", { L"Native (1080p)", L"720p (1280x720)", L"900p (1600x900)", L"1440p (2560x1440)", L"4K (3840x2160)" }, 0 };
static OptionGroup gOptFullscreen = { L"Fill the screen", { L"On", L"Off" }, 0 };
static OptionGroup gOptFramerate  = { L"Frame rate", { L"Unlocked", L"30 FPS", L"60 FPS", L"120 FPS" }, 0 };
static OptionGroup gOptShowFps    = { L"Show FPS counter", { L"Off", L"On" }, 0 };
static OptionGroup gOptVsync      = { L"VSync", { L"On", L"Off" }, 0 };
static OptionGroup gOptMemoryCap  = { L"Memory cap", { L"Automatic", L"4 GB", L"6 GB", L"8 GB" }, 0 };
static OptionGroup gOptTextures   = { L"Textures", { L"Preset (High)", L"Normal", L"Very High" }, 0 };
static OptionGroup gOptShaders    = { L"Shaders", { L"Preset (High)", L"Normal", L"Very High" }, 0 };
static OptionGroup gOptShadows    = { L"Shadows", { L"Preset (Normal)", L"Low", L"High" }, 0 };
static OptionGroup gOptReflections= { L"Reflections", { L"Preset (High)", L"Normal", L"Very High" }, 0 };
static OptionGroup gOptDrawDist   = { L"Draw distance", { L"Preset (70%)", L"40%", L"100%" }, 0 };
static OptionGroup gOptGrass      = { L"Grass", { L"Preset (High)", L"Normal", L"Very High" }, 0 };
static OptionGroup gOptLanguage   = { L"Language", { L"English (US)", L"Fran\u00E7ais", L"Deutsch", L"Espa\u00F1ol", L"Italiano", L"\u0420\u0443\u0441\u0441\u043A\u0438\u0439" }, 0 };
static OptionGroup gOptMods       = { L"Enable Mods", { L"On", L"Off" }, 0 };
static OptionGroup gOptSelfRadio  = { L"Playback Mode", { L"Radio Station", L"Sequential", L"Shuffle" }, 0 };

static std::vector<OptionGroup*> gGraphicsRows = {
    &gOptResolution, &gOptFullscreen, &gOptFramerate, &gOptShowFps,
    &gOptVsync, &gOptMemoryCap, &gOptTextures, &gOptShaders,
    &gOptShadows, &gOptReflections, &gOptDrawDist, &gOptGrass
};

static const wchar_t* kPresetNames[] = {
    L"Ultra Performance", L"Battery", L"Performance", L"Balanced",
    L"Quality (recommended)", L"Ultra", L"Max"
};

static const wchar_t* kSidebarTabs[] = {
    L"Graphics", L"Controls", L"Language and saves", L"Game files",
    L"Mods & patches", L"Self Radio", L"Online", L"This device", L"Credits"
};

static const wchar_t* kCardLabels[] = {
    L"STORY MODE", L"SANDBOX", L"PERFORMANCE", L"MODS & PATCHES", L"SELF RADIO"
};

static const wchar_t* kHeroTitles[] = {
    L"STORY MODE", L"SANDBOX", L"PERFORMANCE", L"MODS & PATCHES", L"SELF RADIO"
};

static const wchar_t* kHeroDescs[] = {
    L"The campaign with missions. Continues your latest save, or starts from the Prologue.",
    L"Free roam in Los Santos with cheats, vehicle spawning, and full map exploration.",
    L"Optimized high-framerate mode with balanced assets for smooth 60+ FPS gameplay.",
    L"Load custom ASI scripts, vehicle models, and community modifications.",
    L"Your own custom music tracks broadcast on the in-game Self Radio station."
};

// Helper: Load binary image from resource module using WIC and Direct2D
static ComPtr<ID2D1Bitmap> LoadBitmapFromResource(int resId) {
    if (!gRenderTarget || !gWicFactory) return nullptr;

    HMODULE hMod = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceA(hMod, MAKEINTRESOURCEA(resId), RT_RCDATA);
    if (!hRes) return nullptr;

    DWORD size = SizeofResource(hMod, hRes);
    HGLOBAL hGlobal = LoadResource(hMod, hRes);
    if (!hGlobal) return nullptr;

    void* pData = LockResource(hGlobal);
    if (!pData || size == 0) return nullptr;

    ComPtr<IWICStream> stream;
    if (FAILED(gWicFactory->CreateStream(stream.GetAddressOf()))) return nullptr;
    if (FAILED(stream->InitializeFromMemory(reinterpret_cast<WICInProcPointer>(pData), size))) return nullptr;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(gWicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()))) return nullptr;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(gWicFactory->CreateFormatConverter(converter.GetAddressOf()))) return nullptr;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return nullptr;

    ComPtr<ID2D1Bitmap> bitmap;
    if (FAILED(gRenderTarget->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap.GetAddressOf()))) return nullptr;

    return bitmap;
}

// Aspect-fill bitmap rendering (no distortion)
static void DrawAspectFillBitmap(ID2D1Bitmap* bitmap, D2D1_RECT_F dst, float opacity = 1.0f) {
    if (!bitmap || !gRenderTarget) return;

    D2D1_SIZE_F srcSize = bitmap->GetSize();
    float dstW = dst.right - dst.left;
    float dstH = dst.bottom - dst.top;

    float scale = (std::max)(dstW / srcSize.width, dstH / srcSize.height);
    float scaledW = srcSize.width * scale;
    float scaledH = srcSize.height * scale;

    float offsetX = (scaledW - dstW) * 0.5f;
    float offsetY = (scaledH - dstH) * 0.5f;

    D2D1_RECT_F srcSub = D2D1::RectF(
        offsetX / scale,
        offsetY / scale,
        (offsetX + dstW) / scale,
        (offsetY + dstH) / scale
    );

    gRenderTarget->DrawBitmap(bitmap, dst, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcSub);
}

static bool IsHovered(D2D1_RECT_F rect) {
    return (gMouseX >= rect.left && gMouseX <= rect.right && gMouseY >= rect.top && gMouseY <= rect.bottom);
}

static void QueryHardware() {
    // CPU
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[256] = {};
        DWORD sz = sizeof(buf);
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
            gCpuName = buf;
            size_t p = gCpuName.find_first_not_of(" ");
            if (p != std::string::npos) gCpuName = gCpuName.substr(p);
        }
        RegCloseKey(hKey);
    }

    // RAM
    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (GlobalMemoryStatusEx(&mem)) {
        int gb = (int)((mem.ullTotalPhys + (1024 * 1024 * 1024 - 1)) / (1024 * 1024 * 1024));
        gRamInfo = std::to_string(gb) + " GB Physical RAM";
    }
}

static void CreateDeviceResources() {
    if (!gRenderTarget) return;

    // Solid Color Brushes
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), gBrushWhite.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), gBrushBlack.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.62f, 0.18f, 1.0f), gBrushAccentOrange.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.29f, 0.56f, 0.89f, 1.0f), gBrushAccentBlue.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.90f, 0.66f, 0.23f, 1.0f), gBrushGoldBeta.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.85f, 0.47f, 1.0f), gBrushOk.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.78f, 0.26f, 1.0f), gBrushWarn.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.42f, 0.38f, 1.0f), gBrushProblem.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.03f, 0.05f, 0.11f, 1.0f), gBrushDarkNight.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.07f, 0.07f, 0.07f, 0.94f), gBrushCardBg.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f, 0.98f), gBrushCardActiveBg.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), gBrushPanelStroke.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.75f), gBrushMuted.GetAddressOf());
    gRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.45f), gBrushDim.GetAddressOf());

    // Linear Gradient Vignette
    D2D1_SIZE_F size = gRenderTarget->GetSize();
    D2D1_GRADIENT_STOP stops[] = {
        { 0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.20f) },
        { 0.45f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f) },
        { 1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.94f) }
    };
    ComPtr<ID2D1GradientStopCollection> stopCollection;
    gRenderTarget->CreateGradientStopCollection(stops, 3, stopCollection.GetAddressOf());
    if (stopCollection) {
        gRenderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, size.height)),
            stopCollection.Get(),
            gVignetteBrush.GetAddressOf()
        );
    }

    // Load Bitmaps from RCDATA resources
    gBmpStartBg      = LoadBitmapFromResource(IDR_START_BG);
    gBmpSettingsBg   = LoadBitmapFromResource(IDR_SETTINGS_BG);
    gBmpAppIcon      = LoadBitmapFromResource(IDR_APP_ICON);
    gBmpCardStory    = LoadBitmapFromResource(IDR_CARD_STORY);
    gBmpCardSandbox  = LoadBitmapFromResource(IDR_CARD_SANDBOX);
    gBmpCardPerf     = LoadBitmapFromResource(IDR_CARD_PERF);
    gBmpCardMods     = LoadBitmapFromResource(IDR_CARD_MODS);
    gBmpCardRadio    = LoadBitmapFromResource(IDR_CARD_RADIO);
}

static void DiscardDeviceResources() {
    gBrushWhite.Reset();
    gBrushBlack.Reset();
    gBrushAccentOrange.Reset();
    gBrushAccentBlue.Reset();
    gBrushGoldBeta.Reset();
    gBrushOk.Reset();
    gBrushWarn.Reset();
    gBrushProblem.Reset();
    gBrushDarkNight.Reset();
    gBrushCardBg.Reset();
    gBrushCardActiveBg.Reset();
    gBrushPanelStroke.Reset();
    gBrushMuted.Reset();
    gBrushDim.Reset();
    gVignetteBrush.Reset();

    gBmpStartBg.Reset();
    gBmpSettingsBg.Reset();
    gBmpAppIcon.Reset();
    gBmpCardStory.Reset();
    gBmpCardSandbox.Reset();
    gBmpCardPerf.Reset();
    gBmpCardMods.Reset();
    gBmpCardRadio.Reset();

    gRenderTarget.Reset();
}

} // namespace

namespace WinNativeUI {

bool Init(HWND hwnd, LaunchHandler onLaunch) {
    gHwnd = hwnd;
    gLaunchHandler = onLaunch;

    // 1. Initialize Direct2D Factory
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, gD2DFactory.GetAddressOf()))) return false;

    // 2. Initialize DirectWrite Factory
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(gDWriteFactory.GetAddressOf())))) return false;

    // 3. Initialize WIC Imaging Factory
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(gWicFactory.GetAddressOf())))) return false;

    // 4. Create Text Formats
    const wchar_t* fontHeading = L"Arial Narrow";
    const wchar_t* fontBody = L"Segoe UI";

    gDWriteFactory->CreateTextFormat(fontHeading, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 38.0f, L"en-US", gFormatHeroTitle.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontHeading, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 22.0f, L"en-US", gFormatSectionTitle.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontHeading, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 16.0f, L"en-US", gFormatNavTab.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontHeading, nullptr, DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 16.0f, L"en-US", gFormatNavTabActive.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontBody, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-US", gFormatBody.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontBody, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-US", gFormatBodySemibold.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontBody, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-US", gFormatSub.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontBody, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-US", gFormatMicro.GetAddressOf());
    gDWriteFactory->CreateTextFormat(fontHeading, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 12.0f, L"en-US", gFormatKeycap.GetAddressOf());

    // Text format alignments
    if (gFormatHeroTitle) gFormatHeroTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (gFormatSectionTitle) gFormatSectionTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (gFormatNavTabActive) gFormatNavTabActive->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (gFormatKeycap) gFormatKeycap->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    // 5. Create Hwnd Render Target
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hwnd, size, D2D1_PRESENT_OPTIONS_NONE);

    if (FAILED(gD2DFactory->CreateHwndRenderTarget(rtProps, hwndProps, gRenderTarget.GetAddressOf()))) return false;

    CreateDeviceResources();
    QueryHardware();
    RefreshStatus();

    gInitialized = true;
    gIsVisible = true;
    return true;
}

void Shutdown() {
    DiscardDeviceResources();
    gFormatHeroTitle.Reset();
    gFormatSectionTitle.Reset();
    gFormatNavTab.Reset();
    gFormatNavTabActive.Reset();
    gFormatBody.Reset();
    gFormatBodySemibold.Reset();
    gFormatSub.Reset();
    gFormatMicro.Reset();
    gFormatKeycap.Reset();

    gWicFactory.Reset();
    gDWriteFactory.Reset();
    gD2DFactory.Reset();
    gInitialized = false;
}

void Resize(int width, int height) {
    if (gRenderTarget) {
        gRenderTarget->Resize(D2D1::SizeU(width, height));
        // Recreate vignette brush for new height
        if (gVignetteBrush) {
            gVignetteBrush->SetEndPoint(D2D1::Point2F(0, (float)height));
        }
    }
}

void SetVisible(bool visible) {
    gIsVisible = visible;
    if (gHwnd) InvalidateRect(gHwnd, nullptr, FALSE);
}

bool IsVisible() {
    return gIsVisible && gInitialized;
}

void RefreshStatus() {
    std::string folder = WinFiles::LoadSavedGameFolder();
    if (folder.empty()) {
        folder = WinFiles::AutoDetectGameFolder();
        if (!folder.empty()) WinFiles::SaveGameFolder(folder);
    }
    gFolderDisplay = folder.empty() ? "C:\\Games\\Grand Theft Auto V" : folder;
    gReport = WinFiles::VerifyGameFolder(gFolderDisplay);
}

// ----------------------------------------------------------------------------
// Screen 1: Home View (PLAY Tab)
// ----------------------------------------------------------------------------
static void RenderScreenPlay(D2D1_SIZE_F size) {
    // 1. Full-Bleed Los Santos Sunset Skyline Backdrop
    if (gBmpStartBg) {
        DrawAspectFillBitmap(gBmpStartBg.Get(), D2D1::RectF(0, 0, size.width, size.height));
    } else {
        gRenderTarget->FillRectangle(D2D1::RectF(0, 0, size.width, size.height), gBrushDarkNight.Get());
    }

    // 2. Dark Vignette Overlay
    if (gVignetteBrush) {
        gRenderTarget->FillRectangle(D2D1::RectF(0, 0, size.width, size.height), gVignetteBrush.Get());
    }

    // 3. Top Navigation Bar
    float topBarY = 16.0f;
    float navX = 36.0f;

    // PLAY Active Pill
    D2D1_RECT_F rPlay = D2D1::RectF(navX, topBarY, navX + 72.0f, topBarY + 34.0f);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rPlay, 2.0f, 2.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"PLAY", 4, gFormatNavTabActive.Get(), D2D1::RectF(rPlay.left, rPlay.top + 7.0f, rPlay.right, rPlay.bottom), gBrushBlack.Get());
    navX += 86.0f;

    // ONLINE [BETA] Tab
    D2D1_RECT_F rOnline = D2D1::RectF(navX, topBarY, navX + 110.0f, topBarY + 34.0f);
    bool hoverOnline = IsHovered(rOnline);
    gRenderTarget->DrawText(L"ONLINE", 6, gFormatNavTab.Get(), D2D1::RectF(rOnline.left, rOnline.top + 7.0f, rOnline.left + 60.0f, rOnline.bottom), hoverOnline ? gBrushWhite.Get() : gBrushMuted.Get());

    // [BETA] Tag
    D2D1_RECT_F rBeta = D2D1::RectF(rOnline.left + 64.0f, rOnline.top + 8.0f, rOnline.left + 104.0f, rOnline.top + 26.0f);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBeta, 2.0f, 2.0f), gBrushGoldBeta.Get());
    gRenderTarget->DrawText(L"BETA", 4, gFormatKeycap.Get(), D2D1::RectF(rBeta.left, rBeta.top + 2.0f, rBeta.right, rBeta.bottom), gBrushBlack.Get());
    navX += 122.0f;

    // SETTINGS Tab
    D2D1_RECT_F rSettings = D2D1::RectF(navX, topBarY, navX + 90.0f, topBarY + 34.0f);
    bool hoverSettings = IsHovered(rSettings);
    gRenderTarget->DrawText(L"SETTINGS", 8, gFormatNavTab.Get(), D2D1::RectF(rSettings.left, rSettings.top + 7.0f, rSettings.right, rSettings.bottom), hoverSettings ? gBrushWhite.Get() : gBrushMuted.Get());

    // Right Links
    float rightX = size.width - 260.0f;
    D2D1_RECT_F rSupport = D2D1::RectF(rightX, topBarY + 7.0f, rightX + 60.0f, topBarY + 28.0f);
    gRenderTarget->DrawText(L"SUPPORT", 7, gFormatBodySemibold.Get(), rSupport, IsHovered(rSupport) ? gBrushWhite.Get() : gBrushDim.Get());
    gRenderTarget->DrawText(L"|", 1, gFormatBodySemibold.Get(), D2D1::RectF(rightX + 66.0f, topBarY + 7.0f, rightX + 74.0f, topBarY + 28.0f), gBrushDim.Get());

    D2D1_RECT_F rTelegram = D2D1::RectF(rightX + 80.0f, topBarY + 7.0f, rightX + 155.0f, topBarY + 28.0f);
    gRenderTarget->DrawText(L"TELEGRAM", 8, gFormatBodySemibold.Get(), rTelegram, IsHovered(rTelegram) ? gBrushWhite.Get() : gBrushDim.Get());
    gRenderTarget->DrawText(L"|", 1, gFormatBodySemibold.Get(), D2D1::RectF(rightX + 160.0f, topBarY + 7.0f, rightX + 168.0f, topBarY + 28.0f), gBrushDim.Get());

    D2D1_RECT_F rDiscord = D2D1::RectF(rightX + 174.0f, topBarY + 7.0f, rightX + 240.0f, topBarY + 28.0f);
    gRenderTarget->DrawText(L"DISCORD", 7, gFormatBodySemibold.Get(), rDiscord, IsHovered(rDiscord) ? gBrushWhite.Get() : gBrushDim.Get());

    // 4. Bottom 5-Card Strip
    float cardGap = 14.0f;
    float marginX = 36.0f;
    float cardStripY = size.height - 180.0f;
    float cardH = 144.0f;
    float cardW = (size.width - 2.0f * marginX - 4.0f * cardGap) / 5.0f;

    ID2D1Bitmap* cardBmps[] = {
        gBmpCardStory.Get(), gBmpCardSandbox.Get(), gBmpCardPerf.Get(),
        gBmpCardMods.Get(), gBmpCardRadio.Get()
    };

    for (int i = 0; i < 5; ++i) {
        float cx = marginX + i * (cardW + cardGap);
        D2D1_RECT_F rCard = D2D1::RectF(cx, cardStripY, cx + cardW, cardStripY + cardH);
        bool isSelected = (gSelectedMode == i);
        bool isCardHover = IsHovered(rCard);

        // Card container fill
        gRenderTarget->FillRectangle(rCard, gBrushCardBg.Get());

        // Thumbnail (108 DIPs height)
        D2D1_RECT_F rThumb = D2D1::RectF(cx, cardStripY, cx + cardW, cardStripY + 108.0f);
        if (cardBmps[i]) {
            DrawAspectFillBitmap(cardBmps[i], rThumb);
        }

        // Label footer (36 DIPs height)
        D2D1_RECT_F rFooter = D2D1::RectF(cx, cardStripY + 108.0f, cx + cardW, cardStripY + cardH);
        gRenderTarget->FillRectangle(rFooter, isSelected ? gBrushCardActiveBg.Get() : gBrushCardBg.Get());
        gRenderTarget->DrawText(kCardLabels[i], (UINT32)wcslen(kCardLabels[i]), gFormatBodySemibold.Get(), D2D1::RectF(cx + 10.0f, cardStripY + 116.0f, cx + cardW - 10.0f, cardStripY + cardH), gBrushWhite.Get());

        // Active State: 3 DIP solid white bottom accent bar
        if (isSelected) {
            D2D1_RECT_F rAccent = D2D1::RectF(cx, cardStripY + cardH - 3.0f, cx + cardW, cardStripY + cardH);
            gRenderTarget->FillRectangle(rAccent, gBrushWhite.Get());
            gRenderTarget->DrawRectangle(rCard, gBrushWhite.Get(), 1.0f);
        } else if (isCardHover) {
            gRenderTarget->DrawRectangle(rCard, gBrushMuted.Get(), 1.0f);
        } else {
            gRenderTarget->DrawRectangle(rCard, gBrushPanelStroke.Get(), 1.0f);
        }
    }

    // 5. Hero Block (Above cards)
    float heroY = cardStripY - 210.0f;
    float heroX = 40.0f;

    // App Branding Logo
    if (gBmpAppIcon) {
        gRenderTarget->DrawBitmap(gBmpAppIcon.Get(), D2D1::RectF(heroX, heroY, heroX + 44.0f, heroY + 44.0f));
    }
    gRenderTarget->DrawText(L"GTA V PORT", 10, gFormatNavTabActive.Get(), D2D1::RectF(heroX + 54.0f, heroY, heroX + 300.0f, heroY + 22.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"Fan project \u00B7 plays your own game files \u00B7 v1.3", 44, gFormatSub.Get(), D2D1::RectF(heroX + 54.0f, heroY + 22.0f, heroX + 450.0f, heroY + 44.0f), gBrushMuted.Get());

    // Selected Mode Title
    heroY += 56.0f;
    const wchar_t* curTitle = kHeroTitles[gSelectedMode];
    gRenderTarget->DrawText(curTitle, (UINT32)wcslen(curTitle), gFormatHeroTitle.Get(), D2D1::RectF(heroX, heroY, heroX + 600.0f, heroY + 46.0f), gBrushWhite.Get());

    // Selected Mode Description
    heroY += 46.0f;
    const wchar_t* curDesc = kHeroDescs[gSelectedMode];
    gRenderTarget->DrawText(curDesc, (UINT32)wcslen(curDesc), gFormatBody.Get(), D2D1::RectF(heroX, heroY, heroX + 560.0f, heroY + 40.0f), gBrushMuted.Get());

    // Sandbox Map Selection Chips (If Sandbox Mode is active)
    if (gSelectedMode == 1) {
        heroY += 40.0f;
        D2D1_RECT_F rChip0 = D2D1::RectF(heroX, heroY, heroX + 100.0f, heroY + 28.0f);
        D2D1_RECT_F rChip1 = D2D1::RectF(heroX + 110.0f, heroY, heroX + 240.0f, heroY + 28.0f);

        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rChip0, 14.0f, 14.0f), (gSandboxMap == 0) ? gBrushAccentOrange.Get() : gBrushCardBg.Get());
        gRenderTarget->DrawText(L"Los Santos", 10, gFormatBodySemibold.Get(), D2D1::RectF(rChip0.left + 12.0f, rChip0.top + 5.0f, rChip0.right, rChip0.bottom), (gSandboxMap == 0) ? gBrushBlack.Get() : gBrushWhite.Get());

        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rChip1, 14.0f, 14.0f), (gSandboxMap == 1) ? gBrushAccentOrange.Get() : gBrushCardBg.Get());
        gRenderTarget->DrawText(L"GTA VI Test Map", 15, gFormatBodySemibold.Get(), D2D1::RectF(rChip1.left + 12.0f, rChip1.top + 5.0f, rChip1.right, rChip1.bottom), (gSandboxMap == 1) ? gBrushBlack.Get() : gBrushWhite.Get());
    }

    // PLAY Button
    heroY += (gSelectedMode == 1 ? 38.0f : 44.0f);
    D2D1_RECT_F rPlayCTA = D2D1::RectF(heroX, heroY, heroX + 150.0f, heroY + 44.0f);
    bool hoverCTA = IsHovered(rPlayCTA);
    bool valid = gReport.isValid;

    if (valid) {
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rPlayCTA, 2.0f, 2.0f), gBrushWhite.Get());
        gRenderTarget->DrawText(L"PLAY", 4, gFormatNavTabActive.Get(), D2D1::RectF(rPlayCTA.left, rPlayCTA.top + 12.0f, rPlayCTA.right, rPlayCTA.bottom), gBrushBlack.Get());
        if (hoverCTA) {
            gRenderTarget->DrawRectangle(rPlayCTA, gBrushAccentOrange.Get(), 2.0f);
        }
    } else {
        // Translucent muted CTA
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rPlayCTA, 2.0f, 2.0f), gBrushDim.Get());
        gRenderTarget->DrawText(L"PLAY", 4, gFormatNavTabActive.Get(), D2D1::RectF(rPlayCTA.left, rPlayCTA.top + 12.0f, rPlayCTA.right, rPlayCTA.bottom), gBrushMuted.Get());
    }

    // Inline File Status Warning Banner (Shown when game files are missing or damaged)
    heroY += 52.0f;
    D2D1_RECT_F rBanner = D2D1::RectF(heroX, heroY, heroX + 540.0f, heroY + 42.0f);
    bool hoverBanner = IsHovered(rBanner);

    if (!valid) {
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBanner, 4.0f, 4.0f), hoverBanner ? gBrushCardActiveBg.Get() : gBrushCardBg.Get());
        gRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rBanner, 4.0f, 4.0f), gBrushProblem.Get(), 1.0f);

        // Warning symbol
        gRenderTarget->DrawText(L"\u26A0", 1, gFormatNavTabActive.Get(), D2D1::RectF(rBanner.left + 12.0f, rBanner.top + 10.0f, rBanner.left + 32.0f, rBanner.bottom), gBrushProblem.Get());
        // Banner Text
        gRenderTarget->DrawText(L"Some game files are damaged", 27, gFormatBodySemibold.Get(), D2D1::RectF(rBanner.left + 36.0f, rBanner.top + 4.0f, rBanner.right, rBanner.top + 22.0f), gBrushWhite.Get());
        gRenderTarget->DrawText(L"Found your game folder, but it can't be used yet. Click to see why and how to fix it.", 84, gFormatSub.Get(), D2D1::RectF(rBanner.left + 36.0f, rBanner.top + 22.0f, rBanner.right, rBanner.bottom), gBrushMuted.Get());
    } else {
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBanner, 4.0f, 4.0f), gBrushCardBg.Get());
        gRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rBanner, 4.0f, 4.0f), gBrushOk.Get(), 1.0f);
        gRenderTarget->DrawText(L"\u2713", 1, gFormatNavTabActive.Get(), D2D1::RectF(rBanner.left + 12.0f, rBanner.top + 10.0f, rBanner.left + 32.0f, rBanner.bottom), gBrushOk.Get());
        gRenderTarget->DrawText(L"All game files verified", 23, gFormatBodySemibold.Get(), D2D1::RectF(rBanner.left + 36.0f, rBanner.top + 4.0f, rBanner.right, rBanner.top + 22.0f), gBrushWhite.Get());
        gRenderTarget->DrawText(L"All 21 GB assets present and verified. Ready to launch.", 54, gFormatSub.Get(), D2D1::RectF(rBanner.left + 36.0f, rBanner.top + 22.0f, rBanner.right, rBanner.bottom), gBrushMuted.Get());
    }
}

// ----------------------------------------------------------------------------
// Screen 2: Settings Overlay (GTA V Pause-Menu 2-Column Split)
// ----------------------------------------------------------------------------
static void RenderScreenSettings(D2D1_SIZE_F size) {
    // 1. Dimmed Street Backdrop
    if (gBmpSettingsBg) {
        DrawAspectFillBitmap(gBmpSettingsBg.Get(), D2D1::RectF(0, 0, size.width, size.height), 0.55f);
    }
    gRenderTarget->FillRectangle(D2D1::RectF(0, 0, size.width, size.height), gBrushDarkNight.Get());

    // 2. Centered Container (74% width, max 1120 DIPs, 82% height)
    float containerW = (std::min)(size.width * 0.74f, 1120.0f);
    float containerH = size.height * 0.82f;
    float cx = (size.width - containerW) * 0.5f;
    float cy = (size.height - containerH) * 0.5f;
    D2D1_RECT_F rBox = D2D1::RectF(cx, cy, cx + containerW, cy + containerH);

    // Box background
    gRenderTarget->FillRectangle(rBox, gBrushCardBg.Get());
    gRenderTarget->DrawRectangle(rBox, gBrushPanelStroke.Get(), 1.0f);

    // Top Accent Stripe (4 DIPs blue #4A90E2)
    gRenderTarget->FillRectangle(D2D1::RectF(cx, cy, cx + containerW, cy + 4.0f), gBrushAccentBlue.Get());

    // Header Bar (38 DIPs white #FFFFFF)
    D2D1_RECT_F rHeader = D2D1::RectF(cx, cy + 4.0f, cx + containerW, cy + 42.0f);
    gRenderTarget->FillRectangle(rHeader, gBrushWhite.Get());
    gRenderTarget->DrawText(L"SETTINGS", 8, gFormatSectionTitle.Get(), D2D1::RectF(cx, cy + 10.0f, cx + containerW, cy + 38.0f), gBrushBlack.Get());

    // Split Body
    float bodyY = cy + 42.0f;
    float footerH = 44.0f;
    float bodyH = containerH - 42.0f - footerH;

    // Left Sidebar (240 DIPs wide)
    float sideW = 240.0f;
    D2D1_RECT_F rSidebar = D2D1::RectF(cx, bodyY, cx + sideW, bodyY + bodyH);
    gRenderTarget->FillRectangle(rSidebar, gBrushCardActiveBg.Get());
    gRenderTarget->DrawLine(D2D1::Point2F(cx + sideW, bodyY), D2D1::Point2F(cx + sideW, bodyY + bodyH), gBrushPanelStroke.Get());

    // Sidebar 9 Tabs
    float tabH = 38.0f;
    for (int i = 0; i < 9; ++i) {
        float ty = bodyY + i * tabH;
        D2D1_RECT_F rTab = D2D1::RectF(cx, ty, cx + sideW, ty + tabH);
        bool isActive = (gActiveSettingsTab == i);
        bool isHover = IsHovered(rTab);

        if (isActive) {
            gRenderTarget->FillRectangle(rTab, gBrushWhite.Get());
            gRenderTarget->DrawText(kSidebarTabs[i], (UINT32)wcslen(kSidebarTabs[i]), gFormatNavTabActive.Get(), D2D1::RectF(cx + 20.0f, ty + 9.0f, cx + sideW, ty + tabH), gBrushBlack.Get());
        } else {
            if (isHover) gRenderTarget->FillRectangle(rTab, gBrushPanelStroke.Get());
            gRenderTarget->DrawText(kSidebarTabs[i], (UINT32)wcslen(kSidebarTabs[i]), gFormatNavTab.Get(), D2D1::RectF(cx + 20.0f, ty + 9.0f, cx + sideW, ty + tabH), gBrushMuted.Get());
        }
    }

    // Right Content Panel
    float contentX = cx + sideW + 28.0f;
    float contentW = containerW - sideW - 56.0f;
    float curY = bodyY + 20.0f;

    if (gActiveSettingsTab == 0) { // Graphics Tab
        gRenderTarget->DrawText(L"GRAPHICS", 8, gFormatSectionTitle.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 30.0f), gBrushWhite.Get());
        curY += 34.0f;

        // Presets Heading
        gRenderTarget->DrawText(L"PRESETS", 7, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 200.0f, curY + 20.0f), gBrushAccentOrange.Get());
        curY += 24.0f;

        // Presets List
        for (int p = 0; p < 7; ++p) {
            D2D1_RECT_F rPreset = D2D1::RectF(contentX, curY, contentX + contentW, curY + 32.0f);
            bool isPActive = (gActivePreset == p);
            bool isPHover = IsHovered(rPreset);

            if (isPActive) {
                gRenderTarget->FillRectangle(rPreset, gBrushPanelStroke.Get());
            } else if (isPHover) {
                gRenderTarget->FillRectangle(rPreset, gBrushCardBg.Get());
            }

            gRenderTarget->DrawText(kPresetNames[p], (UINT32)wcslen(kPresetNames[p]), gFormatBodySemibold.Get(), D2D1::RectF(contentX + 12.0f, curY + 7.0f, contentX + 300.0f, curY + 30.0f), isPActive ? gBrushWhite.Get() : gBrushMuted.Get());
            if (isPActive) {
                gRenderTarget->DrawText(L"\u2713", 1, gFormatBodySemibold.Get(), D2D1::RectF(contentX + contentW - 24.0f, curY + 7.0f, contentX + contentW, curY + 30.0f), gBrushOk.Get());
            }
            curY += 34.0f;
        }

        // Detailed Settings
        curY += 12.0f;
        gRenderTarget->DrawText(L"DETAILED SETTINGS", 17, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 20.0f), gBrushAccentOrange.Get());
        curY += 24.0f;

        // Render Carousel Rows
        for (size_t r = 0; r < gGraphicsRows.size(); ++r) {
            OptionGroup* og = gGraphicsRows[r];
            D2D1_RECT_F rRow = D2D1::RectF(contentX, curY, contentX + contentW, curY + 32.0f);
            gRenderTarget->DrawText(og->name, (UINT32)wcslen(og->name), gFormatBody.Get(), D2D1::RectF(contentX + 8.0f, curY + 6.0f, contentX + 250.0f, curY + 30.0f), gBrushWhite.Get());

            // Carousel control: < Val >
            float ctrlW = 180.0f;
            float ctrlX = contentX + contentW - ctrlW;
            D2D1_RECT_F rPrev = D2D1::RectF(ctrlX, curY + 4.0f, ctrlX + 24.0f, curY + 28.0f);
            D2D1_RECT_F rNext = D2D1::RectF(ctrlX + ctrlW - 24.0f, curY + 4.0f, ctrlX + ctrlW, curY + 28.0f);

            gRenderTarget->DrawText(L"<", 1, gFormatNavTabActive.Get(), rPrev, IsHovered(rPrev) ? gBrushWhite.Get() : gBrushDim.Get());
            const std::wstring& valStr = og->options[og->currentIndex];
            gRenderTarget->DrawText(valStr.c_str(), (UINT32)valStr.length(), gFormatBodySemibold.Get(), D2D1::RectF(ctrlX + 26.0f, curY + 6.0f, ctrlX + ctrlW - 26.0f, curY + 30.0f), gBrushWhite.Get());
            gRenderTarget->DrawText(L">", 1, gFormatNavTabActive.Get(), rNext, IsHovered(rNext) ? gBrushWhite.Get() : gBrushDim.Get());

            gRenderTarget->DrawLine(D2D1::Point2F(contentX, curY + 32.0f), D2D1::Point2F(contentX + contentW, curY + 32.0f), gBrushPanelStroke.Get(), 0.5f);
            curY += 34.0f;
        }
    } else if (gActiveSettingsTab == 1) { // Controls
        gRenderTarget->DrawText(L"CONTROLS", 8, gFormatSectionTitle.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 30.0f), gBrushWhite.Get());
        curY += 36.0f;
        gRenderTarget->DrawText(L"Configure controller and keyboard mappings for Grand Theft Auto V.", 66, gFormatBody.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 30.0f), gBrushMuted.Get());
        curY += 40.0f;
        gRenderTarget->DrawText(L"Controller: Xbox Controller (XInput Native)", 41, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 30.0f), gBrushWhite.Get());
        curY += 30.0f;
        gRenderTarget->DrawText(L"Mouse Sensitivity: 100%", 23, gFormatBody.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 30.0f), gBrushWhite.Get());
        curY += 30.0f;
        gRenderTarget->DrawText(L"Invert Y-Axis: Off", 18, gFormatBody.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 30.0f), gBrushWhite.Get());
    } else if (gActiveSettingsTab == 3) { // Game Files
        gRenderTarget->DrawText(L"GAME FILES", 10, gFormatSectionTitle.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 30.0f), gBrushWhite.Get());
        curY += 36.0f;
        gRenderTarget->DrawText(L"Current game directory:", 23, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 24.0f), gBrushAccentOrange.Get());
        curY += 26.0f;

        std::wstring wFolder(gFolderDisplay.begin(), gFolderDisplay.end());
        gRenderTarget->DrawText(wFolder.c_str(), (UINT32)wFolder.length(), gFormatBody.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 24.0f), gBrushWhite.Get());
        curY += 36.0f;

        D2D1_RECT_F rBtnBrowse = D2D1::RectF(contentX, curY, contentX + 160.0f, curY + 36.0f);
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBtnBrowse, 2.0f, 2.0f), IsHovered(rBtnBrowse) ? gBrushWhite.Get() : gBrushCardActiveBg.Get());
        gRenderTarget->DrawText(L"CHANGE FOLDER...", 16, gFormatBodySemibold.Get(), D2D1::RectF(rBtnBrowse.left + 14.0f, rBtnBrowse.top + 8.0f, rBtnBrowse.right, rBtnBrowse.bottom), IsHovered(rBtnBrowse) ? gBrushBlack.Get() : gBrushWhite.Get());

        D2D1_RECT_F rBtnCheck = D2D1::RectF(contentX + 170.0f, curY, contentX + 300.0f, curY + 36.0f);
        gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBtnCheck, 2.0f, 2.0f), IsHovered(rBtnCheck) ? gBrushWhite.Get() : gBrushCardActiveBg.Get());
        gRenderTarget->DrawText(L"CHECK FILES", 11, gFormatBodySemibold.Get(), D2D1::RectF(rBtnCheck.left + 16.0f, rBtnCheck.top + 8.0f, rBtnCheck.right, rBtnCheck.bottom), IsHovered(rBtnCheck) ? gBrushBlack.Get() : gBrushWhite.Get());
    } else if (gActiveSettingsTab == 7) { // This Device
        gRenderTarget->DrawText(L"THIS DEVICE", 11, gFormatSectionTitle.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 30.0f), gBrushWhite.Get());
        curY += 40.0f;

        std::wstring wCpu(gCpuName.begin(), gCpuName.end());
        std::wstring wRam(gRamInfo.begin(), gRamInfo.end());
        std::wstring wGpu(gGpuName.begin(), gGpuName.end());

        gRenderTarget->DrawText(L"CPU:", 4, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 100.0f, curY + 24.0f), gBrushAccentOrange.Get());
        gRenderTarget->DrawText(wCpu.c_str(), (UINT32)wCpu.length(), gFormatBody.Get(), D2D1::RectF(contentX + 80.0f, curY, contentX + contentW, curY + 24.0f), gBrushWhite.Get());
        curY += 30.0f;

        gRenderTarget->DrawText(L"RAM:", 4, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 100.0f, curY + 24.0f), gBrushAccentOrange.Get());
        gRenderTarget->DrawText(wRam.c_str(), (UINT32)wRam.length(), gFormatBody.Get(), D2D1::RectF(contentX + 80.0f, curY, contentX + contentW, curY + 24.0f), gBrushWhite.Get());
        curY += 30.0f;

        gRenderTarget->DrawText(L"GPU:", 4, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 100.0f, curY + 24.0f), gBrushAccentOrange.Get());
        gRenderTarget->DrawText(wGpu.c_str(), (UINT32)wGpu.length(), gFormatBody.Get(), D2D1::RectF(contentX + 80.0f, curY, contentX + contentW, curY + 24.0f), gBrushWhite.Get());
        curY += 30.0f;

        gRenderTarget->DrawText(L"Build:", 6, gFormatBodySemibold.Get(), D2D1::RectF(contentX, curY, contentX + 100.0f, curY + 24.0f), gBrushAccentOrange.Get());
        gRenderTarget->DrawText(L"Windows x64 Native Ahead-of-Time Engine", 39, gFormatBody.Get(), D2D1::RectF(contentX + 80.0f, curY, contentX + contentW, curY + 24.0f), gBrushWhite.Get());
    } else {
        // Default text for other tabs
        gRenderTarget->DrawText(kSidebarTabs[gActiveSettingsTab], (UINT32)wcslen(kSidebarTabs[gActiveSettingsTab]), gFormatSectionTitle.Get(), D2D1::RectF(contentX, curY, contentX + 300.0f, curY + 30.0f), gBrushWhite.Get());
        curY += 40.0f;
        gRenderTarget->DrawText(L"Grand Theft Auto V \u2014 Five Star Port for Windows.", 47, gFormatBody.Get(), D2D1::RectF(contentX, curY, contentX + contentW, curY + 24.0f), gBrushMuted.Get());
    }

    // Footer Bar
    D2D1_RECT_F rFooter = D2D1::RectF(cx, cy + containerH - footerH, cx + containerW, cy + containerH);
    gRenderTarget->FillRectangle(rFooter, gBrushCardActiveBg.Get());
    gRenderTarget->DrawLine(D2D1::Point2F(cx, cy + containerH - footerH), D2D1::Point2F(cx + containerW, cy + containerH - footerH), gBrushPanelStroke.Get());
    gRenderTarget->DrawText(L"Resolution changes apply right away. Presets apply when you press Play.", 69, gFormatSub.Get(), D2D1::RectF(cx + 20.0f, cy + containerH - 30.0f, cx + 600.0f, cy + containerH), gBrushDim.Get());

    // Bottom-Right BACK [ESC] Button
    D2D1_RECT_F rBack = D2D1::RectF(cx + containerW - 120.0f, cy + containerH - 34.0f, cx + containerW - 20.0f, cy + containerH - 10.0f);
    bool hoverBack = IsHovered(rBack);
    gRenderTarget->DrawText(L"BACK", 4, gFormatBodySemibold.Get(), D2D1::RectF(rBack.left, rBack.top + 3.0f, rBack.left + 50.0f, rBack.bottom), hoverBack ? gBrushWhite.Get() : gBrushMuted.Get());
    D2D1_RECT_F rKeycap = D2D1::RectF(rBack.left + 52.0f, rBack.top, rBack.right, rBack.bottom);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rKeycap, 2.0f, 2.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"[ESC]", 5, gFormatKeycap.Get(), D2D1::RectF(rKeycap.left, rKeycap.top + 4.0f, rKeycap.right, rKeycap.bottom), gBrushBlack.Get());
}

// ----------------------------------------------------------------------------
// Screen 3: Game Files Manager (`SETTINGS -> Game files` & `ONLINE`)
// ----------------------------------------------------------------------------
static void RenderScreenGameFiles(D2D1_SIZE_F size) {
    if (gBmpSettingsBg) {
        DrawAspectFillBitmap(gBmpSettingsBg.Get(), D2D1::RectF(0, 0, size.width, size.height), 0.50f);
    }
    gRenderTarget->FillRectangle(D2D1::RectF(0, 0, size.width, size.height), gBrushDarkNight.Get());

    float modalW = (std::min)(size.width * 0.60f, 620.0f);
    float modalH = size.height * 0.84f;
    float mx = (size.width - modalW) * 0.5f;
    float my = (size.height - modalH) * 0.5f;
    D2D1_RECT_F rModal = D2D1::RectF(mx, my, mx + modalW, my + modalH);

    gRenderTarget->FillRectangle(rModal, gBrushCardBg.Get());
    gRenderTarget->DrawRectangle(rModal, gBrushPanelStroke.Get(), 1.0f);

    // Accent line
    gRenderTarget->FillRectangle(D2D1::RectF(mx, my, mx + modalW, my + 4.0f), gBrushAccentBlue.Get());

    // Header bar
    gRenderTarget->FillRectangle(D2D1::RectF(mx, my + 4.0f, mx + modalW, my + 42.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"GAME FILES", 10, gFormatSectionTitle.Get(), D2D1::RectF(mx, my + 10.0f, mx + modalW, my + 38.0f), gBrushBlack.Get());

    // 3-Step Progress Bar
    float stepH = 34.0f;
    float stepW = modalW / 3.0f;
    float stepY = my + 42.0f;

    D2D1_RECT_F rStep1 = D2D1::RectF(mx, stepY, mx + stepW, stepY + stepH);
    D2D1_RECT_F rStep2 = D2D1::RectF(mx + stepW, stepY, mx + 2 * stepW, stepY + stepH);
    D2D1_RECT_F rStep3 = D2D1::RectF(mx + 2 * stepW, stepY, mx + modalW, stepY + stepH);

    gRenderTarget->FillRectangle(rStep1, gBrushWhite.Get());
    gRenderTarget->DrawText(L"1 GET", 5, gFormatNavTabActive.Get(), D2D1::RectF(rStep1.left, rStep1.top + 7.0f, rStep1.right, rStep1.bottom), gBrushBlack.Get());

    gRenderTarget->FillRectangle(rStep2, gBrushCardActiveBg.Get());
    gRenderTarget->DrawText(L"2 SET UP", 8, gFormatNavTab.Get(), D2D1::RectF(rStep2.left, rStep2.top + 7.0f, rStep2.right, rStep2.bottom), gBrushDim.Get());

    gRenderTarget->FillRectangle(rStep3, gBrushCardActiveBg.Get());
    gRenderTarget->DrawText(L"3 READY", 7, gFormatNavTab.Get(), D2D1::RectF(rStep3.left, rStep3.top + 7.0f, rStep3.right, rStep3.bottom), gBrushDim.Get());

    // Scrollable Body
    float curY = stepY + stepH + 24.0f;
    float padX = mx + 28.0f;
    float bodyW = modalW - 56.0f;

    gRenderTarget->DrawText(L"GET THE GAME FILES", 18, gFormatSectionTitle.Get(), D2D1::RectF(padX, curY, padX + bodyW, curY + 28.0f), gBrushWhite.Get());
    curY += 30.0f;
    gRenderTarget->DrawText(L"Choose a source or select an existing folder. Five Star Port checks every file for you.", 85, gFormatSub.Get(), D2D1::RectF(padX, curY, padX + bodyW, curY + 20.0f), gBrushMuted.Get());
    curY += 28.0f;

    // Folder selection card
    D2D1_RECT_F rFoldCard = D2D1::RectF(padX, curY, padX + bodyW, curY + 54.0f);
    gRenderTarget->FillRectangle(rFoldCard, gBrushCardActiveBg.Get());
    gRenderTarget->DrawRectangle(rFoldCard, gBrushPanelStroke.Get(), 1.0f);

    gRenderTarget->DrawText(L"GAME FOLDER:", 12, gFormatMicro.Get(), D2D1::RectF(padX + 12.0f, curY + 8.0f, padX + 200.0f, curY + 22.0f), gBrushAccentOrange.Get());
    std::wstring wF(gFolderDisplay.begin(), gFolderDisplay.end());
    gRenderTarget->DrawText(wF.c_str(), (UINT32)wF.length(), gFormatSub.Get(), D2D1::RectF(padX + 12.0f, curY + 24.0f, padX + bodyW - 100.0f, curY + 48.0f), gBrushWhite.Get());

    D2D1_RECT_F rBtnBrowse = D2D1::RectF(padX + bodyW - 90.0f, curY + 12.0f, padX + bodyW - 10.0f, curY + 42.0f);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBtnBrowse, 2.0f, 2.0f), IsHovered(rBtnBrowse) ? gBrushWhite.Get() : gBrushCardBg.Get());
    gRenderTarget->DrawText(L"BROWSE...", 9, gFormatBodySemibold.Get(), D2D1::RectF(rBtnBrowse.left + 8.0f, rBtnBrowse.top + 6.0f, rBtnBrowse.right, rBtnBrowse.bottom), IsHovered(rBtnBrowse) ? gBrushBlack.Get() : gBrushWhite.Get());
    curY += 66.0f;

    // Diagnostics Alert Box
    D2D1_RECT_F rDiag = D2D1::RectF(padX, curY, padX + bodyW, curY + 100.0f);
    bool valid = gReport.isValid;

    if (!valid) {
        gRenderTarget->FillRectangle(rDiag, gBrushCardActiveBg.Get());
        gRenderTarget->DrawRectangle(rDiag, gBrushProblem.Get(), 1.0f);
        gRenderTarget->DrawText(L"\u26A0", 1, gFormatNavTabActive.Get(), D2D1::RectF(padX + 12.0f, curY + 10.0f, padX + 32.0f, curY + 34.0f), gBrushProblem.Get());
        gRenderTarget->DrawText(L"SOME GAME FILES ARE DAMAGED", 27, gFormatBodySemibold.Get(), D2D1::RectF(padX + 36.0f, curY + 10.0f, padX + bodyW, curY + 30.0f), gBrushWhite.Get());

        std::string missingStr = std::to_string(gReport.missingItems.size()) + " files missing or mismatched.";
        std::wstring wMissing(missingStr.begin(), missingStr.end());
        gRenderTarget->DrawText(wMissing.c_str(), (UINT32)wMissing.length(), gFormatSub.Get(), D2D1::RectF(padX + 36.0f, curY + 32.0f, padX + bodyW, curY + 50.0f), gBrushMuted.Get());

        // SHOW EVERY PROBLEM Button
        D2D1_RECT_F rBtnProb = D2D1::RectF(padX + 12.0f, curY + 60.0f, padX + bodyW - 12.0f, curY + 88.0f);
        bool hoverProb = IsHovered(rBtnProb);
        gRenderTarget->DrawRectangle(rBtnProb, hoverProb ? gBrushWhite.Get() : gBrushPanelStroke.Get(), 1.0f);
        gRenderTarget->DrawText(L"SHOW EVERY PROBLEM", 18, gFormatBodySemibold.Get(), D2D1::RectF(rBtnProb.left, rBtnProb.top + 5.0f, rBtnProb.right, rBtnProb.bottom), hoverProb ? gBrushWhite.Get() : gBrushMuted.Get());
    } else {
        gRenderTarget->FillRectangle(rDiag, gBrushCardActiveBg.Get());
        gRenderTarget->DrawRectangle(rDiag, gBrushOk.Get(), 1.0f);
        gRenderTarget->DrawText(L"\u2713", 1, gFormatNavTabActive.Get(), D2D1::RectF(padX + 12.0f, curY + 10.0f, padX + 32.0f, curY + 34.0f), gBrushOk.Get());
        gRenderTarget->DrawText(L"ALL GAME FILES VERIFIED", 23, gFormatBodySemibold.Get(), D2D1::RectF(padX + 36.0f, curY + 10.0f, padX + bodyW, curY + 30.0f), gBrushWhite.Get());
        gRenderTarget->DrawText(L"Your Grand Theft Auto V installation is complete and ready to run.", 65, gFormatSub.Get(), D2D1::RectF(padX + 36.0f, curY + 32.0f, padX + bodyW, curY + 50.0f), gBrushMuted.Get());
    }
    curY += 114.0f;

    // Recommended Source
    gRenderTarget->DrawText(L"RECOMMENDED SOURCE", 18, gFormatMicro.Get(), D2D1::RectF(padX, curY, padX + 200.0f, curY + 18.0f), gBrushAccentOrange.Get());
    gRenderTarget->DrawText(L"dropdrive", 9, gFormatBodySemibold.Get(), D2D1::RectF(padX + bodyW - 80.0f, curY, padX + bodyW, curY + 18.0f), gBrushWhite.Get());
    curY += 22.0f;
    gRenderTarget->DrawText(L"The app downloads, unpacks and checks every file for you.", 56, gFormatSub.Get(), D2D1::RectF(padX, curY, padX + bodyW, curY + 20.0f), gBrushMuted.Get());
    curY += 24.0f;

    D2D1_RECT_F rBtnDrop = D2D1::RectF(padX, curY, padX + bodyW, curY + 38.0f);
    bool hoverDrop = IsHovered(rBtnDrop);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBtnDrop, 2.0f, 2.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"DOWNLOAD FROM DROPDRIVE", 23, gFormatNavTabActive.Get(), D2D1::RectF(rBtnDrop.left, rBtnDrop.top + 9.0f, rBtnDrop.right, rBtnDrop.bottom), gBrushBlack.Get());
    if (hoverDrop) gRenderTarget->DrawRectangle(rBtnDrop, gBrushAccentOrange.Get(), 2.0f);
    curY += 50.0f;

    // Other mirrors
    gRenderTarget->DrawText(L"OTHER DOWNLOAD OPTIONS", 22, gFormatMicro.Get(), D2D1::RectF(padX, curY, padX + bodyW, curY + 18.0f), gBrushDim.Get());
    curY += 20.0f;

    D2D1_RECT_F rBtnUpload = D2D1::RectF(padX, curY, padX + bodyW, curY + 32.0f);
    gRenderTarget->DrawRectangle(rBtnUpload, IsHovered(rBtnUpload) ? gBrushWhite.Get() : gBrushPanelStroke.Get(), 1.0f);
    gRenderTarget->DrawText(L"DOWNLOAD FROM UPLOADHIVE", 24, gFormatBodySemibold.Get(), D2D1::RectF(padX, curY + 6.0f, padX + bodyW, curY + 32.0f), gBrushWhite.Get());
    curY += 38.0f;

    D2D1_RECT_F rBtnTorrent = D2D1::RectF(padX, curY, padX + bodyW, curY + 32.0f);
    gRenderTarget->DrawRectangle(rBtnTorrent, IsHovered(rBtnTorrent) ? gBrushWhite.Get() : gBrushPanelStroke.Get(), 1.0f);
    gRenderTarget->DrawText(L"DOWNLOAD WITH TORRENT", 21, gFormatBodySemibold.Get(), D2D1::RectF(padX, curY + 6.0f, padX + bodyW, curY + 32.0f), gBrushWhite.Get());

    // Footer
    float footH = 44.0f;
    D2D1_RECT_F rFoot = D2D1::RectF(mx, my + modalH - footH, mx + modalW, my + modalH);
    gRenderTarget->FillRectangle(rFoot, gBrushCardActiveBg.Get());

    D2D1_RECT_F rRecheck = D2D1::RectF(mx + 20.0f, rFoot.top + 8.0f, mx + 200.0f, rFoot.bottom - 8.0f);
    gRenderTarget->DrawText(L"CHECK EVERY FILE AGAIN", 22, gFormatBodySemibold.Get(), rRecheck, IsHovered(rRecheck) ? gBrushWhite.Get() : gBrushMuted.Get());

    D2D1_RECT_F rBack = D2D1::RectF(mx + modalW - 120.0f, rFoot.top + 8.0f, mx + modalW - 20.0f, rFoot.bottom - 8.0f);
    gRenderTarget->DrawText(L"BACK", 4, gFormatBodySemibold.Get(), D2D1::RectF(rBack.left, rBack.top + 3.0f, rBack.left + 50.0f, rBack.bottom), IsHovered(rBack) ? gBrushWhite.Get() : gBrushMuted.Get());
    D2D1_RECT_F rKeycap = D2D1::RectF(rBack.left + 52.0f, rBack.top, rBack.right, rBack.bottom);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rKeycap, 2.0f, 2.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"[ESC]", 5, gFormatKeycap.Get(), D2D1::RectF(rKeycap.left, rKeycap.top + 4.0f, rKeycap.right, rKeycap.bottom), gBrushBlack.Get());
}

// ----------------------------------------------------------------------------
// Problem Details Modal Dialog
// ----------------------------------------------------------------------------
static void RenderProblemModal(D2D1_SIZE_F size) {
    // Dim background
    gRenderTarget->FillRectangle(D2D1::RectF(0, 0, size.width, size.height), D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.82f));

    float dlgW = (std::min)(size.width * 0.70f, 700.0f);
    float dlgH = (std::min)(size.height * 0.75f, 500.0f);
    float dx = (size.width - dlgW) * 0.5f;
    float dy = (size.height - dlgH) * 0.5f;
    D2D1_RECT_F rDlg = D2D1::RectF(dx, dy, dx + dlgW, dy + dlgH);

    gRenderTarget->FillRectangle(rDlg, gBrushCardBg.Get());
    gRenderTarget->DrawRectangle(rDlg, gBrushPanelStroke.Get(), 1.0f);

    // Header
    D2D1_RECT_F rH = D2D1::RectF(dx, dy, dx + dlgW, dy + 42.0f);
    gRenderTarget->FillRectangle(rH, gBrushCardActiveBg.Get());
    gRenderTarget->DrawText(L"FILE INTEGRITY REPORT", 21, gFormatNavTabActive.Get(), D2D1::RectF(dx + 20.0f, dy + 11.0f, dx + dlgW, dy + 42.0f), gBrushWhite.Get());

    // Close X
    D2D1_RECT_F rClose = D2D1::RectF(dx + dlgW - 36.0f, dy + 8.0f, dx + dlgW - 10.0f, dy + 34.0f);
    gRenderTarget->DrawText(L"\u2715", 1, gFormatNavTabActive.Get(), rClose, IsHovered(rClose) ? gBrushWhite.Get() : gBrushDim.Get());

    // Missing Items List
    float listY = dy + 56.0f;
    for (size_t i = 0; i < gReport.missingItems.size() && i < 10; ++i) {
        D2D1_RECT_F rItem = D2D1::RectF(dx + 20.0f, listY, dx + dlgW - 20.0f, listY + 32.0f);
        gRenderTarget->FillRectangle(rItem, gBrushCardActiveBg.Get());
        gRenderTarget->FillRectangle(D2D1::RectF(rItem.left, rItem.top, rItem.left + 3.0f, rItem.bottom), gBrushProblem.Get());

        std::string s = gReport.missingItems[i];
        std::wstring ws(s.begin(), s.end());
        gRenderTarget->DrawText(ws.c_str(), (UINT32)ws.length(), gFormatSub.Get(), D2D1::RectF(rItem.left + 12.0f, rItem.top + 6.0f, rItem.right, rItem.bottom), gBrushWhite.Get());
        listY += 36.0f;
    }

    if (gReport.missingItems.empty()) {
        gRenderTarget->DrawText(L"No missing files detected in selected directory.", 47, gFormatBody.Get(), D2D1::RectF(dx + 20.0f, listY, dx + dlgW, listY + 30.0f), gBrushOk.Get());
    }

    // Footer Close Button
    D2D1_RECT_F rBtn = D2D1::RectF(dx + dlgW - 120.0f, dy + dlgH - 42.0f, dx + dlgW - 20.0f, dy + dlgH - 12.0f);
    gRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rBtn, 2.0f, 2.0f), gBrushWhite.Get());
    gRenderTarget->DrawText(L"CLOSE", 5, gFormatBodySemibold.Get(), D2D1::RectF(rBtn.left, rBtn.top + 6.0f, rBtn.right, rBtn.bottom), gBrushBlack.Get());
}

void Render() {
    if (!gRenderTarget || !gIsVisible) return;

    gRenderTarget->BeginDraw();
    D2D1_SIZE_F size = gRenderTarget->GetSize();

    if (gCurrentScreen == SCREEN_PLAY) {
        RenderScreenPlay(size);
    } else if (gCurrentScreen == SCREEN_SETTINGS) {
        RenderScreenSettings(size);
    } else if (gCurrentScreen == SCREEN_GAMEFILES) {
        RenderScreenGameFiles(size);
    }

    if (gShowProblemsModal) {
        RenderProblemModal(size);
    }

    HRESULT hr = gRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        CreateDeviceResources();
    }
}

bool ProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!gIsVisible) return false;

    switch (uMsg) {
        case WM_MOUSEMOVE: {
            gMouseX = (float)LOWORD(lParam);
            gMouseY = (float)HIWORD(lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }

        case WM_LBUTTONDOWN: {
            gMouseDown = true;
            D2D1_SIZE_F size = gRenderTarget ? gRenderTarget->GetSize() : D2D1::SizeF(1600, 900);

            if (gShowProblemsModal) {
                float dlgW = (std::min)(size.width * 0.70f, 700.0f);
                float dlgH = (std::min)(size.height * 0.75f, 500.0f);
                float dx = (size.width - dlgW) * 0.5f;
                float dy = (size.height - dlgH) * 0.5f;

                D2D1_RECT_F rClose = D2D1::RectF(dx + dlgW - 36.0f, dy + 8.0f, dx + dlgW - 10.0f, dy + 34.0f);
                D2D1_RECT_F rBtn = D2D1::RectF(dx + dlgW - 120.0f, dy + dlgH - 42.0f, dx + dlgW - 20.0f, dy + dlgH - 12.0f);
                if (IsHovered(rClose) || IsHovered(rBtn)) {
                    gShowProblemsModal = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return true;
            }

            if (gCurrentScreen == SCREEN_PLAY) {
                // Top nav tabs
                if (gMouseY >= 16.0f && gMouseY <= 50.0f) {
                    if (gMouseX >= 36.0f && gMouseX <= 108.0f) {
                        gCurrentScreen = SCREEN_PLAY;
                    } else if (gMouseX >= 122.0f && gMouseX <= 232.0f) {
                        gCurrentScreen = SCREEN_SETTINGS;
                        gActiveSettingsTab = 6; // Online
                    } else if (gMouseX >= 244.0f && gMouseX <= 334.0f) {
                        gCurrentScreen = SCREEN_SETTINGS;
                    }

                    // External URLs
                    float rightX = size.width - 260.0f;
                    if (gMouseX >= rightX && gMouseX <= rightX + 60.0f) {
                        ShellExecuteW(nullptr, L"open", L"https://github.com/kevinerikjs/gta5-macos-port", nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (gMouseX >= rightX + 80.0f && gMouseX <= rightX + 155.0f) {
                        ShellExecuteW(nullptr, L"open", L"https://t.me/fivestarport", nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (gMouseX >= rightX + 174.0f && gMouseX <= rightX + 240.0f) {
                        ShellExecuteW(nullptr, L"open", L"https://discord.gg/fivestarport", nullptr, nullptr, SW_SHOWNORMAL);
                    }
                }

                // Bottom 5 cards
                float cardGap = 14.0f;
                float marginX = 36.0f;
                float cardStripY = size.height - 180.0f;
                float cardH = 144.0f;
                float cardW = (size.width - 2.0f * marginX - 4.0f * cardGap) / 5.0f;

                if (gMouseY >= cardStripY && gMouseY <= cardStripY + cardH) {
                    for (int i = 0; i < 5; ++i) {
                        float cx = marginX + i * (cardW + cardGap);
                        if (gMouseX >= cx && gMouseX <= cx + cardW) {
                            gSelectedMode = i;
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return true;
                        }
                    }
                }

                // Sandbox Map Chips
                float heroY = cardStripY - 210.0f + 56.0f + 46.0f;
                if (gSelectedMode == 1) {
                    heroY += 40.0f;
                    D2D1_RECT_F rChip0 = D2D1::RectF(40.0f, heroY, 140.0f, heroY + 28.0f);
                    D2D1_RECT_F rChip1 = D2D1::RectF(150.0f, heroY, 280.0f, heroY + 28.0f);
                    if (IsHovered(rChip0)) gSandboxMap = 0;
                    if (IsHovered(rChip1)) gSandboxMap = 1;
                }

                // PLAY CTA Button
                heroY += (gSelectedMode == 1 ? 38.0f : 44.0f);
                D2D1_RECT_F rPlayCTA = D2D1::RectF(40.0f, heroY, 190.0f, heroY + 44.0f);
                if (IsHovered(rPlayCTA)) {
                    if (gReport.isValid) {
                        WinNativeLaunchArgs args;
                        args.mode = (gSelectedMode == 0) ? "story" : (gSelectedMode == 1 ? "sandbox" : "performance");
                        args.sandboxMap = gSandboxMap;
                        args.preset = gActivePreset;
                        SetVisible(false);
                        if (gLaunchHandler) gLaunchHandler(args);
                    } else {
                        gCurrentScreen = SCREEN_GAMEFILES;
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }

                // Warning Banner -> Click goes to Screen 3
                heroY += 52.0f;
                D2D1_RECT_F rBanner = D2D1::RectF(40.0f, heroY, 580.0f, heroY + 42.0f);
                if (IsHovered(rBanner)) {
                    gCurrentScreen = SCREEN_GAMEFILES;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
            } else if (gCurrentScreen == SCREEN_SETTINGS) {
                float containerW = (std::min)(size.width * 0.74f, 1120.0f);
                float containerH = size.height * 0.82f;
                float cx = (size.width - containerW) * 0.5f;
                float cy = (size.height - containerH) * 0.5f;

                // Back button
                D2D1_RECT_F rBack = D2D1::RectF(cx + containerW - 120.0f, cy + containerH - 34.0f, cx + containerW - 20.0f, cy + containerH - 10.0f);
                if (IsHovered(rBack)) {
                    gCurrentScreen = SCREEN_PLAY;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }

                // Sidebar tabs
                float bodyY = cy + 42.0f;
                float sideW = 240.0f;
                if (gMouseX >= cx && gMouseX <= cx + sideW && gMouseY >= bodyY && gMouseY <= cy + containerH - 44.0f) {
                    int clickedTab = (int)((gMouseY - bodyY) / 38.0f);
                    if (clickedTab >= 0 && clickedTab < 9) {
                        gActiveSettingsTab = clickedTab;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return true;
                    }
                }

                // Graphics Presets & Carousels
                if (gActiveSettingsTab == 0) {
                    float contentX = cx + sideW + 28.0f;
                    float contentW = containerW - sideW - 56.0f;
                    float curY = bodyY + 20.0f + 34.0f + 24.0f;

                    // Presets
                    for (int p = 0; p < 7; ++p) {
                        D2D1_RECT_F rPreset = D2D1::RectF(contentX, curY, contentX + contentW, curY + 32.0f);
                        if (IsHovered(rPreset)) {
                            gActivePreset = p;
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return true;
                        }
                        curY += 34.0f;
                    }

                    // Carousels
                    curY += 12.0f + 24.0f;
                    for (size_t r = 0; r < gGraphicsRows.size(); ++r) {
                        OptionGroup* og = gGraphicsRows[r];
                        float ctrlW = 180.0f;
                        float ctrlX = contentX + contentW - ctrlW;
                        D2D1_RECT_F rPrev = D2D1::RectF(ctrlX, curY + 4.0f, ctrlX + 24.0f, curY + 28.0f);
                        D2D1_RECT_F rNext = D2D1::RectF(ctrlX + ctrlW - 24.0f, curY + 4.0f, ctrlX + ctrlW, curY + 28.0f);

                        if (IsHovered(rPrev)) {
                            og->currentIndex = (og->currentIndex - 1 + (int)og->options.size()) % (int)og->options.size();
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return true;
                        }
                        if (IsHovered(rNext)) {
                            og->currentIndex = (og->currentIndex + 1) % (int)og->options.size();
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return true;
                        }
                        curY += 34.0f;
                    }
                } else if (gActiveSettingsTab == 3) { // Game Files
                    float contentX = cx + sideW + 28.0f;
                    float curY = bodyY + 20.0f + 36.0f + 26.0f + 24.0f + 36.0f;
                    D2D1_RECT_F rBtnBrowse = D2D1::RectF(contentX, curY, contentX + 160.0f, curY + 36.0f);
                    D2D1_RECT_F rBtnCheck = D2D1::RectF(contentX + 170.0f, curY, contentX + 300.0f, curY + 36.0f);

                    if (IsHovered(rBtnBrowse)) {
                        std::string chosen;
                        if (WinFiles::PromptChooseGameFolder(hwnd, chosen)) {
                            WinFiles::SaveGameFolder(chosen);
                            RefreshStatus();
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                        return true;
                    }
                    if (IsHovered(rBtnCheck)) {
                        RefreshStatus();
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return true;
                    }
                }
            } else if (gCurrentScreen == SCREEN_GAMEFILES) {
                float modalW = (std::min)(size.width * 0.60f, 620.0f);
                float modalH = size.height * 0.84f;
                float mx = (size.width - modalW) * 0.5f;
                float my = (size.height - modalH) * 0.5f;

                // Back button
                float footH = 44.0f;
                D2D1_RECT_F rBack = D2D1::RectF(mx + modalW - 120.0f, my + modalH - footH + 8.0f, mx + modalW - 20.0f, my + modalH - 8.0f);
                if (IsHovered(rBack)) {
                    gCurrentScreen = SCREEN_PLAY;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }

                // Check again
                D2D1_RECT_F rRecheck = D2D1::RectF(mx + 20.0f, my + modalH - footH + 8.0f, mx + 200.0f, my + modalH - 8.0f);
                if (IsHovered(rRecheck)) {
                    RefreshStatus();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }

                // Browse folder
                float padX = mx + 28.0f;
                float bodyW = modalW - 56.0f;
                float curY = my + 42.0f + 34.0f + 24.0f + 30.0f + 28.0f;
                D2D1_RECT_F rBtnBrowse = D2D1::RectF(padX + bodyW - 90.0f, curY + 12.0f, padX + bodyW - 10.0f, curY + 42.0f);
                if (IsHovered(rBtnBrowse)) {
                    std::string chosen;
                    if (WinFiles::PromptChooseGameFolder(hwnd, chosen)) {
                        WinFiles::SaveGameFolder(chosen);
                        RefreshStatus();
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return true;
                }

                // Show every problem
                curY += 66.0f;
                D2D1_RECT_F rBtnProb = D2D1::RectF(padX + 12.0f, curY + 60.0f, padX + bodyW - 12.0f, curY + 88.0f);
                if (!gReport.isValid && IsHovered(rBtnProb)) {
                    gShowProblemsModal = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }

                // Dropdrive CTA
                curY += 114.0f + 22.0f + 24.0f;
                D2D1_RECT_F rBtnDrop = D2D1::RectF(padX, curY, padX + bodyW, curY + 38.0f);
                if (IsHovered(rBtnDrop)) {
                    ShellExecuteW(nullptr, L"open", L"https://dropdrive.net", nullptr, nullptr, SW_SHOWNORMAL);
                    return true;
                }
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }

        case WM_LBUTTONUP: {
            gMouseDown = false;
            return true;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                if (gShowProblemsModal) {
                    gShowProblemsModal = false;
                } else if (gCurrentScreen != SCREEN_PLAY) {
                    gCurrentScreen = SCREEN_PLAY;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return true;
            }

            if (gCurrentScreen == SCREEN_PLAY) {
                if (wParam == VK_RETURN) {
                    if (gReport.isValid) {
                        WinNativeLaunchArgs args;
                        args.mode = (gSelectedMode == 0) ? "story" : (gSelectedMode == 1 ? "sandbox" : "performance");
                        args.sandboxMap = gSandboxMap;
                        args.preset = gActivePreset;
                        SetVisible(false);
                        if (gLaunchHandler) gLaunchHandler(args);
                    } else {
                        gCurrentScreen = SCREEN_GAMEFILES;
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
                if (wParam == VK_RIGHT) {
                    gSelectedMode = (gSelectedMode + 1) % 5;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
                if (wParam == VK_LEFT) {
                    gSelectedMode = (gSelectedMode - 1 + 5) % 5;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
            } else if (gCurrentScreen == SCREEN_SETTINGS) {
                if (wParam == VK_UP) {
                    gActiveSettingsTab = (gActiveSettingsTab - 1 + 9) % 9;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
                if (wParam == VK_DOWN) {
                    gActiveSettingsTab = (gActiveSettingsTab + 1) % 9;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

} // namespace WinNativeUI

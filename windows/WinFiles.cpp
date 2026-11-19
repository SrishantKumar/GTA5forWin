#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WinFiles.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

std::string WinFiles::GetAppDataFolder() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        fs::path p(path);
        CoTaskMemFree(path);
        p /= "GTA5Port";
        std::error_code ec;
        fs::create_directories(p, ec);
        return p.string();
    }
    return "C:\\GTA5Port";
}

std::string WinFiles::GetLocalAppDataFolder() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        fs::path p(path);
        CoTaskMemFree(path);
        p /= "GTA5Port";
        std::error_code ec;
        fs::create_directories(p, ec);
        return p.string();
    }
    return GetAppDataFolder();
}

std::string WinFiles::GetConfigFilePath() {
    fs::path p = fs::path(GetLocalAppDataFolder()) / "config.ini";
    return p.string();
}

std::string WinFiles::GetDocumentsFolder() {
    fs::path p = fs::path(GetAppDataFolder()) / "Data";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p.string();
}

std::string WinFiles::GetModsFolder() {
    fs::path p = fs::path(GetDocumentsFolder()) / "Mods";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p.string();
}

std::string WinFiles::GetUserMusicFolder() {
    fs::path p = fs::path(GetDocumentsFolder()) / "Self Radio";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p.string();
}

void WinFiles::OpenFolderInExplorer(const std::string &path) {
    if (path.empty()) return;
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        fs::create_directories(path, ec);
    }
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void WinFiles::PrepareEnvironment(const std::string &filesRoot) {
    fs::path root(filesRoot);
    std::error_code ec;
    fs::create_directories(root / "userdata" / "Documents" / "Rockstar Games" / "GTA V", ec);
    fs::create_directories(root / "sandbox" / "Documents" / "Rockstar Games" / "GTA V", ec);
    fs::create_directories(root / "Self Radio", ec);
    fs::create_directories(root / "Mods", ec);

    // Clean stale graphics settings on start so app graphics options govern (matching NativePortViewController.swift)
    fs::remove(root / "userdata" / "Documents" / "Rockstar Games" / "GTA V" / "settings.xml", ec);
    fs::remove(root / "sandbox" / "Documents" / "Rockstar Games" / "GTA V" / "settings.xml", ec);
}

bool WinFiles::PromptChooseGameFolder(HWND hwnd, std::string &outPath) {
    ComPtr<IFileOpenDialog> pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr)) return false;

    FILEOPENDIALOGOPTIONS opt;
    pFileOpen->GetOptions(&opt);
    pFileOpen->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    pFileOpen->SetTitle(L"Select Grand Theft Auto V Game Folder");

    hr = pFileOpen->Show(hwnd);
    if (FAILED(hr)) return false;

    ComPtr<IShellItem> pItem;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) return false;

    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr)) return false;

    std::wstring wpath(pszFilePath);
    CoTaskMemFree(pszFilePath);

    int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &str[0], len, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') str.pop_back();

    outPath = str;
    return true;
}

WinGameFolderReport WinFiles::VerifyGameFolder(const std::string &path) {
    WinGameFolderReport report;
    report.folderPath = path;

    if (path.empty()) {
        report.errorMessage = "No folder selected. Please choose your GTA V game folder.";
        return report;
    }

    std::error_code ec;
    fs::path p(path);
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) {
        report.errorMessage = "Specified directory does not exist or is inaccessible.";
        return report;
    }

    report.hasEngine = fs::exists(p / "b" / "8b0b5899ed" / "game.wasm", ec);
    report.hasManifest = fs::exists(p / "data" / "manifest.json", ec);
    report.hasShaders = fs::exists(p / "b" / "8b0b5899ed" / "shaders" / "index.json", ec);
    report.hasGta5Exe = fs::exists(p / "GTA5.exe", ec) || fs::exists(p / "PlayGTAV.exe", ec);
    report.hasCommonRpf = fs::exists(p / "common.rpf", ec);
    report.hasUpdateRpf = fs::exists(p / "update" / "update.rpf", ec);

    // Count standard RPF archives
    report.foundRpfCount = 0;
    const char *rpfNames[] = {
        "x64a.rpf", "x64b.rpf", "x64c.rpf", "x64d.rpf", "x64e.rpf", "x64f.rpf",
        "x64g.rpf", "x64h.rpf", "x64i.rpf", "x64j.rpf", "x64k.rpf", "x64l.rpf",
        "x64m.rpf", "x64n.rpf", "x64o.rpf", "x64p.rpf", "x64q.rpf", "x64r.rpf",
        "x64s.rpf", "x64t.rpf", "x64u.rpf", "x64v.rpf", "x64w.rpf"
    };
    for (const char *name : rpfNames) {
        if (fs::exists(p / name, ec)) {
            report.foundRpfCount++;
        }
    }

    if (report.hasEngine) report.foundItems.push_back("Engine binary (b/8b0b5899ed/game.wasm)");
    else report.missingItems.push_back("Engine binary (b/8b0b5899ed/game.wasm)");

    if (report.hasManifest) report.foundItems.push_back("Asset manifest (data/manifest.json)");
    else report.missingItems.push_back("Asset manifest (data/manifest.json)");

    if (report.hasShaders) report.foundItems.push_back("Shader index (b/8b0b5899ed/shaders/index.json)");
    else report.missingItems.push_back("Shader index (b/8b0b5899ed/shaders/index.json)");

    if (report.hasGta5Exe) report.foundItems.push_back("Game executable (GTA5.exe)");
    if (report.hasCommonRpf) report.foundItems.push_back("Common data archive (common.rpf)");
    if (report.hasUpdateRpf) report.foundItems.push_back("Update package (update/update.rpf)");
    if (report.foundRpfCount > 0) {
        report.foundItems.push_back("Base game archives (" + std::to_string(report.foundRpfCount) + "/23 x64*.rpf)");
    }

    // A valid installation has either the native web engine format OR the PC game archives
    bool hasNativeWebFiles = report.hasEngine && report.hasManifest && report.hasShaders;
    bool hasStandardPcFiles = (report.hasGta5Exe || report.hasCommonRpf) && report.foundRpfCount >= 4;

    if (hasNativeWebFiles || hasStandardPcFiles) {
        report.isValid = true;
        report.errorMessage.clear();
    } else {
        report.isValid = false;
        report.errorMessage = "Missing required game files in directory:\n";
        for (const auto &item : report.missingItems) {
            report.errorMessage += " • " + item + "\n";
        }
    }

    return report;
}

std::string WinFiles::AutoDetectGameFolder() {
    std::error_code ec;
    const std::vector<std::string> searchPaths = {
        "C:\\Program Files\\Rockstar Games\\Grand Theft Auto V",
        "C:\\Program Files (x86)\\Rockstar Games\\Grand Theft Auto V",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Grand Theft Auto V",
        "C:\\Program Files\\Steam\\steamapps\\common\\Grand Theft Auto V",
        "D:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
        "E:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
        "F:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
        "C:\\Program Files\\Epic Games\\GTAV",
        "D:\\Epic Games\\GTAV",
        "E:\\Epic Games\\GTAV",
        ".\\GTA5",
        "."
    };

    for (const auto &candidate : searchPaths) {
        fs::path p(candidate);
        if (fs::exists(p, ec) && fs::is_directory(p, ec)) {
            if (fs::exists(p / "GTA5.exe", ec) ||
                fs::exists(p / "common.rpf", ec) ||
                fs::exists(p / "data" / "manifest.json", ec)) {
                return fs::canonical(p, ec).string();
            }
        }
    }
    return "";
}

std::string WinFiles::LoadSavedGameFolder() {
    std::string configPath = GetConfigFilePath();
    char buffer[MAX_PATH] = {};
    GetPrivateProfileStringA("Launcher", "GameFolder", "", buffer, MAX_PATH, configPath.c_str());
    if (buffer[0] != '\0') return std::string(buffer);

    // Fallback to old settings.ini in AppData Roaming
    fs::path oldIni = fs::path(GetAppDataFolder()) / "settings.ini";
    GetPrivateProfileStringA("Game", "Folder", "", buffer, MAX_PATH, oldIni.string().c_str());
    return std::string(buffer);
}

void WinFiles::SaveGameFolder(const std::string &path) {
    std::string configPath = GetConfigFilePath();
    WritePrivateProfileStringA("Launcher", "GameFolder", path.c_str(), configPath.c_str());
}

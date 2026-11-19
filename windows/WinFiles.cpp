#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "WinFiles.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

std::string WinFiles::GetAppDataFolder() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        fs::path p(path);
        CoTaskMemFree(path);
        p /= "GTA5Port";
        fs::create_directories(p);
        return p.string();
    }
    return "C:\\GTA5Port";
}

std::string WinFiles::GetDocumentsFolder() {
    fs::path p = fs::path(GetAppDataFolder()) / "Data";
    fs::create_directories(p);
    return p.string();
}

void WinFiles::PrepareEnvironment(const std::string &filesRoot) {
    fs::path root(filesRoot);
    fs::create_directories(root / "userdata" / "Documents" / "Rockstar Games" / "GTA V");
    fs::create_directories(root / "sandbox" / "Documents" / "Rockstar Games" / "GTA V");
    fs::create_directories(root / "Self Radio");
    fs::create_directories(root / "Mods");

    // Clean stale graphics settings on start so app graphics options govern (matching NativePortViewController.swift)
    std::error_code ec;
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
    pFileOpen->SetTitle(L"Select Grand Theft Auto V Game Folder (with data/ and b/)");

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

    fs::path p(path);
    if (!fs::exists(p) || !fs::is_directory(p)) {
        report.errorMessage = "Specified directory does not exist.";
        return report;
    }

    report.hasEngine = fs::exists(p / "b" / "8b0b5899ed" / "game.wasm");
    report.hasManifest = fs::exists(p / "data" / "manifest.json");
    report.hasShaders = fs::exists(p / "b" / "8b0b5899ed" / "shaders" / "index.json");

    if (!report.hasEngine) {
        report.errorMessage = "Missing engine file: b/8b0b5899ed/game.wasm";
    } else if (!report.hasManifest) {
        report.errorMessage = "Missing manifest file: data/manifest.json";
    } else if (!report.hasShaders) {
        report.errorMessage = "Missing shader index: b/8b0b5899ed/shaders/index.json";
    } else {
        report.isValid = true;
    }

    return report;
}

std::string WinFiles::LoadSavedGameFolder() {
    fs::path iniPath = fs::path(GetAppDataFolder()) / "settings.ini";
    char buffer[MAX_PATH] = {};
    GetPrivateProfileStringA("Game", "Folder", "", buffer, MAX_PATH, iniPath.string().c_str());
    return std::string(buffer);
}

void WinFiles::SaveGameFolder(const std::string &path) {
    fs::path iniPath = fs::path(GetAppDataFolder()) / "settings.ini";
    WritePrivateProfileStringA("Game", "Folder", path.c_str(), iniPath.string().c_str());
}

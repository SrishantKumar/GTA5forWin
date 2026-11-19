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

struct WinGameFolderReport {
    bool isValid = false;
    bool hasEngine = false;       // b/8b0b5899ed/game.wasm
    bool hasManifest = false;     // data/manifest.json
    bool hasShaders = false;      // b/8b0b5899ed/shaders/index.json
    bool hasCommonRpf = false;    // common.rpf
    bool hasUpdateRpf = false;    // update/update.rpf
    bool hasGta5Exe = false;      // GTA5.exe
    int foundRpfCount = 0;
    std::string folderPath;
    std::string errorMessage;
    std::vector<std::string> foundItems;
    std::vector<std::string> missingItems;
};

class WinFiles {
public:
    static std::string GetAppDataFolder();
    static std::string GetLocalAppDataFolder();
    static std::string GetConfigFilePath();
    static std::string GetDocumentsFolder();
    static std::string GetModsFolder();
    static std::string GetUserMusicFolder();
    static void PrepareEnvironment(const std::string &filesRoot);
    static bool PromptChooseGameFolder(HWND hwnd, std::string &outPath);
    static WinGameFolderReport VerifyGameFolder(const std::string &path);
    static std::string AutoDetectGameFolder();
    static std::string LoadSavedGameFolder();
    static void SaveGameFolder(const std::string &path);
    static void OpenFolderInExplorer(const std::string &path);
};

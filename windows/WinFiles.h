#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct WinGameFolderReport {
    bool isValid = false;
    bool hasEngine = false;
    bool hasManifest = false;
    bool hasShaders = false;
    std::string folderPath;
    std::string errorMessage;
};

class WinFiles {
public:
    static std::string GetAppDataFolder();
    static std::string GetDocumentsFolder();
    static void PrepareEnvironment(const std::string &filesRoot);
    static bool PromptChooseGameFolder(HWND hwnd, std::string &outPath);
    static WinGameFolderReport VerifyGameFolder(const std::string &path);
    static std::string LoadSavedGameFolder();
    static void SaveGameFolder(const std::string &path);
};

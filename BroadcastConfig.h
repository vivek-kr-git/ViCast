/*
 * Copyright @2026 | ViCast
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include "json/json.hpp"
#include "Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <combaseapi.h>

namespace fs = std::filesystem;
using json = nlohmann::json;


// -------------------- Helpers --------------------

// Convert UTF-8 std::string -> UTF-16 std::wstring
inline std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// Get LocalAppData path
inline std::string GetAppDataPath()
{
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        std::string strPath(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &strPath[0], size_needed, nullptr, nullptr);
        CoTaskMemFree(path);
        strPath.resize(size_needed - 1);

        std::string fullPath = strPath + "\\ViCast";
        fs::create_directories(fullPath + "\\logs");
        return fullPath;
    }
    return ".";
}

// Get folder where executable resides
inline std::string GetExeDirectory()
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path().string();
}

// -------------------- Data Structures --------------------

struct DeckLinkDeviceConfig
{
    std::string name;
    std::string outputMode;
    bool keyOutput = true;
};

class BroadcastConfig
{
public:
    // NDI Input
    std::string ndiInputPrimary;
    std::string ndiInputBackup;
    int flapThreshold = 3;

    // NDI Output
    std::string ndiOutputFill = "ViCast Fill";
    std::string ndiOutputKey = "ViCast Key";
    std::string ndiOutputEmbedded = "ViCast Embedded";

    // Output Format
    int outputWidth = 1920;
    int outputHeight = 1080;
    int outputFpsN = 30000;
    int outputFpsD = 1001;

    // NDI Settings
    bool ndiHighestQuality = true;
    bool clockVideo = true;
    bool clockAudio = true;
    std::string pixelFormat = "v210";

    bool forceProgressive = false;
    bool forceInterlaced = false;

    bool enableDesktopCapture = true;
    bool enableNdiInput = true;

    // DeckLink
    bool enableDeckLink = false;
    std::vector<DeckLinkDeviceConfig> decklinkDevices;

    // Paths & Logging
    std::string logFile;
    bool verboseLogging = false;
    std::string configPath;

    // Methods
    BroadcastConfig();
    bool Validate(std::string& error) const;
    static BroadcastConfig Load(const std::string& path = "");
    bool Save() const;
    void EnsureDirectoriesExist() const;
};
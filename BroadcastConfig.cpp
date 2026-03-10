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

#include "BroadcastConfig.h"
#include "json/json.hpp"
#include <fstream>
#include <stdexcept>
#include "Logger.h"
#include <sstream>
#include <limits>
#include <shlobj.h> 
#include <filesystem>
#include <windows.h> // Added for GetModuleFileNameA

using json = nlohmann::json;

// -------------------- Implementation --------------------

BroadcastConfig::BroadcastConfig()
{
    std::string configDir = GetAppDataPath();
    configPath = configDir + "\\config.json";
    logFile = configDir + "\\logs\\service.log";
}

void BroadcastConfig::EnsureDirectoriesExist() const
{
    fs::path cPath(configPath);
    if (cPath.has_parent_path() && !fs::exists(cPath.parent_path()))
        fs::create_directories(cPath.parent_path());

    fs::path lPath(logFile);
    if (lPath.has_parent_path() && !fs::exists(lPath.parent_path()))
        fs::create_directories(lPath.parent_path());
}

bool BroadcastConfig::Validate(std::string& error) const
{
    if (outputWidth <= 0 || outputHeight <= 0) {
        error = "Invalid output resolution.";
        return false;
    }
    if (!enableDesktopCapture && !enableNdiInput) {
        error = "No input source enabled.";
        return false;
    }
    return true;
}

bool BroadcastConfig::Save() const
{
    EnsureDirectoriesExist();
    try {
        json j;
        j["ndiInputPrimary"] = ndiInputPrimary;
        j["ndiInputBackup"] = ndiInputBackup;
        j["flapThreshold"] = flapThreshold;

        j["ndiOutputFill"] = ndiOutputFill;
        j["ndiOutputKey"] = ndiOutputKey;
        j["ndiOutputEmbedded"] = ndiOutputEmbedded;

        j["outputWidth"] = outputWidth;
        j["outputHeight"] = outputHeight;
        j["outputFpsN"] = outputFpsN;
        j["outputFpsD"] = outputFpsD;

        j["ndiHighestQuality"] = ndiHighestQuality;
        j["clockVideo"] = clockVideo;
        j["clockAudio"] = clockAudio;
        j["pixelFormat"] = pixelFormat;
        j["forceProgressive"] = forceProgressive;
        j["forceInterlaced"] = forceInterlaced;

        j["enableDesktopCapture"] = enableDesktopCapture;
        j["enableNdiInput"] = enableNdiInput;
        j["enableDeckLink"] = enableDeckLink;

        json dlArray = json::array();
        for (const auto& dev : decklinkDevices) {
            dlArray.push_back({
                {"name", dev.name},
                {"outputMode", dev.outputMode},
                {"keyOutput", dev.keyOutput}
                });
        }
        j["decklinkDevices"] = dlArray;

        j["logFile"] = logFile;
        j["verboseLogging"] = verboseLogging;

        std::ofstream file(configPath);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    }
    catch (...) {
        return false;
    }
}

BroadcastConfig BroadcastConfig::Load(const std::string& path)
{
    BroadcastConfig cfg;

    // GET THE APPDATA PATH FIRST!
        std::string configDir = GetAppDataPath();

    // Set the paths to AppData (unless the user explicitly passed a custom path)
    cfg.configPath = path.empty() ? configDir + "\\config.json" : path;
    cfg.logFile = configDir + "\\logs\\service.log";

    // ---------------- First-Run / Self-Healing ----------------
    if (!fs::exists(cfg.configPath))
    {
        try {
            std::string installConfig = GetExeDirectory() + "\\config.json";

            if (fs::exists(installConfig)) {
                fs::copy_file(installConfig, cfg.configPath, fs::copy_options::overwrite_existing);
                Logger::Log(L"[INFO] Copied default config from installation folder to AppData.");
            }
            else {
                cfg.Save();
                Logger::Log(L"[INFO] No installer config found. Generated default config at: " + StringToWString(cfg.configPath));
            }
        }
        catch (const std::exception& e) {
            Logger::Log(L"[WARNING] Could not set up default config on disk: " + StringToWString(e.what()));
            Logger::Log(L"[INFO] Proceeding with in-memory defaults.");

            // Bypass the file reading completely to avoid a crash
            return cfg;
        }
    }

    Logger::Log(L"Loading configuration: " + StringToWString(cfg.configPath));

    std::ifstream file(cfg.configPath);
    if (!file.is_open()) {
        Logger::Log(L"[WARNING] Cannot open config file for reading. Proceeding with in-memory defaults.");
        return cfg; // Do not throw! Just return the defaults.
    }

    json j;
    try { file >> j; }
    catch (const std::exception& e) {
        Logger::Log(L"[ERROR] Invalid JSON format: " + StringToWString(e.what()));
        throw;
    }

    try {
        cfg.enableDesktopCapture = j.value("enableDesktopCapture", cfg.enableDesktopCapture);
        cfg.enableNdiInput = j.value("enableNdiInput", cfg.enableNdiInput);
        cfg.ndiInputPrimary = j.value("ndiInputPrimary", cfg.ndiInputPrimary);
        cfg.ndiInputBackup = j.value("ndiInputBackup", cfg.ndiInputBackup);
        cfg.flapThreshold = j.value("flapThreshold", cfg.flapThreshold);

        cfg.ndiOutputFill = j.value("ndiOutputFill", cfg.ndiOutputFill);
        cfg.ndiOutputKey = j.value("ndiOutputKey", cfg.ndiOutputKey);
        cfg.ndiOutputEmbedded = j.value("ndiOutputEmbedded", cfg.ndiOutputEmbedded);

        cfg.outputWidth = j.value("outputWidth", cfg.outputWidth);
        cfg.outputHeight = j.value("outputHeight", cfg.outputHeight);
        cfg.outputFpsN = j.value("outputFpsN", cfg.outputFpsN);
        cfg.outputFpsD = j.value("outputFpsD", cfg.outputFpsD);

        cfg.ndiHighestQuality = j.value("ndiHighestQuality", cfg.ndiHighestQuality);
        cfg.clockVideo = j.value("clockVideo", cfg.clockVideo);
        cfg.clockAudio = j.value("clockAudio", cfg.clockAudio);
        cfg.pixelFormat = j.value("pixelFormat", cfg.pixelFormat);
        cfg.forceProgressive = j.value("forceProgressive", cfg.forceProgressive);
        cfg.forceInterlaced = j.value("forceInterlaced", cfg.forceInterlaced);

        cfg.enableDeckLink = j.value("enableDeckLink", cfg.enableDeckLink);

        if (j.contains("decklinkDevices") && j["decklinkDevices"].is_array()) {
            cfg.decklinkDevices.clear();
            for (const auto& d : j["decklinkDevices"]) {
                DeckLinkDeviceConfig dev;
                dev.name = d.value("name", "");
                dev.outputMode = d.value("outputMode", "");
                dev.keyOutput = d.value("keyOutput", true);
                cfg.decklinkDevices.push_back(dev);
            }
        }

        cfg.logFile = j.value("logFile", cfg.logFile);
        cfg.verboseLogging = j.value("verboseLogging", cfg.verboseLogging);
    }
    catch (const std::exception& e) {
        Logger::Log(L"[ERROR] Invalid config value types: " + StringToWString(e.what()));
        throw;
    }

    // ---------------- Validation ----------------
    std::string error;
    if (!cfg.Validate(error)) {
        Logger::Log(L"[ERROR] Config validation failed: " + StringToWString(error));
        throw std::runtime_error("Config validation failed");
    }

    Logger::Log(L"Configuration loaded successfully.");
    Logger::Log(L"Resolution: " + std::to_wstring(cfg.outputWidth) + L"x" + std::to_wstring(cfg.outputHeight));

    double fps = static_cast<double>(cfg.outputFpsN) / cfg.outputFpsD;
    Logger::Log(L"Frame Rate: " + std::to_wstring(fps));
    Logger::Log(L"DeckLink Enabled: " + std::wstring(cfg.enableDeckLink ? L"YES" : L"NO"));

    return cfg;
}
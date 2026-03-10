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

#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <filesystem>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

std::mutex Logger::mtx;
std::ofstream Logger::logFile;
std::string Logger::logPath;

void Logger::Init(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mtx);
    logPath = path;

    // --- NEW: Automatic Log Rotation ---
    namespace fs = std::filesystem;
    try {
        if (fs::exists(logPath)) {
            // Check if file is larger than 5 MB (5 * 1024 * 1024 = 5242880 bytes)
            if (fs::file_size(logPath) > 5242880) {
                std::string backupPath = logPath + ".bak";

                // Delete the old backup if it exists
                if (fs::exists(backupPath)) {
                    fs::remove(backupPath);
                }

                // Rename the current bloated log to act as the new backup
                fs::rename(logPath, backupPath);
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Logger: Failed to rotate log file: " << e.what() << std::endl;
    }
    // -----------------------------------

    logFile.open(logPath, std::ios::out | std::ios::app);
    if (!logFile.is_open())
        std::cerr << "Logger: Failed to open log file at " << path << std::endl;
}

void Logger::Log(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(mtx);

    // Auto-reconnect to file if it somehow dropped
    if (!logFile.is_open())
        logFile.open(logPath, std::ios::out | std::ios::app);

    if (logFile.is_open())
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        // MSVC safe local time (Prevents C4996 Warning/Error)
        struct tm timeinfo;
        localtime_s(&timeinfo, &t);

        logFile << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << msg << "\n";
        logFile.flush();
    }
}

void Logger::Log(const std::wstring& msg)
{
    if (msg.empty()) return;

    // Safely convert UTF-16 wide string back to UTF-8 standard string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &msg[0], (int)msg.size(), NULL, 0, NULL, NULL);
    std::string s(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &msg[0], (int)msg.size(), &s[0], size_needed, NULL, NULL);

    // Route it through your incredibly robust main Log method
    Log(s);
}
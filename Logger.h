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
#include <mutex>
#include <fstream>

class Logger
{
public:
    // Initialize the log file path
    static void Init(const std::string& path);

    // Log a UTF-8 string with timestamp
    static void Log(const std::string& msg);

    // Log a wide string
    static void Log(const std::wstring& msg);

private:
    static std::mutex mtx;
    static std::ofstream logFile;
    static std::string logPath;
};
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

#include <filesystem>
#include "BroadcastConfig.h"
#include "BroadcastEngine.h"
#include "Logger.h"
#include <chrono>
#include <memory>
#include <psapi.h>
#include <conio.h>
#include <iomanip>
#pragma comment(lib, "psapi.lib")

#include <windows.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <exception>
#include "resource.h"

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
std::unique_ptr<BroadcastEngine> gEngine;
std::atomic<bool> gShutdownRequested{ false };
std::atomic<bool> gCleanupComplete{ false };
int gDashboardLineOffset = 0;

// ------------------------------------------------------------
// Console Helpers
// ------------------------------------------------------------
void SetColor(int color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void SetCursorPosition(int x, int y)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hConsole, pos);
}

void SetConsoleSize(int width, int windowHeight)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 1. Shrink window to 1x1 to allow buffer resizing
    SMALL_RECT minRect = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(hConsole, TRUE, &minRect);

    // 2. Set massive memory buffer
    COORD bufferSize = { (SHORT)width, 1000 };
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    // 3. Attempt to expand to target size
    SMALL_RECT rect = { 0, 0, (SHORT)(width - 1), (SHORT)(windowHeight - 1) };
    if (!SetConsoleWindowInfo(hConsole, TRUE, &rect))
    {
        // FALLBACK: Auto-calculate exact pixel size based on the active console font!
        HWND hwnd = GetConsoleWindow();
        if (hwnd) {
            CONSOLE_FONT_INFO cfi;
            GetCurrentConsoleFont(hConsole, FALSE, &cfi);
            COORD fontSize = GetConsoleFontSize(hConsole, cfi.nFont);

            // Calculate exact pixels + standard padding for Windows borders and title bar
            int pixelWidth = (width * fontSize.X) + 40;
            int pixelHeight = (windowHeight * fontSize.Y) + 40;

            SetWindowPos(hwnd, NULL, 0, 0, pixelWidth, pixelHeight, SWP_NOMOVE | SWP_NOZORDER);
        }
    }
}

void LockConsoleWindow()
{
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow != NULL)
    {
        LONG style = GetWindowLong(consoleWindow, GWL_STYLE);

        // Strip out the resize border (WS_SIZEBOX) and the maximize button (WS_MAXIMIZEBOX)
        style = style & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX;

        SetWindowLong(consoleWindow, GWL_STYLE, style);
    }
}

// ------------------------------------------------------------
// UI: Splash Screen & Config Readout
// ------------------------------------------------------------
void PrintAppHeader(const BroadcastConfig& cfg)
{
    SetColor(6); // Burnt orange/gold
    std::cout << R"(
     __   __    ______          __ 
    | |  / (_) / ____/___  ____/ /_
    | | / / / / /   / __ `/ ___/ __/
    | |/ / / / /___/ /_/ (__  ) /_  
    |___/_/  \____/\__,_/____/\__/ NIP  v1.0r
    )" << '\n';

    SetColor(15); // White
    std::cout << "  Professional NDI/SDI Broadcast Gateway\n";
    SetColor(11);
    std::cout << "======================================================================\n";

    EngineStats stats{};
    if (gEngine) {
        stats = gEngine->GetStats();
    }

    // --- Input Routing ---
    SetColor(14); std::cout << " [ INPUT ROUTING ]\n";

    // Primary NDI Display
    SetColor(7);  std::cout << "  Primary NDI : ";
    if (cfg.ndiInputPrimary.empty()) {
        SetColor(8); std::cout << "DISABLED\n";
    }
    else {
        SetColor(stats.usingBackup ? 8 : 10);
        std::cout << cfg.ndiInputPrimary << (stats.usingBackup ? "" : " [ACTIVE]") << "\n";
    }

    // Backup NDI Display
    SetColor(7);  std::cout << "  Backup NDI  : ";
    if (cfg.ndiInputBackup.empty()) {
        SetColor(8); std::cout << "DISABLED (No Failover)\n";
    }
    else {
        // Yellow (14) if active, Gray (8) if standby
        SetColor(stats.usingBackup ? 14 : 12);
        std::cout << cfg.ndiInputBackup << (stats.usingBackup ? " [FAILOVER ACTIVE]" : " [STANDBY]") << "\n";
    }

    // GPU Capture Status
    SetColor(7);  std::cout << "  GPU Capture : ";
    if (cfg.enableDesktopCapture) {
        SetColor(10); std::cout << "READY \n";
    }
    else {
        SetColor(8); std::cout << "OFF\n";
    }

    // --- Output Routing ---
    SetColor(7);  std::cout << "  Out Fill    : ";
    if (cfg.ndiOutputFill.empty()) { SetColor(8); std::cout << "DISABLED\n"; }
    else { SetColor(9); std::cout << cfg.ndiOutputFill << "\n"; }

    SetColor(7);  std::cout << "  Out Key     : ";
    if (cfg.ndiOutputKey.empty()) { SetColor(8); std::cout << "DISABLED\n"; }
    else { SetColor(14); std::cout << cfg.ndiOutputKey << "\n"; }

    SetColor(7);  std::cout << "  Out Embed   : ";
    if (cfg.ndiOutputEmbedded.empty()) { SetColor(8); std::cout << "DISABLED\n"; }
    else { SetColor(11); std::cout << cfg.ndiOutputEmbedded << "\n"; }

    // --- Hardware ---
    SetColor(14); std::cout << "\n [ HARDWARE ]\n";
    SetColor(7);  std::cout << "  DeckLink    : ";

    if (cfg.enableDeckLink && !cfg.decklinkDevices.empty()) {
        SetColor(10); std::cout << cfg.decklinkDevices[0].name << " (ACTIVE)\n";
    }
    else if (!cfg.decklinkDevices.empty()) {
        SetColor(8);  std::cout << cfg.decklinkDevices[0].name << " (DISABLED IN CONFIG)\n";
    }
    else {
        SetColor(8);  std::cout << "NONE (NDI ONLY)\n";
    }

    // Calculate the actual decimal framerate
    double targetFps = (double)cfg.outputFpsN / (double)cfg.outputFpsD;

    // convert to field rate if interlaced
    if (cfg.forceInterlaced)
        targetFps *= 2.0;

    SetColor(7);
    std::cout << "  Format      : ";
    SetColor(15);

    std::cout << cfg.outputWidth << "x" << cfg.outputHeight << " @ ";

    // if fps is effectively integer
    if (std::fabs(targetFps - std::round(targetFps)) < 0.001)
    {
        std::cout << (int)std::round(targetFps);
    }
    else
    {
        std::cout << std::fixed << std::setprecision(2) << targetFps;
    }

    std::cout << (cfg.forceInterlaced ? "i" : "p") << "\n";

    SetColor(11);
    std::cout << "======================================================================\n\n";
    SetColor(7);
}

// ------------------------------------------------------------
// Telemetry Helpers
// ------------------------------------------------------------
double GetProcessCPUPercent()
{
    static FILETIME prevSysTime, prevKernelTime, prevUserTime;
    static bool firstCall = true;
    static int numProcessors = 0;

    if (firstCall)
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
        GetSystemTimeAsFileTime(&prevSysTime);

        FILETIME creation, exit;
        GetProcessTimes(GetCurrentProcess(), &creation, &exit, &prevKernelTime, &prevUserTime);
        firstCall = false;
        return 0.0;
    }

    FILETIME sysTime, creation, exit, kernelTime, userTime;
    GetSystemTimeAsFileTime(&sysTime);
    GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernelTime, &userTime);

    ULARGE_INTEGER sys, kernel, user, prevSys, prevKernel, prevUser;
    sys.LowPart = sysTime.dwLowDateTime; sys.HighPart = sysTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime; kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime; user.HighPart = userTime.dwHighDateTime;

    prevSys.LowPart = prevSysTime.dwLowDateTime; prevSys.HighPart = prevSysTime.dwHighDateTime;
    prevKernel.LowPart = prevKernelTime.dwLowDateTime; prevKernel.HighPart = prevKernelTime.dwHighDateTime;
    prevUser.LowPart = prevUserTime.dwLowDateTime; prevUser.HighPart = prevUserTime.dwHighDateTime;

    ULONGLONG sysDiff = sys.QuadPart - prevSys.QuadPart;
    ULONGLONG procDiff = (kernel.QuadPart - prevKernel.QuadPart) + (user.QuadPart - prevUser.QuadPart);

    prevSysTime = sysTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    if (sysDiff == 0 || numProcessors == 0) return 0.0;

    // Calculate total system usage and divide by the number of CPU cores
    double cpu = ((double)procDiff / (double)sysDiff) * 100.0;
    return cpu / numProcessors;
}

size_t GetProcessMemoryMB()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024 * 1024);
    }
    return 0;
}

std::string FormatUptime(std::chrono::steady_clock::time_point start)
{
    auto now = std::chrono::steady_clock::now();
    auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    return std::string(buf);
}

// ------------------------------------------------------------
// Console Close Handler
// ------------------------------------------------------------
BOOL WINAPI ConsoleHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
        return TRUE; // <-- NEW: Completely ignore Ctrl + C

    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        gShutdownRequested = true;
        for (int i = 0; i < 50; ++i) {
            if (gCleanupComplete) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return TRUE;
    default:
        return FALSE;
    }
}

std::string GetConfigPath()
{
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string pathStr(exePath);
    auto pos = pathStr.find_last_of("\\/");
    if (pos != std::string::npos) pathStr = pathStr.substr(0, pos);
    return pathStr + "\\config.json";
}

std::wstring ToWide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ------------------------------------------------------------
// WinMain
// ------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // --- COM Initialization ---
    // Critical for multithreaded DeckLink and DXGI operations
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        return -1;
    }

    // --- NDI Runtime Locator (Delay Load Setup) ---
    // This MUST happen before the engine is instantiated
    const wchar_t* ndiEnvVars[] = {
        L"NDI_RUNTIME_DIR_V6",
        L"NDI_RUNTIME_DIR_V5",
        L"NDI_RUNTIME_DIR_V4",
        L"NDI_RUNTIME_DIR_V3"
    };

    bool runtimeFound = false;
    for (const wchar_t* envVar : ndiEnvVars)
    {
        const wchar_t* ndiRuntimePath = _wgetenv(envVar);
        if (ndiRuntimePath)
        {
            SetDllDirectoryW(ndiRuntimePath);
            runtimeFound = true;
            break;
        }
    }

    if (!runtimeFound) {
        SetDllDirectoryW(L"C:\\Program Files\\NDI\\NDI 6 Runtime\\v6");
    }


    // --- Console Setup ---
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$", "r", stdin);
    SetConsoleTitleA("ViCast-NIP");
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SetConsoleSize(75, 37);
    LockConsoleWindow();

    // Disable QuickEdit Mode to prevent freezing
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prev_mode;
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE & ~ENABLE_ECHO_INPUT & ~ENABLE_LINE_INPUT);

    
    std::string appData = GetAppDataPath(); // Ensure this helper from BroadcastConfig is available or moved here
    Logger::Init(appData + "\\logs\\service.log");

    Logger::Log(L"====================================================");
    Logger::Log(L"****************************************************");
    Logger::Log(L"ViCast Starting...");

    BroadcastConfig cfg;
    try {
        cfg = BroadcastConfig::Load();
    }
    catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "ViCast - Config Error", MB_OK | MB_ICONERROR);
        Logger::Log(L"[FATAL] Config load failed: " + ToWide(ex.what()));
        CoUninitialize();
        return -1;
    }

    // Print the cool splash screen!
    PrintAppHeader(cfg);

    try {
        gEngine = std::make_unique<BroadcastEngine>(cfg);
        if (!gEngine->Initialize()) throw std::runtime_error("Engine Init Failed");

        SetColor(10); std::cout << "[SYSTEM] Engine starting...\n"; SetColor(7);
        gEngine->Start();
    }
    catch (const std::exception& ex) {
        SetColor(12); std::cout << "\n[FATAL] Engine start failed: " << ex.what() << "\n"; SetColor(7);
        Logger::Log(L"Engine start failed: " + ToWide(ex.what()));
        if (gEngine) gEngine.reset();
        CoUninitialize();
        system("pause");
        return -1;
    }

    std::cout << "\n";

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    gDashboardLineOffset = csbi.dwCursorPosition.Y;

    auto startTime = std::chrono::steady_clock::now();

    // ------------------------------------------------------------
    // Telemetry Dashboard Loop (Static & Bulletproof)
    // ------------------------------------------------------------
    while (!gShutdownRequested)
    {
        if (_kbhit())
        {
            int key = _getch();
            if (key == 'r' || key == 'R') { if (gEngine) gEngine->ResetStats(); }
            else if (key == 'd' || key == 'D') {
                if (gEngine) {
                    gEngine->DumpDiagnostics();
                    SetCursorPosition(0, gDashboardLineOffset + 6);
                    SetColor(10);
                    std::cout << " >> Diagnostics dumped to log!                                    ";
                    SetColor(7);
                }
            }
            else if (key == 24) { // Ctrl + X
                gShutdownRequested = true;
            }
        }

        if (gEngine)
        {
            auto stats = gEngine->GetStats();
            double cpu = GetProcessCPUPercent();
            size_t mem = GetProcessMemoryMB();

            // ROW 0: Header
            SetCursorPosition(0, gDashboardLineOffset + 0);
            SetColor(11);
            std::cout << "--- [ LIVE DASHBOARD ] -----------------------------------------------";

            // ROW 1: System
            SetCursorPosition(0, gDashboardLineOffset + 2);
            SetColor(7);
            std::cout << " UPTIME: " << FormatUptime(startTime) << "   |   ";
            SetColor(cpu < 50 ? 10 : (cpu < 80 ? 14 : 12));
            std::cout << "CPU: " << std::fixed << std::setprecision(1) << std::setw(5) << cpu << "%   |   ";
            SetColor(10);
            std::cout << "MEM: " << std::setw(4) << mem << " MB                        ";

            // ROW 2: Target Format
            double targetFps = (double)cfg.outputFpsN / (double)cfg.outputFpsD;

            // convert to field rate if interlaced
            if (cfg.forceInterlaced)
                targetFps *= 2.0;

            SetCursorPosition(0, gDashboardLineOffset + 3);
            SetColor(7);
            std::cout << " TARGET: " << cfg.outputWidth << "x" << cfg.outputHeight << " @ ";
            // if fps is effectively integer
            if (std::fabs(targetFps - std::round(targetFps)) < 0.001)
            {
                std::cout << (int)std::round(targetFps);
            }
            else
            {
                std::cout << std::fixed << std::setprecision(2) << targetFps;
            }
            std::cout << (cfg.forceInterlaced ? "i" : "p") << "   |   ";



            // --- Calculate frame and display rates ---
            double frameFps = (double)cfg.outputFpsN / (double)cfg.outputFpsD;
            double displayFps = cfg.forceInterlaced ? frameFps * 2.0 : frameFps; // show fields for interlaced
            double warningFps = frameFps; // use frame rate for color warning
                        
            // --- Print CURRENT FPS with dynamic color ---
            SetColor(stats.outputFPS > (warningFps - 2.0) ? 10 : 12); // green/red
            std::cout << "FPS: "
                << std::fixed << std::setprecision(2)
                << std::setw(6) << stats.outputFPS
                << "   "; // padding for smooth overwrite
            SetColor(7); // reset to default color


            // ROW 3: Input Routing
            SetCursorPosition(0, gDashboardLineOffset + 4);
            SetColor(7);
            std::cout << " ACTIVE: ";

            // Evaluate the routing hierarchy
            if (cfg.enableDesktopCapture)
            {
                SetColor(11); // Cyan for GPU to distinguish it from NDI
                std::cout << std::left << std::setw(18) << "GPU DESKTOP";
            }
            else if (stats.usingBackup)
            {
                SetColor(12); // Red/Yellow warning for Backup
                std::cout << std::left << std::setw(18) << "BACKUP NDI";
            }
            else
            {
                SetColor(10); // Green for Primary NDI
                std::cout << std::left << std::setw(18) << "PRIMARY NDI";
            }

            // Print the Dropped Frames telemetry
            SetColor(7); std::cout << "   |   DROPPED: ";
            SetColor(stats.droppedFrames == 0 ? 10 : 12);
            std::cout << std::left << std::setw(8) << stats.droppedFrames << "                  ";
            std::cout << std::right; // Reset alignment for the next lines

            // ROW 4: Hardware Output
            SetCursorPosition(0, gDashboardLineOffset + 5);
            SetColor(7);
            std::cout << " OUTPUT: ";
            SetColor(stats.decklinkPresent ? 10 : 8);
            std::cout << (stats.decklinkPresent ? "SDI ACTIVE     " : "NDI ONLY       ");

            // --- SYNC/DRIFT LOGIC HERE ---
            if (stats.decklinkPresent) {
                SetColor(7); std::cout << "   |   SYNC: ";
                SetColor(stats.genlockLocked ? 10 : 12);
                std::cout << (stats.genlockLocked ? "LOCKED" : "FREE  ");
                SetColor(7);
                std::cout << "   |   DRIFT: " << std::fixed << std::setprecision(2) << std::setw(6) << stats.driftMs << "ms         ";
            }
            else {
                std::cout << "                                                           ";
            }

            // ROW 5: Top Separator
            SetCursorPosition(0, gDashboardLineOffset + 6);
            SetColor(11);
            std::cout << "======================================================================";

            // ROW 6: Hotkeys
            SetCursorPosition(0, gDashboardLineOffset + 7);
            SetColor(14); std::cout << "  [ HOTKEYS ]  ";
            SetColor(10); std::cout << "R"; SetColor(7); std::cout << " : Reset Stats   |   ";
            SetColor(10); std::cout << "D"; SetColor(7); std::cout << " : Dump Log   |   ";
            SetColor(12); std::cout << "Ctrl+X"; SetColor(7); std::cout << " : Exit                     ";

            // ROW 7: Bottom Separator
            SetCursorPosition(0, gDashboardLineOffset + 8);
            SetColor(11);
            std::cout << "======================================================================";

            // Flush immediately to screen
            std::cout << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ------------------------------------------------------------
    // Clean Shutdown
    // ------------------------------------------------------------

    SetCursorPosition(0, gDashboardLineOffset + 10);
    SetColor(14);
    std::cout << "[SYSTEM] Shutting down Engine safely...\n";
    SetColor(7);

    if (gEngine)
    {
        gEngine->Stop();
        gEngine.reset();
    }

    Logger::Log(L"ViCast stopped cleanly.");
    Logger::Log(L"****************************************************");
    Logger::Log(L"====================================================");
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    CoUninitialize();

    gCleanupComplete = true;
    return 0;
}
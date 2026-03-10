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

#include "BroadcastEngine.h"
#include "BroadcastConfig.h"
#include "libyuv.h"
#include <iostream>
#include <filesystem>
#include "json/json.hpp"
#include <fstream>
#include <algorithm>
#include "Logger.h"
#include <chrono>
#include <thread>
#include <codecvt> // for std::wstring_convert
#include <immintrin.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <execution>
#include <d3d11.h>
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11.lib")

// Definition (allocate storage)
ComPtr<IDXGIOutput1> output1;
ComPtr<IDXGIResource> resource;
DXGI_OUTDUPL_FRAME_INFO frameInfo = {};

// -------------------- DeckLink Mode Table --------------------
struct DeckLinkModeInfo
{
    int width;
    int height;
    int fpsN;
    int fpsD;
    bool interlaced; // NEW FLAG
    BMDDisplayMode mode;
};

static const DeckLinkModeInfo decklinkModes[] = {
    // ==========================================
    // Standard Definition (SD)
    // ==========================================
    {720,  486,  30000, 1001, true,  bmdModeNTSC},
    {720,  486,  24000, 1001, false, bmdModeNTSC2398},
    {720,  486,  30000, 1001, false, bmdModeNTSCp},      // NTSC Progressive
    {720,  576,  25,    1,    true,  bmdModePAL},
    {720,  576,  25,    1,    false, bmdModePALp},       // PAL Progressive

    // ==========================================
    // HD 720 (Progressive only)
    // ==========================================
    {1280, 720,  50,    1,    false, bmdModeHD720p50},
    {1280, 720,  60000, 1001, false, bmdModeHD720p5994},
    {1280, 720,  60,    1,    false, bmdModeHD720p60},

    // ==========================================
    // HD 1080 (Interlaced)
    // ==========================================
    {1920, 1080, 25,    1,    true,  bmdModeHD1080i50},
    {1920, 1080, 30000, 1001, true,  bmdModeHD1080i5994},
    {1920, 1080, 30,    1,    true,  bmdModeHD1080i6000},

    // ==========================================
    // HD 1080 (Progressive)
    // ==========================================
    {1920, 1080, 24000, 1001, false, bmdModeHD1080p2398},
    {1920, 1080, 24,    1,    false, bmdModeHD1080p24},
    {1920, 1080, 25,    1,    false, bmdModeHD1080p25},
    {1920, 1080, 30000, 1001, false, bmdModeHD1080p2997},
    {1920, 1080, 30,    1,    false, bmdModeHD1080p30},
    {1920, 1080, 50,    1,    false, bmdModeHD1080p50},
    {1920, 1080, 60000, 1001, false, bmdModeHD1080p5994},
    {1920, 1080, 60,    1,    false, bmdModeHD1080p6000},

    // ==========================================
    // Legacy 2K Film (1556 lines)
    // ==========================================
    {2048, 1556, 24000, 1001, false, bmdMode2k2398},
    {2048, 1556, 24,    1,    false, bmdMode2k24},
    {2048, 1556, 25,    1,    false, bmdMode2k25},

    // ==========================================
    // 2K DCI (1080 lines)
    // ==========================================
    {2048, 1080, 24000, 1001, false, bmdMode2kDCI2398},
    {2048, 1080, 24,    1,    false, bmdMode2kDCI24},
    {2048, 1080, 25,    1,    false, bmdMode2kDCI25},
    {2048, 1080, 30000, 1001, false, bmdMode2kDCI2997},
    {2048, 1080, 30,    1,    false, bmdMode2kDCI30},
    {2048, 1080, 50,    1,    false, bmdMode2kDCI50},
    {2048, 1080, 60000, 1001, false, bmdMode2kDCI5994},
    {2048, 1080, 60,    1,    false, bmdMode2kDCI60},

    // ==========================================
    // 4K UHD (3840 x 2160)
    // ==========================================
    {3840, 2160, 24000, 1001, false, bmdMode4K2160p2398},
    {3840, 2160, 24,    1,    false, bmdMode4K2160p24},
    {3840, 2160, 25,    1,    false, bmdMode4K2160p25},
    {3840, 2160, 30000, 1001, false, bmdMode4K2160p2997},
    {3840, 2160, 30,    1,    false, bmdMode4K2160p30},
    {3840, 2160, 50,    1,    false, bmdMode4K2160p50},
    {3840, 2160, 60000, 1001, false, bmdMode4K2160p5994},
    {3840, 2160, 60,    1,    false, bmdMode4K2160p60},

    // ==========================================
    // 4K DCI (4096 x 2160)
    // ==========================================
    {4096, 2160, 24000, 1001, false, bmdMode4kDCI2398},
    {4096, 2160, 24,    1,    false, bmdMode4kDCI24},
    {4096, 2160, 25,    1,    false, bmdMode4kDCI25},
    {4096, 2160, 30000, 1001, false, bmdMode4kDCI2997},
    {4096, 2160, 30,    1,    false, bmdMode4kDCI30},
    {4096, 2160, 50,    1,    false, bmdMode4kDCI50},
    {4096, 2160, 60000, 1001, false, bmdMode4kDCI5994},
    {4096, 2160, 60,    1,    false, bmdMode4kDCI60},

    // ==========================================
    // 8K UHD (7680 x 4320)
    // ==========================================
    {7680, 4320, 24000, 1001, false, bmdMode8K4320p2398},
    {7680, 4320, 24,    1,    false, bmdMode8K4320p24},
    {7680, 4320, 25,    1,    false, bmdMode8K4320p25},
    {7680, 4320, 30000, 1001, false, bmdMode8K4320p2997},
    {7680, 4320, 30,    1,    false, bmdMode8K4320p30},
    {7680, 4320, 50,    1,    false, bmdMode8K4320p50},
    {7680, 4320, 60000, 1001, false, bmdMode8K4320p5994},
    {7680, 4320, 60,    1,    false, bmdMode8K4320p60},

    // ==========================================
    // 8K DCI (8192 x 4320)
    // ==========================================
    {8192, 4320, 24000, 1001, false, bmdMode8kDCI2398},
    {8192, 4320, 24,    1,    false, bmdMode8kDCI24},
    {8192, 4320, 25,    1,    false, bmdMode8kDCI25},
    {8192, 4320, 30000, 1001, false, bmdMode8kDCI2997},
    {8192, 4320, 30,    1,    false, bmdMode8kDCI30},
    {8192, 4320, 50,    1,    false, bmdMode8kDCI50},
    {8192, 4320, 60000, 1001, false, bmdMode8kDCI5994},
    {8192, 4320, 60,    1,    false, bmdMode8kDCI60}
};

BMDDisplayMode DetectDisplayMode(int w, int h, int fpsN, int fpsD, bool interlaced)
{
    int searchFps = fpsN;

    // Convert broadcast fields (50i/60i) to DeckLink frames (25/30)
    if (interlaced) {
        if (fpsN == 50) searchFps = 25;
        if (fpsN == 60) searchFps = 30;
        if (fpsN == 60000) searchFps = 30000;
    }

    for (const auto& m : decklinkModes)
    {
        if (m.width == w && m.height == h && m.fpsN == searchFps && m.fpsD == fpsD && m.interlaced == interlaced)
            return m.mode;
    }
    return bmdModeUnknown;
}

// ============================================================
// Constructor / Destructor
// ============================================================

BroadcastEngine::BroadcastEngine(const BroadcastConfig& c) : config(c) {}

BroadcastEngine::~BroadcastEngine() { Stop(); }

// ============================================================
// Initialize
// ============================================================

bool BroadcastEngine::Initialize()
{
    if (!config.enableDesktopCapture && !config.enableNdiInput)
    {
        Logger::Log(L"ERROR: No input source enabled.");
        return false;
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        Logger::Log(L"COM initialization failed.");
        return false;
    }

    if (!NDIlib_initialize())
    {
        Logger::Log(L"NDI initialization failed.");
        return false;
    }

    // D3D11 Init cleanly scoped inside the initialization phase
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, &d3dContext)))
    {
        Logger::Log(L"Warning: D3D11 Initialization failed. GPU features may be limited.");
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIOutput> output;
    
    if (config.enableDesktopCapture)
    {
        if (SUCCEEDED(d3dDevice.As(&dxgiDevice)) &&
            SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) &&
            SUCCEEDED(adapter->EnumOutputs(0, &output)) &&
            SUCCEEDED(output.As(&output1)))
        {
            if (SUCCEEDED(output1->DuplicateOutput(d3dDevice.Get(), &dxgiDuplication)))
            {
                gpuCaptureAvailable = true;

                DXGI_OUTDUPL_DESC dupDesc{};
                dxgiDuplication->GetDesc(&dupDesc);

                gpuCaptureWidth = dupDesc.ModeDesc.Width;
                gpuCaptureHeight = dupDesc.ModeDesc.Height;

                Logger::Log(L"Desktop Duplication initialized.");
                Logger::Log(L"Width: " + std::to_wstring(gpuCaptureWidth));
                Logger::Log(L"Height: " + std::to_wstring(gpuCaptureHeight));
            }
        }
    }
    else
    {
        gpuCaptureAvailable = false;
        Logger::Log(L"Desktop capture disabled by config.");
    }

    if (gpuCaptureAvailable)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = gpuCaptureWidth;
        desc.Height = gpuCaptureHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, &stagingTexture);

        if (FAILED(hr))
        {
            Logger::Log(L"Failed to create staging texture.");
            gpuCaptureAvailable = false;
        }
    }

    Logger::Log(L"Engine initialized successfully.");
    return true;
}

uint64_t GetFileLastWriteTime64(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) return 0;
    ULARGE_INTEGER ull;
    ull.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
    return ull.QuadPart;
}

void BroadcastEngine::Start()
{
    running = true;
    if (!config.configPath.empty())
    {
        lastWriteTime = GetFileLastWriteTime64(config.configPath);
        configWatcher = std::thread(&BroadcastEngine::ConfigWatcherThread, this);
    }
    timeBeginPeriod(1); // Force 1ms OS timer resolution
    ndiThread = std::thread(&BroadcastEngine::NDIThread, this);
    Logger::Log(L"BroadcastEngine started.");
}

void BroadcastEngine::Stop()
{
    if (!running) return;
    running = false;

    // 1. Wait for threads to cleanly exit their loops
    if (configWatcher.joinable()) configWatcher.join();
    if (ndiThread.joinable()) ndiThread.join();

    // 2. Safely cleanup DeckLink hardware
    if (deckLinkAvailable && output)
    {
        output->StopScheduledPlayback(0, nullptr, 0);
        output->DisableVideoOutput();
    }

    // 3. EXACT CODE YOU ASKED ABOUT (Updated for dual receivers)
    {
        std::lock_guard<std::mutex> lock(ndiMutex);
        if (ndiRecvPrimary) { NDIlib_recv_destroy(ndiRecvPrimary); ndiRecvPrimary = nullptr; }
        if (ndiRecvBackup) { NDIlib_recv_destroy(ndiRecvBackup); ndiRecvBackup = nullptr; }
    }

    if (ndiSendFill) { NDIlib_send_destroy(ndiSendFill); ndiSendFill = nullptr; }
    if (ndiSendKey) { NDIlib_send_destroy(ndiSendKey); ndiSendKey = nullptr; }
    if (ndiSendEmbedded) { NDIlib_send_destroy(ndiSendEmbedded); ndiSendEmbedded = nullptr; }

    // 4. Final SDK teardown
    NDIlib_destroy();
    CoUninitialize();

    timeEndPeriod(1);   // Restore OS default
    Logger::Log(L"BroadcastEngine stopped cleanly.");
}

bool BroadcastEngine::InitDeckLink(int w, int h, int& fpsN, int& fpsD, bool& interlaced)
{
    Logger::Log(L"Initializing DeckLink...");

    displayMode = DetectDisplayMode(w, h, fpsN, fpsD, interlaced);
    if (displayMode == bmdModeUnknown) {
        Logger::Log(L"Requested mode not supported by DeckLink! Falling back to strictly 1080p25.");
        displayMode = bmdModeHD1080p25;

        // OVERRIDE the engine's variables to guarantee 25p sync
        fpsN = 25;
        fpsD = 1;
        interlaced = false;
    }

    ComPtr<IDeckLinkIterator> iterator;
    if (FAILED(CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&iterator))) return false;
    if (FAILED(iterator->Next(&device))) return false;
    if (FAILED(device->QueryInterface(IID_IDeckLinkOutput, (void**)&output))) return false;
    if (FAILED(output->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault))) return false;

    // Convert broadcast fields to hardware frames
    int actualFpsN = fpsN;
    if (interlaced) {
        if (fpsN == 50) actualFpsN = 25;
        if (fpsN == 60) actualFpsN = 30;
        if (fpsN == 60000) actualFpsN = 30000;
    }

    decklinkTimeScale = actualFpsN;
    frameDurHW = fpsD;
    nextFrameTimeHW = 0;

    if (FAILED(output->StartScheduledPlayback(0, decklinkTimeScale, 1.0))) return false;

    deckLinkInitialized = true;
    return true;
}

EngineStats BroadcastEngine::GetStats() const
{
    EngineStats s;
    s.outputFPS = statFPS.load();
    s.usingBackup = statUsingBackup.load();
    s.decklinkPresent = statDecklinkPresent.load();
    s.genlockLocked = statGenlockLocked.load();
    s.driftMs = statDriftMs.load();
    s.droppedFrames = statDropped.load();
    return s;
}

void BroadcastEngine::ResetStats() { statDropped = 0; }

void BroadcastEngine::DumpDiagnostics()
{
    Logger::Log(L"==== DIAGNOSTICS DUMP ====");

    Logger::Log(L"FPS: " + std::to_wstring(statFPS.load()));
    Logger::Log(L"Dropped Frames: " + std::to_wstring(statDropped.load()));
    Logger::Log(L"Drift (ms): " + std::to_wstring(statDriftMs.load()));

    // Active input
    std::wstring activeInput = L"Unknown";
    if (gpuCaptureAvailable)
        activeInput = L"Desktop GPU Capture";
    else
        activeInput = usingBackup ? L"NDI Backup" : L"NDI Primary";

    Logger::Log(L"Active Input: " + activeInput);

    Logger::Log(L"Using Backup NDI: " + std::wstring(usingBackup ? L"YES" : L"NO"));
    Logger::Log(L"NDI Primary Receiver: " + std::wstring(ndiRecvPrimary ? L"Connected" : L"NULL"));
    Logger::Log(L"NDI Backup Receiver: " + std::wstring(ndiRecvBackup ? L"Connected" : L"NULL"));

    Logger::Log(L"DeckLink Present: " + std::wstring(statDecklinkPresent.load() ? L"YES" : L"NO"));
    Logger::Log(L"DeckLink Genlock Locked: " + std::wstring(decklinkClockValid ? L"YES" : L"NO"));

    Logger::Log(L"GPU Capture Available: " + std::wstring(gpuCaptureAvailable ? L"YES" : L"NO"));
    Logger::Log(L"GPU Capture Resolution: " + std::to_wstring(gpuCaptureWidth) + L"x" + std::to_wstring(gpuCaptureHeight));

    Logger::Log(L"Output Resolution: " + std::to_wstring(config.outputWidth) + L"x" + std::to_wstring(config.outputHeight));
    Logger::Log(L"Output FPS: " + std::to_wstring(config.outputFpsN) + L"/" + std::to_wstring(config.outputFpsD));
    Logger::Log(L"Force Interlaced: " + std::wstring(config.forceInterlaced ? L"YES" : L"NO"));
    Logger::Log(L"Force Progressive: " + std::wstring(config.forceProgressive ? L"YES" : L"NO"));

    Logger::Log(L"Flap Threshold: " + std::to_wstring(config.flapThreshold));
    Logger::Log(L"NDI Highest Quality: " + std::wstring(config.ndiHighestQuality ? L"YES" : L"NO"));

    Logger::Log(L"==========================");
}

void BroadcastEngine::ConfigWatcherThread()
{
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (config.configPath.empty()) continue;

        try {
            uint64_t current = GetFileLastWriteTime64(config.configPath);
            if (current != lastWriteTime) {
                lastWriteTime = current;
                ApplyLiveConfig(BroadcastConfig::Load(config.configPath));
            }
        }
        catch (...) { Logger::Log(L"Config reload failed."); }
    }
}

void BroadcastEngine::ApplyLiveConfig(const BroadcastConfig& newCfg)
{
    std::lock_guard<std::mutex> lock(configMutex);
    config.flapThreshold = newCfg.flapThreshold;
    Logger::Log(L"Live configuration updated safely.");
}

// SIMD Hardware Accelerated V210 Conversion
void BroadcastEngine::ConvertBGRAtoV210_SIMD(uint8_t* src, uint32_t* dst, int width, int height)
{
    const int stride = width * 4;
    for (int y = 0; y < height; ++y)
    {
        uint8_t* line = src + y * stride;
        uint32_t* out = dst + y * ((width + 5) / 6) * 4;

        for (int x = 0; x < width; x += 6)
        {
            uint8_t R[6], G[6], B[6];
            for (int i = 0; i < 6; ++i)
            {
                int px = (std::min)(x + i, width - 1);
                B[i] = line[px * 4 + 0];
                G[i] = line[px * 4 + 1];
                R[i] = line[px * 4 + 2];
            }

            uint16_t Y[6], Cb[3], Cr[3];
            for (int i = 0; i < 6; ++i)
            {
                int yv = (218 * R[i] + 732 * G[i] + 74 * B[i]) >> 10;
                Y[i] = std::clamp(64 + (yv * 876) / 255, 64, 940);
            }

            for (int i = 0; i < 3; ++i)
            {
                int p0 = i * 2;
                int p1 = p0 + 1;
                int r = (R[p0] + R[p1]) >> 1;
                int g = (G[p0] + G[p1]) >> 1;
                int b = (B[p0] + B[p1]) >> 1;

                int cb = (-117 * r - 395 * g + 512 * b) >> 10;
                int cr = (512 * r - 465 * g - 47 * b) >> 10;

                Cb[i] = std::clamp(512 + (cb * 896) / 255, 64, 960);
                Cr[i] = std::clamp(512 + (cr * 896) / 255, 64, 960);
            }

            out[0] = (Cb[0]) | (Y[0] << 10) | (Cr[0] << 20);
            out[1] = (Y[1]) | (Cb[1] << 10) | (Y[2] << 20);
            out[2] = (Cr[1]) | (Y[3] << 10) | (Cb[2] << 20);
            out[3] = (Y[4]) | (Cr[2] << 10) | (Y[5] << 20);
            out += 4;
        }
    }
}

// ============================================================
// Main Processing Thread (Zero Allocation Loop)
// ============================================================
void BroadcastEngine::NDIThread()
{
    using clock = std::chrono::steady_clock;
    Logger::Log(L"NDI thread started");

    BroadcastConfig cfg;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        cfg = config;
    }

    // --- DeckLink Setup ---
    if (cfg.enableDeckLink && InitDeckLink(cfg.outputWidth, cfg.outputHeight, cfg.outputFpsN, cfg.outputFpsD, cfg.forceInterlaced)) {
        deckLinkAvailable = true;
        statDecklinkPresent = true;
    }
    else {
        deckLinkAvailable = false;
        statDecklinkPresent = false;
    }

    Logger::Log(L"DesktopCapture: " + std::to_wstring(cfg.enableDesktopCapture));
    Logger::Log(L"NDI Input: " + std::to_wstring(cfg.enableNdiInput));

    // --- Create Senders (Only if a name is provided) ---

        // Fill Sender
    if (!cfg.ndiOutputFill.empty()) {
        NDIlib_send_create_t fillDesc{};
        fillDesc.p_ndi_name = cfg.ndiOutputFill.c_str();
        ndiSendFill = NDIlib_send_create(&fillDesc);
        Logger::Log(L"NDI Fill Sender Created: " + std::wstring(cfg.ndiOutputFill.begin(), cfg.ndiOutputFill.end()));
    }
    else {
        ndiSendFill = nullptr;
        Logger::Log(L"NDI Fill Sender: DISABLED (Empty Name)");
    }

    // Key Sender
    if (!cfg.ndiOutputKey.empty()) {
        NDIlib_send_create_t keyDesc{};
        keyDesc.p_ndi_name = cfg.ndiOutputKey.c_str();
        ndiSendKey = NDIlib_send_create(&keyDesc);
        Logger::Log(L"NDI Key Sender Created: " + std::wstring(cfg.ndiOutputKey.begin(), cfg.ndiOutputKey.end()));
    }
    else {
        ndiSendKey = nullptr;
        Logger::Log(L"NDI Key Sender: DISABLED (Empty Name)");
    }

    // Embedded Sender
    if (!cfg.ndiOutputEmbedded.empty()) {
        NDIlib_send_create_t embDesc{};
        embDesc.p_ndi_name = cfg.ndiOutputEmbedded.c_str();
        ndiSendEmbedded = NDIlib_send_create(&embDesc);
        Logger::Log(L"NDI Embedded Sender Created: " + std::wstring(cfg.ndiOutputEmbedded.begin(), cfg.ndiOutputEmbedded.end()));
    }
    else {
        ndiSendEmbedded = nullptr;
        Logger::Log(L"NDI Embedded Sender: DISABLED (Empty Name)");
    }

    // --- Create Receivers (Once, Persistent) ---
    if (cfg.enableNdiInput)
    {
        Logger::Log(L"Initializing NDI receivers...");

        // 1. Initialize Primary (Only if master NDI is ON and name is NOT empty)
        if (!cfg.ndiInputPrimary.empty())
        {
            NDIlib_recv_create_v3_t recvDescPrim{};
            recvDescPrim.color_format = NDIlib_recv_color_format_BGRX_BGRA;
            recvDescPrim.bandwidth = cfg.ndiHighestQuality ?
                NDIlib_recv_bandwidth_highest : NDIlib_recv_bandwidth_lowest;

            recvDescPrim.allow_video_fields = cfg.forceInterlaced;
            recvDescPrim.source_to_connect_to.p_ndi_name = cfg.ndiInputPrimary.c_str();

            ndiRecvPrimary = NDIlib_recv_create_v3(&recvDescPrim);

            if (!ndiRecvPrimary)
                Logger::Log(L"Failed to create primary NDI receiver: " + std::wstring(cfg.ndiInputPrimary.begin(), cfg.ndiInputPrimary.end()));
        }
        else {
            ndiRecvPrimary = nullptr;
            Logger::Log(L"Primary NDI Input: DISABLED (Empty Name)");
        }

        // 2. Initialize Backup (Only if master NDI is ON and name is NOT empty)
        if (!cfg.ndiInputBackup.empty())
        {
            NDIlib_recv_create_v3_t recvDescBack{};
            recvDescBack.color_format = NDIlib_recv_color_format_BGRX_BGRA;
            recvDescBack.bandwidth = cfg.ndiHighestQuality ?
                NDIlib_recv_bandwidth_highest : NDIlib_recv_bandwidth_lowest;

            recvDescBack.allow_video_fields = cfg.forceInterlaced;
            recvDescBack.source_to_connect_to.p_ndi_name = cfg.ndiInputBackup.c_str();

            ndiRecvBackup = NDIlib_recv_create_v3(&recvDescBack);

            if (!ndiRecvBackup)
                Logger::Log(L"Failed to create backup NDI receiver: " + std::wstring(cfg.ndiInputBackup.begin(), cfg.ndiInputBackup.end()));
        }
        else {
            ndiRecvBackup = nullptr;
            Logger::Log(L"Backup NDI Input: DISABLED (Empty Name)");
        }
    }
    else
    {
        Logger::Log(L"NDI input disabled in config via boolean.");
        ndiRecvPrimary = nullptr;
        ndiRecvBackup = nullptr;
    }

    // --- Frame Timing & Pool Setup ---
    double fps = double(cfg.outputFpsN) / cfg.outputFpsD;
    if (cfg.forceInterlaced && cfg.outputFpsN >= 50) {
        fps = fps / 2.0;
    }

    auto frameDurationSW = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / fps));
    nextFrameTimeSW = clock::now();

    size_t bufferSize = cfg.outputWidth * cfg.outputHeight * 4;
    for (auto& slot : framePool) {
        slot.fill = std::make_unique<uint8_t[]>(bufferSize);
        slot.key = std::make_unique<uint8_t[]>(bufferSize);
        slot.embedded = std::make_unique<uint8_t[]>(bufferSize); // NEW: Allocate 3rd buffer safely
    }

    uint64_t frameCounter = 0;
    auto fpsStart = clock::now();
    int noSignalCounter = 0;


    int totalPixels = cfg.outputWidth * cfg.outputHeight;
    std::vector<int> parallelIndices(totalPixels);
    std::iota(parallelIndices.begin(), parallelIndices.end(), 0);

    std::vector<int> rowIndices(cfg.outputHeight);
    std::iota(rowIndices.begin(), rowIndices.end(), 0);

    // ==========================================================
    // MAIN LOOP
    // ==========================================================
    while (running)
    {
        bool gpuFrameCaptured = false;
        uint8_t* gpuFrameData = nullptr;
        int gpuStride = 0;

        if (gpuCaptureAvailable)
        {
            DXGI_OUTDUPL_FRAME_INFO frameInfo{};
            ComPtr<IDXGIResource> resource;

            HRESULT hr = dxgiDuplication->AcquireNextFrame(16, &frameInfo, &resource);

            if (SUCCEEDED(hr))
            {
                ComPtr<ID3D11Texture2D> tex;
                resource.As(&tex);

                d3dContext->CopyResource(stagingTexture.Get(), tex.Get());

                D3D11_MAPPED_SUBRESOURCE mapped{};

                if (SUCCEEDED(d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
                {
                    gpuFrameData = (uint8_t*)mapped.pData;
                    gpuStride = mapped.RowPitch;
                    gpuFrameCaptured = true;

                    // d3dContext->Unmap(stagingTexture.Get(), 0);
                }

                dxgiDuplication->ReleaseFrame();
            }
            else if (hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                // normal when no new frame
            }
            else if (hr == DXGI_ERROR_ACCESS_LOST)
            {
                gpuCaptureAvailable = false;
                Logger::Log(L"Desktop duplication lost.");
            }
                        
        }


        // --- Hybrid Capture & Failover Block ---
        NDIlib_video_frame_v2_t frame{};
        NDIlib_recv_instance_t activeRecv = nullptr;
        auto type = NDIlib_frame_type_none;

        // 1. SELECT ACTIVE RECEIVER
        if (!gpuFrameCaptured && cfg.enableNdiInput)
        {
            // Only use backup if the user is currently in backup mode AND a backup receiver exists
            if (usingBackup && ndiRecvBackup) {
                activeRecv = ndiRecvBackup;
            }
            else {
                activeRecv = ndiRecvPrimary;
                usingBackup = false; // Reset if backup is missing
            }

            if (activeRecv)
                type = NDIlib_recv_capture_v2(activeRecv, &frame, nullptr, nullptr, 10);
        }
        else if (gpuFrameCaptured)
        {
            type = NDIlib_frame_type_video;
        }

        // 2. SIGNAL LOSS & FAILOVER LOGIC
        if (type != NDIlib_frame_type_video)
        {
            noSignalCounter++;

            // Only attempt failover if NDI input is enabled and a backup source actually exists
            if (cfg.enableNdiInput && !cfg.ndiInputBackup.empty() && ndiRecvBackup)
            {
                if (noSignalCounter > cfg.flapThreshold)
                {
                    usingBackup = !usingBackup;
                    statUsingBackup.store(usingBackup.load());
                    noSignalCounter = 0;

                    std::string activeName = usingBackup ? cfg.ndiInputBackup : cfg.ndiInputPrimary;
                    Logger::Log(L"Signal flap detected. Active NDI source: " +
                        std::wstring(activeName.begin(), activeName.end()));
                }
            }

            auto now = clock::now();
            if (nextFrameTimeSW > now) {
                std::this_thread::sleep_until(nextFrameTimeSW);
            }
            nextFrameTimeSW += frameDurationSW;

            // THE CADENCE FIX: Resubmit the last frame to maintain 50p output
            if (poolIndex > 0) {
                // Grab the LAST successfully processed frame from the pool
                auto& lastSlot = framePool[(poolIndex - 1) % framePool.size()];

                NDIlib_video_frame_v2_t out{};
                out.xres = cfg.outputWidth;
                out.yres = cfg.outputHeight;
                out.FourCC = NDIlib_FourCC_type_BGRA;
                out.line_stride_in_bytes = cfg.outputWidth * 4;
                out.frame_rate_N = cfg.outputFpsN;
                out.frame_rate_D = cfg.outputFpsD;
                out.picture_aspect_ratio = (float)cfg.outputWidth / (float)cfg.outputHeight;

                out.frame_format_type = cfg.forceInterlaced ?
                    NDIlib_frame_format_type_interleaved :
                    NDIlib_frame_format_type_progressive;

                // Re-transmit the memory buffers asynchronously 
                out.p_data = lastSlot.fill.get();
                NDIlib_send_send_video_async_v2(ndiSendFill, &out);

                out.p_data = lastSlot.key.get();
                NDIlib_send_send_video_async_v2(ndiSendKey, &out);

                out.p_data = lastSlot.embedded.get();
                NDIlib_send_send_video_async_v2(ndiSendEmbedded, &out);


                // --- Feed the DeckLink before we continue! ---
                if (deckLinkAvailable && output)
                {
                    uint32_t buffered = 0;
                    output->GetBufferedVideoFrameCount(&buffered);

                    if (buffered < 6) // Just feed it 1 frame to keep it alive
                    {
                        ComPtr<IDeckLinkMutableVideoFrame> deckFrame;
                        int stride = ((cfg.outputWidth + 47) / 48) * 128;

                        if (SUCCEEDED(output->CreateVideoFrame(cfg.outputWidth, cfg.outputHeight, stride, bmdFormat10BitYUV, bmdFrameFlagDefault, &deckFrame)))
                        {
                            void* bytes = nullptr;
                            deckFrame->GetBytes(&bytes);

                            // Re-convert the last good Fill frame for SDI
                            ConvertBGRAtoV210_SIMD(lastSlot.fill.get(), (uint32_t*)bytes, cfg.outputWidth, cfg.outputHeight);

                            output->ScheduleVideoFrame(deckFrame.Get(), nextFrameTimeHW, frameDurHW, decklinkTimeScale);
                            nextFrameTimeHW += frameDurHW;
                        }
                    }
                }


                // We successfully output a 50p frame, so increment the counter!
                frameCounter++;
            }

            continue;
        }

        noSignalCounter = 0;

        int idx = poolIndex++ % framePool.size();
        auto& slot = framePool[idx];

        int inW = gpuFrameCaptured ? gpuCaptureWidth : frame.xres;
        int inH = gpuFrameCaptured ? gpuCaptureHeight : frame.yres;
        int outW = cfg.outputWidth;
        int outH = cfg.outputHeight;

        // Scale directly into the Embedded buffer (keeps native alpha)
        uint8_t* sourceData = gpuFrameCaptured ? gpuFrameData : frame.p_data;
        int sourceStride = gpuFrameCaptured ? gpuStride : frame.line_stride_in_bytes;

        if (inW == outW && inH == outH)
        {
            // Fast path: Just copy the memory
            memcpy(slot.embedded.get(), sourceData, outW * outH * 4);
        }
        else
        {
            // Slow path: Scale
            libyuv::ARGBScale(
                sourceData,
                sourceStride,
                inW,
                inH,
                slot.embedded.get(),
                outW * 4,
                outW,
                outH,
                libyuv::kFilterLinear // <-- Use Linear to keep the ticker sharp
            );
        }

        if (gpuFrameCaptured)
        {
            d3dContext->Unmap(stagingTexture.Get(), 0);
        }

        // Derive Fill and Key from the Embedded buffer using Parallel Execution
        uint8_t* emb = slot.embedded.get();
        uint8_t* fill = slot.fill.get();
        uint8_t* key = slot.key.get();

        std::for_each(std::execution::par_unseq, rowIndices.begin(), rowIndices.end(), [&](int y) {
            int rowOffset = y * outW * 4;
            for (int x = 0; x < outW * 4; x += 4) {
                int offset = rowOffset + x;
                uint8_t a = emb[offset + 3];

                fill[offset + 0] = emb[offset + 0]; // B
                fill[offset + 1] = emb[offset + 1]; // G
                fill[offset + 2] = emb[offset + 2]; // R
                fill[offset + 3] = 255;             // Solid Alpha

                key[offset + 0] = a; key[offset + 1] = a; key[offset + 2] = a; key[offset + 3] = 255;
            }
            });

        // Send to output NDI (Async NDI with Null-Safety)
        NDIlib_video_frame_v2_t out{};
        out.xres = outW;
        out.yres = outH;
        out.FourCC = NDIlib_FourCC_type_BGRA;
        out.line_stride_in_bytes = outW * 4;
        out.picture_aspect_ratio = (float)outW / (float)outH;

        // Handle frame rate logic once
        out.frame_rate_N = cfg.outputFpsN;
        out.frame_rate_D = cfg.outputFpsD;
        if (cfg.forceInterlaced && cfg.outputFpsN >= 50) {
            out.frame_rate_N = cfg.outputFpsN / 2;
        }

        out.frame_format_type = cfg.forceInterlaced ?
            NDIlib_frame_format_type_interleaved :
            NDIlib_frame_format_type_progressive;

        // Only send if the instance was successfully created (not nullptr)
        if (ndiSendFill) {
            out.p_data = slot.fill.get();
            NDIlib_send_send_video_async_v2(ndiSendFill, &out);
        }

        if (ndiSendKey) {
            out.p_data = slot.key.get();
            NDIlib_send_send_video_async_v2(ndiSendKey, &out);
        }

        if (ndiSendEmbedded) {
            out.p_data = slot.embedded.get();
            NDIlib_send_send_video_async_v2(ndiSendEmbedded, &out);
        }

        // DeckLink Genlock & Output Processing
        if (deckLinkAvailable && output)
        {
            BMDReferenceStatus refStatus;
            if (output->GetReferenceStatus(&refStatus) == S_OK) {
                decklinkClockValid = (refStatus & bmdReferenceLocked);
                statGenlockLocked.store(decklinkClockValid.load());
            }

            uint32_t buffered = 0;
            output->GetBufferedVideoFrameCount(&buffered);

            if (buffered < 6)
            {
                ComPtr<IDeckLinkMutableVideoFrame> deckFrame;
                int stride = ((outW + 47) / 48) * 128;

                if (SUCCEEDED(output->CreateVideoFrame(outW, outH, stride, bmdFormat10BitYUV, bmdFrameFlagDefault, &deckFrame)))
                {
                    void* bytes = nullptr;
                    deckFrame->GetBytes(&bytes);

                    // Send the FILL buffer to the DeckLink (no alpha channel required for baseband SDI here)
                    ConvertBGRAtoV210_SIMD(slot.fill.get(), (uint32_t*)bytes, outW, outH);

                    output->ScheduleVideoFrame(deckFrame.Get(), nextFrameTimeHW, frameDurHW, decklinkTimeScale);
                    nextFrameTimeHW += frameDurHW;
                }
            }
        }

        // Cleanup & Software Cadence Fallback


        if (!gpuFrameCaptured && type == NDIlib_frame_type_video && activeRecv)
        {
            NDIlib_recv_free_video_v2(activeRecv, &frame);
        }

        if (!deckLinkAvailable || !decklinkClockValid) {
            nextFrameTimeSW += frameDurationSW;
            auto now = clock::now();
            if (nextFrameTimeSW > now) {
                std::this_thread::sleep_until(nextFrameTimeSW);
            }
            else {
                statDropped++;
                nextFrameTimeSW = now;
            }
        }

        // 9. Stats Tracking
        frameCounter++;
        auto now = clock::now();
        double elapsed = std::chrono::duration<double>(now - fpsStart).count();
        if (elapsed >= 1.0) {
            statFPS = frameCounter / elapsed;
            frameCounter = 0;
            fpsStart = now;
        }
    }

    Logger::Log(L"NDIThread loop exited cleanly.");
}
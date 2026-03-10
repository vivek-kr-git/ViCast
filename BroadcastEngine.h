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
#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <wrl/client.h>
#include "DeckLinkAPI_h.h"
#include "BroadcastConfig.h"
#include "NDISDK/Include/Processing.NDI.Lib.h"
#include <array>
#include <memory>
#include <dxgi1_2.h>
#include <wrl.h>
#include <d3d11.h> // Required for ID3D11Device

using Microsoft::WRL::ComPtr;

// ============================================================
// Engine Stats (Thread-Safe Snapshot)
// ============================================================
struct EngineStats
{
    double outputFPS = 0.0;
    bool usingBackup = false;
    bool decklinkPresent = false;
    bool genlockLocked = false;
    double driftMs = 0.0;
    uint64_t droppedFrames = 0;
};

// ============================================================
// Broadcast Engine
// ============================================================
class BroadcastEngine : public IDeckLinkVideoOutputCallback
{
public:
    explicit BroadcastEngine(const BroadcastConfig& cfg); // 'explicit' prevents implicit conversions
    ~BroadcastEngine();

    // ---------------- Public API ----------------
    bool Initialize();
    void Start();
    void Stop();
    EngineStats GetStats() const;
    void ResetStats();
    void DumpDiagnostics();

    // ---------------- DeckLink Callbacks ----------------
    STDMETHOD(ScheduledFrameCompleted)(IDeckLinkVideoFrame*, BMDOutputFrameCompletionResult) override { return S_OK; }
    STDMETHOD(ScheduledPlaybackHasStopped)() override { return S_OK; }

    // ---------------- COM Interface (Inline) ----------------
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        if (iid == IID_IUnknown || iid == IID_IDeckLinkVideoOutputCallback)
        {
            *ppv = this;
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

private:
    // ---------------- Internal State ----------------
    BroadcastConfig config;
    std::atomic<bool> running{ false };

    // NDI State
    NDIlib_recv_instance_t ndiRecvPrimary = nullptr;
    NDIlib_recv_instance_t ndiRecvBackup = nullptr;
    NDIlib_send_instance_t ndiSendFill = nullptr;
    NDIlib_send_instance_t ndiSendKey = nullptr;
    NDIlib_send_instance_t ndiSendEmbedded = nullptr;
    std::atomic<bool> usingBackup{ false };

    // DeckLink State
    ComPtr<IDeckLink> device;
    ComPtr<IDeckLinkOutput> output;
    ComPtr<IDeckLinkKeyer> keyer;
    BMDDisplayMode displayMode = bmdModeUnknown;

    std::atomic<bool> deckLinkInitialized{ false };
    std::atomic<bool> deckLinkAvailable{ false };
    std::atomic<bool> decklinkClockValid{ false };

    // Hardware Sync Tracking
    std::atomic<int64_t> decklinkFrameCounter{ 0 };
    BMDTimeScale decklinkTimeScale = 1000000;
    BMDTimeValue frameDurHW = 0;
    BMDTimeValue nextFrameTimeHW = 0;

    // D3D11 State
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    ComPtr<ID3D11Texture2D> inputTex;
    ComPtr<ID3D11Texture2D> outputTex;

    // Desktop Duplication specific state
    ComPtr<IDXGIOutput1> dxgiOutput1;
    ComPtr<IDXGIResource> dxgiResource;
    DXGI_OUTDUPL_FRAME_INFO dxgiFrameInfo = {};

    // GPU Capture (Desktop Duplication)
    ComPtr<IDXGIOutputDuplication> dxgiDuplication;
    ComPtr<ID3D11Texture2D> captureTexture;
    ComPtr<ID3D11Texture2D> stagingTexture;

    int gpuCaptureWidth = 0;
    int gpuCaptureHeight = 0;

    bool gpuCaptureAvailable = false;

    // Lock-Free Frame Pool
    struct FrameSlot
    {
        std::unique_ptr<uint8_t[]> fill;
        std::unique_ptr<uint8_t[]> key;
        std::unique_ptr<uint8_t[]> embedded;
    };
    std::array<FrameSlot, 16> framePool;
    std::atomic<int> poolIndex{ 0 };

    // Threading & Synchronization
    std::thread ndiThread;
    std::thread configWatcher;
    std::mutex configMutex;
    std::mutex ndiMutex;
    std::mutex ndiSendMutex;

    // Software Clock Fallback
    std::chrono::steady_clock::time_point nextFrameTimeSW;

    // Telemetry
    std::atomic<double> statFPS{ 0.0 };
    std::atomic<bool> statUsingBackup{ false };
    std::atomic<bool> statDecklinkPresent{ false };
    std::atomic<bool> statGenlockLocked{ false };
    std::atomic<double> statDriftMs{ 0.0 };
    std::atomic<uint64_t> statDropped{ 0 };

    // File Watching
    uint64_t lastWriteTime = 0;
    std::chrono::steady_clock::time_point lastPrimaryFrame;
    std::chrono::milliseconds failoverTimeout{ 1000 };

    // ---------------- Private Helpers ----------------
    void NDIThread();
    void ConfigWatcherThread();
    void ApplyLiveConfig(const BroadcastConfig& newCfg);
    void ReconnectNDI();

    bool InitDeckLink(int w, int h, int& fpsN, int& fpsD, bool& interlaced);
    void ConvertBGRAtoV210(uint8_t* src, uint32_t* dst, int w, int h);
    void ConvertAlphaToV210(uint8_t* src, uint32_t* dst, int w, int h);
    void ConvertBGRAtoV210_SIMD(uint8_t* src, uint32_t* dst, int width, int height); // Required for .cpp
};
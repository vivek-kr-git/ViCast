#include "ServiceMain.h"
#include "BroadcastEngine.h"
#include "BroadcastConfig.h"
#include "Logger.h"
#include <memory>
#include <iostream>

static SERVICE_STATUS_HANDLE statusHandle;
static SERVICE_STATUS serviceStatus;
static std::unique_ptr<BroadcastEngine> engine;

// -------------------- Service Control Handler --------------------
void WINAPI ServiceCtrlHandler(DWORD ctrl)
{
    if (ctrl == SERVICE_CONTROL_STOP)
    {
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(statusHandle, &serviceStatus);

        if (engine)
            engine->Stop();

        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(statusHandle, &serviceStatus);
    }
}

// -------------------- Service Main --------------------
void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    statusHandle = RegisterServiceCtrlHandler(L"ViCastService", ServiceCtrlHandler);

    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(statusHandle, &serviceStatus);

    // -------------------- Load Config --------------------
    BroadcastConfig cfg = BroadcastConfig::Load("C:\\Dev\\ViCast\\ViCastService\\config\\config.json");

    // -------------------- Initialize Logging --------------------
    Logger::Init(cfg.logFile);
    Logger::Log(L"Service starting...");

    // -------------------- Initialize Engine --------------------
    engine = std::make_unique<BroadcastEngine>(cfg);

    if (!engine->Initialize())
    {
        Logger::Log(L"Engine failed to initialize.");
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(statusHandle, &serviceStatus);
        return;
    }

    // -------------------- Start Engine --------------------
    try
    {
        engine->Start();
    }
    catch (const std::exception& ex)
    {
        Logger::Log(L"Engine exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(statusHandle, &serviceStatus);
        return;
    }
    catch (...)
    {
        Logger::Log(L"Engine failed to start (unknown exception).");
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(statusHandle, &serviceStatus);
        return;
    }

    // -------------------- Update Service Status --------------------
    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(statusHandle, &serviceStatus);

    Logger::Log(L"Service running.");
}

// -------------------- Service Host --------------------
void ServiceHost::Run()
{
    SERVICE_TABLE_ENTRY table[] =
    {
        { (LPWSTR)L"ViCastService", ServiceMain },
        { nullptr, nullptr }
    };

    StartServiceCtrlDispatcher(table);
}
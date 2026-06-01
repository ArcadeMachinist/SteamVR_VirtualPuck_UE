#include <openvr_driver.h>
#include "tracker_device.h"
#include "config.h"
#include "log.h"
#include <memory>
#include <filesystem>
#include <windows.h>

static HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        g_hModule = hModule;
    return TRUE;
}

static std::string GetConfigPath() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(g_hModule, buf, MAX_PATH);
    // DLL lives at: <driver_root>/bin/win64/driver_arcadecabvr.dll
    // Config lives at: <driver_root>/config.json
    namespace fs = std::filesystem;
    fs::path configPath = fs::path(buf)
                              .parent_path()  // win64
                              .parent_path()  // bin
                              .parent_path()  // driver root
                          / "config.json";
    return configPath.string();
}

static std::unique_ptr<TrackerDevice> g_tracker;

class VRPlayServerProvider : public vr::IServerTrackedDeviceProvider {
    bool         m_trackerAdded = false;
    DriverConfig m_config;

    void AddTracker() {
        m_trackerAdded = true;
        vr::VRServerDriverHost()->TrackedDeviceAdded(
            m_config.trackerName.c_str(),
            vr::TrackedDeviceClass_GenericTracker,
            g_tracker.get()
        );
    }

public:
    vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override {
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

        // Required: SteamVR blocks non-HMD drivers from adding devices by default.
        vr::VRSettings()->SetBool(
            vr::k_pch_SteamVR_Section,
            "activateMultipleDrivers",
            true
        );

        m_config  = LoadConfig(GetConfigPath());
        g_tracker = std::make_unique<TrackerDevice>(m_config);
        return vr::VRInitError_None;
    }

    void Cleanup() override {
        g_tracker.reset();
        VR_CLEANUP_SERVER_DRIVER_CONTEXT();
    }

    const char* const* GetInterfaceVersions() override {
        return vr::k_InterfaceVersions;
    }

    void RunFrame() override {
        // Re-register whenever the tracker is not active and an HMD is connected.
        // bDeviceIsConnected covers both active and sleeping wired HMDs.
        // For wireless HMDs (e.g. Quest via vrlink), the placeholder device at index 0
        // has bDeviceIsConnected = false until the headset connects.
        if (m_trackerAdded && g_tracker->IsActive()) return;

        vr::TrackedDevicePose_t hmdPose{};
        vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0.f, &hmdPose, 1);

        if (hmdPose.bDeviceIsConnected) {
            m_trackerAdded = false;
            AddTracker();
        }
    }

    bool ShouldBlockStandbyMode() override { return false; }
    void EnterStandby() override {}
    void LeaveStandby() override {}
};

static VRPlayServerProvider g_provider;

extern "C" __declspec(dllexport)
void* HmdDriverFactory(const char* pInterfaceName, int* pReturnCode) {
    if (strcmp(pInterfaceName, vr::IServerTrackedDeviceProvider_Version) == 0)
        return &g_provider;

    if (pReturnCode)
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}

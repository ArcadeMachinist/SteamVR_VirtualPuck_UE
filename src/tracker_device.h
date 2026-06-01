#pragma once
#include <openvr_driver.h>
#include "config.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>

class UdpListener;

class TrackerDevice : public vr::ITrackedDeviceServerDriver {
public:
    explicit TrackerDevice(const DriverConfig& cfg);
    ~TrackerDevice();

    vr::EVRInitError Activate(uint32_t unObjectId) override;
    void Deactivate() override;
    void EnterStandby() override {}
    void* GetComponent(const char* pchComponentNameAndVersion) override { return nullptr; }
    void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override {}
    vr::DriverPose_t GetPose() override;

    bool IsActive() const { return m_deviceId != vr::k_unTrackedDeviceIndexInvalid; }

private:
    DriverConfig  m_config;
    uint32_t      m_deviceId = vr::k_unTrackedDeviceIndexInvalid;
    vr::DriverPose_t m_pose{};

    std::atomic<bool> m_running{ false };
    std::thread       m_poseThread;

    float m_pitch = 0.0f;
    float m_roll  = 0.0f;

    vr::VRInputComponentHandle_t m_compSystemClick{};

    // Network mode state
    std::mutex   m_networkMutex;
    bool         m_hasReceivedPacket = false;
    std::chrono::steady_clock::time_point m_lastPacketTime;
    std::unique_ptr<UdpListener> m_udpListener;

    void PoseThread();
    void MockPoseLoop();
    void NetworkPoseLoop();
};

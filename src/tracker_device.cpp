#include "tracker_device.h"
#include "udp_listener.h"
#include "log.h"
#include <cmath>
#include <random>
#include <chrono>

using namespace std::chrono;

// Standard aerospace Euler (pitch, yaw, roll) -> quaternion
static vr::HmdQuaternion_t EulerToQuat(float pitch, float yaw, float roll) {
    float cy = cosf(yaw   * 0.5f), sy = sinf(yaw   * 0.5f);
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    float cr = cosf(roll  * 0.5f), sr = sinf(roll  * 0.5f);

    return {
        (double)(cr * cp * cy + sr * sp * sy),  // w
        (double)(sr * cp * cy - cr * sp * sy),  // x
        (double)(cr * sp * cy + sr * cp * sy),  // y
        (double)(cr * cp * sy - sr * sp * cy)   // z
    };
}

TrackerDevice::TrackerDevice(const DriverConfig& cfg) : m_config(cfg) {
    m_pose.poseIsValid        = (cfg.mode == DriverMode::Mock);
    m_pose.result             = (cfg.mode == DriverMode::Mock)
                                    ? vr::TrackingResult_Running_OK
                                    : vr::TrackingResult_Running_OutOfRange;
    m_pose.deviceIsConnected  = true;
    m_pose.qWorldFromDriverRotation = { 1.0, 0.0, 0.0, 0.0 };
    m_pose.qDriverFromHeadRotation  = { 1.0, 0.0, 0.0, 0.0 };
    m_pose.vecPosition[0] = 0.0;
    m_pose.vecPosition[1] = 1.0;  // 1 m above floor
    m_pose.vecPosition[2] = 0.0;
    m_pose.qRotation = { 1.0, 0.0, 0.0, 0.0 };
}

TrackerDevice::~TrackerDevice() {
    m_running = false;
    if (m_poseThread.joinable())
        m_poseThread.join();
}

vr::EVRInitError TrackerDevice::Activate(uint32_t unObjectId) {
    m_deviceId = unObjectId;

    vr::PropertyContainerHandle_t props =
        vr::VRProperties()->TrackedDeviceToPropertyContainer(m_deviceId);

    vr::VRProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "arcadecabvr");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ManufacturerName_String,   "ArcadeCabVR");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,        m_config.trackerModel.c_str());
    vr::VRProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String,       m_config.trackerName.c_str());
    vr::VRProperties()->SetBoolProperty(props,   vr::Prop_WillDriftInYaw_Bool,       false);
    vr::VRProperties()->SetBoolProperty(props,   vr::Prop_DeviceIsWireless_Bool,     true);
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ControllerType_String,     "arcadecabvr_tracker");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String,   "{arcadecabvr}/input/arcadecabvr_tracker_profile.json");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,    "locator");

    vr::VRDriverInput()->CreateBooleanComponent(props, "/input/system/click", &m_compSystemClick);

    m_running   = true;
    m_poseThread = std::thread(&TrackerDevice::PoseThread, this);

    VRLog("[ArcadeCabVR] Tracker activated (id=%u mode=%s)", unObjectId,
        m_config.mode == DriverMode::Network ? "network" : "mock");

    return vr::VRInitError_None;
}

void TrackerDevice::Deactivate() {
    m_running = false;
    if (m_poseThread.joinable())
        m_poseThread.join();
    m_deviceId = vr::k_unTrackedDeviceIndexInvalid;
}

vr::DriverPose_t TrackerDevice::GetPose() {
    std::lock_guard<std::mutex> lock(m_networkMutex);
    return m_pose;
}

void TrackerDevice::PoseThread() {
    if (m_config.mode == DriverMode::Network)
        NetworkPoseLoop();
    else
        MockPoseLoop();
}

void TrackerDevice::MockPoseLoop() {
    std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> delta(-0.008f, 0.008f);  // ~0.46 deg/frame max

    constexpr float kLimit = 3.14159265f / 4.0f;  // ±45° soft ceiling

    while (m_running) {
        m_pitch += delta(rng);
        m_roll  += delta(rng);

        m_pitch = std::max(-kLimit, std::min(kLimit, m_pitch));
        m_roll  = std::max(-kLimit, std::min(kLimit, m_roll));

        // EulerToQuat param mapping to UE axes:
        //   yaw   param (OVR Z) -> UE Roll
        //   roll  param (OVR X) -> UE Pitch  (negated for correct direction)
        //   pitch param (OVR Y) -> UE Yaw    (not used, stays 0)
        m_pose.qRotation = EulerToQuat(0.0f, m_roll, -m_pitch);

        if (m_deviceId != vr::k_unTrackedDeviceIndexInvalid)
            vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_deviceId, m_pose, sizeof(vr::DriverPose_t));

        std::this_thread::sleep_for(milliseconds(11));  // ~90 Hz
    }
}

void TrackerDevice::NetworkPoseLoop() {
    m_udpListener = std::make_unique<UdpListener>(
        m_config,
        [this](float pitch, float roll) {
            std::lock_guard<std::mutex> lock(m_networkMutex);
            m_pitch              = pitch;
            m_roll               = roll;
            m_lastPacketTime     = steady_clock::now();
            m_hasReceivedPacket  = true;
        },
        [this]() {
            std::lock_guard<std::mutex> lock(m_networkMutex);
            m_pitch              = 0.0f;
            m_roll               = 0.0f;
            m_lastPacketTime     = steady_clock::now();
            m_hasReceivedPacket  = true;
        }
    );
    m_udpListener->Start();

    while (m_running) {
        vr::DriverPose_t pose;
        {
            std::lock_guard<std::mutex> lock(m_networkMutex);

            bool poseValid = false;
            if (m_hasReceivedPacket) {
                if (m_config.invalidatePoseAfterMs < 0) {
                    poseValid = true;
                } else {
                    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - m_lastPacketTime).count();
                    poseValid    = elapsed <= m_config.invalidatePoseAfterMs;
                }
            }

            m_pose.poseIsValid = poseValid;
            m_pose.result      = poseValid ? vr::TrackingResult_Running_OK
                                           : vr::TrackingResult_Running_OutOfRange;
            m_pose.qRotation   = EulerToQuat(0.0f, m_roll, -m_pitch);
            pose               = m_pose;
        }

        if (m_deviceId != vr::k_unTrackedDeviceIndexInvalid)
            vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_deviceId, pose, sizeof(vr::DriverPose_t));

        std::this_thread::sleep_for(milliseconds(11));  // ~90 Hz
    }

    m_udpListener->Stop();
    m_udpListener.reset();
}

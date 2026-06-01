#pragma once
#include "config.h"
#include <functional>
#include <atomic>
#include <thread>
#include <cstdint>

class UdpListener {
public:
    using FrameCallback = std::function<void(float pitch, float roll)>;
    using ResetCallback = std::function<void()>;

    UdpListener(const DriverConfig& cfg, FrameCallback onFrame, ResetCallback onReset);
    ~UdpListener();

    void Start();
    void Stop();

private:
    void ListenThread();
    bool MatchesCIDR(uint32_t hostOrderAddr) const;

    DriverConfig  m_cfg;
    FrameCallback m_onFrame;
    ResetCallback m_onReset;

    std::atomic<bool> m_running{ false };
    std::thread       m_thread;

    uint64_t m_lastFrameId  = 0;
    bool     m_hasSeenFrame = false;

    uint32_t m_cidrNetwork = 0;
    uint32_t m_cidrMask    = 0;
};

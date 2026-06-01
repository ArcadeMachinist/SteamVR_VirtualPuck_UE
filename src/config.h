#pragma once
#include <string>

enum class DriverMode { Mock, Network };

struct DriverConfig {
    DriverMode  mode                  = DriverMode::Mock;
    bool        debug                 = false;
    std::string trackerName           = "ARCADECABVR_01";
    std::string trackerModel          = "ArcadeCabVR Tracker v1";
    std::string listenIP              = "239.255.42.120";
    std::string localIP               = "0.0.0.0";
    int         listenPort            = 8205;
    std::string clientCIDR            = "0.0.0.0/0";
    int         invalidatePoseAfterMs = 2000;
};

// Returns defaults if the file is missing or malformed.
DriverConfig LoadConfig(const std::string& path);

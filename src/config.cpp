#include "config.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

DriverConfig LoadConfig(const std::string& path) {
    DriverConfig cfg;

    std::ifstream f(path);
    if (!f.is_open()) {
        VRLog("[ArcadeCabVR] config.json not found at %s — using defaults", path.c_str());
        return cfg;
    }

    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        VRLog("[ArcadeCabVR] config.json parse error: %s — using defaults", e.what());
        return cfg;
    }

    auto str = [&](const char* key, std::string& out) {
        if (j.contains(key) && j[key].is_string()) out = j[key].get<std::string>();
    };
    auto i32 = [&](const char* key, int& out) {
        if (j.contains(key) && j[key].is_number_integer()) out = j[key].get<int>();
    };
    auto b = [&](const char* key, bool& out) {
        if (j.contains(key)) {
            if (j[key].is_boolean()) out = j[key].get<bool>();
            else if (j[key].is_number_integer()) out = j[key].get<int>() != 0;
        }
    };

    std::string mode = "mock";
    str("VirtualPuckDriver_Mode", mode);
    cfg.mode = (mode == "network") ? DriverMode::Network : DriverMode::Mock;

    b("VirtualPuckDriver_Debug", cfg.debug);
    str("VirtualPuckDriver_TrackerName",     cfg.trackerName);
    str("VirtualPuckDriver_TrackerModel",    cfg.trackerModel);
    str("VirtualPuckDriver_ListenIP",        cfg.listenIP);
    str("VirtualPuckDriver_LocalIP",         cfg.localIP);
    i32("VirtualPuckDriver_ListenPort",      cfg.listenPort);
    str("VirtualPuckDriver_ClientIP",        cfg.clientCIDR);
    i32("VirtualPuckDriver_InvalidatePoseAfter", cfg.invalidatePoseAfterMs);

    VRLog("[ArcadeCabVR] Config loaded: mode=%s debug=%d listenIP=%s localIP=%s port=%d clientCIDR=%s invalidateAfter=%dms",
        (cfg.mode == DriverMode::Network ? "network" : "mock"),
        (int)cfg.debug,
        cfg.listenIP.c_str(), cfg.localIP.c_str(),
        cfg.listenPort, cfg.clientCIDR.c_str(),
        cfg.invalidatePoseAfterMs);

    return cfg;
}

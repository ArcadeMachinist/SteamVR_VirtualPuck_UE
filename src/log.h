#pragma once
#include <openvr_driver.h>
#include <cstdio>
#include <cstdarg>

inline void VRLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    vr::VRDriverLog()->Log(buf);
}

#include <openvr.h>
#include <cstdio>

int main() {
    vr::EVRInitError err = vr::VRInitError_None;
    vr::IVRSystem* sys = vr::VR_Init(&err, vr::VRApplication_Utility);
    if (err != vr::VRInitError_None) {
        printf("VR_Init failed: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(err));
        return 1;
    }

    printf("%-5s %-18s %-16s %-12s %s\n", "Idx", "Class", "Serial", "Connected", "InputProfile");
    printf("%s\n", "---------------------------------------------------------------------");

    for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
        vr::ETrackedDeviceClass cls = sys->GetTrackedDeviceClass(i);
        if (cls == vr::TrackedDeviceClass_Invalid) continue;

        char serial[128] = {}, profile[256] = {};
        vr::ETrackedPropertyError perr;
        sys->GetStringTrackedDeviceProperty(i, vr::Prop_SerialNumber_String,    serial,  sizeof(serial),  &perr);
        sys->GetStringTrackedDeviceProperty(i, vr::Prop_InputProfilePath_String, profile, sizeof(profile), &perr);

        vr::TrackedDevicePose_t pose;
        sys->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.f, &pose, 1);

        const char* clsStr =
            cls == vr::TrackedDeviceClass_HMD              ? "HMD"        :
            cls == vr::TrackedDeviceClass_Controller        ? "Controller" :
            cls == vr::TrackedDeviceClass_GenericTracker    ? "Tracker"    :
            cls == vr::TrackedDeviceClass_TrackingReference ? "Reference"  : "Other";

        printf("%-5u %-18s %-16s %-12s %s\n",
            i, clsStr, serial,
            pose.bDeviceIsConnected ? "YES" : "no",
            profile[0] ? profile : "(none)");
    }

    vr::VR_Shutdown();
    return 0;
}

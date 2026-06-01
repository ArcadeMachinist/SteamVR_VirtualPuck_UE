#include "TrackerBFL.h"
#include "SteamVRFunctionLibrary.h"

#if PLATFORM_WINDOWS
#include "openvr.h"
#endif

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

namespace ArcadeCabVRInternal
{
    // Convert an OpenVR 3x4 pose matrix to a UE FRotator.
    //
    // OpenVR: right-handed, X=right, Y=up, Z=back
    // UE:     left-handed,  X=forward, Y=right, Z=up
    //
    // Remapping: UE(x,y,z) = OVR(-z, x, y)
    // Quaternion form of that remapping: FQuat(-qz, qx, qy, -qw)
    FRotator MatrixToUERotator(const vr::HmdMatrix34_t& M)
    {
        // Shepperd's method: rotation matrix -> quaternion (in OpenVR space)
        float tr = M.m[0][0] + M.m[1][1] + M.m[2][2];
        float qw, qx, qy, qz;

        if (tr > 0.f)
        {
            float s = 0.5f / FMath::Sqrt(tr + 1.f);
            qw = 0.25f / s;
            qx = (M.m[2][1] - M.m[1][2]) * s;
            qy = (M.m[0][2] - M.m[2][0]) * s;
            qz = (M.m[1][0] - M.m[0][1]) * s;
        }
        else if (M.m[0][0] > M.m[1][1] && M.m[0][0] > M.m[2][2])
        {
            float s = 2.f * FMath::Sqrt(1.f + M.m[0][0] - M.m[1][1] - M.m[2][2]);
            qw = (M.m[2][1] - M.m[1][2]) / s;
            qx = 0.25f * s;
            qy = (M.m[0][1] + M.m[1][0]) / s;
            qz = (M.m[0][2] + M.m[2][0]) / s;
        }
        else if (M.m[1][1] > M.m[2][2])
        {
            float s = 2.f * FMath::Sqrt(1.f + M.m[1][1] - M.m[0][0] - M.m[2][2]);
            qw = (M.m[0][2] - M.m[2][0]) / s;
            qx = (M.m[0][1] + M.m[1][0]) / s;
            qy = 0.25f * s;
            qz = (M.m[1][2] + M.m[2][1]) / s;
        }
        else
        {
            float s = 2.f * FMath::Sqrt(1.f + M.m[2][2] - M.m[0][0] - M.m[1][1]);
            qw = (M.m[1][0] - M.m[0][1]) / s;
            qx = (M.m[0][2] + M.m[2][0]) / s;
            qy = (M.m[1][2] + M.m[2][1]) / s;
            qz = 0.25f * s;
        }

        // Remap OVR quaternion to UE space
        return FQuat(-qz, qx, qy, -qw).Rotator();
    }

    FName SpecialHandName(int32 ZeroBasedIndex)
    {
        return FName(*FString::Printf(TEXT("Special_%d"), ZeroBasedIndex + 1));
    }

#if PLATFORM_WINDOWS
    FString GetStringProp(vr::TrackedDeviceIndex_t Id, vr::ETrackedDeviceProperty Prop)
    {
        vr::IVRSystem* Sys = vr::VRSystem();
        if (!Sys) return FString();

        char Buf[vr::k_unMaxPropertyStringSize];
        vr::ETrackedPropertyError Err;
        Sys->GetStringTrackedDeviceProperty(Id, Prop, Buf, sizeof(Buf), &Err);
        return Err == vr::TrackedProp_Success ? FString(UTF8_TO_TCHAR(Buf)) : FString();
    }

    bool IsPoseValid(vr::TrackedDeviceIndex_t Id)
    {
        vr::IVRSystem* Sys = vr::VRSystem();
        if (!Sys || Id >= vr::k_unMaxTrackedDeviceCount) return false;

        vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
        Sys->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0.f, Poses, vr::k_unMaxTrackedDeviceCount);

        return Poses[Id].bPoseIsValid
            && Poses[Id].eTrackingResult == vr::TrackingResult_Running_OK;
    }
#endif

    TArray<FArcadeCabVRTrackerInfo> BuildList()
    {
        TArray<FArcadeCabVRTrackerInfo> Out;

#if PLATFORM_WINDOWS
        TArray<int32> Ids;
        USteamVRFunctionLibrary::GetValidTrackedDeviceIds(
            ESteamVRTrackedDeviceType::Other, Ids);

        for (int32 i = 0; i < Ids.Num(); ++i)
        {
            const vr::TrackedDeviceIndex_t DevId = (vr::TrackedDeviceIndex_t)Ids[i];

            static const EControllerHand SpecialHands[] = {
                EControllerHand::Special_1, EControllerHand::Special_2,
                EControllerHand::Special_3, EControllerHand::Special_4,
                EControllerHand::Special_5, EControllerHand::Special_6,
                EControllerHand::Special_7, EControllerHand::Special_8,
                EControllerHand::Special_9
            };

            FArcadeCabVRTrackerInfo Info;
            Info.MotionSource   = SpecialHandName(i);
            Info.ControllerHand = (i < 9) ? SpecialHands[i] : EControllerHand::Special_9;
            Info.Serial         = GetStringProp(DevId, vr::Prop_SerialNumber_String);
            Info.Model          = GetStringProp(DevId, vr::Prop_ModelNumber_String);
            Info.bIsTracked     = IsPoseValid(DevId);
            Out.Add(Info);
        }
#endif
        return Out;
    }
}

// ---------------------------------------------------------------------------
// UTrackerBFL
// ---------------------------------------------------------------------------

TArray<FArcadeCabVRTrackerInfo> UTrackerBFL::GetAllTrackers()
{
    return ArcadeCabVRInternal::BuildList();
}

FName UTrackerBFL::FindMotionSourceBySerial(const FString& Serial, bool& bFound)
{
    bFound = false;
    for (const FArcadeCabVRTrackerInfo& T : ArcadeCabVRInternal::BuildList())
    {
        if (T.Serial.Equals(Serial, ESearchCase::IgnoreCase))
        {
            bFound = true;
            return T.MotionSource;
        }
    }
    return NAME_None;
}

FString UTrackerBFL::GetSerialForMotionSource(FName MotionSource)
{
    for (const FArcadeCabVRTrackerInfo& T : ArcadeCabVRInternal::BuildList())
    {
        if (T.MotionSource == MotionSource)
            return T.Serial;
    }
    return FString();
}

bool UTrackerBFL::IsTrackerActiveBySerial(const FString& Serial)
{
    for (const FArcadeCabVRTrackerInfo& T : ArcadeCabVRInternal::BuildList())
    {
        if (T.Serial.Equals(Serial, ESearchCase::IgnoreCase))
            return T.bIsTracked;
    }
    return false;
}

FRotator UTrackerBFL::GetTrackerRotationBySerial(const FString& Serial, bool& bIsValid)
{
    bIsValid = false;

#if PLATFORM_WINDOWS
    vr::IVRSystem* Sys = vr::VRSystem();
    if (!Sys) return FRotator::ZeroRotator;

    TArray<int32> Ids;
    USteamVRFunctionLibrary::GetValidTrackedDeviceIds(ESteamVRTrackedDeviceType::Other, Ids);

    vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
    Sys->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding, 0.f, Poses, vr::k_unMaxTrackedDeviceCount);

    for (int32 Id : Ids)
    {
        FString S = ArcadeCabVRInternal::GetStringProp(
            (vr::TrackedDeviceIndex_t)Id, vr::Prop_SerialNumber_String);

        if (!S.Equals(Serial, ESearchCase::IgnoreCase)) continue;

        const vr::TrackedDevicePose_t& P = Poses[Id];
        if (!P.bPoseIsValid || P.eTrackingResult != vr::TrackingResult_Running_OK)
            return FRotator::ZeroRotator;

        bIsValid = true;
        return ArcadeCabVRInternal::MatrixToUERotator(P.mDeviceToAbsoluteTracking);
    }
#endif

    return FRotator::ZeroRotator;
}

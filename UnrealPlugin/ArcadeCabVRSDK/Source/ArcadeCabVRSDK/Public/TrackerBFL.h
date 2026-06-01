#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "TrackerBFL.generated.h"

USTRUCT(BlueprintType)
struct FArcadeCabVRTrackerInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ArcadeCabVR|Tracker")
    EControllerHand ControllerHand = EControllerHand::Special_1;

    UPROPERTY(BlueprintReadOnly, Category="ArcadeCabVR|Tracker")
    FName MotionSource;

    UPROPERTY(BlueprintReadOnly, Category="ArcadeCabVR|Tracker")
    FString Serial;

    UPROPERTY(BlueprintReadOnly, Category="ArcadeCabVR|Tracker")
    FString Model;

    UPROPERTY(BlueprintReadOnly, Category="ArcadeCabVR|Tracker")
    bool bIsTracked = false;
};

UCLASS()
class ARCADECABVRSDK_API UTrackerBFL : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArcadeCabVR|Tracker")
    static TArray<FArcadeCabVRTrackerInfo> GetAllTrackers();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArcadeCabVR|Tracker")
    static FName FindMotionSourceBySerial(const FString& Serial, bool& bFound);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArcadeCabVR|Tracker")
    static FString GetSerialForMotionSource(FName MotionSource);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArcadeCabVR|Tracker")
    static bool IsTrackerActiveBySerial(const FString& Serial);

    /**
     * Returns the tracker's rotation directly from OpenVR — no UE XR session required.
     * Use this in Tick to drive object pitch/roll.
     * bIsValid is false if the tracker is not connected or not tracking.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArcadeCabVR|Tracker")
    static FRotator GetTrackerRotationBySerial(const FString& Serial, bool& bIsValid);
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceTypes.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceStatus
{
	GENERATED_BODY()

	/** 0-100 when available; otherwise -1. */
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int32 BatteryPercent = -1;

	/** 0-100 when available; otherwise -1. */
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int32 VolumePercent = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bBatteryAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bVolumeAvailable = false;

	bool operator==(const FOpenMobileDeviceStatus& Other) const
	{
		return BatteryPercent == Other.BatteryPercent
			&& VolumePercent == Other.VolumePercent
			&& bBatteryAvailable == Other.bBatteryAvailable
			&& bVolumeAvailable == Other.bVolumeAvailable;
	}

	bool operator!=(const FOpenMobileDeviceStatus& Other) const
	{
		return !(*this == Other);
	}
};

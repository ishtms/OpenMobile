#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileDeviceTypes.h"
#include "OpenMobileDeviceBlueprintLibrary.generated.h"

UCLASS()
class OPENMOBILEDEVICE_API UOpenMobileDeviceBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns battery charge as 0-100 or -1 if the platform cannot report it. */
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	static int32 GetBatteryPercent();

	/** Returns active output/media volume as 0-100 or -1 if unavailable. */
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	static int32 GetVolumePercent();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	static FOpenMobileDeviceStatus GetDeviceStatus();
};

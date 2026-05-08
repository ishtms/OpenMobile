#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceTypes.h"
#include "OpenMobileDeviceBlueprintLibrary.generated.h"

UCLASS()
class OPENMOBILEDEVICE_API UOpenMobileDeviceBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns battery charge as 0-100 or -1 if the platform cannot report it. */
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Battery Percent", ToolTip = "Returns battery charge from 0 to 100, or -1 when the legacy platform API cannot report it."))
	static int32 GetBatteryPercent();

	/** Returns active output/media volume as 0-100 or -1 if unavailable. */
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Volume Percent", ToolTip = "Returns active output or media volume from 0 to 100, or -1 when the legacy platform API cannot report it."))
	static int32 GetVolumePercent();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Status", ToolTip = "Returns the legacy combined battery and active output-volume status."))
	static FOpenMobileDeviceStatus GetDeviceStatus();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Capability", ToolTip = "Returns typed support and restriction information for one stable Device capability name without prompting."))
	static FOpenMobileDeviceCapability GetDeviceCapability(FName CapabilityName);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Capability Report", ToolTip = "Returns typed support and restriction information for every stable Device capability without prompting."))
	static FOpenMobileDeviceCapabilityReport GetDeviceCapabilityReport();
};

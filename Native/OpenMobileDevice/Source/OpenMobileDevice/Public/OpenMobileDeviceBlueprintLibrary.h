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
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DeprecatedFunction, DeprecationMessage = "Use Get Power Snapshot on the Open Mobile Device subsystem for typed availability.", DisplayName = "Get Battery Percent", ToolTip = "Returns battery charge from 0 to 100, or -1 when the typed backend snapshot is unavailable."))
	static int32 GetBatteryPercent();

	/** Returns active output/media volume as 0-100 or -1 if unavailable. */
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DeprecatedFunction, DeprecationMessage = "Use Get Media Volume Snapshot on the Open Mobile Device subsystem for typed availability.", DisplayName = "Get Volume Percent", ToolTip = "Returns active output or media volume from 0 to 100, or -1 when the typed backend snapshot is unavailable."))
	static int32 GetVolumePercent();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DeprecatedFunction, DeprecationMessage = "Use the focused Power and Media Volume snapshots and events on the Open Mobile Device subsystem.", DisplayName = "Get Device Status", ToolTip = "Returns the legacy combined battery and active output-volume status."))
	static FOpenMobileDeviceStatus GetDeviceStatus();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Capability", ToolTip = "Returns typed support and restriction information for one stable Device capability name without prompting."))
	static FOpenMobileDeviceCapability GetDeviceCapability(FName CapabilityName);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Capability Report", ToolTip = "Returns typed support and restriction information for every stable Device capability without prompting."))
	static FOpenMobileDeviceCapabilityReport GetDeviceCapabilityReport();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Format Byte Count", ToolTip = "Formats a non-negative byte count with IEC memory units. Negative values return empty text."))
	static FText FormatByteCount(int64 Bytes);
};

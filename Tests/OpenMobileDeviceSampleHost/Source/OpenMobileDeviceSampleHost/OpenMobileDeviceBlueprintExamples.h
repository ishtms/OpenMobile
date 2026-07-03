#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceBlueprintExamples.generated.h"

class UOpenMobileDeviceMonitoringSubscription;

UCLASS()
class OPENMOBILEDEVICESAMPLEHOST_API UOpenMobileDeviceBlueprintExamples final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Device", meta = (WorldContext = "WorldContextObject"))
	static UOpenMobileDeviceMonitoringSubscription* StartMonitoringSubscription(
		UObject* WorldContextObject,
		UObject* Owner
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Device")
	static bool ShouldReduceQualityForPower(const FOpenMobilePowerSnapshot& Power);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Device")
	static bool ShouldAllowDownload(
		const FOpenMobileStorageSnapshot& Storage,
		const FOpenMobileNetworkPathSnapshot& Network,
		int64 RequiredBytes
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Device")
	static bool IsNetworkHandoff(
		const FOpenMobileNetworkPathSnapshot& Previous,
		const FOpenMobileNetworkPathSnapshot& Current
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Device")
	static bool ShouldRebuildSafeArea(
		const FOpenMobileWindowDisplaySnapshot& Previous,
		const FOpenMobileWindowDisplaySnapshot& Current
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Device")
	static bool ShouldRecoverThroughSettings(
		const FOpenMobileDeviceCapability& Capability
	);
};

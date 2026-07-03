#include "OpenMobileDeviceBlueprintExamples.h"

#include "Engine/GameInstance.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceSubsystem.h"

UOpenMobileDeviceMonitoringSubscription*
UOpenMobileDeviceBlueprintExamples::StartMonitoringSubscription(
	UObject* WorldContextObject,
	UObject* Owner
)
{
	if (!WorldContextObject || !Owner)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UOpenMobileDeviceSubsystem* DeviceSubsystem = GameInstance
		? GameInstance->GetSubsystem<UOpenMobileDeviceSubsystem>()
		: nullptr;
	if (!DeviceSubsystem)
	{
		return nullptr;
	}

	const TArray<EOpenMobileDeviceMonitoringGroup> Groups = {
		EOpenMobileDeviceMonitoringGroup::Power,
		EOpenMobileDeviceMonitoringGroup::Storage,
		EOpenMobileDeviceMonitoringGroup::Network,
		EOpenMobileDeviceMonitoringGroup::WindowDisplay
	};
	return DeviceSubsystem->StartMonitoring(Owner, Groups);
}

bool UOpenMobileDeviceBlueprintExamples::ShouldReduceQualityForPower(
	const FOpenMobilePowerSnapshot& Power
)
{
	return (Power.bPowerSavingEnabled.bIsAvailable
			&& Power.bPowerSavingEnabled.Value)
		|| Power.ThermalState == EOpenMobileThermalState::Serious
		|| Power.ThermalState == EOpenMobileThermalState::Critical;
}

bool UOpenMobileDeviceBlueprintExamples::ShouldAllowDownload(
	const FOpenMobileStorageSnapshot& Storage,
	const FOpenMobileNetworkPathSnapshot& Network,
	const int64 RequiredBytes
)
{
	if (RequiredBytes < 0
		|| !Storage.AvailableBytes.bIsAvailable
		|| Storage.AvailableBytes.Value < RequiredBytes
		|| (Storage.bIsLowStorage.bIsAvailable && Storage.bIsLowStorage.Value))
	{
		return false;
	}

	if (Network.PathState != EOpenMobileNetworkPathState::Available
		&& Network.PathState != EOpenMobileNetworkPathState::InternetCapable)
	{
		return false;
	}

	return !(Network.bIsMetered.bIsAvailable && Network.bIsMetered.Value)
		&& !(Network.bIsExpensive.bIsAvailable && Network.bIsExpensive.Value)
		&& !(Network.bIsConstrained.bIsAvailable && Network.bIsConstrained.Value);
}

bool UOpenMobileDeviceBlueprintExamples::IsNetworkHandoff(
	const FOpenMobileNetworkPathSnapshot& Previous,
	const FOpenMobileNetworkPathSnapshot& Current
)
{
	return Previous.PathState != Current.PathState
		|| Previous.bDefaultTransportAvailable != Current.bDefaultTransportAvailable
		|| (Previous.bDefaultTransportAvailable
			&& Previous.DefaultTransport != Current.DefaultTransport);
}

bool UOpenMobileDeviceBlueprintExamples::ShouldRebuildSafeArea(
	const FOpenMobileWindowDisplaySnapshot& Previous,
	const FOpenMobileWindowDisplaySnapshot& Current
)
{
	return !(Previous.SafeAreaInsets == Current.SafeAreaInsets)
		|| Previous.bLogicalWindowSizeAvailable != Current.bLogicalWindowSizeAvailable
		|| (Previous.bLogicalWindowSizeAvailable
			&& Previous.LogicalWindowSize != Current.LogicalWindowSize)
		|| Previous.Orientation != Current.Orientation
		|| Previous.WindowMode != Current.WindowMode;
}

bool UOpenMobileDeviceBlueprintExamples::ShouldRecoverThroughSettings(
	const FOpenMobileDeviceCapability& Capability
)
{
	return Capability.State == EOpenMobileCapabilityState::PermissionRequired
		|| Capability.State == EOpenMobileCapabilityState::Denied
		|| Capability.State == EOpenMobileCapabilityState::Restricted;
}

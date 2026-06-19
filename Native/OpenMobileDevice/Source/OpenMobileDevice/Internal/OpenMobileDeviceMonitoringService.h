#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceNetworkTypes.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceMonitoringGroupChanged,
	EOpenMobileDeviceMonitoringGroup
);
DECLARE_MULTICAST_DELEGATE(FOpenMobileDeviceMonitoringMaintenance);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceMonitoredNetworkPathChanged,
	const FOpenMobileNetworkPathSnapshot&
);

class OPENMOBILEDEVICE_API FOpenMobileDeviceMonitoringService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid AddSubscription(
		const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
		float FallbackPollingIntervalSeconds
	);
	static void RemoveSubscription(const FGuid& RequestId);
	static void NotifyNativeChange(
		const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken,
		uint64 SourceSequence
	);
	static void NotifyWindowSettled();
	static void RefreshActiveGroups();
	static FOpenMobileDeviceMonitoringGroupChanged& OnGroupChanged();
	static FOpenMobileDeviceMonitoredNetworkPathChanged& OnNetworkPathChanged();
	static FOpenMobileDeviceMonitoringMaintenance& OnMaintenance();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
	static int32 GetReferenceCountForTests(EOpenMobileDeviceMonitoringGroup Group);
	static bool IsTickerActiveForTests();
	static bool UsesFallbackForTests(EOpenMobileDeviceMonitoringGroup Group);
	static float GetEffectiveIntervalForTests(EOpenMobileDeviceMonitoringGroup Group);
	static uint64 GetLastNativeSequenceForTests(
		EOpenMobileDeviceMonitoringGroup Group
	);
	static void NotifyNativeChangeForTests(
		EOpenMobileDeviceMonitoringGroup Group
	);
	static void TickForTests(float DeltaTime);
	static void SetApplicationActiveForTests(bool bActive);
#endif
};

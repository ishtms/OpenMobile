#pragma once

#include "CoreMinimal.h"

struct OPENMOBILESENSORS_API FOpenMobileProximityMonitoringAction
{
	bool bShouldSetMonitoringEnabled = false;
	bool bMonitoringEnabled = false;
};

class OPENMOBILESENSORS_API FOpenMobileProximityMonitoringPolicy final
{
public:
	FOpenMobileProximityMonitoringAction Acquire(
		bool bMonitoringEnabled,
		bool bApplicationActive
	);
	FOpenMobileProximityMonitoringAction Release();
	FOpenMobileProximityMonitoringAction SetApplicationActive(bool bActive);
	FOpenMobileProximityMonitoringAction Shutdown();
	void ObserveMonitoringEnabled(bool bEnabled);
	int32 GetLeaseCount() const;

private:
	FOpenMobileProximityMonitoringAction ResolveDesiredState();

	int32 LeaseCount = 0;
	bool bHasInitialState = false;
	bool bInitialMonitoringEnabled = false;
	bool bObservedMonitoringEnabled = false;
	bool bApplicationActive = false;
};

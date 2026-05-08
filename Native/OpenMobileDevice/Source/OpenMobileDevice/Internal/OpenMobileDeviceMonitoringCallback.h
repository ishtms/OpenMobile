#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceMonitoring.h"

struct FOpenMobileDeviceMonitoringCallbackToken
{
	EOpenMobileDeviceMonitoringGroup Group =
		EOpenMobileDeviceMonitoringGroup::Power;
	FOpenMobileDeviceCallbackToken BackendToken;
	uint64 ObserverGeneration = 0;

	bool IsValid() const
	{
		return BackendToken.Generation != 0 && ObserverGeneration != 0;
	}

	bool operator==(
		const FOpenMobileDeviceMonitoringCallbackToken& Other
	) const
	{
		return Group == Other.Group
			&& BackendToken.Generation == Other.BackendToken.Generation
			&& ObserverGeneration == Other.ObserverGeneration;
	}

	bool operator!=(
		const FOpenMobileDeviceMonitoringCallbackToken& Other
	) const
	{
		return !(*this == Other);
	}
};

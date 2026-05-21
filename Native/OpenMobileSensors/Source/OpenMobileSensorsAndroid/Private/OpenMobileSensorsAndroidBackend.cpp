#include "OpenMobileSensorsAndroidBackend.h"

FName FOpenMobileSensorsAndroidBackend::GetBackendName() const
{
	return TEXT("Android");
}

FOpenMobileCapability
FOpenMobileSensorsAndroidBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetModularFeatureName();
	Capability.State = EOpenMobileCapabilityState::Available;
	Capability.Detail = TEXT("The Android Sensors backend is registered.");
	return Capability;
}

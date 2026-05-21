#include "OpenMobileSensorsIOSBackend.h"

FName FOpenMobileSensorsIOSBackend::GetBackendName() const
{
	return TEXT("IOS");
}

FOpenMobileCapability FOpenMobileSensorsIOSBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetModularFeatureName();
	Capability.State = EOpenMobileCapabilityState::Available;
	Capability.Detail = TEXT("The iOS Sensors backend is registered.");
	return Capability;
}

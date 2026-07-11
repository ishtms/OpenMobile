#include "IOpenMobileMotionActivityProvider.h"

FName IOpenMobileMotionActivityProvider::GetModularFeatureName()
{
	static const FName Name(TEXT("OpenMobileMotionActivityProvider"));
	return Name;
}

FOpenMobileSensorCapability
IOpenMobileMotionActivityProvider::GetTransitionCapability() const
{
	FOpenMobileSensorCapability Capability;
	Capability.Sensor.Type = EOpenMobileSensorType::ActivityTransition;
	Capability.Sensor.InstanceId = TEXT("Default");
	Capability.Availability.Name = TEXT("ActivityTransition");
	Capability.Availability.State = EOpenMobileCapabilityState::NotSupported;
	Capability.Availability.Detail =
		TEXT("This provider does not expose native activity transitions.");
	return Capability;
}

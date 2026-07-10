#include "OpenMobileMotionActivityProviderResolver.h"

#include "Features/IModularFeatures.h"
#include "OpenMobileSensorPermissions.h"

IOpenMobileMotionActivityProvider*
FOpenMobileMotionActivityProviderResolver::FindProvider()
{
	TArray<IOpenMobileMotionActivityProvider*> Providers =
		IModularFeatures::Get().GetModularFeatureImplementations<
			IOpenMobileMotionActivityProvider
		>(IOpenMobileMotionActivityProvider::GetModularFeatureName());
	Providers.RemoveAll(
		[](const IOpenMobileMotionActivityProvider* Provider)
		{
			return !Provider
				|| Provider->GetInterfaceVersion() !=
					IOpenMobileMotionActivityProvider::InterfaceVersion;
		}
	);
	Providers.Sort(
		[](const IOpenMobileMotionActivityProvider& Left,
			const IOpenMobileMotionActivityProvider& Right)
		{
			return Left.GetProviderName().LexicalLess(
				Right.GetProviderName()
			);
		}
	);
	return Providers.IsEmpty() ? nullptr : Providers[0];
}

FOpenMobileSensorCapability
FOpenMobileMotionActivityProviderResolver::GetCapability()
{
	IOpenMobileMotionActivityProvider* Provider = FindProvider();
	FOpenMobileSensorCapability Capability;
	if (Provider)
	{
		Capability = Provider->GetCapability();
	}
	Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
	Capability.Sensor.InstanceId = TEXT("Default");
	Capability.Availability.Name =
		FOpenMobileSensorTypes::GetStableName(
			EOpenMobileSensorType::MotionActivity
		);
	Capability.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::ActivityRecognition
		);
	if (Capability.Source == EOpenMobileSensorAvailabilitySource::Unknown)
	{
		Capability.Source = EOpenMobileSensorAvailabilitySource::Derived;
	}
	if (!Provider)
	{
		Capability.Availability.State =
			EOpenMobileCapabilityState::NotSupported;
		Capability.Availability.Detail =
			TEXT("No compatible Android motion-activity provider is enabled.");
	}
	return Capability;
}

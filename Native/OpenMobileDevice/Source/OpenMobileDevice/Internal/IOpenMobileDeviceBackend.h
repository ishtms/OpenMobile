#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileCoreTypes.h"

enum class EOpenMobileDeviceBackendDomain : uint8
{
	Identity,
	Environment,
	Power,
	Connectivity,
	Display,
	Accessibility,
	Utility
};

class IOpenMobileDeviceBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileDeviceBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Device.Backend"));
		return FeatureName;
	}

	static FName GetDomainCapabilityName(EOpenMobileDeviceBackendDomain Domain)
	{
		switch (Domain)
		{
		case EOpenMobileDeviceBackendDomain::Identity:
			return TEXT("OpenMobile.Device.Backend.Identity");
		case EOpenMobileDeviceBackendDomain::Environment:
			return TEXT("OpenMobile.Device.Backend.Environment");
		case EOpenMobileDeviceBackendDomain::Power:
			return TEXT("OpenMobile.Device.Backend.Power");
		case EOpenMobileDeviceBackendDomain::Connectivity:
			return TEXT("OpenMobile.Device.Backend.Connectivity");
		case EOpenMobileDeviceBackendDomain::Display:
			return TEXT("OpenMobile.Device.Backend.Display");
		case EOpenMobileDeviceBackendDomain::Accessibility:
			return TEXT("OpenMobile.Device.Backend.Accessibility");
		case EOpenMobileDeviceBackendDomain::Utility:
			return TEXT("OpenMobile.Device.Backend.Utility");
		}
		return NAME_None;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }

	virtual FOpenMobileCapability GetDomainCapability(
		EOpenMobileDeviceBackendDomain Domain
	) const
	{
		FOpenMobileCapability Capability;
		Capability.Name = GetDomainCapabilityName(Domain);
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		return Capability;
	}

	virtual void BeginShutdown() {}
};

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileCoreTypes.h"

class IOpenMobileSensorsBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileSensorsBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Sensors.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }

	virtual FOpenMobileCapability GetBackendCapability() const
	{
		FOpenMobileCapability Capability;
		Capability.Name = GetModularFeatureName();
		Capability.State = EOpenMobileCapabilityState::Available;
		return Capability;
	}

	virtual void BeginShutdown() {}
};

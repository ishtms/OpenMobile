#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"

struct OPENMOBILESENSORS_API FOpenMobileMotionActivityProviderStreamHandle
{
	FGuid Identifier;

	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	bool operator==(
		const FOpenMobileMotionActivityProviderStreamHandle& Other
	) const
	{
		return Identifier == Other.Identifier;
	}
};

struct OPENMOBILESENSORS_API FOpenMobileMotionActivityProviderRequest
{
	double RequestedFrequencyHz = 1.0;
	double MaximumDeliveryLatencySeconds = 0.0;
	bool bLowLatency = false;
};

DECLARE_DELEGATE_OneParam(
	FOnOpenMobileMotionActivityProviderBatch,
	const FOpenMobileActivitySensorBatch&
);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileMotionActivityProviderFailure,
	const FOpenMobileSensorOperationResult&
);

struct OPENMOBILESENSORS_API FOpenMobileMotionActivityProviderCallbacks
{
	FOnOpenMobileMotionActivityProviderBatch OnBatch;
	FOnOpenMobileMotionActivityProviderFailure OnFailure;
};

class OPENMOBILESENSORS_API IOpenMobileMotionActivityProvider
	: public IModularFeature
{
public:
	static constexpr uint32 InterfaceVersion = 1;
	static FName GetModularFeatureName();

	virtual uint32 GetInterfaceVersion() const = 0;
	virtual FName GetProviderName() const = 0;
	virtual FOpenMobileSensorCapability GetCapability() const = 0;
	virtual FOpenMobileSensorOperationResult StartStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request,
		FOpenMobileMotionActivityProviderCallbacks&& Callbacks
	) = 0;
	virtual FOpenMobileSensorOperationResult ReconfigureStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request
	) = 0;
	virtual void StopStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle
	) = 0;
};

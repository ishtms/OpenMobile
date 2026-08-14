#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"

struct OPENMOBILESENSORS_API FOpenMobileMotionActivityProviderStreamHandle
{
	FGuid Identifier;

	/** Use this before passing a provider stream handle around. A zero GUID means no stream was ever assigned. */
	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	/** This compares the provider-owned GUID only. Two handles with the same GUID refer to the same activity stream. */
	bool operator==(
		const FOpenMobileMotionActivityProviderStreamHandle& Other
	) const
	{
		return Identifier == Other.Identifier;
	}
};

struct OPENMOBILESENSORS_API FOpenMobileMotionActivityProviderRequest
{
	FOpenMobileSensorIdentifier Sensor;
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
	static constexpr uint32 InterfaceVersion = 2;

	/** Providers register against this modular feature name, so don't invent another name in an adapter. */
	static FName GetModularFeatureName();

	/** Return the interface version your provider implements. Sensors uses it to reject an adapter it can't call safely. */
	virtual uint32 GetInterfaceVersion() const = 0;

	/** Return a stable provider name for diagnostics and capability reports. Users will see this when startup fails. */
	virtual FName GetProviderName() const = 0;

	/** Report current motion-activity support without starting a stream. Sensors uses this during discovery only. */
	virtual FOpenMobileSensorCapability GetCapability() const = 0;

	/** Report activity-transition support separately because a provider may only implement normal activity updates. The default reports it unavailable. */
	virtual FOpenMobileSensorCapability GetTransitionCapability() const;

	/** Start one provider stream and retain the callbacks till StopStream. Return Accepted when startup will finish later. */
	virtual FOpenMobileSensorOperationResult StartStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request,
		FOpenMobileMotionActivityProviderCallbacks&& Callbacks
	) = 0;
	/** Apply rate and latency changes to the same handle. Don't replace its identity because Sensors still owns that handle. */
	virtual FOpenMobileSensorOperationResult ReconfigureStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request
	) = 0;
	/** Stop delivery for this handle and release its callbacks. Calling with an unknown handle should stay harmless. */
	virtual void StopStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle
	) = 0;
};

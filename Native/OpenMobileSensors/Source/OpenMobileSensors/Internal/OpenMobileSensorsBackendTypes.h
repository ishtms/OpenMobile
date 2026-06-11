#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorResults.h"

enum class EOpenMobileSensorMetadataUnit : uint8
{
	Portable,
	StandardGravity,
	DegreesPerSecond,
	Tesla,
	Pascal,
	Centimeters,
	Millimeters
};

enum class EOpenMobileSensorMetadataTimeUnit : uint8
{
	Seconds,
	Milliseconds,
	Microseconds,
	Nanoseconds
};

struct FOpenMobileSensorBackendMetadata
{
	FOpenMobileSensorMetadata Metadata;
	EOpenMobileSensorMetadataUnit MeasurementUnit =
		EOpenMobileSensorMetadataUnit::Portable;
	EOpenMobileSensorMetadataTimeUnit IntervalUnit =
		EOpenMobileSensorMetadataTimeUnit::Seconds;
	FString NativeIdentifier;
	bool bMutable = false;
};

struct FOpenMobileSensorBackendStreamHandle
{
	FGuid Identifier;

	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	bool operator==(
		const FOpenMobileSensorBackendStreamHandle& Other
	) const
	{
		return Identifier == Other.Identifier;
	}

	friend uint32 GetTypeHash(
		const FOpenMobileSensorBackendStreamHandle& Handle
	)
	{
		return GetTypeHash(Handle.Identifier);
	}
};

struct FOpenMobileSensorPhysicalStreamRequest
{
	FOpenMobileSensorIdentifier Sensor;
	double RequestedFrequencyHz = 0.0;
	double MaximumDeliveryLatencySeconds = 0.0;
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	bool bAllowDerivedFallback = true;
	bool bAllowHighSamplingRate = false;
	bool bLowLatency = false;
	bool bNativeBatchingRequested = false;
	bool bNativeBatchingApplied = false;

	bool operator==(
		const FOpenMobileSensorPhysicalStreamRequest& Other
	) const
	{
		// Applied batching is backend output, not reconfiguration input.
		return Sensor == Other.Sensor
			&& RequestedFrequencyHz == Other.RequestedFrequencyHz
			&& MaximumDeliveryLatencySeconds ==
				Other.MaximumDeliveryLatencySeconds
			&& AttitudeReferenceFrame == Other.AttitudeReferenceFrame
			&& bAllowDerivedFallback == Other.bAllowDerivedFallback
			&& bAllowHighSamplingRate == Other.bAllowHighSamplingRate
			&& bLowLatency == Other.bLowLatency
			&& bNativeBatchingRequested ==
				Other.bNativeBatchingRequested;
	}

	bool operator!=(
		const FOpenMobileSensorPhysicalStreamRequest& Other
	) const
	{
		return !(*this == Other);
	}
};

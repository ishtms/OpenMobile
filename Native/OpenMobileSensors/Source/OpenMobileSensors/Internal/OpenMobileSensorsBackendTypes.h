#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorMetadata.h"

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

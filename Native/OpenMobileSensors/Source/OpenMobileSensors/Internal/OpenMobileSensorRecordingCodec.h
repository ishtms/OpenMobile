#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorSamples.h"

struct FOpenMobileSensorRecordingStreamDescriptor
{
	FOpenMobileSensorIdentifier Sensor;
	EOpenMobileSensorSampleFamily Family =
		EOpenMobileSensorSampleFamily::Unknown;
	FString Units;
	FOpenMobileSensorCapability Capability;
};

struct FOpenMobileSensorRecordingHeader
{
	int32 FormatVersion = 3;
	FString PluginVersion;
	FString PlatformName;
	FString UnitsConvention;
	FString CoordinateConvention;
	bool bHasSensitiveLocationContext = false;
	FOpenMobileSensorLocationInput SensitiveLocationContext;
	TArray<FOpenMobileSensorRecordingStreamDescriptor> Streams;
};

struct FOpenMobileSensorRecordingFooter
{
	int64 BatchCount = 0;
	int64 SampleCount = 0;
	int64 DroppedSamples = 0;
	double DurationSeconds = 0.0;
};

struct FOpenMobileSensorRecordingDocument
{
	FOpenMobileSensorRecordingHeader Header;
	TArray<FOpenMobileVectorSensorBatch> VectorBatches;
	FOpenMobileSensorRecordingFooter Footer;
};

class OPENMOBILESENSORS_API FOpenMobileSensorRecordingCodec final
{
public:
	static constexpr int32 CurrentFormatVersion = 3;

	static bool EncodeHeader(
		const FOpenMobileSensorRecordingHeader& Header,
		TArray<uint8>& OutBytes,
		FString& OutError
	);
	static bool EncodeVectorBatch(
		const FOpenMobileVectorSensorBatch& Batch,
		TArray<uint8>& OutBytes,
		FString& OutError
	);
	static bool EncodeFooter(
		const FOpenMobileSensorRecordingFooter& Footer,
		TArray<uint8>& OutBytes,
		FString& OutError
	);
	static bool EncodeComplete(
		const FOpenMobileSensorRecordingDocument& Document,
		TArray<uint8>& OutBytes,
		FString& OutError
	);
	static bool DecodeComplete(
		TConstArrayView<uint8> Bytes,
		FOpenMobileSensorRecordingDocument& OutDocument,
		EOpenMobileSensorRecordingDecodeStatus& OutStatus,
		FString& OutError
	);
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorProvenanceCodec final
{
public:
	static bool Encode(
		const FOpenMobileSensorSampleHeader& Header,
		TArray<uint8>& OutBytes
	);
	static bool Decode(
		const TArray<uint8>& Bytes,
		bool bMarkReplayed,
		FOpenMobileSensorSampleHeader& OutHeader
	);
};

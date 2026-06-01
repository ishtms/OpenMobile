#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorMetadata.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsMetadataService final
{
public:
	static void Start();
	static void BeginShutdown();
	static TArray<FOpenMobileSensorMetadata> GetMetadata();
	static TArray<FString> GetVerboseNativeMetadataForDiagnostics();
	static void HandleBackendGenerationChanged();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};

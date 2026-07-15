#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsRecordingService final
{
public:
	static void Start();
	static void BeginShutdown();
	static FGuid StartRecording(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorRecordingOptions& Options,
		TFunction<void(const FOpenMobileSensorRecordingResult&)>&& Completion
	);
	static FGuid StopRecording(
		const FGuid& OwnerIdentifier,
		const FGuid& RequestId,
		TFunction<void(const FOpenMobileSensorRecordingResult&)>&& Completion
	);
	static FGuid ReplayRecording(
		const FGuid& OwnerIdentifier,
		const FString& FilePath,
		const FOpenMobileSensorReplayOptions& Options,
		TFunction<void(const FOpenMobileSensorReplayResult&)>&& Completion
	);
	static void CancelOwner(const FGuid& OwnerIdentifier);

#if WITH_DEV_AUTOMATION_TESTS
	static void TickForTests(double NowSeconds);
	static bool GetRecordingStateForTests(
		const FGuid& RequestId,
		EOpenMobileSensorRecordingState& OutState
	);
	static void ResetForTests();
#endif
};

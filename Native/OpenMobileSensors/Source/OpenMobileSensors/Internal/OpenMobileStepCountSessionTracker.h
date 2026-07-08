#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileStepCountSessionTracker final
{
public:
	explicit FOpenMobileStepCountSessionTracker(
		const FGuid& InSessionIdentifier = {}
	);

	void Initialize(const FGuid& InSessionIdentifier);
	bool Process(
		const FOpenMobileStepsSensorSample& NativeSample,
		FOpenMobileStepsSensorSample& OutSessionSample
	);
	void ResetBaseline();

private:
	FGuid SessionIdentifier;
	FGuid NativeOriginIdentifier;
	EOpenMobileStepCountOrigin NativeOrigin =
		EOpenMobileStepCountOrigin::Unknown;
	int64 NativeBaseline = 0;
	int64 LastNativeCount = 0;
	int64 CommittedCount = 0;
	int64 LastSessionCount = 0;
	double NativeQueryStartUnixTimeSeconds = 0.0;
	bool bHasBaseline = false;
	bool bExplicitResetPending = false;
	bool bCountSaturated = false;
};

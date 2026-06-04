#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsNativeDurationOutcome : uint8
{
	Accepted,
	Split,
	Rejected
};

struct FOpenMobileHapticsNativeDurationResolution
{
	EOpenMobileHapticsNativeDurationOutcome Outcome =
		EOpenMobileHapticsNativeDurationOutcome::Rejected;
	double RequestedSeconds = 0.0;
	double NativeSegmentSeconds = 0.0;
	int32 SegmentCount = 0;
};

class FOpenMobileHapticsDurationPolicy final
{
public:
	static bool IsWithinBounds(
		double DurationSeconds,
		double MinimumSeconds,
		double MaximumSeconds
	);
	static FOpenMobileHapticsNativeDurationResolution ResolveNativeLimit(
		double DurationSeconds,
		double NativeMaximumSeconds,
		bool bCanSplit
	);
	static bool TryCalculatePatternDuration(
		const FOpenMobileHapticPattern& Pattern,
		double MaximumSeconds,
		double& OutDurationSeconds
	);
	static bool TryCalculatePatternDuration(
		const FOpenMobileHapticPattern& Pattern,
		double MaximumSeconds,
		double MaximumEventSeconds,
		double& OutDurationSeconds
	);
};

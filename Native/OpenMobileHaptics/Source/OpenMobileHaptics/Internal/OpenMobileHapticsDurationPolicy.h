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
	/** Rejects non-finite and out-of-range durations early, native APIs don't agree on how those values fail. */
	static bool IsWithinBounds(
		double DurationSeconds,
		double MinimumSeconds,
		double MaximumSeconds
	);
	/** Decides whether a long request can be split safely or has to fail before playback starts. */
	static FOpenMobileHapticsNativeDurationResolution ResolveNativeLimit(
		double DurationSeconds,
		double NativeMaximumSeconds,
		bool bCanSplit
	);
	/** Totals the portable pattern using the caller's overall cap and the default per-event rules. */
	static bool TryCalculatePatternDuration(
		const FOpenMobileHapticPattern& Pattern,
		double MaximumSeconds,
		double& OutDurationSeconds
	);
	/** Totals the pattern with an explicit event cap, used where the native backend has a tighter limit. */
	static bool TryCalculatePatternDuration(
		const FOpenMobileHapticPattern& Pattern,
		double MaximumSeconds,
		double MaximumEventSeconds,
		double& OutDurationSeconds
	);
};

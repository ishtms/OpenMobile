#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsTimingOutcome : uint8
{
	Ready,
	InvalidSchedule,
	MissingCalibration,
	StaleCalibration,
	ClockDiscontinuity,
	TooLate,
	TooFar
};

struct FOpenMobileHapticsTimingLimits
{
	double MaximumLatencyOffsetSeconds = 0.25;
	double MaximumScheduleHorizonSeconds = 60.0;
	double MaximumLatenessSeconds = 0.05;
	double MaximumCalibrationDriftSeconds = 0.05;
	double MaximumReportedPrecisionSeconds = 0.1;
};

struct FOpenMobileHapticsTimingResolution
{
	EOpenMobileHapticsTimingOutcome Outcome =
		EOpenMobileHapticsTimingOutcome::InvalidSchedule;
	double StartDelaySeconds = 0.0;
	FOpenMobileHapticSynchronizationDiagnostics Diagnostics;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsTimingPolicy final
{
public:
	explicit FOpenMobileHapticsTimingPolicy(
		FOpenMobileHapticsTimingLimits InLimits = {}
	);

	FOpenMobileHapticTimingCalibrationResult Calibrate(
		EOpenMobileHapticTimingClock Clock,
		double ClockTimeSeconds,
		double PlatformMonotonicTimeSeconds,
		double EstimatedPrecisionSeconds,
		int64 LifecycleGeneration
	);

	void Invalidate();

	FOpenMobileHapticsTimingResolution Resolve(
		const FOpenMobileHapticSchedule& Schedule,
		double PlatformMonotonicNowSeconds,
		int64 LifecycleGeneration,
		EOpenMobileHapticSynchronizationMode SynchronizationMode
	) const;

private:
	TOptional<FOpenMobileHapticTimingAnchor>& AnchorFor(
		EOpenMobileHapticTimingClock Clock
	);
	const TOptional<FOpenMobileHapticTimingAnchor>& AnchorFor(
		EOpenMobileHapticTimingClock Clock
	) const;

	FOpenMobileHapticsTimingLimits Limits;
	TOptional<FOpenMobileHapticTimingAnchor> GameAnchor;
	TOptional<FOpenMobileHapticTimingAnchor> AudioAnchor;
	int64 NextCalibrationRevision = 1;
};

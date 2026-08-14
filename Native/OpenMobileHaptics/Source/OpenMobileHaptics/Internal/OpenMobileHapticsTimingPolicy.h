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
	/** Stores validated timing limits once so calibration and scheduling use identical tolerances. */
	explicit FOpenMobileHapticsTimingPolicy(
		FOpenMobileHapticsTimingLimits InLimits = {}
	);

	/** Captures the offset between an external clock and platform monotonic time for one lifecycle generation. */
	FOpenMobileHapticTimingCalibrationResult Calibrate(
		EOpenMobileHapticTimingClock Clock,
		double ClockTimeSeconds,
		double PlatformMonotonicTimeSeconds,
		double EstimatedPrecisionSeconds,
		int64 LifecycleGeneration
	);

	/** Drops both clock anchors after lifecycle or audio timing changes, stale calibration is worse than no calibration. */
	void Invalidate();

	/** Converts an immediate, delayed, game-clock, or audio-clock schedule into a native start delay with lateness checks. */
	FOpenMobileHapticsTimingResolution Resolve(
		const FOpenMobileHapticSchedule& Schedule,
		double PlatformMonotonicNowSeconds,
		int64 LifecycleGeneration,
		EOpenMobileHapticSynchronizationMode SynchronizationMode
	) const;

	/** Returns precision only for a calibration from the current lifecycle generation. */
	bool GetCalibrationPrecision(
		EOpenMobileHapticTimingClock Clock,
		int64 LifecycleGeneration,
		double& OutEstimatedPrecisionSeconds
	) const;

private:
	/** Selects mutable storage for supported external clocks, platform monotonic needs no anchor. */
	TOptional<FOpenMobileHapticTimingAnchor>& AnchorFor(
		EOpenMobileHapticTimingClock Clock
	);
	/** Reads the same clock anchor from const scheduling paths without copying it. */
	const TOptional<FOpenMobileHapticTimingAnchor>& AnchorFor(
		EOpenMobileHapticTimingClock Clock
	) const;

	FOpenMobileHapticsTimingLimits Limits;
	TOptional<FOpenMobileHapticTimingAnchor> GameAnchor;
	TOptional<FOpenMobileHapticTimingAnchor> AudioAnchor;
	int64 NextCalibrationRevision = 1;
};

#include "OpenMobileHapticsTimingPolicy.h"

FOpenMobileHapticsTimingPolicy::FOpenMobileHapticsTimingPolicy(
	FOpenMobileHapticsTimingLimits InLimits
)
	: Limits(InLimits)
{
}

TOptional<FOpenMobileHapticTimingAnchor>&
FOpenMobileHapticsTimingPolicy::AnchorFor(
	EOpenMobileHapticTimingClock Clock
)
{
	return Clock == EOpenMobileHapticTimingClock::Game
		? GameAnchor
		: AudioAnchor;
}

const TOptional<FOpenMobileHapticTimingAnchor>&
FOpenMobileHapticsTimingPolicy::AnchorFor(
	EOpenMobileHapticTimingClock Clock
) const
{
	return Clock == EOpenMobileHapticTimingClock::Game
		? GameAnchor
		: AudioAnchor;
}

FOpenMobileHapticTimingCalibrationResult
FOpenMobileHapticsTimingPolicy::Calibrate(
	EOpenMobileHapticTimingClock Clock,
	double ClockTimeSeconds,
	double PlatformMonotonicTimeSeconds,
	double EstimatedPrecisionSeconds,
	int64 LifecycleGeneration
)
{
	FOpenMobileHapticTimingCalibrationResult Result;
	if ((Clock != EOpenMobileHapticTimingClock::Game
			&& Clock != EOpenMobileHapticTimingClock::Audio)
		|| !FMath::IsFinite(ClockTimeSeconds)
		|| !FMath::IsFinite(PlatformMonotonicTimeSeconds)
		|| !FMath::IsFinite(EstimatedPrecisionSeconds)
		|| ClockTimeSeconds < 0.0
		|| PlatformMonotonicTimeSeconds < 0.0
		|| EstimatedPrecisionSeconds < 0.0
		|| EstimatedPrecisionSeconds
			> Limits.MaximumReportedPrecisionSeconds
		|| LifecycleGeneration <= 0)
	{
		Result.Error = TEXT("The timing calibration is invalid.");
		return Result;
	}

	TOptional<FOpenMobileHapticTimingAnchor>& Existing = AnchorFor(Clock);
	if (Existing.IsSet()
		&& Existing->LifecycleGeneration == LifecycleGeneration)
	{
		const double ClockDelta =
			ClockTimeSeconds - Existing->ClockTimeSeconds;
		const double PlatformDelta = PlatformMonotonicTimeSeconds
			- Existing->PlatformMonotonicTimeSeconds;
		if (ClockDelta < 0.0
			|| PlatformDelta < 0.0
			|| FMath::Abs(ClockDelta - PlatformDelta)
				> Limits.MaximumCalibrationDriftSeconds)
		{
			Existing.Reset();
			Result.Status =
				EOpenMobileHapticTimingCalibrationStatus::ClockDiscontinuity;
			Result.Error = TEXT("The timing clock changed discontinuously.");
			return Result;
		}
	}

	FOpenMobileHapticTimingAnchor Anchor;
	Anchor.Clock = Clock;
	Anchor.ClockTimeSeconds = ClockTimeSeconds;
	Anchor.PlatformMonotonicTimeSeconds = PlatformMonotonicTimeSeconds;
	Anchor.EstimatedPrecisionSeconds = EstimatedPrecisionSeconds;
	Anchor.CalibrationRevision = NextCalibrationRevision++;
	Anchor.LifecycleGeneration = LifecycleGeneration;
	Existing = Anchor;
	Result.bAccepted = true;
	Result.Status = EOpenMobileHapticTimingCalibrationStatus::Accepted;
	Result.Anchor = Anchor;
	return Result;
}

void FOpenMobileHapticsTimingPolicy::Invalidate()
{
	GameAnchor.Reset();
	AudioAnchor.Reset();
}

FOpenMobileHapticsTimingResolution FOpenMobileHapticsTimingPolicy::Resolve(
	const FOpenMobileHapticSchedule& Schedule,
	double PlatformMonotonicNowSeconds,
	int64 LifecycleGeneration,
	EOpenMobileHapticSynchronizationMode SynchronizationMode
) const
{
	FOpenMobileHapticsTimingResolution Result;
	if (static_cast<uint8>(Schedule.Mode)
			> static_cast<uint8>(
				EOpenMobileHapticScheduleMode::AbsoluteAudioTime)
		|| static_cast<uint8>(SynchronizationMode)
			> static_cast<uint8>(
				EOpenMobileHapticSynchronizationMode::BestEffort)
		|| !FMath::IsFinite(Schedule.TimeSeconds)
		|| !FMath::IsFinite(Schedule.LatencyOffsetSeconds)
		|| !FMath::IsFinite(PlatformMonotonicNowSeconds)
		|| Schedule.TimeSeconds < 0.0
		|| PlatformMonotonicNowSeconds < 0.0
		|| FMath::Abs(Schedule.LatencyOffsetSeconds)
			> Limits.MaximumLatencyOffsetSeconds
		|| LifecycleGeneration <= 0)
	{
		return Result;
	}

	Result.Diagnostics.Mode = SynchronizationMode;
	Result.Diagnostics.RequestedTimeSeconds = Schedule.TimeSeconds;
	double TargetTimeSeconds = PlatformMonotonicNowSeconds
		+ Schedule.LatencyOffsetSeconds;
	if (Schedule.Mode == EOpenMobileHapticScheduleMode::Relative)
	{
		TargetTimeSeconds += Schedule.TimeSeconds;
	}
	else if (Schedule.Mode
		== EOpenMobileHapticScheduleMode::AbsoluteGameTime
		|| Schedule.Mode
			== EOpenMobileHapticScheduleMode::AbsoluteAudioTime)
	{
		const EOpenMobileHapticTimingClock Clock = Schedule.Mode
			== EOpenMobileHapticScheduleMode::AbsoluteGameTime
				? EOpenMobileHapticTimingClock::Game
				: EOpenMobileHapticTimingClock::Audio;
		const TOptional<FOpenMobileHapticTimingAnchor>& Anchor =
			AnchorFor(Clock);
		if (!Anchor.IsSet())
		{
			Result.Outcome =
				EOpenMobileHapticsTimingOutcome::MissingCalibration;
			return Result;
		}
		if (Anchor->LifecycleGeneration != LifecycleGeneration)
		{
			Result.Outcome =
				EOpenMobileHapticsTimingOutcome::StaleCalibration;
			return Result;
		}
		Result.Diagnostics.Clock = Clock;
		Result.Diagnostics.CalibrationRevision =
			Anchor->CalibrationRevision;
		Result.Diagnostics.EstimatedPrecisionSeconds =
			Anchor->EstimatedPrecisionSeconds;
		TargetTimeSeconds = Anchor->PlatformMonotonicTimeSeconds
			+ Schedule.TimeSeconds
			- Anchor->ClockTimeSeconds
			+ Schedule.LatencyOffsetSeconds;
	}

	const double DelaySeconds =
		TargetTimeSeconds - PlatformMonotonicNowSeconds;
	if (DelaySeconds > Limits.MaximumScheduleHorizonSeconds)
	{
		Result.Outcome = EOpenMobileHapticsTimingOutcome::TooFar;
		return Result;
	}
	if (DelaySeconds < -Limits.MaximumLatenessSeconds
		&& !FMath::IsNearlyEqual(
			DelaySeconds,
			-Limits.MaximumLatenessSeconds,
			UE_DOUBLE_SMALL_NUMBER
		))
	{
		Result.Outcome = EOpenMobileHapticsTimingOutcome::TooLate;
		return Result;
	}

	Result.Outcome = EOpenMobileHapticsTimingOutcome::Ready;
	Result.StartDelaySeconds = FMath::Max(0.0, DelaySeconds);
	Result.Diagnostics.ResolvedPlatformTimeSeconds = TargetTimeSeconds;
	Result.Diagnostics.LatenessSeconds = FMath::Max(0.0, -DelaySeconds);
	return Result;
}

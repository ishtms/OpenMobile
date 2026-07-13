#include "OpenMobileSensorShakeDetector.h"

#include "OpenMobileSensorSourcePolicy.h"

namespace OpenMobileSensorShakeDetectorPrivate
{
	bool IsValidOptions(const FOpenMobileShakeDetectionOptions& Options)
	{
		return FMath::IsFinite(
				Options.StrengthThresholdMetresPerSecondSquared
			)
			&& Options.StrengthThresholdMetresPerSecondSquared >= 0.1
			&& Options.StrengthThresholdMetresPerSecondSquared <= 1000.0
			&& Options.MinimumImpulses >= 1
			&& Options.MinimumImpulses <= 32
			&& FMath::IsFinite(Options.DurationWindowSeconds)
			&& Options.DurationWindowSeconds >= 0.01
			&& Options.DurationWindowSeconds <= 10.0
			&& FMath::IsFinite(Options.QuietResetSeconds)
			&& Options.QuietResetSeconds >= 0.0
			&& Options.QuietResetSeconds <= 5.0
			&& FMath::IsFinite(Options.CooldownSeconds)
			&& Options.CooldownSeconds >= 0.0
			&& Options.CooldownSeconds <= 60.0;
	}

	bool IsSupportedInput(const FOpenMobileVectorSensorSample& Sample)
	{
		return (Sample.Header.Sensor.Type ==
				EOpenMobileSensorType::LinearAcceleration
				|| Sample.Header.Sensor.Type ==
					EOpenMobileSensorType::Accelerometer)
			&& Sample.Header.bValid
			&& FMath::IsFinite(Sample.Header.TimestampSeconds)
			&& Sample.Header.TimestampSeconds >= 0.0
			&& !Sample.Value.ContainsNaN()
			&& FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
				Sample.Header.SourceFlags
			);
	}

	int32 MakeDerivedSourceFlags(int32 InputFlags)
	{
		constexpr int32 Overlays = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Mock
		) | static_cast<int32>(EOpenMobileSensorSourceFlags::Replay);
		return static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		) | (InputFlags & Overlays);
	}
}

void FOpenMobileSensorShakeDetector::Configure(
	const FOpenMobileShakeDetectionOptions& InOptions
)
{
	Options = InOptions;
	Impulses.Reserve(32);
	Reset();
}

bool FOpenMobileSensorShakeDetector::Process(
	const FOpenMobileVectorSensorSample& Sample,
	FOpenMobileVectorSensorSample& OutEvent
)
{
	using namespace OpenMobileSensorShakeDetectorPrivate;
	OutEvent = {};
	if (!IsValidOptions(Options) || !IsSupportedInput(Sample))
	{
		Reset();
		return false;
	}
	const bool bSourceChanged = bHasSource
		&& (LastSourceSensor != Sample.Header.Sensor
			|| LastSourceFlags != Sample.Header.SourceFlags);
	if (Sample.Header.bStatefulProcessingReset || bSourceChanged)
	{
		Reset();
	}
	if (bHasTimestamp
		&& Sample.Header.TimestampSeconds <= LastTimestampSeconds)
	{
		Reset();
		return false;
	}
	const double ContinuityGapSeconds = FMath::Max(
		5.0,
		Options.DurationWindowSeconds + Options.QuietResetSeconds
	);
	if (bHasTimestamp
		&& Sample.Header.TimestampSeconds - LastTimestampSeconds >
			ContinuityGapSeconds)
	{
		Reset();
	}
	LastSourceSensor = Sample.Header.Sensor;
	LastSourceFlags = Sample.Header.SourceFlags;
	bHasSource = true;
	LastTimestampSeconds = Sample.Header.TimestampSeconds;
	bHasTimestamp = true;

	const double Strength = Sample.Value.Size();
	if (Strength < Options.StrengthThresholdMetresPerSecondSquared)
	{
		if (!bQuietPeriodActive)
		{
			QuietSinceSeconds = Sample.Header.TimestampSeconds;
			bQuietPeriodActive = true;
		}
		if (Sample.Header.TimestampSeconds - QuietSinceSeconds >=
			Options.QuietResetSeconds)
		{
			bArmed = true;
		}
		return false;
	}
	bQuietPeriodActive = false;
	if (!bArmed)
	{
		return false;
	}
	bArmed = false;
	if (Sample.Header.TimestampSeconds < CooldownUntilSeconds)
	{
		return false;
	}
	while (!Impulses.IsEmpty()
		&& Sample.Header.TimestampSeconds - Impulses[0].TimestampSeconds >
			Options.DurationWindowSeconds)
	{
		Impulses.RemoveAt(0, 1, EAllowShrinking::No);
	}
	FImpulse& Impulse = Impulses.AddDefaulted_GetRef();
	Impulse.TimestampSeconds = Sample.Header.TimestampSeconds;
	Impulse.StrengthMetresPerSecondSquared = Strength;
	if (Impulses.Num() < Options.MinimumImpulses)
	{
		return false;
	}
	double PeakStrength = 0.0;
	for (const FImpulse& Candidate : Impulses)
	{
		PeakStrength = FMath::Max(
			PeakStrength,
			Candidate.StrengthMetresPerSecondSquared
		);
	}
	OutEvent = Sample;
	OutEvent.Header.Sensor.Type = EOpenMobileSensorType::Shake;
	OutEvent.Header.SourceFlags = MakeDerivedSourceFlags(
		Sample.Header.SourceFlags
	);
	OutEvent.Header.bSourceChanged |= bSourceChanged;
	OutEvent.bHasShakeEvent = true;
	OutEvent.ShakeEvent.StrengthMetresPerSecondSquared = PeakStrength;
	OutEvent.ShakeEvent.DurationSeconds =
		Sample.Header.TimestampSeconds - Impulses[0].TimestampSeconds;
	OutEvent.ShakeEvent.TimestampSeconds =
		Sample.Header.TimestampSeconds;
	OutEvent.ShakeEvent.ImpulseCount = Impulses.Num();
	OutEvent.ShakeEvent.SourceSensor = Sample.Header.Sensor;
	CooldownUntilSeconds =
		Sample.Header.TimestampSeconds + Options.CooldownSeconds;
	Impulses.Reset();
	return true;
}

void FOpenMobileSensorShakeDetector::Reset()
{
	Impulses.Reset();
	LastSourceSensor = {};
	LastTimestampSeconds = 0.0;
	QuietSinceSeconds = 0.0;
	CooldownUntilSeconds = 0.0;
	LastSourceFlags = 0;
	bHasTimestamp = false;
	bHasSource = false;
	bQuietPeriodActive = false;
	bArmed = true;
}

#include "OpenMobileSensorVectorFilter.h"

#include "OpenMobileSensorStreamOptions.h"

namespace OpenMobileSensorVectorFilterPrivate
{
	bool IsPositiveFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}

	FVector ApplyDeadZone(
		const FVector& Value,
		double DeadZone,
		bool& bOutSuppressed
	)
	{
		bOutSuppressed = false;
		const double MagnitudeSquared = Value.SizeSquared();
		if (DeadZone > 0.0
			&& MagnitudeSquared > 0.0
			&& MagnitudeSquared <= DeadZone * DeadZone)
		{
			bOutSuppressed = true;
			return FVector::ZeroVector;
		}
		return Value;
	}
}

bool FOpenMobileSensorVectorFilter::Apply(
	const FOpenMobileSensorFilterOptions& Options,
	FOpenMobileVectorSensorSample& Sample
)
{
	using namespace OpenMobileSensorVectorFilterPrivate;
	if (!Sample.Header.bValid
		|| Sample.Value.ContainsNaN()
		|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
		|| Sample.Header.TimestampSeconds < 0.0
		|| (Options.bEnableLowPass
			&& !IsPositiveFinite(Options.LowPassTimeConstantSeconds))
		|| (Options.bEnableHighPass
			&& !IsPositiveFinite(Options.HighPassTimeConstantSeconds))
		|| (Options.bEnableExponentialSmoothing
			&& !IsPositiveFinite(Options.SmoothingTimeConstantSeconds))
		|| !FMath::IsFinite(Options.DeadZone)
		|| Options.DeadZone < 0.0)
	{
		Reset();
		return false;
	}

	Sample.bHighPassFiltered = Options.bEnableHighPass;
	Sample.bHighPassFilterWarmingUp = false;
	Sample.bExponentiallySmoothed =
		Options.bEnableExponentialSmoothing;
	Sample.bDeadZoneSuppressed = false;
	const bool bReset = Sample.Header.bStatefulProcessingReset
		|| !bInitialized
		|| Sample.Header.TimestampSeconds <= LastTimestampSeconds;
	FVector Value = Sample.Value;
	if (bReset)
	{
		LowPassState = Value;
		if (Options.bEnableLowPass)
		{
			Value = LowPassState;
		}
		HighPassPreviousInput = Value;
		HighPassState = FVector::ZeroVector;
		HighPassWarmupElapsedSeconds = 0.0;
		if (Options.bEnableHighPass)
		{
			Value = HighPassState;
			Sample.bHighPassFilterWarmingUp = true;
		}
		SmoothingState = Value;
		bInitialized = true;
	}
	else
	{
		const double DeltaSeconds =
			Sample.Header.TimestampSeconds - LastTimestampSeconds;
		if (Options.bEnableLowPass)
		{
			const double Alpha = DeltaSeconds
				/ (Options.LowPassTimeConstantSeconds + DeltaSeconds);
			LowPassState += Alpha * (Value - LowPassState);
			Value = LowPassState;
		}
		if (Options.bEnableHighPass)
		{
			const double Alpha = Options.HighPassTimeConstantSeconds
				/ (Options.HighPassTimeConstantSeconds + DeltaSeconds);
			HighPassState = Alpha * (
				HighPassState + Value - HighPassPreviousInput
			);
			HighPassPreviousInput = Value;
			Value = HighPassState;
			HighPassWarmupElapsedSeconds += DeltaSeconds;
			Sample.bHighPassFilterWarmingUp =
				HighPassWarmupElapsedSeconds <
					Options.HighPassTimeConstantSeconds;
		}
		if (Options.bEnableExponentialSmoothing)
		{
			const double Alpha = DeltaSeconds
				/ (Options.SmoothingTimeConstantSeconds + DeltaSeconds);
			SmoothingState += Alpha * (Value - SmoothingState);
			Value = SmoothingState;
		}
	}
	if (Options.bEnableExponentialSmoothing && bReset)
	{
		SmoothingState = Value;
	}
	Sample.Value = ApplyDeadZone(
		Value,
		Options.DeadZone,
		Sample.bDeadZoneSuppressed
	);
	LastTimestampSeconds = Sample.Header.TimestampSeconds;
	return true;
}

void FOpenMobileSensorVectorFilter::Reset()
{
	LowPassState = FVector::ZeroVector;
	HighPassPreviousInput = FVector::ZeroVector;
	HighPassState = FVector::ZeroVector;
	SmoothingState = FVector::ZeroVector;
	HighPassWarmupElapsedSeconds = 0.0;
	LastTimestampSeconds = 0.0;
	bInitialized = false;
}

#include "OpenMobileHapticsPatternCompiler.h"

#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsPatternCompilerPrivate
{
	double Quantize(double Value, double Granularity)
	{
		const double Quantized = FMath::RoundToDouble(Value / Granularity)
			* Granularity;
		return FMath::IsNearlyZero(Quantized, UE_DOUBLE_SMALL_NUMBER)
			? 0.0
			: Quantized;
	}

	FOpenMobileHapticsPatternCompileResult Failure(
		EOpenMobileHapticsPatternCompileError Error,
		int32 EventIndex = INDEX_NONE
	)
	{
		FOpenMobileHapticsPatternCompileResult Result;
		Result.Error = Error;
		Result.EventIndex = EventIndex;
		return Result;
	}

	bool IsNormalized(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
	}
}

FOpenMobileHapticsPatternCompileLimits
FOpenMobileHapticsPatternCompiler::MakeLimits(
	const UOpenMobileHapticsSettings& Settings,
	const FOpenMobileHapticCapabilities& Capabilities
)
{
	FOpenMobileHapticsPatternCompileLimits Limits;
	Limits.MaximumEventCount = Settings.MaximumPatternEventCount;
	Limits.MaximumDurationSeconds = Settings.MaximumContinuousDurationSeconds;
	Limits.MaximumEventDurationSeconds =
		Settings.MaximumPatternEventDurationSeconds;
	Limits.MinimumGranularitySeconds =
		Settings.MinimumPatternGranularitySeconds;
	if (Capabilities.MaximumEventCount.bKnown)
	{
		Limits.MaximumEventCount = FMath::Min(
			Limits.MaximumEventCount,
			Capabilities.MaximumEventCount.Value
		);
	}
	if (Capabilities.MaximumDurationSeconds.bKnown)
	{
		Limits.MaximumDurationSeconds = FMath::Min(
			Limits.MaximumDurationSeconds,
			Capabilities.MaximumDurationSeconds.Seconds
		);
		Limits.MaximumEventDurationSeconds = FMath::Min(
			Limits.MaximumEventDurationSeconds,
			Limits.MaximumDurationSeconds
		);
	}
	if (Capabilities.MinimumTimingGranularitySeconds.bKnown)
	{
		Limits.MinimumGranularitySeconds = FMath::Max(
			Limits.MinimumGranularitySeconds,
			Capabilities.MinimumTimingGranularitySeconds.Seconds
		);
	}
	return Limits;
}

FOpenMobileHapticsPatternCompileResult
FOpenMobileHapticsPatternCompiler::Compile(
	const FOpenMobileHapticPattern& Pattern,
	const FOpenMobileHapticsPatternCompileLimits& Limits
)
{
	using namespace OpenMobileHapticsPatternCompilerPrivate;
	if (Pattern.Events.IsEmpty())
	{
		return Failure(EOpenMobileHapticsPatternCompileError::Empty);
	}
	if (Limits.MaximumEventCount < 1
		|| !FMath::IsFinite(Limits.MaximumDurationSeconds)
		|| !FMath::IsFinite(Limits.MaximumEventDurationSeconds)
		|| !FMath::IsFinite(Limits.MinimumGranularitySeconds)
		|| Limits.MaximumDurationSeconds <= 0.0
		|| Limits.MaximumEventDurationSeconds <= 0.0
		|| Limits.MaximumEventDurationSeconds
			> Limits.MaximumDurationSeconds
		|| Limits.MinimumGranularitySeconds <= 0.0
		|| Limits.MinimumGranularitySeconds
			> Limits.MaximumEventDurationSeconds)
	{
		return Failure(EOpenMobileHapticsPatternCompileError::InvalidLimits);
	}
	if (Pattern.Events.Num() > Limits.MaximumEventCount)
	{
		return Failure(EOpenMobileHapticsPatternCompileError::EventLimit);
	}

	TArray<FOpenMobileHapticsCompiledPatternEvent> CompiledEvents;
	CompiledEvents.Reserve(Pattern.Events.Num());
	double PreviousSourceStart = -1.0;
	double PreviousResolvedEnd = 0.0;
	double PatternDuration = 0.0;
	for (int32 Index = 0; Index < Pattern.Events.Num(); ++Index)
	{
		const FOpenMobileHapticPatternEvent& Event = Pattern.Events[Index];
		if (static_cast<uint8>(Event.Type)
			> static_cast<uint8>(EOpenMobileHapticPatternEventType::Silence))
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::InvalidEventType,
				Index
			);
		}
		if (!FMath::IsFinite(Event.StartTimeSeconds)
			|| !FMath::IsFinite(Event.DurationSeconds)
			|| !IsNormalized(Event.Intensity)
			|| !IsNormalized(Event.Sharpness)
			|| !IsNormalized(Event.FrequencyIntent))
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::Nonfinite,
				Index
			);
		}
		if (Event.StartTimeSeconds < 0.0
			|| Event.DurationSeconds < 0.0
			|| (Event.Type == EOpenMobileHapticPatternEventType::Transient
				&& Event.DurationSeconds != 0.0)
			|| (Event.Type != EOpenMobileHapticPatternEventType::Transient
				&& Event.DurationSeconds <= 0.0))
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::InvalidRange,
				Index
			);
		}
		if (Event.StartTimeSeconds < PreviousSourceStart)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::Unsorted,
				Index
			);
		}
		if (Event.DurationSeconds > Limits.MaximumEventDurationSeconds)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::EventDurationLimit,
				Index
			);
		}

		FOpenMobileHapticsCompiledPatternEvent Compiled;
		Compiled.Type = Event.Type;
		Compiled.StartTimeSeconds = Quantize(
			Event.StartTimeSeconds,
			Limits.MinimumGranularitySeconds
		);
		Compiled.DurationSeconds =
			Event.Type == EOpenMobileHapticPatternEventType::Transient
				? 0.0
				: Quantize(
					Event.DurationSeconds,
					Limits.MinimumGranularitySeconds
				);
		if (Event.Type != EOpenMobileHapticPatternEventType::Transient
			&& Compiled.DurationSeconds
				< Limits.MinimumGranularitySeconds)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::Granularity,
				Index
			);
		}
		if (Compiled.StartTimeSeconds < PreviousResolvedEnd)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::Overlap,
				Index
			);
		}
		if (Compiled.StartTimeSeconds > Limits.MaximumDurationSeconds
			|| Compiled.DurationSeconds
				> Limits.MaximumDurationSeconds
					- Compiled.StartTimeSeconds)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::DurationLimit,
				Index
			);
		}
		Compiled.Intensity =
			Event.Type == EOpenMobileHapticPatternEventType::Silence
				? 0.0f
				: Event.Intensity;
		Compiled.Sharpness = Event.Sharpness;
		Compiled.FrequencyIntent = Event.FrequencyIntent;
		PreviousSourceStart = Event.StartTimeSeconds;
		PreviousResolvedEnd =
			Compiled.StartTimeSeconds + Compiled.DurationSeconds;
		PatternDuration = FMath::Max(
			PatternDuration,
			PreviousResolvedEnd
		);
		CompiledEvents.Add(Compiled);
	}

	FOpenMobileHapticsPatternCompileResult Result;
	Result.Pattern = MakeShareable(new FOpenMobileHapticsCompiledPattern(
		MoveTemp(CompiledEvents),
		PatternDuration,
		Limits.MinimumGranularitySeconds
	));
	return Result;
}

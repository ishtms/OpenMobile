#include "OpenMobileHapticsPatternCompiler.h"

#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsPatternCompilerPrivate
{
	/** Quantizes source timing once before ordering and overlap checks so tiny floating differences don't change acceptance. */
	double Quantize(double Value, double Granularity)
	{
		const double Quantized = FMath::RoundToDouble(Value / Granularity)
			* Granularity;
		return FMath::IsNearlyZero(Quantized, UE_DOUBLE_SMALL_NUMBER)
			? 0.0
			: Quantized;
	}

	/** Preserves the exact failed source index while returning no compiled pattern. */
	FOpenMobileHapticsPatternCompileResult Failure(
		EOpenMobileHapticsPatternCompileError Error,
		int32 EventIndex = INDEX_NONE,
		int32 CurveIndex = INDEX_NONE,
		int32 ControlPointIndex = INDEX_NONE
	)
	{
		FOpenMobileHapticsPatternCompileResult Result;
		Result.Error = Error;
		Result.EventIndex = EventIndex;
		Result.CurveIndex = CurveIndex;
		Result.ControlPointIndex = ControlPointIndex;
		return Result;
	}

	/** Requires finite zero-to-one values before they're packed into cooked integers. */
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
	Limits.MaximumEventCount =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(
			Settings.MaximumPatternEventCount
		);
	Limits.MaximumCurveCount =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurves(
			Settings.MaximumPatternCurveCount
		);
	Limits.MaximumCurvePointCount =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurvePoints(
			Settings.MaximumPatternCurvePointCount
		);
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
	if (Capabilities.MaximumControlPointCount.bKnown)
	{
		Limits.MaximumCurvePointCount = FMath::Min(
			Limits.MaximumCurvePointCount,
			Capabilities.MaximumControlPointCount.Value
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
		|| Limits.MaximumCurveCount < 0
		|| Limits.MaximumCurvePointCount < 1
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
	if (Pattern.ParameterCurves.Num() > Limits.MaximumCurveCount)
	{
		return Failure(EOpenMobileHapticsPatternCompileError::CurveLimit);
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

	TArray<FOpenMobileHapticsCompiledParameterCurve> CompiledCurves;
	CompiledCurves.Reserve(Pattern.ParameterCurves.Num());
	int32 TotalControlPointCount = 0;
	double PreviousCurveStart = -1.0;
	double PreviousCurveEnds[2] = {-1.0, -1.0};
	for (int32 CurveIndex = 0;
		CurveIndex < Pattern.ParameterCurves.Num();
		++CurveIndex)
	{
		const FOpenMobileHapticParameterCurve& Curve =
			Pattern.ParameterCurves[CurveIndex];
		if (static_cast<uint8>(Curve.Parameter)
			> static_cast<uint8>(
				EOpenMobileHapticCurveParameter::SharpnessControl))
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::InvalidCurveType,
				INDEX_NONE,
				CurveIndex
			);
		}
		if (!FMath::IsFinite(Curve.StartTimeSeconds)
			|| Curve.StartTimeSeconds < 0.0
			|| Curve.ControlPoints.Num() < 2)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::InvalidCurve,
				INDEX_NONE,
				CurveIndex
			);
		}
		if (Curve.StartTimeSeconds < PreviousCurveStart)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::CurveUnsorted,
				INDEX_NONE,
				CurveIndex
			);
		}
		PreviousCurveStart = Curve.StartTimeSeconds;
		if (Curve.ControlPoints.Num()
			> Limits.MaximumCurvePointCount - TotalControlPointCount)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::CurvePointLimit,
				INDEX_NONE,
				CurveIndex
			);
		}
		TotalControlPointCount += Curve.ControlPoints.Num();

		FOpenMobileHapticsCompiledParameterCurve CompiledCurve;
		CompiledCurve.Parameter = Curve.Parameter;
		CompiledCurve.StartTimeSeconds = Quantize(
			Curve.StartTimeSeconds,
			Limits.MinimumGranularitySeconds
		);
		CompiledCurve.ControlPoints.Reserve(Curve.ControlPoints.Num());
		double PreviousSourcePointTime = -1.0;
		double PreviousCompiledPointTime = -1.0;
		for (int32 PointIndex = 0;
			PointIndex < Curve.ControlPoints.Num();
			++PointIndex)
		{
			const FOpenMobileHapticCurvePoint& Point =
				Curve.ControlPoints[PointIndex];
			if (!FMath::IsFinite(Point.RelativeTimeSeconds)
				|| Point.RelativeTimeSeconds < 0.0
				|| !IsNormalized(Point.Value)
				|| PointIndex == 0 && Point.RelativeTimeSeconds != 0.0)
			{
				return Failure(
					EOpenMobileHapticsPatternCompileError::InvalidCurve,
					INDEX_NONE,
					CurveIndex,
					PointIndex
				);
			}
			const double CompiledPointTime = Quantize(
				Point.RelativeTimeSeconds,
				Limits.MinimumGranularitySeconds
			);
			if (PointIndex > 0
				&& (Point.RelativeTimeSeconds <= PreviousSourcePointTime
					|| CompiledPointTime <= PreviousCompiledPointTime))
			{
				return Failure(
					EOpenMobileHapticsPatternCompileError::CurveUnsorted,
					INDEX_NONE,
					CurveIndex,
					PointIndex
				);
			}
			CompiledCurve.ControlPoints.Add({
				CompiledPointTime,
				Point.Value
			});
			PreviousSourcePointTime = Point.RelativeTimeSeconds;
			PreviousCompiledPointTime = CompiledPointTime;
		}

		const double CurveEnd = CompiledCurve.StartTimeSeconds
			+ CompiledCurve.ControlPoints.Last().RelativeTimeSeconds;
		if (CompiledCurve.StartTimeSeconds > PatternDuration
			|| CurveEnd > PatternDuration)
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::CurveDurationLimit,
				INDEX_NONE,
				CurveIndex
			);
		}
		const int32 ParameterIndex = static_cast<int32>(Curve.Parameter);
		if (CompiledCurve.StartTimeSeconds < PreviousCurveEnds[ParameterIndex])
		{
			return Failure(
				EOpenMobileHapticsPatternCompileError::CurveOverlap,
				INDEX_NONE,
				CurveIndex
			);
		}
		PreviousCurveEnds[ParameterIndex] = CurveEnd;
		CompiledCurves.Add(MoveTemp(CompiledCurve));
	}

	FOpenMobileHapticsPatternCompileResult Result;
	Result.Pattern = MakeShareable(new FOpenMobileHapticsCompiledPattern(
		MoveTemp(CompiledEvents),
		MoveTemp(CompiledCurves),
		PatternDuration,
		Limits.MinimumGranularitySeconds
	));
	return Result;
}

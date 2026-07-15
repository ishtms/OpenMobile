#include "OpenMobileHapticsAppleContinuousPolicy.h"

#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsRepeatPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsAppleContinuousPolicyPrivate
{
	constexpr double MaximumCoreHapticsEventDurationSeconds = 30.0;

	FOpenMobileHapticsAppleContinuousResolution MakeResolution(
		EOpenMobileHapticsAppleContinuousOutcome Outcome,
		FName Reason
	)
	{
		FOpenMobileHapticsAppleContinuousResolution Resolution;
		Resolution.Outcome = Outcome;
		Resolution.Reason = Reason;
		return Resolution;
	}

	float DecodeNormalized(uint16 Value)
	{
		return static_cast<float>(Value) / static_cast<float>(MAX_uint16);
	}

	double Seconds(uint32 Microseconds)
	{
		return static_cast<double>(Microseconds) / 1000000.0;
	}

	float EvaluateCurve(
		const FOpenMobileHapticsAppleParameterCurve& Curve,
		double PatternTimeSeconds
	)
	{
		const double RelativeTime = PatternTimeSeconds
			- Curve.StartTimeSeconds;
		for (int32 Index = 1;
			Index < Curve.RelativeTimesSeconds.Num();
			++Index)
		{
			if (RelativeTime <= Curve.RelativeTimesSeconds[Index])
			{
				const double StartTime = Curve.RelativeTimesSeconds[Index - 1];
				const double EndTime = Curve.RelativeTimesSeconds[Index];
				const float Alpha = static_cast<float>(
					(RelativeTime - StartTime) / (EndTime - StartTime)
				);
				return FMath::Lerp(
					Curve.Values[Index - 1],
					Curve.Values[Index],
					FMath::Clamp(Alpha, 0.0f, 1.0f)
				);
			}
		}
		return Curve.Values.Last();
	}

	float EvaluateParameter(
		const TArray<FOpenMobileHapticsAppleParameterCurve>& Curves,
		EOpenMobileHapticCurveParameter Parameter,
		double PatternTimeSeconds
	)
	{
		float Value = Parameter
			== EOpenMobileHapticCurveParameter::IntensityControl
				? 1.0f
				: 0.0f;
		for (const FOpenMobileHapticsAppleParameterCurve& Curve : Curves)
		{
			if (Curve.Parameter != Parameter)
			{
				continue;
			}
			if (PatternTimeSeconds < Curve.StartTimeSeconds)
			{
				break;
			}
			const double CurveEnd = Curve.StartTimeSeconds
				+ Curve.RelativeTimesSeconds.Last();
			if (PatternTimeSeconds <= CurveEnd)
			{
				return EvaluateCurve(Curve, PatternTimeSeconds);
			}
			Value = Curve.Values.Last();
		}
		return Value;
	}
}

FOpenMobileHapticsAppleContinuousLimits
FOpenMobileHapticsAppleContinuousPolicy::MakeLimits(
	const UOpenMobileHapticsSettings& Settings,
	const FOpenMobileHapticCapabilities& Capabilities
)
{
	using namespace OpenMobileHapticsAppleContinuousPolicyPrivate;
	FOpenMobileHapticsAppleContinuousLimits Limits;
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
	Limits.MaximumFiniteRepeatCount = Settings.MaximumFiniteRepeatCount;
	Limits.MaximumDurationSeconds =
		Settings.MaximumContinuousDurationSeconds;
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
	}
	Limits.MaximumEventDurationSeconds = FMath::Min3(
		static_cast<double>(Settings.MaximumPatternEventDurationSeconds),
		MaximumCoreHapticsEventDurationSeconds,
		Limits.MaximumDurationSeconds
	);
	return Limits;
}

FOpenMobileHapticsAppleContinuousResolution
FOpenMobileHapticsAppleContinuousPolicy::Resolve(
	const FOpenMobileHapticCookedPatternData& Pattern,
	const FOpenMobileHapticLoopOptions& Loop,
	const FOpenMobileHapticCapabilities& Capabilities,
	float RequestIntensity,
	const FOpenMobileHapticsAppleContinuousLimits& Limits
)
{
	using namespace OpenMobileHapticsAppleContinuousPolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f || RequestIntensity > 1.0f)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("InvalidIntensity")
		);
	}
	if (Pattern.DataFormatVersion
			!= FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		|| Pattern.GranularityMicroseconds == 0
		|| Pattern.Events.IsEmpty())
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("InvalidCookedPattern")
		);
	}
	if (Limits.MaximumEventCount < 1
		|| Limits.MaximumCurveCount < 0
		|| Limits.MaximumCurvePointCount < 1
		|| Limits.MaximumFiniteRepeatCount < 1
		|| !FMath::IsFinite(Limits.MaximumEventDurationSeconds)
		|| !FMath::IsFinite(Limits.MaximumDurationSeconds)
		|| Limits.MaximumEventDurationSeconds <= 0.0
		|| Limits.MaximumDurationSeconds <= 0.0)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("InvalidLimits")
		);
	}
	if (RequestIntensity == 0.0f)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Suppressed,
			TEXT("ZeroIntensity")
		);
	}
	if (Pattern.Events.Num() > Limits.MaximumEventCount
		|| Pattern.ParameterCurves.Num() > Limits.MaximumCurveCount)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("ConfiguredLimit")
		);
	}
	if (Capabilities.MaximumEventCount.bKnown
		&& Pattern.Events.Num() > Capabilities.MaximumEventCount.Value)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
			TEXT("HardwareEventLimit")
		);
	}
	const double PatternDurationSeconds = Seconds(
		Pattern.DurationMicroseconds
	);
	const double MaximumDurationSeconds =
		Capabilities.MaximumDurationSeconds.bKnown
			? FMath::Min(
				Limits.MaximumDurationSeconds,
				Capabilities.MaximumDurationSeconds.Seconds
			)
			: Limits.MaximumDurationSeconds;
	if (PatternDurationSeconds <= 0.0
		|| PatternDurationSeconds > MaximumDurationSeconds)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("DurationLimit")
		);
	}
	FOpenMobileHapticsAppleContinuousResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsAppleContinuousOutcome::Ready;
	Resolution.Reason = TEXT("Ready");
	Resolution.Pattern.DurationSeconds = PatternDurationSeconds;
	Resolution.Pattern.SafetyDurationSeconds = PatternDurationSeconds;
	Resolution.Pattern.Events.Reserve(Pattern.Events.Num());
	uint32 PreviousEndMicroseconds = 0;
	uint32 PreviousStartMicroseconds = 0;
	bool bHasPrevious = false;
	bool bHasContinuous = false;
	bool bHasTransient = false;
	bool bHasAudibleEvent = false;
	for (const FOpenMobileHapticCookedPatternEvent& Event : Pattern.Events)
	{
		if (static_cast<uint8>(Event.Type)
			> static_cast<uint8>(EOpenMobileHapticPatternEventType::Silence)
			|| Event.StartTimeMicroseconds > Pattern.DurationMicroseconds
			|| Event.DurationMicroseconds
				> Pattern.DurationMicroseconds - Event.StartTimeMicroseconds)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("InvalidEvent")
			);
		}
		if (bHasPrevious
			&& (Event.StartTimeMicroseconds < PreviousStartMicroseconds
				|| Event.StartTimeMicroseconds < PreviousEndMicroseconds))
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("EventOverlap")
			);
		}
		if (Event.Type == EOpenMobileHapticPatternEventType::Transient
			&& Event.DurationMicroseconds != 0
			|| Event.Type != EOpenMobileHapticPatternEventType::Transient
				&& Event.DurationMicroseconds == 0)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("EventDuration")
			);
		}
		bHasPrevious = true;
		PreviousStartMicroseconds = Event.StartTimeMicroseconds;
		PreviousEndMicroseconds = Event.StartTimeMicroseconds
			+ Event.DurationMicroseconds;
		if (Event.Type != EOpenMobileHapticPatternEventType::Transient
			&& Seconds(Event.DurationMicroseconds)
				> Limits.MaximumEventDurationSeconds)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
				TEXT("NativeEventDuration")
			);
		}

		FOpenMobileHapticsAppleRichEvent& NativeEvent =
			Resolution.Pattern.Events.AddDefaulted_GetRef();
		const bool bSilence = Event.Type
			== EOpenMobileHapticPatternEventType::Silence;
		NativeEvent.Type = bSilence
			? EOpenMobileHapticPatternEventType::Continuous
			: Event.Type;
		NativeEvent.StartTimeSeconds = Seconds(Event.StartTimeMicroseconds);
		NativeEvent.DurationSeconds = Seconds(Event.DurationMicroseconds);
		NativeEvent.Intensity = bSilence
			? 0.0f
			: DecodeNormalized(Event.Intensity) * RequestIntensity;
		NativeEvent.Sharpness = bSilence
			? 0.5f
			: DecodeNormalized(Event.Sharpness);
		bHasAudibleEvent |= NativeEvent.Intensity > 0.0f;
		bHasContinuous |=
			Event.Type == EOpenMobileHapticPatternEventType::Continuous;
		bHasTransient |=
			Event.Type == EOpenMobileHapticPatternEventType::Transient;
	}

	if (!bHasContinuous)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
			TEXT("TransientOnly")
		);
	}
	if (Capabilities.RichHaptics != EOpenMobileHapticSupportState::Supported
		|| Capabilities.ContinuousEvents
			!= EOpenMobileHapticSupportState::Supported
		|| bHasTransient
			&& Capabilities.TransientEvents
				!= EOpenMobileHapticSupportState::Supported)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
			TEXT("UnsupportedHardware")
		);
	}

	Resolution.Pattern.ParameterCurves.Reserve(
		Pattern.ParameterCurves.Num()
	);
	int32 TotalPointCount = 0;
	uint32 PreviousCurveStart = 0;
	uint32 PreviousCurveEnds[2] = {0, 0};
	bool bHasPreviousCurve = false;
	bool bHasCurveForParameter[2] = {false, false};
	for (const FOpenMobileHapticCookedParameterCurve& Curve
		: Pattern.ParameterCurves)
	{
		if (static_cast<uint8>(Curve.Parameter)
			> static_cast<uint8>(
				EOpenMobileHapticCurveParameter::SharpnessControl)
			|| Curve.StartTimeMicroseconds > Pattern.DurationMicroseconds
			|| Curve.ControlPoints.Num() < 2
			|| Curve.ControlPoints.Num()
				> Limits.MaximumCurvePointCount - TotalPointCount)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("InvalidCurve")
			);
		}
		if (bHasPreviousCurve
			&& Curve.StartTimeMicroseconds < PreviousCurveStart)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("UnsortedCurves")
			);
		}
		PreviousCurveStart = Curve.StartTimeMicroseconds;
		bHasPreviousCurve = true;
		TotalPointCount += Curve.ControlPoints.Num();
		FOpenMobileHapticsAppleParameterCurve& NativeCurve =
			Resolution.Pattern.ParameterCurves.AddDefaulted_GetRef();
		NativeCurve.Parameter = Curve.Parameter;
		NativeCurve.StartTimeSeconds = Seconds(Curve.StartTimeMicroseconds);
		NativeCurve.RelativeTimesSeconds.Reserve(Curve.ControlPoints.Num());
		NativeCurve.Values.Reserve(Curve.ControlPoints.Num());
		uint32 PreviousPointTime = 0;
		for (int32 PointIndex = 0;
			PointIndex < Curve.ControlPoints.Num();
			++PointIndex)
		{
			const FOpenMobileHapticCookedCurvePoint& Point =
				Curve.ControlPoints[PointIndex];
			if (PointIndex == 0 && Point.RelativeTimeMicroseconds != 0
				|| PointIndex > 0
					&& Point.RelativeTimeMicroseconds <= PreviousPointTime
				|| Point.RelativeTimeMicroseconds
					> Pattern.DurationMicroseconds
						- Curve.StartTimeMicroseconds)
			{
				return MakeResolution(
					EOpenMobileHapticsAppleContinuousOutcome::Invalid,
					TEXT("InvalidCurvePoint")
				);
			}
			PreviousPointTime = Point.RelativeTimeMicroseconds;
			NativeCurve.RelativeTimesSeconds.Add(
				Seconds(Point.RelativeTimeMicroseconds)
			);
			const float Normalized = DecodeNormalized(Point.Value);
			NativeCurve.Values.Add(
				Curve.Parameter
					== EOpenMobileHapticCurveParameter::IntensityControl
						? Normalized
						: Normalized * 2.0f - 1.0f
			);
		}
		const uint32 CurveEnd = Curve.StartTimeMicroseconds
			+ Curve.ControlPoints.Last().RelativeTimeMicroseconds;
		const int32 ParameterIndex = static_cast<int32>(Curve.Parameter);
		if (bHasCurveForParameter[ParameterIndex]
			&& Curve.StartTimeMicroseconds
				< PreviousCurveEnds[ParameterIndex])
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::Invalid,
				TEXT("CurveOverlap")
			);
		}
		bHasCurveForParameter[ParameterIndex] = true;
		PreviousCurveEnds[ParameterIndex] = CurveEnd;
	}

	if (!bHasAudibleEvent || !Resolution.Pattern.IsValid())
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Suppressed,
			TEXT("NoActiveEvent")
		);
	}

	const FOpenMobileHapticsRepeatPlanResult Repeat =
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Loop,
			PatternDurationSeconds,
			Limits.MaximumFiniteRepeatCount,
			MaximumDurationSeconds
		);
	if (!Repeat.IsSuccess())
	{
		return MakeResolution(
			EOpenMobileHapticsAppleContinuousOutcome::Invalid,
			TEXT("InvalidLoop")
		);
	}
	if (!Repeat.Plan.bLoop)
	{
		return Resolution;
	}
	if (Repeat.Plan.bRepeatUntilStopped)
	{
		if (!FMath::IsNearlyZero(
			Repeat.Plan.RepeatStartTimeSeconds,
			Seconds(Pattern.GranularityMicroseconds) * 0.5
		))
		{
			return MakeResolution(
				EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
				TEXT("IndefiniteRepeatStart")
			);
		}
		Resolution.Pattern.bLoop = true;
		Resolution.Pattern.LoopEndSeconds = PatternDurationSeconds;
		Resolution.Pattern.SafetyDurationSeconds =
			Repeat.Plan.MaximumDurationSeconds;
		return Resolution;
	}
	const TArray<FOpenMobileHapticsAppleRichEvent> BaseEvents =
		Resolution.Pattern.Events;
	const TArray<FOpenMobileHapticsAppleParameterCurve> BaseCurves =
		Resolution.Pattern.ParameterCurves;
	int32 ExpandedCurvePointCount = TotalPointCount;
	for (int32 RepeatIndex = 0;
		RepeatIndex < Repeat.Plan.RepeatCount;
		++RepeatIndex)
	{
		const double IterationStart = PatternDurationSeconds
			+ static_cast<double>(RepeatIndex)
				* Repeat.Plan.RepeatDurationSeconds;
		for (const FOpenMobileHapticsAppleRichEvent& BaseEvent : BaseEvents)
		{
			const double BaseEventEnd = BaseEvent.StartTimeSeconds
				+ BaseEvent.DurationSeconds;
			if (BaseEvent.Type
					== EOpenMobileHapticPatternEventType::Transient
				? BaseEvent.StartTimeSeconds
					< Repeat.Plan.RepeatStartTimeSeconds
				: BaseEventEnd <= Repeat.Plan.RepeatStartTimeSeconds)
			{
				continue;
			}
			if (Resolution.Pattern.Events.Num() >= Limits.MaximumEventCount)
			{
				return MakeResolution(
					EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
					TEXT("ExpandedEventLimit")
				);
			}
			FOpenMobileHapticsAppleRichEvent RepeatedEvent = BaseEvent;
			const double SlicedStart = FMath::Max(
				BaseEvent.StartTimeSeconds,
				Repeat.Plan.RepeatStartTimeSeconds
			);
			RepeatedEvent.StartTimeSeconds = IterationStart
				+ SlicedStart - Repeat.Plan.RepeatStartTimeSeconds;
			if (BaseEvent.Type
				== EOpenMobileHapticPatternEventType::Continuous)
			{
				RepeatedEvent.DurationSeconds = BaseEventEnd - SlicedStart;
			}
			Resolution.Pattern.Events.Add(RepeatedEvent);
		}

		TArray<FOpenMobileHapticsAppleParameterCurve> IterationCurves;
		for (const EOpenMobileHapticCurveParameter Parameter : {
			EOpenMobileHapticCurveParameter::IntensityControl,
			EOpenMobileHapticCurveParameter::SharpnessControl
		})
		{
			bool bHasParameterCurve = false;
			bool bCurveCoversBoundary = false;
			double FirstFutureCurveOffset = Repeat.Plan.RepeatDurationSeconds;
			for (const FOpenMobileHapticsAppleParameterCurve& BaseCurve
				: BaseCurves)
			{
				if (BaseCurve.Parameter != Parameter)
				{
					continue;
				}
				bHasParameterCurve = true;
				const double BaseCurveEnd = BaseCurve.StartTimeSeconds
					+ BaseCurve.RelativeTimesSeconds.Last();
				if (BaseCurve.StartTimeSeconds
						<= Repeat.Plan.RepeatStartTimeSeconds
					&& BaseCurveEnd
						> Repeat.Plan.RepeatStartTimeSeconds)
				{
					bCurveCoversBoundary = true;
				}
				if (BaseCurve.StartTimeSeconds
					> Repeat.Plan.RepeatStartTimeSeconds)
				{
					FirstFutureCurveOffset = FMath::Min(
						FirstFutureCurveOffset,
						BaseCurve.StartTimeSeconds
							- Repeat.Plan.RepeatStartTimeSeconds
					);
				}
			}
			if (bHasParameterCurve && !bCurveCoversBoundary)
			{
				const double ResetDuration = FMath::Min(
					Seconds(Pattern.GranularityMicroseconds),
					FirstFutureCurveOffset
				);
				if (ResetDuration > 0.0)
				{
					FOpenMobileHapticsAppleParameterCurve Reset;
					Reset.Parameter = Parameter;
					Reset.StartTimeSeconds = IterationStart;
					Reset.RelativeTimesSeconds = {0.0, ResetDuration};
					const float ResetValue = EvaluateParameter(
						BaseCurves,
						Parameter,
						Repeat.Plan.RepeatStartTimeSeconds
					);
					Reset.Values = {ResetValue, ResetValue};
					IterationCurves.Add(MoveTemp(Reset));
				}
			}
		}

		for (const FOpenMobileHapticsAppleParameterCurve& BaseCurve
			: BaseCurves)
		{
			const double BaseCurveEnd = BaseCurve.StartTimeSeconds
				+ BaseCurve.RelativeTimesSeconds.Last();
			if (BaseCurveEnd <= Repeat.Plan.RepeatStartTimeSeconds)
			{
				continue;
			}
			FOpenMobileHapticsAppleParameterCurve RepeatedCurve;
			RepeatedCurve.Parameter = BaseCurve.Parameter;
			if (BaseCurve.StartTimeSeconds
				< Repeat.Plan.RepeatStartTimeSeconds)
			{
				RepeatedCurve.StartTimeSeconds = IterationStart;
				RepeatedCurve.RelativeTimesSeconds.Add(0.0);
				RepeatedCurve.Values.Add(EvaluateCurve(
					BaseCurve,
					Repeat.Plan.RepeatStartTimeSeconds
				));
				for (int32 PointIndex = 0;
					PointIndex < BaseCurve.RelativeTimesSeconds.Num();
					++PointIndex)
				{
					const double AbsolutePointTime =
						BaseCurve.StartTimeSeconds
						+ BaseCurve.RelativeTimesSeconds[PointIndex];
					if (AbsolutePointTime
						<= Repeat.Plan.RepeatStartTimeSeconds)
					{
						continue;
					}
					RepeatedCurve.RelativeTimesSeconds.Add(
						AbsolutePointTime
							- Repeat.Plan.RepeatStartTimeSeconds
					);
					RepeatedCurve.Values.Add(BaseCurve.Values[PointIndex]);
				}
			}
			else
			{
				RepeatedCurve = BaseCurve;
				RepeatedCurve.StartTimeSeconds = IterationStart
					+ BaseCurve.StartTimeSeconds
					- Repeat.Plan.RepeatStartTimeSeconds;
			}
			if (RepeatedCurve.RelativeTimesSeconds.Num() == 1)
			{
				RepeatedCurve.RelativeTimesSeconds.Add(FMath::Min(
					Seconds(Pattern.GranularityMicroseconds),
					Repeat.Plan.RepeatDurationSeconds
				));
				RepeatedCurve.Values.Add(RepeatedCurve.Values[0]);
			}
			IterationCurves.Add(MoveTemp(RepeatedCurve));
		}
		IterationCurves.Sort([](
			const FOpenMobileHapticsAppleParameterCurve& Left,
			const FOpenMobileHapticsAppleParameterCurve& Right
		)
		{
			if (Left.StartTimeSeconds != Right.StartTimeSeconds)
			{
				return Left.StartTimeSeconds < Right.StartTimeSeconds;
			}
			return static_cast<uint8>(Left.Parameter)
				< static_cast<uint8>(Right.Parameter);
		});
		for (FOpenMobileHapticsAppleParameterCurve& Curve : IterationCurves)
		{
			if (Resolution.Pattern.ParameterCurves.Num()
					>= Limits.MaximumCurveCount
				|| Curve.Values.Num()
					> Limits.MaximumCurvePointCount
						- ExpandedCurvePointCount)
			{
				return MakeResolution(
					EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired,
					TEXT("ExpandedCurveLimit")
				);
			}
			ExpandedCurvePointCount += Curve.Values.Num();
			Resolution.Pattern.ParameterCurves.Add(MoveTemp(Curve));
		}
	}
	Resolution.Pattern.DurationSeconds = Repeat.Plan.TotalDurationSeconds;
	Resolution.Pattern.SafetyDurationSeconds =
		Repeat.Plan.TotalDurationSeconds;
	return Resolution;
}

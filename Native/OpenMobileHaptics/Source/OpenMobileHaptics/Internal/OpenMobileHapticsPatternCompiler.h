#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class UOpenMobileHapticsSettings;

enum class EOpenMobileHapticsPatternCompileError : uint8
{
	None,
	Empty,
	InvalidLimits,
	EventLimit,
	InvalidEventType,
	Nonfinite,
	InvalidRange,
	Unsorted,
	Overlap,
	EventDurationLimit,
	DurationLimit,
	Granularity,
	CurveLimit,
	CurvePointLimit,
	InvalidCurveType,
	InvalidCurve,
	CurveUnsorted,
	CurveOverlap,
	CurveDurationLimit
};

struct FOpenMobileHapticsPatternCompileLimits
{
	int32 MaximumEventCount = 128;
	int32 MaximumCurveCount = 16;
	int32 MaximumCurvePointCount = 256;
	double MaximumDurationSeconds = 30.0;
	double MaximumEventDurationSeconds = 10.0;
	double MinimumGranularitySeconds = 0.001;
};

struct FOpenMobileHapticsCompiledPatternEvent
{
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;
	double StartTimeSeconds = 0.0;
	double DurationSeconds = 0.0;
	float Intensity = 1.0f;
	float Sharpness = 0.5f;
	float FrequencyIntent = 0.5f;
};

struct FOpenMobileHapticsCompiledCurvePoint
{
	double RelativeTimeSeconds = 0.0;
	float Value = 0.5f;
};

struct FOpenMobileHapticsCompiledParameterCurve
{
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;
	double StartTimeSeconds = 0.0;
	TArray<FOpenMobileHapticsCompiledCurvePoint> ControlPoints;
};

class FOpenMobileHapticsCompiledPattern final
{
public:
	/** Exposes immutable events from a pattern that already passed every compile check. */
	const TArray<FOpenMobileHapticsCompiledPatternEvent>& GetEvents() const
	{
		return Events;
	}

	/** Exposes immutable curves with their point ordering already validated. */
	const TArray<FOpenMobileHapticsCompiledParameterCurve>&
	GetParameterCurves() const
	{
		return ParameterCurves;
	}

	/** Returns the compiled end time used by repeat and native duration policies. */
	double GetDurationSeconds() const
	{
		return DurationSeconds;
	}

	/** Returns the accepted timing step so later seek and quantization use the same precision. */
	double GetGranularitySeconds() const
	{
		return GranularitySeconds;
	}

private:
	friend class FOpenMobileHapticsPatternCompiler;

	/** Can be built only by the compiler, which prevents unvalidated arrays from masquerading as compiled data. */
	FOpenMobileHapticsCompiledPattern(
		TArray<FOpenMobileHapticsCompiledPatternEvent>&& InEvents,
		TArray<FOpenMobileHapticsCompiledParameterCurve>&& InParameterCurves,
		double InDurationSeconds,
		double InGranularitySeconds
	)
		: Events(MoveTemp(InEvents))
		, ParameterCurves(MoveTemp(InParameterCurves))
		, DurationSeconds(InDurationSeconds)
		, GranularitySeconds(InGranularitySeconds)
	{
	}

	TArray<FOpenMobileHapticsCompiledPatternEvent> Events;
	TArray<FOpenMobileHapticsCompiledParameterCurve> ParameterCurves;
	double DurationSeconds = 0.0;
	double GranularitySeconds = 0.001;
};

struct FOpenMobileHapticsPatternCompileResult
{
	TSharedPtr<const FOpenMobileHapticsCompiledPattern, ESPMode::ThreadSafe>
		Pattern;
	EOpenMobileHapticsPatternCompileError Error =
		EOpenMobileHapticsPatternCompileError::None;
	int32 EventIndex = INDEX_NONE;
	int32 CurveIndex = INDEX_NONE;
	int32 ControlPointIndex = INDEX_NONE;

	/** Requires both an immutable pattern and no recorded error before callers can consume output. */
	bool IsSuccess() const
	{
		return Pattern.IsValid()
			&& Error == EOpenMobileHapticsPatternCompileError::None;
	}
};

class FOpenMobileHapticsPatternCompiler final
{
public:
	/** Resolves compile caps from project settings and platform capability, always taking the tighter limit. */
	static FOpenMobileHapticsPatternCompileLimits MakeLimits(
		const UOpenMobileHapticsSettings& Settings,
		const FOpenMobileHapticCapabilities& Capabilities
	);
	/** Validates ordering, ranges, overlap, duration, and curves before producing immutable portable data. */
	static FOpenMobileHapticsPatternCompileResult Compile(
		const FOpenMobileHapticPattern& Pattern,
		const FOpenMobileHapticsPatternCompileLimits& Limits
	);
};

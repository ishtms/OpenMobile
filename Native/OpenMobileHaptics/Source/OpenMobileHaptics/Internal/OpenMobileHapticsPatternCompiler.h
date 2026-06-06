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
	Granularity
};

struct FOpenMobileHapticsPatternCompileLimits
{
	int32 MaximumEventCount = 128;
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

class FOpenMobileHapticsCompiledPattern final
{
public:
	const TArray<FOpenMobileHapticsCompiledPatternEvent>& GetEvents() const
	{
		return Events;
	}

	double GetDurationSeconds() const
	{
		return DurationSeconds;
	}

	double GetGranularitySeconds() const
	{
		return GranularitySeconds;
	}

private:
	friend class FOpenMobileHapticsPatternCompiler;

	FOpenMobileHapticsCompiledPattern(
		TArray<FOpenMobileHapticsCompiledPatternEvent>&& InEvents,
		double InDurationSeconds,
		double InGranularitySeconds
	)
		: Events(MoveTemp(InEvents))
		, DurationSeconds(InDurationSeconds)
		, GranularitySeconds(InGranularitySeconds)
	{
	}

	TArray<FOpenMobileHapticsCompiledPatternEvent> Events;
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

	bool IsSuccess() const
	{
		return Pattern.IsValid()
			&& Error == EOpenMobileHapticsPatternCompileError::None;
	}
};

class FOpenMobileHapticsPatternCompiler final
{
public:
	static FOpenMobileHapticsPatternCompileLimits MakeLimits(
		const UOpenMobileHapticsSettings& Settings,
		const FOpenMobileHapticCapabilities& Capabilities
	);
	static FOpenMobileHapticsPatternCompileResult Compile(
		const FOpenMobileHapticPattern& Pattern,
		const FOpenMobileHapticsPatternCompileLimits& Limits
	);
};

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"

class UOpenMobileHapticsSettings;

enum class EOpenMobileHapticsAppleContinuousOutcome : uint8
{
	Ready,
	Suppressed,
	FallbackRequired,
	Invalid
};

struct FOpenMobileHapticsAppleRichEvent
{
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;
	double StartTimeSeconds = 0.0;
	double DurationSeconds = 0.0;
	float Intensity = 1.0f;
	float Sharpness = 0.5f;
};

struct FOpenMobileHapticsAppleParameterCurve
{
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;
	double StartTimeSeconds = 0.0;
	TArray<double> RelativeTimesSeconds;
	TArray<float> Values;

	/** Requires at least two paired points before Core Haptics receives a parameter curve. */
	bool IsValid() const
	{
		return RelativeTimesSeconds.Num() >= 2
			&& RelativeTimesSeconds.Num() == Values.Num();
	}
};

struct FOpenMobileHapticsAppleContinuousPattern
{
	TArray<FOpenMobileHapticsAppleRichEvent> Events;
	TArray<FOpenMobileHapticsAppleParameterCurve> ParameterCurves;
	bool bHasInitialDynamicParameters = false;
	FOpenMobileHapticDynamicParameterUpdate InitialDynamicParameters;
	double DurationSeconds = 0.0;
	bool bLoop = false;
	double LoopEndSeconds = 0.0;
	double SafetyDurationSeconds = 0.0;

	/** Requires real events and a finite positive duration, the remaining fields are meaningful only after this passes. */
	bool IsValid() const
	{
		return !Events.IsEmpty()
			&& FMath::IsFinite(DurationSeconds)
			&& DurationSeconds > 0.0;
	}
};

struct FOpenMobileHapticsAppleContinuousLimits
{
	int32 MaximumEventCount = 128;
	int32 MaximumCurveCount = 16;
	int32 MaximumCurvePointCount = 256;
	int32 MaximumFiniteRepeatCount = 32;
	double MaximumEventDurationSeconds = 30.0;
	double MaximumDurationSeconds = 30.0;
};

struct FOpenMobileHapticsAppleContinuousResolution
{
	EOpenMobileHapticsAppleContinuousOutcome Outcome =
		EOpenMobileHapticsAppleContinuousOutcome::Invalid;
	FOpenMobileHapticsAppleContinuousPattern Pattern;
	FName Reason;
};

class FOpenMobileHapticsAppleContinuousPolicy final
{
public:
	/** Combines project caps with native capability limits so compilation uses the tighter value only. */
	static FOpenMobileHapticsAppleContinuousLimits MakeLimits(
		const UOpenMobileHapticsSettings& Settings,
		const FOpenMobileHapticCapabilities& Capabilities
	);
	/** Converts cooked portable events and curves into one Core Haptics timeline, including repeat safety duration. */
	static FOpenMobileHapticsAppleContinuousResolution Resolve(
		const FOpenMobileHapticCookedPatternData& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		const FOpenMobileHapticsAppleContinuousLimits& Limits
	);
};

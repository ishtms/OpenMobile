#include "OpenMobileHapticsAppleAHAPPlaybackPolicy.h"

#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsRepeatPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsAppleAHAPPlaybackPolicyPrivate
{
	FOpenMobileHapticsAppleAHAPResolution Result(
		EOpenMobileHapticsAppleAHAPOutcome Outcome,
		FName Reason
	)
	{
		FOpenMobileHapticsAppleAHAPResolution Resolution;
		Resolution.Outcome = Outcome;
		Resolution.Reason = Reason;
		return Resolution;
	}
}

FOpenMobileHapticsAppleAHAPLimits
FOpenMobileHapticsAppleAHAPPlaybackPolicy::MakeLimits(
	const UOpenMobileHapticsSettings& Settings
)
{
	FOpenMobileHapticsAppleAHAPLimits Limits;
	Limits.MaximumFiniteRepeatCount = Settings.MaximumFiniteRepeatCount;
	Limits.MaximumDurationSeconds =
		Settings.MaximumContinuousDurationSeconds;
	Limits.MinimumCompletionDurationSeconds =
		Settings.MinimumOneShotDurationSeconds;
	return Limits;
}

FOpenMobileHapticsAppleAHAPResolution
FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
	const UOpenMobileHapticIOSPatternAsset& Asset,
	const FOpenMobileHapticLoopOptions& Loop,
	const FOpenMobileHapticCapabilities& Capabilities,
	const FOpenMobileHapticsAppleAHAPLimits& Limits
)
{
	using namespace OpenMobileHapticsAppleAHAPPlaybackPolicyPrivate;
	if (Limits.MaximumFiniteRepeatCount < 1
		|| !FMath::IsFinite(Limits.MaximumDurationSeconds)
		|| Limits.MaximumDurationSeconds <= 0.0
		|| !FMath::IsFinite(Limits.MinimumCompletionDurationSeconds)
		|| Limits.MinimumCompletionDurationSeconds <= 0.0
		|| Limits.MinimumCompletionDurationSeconds
			> Limits.MaximumDurationSeconds)
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::Invalid,
			TEXT("InvalidLimits"));
	}
	TArray<FString> Errors;
	if (!Asset.Validate(Errors))
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::Invalid,
			TEXT("InvalidAHAP"));
	}
	if (Capabilities.AHAP != EOpenMobileHapticSupportState::Supported)
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired,
			TEXT("AHAPUnsupported"));
	}
	if (Asset.ContainsAudioEvents()
		&& Capabilities.AudioEvents
			!= EOpenMobileHapticSupportState::Supported)
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired,
			TEXT("AudioUnsupported"));
	}

	FOpenMobileHapticsAppleAHAPResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsAppleAHAPOutcome::Ready;
	Resolution.Reason = TEXT("Ready");
	Resolution.Pattern.NormalizedJson = Asset.GetNormalizedAHAPJson();
	Resolution.Pattern.DurationSeconds = Asset.GetAHAPDurationSeconds();
	Resolution.Pattern.SafetyDurationSeconds = FMath::Max(
		Asset.GetAHAPDurationSeconds(),
		Limits.MinimumCompletionDurationSeconds
	);
	Resolution.Pattern.bRequiresAdvancedPlayer =
		Asset.RequiresAdvancedPlayer();
	if (!Loop.bLoop)
	{
		if (Resolution.Pattern.SafetyDurationSeconds
			> Limits.MaximumDurationSeconds)
		{
			return Result(EOpenMobileHapticsAppleAHAPOutcome::Invalid,
				TEXT("DurationLimit"));
		}
		return Resolution;
	}
	if (!FMath::IsNearlyZero(Loop.RepeatStartTimeSeconds))
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired,
			TEXT("RepeatStart"));
	}
	if (Asset.GetAHAPDurationSeconds() <= 0.0)
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired,
			TEXT("LoopDuration"));
	}
	const FOpenMobileHapticsRepeatPlanResult Repeat =
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Loop,
			Asset.GetAHAPDurationSeconds(),
			Limits.MaximumFiniteRepeatCount,
			Limits.MaximumDurationSeconds
		);
	if (!Repeat.IsSuccess())
	{
		return Result(EOpenMobileHapticsAppleAHAPOutcome::Invalid,
			TEXT("InvalidLoop"));
	}
	Resolution.Pattern.bLoop = true;
	Resolution.Pattern.bRequiresAdvancedPlayer = true;
	Resolution.Pattern.SafetyDurationSeconds =
		Repeat.Plan.TotalDurationSeconds;
	return Resolution;
}

FOpenMobileHapticDynamicParameterUpdate
FOpenMobileHapticsAppleAHAPPlaybackPolicy::ComposeDynamicUpdate(
	const FOpenMobileHapticDynamicParameterUpdate& Update,
	float StaticIntensityScale
)
{
	FOpenMobileHapticDynamicParameterUpdate Composed = Update;
	if (Composed.bUpdateIntensity)
	{
		Composed.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
			Composed.Intensity,
			StaticIntensityScale,
			1.0f,
			1.0f,
			1.0f,
			1.0f
		);
	}
	return Composed;
}

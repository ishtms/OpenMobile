#include "OpenMobileHapticsSampleRecipes.h"

#include "Engine/GameInstance.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticsSampleRecipesPrivate
{
	FOpenMobileHapticPlaybackResult MissingSubsystem()
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Haptics subsystem is unavailable.")
		);
	}

	FOpenMobileHapticPlaybackOptions OptionsFor(
		FName Channel,
		FName Category,
		EOpenMobileHapticChannelPriority Priority,
		EOpenMobileHapticOverlapPolicy OverlapPolicy,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	)
	{
		FOpenMobileHapticPlaybackOptions Options;
		Options.Channel = Channel;
		Options.Category = Category;
		Options.Priority = Priority;
		Options.OverlapPolicy = OverlapPolicy;
		Options.FallbackPolicy = FallbackPolicy;
		return Options;
	}
}

UOpenMobileHapticsSubsystem*
UOpenMobileHapticsSampleRecipes::GetHapticsSubsystem(
	UObject* WorldContextObject
)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSampleRecipes::PlayPreparedPattern(
	UObject* WorldContextObject,
	FName PatternName,
	float Intensity,
	FName Channel,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsSampleRecipesPrivate;
	UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject);
	if (!Haptics)
	{
		return MissingSubsystem();
	}
	if (Haptics->GetNamedPatternStatus(PatternName)
		!= EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Prepare the named Haptics libraries before playback.")
		);
	}
	return Haptics->PlayNamedPatternAdvanced(
		PatternName,
		FMath::Clamp(Intensity, 0.0f, 1.0f),
		OptionsFor(
			Channel,
			Channel,
			EOpenMobileHapticChannelPriority::Normal,
			EOpenMobileHapticOverlapPolicy::Replace,
			FallbackPolicy
		)
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSampleRecipes::StartBoundedVehicleFeedback(
	UObject* WorldContextObject,
	float Intensity
)
{
	using namespace OpenMobileHapticsSampleRecipesPrivate;
	UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject);
	if (!Haptics)
	{
		return MissingSubsystem();
	}
	if (Haptics->GetNamedPatternStatus(TEXT("Vehicle_Bump"))
		!= EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Prepare the starter library before vehicle feedback.")
		);
	}
	FOpenMobileHapticPlaybackOptions Options = OptionsFor(
		TEXT("Gameplay"),
		TEXT("Vehicle"),
		EOpenMobileHapticChannelPriority::Low,
		EOpenMobileHapticOverlapPolicy::Replace,
		EOpenMobileHapticFallbackPolicy::NoBasicVibration
	);
	Options.Loop.bLoop = true;
	Options.Loop.RepeatCount = 4;
	Options.Loop.MaximumDurationSeconds = 2.0;
	return Haptics->PlayNamedPatternAdvanced(
		TEXT("Vehicle_Bump"),
		FMath::Clamp(Intensity, 0.0f, 0.55f),
		Options
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSampleRecipes::ScheduleWithAudioClock(
	UObject* WorldContextObject,
	FName PatternName,
	double AudioClockSeconds,
	double DelaySeconds
)
{
	using namespace OpenMobileHapticsSampleRecipesPrivate;
	UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject);
	if (!Haptics)
	{
		return MissingSubsystem();
	}
	if (!FMath::IsFinite(AudioClockSeconds)
		|| !FMath::IsFinite(DelaySeconds)
		|| DelaySeconds < 0.05
		|| DelaySeconds > 2.0)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Use a finite audio clock and a delay from 0.05 to 2 seconds.")
		);
	}
	if (Haptics->GetNamedPatternStatus(PatternName)
		!= EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("Prepare the named Haptics libraries before scheduling.")
		);
	}
	const FOpenMobileHapticTimingCalibrationResult Calibration =
		Haptics->CalibrateTimingClock(
			EOpenMobileHapticTimingClock::Audio,
			AudioClockSeconds,
			0.005
		);
	if (!Calibration.bAccepted)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The audio clock could not be calibrated.")
		);
	}
	FOpenMobileHapticPlaybackOptions Options = OptionsFor(
		TEXT("Cinematic"),
		TEXT("AudioSync"),
		EOpenMobileHapticChannelPriority::Low,
		EOpenMobileHapticOverlapPolicy::Replace,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	Options.Schedule.Mode = EOpenMobileHapticScheduleMode::AbsoluteAudioTime;
	Options.Schedule.TimeSeconds = AudioClockSeconds + DelaySeconds;
	return Haptics->PlayNamedPatternAdvanced(
		PatternName,
		0.45f,
		Options
	);
}

FOpenMobileHapticPlaybackResult
UOpenMobileHapticsSampleRecipes::PlayAccessibilityConfirmation(
	UObject* WorldContextObject,
	float Intensity
)
{
	using namespace OpenMobileHapticsSampleRecipesPrivate;
	UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject);
	if (!Haptics)
	{
		return MissingSubsystem();
	}
	FOpenMobileHapticPlaybackOptions Options = OptionsFor(
		TEXT("Alerts"),
		TEXT("Accessibility"),
		EOpenMobileHapticChannelPriority::High,
		EOpenMobileHapticOverlapPolicy::Replace,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	return Haptics->PlayGameFeedbackAdvanced(
		EOpenMobileHapticGamePreset::Confirm,
		FMath::Clamp(Intensity, 0.0f, 0.5f),
		Options
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSampleRecipes::CancelSamplePlayback(
	UObject* WorldContextObject,
	FOpenMobileHapticPlaybackHandle Handle
)
{
	if (UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject))
	{
		return Haptics->CancelPlayback(Handle);
	}
	return FOpenMobileHapticControlResult::MakeRejected(
		EOpenMobileErrorCode::Unavailable,
		TEXT("The Haptics subsystem is unavailable.")
	);
}

FOpenMobileHapticControlResult
UOpenMobileHapticsSampleRecipes::StopSampleChannel(
	UObject* WorldContextObject,
	FName Channel
)
{
	if (UOpenMobileHapticsSubsystem* Haptics =
		GetHapticsSubsystem(WorldContextObject))
	{
		return Haptics->StopChannel(Channel);
	}
	return FOpenMobileHapticControlResult::MakeRejected(
		EOpenMobileErrorCode::Unavailable,
		TEXT("The Haptics subsystem is unavailable.")
	);
}

bool UOpenMobileHapticsSampleRecipes::HasRichHaptics(
	const FOpenMobileHapticCapabilities& Capabilities
)
{
	return Capabilities.RichHaptics == EOpenMobileHapticSupportState::Supported;
}

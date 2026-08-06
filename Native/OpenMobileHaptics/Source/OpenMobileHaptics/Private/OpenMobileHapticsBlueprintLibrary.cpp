#include "OpenMobileHapticsBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPlayback.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticsBlueprintLibraryPrivate
{
	UOpenMobileHapticsSubsystem* ResolveSubsystem(
		const UObject* WorldContextObject,
		FOpenMobileHapticError& OutError
	)
	{
		UWorld* World = GEngine && WorldContextObject
			? GEngine->GetWorldFromContextObject(
				WorldContextObject,
				EGetWorldErrorMode::ReturnNull
			)
			: nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UOpenMobileHapticsSubsystem* Subsystem = GameInstance
			? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
			: nullptr;
		if (!Subsystem)
		{
			OutError = FOpenMobileHapticError::FromCommon(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("No Game Instance is available for this Haptics request."),
				EOpenMobileHapticFailureStage::Lifecycle
			);
			OutError.Correction =
				TEXT("Use a world context that belongs to an active game.");
		}
		return Subsystem;
	}

	FOpenMobileHapticPlaybackResult NoGameInstanceResult(
		const FOpenMobileHapticError& Error
	)
	{
		return FOpenMobileHapticPlaybackResult::MakeRejected(Error);
	}

	FName EnumValueName(const UEnum* Enum, int64 Value)
	{
		return Enum ? FName(*Enum->GetNameStringByValue(Value)) : NAME_None;
	}

	EOpenMobileHapticCapabilityTier ToCapabilityTier(
		EOpenMobileHapticAvailability Availability
	)
	{
		switch (Availability)
		{
		case EOpenMobileHapticAvailability::BasicVibration:
			return EOpenMobileHapticCapabilityTier::Basic;
		case EOpenMobileHapticAvailability::SemanticFeedback:
			return EOpenMobileHapticCapabilityTier::Semantic;
		case EOpenMobileHapticAvailability::RichHaptics:
			return EOpenMobileHapticCapabilityTier::Rich;
		default:
			return EOpenMobileHapticCapabilityTier::None;
		}
	}
}

void UOpenMobileHapticsBlueprintLibrary::PlaySelectionHaptic(
	const UObject* WorldContextObject,
	float Intensity,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	const FOpenMobileHapticPlaybackResult Result = Subsystem
		? Subsystem->PlaySelectionFeedback(
			FMath::Clamp(Intensity, 0.0f, 1.0f)
		)
		: OpenMobileHapticsBlueprintLibraryPrivate::NoGameInstanceResult(
			ContextError
		);
	ResolveCommonResult(
		Subsystem,
		Result,
		TEXT("Selection"),
		Outcome,
		Playback,
		bUsedFallback,
		ResolvedQuality,
		Error
	);
}

void UOpenMobileHapticsBlueprintLibrary::PlayImpactHaptic(
	const UObject* WorldContextObject,
	EOpenMobileHapticImpactStyle Style,
	float Intensity,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	const FOpenMobileHapticPlaybackResult Result = Subsystem
		? Subsystem->PlayImpactFeedback(
			Style,
			FMath::Clamp(Intensity, 0.0f, 1.0f)
		)
		: OpenMobileHapticsBlueprintLibraryPrivate::NoGameInstanceResult(
			ContextError
		);
	ResolveCommonResult(
		Subsystem,
		Result,
		OpenMobileHapticsBlueprintLibraryPrivate::EnumValueName(
			StaticEnum<EOpenMobileHapticImpactStyle>(),
			static_cast<int64>(Style)
		),
		Outcome,
		Playback,
		bUsedFallback,
		ResolvedQuality,
		Error
	);
}

void UOpenMobileHapticsBlueprintLibrary::PlayNotificationHaptic(
	const UObject* WorldContextObject,
	EOpenMobileHapticNotificationType Type,
	float Intensity,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	const FOpenMobileHapticPlaybackResult Result = Subsystem
		? Subsystem->PlayNotificationFeedback(
			Type,
			FMath::Clamp(Intensity, 0.0f, 1.0f)
		)
		: OpenMobileHapticsBlueprintLibraryPrivate::NoGameInstanceResult(
			ContextError
		);
	ResolveCommonResult(
		Subsystem,
		Result,
		OpenMobileHapticsBlueprintLibraryPrivate::EnumValueName(
			StaticEnum<EOpenMobileHapticNotificationType>(),
			static_cast<int64>(Type)
		),
		Outcome,
		Playback,
		bUsedFallback,
		ResolvedQuality,
		Error
	);
}

void UOpenMobileHapticsBlueprintLibrary::PlayGameHaptic(
	const UObject* WorldContextObject,
	EOpenMobileHapticGamePreset Preset,
	float Intensity,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	const FOpenMobileHapticPlaybackResult Result = Subsystem
		? Subsystem->PlayGameFeedback(
			Preset,
			FMath::Clamp(Intensity, 0.0f, 1.0f)
		)
		: OpenMobileHapticsBlueprintLibraryPrivate::NoGameInstanceResult(
			ContextError
		);
	ResolveCommonResult(
		Subsystem,
		Result,
		OpenMobileHapticsBlueprintLibraryPrivate::EnumValueName(
			StaticEnum<EOpenMobileHapticGamePreset>(),
			static_cast<int64>(Preset)
		),
		Outcome,
		Playback,
		bUsedFallback,
		ResolvedQuality,
		Error
	);
}

void UOpenMobileHapticsBlueprintLibrary::VibratePhone(
	const UObject* WorldContextObject,
	float DurationSeconds,
	float Intensity,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const float MinimumDuration = FMath::Max(
		0.001f,
		Settings->MinimumOneShotDurationSeconds
	);
	const float MaximumDuration = FMath::Max(
		MinimumDuration,
		Settings->MaximumOneShotDurationSeconds
	);
	const FOpenMobileHapticPlaybackResult Result = Subsystem
		? Subsystem->Vibrate(
			FMath::Clamp(DurationSeconds, MinimumDuration, MaximumDuration),
			FMath::Clamp(Intensity, 0.0f, 1.0f)
		)
		: OpenMobileHapticsBlueprintLibraryPrivate::NoGameInstanceResult(
			ContextError
		);
	ResolveCommonResult(
		Subsystem,
		Result,
		TEXT("OneShot"),
		Outcome,
		Playback,
		bUsedFallback,
		ResolvedQuality,
		Error
	);
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticPlaybackHandleValid(
	FOpenMobileHapticPlaybackHandle Handle
)
{
	return Handle.IsValid();
}

bool UOpenMobileHapticsBlueprintLibrary::EqualHapticPlaybackHandles(
	FOpenMobileHapticPlaybackHandle A,
	FOpenMobileHapticPlaybackHandle B
)
{
	return A == B;
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticPreloadHandleValid(
	FOpenMobileHapticLibraryPreloadHandle Handle
)
{
	return Handle.IsValid();
}

bool UOpenMobileHapticsBlueprintLibrary::EqualHapticPreloadHandles(
	FOpenMobileHapticLibraryPreloadHandle A,
	FOpenMobileHapticLibraryPreloadHandle B
)
{
	return A == B;
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticRequestAccepted(
	const FOpenMobileHapticPlaybackResult& Result
)
{
	return Result.IsAccepted();
}

bool UOpenMobileHapticsBlueprintLibrary::DidHapticRequestProduceOutput(
	const FOpenMobileHapticPlaybackResult& Result
)
{
	return Result.IsAccepted();
}

bool UOpenMobileHapticsBlueprintLibrary::HasHapticError(
	const FOpenMobileHapticError& Error
)
{
	return Error.IsSet();
}

FString UOpenMobileHapticsBlueprintLibrary::GetHapticResultSummary(
	const FOpenMobileHapticPlaybackResult& Result
)
{
	FString Prefix;
	switch (Result.Outcome)
	{
	case EOpenMobileHapticPlaybackOutcome::Accepted:
		Prefix = TEXT("Accepted");
		break;
	case EOpenMobileHapticPlaybackOutcome::Fallback:
		Prefix = TEXT("Accepted with fallback");
		break;
	case EOpenMobileHapticPlaybackOutcome::Suppressed:
		Prefix = FString::Printf(
			TEXT("Suppressed (%s)"),
			*StaticEnum<EOpenMobileHapticSuppressionReason>()->
				GetDisplayNameTextByValue(
					static_cast<int64>(Result.SuppressionReason)
				).ToString()
		);
		break;
	default:
		Prefix = TEXT("Rejected");
		break;
	}
	if (Result.Error.IsSet())
	{
		return FString::Printf(
			TEXT("%s: %s"),
			*Prefix,
			*FormatHapticError(Result.Error)
		);
	}
	if (!Result.ResolvedPath.IsNone())
	{
		return FString::Printf(
			TEXT("%s via %s"),
			*Prefix,
			*Result.ResolvedPath.ToString()
		);
	}
	return Prefix;
}

FString UOpenMobileHapticsBlueprintLibrary::FormatHapticError(
	const FOpenMobileHapticError& Error
)
{
	if (!Error.IsSet())
	{
		return TEXT("No Haptics error.");
	}
	const FString Code = StaticEnum<EOpenMobileHapticErrorCode>()->
		GetDisplayNameTextByValue(static_cast<int64>(Error.Code)).ToString();
	FString Message = Error.Message.IsEmpty()
		? Code
		: FString::Printf(TEXT("%s: %s"), *Code, *Error.Message);
	if (!Error.Correction.IsEmpty())
	{
		Message += FString::Printf(TEXT(" Fix: %s"), *Error.Correction);
	}
	return Message;
}

EOpenMobileHapticRecoveryAction
UOpenMobileHapticsBlueprintLibrary::GetHapticRecoveryAction(
	const FOpenMobileHapticError& Error
)
{
	switch (Error.Code)
	{
	case EOpenMobileHapticErrorCode::None:
		return EOpenMobileHapticRecoveryAction::None;
	case EOpenMobileHapticErrorCode::NotConfigured:
		return EOpenMobileHapticRecoveryAction::CheckConfiguration;
	case EOpenMobileHapticErrorCode::InvalidPattern:
		return Error.Stage == EOpenMobileHapticFailureStage::Preparation
			? EOpenMobileHapticRecoveryAction::PrepareContent
			: EOpenMobileHapticRecoveryAction::FixInput;
	case EOpenMobileHapticErrorCode::RateLimited:
	case EOpenMobileHapticErrorCode::ChannelBusy:
	case EOpenMobileHapticErrorCode::BackendUnavailable:
		return EOpenMobileHapticRecoveryAction::RetryWhenAvailable;
	case EOpenMobileHapticErrorCode::LifecycleRestricted:
		return EOpenMobileHapticRecoveryAction::WaitForForeground;
	case EOpenMobileHapticErrorCode::UnsupportedHardware:
	case EOpenMobileHapticErrorCode::UnsupportedFeature:
		return EOpenMobileHapticRecoveryAction::UseFallback;
	case EOpenMobileHapticErrorCode::InvalidRequest:
		return EOpenMobileHapticRecoveryAction::FixInput;
	case EOpenMobileHapticErrorCode::NativeEngineFailure:
	case EOpenMobileHapticErrorCode::Internal:
		return EOpenMobileHapticRecoveryAction::ReportNativeFailure;
	default:
		return EOpenMobileHapticRecoveryAction::None;
	}
}

EOpenMobileHapticAvailability
UOpenMobileHapticsBlueprintLibrary::GetHapticAvailability(
	const UObject* WorldContextObject
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	return Subsystem
		? Subsystem->GetHapticCapabilities().Availability
		: EOpenMobileHapticAvailability::TemporarilyUnavailable;
}

EOpenMobileHapticCapabilityTier
UOpenMobileHapticsBlueprintLibrary::GetHapticCapabilityTier(
	const UObject* WorldContextObject
)
{
	return OpenMobileHapticsBlueprintLibraryPrivate::ToCapabilityTier(
		GetHapticAvailability(WorldContextObject)
	);
}

bool UOpenMobileHapticsBlueprintLibrary::CanPlayHaptics(
	const UObject* WorldContextObject
)
{
	return GetHapticCapabilityTier(WorldContextObject)
		!= EOpenMobileHapticCapabilityTier::None;
}

bool UOpenMobileHapticsBlueprintLibrary::CanPlaySemanticHaptics(
	const UObject* WorldContextObject
)
{
	return static_cast<uint8>(GetHapticCapabilityTier(WorldContextObject))
		>= static_cast<uint8>(EOpenMobileHapticCapabilityTier::Semantic);
}

bool UOpenMobileHapticsBlueprintLibrary::CanPlayRichHaptics(
	const UObject* WorldContextObject
)
{
	return GetHapticCapabilityTier(WorldContextObject)
		== EOpenMobileHapticCapabilityTier::Rich;
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticFeatureSupported(
	EOpenMobileHapticSupportState Support
)
{
	return Support == EOpenMobileHapticSupportState::Supported;
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticFeatureKnown(
	EOpenMobileHapticSupportState Support
)
{
	return Support != EOpenMobileHapticSupportState::Unknown;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticPatternNames()
{
	TArray<FName> PatternNames;
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	for (const FOpenMobileHapticNamedLibrarySettings& ConfiguredLibrary :
		Settings->NamedLibraries)
	{
		UOpenMobileHapticLibrary* Library = Cast<UOpenMobileHapticLibrary>(
			ConfiguredLibrary.Asset.ResolveObject()
		);
		if (!Library && !ConfiguredLibrary.Asset.IsNull())
		{
			Library = Cast<UOpenMobileHapticLibrary>(
				ConfiguredLibrary.Asset.TryLoad()
			);
		}
		if (!Library)
		{
			continue;
		}
		for (const FOpenMobileHapticLibraryEntry& Entry : Library->Patterns)
		{
			if (!Entry.Name.IsNone())
			{
				PatternNames.AddUnique(Entry.Name);
			}
		}
	}
	PatternNames.Sort(FNameLexicalLess());
	return PatternNames;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetPreparedHapticPatternNames(
	const UObject* WorldContextObject
)
{
	TArray<FName> PatternNames;
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (Subsystem)
	{
		Subsystem->GetPreparedPatternNames(PatternNames);
	}
	return PatternNames;
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticPatternReady(
	const UObject* WorldContextObject,
	FName PatternName
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	return Subsystem && Subsystem->GetNamedPatternStatus(PatternName)
		== EOpenMobileHapticNamedPatternStatus::Loaded;
}

FOpenMobileHapticPlaybackOptions
UOpenMobileHapticsBlueprintLibrary::MakeUIHapticOptions()
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("UI");
	Options.Category = TEXT("UI");
	return Options;
}

FOpenMobileHapticPlaybackOptions
UOpenMobileHapticsBlueprintLibrary::MakeGameplayHapticOptions()
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("Gameplay");
	Options.Category = TEXT("Gameplay");
	return Options;
}

FOpenMobileHapticPlaybackOptions
UOpenMobileHapticsBlueprintLibrary::MakeAlertHapticOptions()
{
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("Alerts");
	Options.Category = TEXT("Alerts");
	Options.Priority = EOpenMobileHapticChannelPriority::High;
	return Options;
}

FOpenMobileHapticPlaybackOptions
UOpenMobileHapticsBlueprintLibrary::MakeProjectDefaultHapticOptions()
{
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Settings->DefaultChannel;
	Options.Category = Settings->DefaultCategory;
	return Options;
}

FOpenMobileHapticLoopOptions
UOpenMobileHapticsBlueprintLibrary::MakeFiniteHapticLoop(
	int32 TotalPlayCount,
	double RepeatStartTimeSeconds,
	double MaximumDurationSeconds
)
{
	FOpenMobileHapticLoopOptions Loop;
	Loop.bLoop = true;
	Loop.RepeatCount = FMath::Max(1, TotalPlayCount - 1);
	Loop.RepeatStartTimeSeconds = FMath::Max(0.0, RepeatStartTimeSeconds);
	Loop.MaximumDurationSeconds = FMath::Max(0.1, MaximumDurationSeconds);
	return Loop;
}

FOpenMobileHapticLoopOptions
UOpenMobileHapticsBlueprintLibrary::MakeHapticLoopUntilStopped(
	double RepeatStartTimeSeconds,
	double MaximumDurationSeconds
)
{
	FOpenMobileHapticLoopOptions Loop;
	Loop.bLoop = true;
	Loop.RepeatCount = 0;
	Loop.RepeatStartTimeSeconds = FMath::Max(0.0, RepeatStartTimeSeconds);
	Loop.MaximumDurationSeconds = FMath::Max(0.1, MaximumDurationSeconds);
	return Loop;
}

FOpenMobileHapticSchedule
UOpenMobileHapticsBlueprintLibrary::MakeHapticDelaySchedule(
	double DelaySeconds
)
{
	FOpenMobileHapticSchedule Schedule;
	Schedule.Mode = EOpenMobileHapticScheduleMode::Relative;
	Schedule.TimeSeconds = FMath::Max(0.0, DelaySeconds);
	return Schedule;
}

FOpenMobileHapticSchedule
UOpenMobileHapticsBlueprintLibrary::MakeHapticGameTimeSchedule(
	double GameTimeSeconds,
	double LatencyOffsetSeconds
)
{
	FOpenMobileHapticSchedule Schedule;
	Schedule.Mode = EOpenMobileHapticScheduleMode::AbsoluteGameTime;
	Schedule.TimeSeconds = FMath::Max(0.0, GameTimeSeconds);
	Schedule.LatencyOffsetSeconds = LatencyOffsetSeconds;
	return Schedule;
}

FOpenMobileHapticSchedule
UOpenMobileHapticsBlueprintLibrary::MakeHapticAudioTimeSchedule(
	double AudioTimeSeconds,
	double LatencyOffsetSeconds
)
{
	FOpenMobileHapticSchedule Schedule;
	Schedule.Mode = EOpenMobileHapticScheduleMode::AbsoluteAudioTime;
	Schedule.TimeSeconds = FMath::Max(0.0, AudioTimeSeconds);
	Schedule.LatencyOffsetSeconds = LatencyOffsetSeconds;
	return Schedule;
}

FOpenMobileHapticsDiagnostics
UOpenMobileHapticsBlueprintLibrary::GetHapticsDiagnosticsSnapshot(
	const UObject* WorldContextObject
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	return Subsystem ? Subsystem->GetDiagnosticsNative() : FOpenMobileHapticsDiagnostics{};
}

void UOpenMobileHapticsBlueprintLibrary::ResolveCommonResult(
	UOpenMobileHapticsSubsystem* Subsystem,
	const FOpenMobileHapticPlaybackResult& Result,
	FName PatternOrEffect,
	EOpenMobileHapticRequestOutcome& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	bool& bUsedFallback,
	FName& ResolvedQuality,
	FOpenMobileHapticError& Error
)
{
	Playback = nullptr;
	bUsedFallback =
		Result.Outcome == EOpenMobileHapticPlaybackOutcome::Fallback;
	ResolvedQuality = Result.ResolvedPath;
	Error = Result.Error;
	switch (Result.Outcome)
	{
	case EOpenMobileHapticPlaybackOutcome::Accepted:
	case EOpenMobileHapticPlaybackOutcome::Fallback:
		Outcome = EOpenMobileHapticRequestOutcome::Accepted;
		if (Subsystem && Result.Handle.IsValid())
		{
			UGameInstance* GameInstance = Subsystem->GetGameInstance();
			Playback = NewObject<UOpenMobileHapticPlayback>(
				GameInstance ? static_cast<UObject*>(GameInstance) : Subsystem
			);
			Playback->InitializePlayback(Subsystem, Result, PatternOrEffect);
		}
		break;
	case EOpenMobileHapticPlaybackOutcome::Suppressed:
		Outcome = EOpenMobileHapticRequestOutcome::Suppressed;
		break;
	default:
		Outcome = EOpenMobileHapticRequestOutcome::Rejected;
		break;
	}
}

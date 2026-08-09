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

	EOpenMobileHapticControlBranch ToControlBranch(
		EOpenMobileHapticControlOutcome Outcome
	)
	{
		switch (Outcome)
		{
		case EOpenMobileHapticControlOutcome::Accepted:
			return EOpenMobileHapticControlBranch::Succeeded;
		case EOpenMobileHapticControlOutcome::Unsupported:
			return EOpenMobileHapticControlBranch::Unsupported;
		case EOpenMobileHapticControlOutcome::StaleHandle:
			return EOpenMobileHapticControlBranch::Stale;
		default:
			return EOpenMobileHapticControlBranch::Failed;
		}
	}

	FName StandardChannelName(EOpenMobileHapticStandardChannel Channel)
	{
		switch (Channel)
		{
		case EOpenMobileHapticStandardChannel::UI:
			return TEXT("UI");
		case EOpenMobileHapticStandardChannel::Gameplay:
			return TEXT("Gameplay");
		case EOpenMobileHapticStandardChannel::Alerts:
			return TEXT("Alerts");
		case EOpenMobileHapticStandardChannel::Accessibility:
			return TEXT("Accessibility");
		case EOpenMobileHapticStandardChannel::Cinematic:
			return TEXT("Cinematic");
		case EOpenMobileHapticStandardChannel::Critical:
			return TEXT("Critical");
		default:
			return NAME_None;
		}
	}

	FOpenMobileHapticError InvalidIdentifierError(const TCHAR* Kind)
	{
		FOpenMobileHapticError Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::InvalidArgument,
			FString::Printf(TEXT("The Haptic %s identifier is empty."), Kind),
			EOpenMobileHapticFailureStage::Policy
		);
		Error.Correction = FString::Printf(
			TEXT("Use a standard %s identifier or a configured project value."),
			Kind
		);
		return Error;
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

bool UOpenMobileHapticsBlueprintLibrary::SupportsHapticFeature(
	const UObject* WorldContextObject,
	EOpenMobileHapticFeature Feature
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		return false;
	}
	const FOpenMobileHapticCapabilities Capabilities =
		Subsystem->GetHapticCapabilities();
	EOpenMobileHapticSupportState Support =
		EOpenMobileHapticSupportState::Unknown;
	switch (Feature)
	{
	case EOpenMobileHapticFeature::BasicVibration:
		Support = Capabilities.BasicVibration;
		break;
	case EOpenMobileHapticFeature::SemanticFeedback:
		Support = Capabilities.SemanticFeedback;
		break;
	case EOpenMobileHapticFeature::RichPatterns:
		Support = Capabilities.RichHaptics;
		break;
	case EOpenMobileHapticFeature::AmplitudeControl:
		Support = Capabilities.AmplitudeControl;
		break;
	case EOpenMobileHapticFeature::Looping:
		Support = Capabilities.Looping;
		break;
	case EOpenMobileHapticFeature::DynamicParameters:
		Support = Capabilities.DynamicParameters;
		break;
	case EOpenMobileHapticFeature::Scheduling:
		Support = Capabilities.Scheduling;
		break;
	case EOpenMobileHapticFeature::Pause:
		Support = Capabilities.Pause;
		break;
	case EOpenMobileHapticFeature::Resume:
		Support = Capabilities.Resume;
		break;
	case EOpenMobileHapticFeature::Seek:
		Support = Capabilities.Seek;
		break;
	default:
		break;
	}
	return Support == EOpenMobileHapticSupportState::Supported;
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

TArray<FOpenMobileHapticPatternIdentifier>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticPatternIdentifiers()
{
	TArray<FOpenMobileHapticPatternIdentifier> Identifiers;
	for (const FName PatternName : GetConfiguredHapticPatternNames())
	{
		FOpenMobileHapticPatternIdentifier& Identifier =
			Identifiers.AddDefaulted_GetRef();
		Identifier.Name = PatternName;
	}
	return Identifiers;
}

TArray<FOpenMobileHapticLibraryIdentifier>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticLibraryIdentifiers()
{
	TArray<FOpenMobileHapticLibraryIdentifier> Identifiers;
	for (const FName LibraryName : GetConfiguredHapticLibraryNames())
	{
		FOpenMobileHapticLibraryIdentifier& Identifier =
			Identifiers.AddDefaulted_GetRef();
		Identifier.Name = LibraryName;
	}
	return Identifiers;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticLibraryNames()
{
	TArray<FName> Names;
	for (const FOpenMobileHapticNamedLibrarySettings& Library :
		GetDefault<UOpenMobileHapticsSettings>()->NamedLibraries)
	{
		if (!Library.Name.IsNone())
		{
			Names.AddUnique(Library.Name);
		}
	}
	Names.Sort(FNameLexicalLess());
	return Names;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticChannelNames()
{
	TArray<FName> Names = {
		TEXT("UI"),
		TEXT("Gameplay"),
		TEXT("Alerts"),
		TEXT("Accessibility"),
		TEXT("Cinematic"),
		TEXT("Critical")
	};
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (!Settings->DefaultChannel.IsNone())
	{
		Names.AddUnique(Settings->DefaultChannel);
	}
	for (const FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		if (!Channel.Name.IsNone())
		{
			Names.AddUnique(Channel.Name);
		}
	}
	Names.Sort(FNameLexicalLess());
	return Names;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticCategoryNames()
{
	TArray<FName> Names = {
		TEXT("UI"),
		TEXT("Gameplay"),
		TEXT("Alerts"),
		TEXT("Accessibility"),
		TEXT("Cinematic"),
		TEXT("Critical")
	};
	const FName DefaultCategory =
		GetDefault<UOpenMobileHapticsSettings>()->DefaultCategory;
	if (!DefaultCategory.IsNone())
	{
		Names.AddUnique(DefaultCategory);
	}
	Names.Sort(FNameLexicalLess());
	return Names;
}

TArray<FName>
UOpenMobileHapticsBlueprintLibrary::GetConfiguredHapticEffectNames()
{
	TArray<FName> Names;
	for (const FOpenMobileHapticEffectSettings& Effect :
		GetDefault<UOpenMobileHapticsSettings>()->EffectOverrides)
	{
		if (!Effect.Name.IsNone())
		{
			Names.AddUnique(Effect.Name);
		}
	}
	Names.Sort(FNameLexicalLess());
	return Names;
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

bool UOpenMobileHapticsBlueprintLibrary::GetHapticsEnabled(
	const UObject* WorldContextObject
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	return Subsystem && Subsystem->IsHapticsEnabled();
}

void UOpenMobileHapticsBlueprintLibrary::SetHapticsEnabled(
	const UObject* WorldContextObject,
	bool bEnabled,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	ResolveControlResult(
		Subsystem->SetHapticsEnabled(bEnabled),
		Outcome,
		Error
	);
}

float UOpenMobileHapticsBlueprintLibrary::GetHapticsMasterIntensity(
	const UObject* WorldContextObject
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	return Subsystem ? Subsystem->GetMasterIntensity() : 0.0f;
}

void UOpenMobileHapticsBlueprintLibrary::SetHapticsMasterIntensity(
	const UObject* WorldContextObject,
	float MasterIntensity,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	ResolveControlResult(
		Subsystem->SetMasterIntensity(
			FMath::Clamp(MasterIntensity, 0.0f, 1.0f)
		),
		Outcome,
		Error
	);
}

float UOpenMobileHapticsBlueprintLibrary::GetHapticCategoryIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticCategoryIdentifier Category,
	bool& bHasOverride
)
{
	bHasOverride = false;
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem || !Category.IsValid())
	{
		return 1.0f;
	}
	const FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	const float* Scale = Policy.CategoryScales.Find(Category.Name);
	bHasOverride = Scale != nullptr;
	return Scale ? *Scale : 1.0f;
}

void UOpenMobileHapticsBlueprintLibrary::SetHapticCategoryIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticCategoryIdentifier Category,
	float Intensity,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	if (!Category.IsValid())
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		Error = OpenMobileHapticsBlueprintLibraryPrivate::
			InvalidIdentifierError(TEXT("category"));
		return;
	}
	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.CategoryScales.Add(
		Category.Name,
		FMath::Clamp(Intensity, 0.0f, 1.0f)
	);
	ResolveControlResult(Subsystem->SetUserPolicy(Policy), Outcome, Error);
}

void UOpenMobileHapticsBlueprintLibrary::ResetHapticCategoryIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticCategoryIdentifier Category,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	if (!Category.IsValid())
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		Error = OpenMobileHapticsBlueprintLibraryPrivate::
			InvalidIdentifierError(TEXT("category"));
		return;
	}
	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.CategoryScales.Remove(Category.Name);
	ResolveControlResult(Subsystem->SetUserPolicy(Policy), Outcome, Error);
}

float UOpenMobileHapticsBlueprintLibrary::GetHapticEffectIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticEffectIdentifier Effect,
	bool& bHasOverride
)
{
	bHasOverride = false;
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem || !Effect.IsValid())
	{
		return 1.0f;
	}
	const FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	const float* Scale = Policy.EffectScales.Find(Effect.Name);
	bHasOverride = Scale != nullptr;
	return Scale ? *Scale : 1.0f;
}

void UOpenMobileHapticsBlueprintLibrary::SetHapticEffectIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticEffectIdentifier Effect,
	float Intensity,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	if (!Effect.IsValid())
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		Error = OpenMobileHapticsBlueprintLibraryPrivate::
			InvalidIdentifierError(TEXT("effect"));
		return;
	}
	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.EffectScales.Add(
		Effect.Name,
		FMath::Clamp(Intensity, 0.0f, 1.0f)
	);
	ResolveControlResult(Subsystem->SetUserPolicy(Policy), Outcome, Error);
}

void UOpenMobileHapticsBlueprintLibrary::ResetHapticEffectIntensity(
	const UObject* WorldContextObject,
	FOpenMobileHapticEffectIdentifier Effect,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	if (!Effect.IsValid())
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		Error = OpenMobileHapticsBlueprintLibraryPrivate::
			InvalidIdentifierError(TEXT("effect"));
		return;
	}
	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.EffectScales.Remove(Effect.Name);
	ResolveControlResult(Subsystem->SetUserPolicy(Policy), Outcome, Error);
}

void UOpenMobileHapticsBlueprintLibrary::CalibrateHapticTiming(
	const UObject* WorldContextObject,
	EOpenMobileHapticTimingClock Clock,
	double ClockTimeSeconds,
	double EstimatedPrecisionSeconds,
	EOpenMobileHapticCalibrationOutcome& Outcome,
	FString& Error
)
{
	FOpenMobileHapticError ContextError;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			ContextError
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticCalibrationOutcome::Rejected;
		Error = ContextError.Message;
		return;
	}
	const FOpenMobileHapticTimingCalibrationResult Result =
		Subsystem->CalibrateTimingClock(
			Clock,
			ClockTimeSeconds,
			EstimatedPrecisionSeconds
		);
	Error = Result.Error;
	switch (Result.Status)
	{
	case EOpenMobileHapticTimingCalibrationStatus::Accepted:
		Outcome = EOpenMobileHapticCalibrationOutcome::Calibrated;
		break;
	case EOpenMobileHapticTimingCalibrationStatus::ClockDiscontinuity:
		Outcome = EOpenMobileHapticCalibrationOutcome::ClockReset;
		break;
	default:
		Outcome = EOpenMobileHapticCalibrationOutcome::Rejected;
		break;
	}
}

bool UOpenMobileHapticsBlueprintLibrary::IsHapticClockCalibrated(
	const UObject* WorldContextObject,
	EOpenMobileHapticTimingClock Clock
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	double Precision = 0.0;
	return Subsystem
		&& Subsystem->GetTimingCalibrationPrecision(Clock, Precision);
}

double UOpenMobileHapticsBlueprintLibrary::GetHapticTimingAccuracy(
	const UObject* WorldContextObject,
	EOpenMobileHapticTimingClock Clock
)
{
	FOpenMobileHapticError Error;
	const UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	double Precision = 0.0;
	return Subsystem
		&& Subsystem->GetTimingCalibrationPrecision(Clock, Precision)
			? Precision
			: 0.0;
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

FOpenMobileHapticChannelIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeStandardHapticChannel(
	EOpenMobileHapticStandardChannel Channel
)
{
	FOpenMobileHapticChannelIdentifier Identifier;
	Identifier.Name =
		OpenMobileHapticsBlueprintLibraryPrivate::StandardChannelName(Channel);
	return Identifier;
}

FOpenMobileHapticCategoryIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeStandardHapticCategory(
	EOpenMobileHapticStandardChannel Category
)
{
	FOpenMobileHapticCategoryIdentifier Identifier;
	Identifier.Name =
		OpenMobileHapticsBlueprintLibraryPrivate::StandardChannelName(Category);
	return Identifier;
}

FOpenMobileHapticPlaybackOptions
UOpenMobileHapticsBlueprintLibrary::MakeHapticPlaybackOptions(
	FOpenMobileHapticChannelIdentifier Channel,
	FOpenMobileHapticCategoryIdentifier Category,
	EOpenMobileHapticChannelPriority Priority,
	EOpenMobileHapticOverlapPolicy OverlapPolicy,
	EOpenMobileHapticFallbackPolicy FallbackPolicy,
	float IntensityScale,
	FOpenMobileHapticSchedule Schedule,
	FOpenMobileHapticLoopOptions Loop,
	EOpenMobileHapticInterruptionPolicy InterruptionPolicy
)
{
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = Channel.IsValid()
		? Channel.Name
		: Settings->DefaultChannel;
	Options.Category = Category.IsValid()
		? Category.Name
		: Settings->DefaultCategory;
	Options.Priority = Priority;
	Options.OverlapPolicy = OverlapPolicy;
	Options.FallbackPolicy = FallbackPolicy;
	Options.IntensityScale = FMath::Clamp(IntensityScale, 0.0f, 1.0f);
	Options.Schedule = Schedule;
	Options.Loop = Loop;
	Options.InterruptionPolicy = InterruptionPolicy;
	return Options;
}

FOpenMobileHapticPatternIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeHapticPatternIdentifier(FName Name)
{
	FOpenMobileHapticPatternIdentifier Identifier;
	Identifier.Name = Name;
	return Identifier;
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticPatternIdentifier(
	FOpenMobileHapticPatternIdentifier Identifier,
	FName& Name
)
{
	Name = Identifier.Name;
}

FOpenMobileHapticLibraryIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeHapticLibraryIdentifier(FName Name)
{
	FOpenMobileHapticLibraryIdentifier Identifier;
	Identifier.Name = Name;
	return Identifier;
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticLibraryIdentifier(
	FOpenMobileHapticLibraryIdentifier Identifier,
	FName& Name
)
{
	Name = Identifier.Name;
}

FOpenMobileHapticChannelIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeHapticChannelIdentifier(FName Name)
{
	FOpenMobileHapticChannelIdentifier Identifier;
	Identifier.Name = Name;
	return Identifier;
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticChannelIdentifier(
	FOpenMobileHapticChannelIdentifier Identifier,
	FName& Name
)
{
	Name = Identifier.Name;
}

FOpenMobileHapticCategoryIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeHapticCategoryIdentifier(FName Name)
{
	FOpenMobileHapticCategoryIdentifier Identifier;
	Identifier.Name = Name;
	return Identifier;
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticCategoryIdentifier(
	FOpenMobileHapticCategoryIdentifier Identifier,
	FName& Name
)
{
	Name = Identifier.Name;
}

FOpenMobileHapticEffectIdentifier
UOpenMobileHapticsBlueprintLibrary::MakeHapticEffectIdentifier(FName Name)
{
	FOpenMobileHapticEffectIdentifier Identifier;
	Identifier.Name = Name;
	return Identifier;
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticEffectIdentifier(
	FOpenMobileHapticEffectIdentifier Identifier,
	FName& Name
)
{
	Name = Identifier.Name;
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

void UOpenMobileHapticsBlueprintLibrary::WaitForHapticPlayback(
	const UObject* WorldContextObject,
	FOpenMobileHapticPlaybackHandle Handle,
	EOpenMobileHapticControlBranch& Outcome,
	UOpenMobileHapticPlayback*& Playback,
	FOpenMobileHapticError& Error
)
{
	Playback = nullptr;
	UOpenMobileHapticsSubsystem* Subsystem =
		OpenMobileHapticsBlueprintLibraryPrivate::ResolveSubsystem(
			WorldContextObject,
			Error
		);
	if (!Subsystem)
	{
		Outcome = EOpenMobileHapticControlBranch::Failed;
		return;
	}
	const EOpenMobileHapticPlaybackState State =
		Subsystem->GetPlaybackState(Handle);
	if (!Handle.IsValid() || State == EOpenMobileHapticPlaybackState::Invalid)
	{
		Outcome = EOpenMobileHapticControlBranch::Stale;
		Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The raw Haptic playback handle is invalid or stale."),
			EOpenMobileHapticFailureStage::Playback
		);
		return;
	}
	FOpenMobileHapticPlaybackResult Result;
	Result.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
	Result.State = State;
	Result.Handle = Handle;
	Playback = NewObject<UOpenMobileHapticPlayback>(
		Subsystem->GetGameInstance()
	);
	Playback->InitializePlayback(Subsystem, Result, TEXT("AdaptedHandle"));
	Outcome = EOpenMobileHapticControlBranch::Succeeded;
	Error = {};
}

void UOpenMobileHapticsBlueprintLibrary::BreakHapticPlaybackResult(
	const FOpenMobileHapticPlaybackResult& Result,
	bool& bAccepted,
	bool& bSuppressed,
	bool& bUsedFallback,
	FOpenMobileHapticPlaybackHandle& Handle,
	EOpenMobileHapticSuppressionReason& SuppressionReason,
	EOpenMobileHapticErrorCode& ErrorCode,
	FString& Message
)
{
	bAccepted = Result.IsAccepted();
	bSuppressed =
		Result.Outcome == EOpenMobileHapticPlaybackOutcome::Suppressed;
	bUsedFallback =
		Result.Outcome == EOpenMobileHapticPlaybackOutcome::Fallback;
	Handle = Result.Handle;
	SuppressionReason = Result.SuppressionReason;
	ErrorCode = Result.Error.Code;
	Message = Result.Error.IsSet()
		? FormatHapticError(Result.Error)
		: GetHapticResultSummary(Result);
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

void UOpenMobileHapticsBlueprintLibrary::ResolveControlResult(
	const FOpenMobileHapticControlResult& Result,
	EOpenMobileHapticControlBranch& Outcome,
	FOpenMobileHapticError& Error
)
{
	Outcome = OpenMobileHapticsBlueprintLibraryPrivate::ToControlBranch(
		Result.Outcome
	);
	Error = Result.Error;
}

#include "AnimNotify_OpenMobileHaptics.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_OpenMobileHaptics)

UAnimNotify_OpenMobileHaptics::UAnimNotify_OpenMobileHaptics()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
	NotifyColor = FColor(230, 118, 40);
#endif
}

void UAnimNotify_OpenMobileHaptics::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);
	Dispatch(MeshComp);
}

FString UAnimNotify_OpenMobileHaptics::GetNotifyName_Implementation() const
{
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::NamedPattern
		&& !NamedPattern.IsNone())
	{
		return FString::Printf(TEXT("Haptics: %s"), *NamedPattern.ToString());
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::GamePreset)
	{
		return FString::Printf(
			TEXT("Haptics: %s"),
			*StaticEnum<EOpenMobileHapticGamePreset>()->
				GetDisplayNameTextByValue(
					static_cast<int64>(GamePreset)
				).ToString()
		);
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::PatternAsset
		&& PatternAsset)
	{
		return FString::Printf(
			TEXT("Haptics: %s"),
			*PatternAsset->GetName()
		);
	}
	return TEXT("OpenMobile Haptics");
}

bool UAnimNotify_OpenMobileHaptics::Dispatch(
	USkeletalMeshComponent* MeshComp
) const
{
	if (!IsInGameThread() || !MeshComp)
	{
		return false;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World
		|| World->WorldType == EWorldType::EditorPreview
		|| (bSuppressOnDedicatedServer
			&& World->GetNetMode() == NM_DedicatedServer))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	UOpenMobileHapticsSubsystem* Subsystem = GameInstance
		? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
	if (!Subsystem
		|| Channel.IsNone()
		|| !FMath::IsFinite(Intensity)
		|| Intensity < 0.0f
		|| Intensity > 1.0f
		|| !FMath::IsFinite(IntensityScale)
		|| IntensityScale < 0.0f
		|| IntensityScale > 1.0f
		|| static_cast<uint8>(Priority)
			> static_cast<uint8>(EOpenMobileHapticChannelPriority::Critical))
	{
		return false;
	}

	FOpenMobileHapticPlaybackOptions Options;
	Options.Category = Category;
	Options.Channel = Channel;
	Options.IntensityScale = IntensityScale;
	Options.Priority = Priority;

	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::Semantic
		&& static_cast<uint8>(SemanticEffect)
			<= static_cast<uint8>(EOpenMobileHapticSemanticEffect::Achievement))
	{
		FOpenMobileHapticSemanticRequest Request;
		Request.Effect = SemanticEffect;
		Request.Intensity = Intensity;
		Request.Options = Options;
		return Subsystem->SubmitSemantic(Request).IsAccepted();
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::NamedPattern
		&& !NamedPattern.IsNone())
	{
		FOpenMobileHapticNamedPatternRequest Request;
		Request.PatternName = NamedPattern;
		Request.Intensity = Intensity;
		Request.Options = Options;
		return Subsystem->SubmitNamedPattern(Request).IsAccepted();
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::GamePreset
		&& static_cast<uint8>(GamePreset)
			<= static_cast<uint8>(EOpenMobileHapticGamePreset::Achievement))
	{
		return Subsystem->PlayGameFeedbackAdvanced(
			GamePreset,
			Intensity,
			Options
		).IsAccepted();
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::PatternAsset
		&& PatternAsset)
	{
		return Subsystem->SubmitPatternAsset(
			PatternAsset,
			Intensity,
			Options
		).IsAccepted();
	}
	return false;
}

#if WITH_EDITOR
EDataValidationResult UAnimNotify_OpenMobileHaptics::IsDataValid(
	FDataValidationContext& Context
) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bValid = true;
	if (Channel.IsNone())
	{
		Context.AddError(FText::FromString(
			TEXT("Haptics channel cannot be empty.")
		));
		bValid = false;
	}
	if (!FMath::IsFinite(Intensity) || Intensity < 0.0f || Intensity > 1.0f
		|| !FMath::IsFinite(IntensityScale)
		|| IntensityScale < 0.0f || IntensityScale > 1.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("Haptics intensity values must be from zero through one.")
		));
		bValid = false;
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::NamedPattern
		&& NamedPattern.IsNone())
	{
		Context.AddError(FText::FromString(
			TEXT("Named Pattern mode requires a configured alias.")
		));
		bValid = false;
	}
	if (EffectMode == EOpenMobileHapticAnimNotifyEffectMode::PatternAsset
		&& !PatternAsset)
	{
		Context.AddError(FText::FromString(
			TEXT("Pattern Asset mode requires a Haptic Pattern asset.")
		));
		bValid = false;
	}
	return bValid
		? CombineDataValidationResults(Result, EDataValidationResult::Valid)
		: EDataValidationResult::Invalid;
}
#endif

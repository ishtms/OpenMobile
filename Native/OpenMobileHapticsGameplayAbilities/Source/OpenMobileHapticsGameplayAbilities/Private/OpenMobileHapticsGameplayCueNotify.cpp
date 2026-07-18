#include "OpenMobileHapticsGameplayCueNotify.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenMobileHapticsGameplayCueNotify)

bool UOpenMobileHapticsGameplayCueNotify::HandlesEvent(
	EGameplayCueEvent::Type EventType
) const
{
	return EventType == EGameplayCueEvent::Executed;
}

bool UOpenMobileHapticsGameplayCueNotify::OnExecute_Implementation(
	AActor* Target,
	const FGameplayCueParameters& Parameters
) const
{
	return Submit(Target, Parameters);
}

bool UOpenMobileHapticsGameplayCueNotify::Submit(
	AActor* Target,
	const FGameplayCueParameters& Parameters
) const
{
	if (!IsInGameThread() || !Target)
	{
		return false;
	}

	UWorld* World = Target->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const FGameplayCueNotify_SpawnContext SpawnContext(
		World,
		Target,
		Parameters
	);
	if (!SpawnContext.FindLocalPlayerController(LocalOwnerSource))
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

	if (EffectMode == EOpenMobileHapticsGameplayCueEffectMode::Semantic
		&& static_cast<uint8>(SemanticEffect)
			<= static_cast<uint8>(EOpenMobileHapticSemanticEffect::Achievement))
	{
		FOpenMobileHapticSemanticRequest Request;
		Request.Effect = SemanticEffect;
		Request.Intensity = Intensity;
		Request.Options = Options;
		return Subsystem->SubmitSemantic(Request).IsAccepted();
	}
	if (EffectMode == EOpenMobileHapticsGameplayCueEffectMode::NamedPattern
		&& !NamedPattern.IsNone())
	{
		FOpenMobileHapticNamedPatternRequest Request;
		Request.PatternName = NamedPattern;
		Request.Intensity = Intensity;
		Request.Options = Options;
		return Subsystem->SubmitNamedPattern(Request).IsAccepted();
	}
	return false;
}

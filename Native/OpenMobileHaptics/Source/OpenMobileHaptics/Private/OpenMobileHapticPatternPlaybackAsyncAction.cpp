#include "OpenMobileHapticPatternPlaybackAsyncAction.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlayback.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticPatternPlaybackAsyncActionPrivate
{
	FOpenMobileHapticError MakeError(
		EOpenMobileErrorCode Code,
		FString Message,
		FString Correction = {}
	)
	{
		FOpenMobileHapticError Error = FOpenMobileHapticError::FromCommon(
			Code,
			MoveTemp(Message),
			EOpenMobileHapticFailureStage::Preparation
		);
		Error.Correction = MoveTemp(Correction);
		return Error;
	}
}

UOpenMobileHapticPatternPlaybackAsyncAction*
UOpenMobileHapticPatternPlaybackAsyncAction::PlayHapticPatternAsset(
	const UObject* WorldContextObject,
	UOpenMobileHapticPatternAsset* Pattern,
	float Intensity,
	bool bPrepareIfNeeded,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	UOpenMobileHapticPatternPlaybackAsyncAction* Action =
		NewObject<UOpenMobileHapticPatternPlaybackAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->RequestedPattern = Pattern;
	Action->RequestedIntensity = Intensity;
	Action->bRequestedPrepareIfNeeded = bPrepareIfNeeded;
	Action->RequestedOptions = Options;
	return Action;
}

void UOpenMobileHapticPatternPlaybackAsyncAction::Activate()
{
	check(IsInGameThread());
	if (bFinished)
	{
		return;
	}
	UWorld* World = GEngine && StoredWorldContextObject
		? GEngine->GetWorldFromContextObject(
			StoredWorldContextObject,
			EGetWorldErrorMode::ReturnNull
		)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInstance)
	{
		FinishRejected(
			OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("No Game Instance is available for Haptic Pattern playback."),
				TEXT("Use a world context that belongs to an active game.")
			)
		);
		return;
	}
	if (!RequestedPattern || !RequestedPattern->IsDerivedDataCurrent())
	{
		FinishRejected(
			OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The selected Haptic Pattern has invalid cooked data."),
				TEXT("Open and save the pattern asset, then resolve its validation errors.")
			)
		);
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileHapticPatternPlaybackAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishRejected(
			OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Haptics subsystem is unavailable."),
				TEXT("Enable OpenMobile Haptics for this project and Game Instance.")
			)
		);
		return;
	}

	const FSoftObjectPath Override =
		RequestedPattern->GetOverrideForCurrentPlatform();
	if (!Override.IsNull() && !Override.ResolveObject())
	{
		if (!bRequestedPrepareIfNeeded)
		{
			FinishRejected(
				OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::NotConfigured,
					TEXT("The selected pattern's platform override is not prepared."),
					TEXT("Enable Prepare If Needed or prepare this asset before playback.")
				)
			);
			return;
		}
		WaitingForPreparation.Broadcast();
		PreparationHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Override,
			FStreamableDelegate::CreateUObject(
				this,
				&UOpenMobileHapticPatternPlaybackAsyncAction::
					HandlePreparationFinished
			),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			TEXT("OpenMobile Haptic Pattern asset playback")
		);
		if (!PreparationHandle)
		{
			FinishRejected(
				OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The pattern's platform override could not begin loading."),
					TEXT("Check the override asset reference and try again.")
				)
			);
		}
		return;
	}
	SubmitPreparedPattern();
}

void UOpenMobileHapticPatternPlaybackAsyncAction::Cancel()
{
	check(IsInGameThread());
	if (!TryFinish())
	{
		return;
	}
	Cleanup();
	Cancelled.Broadcast();
	SetReadyToDestroy();
}

void UOpenMobileHapticPatternPlaybackAsyncAction::SubmitPreparedPattern()
{
	if (bFinished || !Subsystem.IsValid() || !RequestedPattern)
	{
		return;
	}
	ImmediateResult = Subsystem->SubmitPatternAsset(
		RequestedPattern,
		FMath::Clamp(RequestedIntensity, 0.0f, 1.0f),
		RequestedOptions
	);
	switch (ImmediateResult.Outcome)
	{
	case EOpenMobileHapticPlaybackOutcome::Accepted:
	case EOpenMobileHapticPlaybackOutcome::Fallback:
		if (ImmediateResult.Handle.IsValid())
		{
			Playback = NewObject<UOpenMobileHapticPlayback>(
				Subsystem->GetGameInstance()
			);
			Playback->InitializePlayback(
				Subsystem.Get(),
				ImmediateResult,
				RequestedPattern->GetFName()
			);
		}
		if (TryFinish())
		{
			Cleanup();
			Accepted.Broadcast(
				Playback,
				ImmediateResult.Outcome
					== EOpenMobileHapticPlaybackOutcome::Fallback,
				ImmediateResult.ResolvedPath
			);
			SetReadyToDestroy();
		}
		break;
	case EOpenMobileHapticPlaybackOutcome::Suppressed:
		if (TryFinish())
		{
			Cleanup();
			Suppressed.Broadcast(ImmediateResult);
			SetReadyToDestroy();
		}
		break;
	default:
		FinishRejected(ImmediateResult.Error);
		break;
	}
}

void UOpenMobileHapticPatternPlaybackAsyncAction::HandlePreparationFinished()
{
	if (bFinished || !RequestedPattern)
	{
		return;
	}
	const FSoftObjectPath Override =
		RequestedPattern->GetOverrideForCurrentPlatform();
	if (!Override.IsNull() && !Override.ResolveObject())
	{
		FinishRejected(
			OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The pattern's platform override failed to load."),
				TEXT("Open the override asset and resolve its validation errors.")
			)
		);
		return;
	}
	SubmitPreparedPattern();
}

void UOpenMobileHapticPatternPlaybackAsyncAction::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	static_cast<void>(bSessionEnded);
	static_cast<void>(bCleanupResources);
	if (!bFinished && World == TargetWorld.Get())
	{
		Cancel();
	}
}

void UOpenMobileHapticPatternPlaybackAsyncAction::FinishRejected(
	FOpenMobileHapticError Error
)
{
	if (!TryFinish())
	{
		return;
	}
	if (!Error.IsSet())
	{
		Error = OpenMobileHapticPatternPlaybackAsyncActionPrivate::MakeError(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Haptic Pattern playback was rejected.")
		);
	}
	Cleanup();
	Rejected.Broadcast(Error);
	SetReadyToDestroy();
}

void UOpenMobileHapticPatternPlaybackAsyncAction::Cleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
	if (PreparationHandle)
	{
		PreparationHandle->ReleaseHandle();
		PreparationHandle.Reset();
	}
	TargetWorld.Reset();
	Subsystem.Reset();
	StoredWorldContextObject = nullptr;
	RequestedPattern = nullptr;
}

bool UOpenMobileHapticPatternPlaybackAsyncAction::TryFinish()
{
	if (bFinished)
	{
		return false;
	}
	bFinished = true;
	return true;
}

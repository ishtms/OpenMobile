#include "OpenMobileHapticNamedPlaybackAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileHapticPlayback.h"
#include "OpenMobileHapticPreparationLease.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticNamedPlaybackAsyncActionPrivate
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

UOpenMobileHapticNamedPlaybackAsyncAction*
UOpenMobileHapticNamedPlaybackAsyncAction::PlayNamedHaptic(
	const UObject* WorldContextObject,
	FOpenMobileHapticPatternIdentifier Pattern,
	float Intensity,
	bool bPrepareIfNeeded,
	const FOpenMobileHapticPlaybackOptions& Options
)
{
	UOpenMobileHapticNamedPlaybackAsyncAction* Action =
		NewObject<UOpenMobileHapticNamedPlaybackAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->RequestedPattern = Pattern;
	Action->RequestedIntensity = Intensity;
	Action->bRequestedPrepareIfNeeded = bPrepareIfNeeded;
	Action->RequestedOptions = Options;
	return Action;
}

void UOpenMobileHapticNamedPlaybackAsyncAction::Activate()
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
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("No Game Instance is available for named Haptic playback."),
				TEXT("Use a world context that belongs to an active game.")
			)
		);
		return;
	}
	if (!RequestedPattern.IsValid())
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The Haptic pattern identifier is empty."),
				TEXT("Use a configured identifier or a direct pattern asset.")
			)
		);
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileHapticNamedPlaybackAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The Haptics subsystem is unavailable."),
				TEXT("Enable OpenMobile Haptics for this project and Game Instance.")
			)
		);
		return;
	}
	Subsystem->RegisterNamedPlaybackAction(this);
	if (Subsystem->GetNamedPatternStatus(RequestedPattern.Name)
			== EOpenMobileHapticNamedPatternStatus::Loaded
		&& Subsystem->GetPreparationState()
			== EOpenMobileHapticPreparationState::Prepared)
	{
		PreparationLease = Subsystem->AcquirePreparationLease();
		if (PreparationLease)
		{
			SubmitPreparedPattern();
			return;
		}
	}
	if (!bRequestedPrepareIfNeeded)
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::NotConfigured,
				TEXT("The configured Haptic pattern is not prepared."),
				TEXT("Enable Prepare If Needed or prepare content with an owned lease.")
			)
		);
		return;
	}

	WaitingForPreparation.Broadcast();
	Subsystem->OnNamedLibrariesPrepared.AddDynamic(
		this,
		&UOpenMobileHapticNamedPlaybackAsyncAction::HandlePreparationFinished
	);
	PreloadHandle = Subsystem->PreloadNamedLibrariesInternal(false);
	if (!PreloadHandle.IsValid())
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::Unavailable,
				TEXT("Haptics preparation could not start."),
				TEXT("Retry after the app is active and backend recovery completes.")
			)
		);
	}
}

void UOpenMobileHapticNamedPlaybackAsyncAction::Cancel()
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

void UOpenMobileHapticNamedPlaybackAsyncAction::HandlePreparationFinished(
	const FOpenMobileHapticLibraryPreloadResult& Result
)
{
	if (bFinished || Result.Handle != PreloadHandle)
	{
		return;
	}
	if (Result.Outcome != EOpenMobileHapticLibraryPreloadOutcome::Prepared
		|| !Subsystem.IsValid())
	{
		const FString Message = Result.Errors.IsEmpty()
			? TEXT("Haptics preparation failed.")
			: Result.Errors[0].Left(256);
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				Result.Outcome
					== EOpenMobileHapticLibraryPreloadOutcome::Cancelled
						? EOpenMobileErrorCode::Cancelled
						: EOpenMobileErrorCode::NativeFailure,
				Message
			)
		);
		return;
	}
	if (Subsystem->GetNamedPatternStatus(RequestedPattern.Name)
		!= EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::NotConfigured,
				TEXT("No prepared pattern matches the typed identifier."),
				TEXT("Select an identifier returned by the configured pattern query.")
			)
		);
		return;
	}
	PreparationLease = Subsystem->AcquirePreparationLease();
	if (!PreparationLease)
	{
		FinishRejected(
			OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
				EOpenMobileErrorCode::Unavailable,
				TEXT("Prepared Haptics content lost its Game Instance owner.")
			)
		);
		return;
	}
	SubmitPreparedPattern();
}

void UOpenMobileHapticNamedPlaybackAsyncAction::SubmitPreparedPattern()
{
	if (bFinished || !Subsystem.IsValid() || !PreparationLease)
	{
		return;
	}
	ImmediateResult = Subsystem->PlayNamedPatternAdvanced(
		RequestedPattern.Name,
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
				RequestedPattern.Name
			);
			Playback->AttachPreparationLease(PreparationLease);
			PreparationLease = nullptr;
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

void UOpenMobileHapticNamedPlaybackAsyncAction::HandleWorldCleanup(
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

void UOpenMobileHapticNamedPlaybackAsyncAction::HandleGameInstanceTeardown()
{
	if (!bFinished)
	{
		Cancel();
	}
}

void UOpenMobileHapticNamedPlaybackAsyncAction::FinishRejected(
	FOpenMobileHapticError Error
)
{
	if (!TryFinish())
	{
		return;
	}
	if (!Error.IsSet())
	{
		Error = OpenMobileHapticNamedPlaybackAsyncActionPrivate::MakeError(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Named Haptic playback was rejected.")
		);
	}
	ImmediateResult.Outcome = EOpenMobileHapticPlaybackOutcome::Rejected;
	ImmediateResult.State = EOpenMobileHapticPlaybackState::Failed;
	ImmediateResult.Error = Error;
	Cleanup();
	Rejected.Broadcast(Error);
	SetReadyToDestroy();
}

void UOpenMobileHapticNamedPlaybackAsyncAction::ReleasePreparationLease()
{
	if (PreparationLease)
	{
		PreparationLease->Release();
		PreparationLease = nullptr;
	}
}

void UOpenMobileHapticNamedPlaybackAsyncAction::Cleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
	if (Subsystem.IsValid())
	{
		Subsystem->OnNamedLibrariesPrepared.RemoveDynamic(
			this,
			&UOpenMobileHapticNamedPlaybackAsyncAction::HandlePreparationFinished
		);
		Subsystem->UnregisterNamedPlaybackAction(this);
	}
	ReleasePreparationLease();
	Subsystem.Reset();
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

bool UOpenMobileHapticNamedPlaybackAsyncAction::TryFinish()
{
	if (bFinished)
	{
		return false;
	}
	bFinished = true;
	return true;
}

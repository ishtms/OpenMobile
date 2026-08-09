#include "OpenMobileHapticPreparationAsyncAction.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPreparationLease.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsSubsystem.h"

namespace OpenMobileHapticPreparationAsyncActionPrivate
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

	FOpenMobileHapticPreparationResult MakeFailure(
		FOpenMobileHapticError Error
	)
	{
		FOpenMobileHapticPreparationResult Result;
		Result.Outcome = EOpenMobileHapticPreparationOutcome::Failed;
		Result.Error = MoveTemp(Error);
		Result.ItemErrors.Add(Result.Error);
		return Result;
	}

	void AddStringErrors(
		const TArray<FString>& Messages,
		FOpenMobileHapticPreparationResult& Result
	)
	{
		for (const FString& Message : Messages)
		{
			Result.ItemErrors.Add(MakeError(
				EOpenMobileErrorCode::NativeFailure,
				Message.Left(256)
			));
		}
		if (!Result.ItemErrors.IsEmpty())
		{
			Result.Error = Result.ItemErrors[0];
		}
	}
}

UOpenMobileHapticPreparationAsyncAction*
UOpenMobileHapticPreparationAsyncAction::PrepareHapticsAsync(
	const UObject* WorldContextObject
)
{
	UOpenMobileHapticPreparationAsyncAction* Action =
		NewObject<UOpenMobileHapticPreparationAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	return Action;
}

UOpenMobileHapticPreparationAsyncAction*
UOpenMobileHapticPreparationAsyncAction::PrepareHapticLibraryAsync(
	const UObject* WorldContextObject,
	FOpenMobileHapticLibraryIdentifier Library
)
{
	UOpenMobileHapticPreparationAsyncAction* Action =
		NewObject<UOpenMobileHapticPreparationAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->RequestedLibrary = Library;
	Action->PreparationTarget = EPreparationTarget::ConfiguredLibrary;
	return Action;
}

UOpenMobileHapticPreparationAsyncAction*
UOpenMobileHapticPreparationAsyncAction::PrepareHapticPatternAsync(
	const UObject* WorldContextObject,
	UOpenMobileHapticPatternAsset* Pattern
)
{
	UOpenMobileHapticPreparationAsyncAction* Action =
		NewObject<UOpenMobileHapticPreparationAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->RequestedPattern = Pattern;
	Action->PreparationTarget = EPreparationTarget::PatternAsset;
	return Action;
}

void UOpenMobileHapticPreparationAsyncAction::Activate()
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
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::InvalidArgument,
					TEXT("No Game Instance is available for Haptics preparation."),
					TEXT("Use a world context that belongs to an active game.")
				)
			)
		);
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	TargetGameInstance = GameInstance;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileHapticPreparationAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The Haptics subsystem is unavailable."),
					TEXT("Enable OpenMobile Haptics for this project and Game Instance.")
				)
			)
		);
		return;
	}
	Subsystem->RegisterPreparationAction(this);
	if (PreparationTarget == EPreparationTarget::PatternAsset)
	{
		ActivatePattern();
		return;
	}
	ActivateConfiguredLibraries();
}

void UOpenMobileHapticPreparationAsyncAction::ActivateConfiguredLibraries()
{
	if (PreparationTarget == EPreparationTarget::ConfiguredLibrary)
	{
		const bool bConfigured = RequestedLibrary.IsValid()
			&& GetDefault<UOpenMobileHapticsSettings>()->NamedLibraries
				.ContainsByPredicate(
					[this](
						const FOpenMobileHapticNamedLibrarySettings& Library
					)
					{
						return Library.Name == RequestedLibrary.Name
							&& !Library.Asset.IsNull();
					}
				);
		if (!bConfigured)
		{
			FinishFailed(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
					OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
						EOpenMobileErrorCode::NotConfigured,
						TEXT("The selected Haptic library is not configured."),
						TEXT("Select a project-backed library identifier in OpenMobile Haptics settings.")
					)
				)
			);
			return;
		}
	}
	Subsystem->OnNamedLibrariesPrepared.AddDynamic(
		this,
		&UOpenMobileHapticPreparationAsyncAction::HandlePreparationFinished
	);
	PreloadHandle = Subsystem->PreloadNamedLibrariesInternal(false);
	if (!PreloadHandle.IsValid())
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("Haptics preparation could not start."),
					TEXT("Retry after the app is active and backend recovery completes.")
				)
			)
		);
	}
}

void UOpenMobileHapticPreparationAsyncAction::ActivatePattern()
{
	if (!RequestedPattern || !RequestedPattern->IsDerivedDataCurrent())
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::InvalidArgument,
					TEXT("The selected Haptic Pattern has invalid cooked data."),
					TEXT("Select a valid pattern asset, then open and save it to rebuild cooked data.")
				)
			)
		);
		return;
	}
	const FSoftObjectPath Override =
		RequestedPattern->GetOverrideForCurrentPlatform();
	if (Override.IsNull() || Override.ResolveObject())
	{
		FinishPatternReady();
		return;
	}
	PatternPreparationHandle =
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Override,
			FStreamableDelegate::CreateUObject(
				this,
				&UOpenMobileHapticPreparationAsyncAction::
					HandlePatternPreparationFinished
			),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			TEXT("OpenMobile Haptic Pattern preparation")
		);
	if (!PatternPreparationHandle)
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The Haptic Pattern override could not begin loading."),
					TEXT("Check the current-platform override reference and try again.")
				)
			)
		);
	}
}

void UOpenMobileHapticPreparationAsyncAction::HandlePatternPreparationFinished()
{
	if (bFinished || !RequestedPattern)
	{
		return;
	}
	const FSoftObjectPath Override =
		RequestedPattern->GetOverrideForCurrentPlatform();
	if (!Override.IsNull() && !Override.ResolveObject())
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("The Haptic Pattern override failed to load."),
					TEXT("Open the override asset and resolve its validation errors.")
				)
			)
		);
		return;
	}
	FinishPatternReady();
}

void UOpenMobileHapticPreparationAsyncAction::FinishPatternReady()
{
	UGameInstance* GameInstance = TargetGameInstance.Get();
	if (!GameInstance || !RequestedPattern)
	{
		FinishFailed(
			OpenMobileHapticPreparationAsyncActionPrivate::MakeFailure(
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The Haptic Pattern lost its Game Instance owner."),
					TEXT("Start preparation again from an active game world.")
				)
			)
		);
		return;
	}
	const FSoftObjectPath Override =
		RequestedPattern->GetOverrideForCurrentPlatform();
	UOpenMobileHapticPreparationLease* Lease =
		NewObject<UOpenMobileHapticPreparationLease>(GameInstance);
	Lease->InitializeAssetLease(
		RequestedPattern,
		Override.ResolveObject(),
		MoveTemp(PatternPreparationHandle)
	);
	FOpenMobileHapticPreparationResult Result;
	Result.Outcome = EOpenMobileHapticPreparationOutcome::Ready;
	Result.Lease = Lease;
	Result.PreparedPatternCount = 1;
	FinishReady(MoveTemp(Result));
}

void UOpenMobileHapticPreparationAsyncAction::Cancel()
{
	check(IsInGameThread());
	if (bFinished)
	{
		return;
	}
	FOpenMobileHapticPreparationResult Result;
	Result.Outcome = EOpenMobileHapticPreparationOutcome::Cancelled;
	Result.Error = OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
		EOpenMobileErrorCode::Cancelled,
		TEXT("This caller stopped waiting for Haptics preparation.")
	);
	FinishCancelled(MoveTemp(Result));
}

void UOpenMobileHapticPreparationAsyncAction::HandlePreparationFinished(
	const FOpenMobileHapticLibraryPreloadResult& Result
)
{
	if (bFinished || Result.Handle != PreloadHandle)
	{
		return;
	}
	FOpenMobileHapticPreparationResult TypedResult;
	TypedResult.PreparedPatternCount = Result.PreparedPatternCount;
	OpenMobileHapticPreparationAsyncActionPrivate::AddStringErrors(
		Result.Errors,
		TypedResult
	);
	switch (Result.Outcome)
	{
	case EOpenMobileHapticLibraryPreloadOutcome::Prepared:
		TypedResult.Outcome = EOpenMobileHapticPreparationOutcome::Ready;
		TypedResult.Lease = Subsystem.IsValid()
			? Subsystem->AcquirePreparationLease()
			: nullptr;
		if (!TypedResult.Lease)
		{
			TypedResult.Outcome = EOpenMobileHapticPreparationOutcome::Failed;
			TypedResult.Error =
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Unavailable,
					TEXT("Prepared Haptics content lost its Game Instance owner."),
					TEXT("Start preparation again from an active game world.")
				);
			TypedResult.ItemErrors.Add(TypedResult.Error);
			FinishFailed(MoveTemp(TypedResult));
			return;
		}
		FinishReady(MoveTemp(TypedResult));
		break;
	case EOpenMobileHapticLibraryPreloadOutcome::Cancelled:
		TypedResult.Outcome = EOpenMobileHapticPreparationOutcome::Cancelled;
		if (!TypedResult.Error.IsSet())
		{
			TypedResult.Error =
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::Cancelled,
					TEXT("Haptics preparation was cancelled.")
				);
		}
		FinishCancelled(MoveTemp(TypedResult));
		break;
	default:
		TypedResult.Outcome = EOpenMobileHapticPreparationOutcome::Failed;
		if (!TypedResult.Error.IsSet())
		{
			TypedResult.Error =
				OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("Haptics preparation failed."),
					TEXT("Inspect the configured library and first typed item error.")
				);
			TypedResult.ItemErrors.Add(TypedResult.Error);
		}
		FinishFailed(MoveTemp(TypedResult));
		break;
	}
}

void UOpenMobileHapticPreparationAsyncAction::HandleWorldCleanup(
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

void UOpenMobileHapticPreparationAsyncAction::HandleGameInstanceTeardown()
{
	if (bFinished)
	{
		return;
	}
	FOpenMobileHapticPreparationResult Result;
	Result.Outcome = EOpenMobileHapticPreparationOutcome::Cancelled;
	Result.Error = OpenMobileHapticPreparationAsyncActionPrivate::MakeError(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The owning Game Instance ended during Haptics preparation.")
	);
	FinishCancelled(MoveTemp(Result));
}

void UOpenMobileHapticPreparationAsyncAction::FinishReady(
	FOpenMobileHapticPreparationResult Result
)
{
	if (!TryFinish())
	{
		return;
	}
	PreparationResult = MoveTemp(Result);
	Cleanup();
	Ready.Broadcast(PreparationResult);
	SetReadyToDestroy();
}

void UOpenMobileHapticPreparationAsyncAction::FinishCancelled(
	FOpenMobileHapticPreparationResult Result
)
{
	if (!TryFinish())
	{
		return;
	}
	PreparationResult = MoveTemp(Result);
	Cleanup();
	Cancelled.Broadcast(PreparationResult);
	SetReadyToDestroy();
}

void UOpenMobileHapticPreparationAsyncAction::FinishFailed(
	FOpenMobileHapticPreparationResult Result
)
{
	if (!TryFinish())
	{
		return;
	}
	PreparationResult = MoveTemp(Result);
	Cleanup();
	Failed.Broadcast(PreparationResult);
	SetReadyToDestroy();
}

void UOpenMobileHapticPreparationAsyncAction::Cleanup()
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
			&UOpenMobileHapticPreparationAsyncAction::HandlePreparationFinished
		);
		Subsystem->UnregisterPreparationAction(this);
		Subsystem.Reset();
	}
	if (PatternPreparationHandle)
	{
		PatternPreparationHandle->CancelHandle();
		PatternPreparationHandle->ReleaseHandle();
		PatternPreparationHandle.Reset();
	}
	RequestedPattern = nullptr;
	RequestedLibrary = {};
	TargetGameInstance.Reset();
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

bool UOpenMobileHapticPreparationAsyncAction::TryFinish()
{
	if (bFinished)
	{
		return false;
	}
	bFinished = true;
	return true;
}

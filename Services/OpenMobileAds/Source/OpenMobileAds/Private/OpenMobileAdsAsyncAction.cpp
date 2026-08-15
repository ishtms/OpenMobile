#include "OpenMobileAdsAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileAdsSubsystem.h"

namespace
{
	bool CompletesShowWhenShown(EOpenMobileAdFormat Format)
	{
		return Format == EOpenMobileAdFormat::Banner
			|| Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
			|| Format == EOpenMobileAdFormat::MediumRectangle;
	}
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::Create(
	const UObject* WorldContextObject,
	EOperation InOperation,
	FName InPlacement
)
{
	UOpenMobileAdsAsyncAction* Action = NewObject<UOpenMobileAdsAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Operation = InOperation;
	Action->Placement = InPlacement;
	return Action;
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::LoadAd(
	const UObject* WorldContextObject,
	FName Placement,
	FOpenMobileAdsLoadOptions Options
)
{
	UOpenMobileAdsAsyncAction* Action = Create(
		WorldContextObject,
		EOperation::Load,
		Placement
	);
	Action->LoadOptions = MoveTemp(Options);
	return Action;
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::ShowAd(
	const UObject* WorldContextObject,
	FName Placement,
	FOpenMobileAdsShowOptions Options
)
{
	UOpenMobileAdsAsyncAction* Action = Create(
		WorldContextObject,
		EOperation::Show,
		Placement
	);
	Action->ShowOptions = MoveTemp(Options);
	return Action;
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::ReloadAd(
	const UObject* WorldContextObject,
	FName Placement
)
{
	return Create(WorldContextObject, EOperation::Reload, Placement);
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::HideAd(
	const UObject* WorldContextObject,
	FName Placement
)
{
	return Create(WorldContextObject, EOperation::Hide, Placement);
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::DestroyAd(
	const UObject* WorldContextObject,
	FName Placement
)
{
	return Create(WorldContextObject, EOperation::Destroy, Placement);
}

UOpenMobileAdsAsyncAction* UOpenMobileAdsAsyncAction::DestroyAllAds(
	const UObject* WorldContextObject
)
{
	return Create(WorldContextObject, EOperation::DestroyAll, NAME_None);
}

void UOpenMobileAdsAsyncAction::Activate()
{
	Super::Activate();
	if (!StoredWorldContextObject || !GEngine)
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			GetFailureStage(),
			Placement,
			TEXT("The async ads operation requires a valid world context object.")
		));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(
		StoredWorldContextObject,
		EGetWorldErrorMode::ReturnNull
	);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInstance)
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			GetFailureStage(),
			Placement,
			TEXT("The async ads operation could not resolve a game instance.")
		));
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileAdsAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileAdsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			GetFailureStage(),
			Placement,
			TEXT("The Open Mobile Ads subsystem is unavailable.")
		));
		return;
	}

	AdsEventHandle = Subsystem->OnNativeAdsEvent().AddUObject(
		this,
		&UOpenMobileAdsAsyncAction::HandleAdsEvent
	);
	FOpenMobileAdsOperationResult Result;
	switch (Operation)
	{
	case EOperation::Load:
		Result = Subsystem->LoadAd(Placement, MoveTemp(LoadOptions));
		break;
	case EOperation::Reload:
		Result = Subsystem->ReloadAd(Placement);
		break;
	case EOperation::Show:
		Result = Subsystem->ShowAd(Placement, MoveTemp(ShowOptions));
		break;
	case EOperation::Hide:
		Result = Subsystem->HideAd(Placement);
		break;
	case EOperation::Destroy:
		Result = Subsystem->DestroyAd(Placement);
		break;
	case EOperation::DestroyAll:
		Result = Subsystem->DestroyAllAds();
		break;
	}

	if (!Result.bAccepted)
	{
		FinishFailed(Result.Error);
		return;
	}
	RequestId = Result.RequestId;
}

void UOpenMobileAdsAsyncAction::Cancel()
{
	if (bFinished)
	{
		return;
	}
	if (Subsystem.IsValid() && RequestId.IsValid())
	{
		const FOpenMobileAdsOperationResult Result = Subsystem->CancelRequest(RequestId);
		if (Result.bAccepted)
		{
			return;
		}
	}
	FinishCancelled(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::Cancelled,
		GetFailureStage(),
		Placement,
		TEXT("The async ads operation was cancelled.")
	));
}

void UOpenMobileAdsAsyncAction::HandleAdsEvent(const FOpenMobileAdsEvent& Event)
{
	if (bFinished || Event.RequestId != RequestId)
	{
		return;
	}
	if (Event.Error.Code == EOpenMobileAdsErrorCode::Cancelled)
	{
		FinishCancelled(Event.Error);
		return;
	}
	if (Event.Type == EOpenMobileAdsEventType::LoadFailed
		|| Event.Type == EOpenMobileAdsEventType::Failed)
	{
		FinishFailed(Event.Error);
		return;
	}

	const bool bCompleted =
		((Operation == EOperation::Load || Operation == EOperation::Reload)
			&& Event.Type == EOpenMobileAdsEventType::Loaded)
		|| (
			Operation == EOperation::Show
			&& (
				(Event.Type == EOpenMobileAdsEventType::Shown
					&& CompletesShowWhenShown(Event.Format))
				|| (Event.Type == EOpenMobileAdsEventType::Dismissed
					&& !CompletesShowWhenShown(Event.Format))
			)
		)
		|| (Operation == EOperation::Hide && Event.Type == EOpenMobileAdsEventType::Hidden)
		|| ((Operation == EOperation::Destroy || Operation == EOperation::DestroyAll)
			&& Event.Type == EOpenMobileAdsEventType::Destroyed);
	if (bCompleted)
	{
		FinishCompleted(Event);
	}
}

void UOpenMobileAdsAsyncAction::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	if (bFinished || World != TargetWorld.Get())
	{
		return;
	}
	if (Subsystem.IsValid() && RequestId.IsValid())
	{
		Subsystem->CancelRequest(RequestId);
	}
	FinishCancelled(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::Cancelled,
		GetFailureStage(),
		Placement,
		TEXT("The async ads operation was cancelled because its world is shutting down.")
	));
}

void UOpenMobileAdsAsyncAction::FinishCompleted(const FOpenMobileAdsEvent& Event)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	Cleanup();
	OnCompleted.Broadcast(Event);
	SetReadyToDestroy();
}

void UOpenMobileAdsAsyncAction::FinishFailed(const FOpenMobileAdsError& Error)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	Cleanup();
	OnFailed.Broadcast(Error);
	SetReadyToDestroy();
}

void UOpenMobileAdsAsyncAction::FinishCancelled(const FOpenMobileAdsError& Error)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	Cleanup();
	OnCancelled.Broadcast(Error);
	SetReadyToDestroy();
}

void UOpenMobileAdsAsyncAction::Cleanup()
{
	if (Subsystem.IsValid() && AdsEventHandle.IsValid())
	{
		Subsystem->OnNativeAdsEvent().Remove(AdsEventHandle);
	}
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	}
	AdsEventHandle.Reset();
	WorldCleanupHandle.Reset();
	Subsystem.Reset();
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

EOpenMobileAdsFailureStage UOpenMobileAdsAsyncAction::GetFailureStage() const
{
	switch (Operation)
	{
	case EOperation::Load:
	case EOperation::Reload:
		return EOpenMobileAdsFailureStage::Load;
	case EOperation::Show:
		return EOpenMobileAdsFailureStage::Show;
	case EOperation::Hide:
		return EOpenMobileAdsFailureStage::Hide;
	case EOperation::Destroy:
	case EOperation::DestroyAll:
		return EOpenMobileAdsFailureStage::Teardown;
	}
	return EOpenMobileAdsFailureStage::Internal;
}

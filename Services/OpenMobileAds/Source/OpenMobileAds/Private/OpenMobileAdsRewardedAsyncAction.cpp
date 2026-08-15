#include "OpenMobileAdsRewardedAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileAdsSubsystem.h"

UOpenMobileAdsRewardedAsyncAction*
UOpenMobileAdsRewardedAsyncAction::ShowRewardedAd(
	const UObject* WorldContextObject,
	FName Placement,
	FOpenMobileAdsShowOptions Options
)
{
	UOpenMobileAdsRewardedAsyncAction* Action =
		NewObject<UOpenMobileAdsRewardedAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Placement = Placement;
	Action->Options = MoveTemp(Options);
	return Action;
}

void UOpenMobileAdsRewardedAsyncAction::Activate()
{
	Super::Activate();
	if (!StoredWorldContextObject || !GEngine)
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("Show Rewarded Ad Async requires a valid world context object.")
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
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("Show Rewarded Ad Async could not resolve a game instance.")
		));
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileAdsRewardedAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileAdsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("The OpenMobile Ads subsystem is unavailable.")
		));
		return;
	}

	const EOpenMobileAdFormat Format =
		Subsystem->GetPlacementStatus(Placement).Format;
	if (Format != EOpenMobileAdFormat::Rewarded
		&& Format != EOpenMobileAdFormat::RewardedInterstitial)
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnsupportedFormat,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("Show Rewarded Ad Async requires a rewarded or rewarded-interstitial placement.")
		));
		return;
	}

	AdsEventHandle = Subsystem->OnNativeAdsEvent().AddUObject(
		this,
		&UOpenMobileAdsRewardedAsyncAction::HandleAdsEvent
	);
	const FOpenMobileAdsOperationResult Result =
		Subsystem->ShowAd(Placement, MoveTemp(Options));
	if (!Result.bAccepted)
	{
		FinishFailed(Result.Error);
		return;
	}
	RequestId = Result.RequestId;
}

void UOpenMobileAdsRewardedAsyncAction::Cancel()
{
	if (bFinished)
	{
		return;
	}
	if (Subsystem.IsValid() && RequestId.IsValid())
	{
		const FOpenMobileAdsOperationResult Result =
			Subsystem->CancelRequest(RequestId);
		if (Result.bAccepted)
		{
			return;
		}
	}
	FinishCancelled(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::Cancelled,
		EOpenMobileAdsFailureStage::Show,
		Placement,
		TEXT("The rewarded Ads listener was cancelled.")
	));
}

void UOpenMobileAdsRewardedAsyncAction::HandleAdsEvent(
	const FOpenMobileAdsEvent& Event
)
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
	if (Event.Type == EOpenMobileAdsEventType::Failed)
	{
		FinishFailed(Event.Error);
		return;
	}
	switch (Event.Type)
	{
	case EOpenMobileAdsEventType::Shown:
		OnShown.Broadcast(Event);
		break;
	case EOpenMobileAdsEventType::RewardEarned:
		bRewardReceived = true;
		OnRewardEarned.Broadcast(Event);
		if (bDismissed)
		{
			FinishListening();
		}
		break;
	case EOpenMobileAdsEventType::Dismissed:
		bDismissed = true;
		OnDismissed.Broadcast(Event);
		if (bRewardReceived)
		{
			FinishListening();
		}
		else
		{
			DismissalGraceHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(
					this,
					&UOpenMobileAdsRewardedAsyncAction::HandleDismissalGrace
				),
				1.0f
			);
		}
		break;
	default:
		break;
	}
}

bool UOpenMobileAdsRewardedAsyncAction::HandleDismissalGrace(float DeltaTime)
{
	static_cast<void>(DeltaTime);
	DismissalGraceHandle.Reset();
	FinishListening();
	return false;
}

void UOpenMobileAdsRewardedAsyncAction::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	static_cast<void>(bSessionEnded);
	static_cast<void>(bCleanupResources);
	if (!bFinished && World == TargetWorld.Get())
	{
		if (Subsystem.IsValid() && RequestId.IsValid())
		{
			Subsystem->CancelRequest(RequestId);
		}
		FinishCancelled(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::Cancelled,
			EOpenMobileAdsFailureStage::Show,
			Placement,
			TEXT("The rewarded Ads listener was cancelled because its world is shutting down.")
		));
	}
}

void UOpenMobileAdsRewardedAsyncAction::FinishFailed(
	const FOpenMobileAdsError& Error
)
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

void UOpenMobileAdsRewardedAsyncAction::FinishCancelled(
	const FOpenMobileAdsError& Error
)
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

void UOpenMobileAdsRewardedAsyncAction::FinishListening()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	Cleanup();
	SetReadyToDestroy();
}

void UOpenMobileAdsRewardedAsyncAction::Cleanup()
{
	if (Subsystem.IsValid() && AdsEventHandle.IsValid())
	{
		Subsystem->OnNativeAdsEvent().Remove(AdsEventHandle);
	}
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	}
	if (DismissalGraceHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DismissalGraceHandle);
	}
	AdsEventHandle.Reset();
	WorldCleanupHandle.Reset();
	DismissalGraceHandle.Reset();
	Subsystem.Reset();
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

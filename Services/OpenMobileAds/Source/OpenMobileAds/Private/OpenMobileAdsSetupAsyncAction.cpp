#include "OpenMobileAdsSetupAsyncAction.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileAdsSubsystem.h"

UOpenMobileAdsSetupAsyncAction* UOpenMobileAdsSetupAsyncAction::Create(
	const UObject* WorldContextObject,
	EOperation InOperation
)
{
	UOpenMobileAdsSetupAsyncAction* Action =
		NewObject<UOpenMobileAdsSetupAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Operation = InOperation;
	return Action;
}

UOpenMobileAdsSetupAsyncAction*
UOpenMobileAdsSetupAsyncAction::RefreshAdsConsent(
	const UObject* WorldContextObject
)
{
	return Create(WorldContextObject, EOperation::RefreshConsent);
}

UOpenMobileAdsSetupAsyncAction*
UOpenMobileAdsSetupAsyncAction::PresentAdsPrivacyOptions(
	const UObject* WorldContextObject
)
{
	return Create(WorldContextObject, EOperation::PresentPrivacyOptions);
}

UOpenMobileAdsSetupAsyncAction*
UOpenMobileAdsSetupAsyncAction::RequestTrackingAuthorization(
	const UObject* WorldContextObject
)
{
	return Create(WorldContextObject, EOperation::RequestTrackingAuthorization);
}

UOpenMobileAdsSetupAsyncAction* UOpenMobileAdsSetupAsyncAction::InitializeAds(
	const UObject* WorldContextObject
)
{
	return Create(WorldContextObject, EOperation::Initialize);
}

void UOpenMobileAdsSetupAsyncAction::Activate()
{
	Super::Activate();
	if (!StoredWorldContextObject || !GEngine)
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			GetFailureStage(),
			NAME_None,
			TEXT("The async Ads setup operation requires a valid world context object.")
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
			NAME_None,
			TEXT("The async Ads setup operation could not resolve a game instance.")
		));
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileAdsSetupAsyncAction::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileAdsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::InvalidState,
			GetFailureStage(),
			NAME_None,
			TEXT("The OpenMobile Ads subsystem is unavailable.")
		));
		return;
	}

	InitializationHandle = Subsystem->OnNativeInitializationStatusChanged().AddUObject(
		this,
		&UOpenMobileAdsSetupAsyncAction::HandleInitialization
	);
	ConsentHandle = Subsystem->OnNativeConsentStatusChanged().AddUObject(
		this,
		&UOpenMobileAdsSetupAsyncAction::HandleConsent
	);
	TrackingHandle = Subsystem->OnNativeTrackingAuthorizationRequestCompleted().AddUObject(
		this,
		&UOpenMobileAdsSetupAsyncAction::HandleTracking
	);

	FOpenMobileAdsOperationResult Result;
	switch (Operation)
	{
	case EOperation::RefreshConsent:
		Result = Subsystem->RefreshConsent();
		break;
	case EOperation::PresentPrivacyOptions:
		Result = Subsystem->PresentPrivacyOptionsForm();
		break;
	case EOperation::RequestTrackingAuthorization:
		Result = Subsystem->RequestTrackingAuthorization();
		break;
	case EOperation::Initialize:
		Result = Subsystem->InitializeAds();
		break;
	}
	if (!Result.bAccepted)
	{
		FinishFailed(Result.Error);
		return;
	}
	RequestId = Result.RequestId;
	TryCompleteFromCurrentState();
}

void UOpenMobileAdsSetupAsyncAction::Cancel()
{
	if (!bFinished)
	{
		FinishCancelled();
	}
}

void UOpenMobileAdsSetupAsyncAction::HandleInitialization(
	const FOpenMobileAdsInitializationStatusSnapshot& Status
)
{
	if (bFinished || Operation != EOperation::Initialize
		|| Status.RequestId != RequestId)
	{
		return;
	}
	if (Status.ServiceState == EOpenMobileAdsServiceState::Ready)
	{
		FinishCompleted();
	}
	else if (Status.ServiceState == EOpenMobileAdsServiceState::Failed)
	{
		FinishFailed(Status.Error);
	}
}

void UOpenMobileAdsSetupAsyncAction::HandleConsent(
	const FOpenMobileAdsPrivacySnapshot& Privacy
)
{
	if (bFinished
		|| (Operation != EOperation::RefreshConsent
			&& Operation != EOperation::PresentPrivacyOptions)
		|| Privacy.RequestId != RequestId
		|| Privacy.ConsentActivity != EOpenMobileAdsConsentActivity::Idle)
	{
		return;
	}
	if (Privacy.Error.IsSet())
	{
		FinishFailed(Privacy.Error);
	}
	else
	{
		FinishCompleted();
	}
}

void UOpenMobileAdsSetupAsyncAction::HandleTracking(
	FGuid CompletedRequestId,
	EOpenMobileAdsTrackingAuthorizationStatus Status
)
{
	static_cast<void>(Status);
	if (!bFinished
		&& Operation == EOperation::RequestTrackingAuthorization
		&& CompletedRequestId == RequestId)
	{
		FinishCompleted();
	}
}

void UOpenMobileAdsSetupAsyncAction::TryCompleteFromCurrentState()
{
	if (!Subsystem.IsValid() || bFinished)
	{
		return;
	}
	if (Operation == EOperation::Initialize)
	{
		HandleInitialization(Subsystem->GetInitializationStatusRef());
	}
	else if (Operation == EOperation::RefreshConsent
		|| Operation == EOperation::PresentPrivacyOptions)
	{
		HandleConsent(Subsystem->GetPrivacySnapshot());
	}
	else if (
		Subsystem->GetTrackingAuthorizationStatus()
			!= EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined
	)
	{
		FinishCompleted();
	}
}

FOpenMobileAdsSetupResult UOpenMobileAdsSetupAsyncAction::MakeResult(
	const FOpenMobileAdsError& Error
) const
{
	FOpenMobileAdsSetupResult Result;
	Result.RequestId = RequestId;
	Result.Error = Error;
	if (Subsystem.IsValid())
	{
		Result.ServiceState = Subsystem->GetServiceState();
		Result.Initialization = Subsystem->GetInitializationStatusRef();
		Result.Privacy = Subsystem->GetPrivacySnapshot();
		Result.TrackingAuthorization =
			Subsystem->GetTrackingAuthorizationStatus();
		Result.RequestEligibility = Subsystem->CanRequestAds();
		Result.ActiveProvider = Subsystem->GetActiveProviderName();
	}
	return Result;
}

void UOpenMobileAdsSetupAsyncAction::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	static_cast<void>(bSessionEnded);
	static_cast<void>(bCleanupResources);
	if (!bFinished && World == TargetWorld.Get())
	{
		FinishCancelled();
	}
}

void UOpenMobileAdsSetupAsyncAction::FinishCompleted()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	const FOpenMobileAdsSetupResult Result = MakeResult();
	Cleanup();
	OnCompleted.Broadcast(Result);
	SetReadyToDestroy();
}

void UOpenMobileAdsSetupAsyncAction::FinishFailed(
	const FOpenMobileAdsError& Error
)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	const FOpenMobileAdsSetupResult Result = MakeResult(Error);
	Cleanup();
	OnFailed.Broadcast(Result);
	SetReadyToDestroy();
}

void UOpenMobileAdsSetupAsyncAction::FinishCancelled()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	const FOpenMobileAdsError Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::Cancelled,
		GetFailureStage(),
		NAME_None,
		TEXT("The request-local Ads setup listener was cancelled. The platform operation may still finish globally.")
	);
	const FOpenMobileAdsSetupResult Result = MakeResult(Error);
	Cleanup();
	OnCancelled.Broadcast(Result);
	SetReadyToDestroy();
}

void UOpenMobileAdsSetupAsyncAction::Cleanup()
{
	if (Subsystem.IsValid())
	{
		Subsystem->OnNativeInitializationStatusChanged().Remove(
			InitializationHandle
		);
		Subsystem->OnNativeConsentStatusChanged().Remove(ConsentHandle);
		Subsystem->OnNativeTrackingAuthorizationRequestCompleted().Remove(
			TrackingHandle
		);
	}
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	}
	InitializationHandle.Reset();
	ConsentHandle.Reset();
	TrackingHandle.Reset();
	WorldCleanupHandle.Reset();
	Subsystem.Reset();
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}

EOpenMobileAdsFailureStage UOpenMobileAdsSetupAsyncAction::GetFailureStage() const
{
	switch (Operation)
	{
	case EOperation::RefreshConsent:
	case EOperation::PresentPrivacyOptions:
		return EOpenMobileAdsFailureStage::Consent;
	case EOperation::RequestTrackingAuthorization:
		return EOpenMobileAdsFailureStage::TrackingAuthorization;
	case EOperation::Initialize:
		return EOpenMobileAdsFailureStage::Initialization;
	}
	return EOpenMobileAdsFailureStage::Internal;
}

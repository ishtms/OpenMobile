#include "OpenMobileDeviceEndpointReachabilityAsyncAction.h"

#include "Containers/Ticker.h"
#include "OpenMobileDeviceEndpointReachabilityPolicy.h"
#include "OpenMobileDeviceEndpointTransport.h"
#include "OpenMobileDeviceSettings.h"

UOpenMobileDeviceEndpointReachabilityAsyncAction*
UOpenMobileDeviceEndpointReachabilityAsyncAction::TestEndpointReachability(
	const UObject* WorldContextObject,
	const FString& HttpsEndpoint,
	FOpenMobileEndpointReachabilityOptions Options
)
{
	UOpenMobileDeviceEndpointReachabilityAsyncAction* Action =
		NewObject<UOpenMobileDeviceEndpointReachabilityAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Endpoint = HttpsEndpoint;
	Action->RequestOptions = Options;
	return Action;
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::Activate()
{
	check(IsInGameThread());
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	RequestOptions =
		FOpenMobileDeviceEndpointReachabilityPolicy::NormalizeOptions(
			RequestOptions
		);
	if (!FOpenMobileDeviceEndpointReachabilityPolicy::IsValidEndpoint(Endpoint))
	{
		Result.Outcome = EOpenMobileEndpointReachabilityOutcome::InvalidRequest;
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Endpoint reachability requires an explicit HTTPS URL without embedded credentials.")
		));
		return;
	}
	const int32 MaximumConcurrentRequests =
		GetDefault<UOpenMobileDeviceSettings>()
			->GetValidatedEndpointReachabilityMaximumConcurrentRequests();
	if (!FOpenMobileDeviceEndpointRequestLimiter::TryAcquire(
		MaximumConcurrentRequests
	))
	{
		CompleteResult(
			EOpenMobileEndpointReachabilityOutcome::ConcurrencyLimitReached
		);
		return;
	}
	bRequestSlotAcquired = true;
	StartedAtSeconds = FPlatformTime::Seconds();
	TimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleTimeoutTick
		),
		0.1f
	);
	CancellationFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	StartHttpRequest();
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::StartHttpRequest()
{
	const TWeakObjectPtr<UOpenMobileDeviceEndpointReachabilityAsyncAction> WeakThis(this);
	FOpenMobileDeviceEndpointTransport::Start(Endpoint, RequestOptions, CancellationFlag.ToSharedRef(),
		[WeakThis](FOpenMobileDeviceEndpointResponse Response)
	{
		if (!WeakThis.IsValid() || WeakThis->IsFinished()) return;
		auto& Action = *WeakThis.Get();
		Action.Result.bHttpStatusAvailable = Response.Evidence.StatusCode > 0;
		Action.Result.HttpStatusCode = Response.Evidence.StatusCode;
		Action.Result.ResponseBytes = Response.ResponseBytes;
		Action.Result.bRedirected = Response.Evidence.bRedirected;
		Action.CompleteResult(FOpenMobileDeviceEndpointReachabilityPolicy::Classify(
			Response.Evidence, Action.RequestOptions));
	});
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::CancelNativeOperation()
{
	if (CancellationFlag)
	{
		CancellationFlag->Store(true);
	}
	Result.Outcome = EOpenMobileEndpointReachabilityOutcome::Cancelled;
	StopTimeoutTicker();
	ReleaseRequestSlot();
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::OnActionSucceeded()
{
	Completed.Broadcast(Result);
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	static_cast<void>(Error);
	Result.Outcome = EOpenMobileEndpointReachabilityOutcome::Cancelled;
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::CompleteResult(
	EOpenMobileEndpointReachabilityOutcome Outcome
)
{
	Result.Outcome = Outcome;
	Result.DurationSeconds = StartedAtSeconds > 0.0
		? static_cast<float>(FPlatformTime::Seconds() - StartedAtSeconds)
		: 0.0f;
	StopTimeoutTicker();
	ReleaseRequestSlot();
	FinishSucceeded();
}

bool UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleTimeoutTick(
	float DeltaTime
)
{
	static_cast<void>(DeltaTime);
	if (IsFinished())
	{
		TimeoutTickerHandle.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() - StartedAtSeconds
		< RequestOptions.TimeoutSeconds)
	{
		return true;
	}
	if (CancellationFlag)
	{
		CancellationFlag->Store(true);
	}
	TimeoutTickerHandle.Reset();
	CompleteResult(EOpenMobileEndpointReachabilityOutcome::Timeout);
	return false;
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::StopTimeoutTicker()
{
	if (TimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTickerHandle);
		TimeoutTickerHandle.Reset();
	}
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::ReleaseRequestSlot()
{
	if (bRequestSlotAcquired)
	{
		FOpenMobileDeviceEndpointRequestLimiter::Release();
		bRequestSlotAcquired = false;
	}
}

#include "OpenMobileDeviceEndpointReachabilityAsyncAction.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAsync.h"
#include "OpenMobileDeviceEndpointReachabilityPolicy.h"
#include "OpenMobileDeviceSettings.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

struct UOpenMobileDeviceEndpointReachabilityAsyncAction::FResponseBodyState
{
	FCriticalSection Mutex;
	int64 BytesReceived = 0;
	int64 MaximumBytes = 0;
	bool bTooLarge = false;

	void Receive(int64& InOutLength)
	{
		FScopeLock Lock(&Mutex);
		if (InOutLength < 0
			|| InOutLength > MaximumBytes - BytesReceived)
		{
			bTooLarge = true;
			InOutLength = 0;
			return;
		}
		BytesReceived += InOutLength;
	}

	void Read(int64& OutBytesReceived, bool& bOutTooLarge)
	{
		FScopeLock Lock(&Mutex);
		OutBytesReceived = BytesReceived;
		bOutTooLarge = bTooLarge;
	}
};

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
	EndpointHost = FGenericPlatformHttp::GetUrlDomain(Endpoint);
	CancellationFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	StartDnsResolution();
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::StartDnsResolution()
{
	ISocketSubsystem* SocketSubsystem =
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem || EndpointHost.IsEmpty())
	{
		CompleteResult(EOpenMobileEndpointReachabilityOutcome::DnsFailure);
		return;
	}
	const TWeakObjectPtr<UOpenMobileDeviceEndpointReachabilityAsyncAction>
		WeakThis(this);
	const TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> WorkerCancellation =
		CancellationFlag;
	const uint16 EndpointPort =
		FGenericPlatformHttp::GetUrlPort(Endpoint).Get(443);
	const double DeadlineSeconds =
		StartedAtSeconds + RequestOptions.TimeoutSeconds;
	SocketSubsystem->GetAddressInfoAsync(
		[
			WeakThis,
			WorkerCancellation,
			SocketSubsystem,
			EndpointPort,
			DeadlineSeconds
		](FAddressInfoResult AddressInfo) mutable
		{
			const bool bDnsSucceeded = AddressInfo.ReturnCode == SE_NO_ERROR
				&& !AddressInfo.Results.IsEmpty();
			bool bTcpSucceeded = false;
			if (bDnsSucceeded)
			{
				int32 RemainingAddresses = AddressInfo.Results.Num();
				for (FAddressInfoResultData& Result : AddressInfo.Results)
				{
					const double AttemptDeadline = FOpenMobileDeviceEndpointReachabilityPolicy::
						GetConnectAttemptDeadline(FPlatformTime::Seconds(), DeadlineSeconds, RemainingAddresses--);
					if (WorkerCancellation->Load()
						|| FPlatformTime::Seconds() >= DeadlineSeconds)
					{
						break;
					}
					Result.Address->SetPort(EndpointPort);
					FUniqueSocket Socket = SocketSubsystem->CreateUniqueSocket(
						NAME_Stream,
						TEXT("OpenMobileEndpointReachability"),
						Result.AddressProtocolName
					);
					if (!Socket.IsValid() || !Socket->SetNonBlocking(true)
						|| !Socket->Connect(*Result.Address))
					{
						continue;
					}
					while (!WorkerCancellation->Load())
					{
						const double RemainingSeconds =
							AttemptDeadline - FPlatformTime::Seconds();
						if (RemainingSeconds <= 0.0)
						{
							break;
						}
						const double WaitSeconds = FMath::Min(
							RemainingSeconds,
							0.05
						);
						if (!Socket->Wait(
							ESocketWaitConditions::WaitForWrite,
							FTimespan::FromSeconds(WaitSeconds)
						))
						{
							continue;
						}
						bTcpSucceeded = Socket->GetConnectionState()
							== SCS_Connected;
						break;
					}
					if (bTcpSucceeded)
					{
						break;
					}
				}
			}
			OpenMobile::DispatchToGameThread([
				WeakThis,
				WorkerCancellation,
				bDnsSucceeded,
				bTcpSucceeded
			]()
			{
				if (!WorkerCancellation->Load() && WeakThis.IsValid())
				{
					WeakThis->HandleConnectionPreflight(
						bDnsSucceeded,
						bTcpSucceeded
					);
				}
			});
		},
		*EndpointHost,
		nullptr,
		EAddressInfoFlags::Default,
		NAME_None,
		SOCKTYPE_Streaming
	);
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleConnectionPreflight(
	bool bDnsSucceeded,
	bool bTcpSucceeded
)
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	if (!bDnsSucceeded)
	{
		CompleteResult(EOpenMobileEndpointReachabilityOutcome::DnsFailure);
		return;
	}
	if (!bTcpSucceeded)
	{
		CompleteResult(EOpenMobileEndpointReachabilityOutcome::ConnectionFailure);
		return;
	}
	bTcpConnectionSucceeded = true;
	StartHttpRequest();
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::StartHttpRequest()
{
	ResponseBodyState = MakeShared<FResponseBodyState, ESPMode::ThreadSafe>();
	ResponseBodyState->MaximumBytes = RequestOptions.MaximumResponseBytes;
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Endpoint);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(RequestOptions.TimeoutSeconds);
	HttpRequest->SetActivityTimeout(RequestOptions.TimeoutSeconds);
	HttpRequest->SetOption(HttpRequestOptions::EnableCookies, TEXT("false"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("*/*"));
	HttpRequest->SetDelegateThreadPolicy(
		EHttpRequestDelegateThreadPolicy::CompleteOnGameThread
	);
	const TSharedPtr<FResponseBodyState, ESPMode::ThreadSafe> BodyState =
		ResponseBodyState;
	if (!HttpRequest->SetResponseBodyReceiveStreamDelegateV2(
		FHttpRequestStreamDelegateV2::CreateLambda(
			[BodyState](void* Data, int64& InOutLength)
			{
				static_cast<void>(Data);
				BodyState->Receive(InOutLength);
			}
		)
	))
	{
		HttpRequest.Reset();
		ResponseBodyState.Reset();
		CompleteResult(EOpenMobileEndpointReachabilityOutcome::TransportFailure);
		return;
	}
	HttpRequest->OnStatusCodeReceived().BindUObject(
		this,
		&UOpenMobileDeviceEndpointReachabilityAsyncAction::
			HandleStatusCodeReceived
	);
	HttpRequest->OnProcessRequestComplete().BindUObject(
		this,
		&UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleHttpComplete
	);
	if (!HttpRequest->ProcessRequest())
	{
		HttpRequest.Reset();
		CompleteResult(EOpenMobileEndpointReachabilityOutcome::TransportFailure);
	}
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleStatusCodeReceived(
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
	int32 StatusCode
)
{
	if (!RequestOptions.bAllowRedirects
		&& StatusCode >= 300
		&& StatusCode <= 399)
	{
		bRedirectRejected = true;
		Request->CancelRequest();
	}
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::HandleHttpComplete(
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response,
	bool bProcessedSuccessfully
)
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	FOpenMobileDeviceEndpointCompletionEvidence Evidence;
	Evidence.bTcpConnectionSucceeded = bTcpConnectionSucceeded;
	if (Response)
	{
		Evidence.bResponseReceived = true;
		Evidence.StatusCode = Response->GetResponseCode();
		Result.bHttpStatusAvailable = Evidence.StatusCode > 0;
		Result.HttpStatusCode = Evidence.StatusCode;
	}
	const FString EffectiveUrl = Request->GetEffectiveURL();
	Evidence.bRedirected = bRedirectRejected
		|| (!EffectiveUrl.IsEmpty() && EffectiveUrl != Endpoint);
	if (Evidence.bRedirected
		&& (!RequestOptions.bAllowRedirects
			|| !FOpenMobileDeviceEndpointReachabilityPolicy::IsValidEndpoint(
				EffectiveUrl
			)))
	{
		bRedirectRejected = true;
	}
	if (ResponseBodyState)
	{
		ResponseBodyState->Read(
			Result.ResponseBytes,
			Evidence.bResponseTooLarge
		);
	}
	Result.bRedirected = Evidence.bRedirected;
	if (!bProcessedSuccessfully && !bRedirectRejected
		&& !Evidence.bResponseTooLarge)
	{
		switch (Request->GetFailureReason())
		{
		case EHttpFailureReason::TimedOut:
			Evidence.TransportFailure =
				EOpenMobileDeviceEndpointTransportFailure::Timeout;
			break;
		case EHttpFailureReason::Cancelled:
			Evidence.TransportFailure =
				EOpenMobileDeviceEndpointTransportFailure::Cancelled;
			break;
		case EHttpFailureReason::ConnectionError:
			Evidence.TransportFailure =
				EOpenMobileDeviceEndpointTransportFailure::Connection;
			break;
		case EHttpFailureReason::ResponseTooLarge:
			Evidence.bResponseTooLarge = true;
			break;
		case EHttpFailureReason::Other:
			Evidence.TransportFailure =
				EOpenMobileDeviceEndpointTransportFailure::Tls;
			break;
		case EHttpFailureReason::None:
		case EHttpFailureReason::Count:
			Evidence.TransportFailure =
				EOpenMobileDeviceEndpointTransportFailure::Other;
			break;
		}
	}
	if (bRedirectRejected)
	{
		Evidence.TransportFailure =
			EOpenMobileDeviceEndpointTransportFailure::None;
		Evidence.bRedirected = true;
	}
	HttpRequest.Reset();
	ResponseBodyState.Reset();
	CompleteResult(
		FOpenMobileDeviceEndpointReachabilityPolicy::Classify(
			Evidence,
			RequestOptions
		)
	);
}

void UOpenMobileDeviceEndpointReachabilityAsyncAction::CancelNativeOperation()
{
	if (CancellationFlag)
	{
		CancellationFlag->Store(true);
	}
	if (HttpRequest)
	{
		HttpRequest->OnStatusCodeReceived().Unbind();
		HttpRequest->OnProcessRequestComplete().Unbind();
		HttpRequest->CancelRequest();
		HttpRequest.Reset();
	}
	ResponseBodyState.Reset();
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
	if (HttpRequest)
	{
		HttpRequest->OnStatusCodeReceived().Unbind();
		HttpRequest->OnProcessRequestComplete().Unbind();
		HttpRequest->CancelRequest();
		HttpRequest.Reset();
	}
	ResponseBodyState.Reset();
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

#include "OpenMobileDeviceEndpointReachabilityPolicy.h"

namespace OpenMobileDeviceEndpointReachabilityPolicyPrivate
{
	int32 ActiveRequestCount = 0;
}

FOpenMobileEndpointReachabilityOptions
FOpenMobileDeviceEndpointReachabilityPolicy::NormalizeOptions(
	FOpenMobileEndpointReachabilityOptions Options
)
{
	if (!FMath::IsFinite(Options.TimeoutSeconds))
	{
		Options.TimeoutSeconds = 10.0f;
	}
	Options.TimeoutSeconds = FMath::Clamp(Options.TimeoutSeconds, 0.1f, 60.0f);
	if (Options.MinimumAcceptedStatusCode < 100
		|| Options.MinimumAcceptedStatusCode > 599
		|| Options.MaximumAcceptedStatusCode <
			Options.MinimumAcceptedStatusCode
		|| Options.MaximumAcceptedStatusCode > 599)
	{
		Options.MinimumAcceptedStatusCode = 200;
		Options.MaximumAcceptedStatusCode = 299;
	}
	Options.MaximumResponseBytes = FMath::Clamp<int64>(
		Options.MaximumResponseBytes,
		0,
		1024 * 1024
	);
	return Options;
}

bool FOpenMobileDeviceEndpointReachabilityPolicy::IsValidEndpoint(
	const FString& Endpoint
)
{
	FString Trimmed = Endpoint;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed != Endpoint
		|| Trimmed.Len() > 2048
		|| !Trimmed.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	const int32 AuthorityStart = 8;
	int32 AuthorityEnd = Trimmed.Len();
	for (TCHAR Delimiter : {TEXT('/'), TEXT('?'), TEXT('#')})
	{
		const int32 Index = Trimmed.Find(
			FString::Chr(Delimiter),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			AuthorityStart
		);
		if (Index != INDEX_NONE)
		{
			AuthorityEnd = FMath::Min(AuthorityEnd, Index);
		}
	}
	const FString Authority = Trimmed.Mid(
		AuthorityStart,
		AuthorityEnd - AuthorityStart
	);
	return !Authority.IsEmpty()
		&& !Authority.Contains(TEXT("@"))
		&& !Authority.Contains(TEXT(" "))
		&& !Authority.Contains(TEXT("\t"));
}

FString FOpenMobileDeviceEndpointReachabilityPolicy::RedactEndpoint(
	const FString& Endpoint
)
{
	static_cast<void>(Endpoint);
	return TEXT("https://<redacted>");
}

EOpenMobileEndpointReachabilityOutcome
FOpenMobileDeviceEndpointReachabilityPolicy::Classify(
	const FOpenMobileDeviceEndpointCompletionEvidence& Evidence,
	const FOpenMobileEndpointReachabilityOptions& Options
)
{
	if (Evidence.TransportFailure
		== EOpenMobileDeviceEndpointTransportFailure::Cancelled)
	{
		return EOpenMobileEndpointReachabilityOutcome::Cancelled;
	}
	if (Evidence.bResponseTooLarge)
	{
		return EOpenMobileEndpointReachabilityOutcome::ResponseTooLarge;
	}
	if (Evidence.bRedirected && !Options.bAllowRedirects)
	{
		return EOpenMobileEndpointReachabilityOutcome::RedirectRejected;
	}
	switch (Evidence.TransportFailure)
	{
	case EOpenMobileDeviceEndpointTransportFailure::Dns:
		return EOpenMobileEndpointReachabilityOutcome::DnsFailure;
	case EOpenMobileDeviceEndpointTransportFailure::Connection:
		return Evidence.bTcpConnectionSucceeded
			? EOpenMobileEndpointReachabilityOutcome::TlsFailure
			: EOpenMobileEndpointReachabilityOutcome::ConnectionFailure;
	case EOpenMobileDeviceEndpointTransportFailure::Tls:
		return EOpenMobileEndpointReachabilityOutcome::TlsFailure;
	case EOpenMobileDeviceEndpointTransportFailure::Timeout:
		return EOpenMobileEndpointReachabilityOutcome::Timeout;
	case EOpenMobileDeviceEndpointTransportFailure::Other:
		return EOpenMobileEndpointReachabilityOutcome::TransportFailure;
	case EOpenMobileDeviceEndpointTransportFailure::None:
	case EOpenMobileDeviceEndpointTransportFailure::Cancelled:
		break;
	}
	if (!Evidence.bResponseReceived)
	{
		return EOpenMobileEndpointReachabilityOutcome::TransportFailure;
	}
	return Evidence.StatusCode >= Options.MinimumAcceptedStatusCode
		&& Evidence.StatusCode <= Options.MaximumAcceptedStatusCode
		? EOpenMobileEndpointReachabilityOutcome::Succeeded
		: EOpenMobileEndpointReachabilityOutcome::HttpResponseRejected;
}

bool FOpenMobileDeviceEndpointRequestLimiter::TryAcquire(
	int32 MaximumConcurrentRequests
)
{
	using namespace OpenMobileDeviceEndpointReachabilityPolicyPrivate;
	check(IsInGameThread());
	if (MaximumConcurrentRequests <= 0
		|| ActiveRequestCount >= MaximumConcurrentRequests)
	{
		return false;
	}
	++ActiveRequestCount;
	return true;
}

void FOpenMobileDeviceEndpointRequestLimiter::Release()
{
	using namespace OpenMobileDeviceEndpointReachabilityPolicyPrivate;
	check(IsInGameThread());
	ActiveRequestCount = FMath::Max(ActiveRequestCount - 1, 0);
}

int32 FOpenMobileDeviceEndpointRequestLimiter::GetActiveRequestCount()
{
	return OpenMobileDeviceEndpointReachabilityPolicyPrivate::ActiveRequestCount;
}

void FOpenMobileDeviceEndpointRequestLimiter::ResetForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceEndpointReachabilityPolicyPrivate::ActiveRequestCount = 0;
}

double FOpenMobileDeviceEndpointReachabilityPolicy::GetConnectAttemptDeadline(
	double NowSeconds, double DeadlineSeconds, int32 RemainingAddresses)
{
	return NowSeconds + FMath::Max(0.0, DeadlineSeconds - NowSeconds)
		/ FMath::Max(1, RemainingAddresses);
}

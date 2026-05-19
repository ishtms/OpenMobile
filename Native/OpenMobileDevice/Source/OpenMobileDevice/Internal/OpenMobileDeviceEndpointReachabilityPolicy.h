#pragma once

#include "OpenMobileDeviceEndpointReachabilityTypes.h"

enum class EOpenMobileDeviceEndpointTransportFailure : uint8
{
	None,
	Dns,
	Connection,
	Tls,
	Timeout,
	Cancelled,
	Other
};

struct FOpenMobileDeviceEndpointCompletionEvidence
{
	EOpenMobileDeviceEndpointTransportFailure TransportFailure =
		EOpenMobileDeviceEndpointTransportFailure::None;
	bool bResponseReceived = false;
	int32 StatusCode = 0;
	bool bRedirected = false;
	bool bResponseTooLarge = false;
	bool bTcpConnectionSucceeded = false;
};

class FOpenMobileDeviceEndpointReachabilityPolicy final
{
public:
	static FOpenMobileEndpointReachabilityOptions NormalizeOptions(
		FOpenMobileEndpointReachabilityOptions Options
	);
	static bool IsValidEndpoint(const FString& Endpoint);
	static FString RedactEndpoint(const FString& Endpoint);
	static EOpenMobileEndpointReachabilityOutcome Classify(
		const FOpenMobileDeviceEndpointCompletionEvidence& Evidence,
		const FOpenMobileEndpointReachabilityOptions& Options
	);
};

class FOpenMobileDeviceEndpointRequestLimiter final
{
public:
	static bool TryAcquire(int32 MaximumConcurrentRequests);
	static void Release();
	static int32 GetActiveRequestCount();
	static void ResetForTests();
};

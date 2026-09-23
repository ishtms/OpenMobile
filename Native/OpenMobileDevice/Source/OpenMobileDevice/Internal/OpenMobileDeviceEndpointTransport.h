#pragma once

#include "OpenMobileDeviceEndpointReachabilityPolicy.h"

struct FOpenMobileDeviceEndpointResponse
{
	FOpenMobileDeviceEndpointCompletionEvidence Evidence;
	int64 ResponseBytes = 0;
};

// A bounded, cookie-free HTTPS probe. Redirect policy is enforced before following.
class OPENMOBILEDEVICE_API FOpenMobileDeviceEndpointTransport final
{
public:
	using FQuery = FOpenMobileDeviceEndpointResponse (*)(const FString&,
		const FOpenMobileEndpointReachabilityOptions&, const TAtomic<bool>&);
	using FCompletion = TFunction<void(FOpenMobileDeviceEndpointResponse)>;
	static void Start(const FString& Endpoint, FOpenMobileEndpointReachabilityOptions Options,
		TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> Cancellation, FCompletion Completion);
	// Platform modules register a synchronous worker implementation; removal drains it.
	static void SetPlatformQuery(FQuery Query);
	static void Shutdown();
};

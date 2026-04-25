#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.h"

class OPENMOBILEADS_API FOpenMobileAdsTrackingAuthorizationPlatform
{
public:
	using FCompletion =
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>;

	static bool IsAvailable();
	static EOpenMobileAdsTrackingAuthorizationStatus GetStatus();
	static bool IsAdvertisingIdentifierAvailable();
	static bool RequestAuthorization(
		FCompletion&& Completion,
		FString& OutError
	);
};

OPENMOBILEADS_API EOpenMobileAdsTrackingAuthorizationStatus
OpenMobileAdsMapAppleTrackingAuthorizationStatus(int64 RawStatus);

OPENMOBILEADS_API bool OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
	const uint8* IdentifierBytes,
	int32 ByteCount
);

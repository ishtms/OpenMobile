#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.h"

class OPENMOBILEADS_API FOpenMobileAdsTrackingAuthorizationPlatform
{
public:
	using FCompletion =
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>;

	/** Reports whether any registered backend can answer tracking authorization on this runtime. */
	static bool IsAvailable();
	/** Reads the highest-priority backend status without opening a system prompt. */
	static EOpenMobileAdsTrackingAuthorizationStatus GetStatus();
	/** Requires both authorized status and a nonzero platform advertising identifier. */
	static bool IsAdvertisingIdentifierAvailable();
	/** Forwards one prompt request to the selected backend and keeps completion on the game thread. */
	static bool RequestAuthorization(
		FCompletion&& Completion,
		FString& OutError
	);
};

/** Converts Apple's raw authorization value without leaking platform enums into the core module. */
OPENMOBILEADS_API EOpenMobileAdsTrackingAuthorizationStatus
OpenMobileAdsMapAppleTrackingAuthorizationStatus(int64 RawStatus);

/** Rejects an absent, short, or all-zero Apple advertising identifier. */
OPENMOBILEADS_API bool OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
	const uint8* IdentifierBytes,
	int32 ByteCount
);

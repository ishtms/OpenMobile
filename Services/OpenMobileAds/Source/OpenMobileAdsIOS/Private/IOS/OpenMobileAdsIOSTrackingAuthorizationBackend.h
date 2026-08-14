#pragma once

#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

class FOpenMobileAdsIOSTrackingAuthorizationBackend final
	: public IOpenMobileAdsTrackingAuthorizationBackend
{
public:
	/** Keeps the backend identity stable for deterministic platform selection. */
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	/** Checks framework and OS availability without touching the tracking prompt. */
	virtual bool IsAvailable() const override;
	/** Maps Apple's current authorization value to the provider-neutral status. */
	virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const override;
	/** Reads identifier bytes only after the platform framework is available. */
	virtual bool HasNonZeroAdvertisingIdentifier() const override;
	/** Opens Apple's prompt on the main queue and reports the normalized result once. */
	virtual bool RequestAuthorization(
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& Completion,
		FString& OutError
	) override;
};

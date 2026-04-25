#pragma once

#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

class FOpenMobileAdsIOSTrackingAuthorizationBackend final
	: public IOpenMobileAdsTrackingAuthorizationBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual bool IsAvailable() const override;
	virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const override;
	virtual bool HasNonZeroAdvertisingIdentifier() const override;
	virtual bool RequestAuthorization(
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& Completion,
		FString& OutError
	) override;
};

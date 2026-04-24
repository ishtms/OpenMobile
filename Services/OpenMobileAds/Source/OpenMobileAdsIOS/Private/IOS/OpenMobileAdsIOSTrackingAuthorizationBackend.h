#pragma once

#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

class FOpenMobileAdsIOSTrackingAuthorizationBackend final
	: public IOpenMobileAdsTrackingAuthorizationBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual bool IsAvailable() const override;
	virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const override;
};

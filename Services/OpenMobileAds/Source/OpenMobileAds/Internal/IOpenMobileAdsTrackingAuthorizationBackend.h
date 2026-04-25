#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileAdsPrivacy.h"

class IOpenMobileAdsTrackingAuthorizationBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsTrackingAuthorizationBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.TrackingAuthorization.Backend")
		);
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const = 0;
	virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const = 0;
	virtual bool HasNonZeroAdvertisingIdentifier() const = 0;
	virtual bool RequestAuthorization(
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& Completion,
		FString& OutError
	) = 0;
};

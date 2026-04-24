#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.h"

class OPENMOBILEADS_API FOpenMobileAdsTrackingAuthorizationPlatform
{
public:
	static EOpenMobileAdsTrackingAuthorizationStatus GetStatus();
};

OPENMOBILEADS_API EOpenMobileAdsTrackingAuthorizationStatus
OpenMobileAdsMapAppleTrackingAuthorizationStatus(int64 RawStatus);

#include "OpenMobileAdsIOSTrackingAuthorizationBackend.h"

#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#import <AppTrackingTransparency/AppTrackingTransparency.h>

bool FOpenMobileAdsIOSTrackingAuthorizationBackend::IsAvailable() const
{
	if (@available(iOS 14.0, *))
	{
		return true;
	}
	return false;
}

EOpenMobileAdsTrackingAuthorizationStatus
FOpenMobileAdsIOSTrackingAuthorizationBackend::GetStatus() const
{
	if (@available(iOS 14.0, *))
	{
		return OpenMobileAdsMapAppleTrackingAuthorizationStatus(
			static_cast<int64>([ATTrackingManager trackingAuthorizationStatus])
		);
	}
	return EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
}

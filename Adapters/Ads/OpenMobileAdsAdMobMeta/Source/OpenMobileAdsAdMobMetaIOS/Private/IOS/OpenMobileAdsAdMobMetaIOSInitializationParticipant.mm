#include "IOS/OpenMobileAdsAdMobMetaIOSInitializationParticipant.h"

#include "OpenMobileAdsOperations.h"

#import <FBAudienceNetwork/FBAdSettings.h>
#import <Foundation/Foundation.h>

bool FOpenMobileAdsAdMobMetaIOSInitializationParticipant::
	PrepareForInitialization(
		const FOpenMobileAdsInitializationRequest& Request,
		FOpenMobileAdsError& OutError
	)
{
	(void)OutError;
	if (@available(iOS 17.0, *))
	{
		return true;
	}

	const BOOL bAdvertiserTrackingEnabled =
		Request.TrackingAuthorizationStatus
			== EOpenMobileAdsTrackingAuthorizationStatus::Authorized;
	void (^ApplyTrackingStatus)(void) = ^{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		[FBAdSettings setAdvertiserTrackingEnabled:bAdvertiserTrackingEnabled];
#pragma clang diagnostic pop
	};
	if (NSThread.isMainThread)
	{
		ApplyTrackingStatus();
	}
	else
	{
		dispatch_sync(dispatch_get_main_queue(), ApplyTrackingStatus);
	}
	return true;
}

#include "OpenMobileAdsIOSTrackingAuthorizationBackend.h"

#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#import <AppTrackingTransparency/AppTrackingTransparency.h>
#import <UIKit/UIKit.h>

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

bool FOpenMobileAdsIOSTrackingAuthorizationBackend::RequestAuthorization(
	TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& Completion,
	FString& OutError
)
{
	if (!IsAvailable())
	{
		OutError = TEXT("App Tracking Transparency requires iOS 14 or newer.");
		return false;
	}
	NSString* UsageDescription = [[NSBundle mainBundle]
		objectForInfoDictionaryKey:@"NSUserTrackingUsageDescription"];
	if (
		![UsageDescription isKindOfClass:[NSString class]]
		|| [UsageDescription stringByTrimmingCharactersInSet:
			[NSCharacterSet whitespaceAndNewlineCharacterSet]].length == 0
	)
	{
		OutError = TEXT("NSUserTrackingUsageDescription is missing from the application plist.");
		return false;
	}
	if ([UIApplication sharedApplication].applicationState != UIApplicationStateActive)
	{
		OutError = TEXT("The application must be active before requesting tracking authorization.");
		return false;
	}
	TSharedRef<
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>,
		ESPMode::ThreadSafe
	> SharedCompletion = MakeShared<
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>,
		ESPMode::ThreadSafe
	>(MoveTemp(Completion));
	dispatch_block_t RequestBlock = ^{
		if (
			[UIApplication sharedApplication].applicationState
				!= UIApplicationStateActive
		)
		{
			(*SharedCompletion)(GetStatus());
			return;
		}
		[ATTrackingManager requestTrackingAuthorizationWithCompletionHandler:
			^(ATTrackingManagerAuthorizationStatus Status)
			{
				(*SharedCompletion)(
					OpenMobileAdsMapAppleTrackingAuthorizationStatus(
						static_cast<int64>(Status)
					)
				);
			}];
	};
	if ([NSThread isMainThread])
	{
		RequestBlock();
	}
	else
	{
		dispatch_async(dispatch_get_main_queue(), RequestBlock);
	}
	return true;
}

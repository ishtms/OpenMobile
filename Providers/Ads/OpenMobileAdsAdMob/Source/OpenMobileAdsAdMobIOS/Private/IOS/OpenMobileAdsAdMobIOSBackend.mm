#include "OpenMobileAdsAdMobIOSBackend.h"

#include "OpenMobileAdsAdMobPlatform.h"

#if PLATFORM_IOS

#import <GoogleMobileAds/GoogleMobileAds.h>
#import <UIKit/UIKit.h>

#import "IOS/IOSAppDelegate.h"

namespace OpenMobileAdsAdMobIOS
{
	UIViewController* TopViewController(UIViewController* Controller)
	{
		if (!Controller)
		{
			return nil;
		}

		if (Controller.presentedViewController && !Controller.presentedViewController.isBeingDismissed)
		{
			return TopViewController(Controller.presentedViewController);
		}

		if ([Controller isKindOfClass:[UINavigationController class]])
		{
			return TopViewController(((UINavigationController*)Controller).visibleViewController);
		}

		if ([Controller isKindOfClass:[UITabBarController class]])
		{
			return TopViewController(((UITabBarController*)Controller).selectedViewController);
		}

		return Controller;
	}

	FString ToFString(NSString* String)
	{
		return String ? FString(UTF8_TO_TCHAR(String.UTF8String)) : FString();
	}
}

@interface OpenMobileRewardedAdDelegate : NSObject <GADFullScreenContentDelegate>

@property(nonatomic, assign) int64_t requestId;
@property(nonatomic, strong, nullable) GADRewardedAd* rewardedAd;

@end

static OpenMobileRewardedAdDelegate* GOpenMobileRewardedAdDelegate = nil;

@implementation OpenMobileRewardedAdDelegate

- (void)adWillPresentFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	FOpenMobileAdsAdMobPlatform::NativeShown(self.requestId);
}

- (void)ad:(id<GADFullScreenPresentingAd>)ad
	didFailToPresentFullScreenContentWithError:(NSError*)error
{
	const int64_t failedRequestId = self.requestId;
	self.rewardedAd = nil;
	if (GOpenMobileRewardedAdDelegate == self)
	{
		GOpenMobileRewardedAdDelegate = nil;
	}

	NSString* detail = error.localizedDescription ?: @"Unknown presentation error.";
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		failedRequestId,
		OpenMobileAdsAdMobIOS::ToFString(
			[@"Rewarded ad failed to show: " stringByAppendingString:detail]
		)
	);
}

- (void)adDidDismissFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	const int64_t closedRequestId = self.requestId;
	self.rewardedAd = nil;
	if (GOpenMobileRewardedAdDelegate == self)
	{
		GOpenMobileRewardedAdDelegate = nil;
	}

	FOpenMobileAdsAdMobPlatform::NativeClosed(closedRequestId);
}

@end

void FOpenMobileAdsAdMobIOSBackend::Initialize()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[GADMobileAds.sharedInstance startWithCompletionHandler:nil];
	});
}

void FOpenMobileAdsAdMobIOSBackend::Shutdown()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		GOpenMobileRewardedAdDelegate.rewardedAd.fullScreenContentDelegate = nil;
		GOpenMobileRewardedAdDelegate.rewardedAd = nil;
		GOpenMobileRewardedAdDelegate = nil;
	});
}

bool FOpenMobileAdsAdMobIOSBackend::LaunchRewardedAd(
	const FString& AdUnitId,
	const int64 RequestId,
	FString& OutError
)
{
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The iOS rewarded ad unit ID is empty.");
		return false;
	}

	NSString* IOSAdUnitId = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*AdUnitId)];
	if (!IOSAdUnitId)
	{
		OutError = TEXT("The iOS rewarded ad unit ID could not be encoded.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (GOpenMobileRewardedAdDelegate != nil)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				RequestId,
				TEXT("An iOS rewarded ad is already loading or showing.")
			);
			return;
		}

		OpenMobileRewardedAdDelegate* Handler = [[OpenMobileRewardedAdDelegate alloc] init];
		Handler.requestId = RequestId;
		GOpenMobileRewardedAdDelegate = Handler;

		[GADRewardedAd loadWithAdUnitID:IOSAdUnitId
							   request:[GADRequest request]
					 completionHandler:^(GADRewardedAd* rewardedAd, NSError* error)
		{
			if (GOpenMobileRewardedAdDelegate != Handler || Handler.requestId != RequestId)
			{
				return;
			}

			if (error || !rewardedAd)
			{
				GOpenMobileRewardedAdDelegate = nil;
				NSString* detail = error.localizedDescription ?: @"No rewarded ad was returned.";
				FOpenMobileAdsAdMobPlatform::NativeFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"Rewarded ad failed to load: " stringByAppendingString:detail]
					)
				);
				return;
			}

			Handler.rewardedAd = rewardedAd;
			rewardedAd.fullScreenContentDelegate = Handler;
			FOpenMobileAdsAdMobPlatform::NativeLoaded(RequestId);

			UIViewController* rootController = OpenMobileAdsAdMobIOS::TopViewController(
				(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
			);
			NSError* presentationError = nil;
			if (![rewardedAd canPresentFromRootViewController:rootController error:&presentationError])
			{
				Handler.rewardedAd = nil;
				GOpenMobileRewardedAdDelegate = nil;
				NSString* detail = presentationError.localizedDescription ?: @"No presenter is available.";
				FOpenMobileAdsAdMobPlatform::NativeFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"Rewarded ad could not be presented: " stringByAppendingString:detail]
					)
				);
				return;
			}

			__weak OpenMobileRewardedAdDelegate* weakHandler = Handler;
			[rewardedAd presentFromRootViewController:rootController
							 userDidEarnRewardHandler:^
			{
				OpenMobileRewardedAdDelegate* strongHandler = weakHandler;
				if (!strongHandler || GOpenMobileRewardedAdDelegate != strongHandler)
				{
					return;
				}

				GADAdReward* reward = strongHandler.rewardedAd.adReward;
				FOpenMobileAdsAdMobPlatform::NativeEarned(
					RequestId,
					reward.amount.intValue,
					OpenMobileAdsAdMobIOS::ToFString(reward.type)
				);
			}];
		}];
	});

	return true;
}

#endif

#include "OpenMobileAdsAdMobIOSBackend.h"

#include "OpenMobileAdsAdMobPlatform.h"

#if PLATFORM_IOS

#include "Apple/AppleStringUtils.h"

#import <GoogleMobileAds/GoogleMobileAds.h>
#import <UserMessagingPlatform/UserMessagingPlatform.h>
#import <UIKit/UIKit.h>

#import "IOS/IOSAppDelegate.h"

namespace OpenMobileAdsAdMobIOS
{
	NSNumber* ToNSNumber(EOpenMobileAdsAgeTreatment Treatment)
	{
		switch (Treatment)
		{
		case EOpenMobileAdsAgeTreatment::No:
			return @NO;
		case EOpenMobileAdsAgeTreatment::Yes:
			return @YES;
		default:
			return nil;
		}
	}

	GADMaxAdContentRating ToMaxAdContentRating(EOpenMobileAdsMaxAdContentRating Rating)
	{
		switch (Rating)
		{
		case EOpenMobileAdsMaxAdContentRating::General:
			return GADMaxAdContentRatingGeneral;
		case EOpenMobileAdsMaxAdContentRating::ParentalGuidance:
			return GADMaxAdContentRatingParentalGuidance;
		case EOpenMobileAdsMaxAdContentRating::Teen:
			return GADMaxAdContentRatingTeen;
		case EOpenMobileAdsMaxAdContentRating::Mature:
			return GADMaxAdContentRatingMatureAudience;
		default:
			return nil;
		}
	}

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

	int32 ToCanonicalConsentStatus(UMPConsentStatus Status)
	{
		switch (Status)
		{
		case UMPConsentStatusNotRequired:
			return 1;
		case UMPConsentStatusRequired:
			return 2;
		case UMPConsentStatusObtained:
			return 3;
		default:
			return 0;
		}
	}

	int32 ToCanonicalPrivacyOptionsRequirement(
		UMPPrivacyOptionsRequirementStatus Requirement
	)
	{
		switch (Requirement)
		{
		case UMPPrivacyOptionsRequirementStatusNotRequired:
			return 1;
		case UMPPrivacyOptionsRequirementStatusRequired:
			return 2;
		default:
			return 0;
		}
	}

	FString ToUMPErrorCode(NSError* Error, bool bFormOperation)
	{
		if (!Error)
		{
			return TEXT("ump_internal");
		}
		if (bFormOperation)
		{
			switch (static_cast<UMPFormErrorCode>(Error.code))
			{
			case UMPFormErrorCodeAlreadyUsed:
			case UMPFormErrorCodeInvalidViewController:
				return TEXT("ump_invalid_operation");
			case UMPFormErrorCodeUnavailable:
				return TEXT("form_unavailable");
			case UMPFormErrorCodeTimeout:
				return TEXT("ump_timeout");
			default:
				return TEXT("ump_internal");
			}
		}
		switch (static_cast<UMPRequestErrorCode>(Error.code))
		{
		case UMPRequestErrorCodeInvalidAppID:
		case UMPRequestErrorCodeMisconfiguration:
			return TEXT("ump_configuration");
		case UMPRequestErrorCodeNetwork:
			return TEXT("ump_network");
		default:
			return TEXT("ump_internal");
		}
	}

	void CompleteConsentInfo(int64 RequestId, bool bFormDismissed)
	{
		UMPConsentInformation* ConsentInformation =
			UMPConsentInformation.sharedInstance;
		const int32 ConsentStatus = ToCanonicalConsentStatus(
			ConsentInformation.consentStatus
		);
		const int32 PrivacyOptionsRequirement =
			ToCanonicalPrivacyOptionsRequirement(
				ConsentInformation.privacyOptionsRequirementStatus
			);
		if (bFormDismissed)
		{
			FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
				RequestId,
				ConsentStatus,
				ConsentInformation.canRequestAds,
				PrivacyOptionsRequirement
			);
			return;
		}
		FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
			RequestId,
			ConsentStatus,
			ConsentInformation.canRequestAds,
			PrivacyOptionsRequirement
		);
	}
}

@interface OpenMobileRewardedAdDelegate : NSObject <GADFullScreenContentDelegate>

@property(nonatomic, assign) int64_t requestId;
@property(nonatomic, strong, nullable) GADRewardedAd* rewardedAd;

@end

static OpenMobileRewardedAdDelegate* GOpenMobileRewardedAdDelegate = nil;
static NSMutableDictionary<NSNumber*, GADRewardedAd*>* GOpenMobileLoadedRewardedAds = nil;
static NSMutableSet<NSNumber*>* GOpenMobileRewardedAdLoadRequests = nil;

@implementation OpenMobileRewardedAdDelegate

- (void)adWillPresentFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	FOpenMobileAdsAdMobPlatform::NativeShown(self.requestId);
}

- (void)adDidRecordImpression:(id<GADFullScreenPresentingAd>)ad
{
	FOpenMobileAdsAdMobPlatform::NativeImpression(self.requestId);
}

- (void)adDidRecordClick:(id<GADFullScreenPresentingAd>)ad
{
	FOpenMobileAdsAdMobPlatform::NativeClicked(self.requestId);
}

- (void)ad:(id<GADFullScreenPresentingAd>)ad
	didFailToPresentFullScreenContentWithError:(NSError*)error
{
	const int64_t failedRequestId = self.requestId;
	self.rewardedAd.paidEventHandler = nil;
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
	self.rewardedAd.paidEventHandler = nil;
	self.rewardedAd = nil;
	if (GOpenMobileRewardedAdDelegate == self)
	{
		GOpenMobileRewardedAdDelegate = nil;
	}

	FOpenMobileAdsAdMobPlatform::NativeClosed(closedRequestId);
}

@end

bool FOpenMobileAdsAdMobIOSBackend::Initialize(
	const FOpenMobileAdsInitializationRequest& Request,
	const int64 RequestId,
	FString& OutError
)
{
	const EOpenMobileAdsAgeTreatment ChildDirectedTreatment =
		Request.Privacy.ChildDirectedTreatment;
	const EOpenMobileAdsAgeTreatment UnderAgeOfConsent =
		Request.Privacy.UnderAgeOfConsent;
	const EOpenMobileAdsMaxAdContentRating MaxAdContentRating =
		Request.RequestConfiguration.MaxAdContentRating;
	NSMutableArray<NSString*>* TestDeviceIdentifiers = [NSMutableArray
		arrayWithCapacity:Request.Development.TestDeviceIdentifiers.Num()];
	for (const FString& Identifier : Request.Development.TestDeviceIdentifiers)
	{
		[TestDeviceIdentifiers addObject:FAppleStringUtils::ConvertToNSString(Identifier)];
	}
	dispatch_async(dispatch_get_main_queue(), ^
	{
		GADRequestConfiguration* Configuration =
			GADMobileAds.sharedInstance.requestConfiguration;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		Configuration.tagForChildDirectedTreatment =
			OpenMobileAdsAdMobIOS::ToNSNumber(ChildDirectedTreatment);
		Configuration.tagForUnderAgeOfConsent =
			OpenMobileAdsAdMobIOS::ToNSNumber(UnderAgeOfConsent);
#pragma clang diagnostic pop
		Configuration.maxAdContentRating =
			OpenMobileAdsAdMobIOS::ToMaxAdContentRating(MaxAdContentRating);
		Configuration.testDeviceIdentifiers = TestDeviceIdentifiers.count > 0
			? TestDeviceIdentifiers
			: nil;
		[GADMobileAds.sharedInstance startWithCompletionHandler:^(GADInitializationStatus* Status)
		{
			for (NSString* AdapterName in Status.adapterStatusesByClassName)
			{
				GADAdapterStatus* AdapterStatus =
					Status.adapterStatusesByClassName[AdapterName];
				FOpenMobileAdsAdMobPlatform::NativeAdapterInitializationStatus(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(AdapterName),
					AdapterStatus.state == GADAdapterInitializationStateReady,
					AdapterStatus.latency * 1000.0,
					OpenMobileAdsAdMobIOS::ToFString(AdapterStatus.description)
				);
			}
			FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(RequestId);
		}];
	});
	return true;
}

void FOpenMobileAdsAdMobIOSBackend::Shutdown()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		GOpenMobileRewardedAdDelegate.rewardedAd.fullScreenContentDelegate = nil;
		GOpenMobileRewardedAdDelegate.rewardedAd = nil;
		GOpenMobileRewardedAdDelegate = nil;
		[GOpenMobileLoadedRewardedAds removeAllObjects];
		[GOpenMobileRewardedAdLoadRequests removeAllObjects];
		GOpenMobileLoadedRewardedAds = nil;
		GOpenMobileRewardedAdLoadRequests = nil;
	});
}

bool FOpenMobileAdsAdMobIOSBackend::RequestConsentInfo(
	const FOpenMobileAdsConsentRequest& Request,
	const int64 RequestId,
	FString& OutError
)
{
	if (RequestId <= 0)
	{
		OutError = TEXT("The iOS Google UMP request ID is invalid.");
		return false;
	}

	const bool bUnderAgeOfConsent =
		Request.Privacy.UnderAgeOfConsent == EOpenMobileAdsAgeTreatment::Yes;
	const bool bEnableConsentDebug =
		Request.Development.bEnableConsentDebug && !bUnderAgeOfConsent;
	NSMutableArray<NSString*>* TestDeviceIdentifiers = [NSMutableArray
		arrayWithCapacity:bEnableConsentDebug
			? Request.Development.TestDeviceIdentifiers.Num()
			: 0];
	if (bEnableConsentDebug)
	{
		for (const FString& Identifier : Request.Development.TestDeviceIdentifiers)
		{
			[TestDeviceIdentifiers addObject:FAppleStringUtils::ConvertToNSString(Identifier)];
		}
	}
	dispatch_async(dispatch_get_main_queue(), ^
	{
		UMPRequestParameters* Parameters = [[UMPRequestParameters alloc] init];
		Parameters.tagForUnderAgeOfConsent = bUnderAgeOfConsent;
		if (bEnableConsentDebug && TestDeviceIdentifiers.count > 0)
		{
			UMPDebugSettings* DebugSettings = [[UMPDebugSettings alloc] init];
			DebugSettings.testDeviceIdentifiers = TestDeviceIdentifiers;
			Parameters.debugSettings = DebugSettings;
		}
		[UMPConsentInformation.sharedInstance
			requestConsentInfoUpdateWithParameters:Parameters
			completionHandler:^(NSError* Error)
		{
			if (Error)
			{
				FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToUMPErrorCode(Error, false),
					OpenMobileAdsAdMobIOS::ToFString(
						Error.localizedDescription ?: @"Unknown UMP request error."
					)
				);
				return;
			}
			OpenMobileAdsAdMobIOS::CompleteConsentInfo(RequestId, false);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::PresentRequiredConsentForm(
	const int64 RequestId,
	FString& OutError
)
{
	if (RequestId <= 0)
	{
		OutError = TEXT("The iOS Google UMP form request ID is invalid.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		[UMPConsentForm
			loadAndPresentIfRequiredFromViewController:RootController
			completionHandler:^(NSError* Error)
		{
			if (Error)
			{
				FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToUMPErrorCode(Error, true),
					OpenMobileAdsAdMobIOS::ToFString(
						Error.localizedDescription ?: @"Unknown UMP form error."
					)
				);
				return;
			}
			OpenMobileAdsAdMobIOS::CompleteConsentInfo(RequestId, true);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::LoadRewardedAd(
	const FString& AdUnitId,
	const int64 RequestId,
	const EOpenMobileAdsDataProcessingMode DataProcessingMode,
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
		if (!GOpenMobileLoadedRewardedAds)
		{
			GOpenMobileLoadedRewardedAds = [[NSMutableDictionary alloc] init];
		}
		if (!GOpenMobileRewardedAdLoadRequests)
		{
			GOpenMobileRewardedAdLoadRequests = [[NSMutableSet alloc] init];
		}
		NSNumber* Key = @(RequestId);
		if (
			[GOpenMobileRewardedAdLoadRequests containsObject:Key]
			|| GOpenMobileLoadedRewardedAds[Key] != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
				RequestId,
				TEXT("The iOS rewarded load request is already active.")
			);
			return;
		}

		[GOpenMobileRewardedAdLoadRequests addObject:Key];
		if (DataProcessingMode == EOpenMobileAdsDataProcessingMode::Restricted)
		{
			[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"gad_rdp"];
		}
		else if (DataProcessingMode == EOpenMobileAdsDataProcessingMode::Standard)
		{
			[NSUserDefaults.standardUserDefaults removeObjectForKey:@"gad_rdp"];
		}
		[GADRewardedAd loadWithAdUnitID:IOSAdUnitId
						   request:[GADRequest request]
					completionHandler:^(GADRewardedAd* RewardedAd, NSError* Error)
		{
			if (![GOpenMobileRewardedAdLoadRequests containsObject:Key])
			{
				return;
			}
			[GOpenMobileRewardedAdLoadRequests removeObject:Key];
			if (Error || !RewardedAd)
			{
				NSString* Detail = Error.localizedDescription ?: @"No rewarded ad was returned.";
				FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"Rewarded ad failed to load: " stringByAppendingString:Detail]
					)
				);
				return;
			}

			GOpenMobileLoadedRewardedAds[Key] = RewardedAd;
			FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(RequestId);
		}];
	});
	return true;
}

void FOpenMobileAdsAdMobIOSBackend::CancelRewardedAd(const int64 RequestId)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		NSNumber* Key = @(RequestId);
		[GOpenMobileRewardedAdLoadRequests removeObject:Key];
		[GOpenMobileLoadedRewardedAds removeObjectForKey:Key];
	});
}

bool FOpenMobileAdsAdMobIOSBackend::ShowRewardedAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FString& ServerVerificationCustomData,
	FString& OutError
)
{
	const FString VerificationData = ServerVerificationCustomData;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (GOpenMobileRewardedAdDelegate != nil)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("An iOS rewarded ad is already loading or showing.")
			);
			return;
		}

		NSNumber* Key = @(LoadedRequestId);
		GADRewardedAd* RewardedAd = GOpenMobileLoadedRewardedAds[Key];
		[GOpenMobileLoadedRewardedAds removeObjectForKey:Key];
		if (!RewardedAd)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("The cached iOS rewarded ad is unavailable or already consumed.")
			);
			return;
		}

		OpenMobileRewardedAdDelegate* Handler =
			[[OpenMobileRewardedAdDelegate alloc] init];
		Handler.requestId = ShowRequestId;
		Handler.rewardedAd = RewardedAd;
		GOpenMobileRewardedAdDelegate = Handler;
		RewardedAd.fullScreenContentDelegate = Handler;
		if (!VerificationData.IsEmpty())
		{
			GADServerSideVerificationOptions* Options =
				[[GADServerSideVerificationOptions alloc] init];
			Options.customRewardString =
				FAppleStringUtils::ConvertToNSString(VerificationData);
			RewardedAd.serverSideVerificationOptions = Options;
		}
		RewardedAd.paidEventHandler = ^(GADAdValue* AdValue)
		{
			FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
				ShowRequestId,
				AdValue.value.longLongValue,
				OpenMobileAdsAdMobIOS::ToFString(AdValue.currencyCode),
				static_cast<int32>(AdValue.precision)
			);
		};

		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		NSError* PresentationError = nil;
		if (![RewardedAd canPresentFromRootViewController:RootController error:&PresentationError])
		{
			RewardedAd.paidEventHandler = nil;
			Handler.rewardedAd = nil;
			GOpenMobileRewardedAdDelegate = nil;
			NSString* Detail = PresentationError.localizedDescription
				?: @"No presenter is available.";
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					[@"Rewarded ad could not be presented: " stringByAppendingString:Detail]
				)
			);
			return;
		}

		__weak OpenMobileRewardedAdDelegate* WeakHandler = Handler;
		[RewardedAd presentFromRootViewController:RootController
						 userDidEarnRewardHandler:^
		{
			OpenMobileRewardedAdDelegate* StrongHandler = WeakHandler;
			if (!StrongHandler || GOpenMobileRewardedAdDelegate != StrongHandler)
			{
				return;
			}
			GADAdReward* Reward = StrongHandler.rewardedAd.adReward;
			FOpenMobileAdsAdMobPlatform::NativeEarned(
				ShowRequestId,
				Reward.amount.intValue,
				OpenMobileAdsAdMobIOS::ToFString(Reward.type)
			);
		}];
	});
	return true;
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

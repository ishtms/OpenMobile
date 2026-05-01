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

	UMPDebugGeography ToDebugGeography(EOpenMobileAdsDebugGeography Geography)
	{
		switch (Geography)
		{
		case EOpenMobileAdsDebugGeography::Eea:
			return UMPDebugGeographyEEA;
		case EOpenMobileAdsDebugGeography::RegulatedUsState:
			return UMPDebugGeographyRegulatedUSState;
		case EOpenMobileAdsDebugGeography::Other:
			return UMPDebugGeographyOther;
		default:
			return UMPDebugGeographyDisabled;
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

	void ApplyDataProcessingMode(EOpenMobileAdsDataProcessingMode Mode)
	{
		if (Mode == EOpenMobileAdsDataProcessingMode::Restricted)
		{
			[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"gad_rdp"];
		}
		else if (Mode == EOpenMobileAdsDataProcessingMode::Standard)
		{
			[NSUserDefaults.standardUserDefaults removeObjectForKey:@"gad_rdp"];
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

@interface OpenMobileRewardedInterstitialAdDelegate : NSObject <GADFullScreenContentDelegate>

@property(nonatomic, assign) int64_t requestId;
@property(nonatomic, strong, nullable) GADRewardedInterstitialAd* rewardedInterstitialAd;

@end

static OpenMobileRewardedInterstitialAdDelegate*
	GOpenMobileRewardedInterstitialAdDelegate = nil;
static NSMutableDictionary<NSNumber*, GADRewardedInterstitialAd*>*
	GOpenMobileLoadedRewardedInterstitialAds = nil;
static NSMutableSet<NSNumber*>* GOpenMobileRewardedInterstitialAdLoadRequests = nil;

@interface OpenMobileInterstitialAdDelegate : NSObject <GADFullScreenContentDelegate>

@property(nonatomic, assign) int64_t requestId;
@property(nonatomic, strong, nullable) GADInterstitialAd* interstitialAd;

@end

static OpenMobileInterstitialAdDelegate* GOpenMobileInterstitialAdDelegate = nil;
static NSMutableDictionary<NSNumber*, GADInterstitialAd*>*
	GOpenMobileLoadedInterstitialAds = nil;
static NSMutableSet<NSNumber*>* GOpenMobileInterstitialAdLoadRequests = nil;

@interface OpenMobileAppOpenAdDelegate : NSObject <GADFullScreenContentDelegate>

@property(nonatomic, assign) int64_t requestId;
@property(nonatomic, strong, nullable) GADAppOpenAd* appOpenAd;

@end

static OpenMobileAppOpenAdDelegate* GOpenMobileAppOpenAdDelegate = nil;
static NSMutableDictionary<NSNumber*, GADAppOpenAd*>*
	GOpenMobileLoadedAppOpenAds = nil;
static NSMutableSet<NSNumber*>* GOpenMobileAppOpenAdLoadRequests = nil;

@interface OpenMobileBannerAdDelegate : NSObject <GADBannerViewDelegate>

@property(nonatomic, assign) int64_t loadRequestId;
@property(nonatomic, assign) int64_t showRequestId;
@property(nonatomic, strong, nullable) GADBannerView* bannerView;
@property(nonatomic, weak, nullable) UIView* hostView;
@property(nonatomic, strong, nullable) UILayoutGuide* layoutGuide;
@property(nonatomic, strong, nullable) NSArray<NSLayoutConstraint*>* constraints;
@property(nonatomic, copy, nullable) NSString* adUnitId;
@property(nonatomic, assign) BOOL anchoredAdaptive;
@property(nonatomic, assign) BOOL mediumRectangle;
@property(nonatomic, assign) NSInteger anchor;
@property(nonatomic, assign) NSInteger horizontalAlignment;
@property(nonatomic, assign) BOOL respectSafeArea;
@property(nonatomic, assign) CGFloat availableWidth;
@property(nonatomic, assign) CGFloat leftMargin;
@property(nonatomic, assign) CGFloat topMargin;
@property(nonatomic, assign) CGFloat rightMargin;
@property(nonatomic, assign) CGFloat bottomMargin;
@property(nonatomic, assign) GADAdSize currentAdSize;
@property(nonatomic, assign) BOOL initialLoadComplete;
@property(nonatomic, assign) BOOL bannerReady;
@property(nonatomic, assign) BOOL notifyShownAfterLoad;
@property(nonatomic, assign) BOOL layoutRefreshScheduled;

@end

@interface OpenMobileBannerLayoutObserver : UIView

@property(nonatomic, copy, nullable) dispatch_block_t layoutHandler;

@end


@implementation OpenMobileBannerLayoutObserver

- (void)layoutSubviews
{
	[super layoutSubviews];
	if (self.layoutHandler)
	{
		self.layoutHandler();
	}
}

- (void)safeAreaInsetsDidChange
{
	[super safeAreaInsetsDidChange];
	if (self.layoutHandler)
	{
		self.layoutHandler();
	}
}

@end

@interface OpenMobileBannerAdDelegate ()

@property(nonatomic, strong, nullable) OpenMobileBannerLayoutObserver* layoutObserver;

@end

static NSMutableDictionary<NSNumber*, OpenMobileBannerAdDelegate*>*
	GOpenMobileBannerAds = nil;

static BOOL AttachOpenMobileBanner(
	OpenMobileBannerAdDelegate* Handler,
	NSString** OutError
);

static void DetachOpenMobileBanner(
	OpenMobileBannerAdDelegate* Handler,
	BOOL ClearShowRequest
)
{
	if (!Handler)
	{
		return;
	}
	[NSLayoutConstraint deactivateConstraints:Handler.constraints ?: @[]];
	[Handler.bannerView removeFromSuperview];
	Handler.layoutObserver.layoutHandler = nil;
	[Handler.layoutObserver removeFromSuperview];
	if (Handler.layoutGuide && Handler.hostView)
	{
		[Handler.hostView removeLayoutGuide:Handler.layoutGuide];
	}
	Handler.constraints = nil;
	Handler.layoutGuide = nil;
	Handler.hostView = nil;
	Handler.layoutObserver = nil;
	Handler.layoutRefreshScheduled = NO;
	if (ClearShowRequest)
	{
		Handler.showRequestId = 0;
		Handler.notifyShownAfterLoad = NO;
	}
}

static void DestroyOpenMobileBannerView(OpenMobileBannerAdDelegate* Handler)
{
	Handler.bannerView.delegate = nil;
	Handler.bannerView.paidEventHandler = nil;
	Handler.bannerView = nil;
}

static void DestroyOpenMobileBanner(OpenMobileBannerAdDelegate* Handler)
{
	DetachOpenMobileBanner(Handler, YES);
	DestroyOpenMobileBannerView(Handler);
}

static void UpdateOpenMobileBannerLayout(
	OpenMobileBannerAdDelegate* Handler,
	const FOpenMobileAdsBannerLayout& Layout
)
{
	Handler.anchor = static_cast<NSInteger>(Layout.Anchor);
	Handler.horizontalAlignment = static_cast<NSInteger>(Layout.HorizontalAlignment);
	Handler.respectSafeArea = Layout.bRespectSafeArea;
	Handler.availableWidth = Layout.AvailableWidth;
	Handler.leftMargin = Layout.Margins.Left;
	Handler.topMargin = Layout.Margins.Top;
	Handler.rightMargin = Layout.Margins.Right;
	Handler.bottomMargin = Layout.Margins.Bottom;
}

static BOOL ResolveOpenMobileBannerSize(
	OpenMobileBannerAdDelegate* Handler,
	UIView* HostView,
	GADAdSize* OutSize,
	NSString** OutError
)
{
	if (Handler.mediumRectangle)
	{
		*OutSize = GADAdSizeMediumRectangle;
		return YES;
	}
	if (!Handler.anchoredAdaptive)
	{
		*OutSize = GADAdSizeBanner;
		return YES;
	}
	[HostView layoutIfNeeded];
	CGRect AvailableFrame = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.layoutFrame
		: HostView.bounds;
	CGFloat AvailableWidth = CGRectGetWidth(AvailableFrame)
		- Handler.leftMargin - Handler.rightMargin;
	if (Handler.availableWidth > 0.0)
	{
		AvailableWidth = MIN(AvailableWidth, Handler.availableWidth);
	}
	if (AvailableWidth < 320.0)
	{
		if (OutError)
		{
			*OutError = @"The iOS safe area and margins cannot fit an adaptive banner.";
		}
		return NO;
	}
	*OutSize = GADLargeAnchoredAdaptiveBannerAdSizeWithWidth(
		floor(AvailableWidth)
	);
	return YES;
}

static void LoadOpenMobileBannerView(
	OpenMobileBannerAdDelegate* Handler,
	GADAdSize AdSize
)
{
	DestroyOpenMobileBannerView(Handler);
	Handler.bannerReady = NO;
	GADBannerView* BannerView = [[GADBannerView alloc] initWithAdSize:AdSize];
	Handler.bannerView = BannerView;
	Handler.currentAdSize = AdSize;
	BannerView.adUnitID = Handler.adUnitId;
	BannerView.delegate = Handler;
	BannerView.rootViewController = OpenMobileAdsAdMobIOS::TopViewController(
		(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
	);
	__weak OpenMobileBannerAdDelegate* WeakHandler = Handler;
	BannerView.paidEventHandler = ^(GADAdValue* AdValue)
	{
		OpenMobileBannerAdDelegate* StrongHandler = WeakHandler;
		if (!StrongHandler || StrongHandler.showRequestId <= 0)
		{
			return;
		}
		FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
			StrongHandler.showRequestId,
			AdValue.value.longLongValue,
			OpenMobileAdsAdMobIOS::ToFString(AdValue.currencyCode),
			static_cast<int32>(AdValue.precision)
		);
	};
	[BannerView loadRequest:[GADRequest request]];
}

static void FailOpenMobileBannerLayout(
	OpenMobileBannerAdDelegate* Handler,
	NSString* ErrorMessage
)
{
	const int64_t FailedShowRequestId = Handler.showRequestId;
	[GOpenMobileBannerAds removeObjectForKey:@(Handler.loadRequestId)];
	DestroyOpenMobileBanner(Handler);
	FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
		FailedShowRequestId,
		OpenMobileAdsAdMobIOS::ToFString(ErrorMessage)
	);
}

static void ScheduleOpenMobileBannerLayout(
	OpenMobileBannerAdDelegate* Handler
)
{
	if (Handler.layoutRefreshScheduled)
	{
		return;
	}
	Handler.layoutRefreshScheduled = YES;
	__weak OpenMobileBannerAdDelegate* WeakHandler = Handler;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		OpenMobileBannerAdDelegate* StrongHandler = WeakHandler;
		if (!StrongHandler)
		{
			return;
		}
		StrongHandler.layoutRefreshScheduled = NO;
		if (
			StrongHandler.showRequestId <= 0
			|| GOpenMobileBannerAds[@(StrongHandler.loadRequestId)] != StrongHandler
		)
		{
			return;
		}
		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		NSString* Error = nil;
		GADAdSize DesiredSize;
		if (!RootController.view || !ResolveOpenMobileBannerSize(
			StrongHandler,
			RootController.view,
			&DesiredSize,
			&Error
		))
		{
			FailOpenMobileBannerLayout(
				StrongHandler,
				Error ?: @"No iOS view is available for persistent ad layout."
			);
			return;
		}
		CGRect AvailableFrame = StrongHandler.respectSafeArea
			? RootController.view.safeAreaLayoutGuide.layoutFrame
			: RootController.view.bounds;
		CGSize BannerSize = CGSizeFromGADAdSize(DesiredSize);
		if (
			CGRectGetWidth(AvailableFrame)
				- StrongHandler.leftMargin - StrongHandler.rightMargin
				< BannerSize.width
			|| CGRectGetHeight(AvailableFrame)
				- StrongHandler.topMargin - StrongHandler.bottomMargin
				< BannerSize.height
		)
		{
			FailOpenMobileBannerLayout(
				StrongHandler,
				@"The iOS safe area and margins cannot fit the ad."
			);
			return;
		}
		if (
			!StrongHandler.anchoredAdaptive
			|| GADAdSizeEqualToSize(DesiredSize, StrongHandler.currentAdSize)
		)
		{
			return;
		}
		DetachOpenMobileBanner(StrongHandler, NO);
		StrongHandler.notifyShownAfterLoad = NO;
		LoadOpenMobileBannerView(StrongHandler, DesiredSize);
	});
}

static BOOL AttachOpenMobileBanner(
	OpenMobileBannerAdDelegate* Handler,
	NSString** OutError
)
{
	UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
		(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
	);
	UIView* HostView = RootController.view;
	if (!RootController || !HostView || !Handler.bannerView)
	{
		if (OutError)
		{
			*OutError = @"No iOS view is available for the banner.";
		}
		return NO;
	}

	[HostView layoutIfNeeded];
	CGRect AvailableFrame = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.layoutFrame
		: HostView.bounds;
	CGSize BannerSize = CGSizeFromGADAdSize(Handler.currentAdSize);
	CGFloat AvailableWidth = CGRectGetWidth(AvailableFrame)
		- Handler.leftMargin - Handler.rightMargin;
	CGFloat AvailableHeight = CGRectGetHeight(AvailableFrame)
		- Handler.topMargin - Handler.bottomMargin;
	if (
		AvailableWidth < BannerSize.width
		|| AvailableHeight < BannerSize.height
	)
	{
		if (OutError)
		{
			*OutError = @"The iOS safe area and margins cannot fit the banner.";
		}
		return NO;
	}

	UILayoutGuide* Guide = [[UILayoutGuide alloc] init];
	[HostView addLayoutGuide:Guide];
	Handler.hostView = HostView;
	Handler.layoutGuide = Guide;
	Handler.bannerView.rootViewController = RootController;
	Handler.bannerView.translatesAutoresizingMaskIntoConstraints = NO;
	[HostView addSubview:Handler.bannerView];

	NSLayoutXAxisAnchor* LeadingAnchor = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.leadingAnchor
		: HostView.leadingAnchor;
	NSLayoutXAxisAnchor* TrailingAnchor = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.trailingAnchor
		: HostView.trailingAnchor;
	NSLayoutYAxisAnchor* TopAnchor = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.topAnchor
		: HostView.topAnchor;
	NSLayoutYAxisAnchor* BottomAnchor = Handler.respectSafeArea
		? HostView.safeAreaLayoutGuide.bottomAnchor
		: HostView.bottomAnchor;

	NSMutableArray<NSLayoutConstraint*>* Constraints = [NSMutableArray arrayWithArray:@[
		[Guide.leadingAnchor constraintEqualToAnchor:LeadingAnchor
			constant:Handler.leftMargin],
		[Guide.trailingAnchor constraintEqualToAnchor:TrailingAnchor
			constant:-Handler.rightMargin],
		[Guide.topAnchor constraintEqualToAnchor:TopAnchor
			constant:Handler.topMargin],
		[Guide.bottomAnchor constraintEqualToAnchor:BottomAnchor
			constant:-Handler.bottomMargin],
		[Handler.bannerView.widthAnchor constraintEqualToConstant:BannerSize.width],
		[Handler.bannerView.heightAnchor constraintEqualToConstant:BannerSize.height]
	]];
	if (
		Handler.horizontalAlignment
		== static_cast<NSInteger>(EOpenMobileAdsBannerHorizontalAlignment::Left)
	)
	{
		[Constraints addObject:[Handler.bannerView.leadingAnchor
			constraintEqualToAnchor:Guide.leadingAnchor]];
	}
	else if (
		Handler.horizontalAlignment
		== static_cast<NSInteger>(EOpenMobileAdsBannerHorizontalAlignment::Right)
	)
	{
		[Constraints addObject:[Handler.bannerView.trailingAnchor
			constraintEqualToAnchor:Guide.trailingAnchor]];
	}
	else
	{
		[Constraints addObject:[Handler.bannerView.centerXAnchor
			constraintEqualToAnchor:Guide.centerXAnchor]];
	}
	if (Handler.anchor == static_cast<NSInteger>(EOpenMobileAdsBannerAnchor::Top))
	{
		[Constraints addObject:[Handler.bannerView.topAnchor
			constraintEqualToAnchor:Guide.topAnchor]];
	}
	else if (
		Handler.anchor
		== static_cast<NSInteger>(EOpenMobileAdsBannerAnchor::Center)
	)
	{
		[Constraints addObject:[Handler.bannerView.centerYAnchor
			constraintEqualToAnchor:Guide.centerYAnchor]];
	}
	else
	{
		[Constraints addObject:[Handler.bannerView.bottomAnchor
			constraintEqualToAnchor:Guide.bottomAnchor]];
	}

	if (Handler.anchoredAdaptive || Handler.mediumRectangle)
	{
		OpenMobileBannerLayoutObserver* Observer =
			[[OpenMobileBannerLayoutObserver alloc] initWithFrame:CGRectZero];
		Observer.translatesAutoresizingMaskIntoConstraints = NO;
		Observer.userInteractionEnabled = NO;
		__weak OpenMobileBannerAdDelegate* WeakHandler = Handler;
		Observer.layoutHandler = ^
		{
			OpenMobileBannerAdDelegate* StrongHandler = WeakHandler;
			if (StrongHandler)
			{
				ScheduleOpenMobileBannerLayout(StrongHandler);
			}
		};
		[HostView insertSubview:Observer belowSubview:Handler.bannerView];
		[Constraints addObjectsFromArray:@[
			[Observer.leadingAnchor constraintEqualToAnchor:HostView.leadingAnchor],
			[Observer.trailingAnchor constraintEqualToAnchor:HostView.trailingAnchor],
			[Observer.topAnchor constraintEqualToAnchor:HostView.topAnchor],
			[Observer.bottomAnchor constraintEqualToAnchor:HostView.bottomAnchor]
		]];
		Handler.layoutObserver = Observer;
	}

	Handler.constraints = Constraints;
	[NSLayoutConstraint activateConstraints:Constraints];
	return YES;
}

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

@implementation OpenMobileRewardedInterstitialAdDelegate

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
	const int64_t FailedRequestId = self.requestId;
	self.rewardedInterstitialAd.paidEventHandler = nil;
	self.rewardedInterstitialAd = nil;
	if (GOpenMobileRewardedInterstitialAdDelegate == self)
	{
		GOpenMobileRewardedInterstitialAdDelegate = nil;
	}

	NSString* Detail = error.localizedDescription ?: @"Unknown presentation error.";
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		FailedRequestId,
		OpenMobileAdsAdMobIOS::ToFString(
			[@"Rewarded interstitial failed to show: " stringByAppendingString:Detail]
		)
	);
}

- (void)adDidDismissFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	const int64_t ClosedRequestId = self.requestId;
	self.rewardedInterstitialAd.paidEventHandler = nil;
	self.rewardedInterstitialAd = nil;
	if (GOpenMobileRewardedInterstitialAdDelegate == self)
	{
		GOpenMobileRewardedInterstitialAdDelegate = nil;
	}

	FOpenMobileAdsAdMobPlatform::NativeClosed(ClosedRequestId);
}

@end

@implementation OpenMobileInterstitialAdDelegate

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
	const int64_t FailedRequestId = self.requestId;
	self.interstitialAd.paidEventHandler = nil;
	self.interstitialAd = nil;
	if (GOpenMobileInterstitialAdDelegate == self)
	{
		GOpenMobileInterstitialAdDelegate = nil;
	}

	NSString* Detail = error.localizedDescription ?: @"Unknown presentation error.";
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		FailedRequestId,
		OpenMobileAdsAdMobIOS::ToFString(
			[@"Interstitial failed to show: " stringByAppendingString:Detail]
		)
	);
}

- (void)adDidDismissFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	const int64_t ClosedRequestId = self.requestId;
	self.interstitialAd.paidEventHandler = nil;
	self.interstitialAd = nil;
	if (GOpenMobileInterstitialAdDelegate == self)
	{
		GOpenMobileInterstitialAdDelegate = nil;
	}

	FOpenMobileAdsAdMobPlatform::NativeClosed(ClosedRequestId);
}

@end

@implementation OpenMobileAppOpenAdDelegate

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
	const int64_t FailedRequestId = self.requestId;
	self.appOpenAd.paidEventHandler = nil;
	self.appOpenAd = nil;
	if (GOpenMobileAppOpenAdDelegate == self)
	{
		GOpenMobileAppOpenAdDelegate = nil;
	}

	NSString* Detail = error.localizedDescription ?: @"Unknown presentation error.";
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		FailedRequestId,
		OpenMobileAdsAdMobIOS::ToFString(
			[@"App-open ad failed to show: " stringByAppendingString:Detail]
		)
	);
}

- (void)adDidDismissFullScreenContent:(id<GADFullScreenPresentingAd>)ad
{
	const int64_t ClosedRequestId = self.requestId;
	self.appOpenAd.paidEventHandler = nil;
	self.appOpenAd = nil;
	if (GOpenMobileAppOpenAdDelegate == self)
	{
		GOpenMobileAppOpenAdDelegate = nil;
	}

	FOpenMobileAdsAdMobPlatform::NativeClosed(ClosedRequestId);
}

@end

@implementation OpenMobileBannerAdDelegate

- (void)bannerViewDidReceiveAd:(GADBannerView*)bannerView
{
	if (
		GOpenMobileBannerAds[@(self.loadRequestId)] != self
		|| self.bannerView != bannerView
	)
	{
		return;
	}
	if (!self.initialLoadComplete)
	{
		self.bannerReady = YES;
		self.initialLoadComplete = YES;
		FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(self.loadRequestId);
		return;
	}
	if (self.showRequestId <= 0)
	{
		self.bannerReady = YES;
		return;
	}
	self.bannerReady = YES;

	NSString* Error = nil;
	if (!AttachOpenMobileBanner(self, &Error))
	{
		FailOpenMobileBannerLayout(
			self,
			Error ?: @"The resized iOS adaptive banner could not be attached."
		);
		return;
	}
	if (self.notifyShownAfterLoad)
	{
		self.notifyShownAfterLoad = NO;
		FOpenMobileAdsAdMobPlatform::NativeBannerShown(self.showRequestId);
	}
}

- (void)bannerView:(GADBannerView*)bannerView
	didFailToReceiveAdWithError:(NSError*)error
{
	NSNumber* Key = @(self.loadRequestId);
	if (GOpenMobileBannerAds[Key] != self)
	{
		return;
	}
	NSString* Detail = error.localizedDescription ?: @"Unknown banner load error.";
	NSString* Message = [@"Banner failed to load: " stringByAppendingString:Detail];
	if (self.initialLoadComplete)
	{
		if (self.showRequestId > 0)
		{
			FailOpenMobileBannerLayout(self, Message);
		}
		else
		{
			DestroyOpenMobileBannerView(self);
		}
	}
	else
	{
		[GOpenMobileBannerAds removeObjectForKey:Key];
		const int64_t FailedRequestId = self.loadRequestId;
		DestroyOpenMobileBanner(self);
		FOpenMobileAdsAdMobPlatform::NativeBannerLoadFailed(
			FailedRequestId,
			OpenMobileAdsAdMobIOS::ToFString(Message)
		);
	}
}

- (void)bannerViewDidRecordImpression:(GADBannerView*)bannerView
{
	if (self.showRequestId > 0)
	{
		FOpenMobileAdsAdMobPlatform::NativeImpression(self.showRequestId);
	}
}

- (void)bannerViewDidRecordClick:(GADBannerView*)bannerView
{
	if (self.showRequestId > 0)
	{
		FOpenMobileAdsAdMobPlatform::NativeClicked(self.showRequestId);
	}
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
		GOpenMobileRewardedInterstitialAdDelegate.rewardedInterstitialAd
			.fullScreenContentDelegate = nil;
		GOpenMobileRewardedInterstitialAdDelegate.rewardedInterstitialAd = nil;
		GOpenMobileRewardedInterstitialAdDelegate = nil;
		GOpenMobileInterstitialAdDelegate.interstitialAd.fullScreenContentDelegate = nil;
		GOpenMobileInterstitialAdDelegate.interstitialAd = nil;
		GOpenMobileInterstitialAdDelegate = nil;
		GOpenMobileAppOpenAdDelegate.appOpenAd.fullScreenContentDelegate = nil;
		GOpenMobileAppOpenAdDelegate.appOpenAd = nil;
		GOpenMobileAppOpenAdDelegate = nil;
		[GOpenMobileLoadedRewardedAds removeAllObjects];
		[GOpenMobileRewardedAdLoadRequests removeAllObjects];
		[GOpenMobileLoadedRewardedInterstitialAds removeAllObjects];
		[GOpenMobileRewardedInterstitialAdLoadRequests removeAllObjects];
		[GOpenMobileLoadedInterstitialAds removeAllObjects];
		[GOpenMobileInterstitialAdLoadRequests removeAllObjects];
		[GOpenMobileLoadedAppOpenAds removeAllObjects];
		[GOpenMobileAppOpenAdLoadRequests removeAllObjects];
		for (OpenMobileBannerAdDelegate* Handler in GOpenMobileBannerAds.allValues)
		{
			DestroyOpenMobileBanner(Handler);
		}
		[GOpenMobileBannerAds removeAllObjects];
		GOpenMobileLoadedRewardedAds = nil;
		GOpenMobileRewardedAdLoadRequests = nil;
		GOpenMobileLoadedRewardedInterstitialAds = nil;
		GOpenMobileRewardedInterstitialAdLoadRequests = nil;
		GOpenMobileLoadedInterstitialAds = nil;
		GOpenMobileInterstitialAdLoadRequests = nil;
		GOpenMobileLoadedAppOpenAds = nil;
		GOpenMobileAppOpenAdLoadRequests = nil;
		GOpenMobileBannerAds = nil;
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
			DebugSettings.geography = OpenMobileAdsAdMobIOS::ToDebugGeography(
				Request.Development.GetEffectiveDebugGeography()
			);
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

bool FOpenMobileAdsAdMobIOSBackend::PresentPrivacyOptionsForm(
	const int64 RequestId,
	FString& OutError
)
{
	if (RequestId <= 0)
	{
		OutError = TEXT("The iOS Google UMP privacy-options request ID is invalid.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		[UMPConsentForm
			presentPrivacyOptionsFormFromViewController:RootController
			completionHandler:^(NSError* Error)
		{
			if (Error)
			{
				FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToUMPErrorCode(Error, true),
					OpenMobileAdsAdMobIOS::ToFString(
						Error.localizedDescription ?: @"Unknown UMP privacy-options error."
					)
				);
				return;
			}
			OpenMobileAdsAdMobIOS::CompleteConsentInfo(RequestId, true);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::ResetConsentForTesting(
	FString& OutError
)
{
	__block bool bResetConfirmed = false;
	void (^ResetBlock)(void) = ^
	{
		[UMPConsentInformation.sharedInstance reset];
		bResetConfirmed =
			UMPConsentInformation.sharedInstance.consentStatus
			== UMPConsentStatusUnknown;
		[NSUserDefaults.standardUserDefaults removeObjectForKey:@"gad_rdp"];
	};
	if (NSThread.isMainThread)
	{
		ResetBlock();
	}
	else
	{
		dispatch_sync(dispatch_get_main_queue(), ResetBlock);
	}
	if (!bResetConfirmed)
	{
		OutError = TEXT("iOS Google UMP consent state did not reset to unknown.");
	}
	return bResetConfirmed;
}

bool FOpenMobileAdsAdMobIOSBackend::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask,
	FString& OutError
)
{
	if (
		(SignalMask & static_cast<int32>(
			EOpenMobileAdsConsentSignal::UsPrivacy
		)) != 0
	)
	{
		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(
			Signals.UsPrivacy.DataProcessingMode
		);
	}
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
		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(DataProcessingMode);
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

bool FOpenMobileAdsAdMobIOSBackend::LoadRewardedInterstitialAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The iOS rewarded-interstitial ad unit ID is empty.");
		return false;
	}

	NSString* IOSAdUnitId = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*AdUnitId)];
	if (!IOSAdUnitId)
	{
		OutError = TEXT("The iOS rewarded-interstitial ad unit ID could not be encoded.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (!GOpenMobileLoadedRewardedInterstitialAds)
		{
			GOpenMobileLoadedRewardedInterstitialAds =
				[[NSMutableDictionary alloc] init];
		}
		if (!GOpenMobileRewardedInterstitialAdLoadRequests)
		{
			GOpenMobileRewardedInterstitialAdLoadRequests =
				[[NSMutableSet alloc] init];
		}
		NSNumber* Key = @(RequestId);
		if (
			[GOpenMobileRewardedInterstitialAdLoadRequests containsObject:Key]
			|| GOpenMobileLoadedRewardedInterstitialAds[Key] != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadFailed(
				RequestId,
				TEXT("The iOS rewarded-interstitial load request is already active.")
			);
			return;
		}

		[GOpenMobileRewardedInterstitialAdLoadRequests addObject:Key];
		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(DataProcessingMode);
		[GADRewardedInterstitialAd loadWithAdUnitID:IOSAdUnitId
			request:[GADRequest request]
			completionHandler:^(
				GADRewardedInterstitialAd* RewardedInterstitialAd,
				NSError* Error
			)
		{
			if (![GOpenMobileRewardedInterstitialAdLoadRequests containsObject:Key])
			{
				return;
			}
			[GOpenMobileRewardedInterstitialAdLoadRequests removeObject:Key];
			if (Error || !RewardedInterstitialAd)
			{
				NSString* Detail = Error.localizedDescription
					?: @"No rewarded interstitial was returned.";
				FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"Rewarded interstitial failed to load: "
							stringByAppendingString:Detail]
					)
				);
				return;
			}

			GOpenMobileLoadedRewardedInterstitialAds[Key] = RewardedInterstitialAd;
			GADAdReward* Reward = RewardedInterstitialAd.adReward;
			FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
				RequestId,
				Reward.amount.longLongValue,
				OpenMobileAdsAdMobIOS::ToFString(Reward.type)
			);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::LoadInterstitialAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The iOS interstitial ad unit ID is empty.");
		return false;
	}

	NSString* IOSAdUnitId = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*AdUnitId)];
	if (!IOSAdUnitId)
	{
		OutError = TEXT("The iOS interstitial ad unit ID could not be encoded.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (!GOpenMobileLoadedInterstitialAds)
		{
			GOpenMobileLoadedInterstitialAds = [[NSMutableDictionary alloc] init];
		}
		if (!GOpenMobileInterstitialAdLoadRequests)
		{
			GOpenMobileInterstitialAdLoadRequests = [[NSMutableSet alloc] init];
		}
		NSNumber* Key = @(RequestId);
		if (
			[GOpenMobileInterstitialAdLoadRequests containsObject:Key]
			|| GOpenMobileLoadedInterstitialAds[Key] != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadFailed(
				RequestId,
				TEXT("The iOS interstitial load request is already active.")
			);
			return;
		}

		[GOpenMobileInterstitialAdLoadRequests addObject:Key];
		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(DataProcessingMode);
		[GADInterstitialAd loadWithAdUnitID:IOSAdUnitId
							 request:[GADRequest request]
					  completionHandler:^(GADInterstitialAd* InterstitialAd, NSError* Error)
		{
			if (![GOpenMobileInterstitialAdLoadRequests containsObject:Key])
			{
				return;
			}
			[GOpenMobileInterstitialAdLoadRequests removeObject:Key];
			if (Error || !InterstitialAd)
			{
				NSString* Detail = Error.localizedDescription
					?: @"No interstitial was returned.";
				FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"Interstitial failed to load: " stringByAppendingString:Detail]
					)
				);
				return;
			}

			GOpenMobileLoadedInterstitialAds[Key] = InterstitialAd;
			FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(RequestId);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::LoadAppOpenAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	FString& OutError
)
{
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The iOS app-open ad unit ID is empty.");
		return false;
	}

	NSString* IOSAdUnitId = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*AdUnitId)];
	if (!IOSAdUnitId)
	{
		OutError = TEXT("The iOS app-open ad unit ID could not be encoded.");
		return false;
	}

	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (!GOpenMobileLoadedAppOpenAds)
		{
			GOpenMobileLoadedAppOpenAds = [[NSMutableDictionary alloc] init];
		}
		if (!GOpenMobileAppOpenAdLoadRequests)
		{
			GOpenMobileAppOpenAdLoadRequests = [[NSMutableSet alloc] init];
		}
		NSNumber* Key = @(RequestId);
		if (
			[GOpenMobileAppOpenAdLoadRequests containsObject:Key]
			|| GOpenMobileLoadedAppOpenAds[Key] != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadFailed(
				RequestId,
				TEXT("The iOS app-open load request is already active.")
			);
			return;
		}

		[GOpenMobileAppOpenAdLoadRequests addObject:Key];
		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(DataProcessingMode);
		[GADAppOpenAd loadWithAdUnitID:IOSAdUnitId
			request:[GADRequest request]
			completionHandler:^(GADAppOpenAd* AppOpenAd, NSError* Error)
		{
			if (![GOpenMobileAppOpenAdLoadRequests containsObject:Key])
			{
				return;
			}
			[GOpenMobileAppOpenAdLoadRequests removeObject:Key];
			if (Error || !AppOpenAd)
			{
				NSString* Detail = Error.localizedDescription
					?: @"No app-open ad was returned.";
				FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadFailed(
					RequestId,
					OpenMobileAdsAdMobIOS::ToFString(
						[@"App-open ad failed to load: "
							stringByAppendingString:Detail]
					)
				);
				return;
			}

			GOpenMobileLoadedAppOpenAds[Key] = AppOpenAd;
			FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadCompleted(RequestId);
		}];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::LoadBannerAd(
	const FString& AdUnitId,
	const int64 RequestId,
	EOpenMobileAdsDataProcessingMode DataProcessingMode,
	EOpenMobileAdFormat Format,
	const FOpenMobileAdsBannerLayout& Layout,
	FString& OutError
)
{
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The iOS banner ad unit ID is empty.");
		return false;
	}

	NSString* IOSAdUnitId = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*AdUnitId)];
	if (!IOSAdUnitId)
	{
		OutError = TEXT("The iOS banner ad unit ID could not be encoded.");
		return false;
	}

	const bool bAdaptive = Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner;
	const bool bMediumRectangle = Format == EOpenMobileAdFormat::MediumRectangle;
	if (!bAdaptive && !bMediumRectangle && Format != EOpenMobileAdFormat::Banner)
	{
		OutError = TEXT("iOS received an unsupported persistent ad format.");
		return false;
	}
	const FOpenMobileAdsBannerLayout BannerLayout = Layout;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (!GOpenMobileBannerAds)
		{
			GOpenMobileBannerAds = [[NSMutableDictionary alloc] init];
		}
		NSNumber* Key = @(RequestId);
		if (GOpenMobileBannerAds[Key] != nil)
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerLoadFailed(
				RequestId,
				TEXT("The iOS banner load request is already active.")
			);
			return;
		}

		OpenMobileAdsAdMobIOS::ApplyDataProcessingMode(DataProcessingMode);
		OpenMobileBannerAdDelegate* Handler =
			[[OpenMobileBannerAdDelegate alloc] init];
		Handler.loadRequestId = RequestId;
		Handler.adUnitId = IOSAdUnitId;
		Handler.anchoredAdaptive = bAdaptive;
		Handler.mediumRectangle = bMediumRectangle;
		UpdateOpenMobileBannerLayout(Handler, BannerLayout);

		GADAdSize BannerSize = bMediumRectangle
			? GADAdSizeMediumRectangle
			: GADAdSizeBanner;
		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		NSString* Error = nil;
		if (bAdaptive && (!RootController.view || !ResolveOpenMobileBannerSize(
			Handler,
			RootController.view,
			&BannerSize,
			&Error
		)))
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerLoadFailed(
				RequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					Error ?: @"No iOS view is available for adaptive banner sizing."
				)
			);
			return;
		}
		GOpenMobileBannerAds[Key] = Handler;
		LoadOpenMobileBannerView(Handler, BannerSize);
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

void FOpenMobileAdsAdMobIOSBackend::CancelRewardedInterstitialAd(
	const int64 RequestId
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		NSNumber* Key = @(RequestId);
		[GOpenMobileRewardedInterstitialAdLoadRequests removeObject:Key];
		[GOpenMobileLoadedRewardedInterstitialAds removeObjectForKey:Key];
	});
}

void FOpenMobileAdsAdMobIOSBackend::CancelInterstitialAd(
	const int64 RequestId
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		NSNumber* Key = @(RequestId);
		[GOpenMobileInterstitialAdLoadRequests removeObject:Key];
		[GOpenMobileLoadedInterstitialAds removeObjectForKey:Key];
	});
}

void FOpenMobileAdsAdMobIOSBackend::CancelAppOpenAd(
	const int64 RequestId
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		NSNumber* Key = @(RequestId);
		[GOpenMobileAppOpenAdLoadRequests removeObject:Key];
		[GOpenMobileLoadedAppOpenAds removeObjectForKey:Key];
	});
}

void FOpenMobileAdsAdMobIOSBackend::CancelBannerAd(const int64 RequestId)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		NSNumber* Key = @(RequestId);
		OpenMobileBannerAdDelegate* Handler = GOpenMobileBannerAds[Key];
		[GOpenMobileBannerAds removeObjectForKey:Key];
		DestroyOpenMobileBanner(Handler);
	});
}

bool FOpenMobileAdsAdMobIOSBackend::ShowAppOpenAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	FString& OutError
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (
			GOpenMobileRewardedAdDelegate != nil
			|| GOpenMobileRewardedInterstitialAdDelegate != nil
			|| GOpenMobileInterstitialAdDelegate != nil
			|| GOpenMobileAppOpenAdDelegate != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("An iOS full-screen ad is already loading or showing.")
			);
			return;
		}

		NSNumber* Key = @(LoadedRequestId);
		GADAppOpenAd* AppOpenAd = GOpenMobileLoadedAppOpenAds[Key];
		[GOpenMobileLoadedAppOpenAds removeObjectForKey:Key];
		if (!AppOpenAd)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("The cached iOS app-open ad is unavailable or already consumed.")
			);
			return;
		}

		OpenMobileAppOpenAdDelegate* Handler =
			[[OpenMobileAppOpenAdDelegate alloc] init];
		Handler.requestId = ShowRequestId;
		Handler.appOpenAd = AppOpenAd;
		GOpenMobileAppOpenAdDelegate = Handler;
		AppOpenAd.fullScreenContentDelegate = Handler;
		AppOpenAd.paidEventHandler = ^(GADAdValue* AdValue)
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
		if (![AppOpenAd canPresentFromRootViewController:RootController
			error:&PresentationError])
		{
			AppOpenAd.paidEventHandler = nil;
			Handler.appOpenAd = nil;
			GOpenMobileAppOpenAdDelegate = nil;
			NSString* Detail = PresentationError.localizedDescription
				?: @"No presenter is available.";
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					[@"App-open ad could not be presented: "
						stringByAppendingString:Detail]
				)
			);
			return;
		}

		[AppOpenAd presentFromRootViewController:RootController];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::ShowRewardedInterstitialAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FString& ServerVerificationCustomData,
	FString& OutError
)
{
	const FString VerificationData = ServerVerificationCustomData;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (
			GOpenMobileRewardedAdDelegate != nil
			|| GOpenMobileRewardedInterstitialAdDelegate != nil
			|| GOpenMobileInterstitialAdDelegate != nil
			|| GOpenMobileAppOpenAdDelegate != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("An iOS full-screen ad is already loading or showing.")
			);
			return;
		}

		NSNumber* Key = @(LoadedRequestId);
		GADRewardedInterstitialAd* RewardedInterstitialAd =
			GOpenMobileLoadedRewardedInterstitialAds[Key];
		[GOpenMobileLoadedRewardedInterstitialAds removeObjectForKey:Key];
		if (!RewardedInterstitialAd)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("The cached iOS rewarded interstitial is unavailable or already consumed.")
			);
			return;
		}

		OpenMobileRewardedInterstitialAdDelegate* Handler =
			[[OpenMobileRewardedInterstitialAdDelegate alloc] init];
		Handler.requestId = ShowRequestId;
		Handler.rewardedInterstitialAd = RewardedInterstitialAd;
		GOpenMobileRewardedInterstitialAdDelegate = Handler;
		RewardedInterstitialAd.fullScreenContentDelegate = Handler;
		if (!VerificationData.IsEmpty())
		{
			GADServerSideVerificationOptions* Options =
				[[GADServerSideVerificationOptions alloc] init];
			Options.customRewardString =
				FAppleStringUtils::ConvertToNSString(VerificationData);
			RewardedInterstitialAd.serverSideVerificationOptions = Options;
		}
		RewardedInterstitialAd.paidEventHandler = ^(GADAdValue* AdValue)
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
		if (![RewardedInterstitialAd canPresentFromRootViewController:RootController
			error:&PresentationError])
		{
			RewardedInterstitialAd.paidEventHandler = nil;
			Handler.rewardedInterstitialAd = nil;
			GOpenMobileRewardedInterstitialAdDelegate = nil;
			NSString* Detail = PresentationError.localizedDescription
				?: @"No presenter is available.";
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					[@"Rewarded interstitial could not be presented: "
						stringByAppendingString:Detail]
				)
			);
			return;
		}

		__weak OpenMobileRewardedInterstitialAdDelegate* WeakHandler = Handler;
		[RewardedInterstitialAd presentFromRootViewController:RootController
			userDidEarnRewardHandler:^
		{
			OpenMobileRewardedInterstitialAdDelegate* StrongHandler = WeakHandler;
			if (
				!StrongHandler
				|| GOpenMobileRewardedInterstitialAdDelegate != StrongHandler
			)
			{
				return;
			}
			GADAdReward* Reward = StrongHandler.rewardedInterstitialAd.adReward;
			FOpenMobileAdsAdMobPlatform::NativeEarned(
				ShowRequestId,
				Reward.amount.intValue,
				OpenMobileAdsAdMobIOS::ToFString(Reward.type)
			);
		}];
	});
	return true;
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
		if (
			GOpenMobileRewardedAdDelegate != nil
			|| GOpenMobileRewardedInterstitialAdDelegate != nil
			|| GOpenMobileInterstitialAdDelegate != nil
			|| GOpenMobileAppOpenAdDelegate != nil
		)
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

bool FOpenMobileAdsAdMobIOSBackend::ShowInterstitialAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	FString& OutError
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (
			GOpenMobileRewardedAdDelegate != nil
			|| GOpenMobileRewardedInterstitialAdDelegate != nil
			|| GOpenMobileInterstitialAdDelegate != nil
			|| GOpenMobileAppOpenAdDelegate != nil
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("An iOS full-screen ad is already loading or showing.")
			);
			return;
		}

		NSNumber* Key = @(LoadedRequestId);
		GADInterstitialAd* InterstitialAd = GOpenMobileLoadedInterstitialAds[Key];
		[GOpenMobileLoadedInterstitialAds removeObjectForKey:Key];
		if (!InterstitialAd)
		{
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				TEXT("The cached iOS interstitial is unavailable or already consumed.")
			);
			return;
		}

		OpenMobileInterstitialAdDelegate* Handler =
			[[OpenMobileInterstitialAdDelegate alloc] init];
		Handler.requestId = ShowRequestId;
		Handler.interstitialAd = InterstitialAd;
		GOpenMobileInterstitialAdDelegate = Handler;
		InterstitialAd.fullScreenContentDelegate = Handler;
		InterstitialAd.paidEventHandler = ^(GADAdValue* AdValue)
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
		if (![InterstitialAd canPresentFromRootViewController:RootController error:&PresentationError])
		{
			InterstitialAd.paidEventHandler = nil;
			Handler.interstitialAd = nil;
			GOpenMobileInterstitialAdDelegate = nil;
			NSString* Detail = PresentationError.localizedDescription
				?: @"No presenter is available.";
			FOpenMobileAdsAdMobPlatform::NativeFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					[@"Interstitial could not be presented: " stringByAppendingString:Detail]
				)
			);
			return;
		}

		[InterstitialAd presentFromRootViewController:RootController];
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::ShowBannerAd(
	const int64 LoadedRequestId,
	const int64 ShowRequestId,
	const FOpenMobileAdsBannerLayout& Layout,
	FString& OutError
)
{
	const FOpenMobileAdsBannerLayout BannerLayout = Layout;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		OpenMobileBannerAdDelegate* Handler =
			GOpenMobileBannerAds[@(LoadedRequestId)];
		if (!Handler)
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
				ShowRequestId,
				TEXT("The cached iOS banner is unavailable.")
			);
			return;
		}
		if (Handler.showRequestId > 0 || Handler.bannerView.superview != nil)
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
				ShowRequestId,
				TEXT("The cached iOS banner is already visible.")
			);
			return;
		}

		UpdateOpenMobileBannerLayout(Handler, BannerLayout);
		UIViewController* RootController = OpenMobileAdsAdMobIOS::TopViewController(
			(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
		);
		GADAdSize DesiredSize;
		NSString* Error = nil;
		if (!RootController.view || !ResolveOpenMobileBannerSize(
			Handler,
			RootController.view,
			&DesiredSize,
			&Error
		))
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					Error ?: @"No iOS view is available for the banner."
				)
			);
			return;
		}

		Handler.showRequestId = ShowRequestId;
		if (
			Handler.anchoredAdaptive
			&& (
				!Handler.bannerReady
				|| !GADAdSizeEqualToSize(DesiredSize, Handler.currentAdSize)
			)
		)
		{
			Handler.notifyShownAfterLoad = YES;
			LoadOpenMobileBannerView(Handler, DesiredSize);
			return;
		}

		if (!AttachOpenMobileBanner(Handler, &Error))
		{
			Handler.showRequestId = 0;
			FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
				ShowRequestId,
				OpenMobileAdsAdMobIOS::ToFString(
					Error ?: @"The iOS banner could not be attached."
				)
			);
			return;
		}
		FOpenMobileAdsAdMobPlatform::NativeBannerShown(ShowRequestId);
	});
	return true;
}

bool FOpenMobileAdsAdMobIOSBackend::HideBannerAd(
	const int64 LoadedRequestId,
	const int64 HideRequestId,
	FString& OutError
)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		OpenMobileBannerAdDelegate* Handler =
			GOpenMobileBannerAds[@(LoadedRequestId)];
		if (
			!Handler
			|| Handler.showRequestId <= 0
			|| (!Handler.anchoredAdaptive && !Handler.bannerView.superview)
		)
		{
			FOpenMobileAdsAdMobPlatform::NativeBannerOperationFailed(
				HideRequestId,
				TEXT("The cached iOS banner is not visible.")
			);
			return;
		}
		DetachOpenMobileBanner(Handler, YES);
		FOpenMobileAdsAdMobPlatform::NativeBannerHidden(HideRequestId);
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
		if (
			GOpenMobileRewardedAdDelegate != nil
			|| GOpenMobileRewardedInterstitialAdDelegate != nil
			|| GOpenMobileInterstitialAdDelegate != nil
			|| GOpenMobileAppOpenAdDelegate != nil
		)
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

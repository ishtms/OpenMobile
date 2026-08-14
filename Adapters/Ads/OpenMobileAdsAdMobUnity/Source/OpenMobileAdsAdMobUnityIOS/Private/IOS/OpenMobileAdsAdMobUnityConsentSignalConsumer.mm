#include "IOS/OpenMobileAdsAdMobUnityConsentSignalConsumer.h"

#include "OpenMobileAdsConfiguration.h"
#include "UObject/UObjectGlobals.h"

#import <Foundation/Foundation.h>
#import <UnityAds/UnityAds-Swift.h>

@interface GADMediationAdapterUnity : NSObject
@property(class, nonatomic, assign) BOOL testMode;
@end

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	constexpr int32 ChildDirectedSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::ChildDirected);
	constexpr int32 UnderAgeSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UnderAgeOfConsent);

	/** Keeps Unity GDPR metadata absent till consent state is usable. */
	bool RequiresGdprValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.GdprApplicability
				== EOpenMobileAdsGdprApplicability::Applicable
			|| Signals.ConsentRequirement
				== EOpenMobileAdsConsentRequirement::Required
			|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Required
			|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted
			|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Denied
			|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Obtained;
	}

	/** Keeps Unity do-not-sell metadata absent till US privacy has a real value. */
	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.UsPrivacy.Applicability
				== EOpenMobileAdsUsPrivacyApplicability::Applicable
			|| Signals.UsPrivacy.Choice
				!= EOpenMobileAdsUsPrivacyChoice::Unknown
			|| Signals.UsPrivacy.DataProcessingMode
				!= EOpenMobileAdsDataProcessingMode::Unspecified;
	}

	/** Resolves Unity non-behavioral metadata only from explicit age treatment. */
	bool RequiresNonBehavioralValue(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	)
	{
		return (
			(SignalMask & ChildDirectedSignal) != 0
			&& Signals.ChildDirectedTreatment != EOpenMobileAdsAgeTreatment::Unspecified
		) || (
			(SignalMask & UnderAgeSignal) != 0
			&& Signals.UnderAgeOfConsent != EOpenMobileAdsAgeTreatment::Unspecified
		);
	}
}

FOpenMobileAdsConsentSignalApplyResult
FOpenMobileAdsAdMobUnityConsentSignalConsumer::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask
)
{
	SignalMask &= FOpenMobileAdsConsentSignals::AllSignalMask;
	const bool bApplyGdpr = (SignalMask & GdprSignal) != 0
		&& RequiresGdprValue(Signals);
	const BOOL bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted
		|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Obtained;
	const bool bApplyUsPrivacy = (SignalMask & UsPrivacySignal) != 0
		&& RequiresUsPrivacyValue(Signals);
	const BOOL bDoNotSell =
		Signals.UsPrivacy.Choice == EOpenMobileAdsUsPrivacyChoice::OptedOut
		|| Signals.UsPrivacy.DataProcessingMode
			== EOpenMobileAdsDataProcessingMode::Restricted;
	const bool bApplyNonBehavioral = RequiresNonBehavioralValue(
		Signals,
		SignalMask
	);
	const BOOL bNonBehavioral =
		((SignalMask & ChildDirectedSignal) != 0
			&& Signals.ChildDirectedTreatment == EOpenMobileAdsAgeTreatment::Yes)
		|| ((SignalMask & UnderAgeSignal) != 0
			&& Signals.UnderAgeOfConsent == EOpenMobileAdsAgeTreatment::Yes);

	void (^ApplySignals)(void) = ^{
		const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
		GADMediationAdapterUnity.testMode =
			Settings && Settings->IsDevelopmentTestModeEnabled();
		if (bApplyGdpr)
		{
			[UnityAds setUserConsent:bHasUserConsent];
		}
		if (bApplyUsPrivacy)
		{
			[UnityAds setUserOptOut:bDoNotSell];
		}
		if (bApplyNonBehavioral)
		{
			[UnityAds setNonBehavioral:bNonBehavioral];
		}
	};
	if (NSThread.isMainThread)
	{
		ApplySignals();
	}
	else
	{
		dispatch_sync(dispatch_get_main_queue(), ApplySignals);
	}
	return FOpenMobileAdsConsentSignalApplyResult::Applied(
		SignalMask,
		0
	);
}

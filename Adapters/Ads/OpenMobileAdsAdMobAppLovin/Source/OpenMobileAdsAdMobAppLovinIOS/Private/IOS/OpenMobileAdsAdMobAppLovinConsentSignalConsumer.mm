#include "IOS/OpenMobileAdsAdMobAppLovinConsentSignalConsumer.h"

#include "OpenMobileAdsErrors.h"

#import <Foundation/Foundation.h>
#import <AppLovinSDK/ALPrivacySettings.h>

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);

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

	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.UsPrivacy.Applicability
				== EOpenMobileAdsUsPrivacyApplicability::Applicable
			|| Signals.UsPrivacy.Choice
				!= EOpenMobileAdsUsPrivacyChoice::Unknown
			|| Signals.UsPrivacy.DataProcessingMode
				!= EOpenMobileAdsDataProcessingMode::Unspecified;
	}
}

FOpenMobileAdsConsentSignalApplyResult
FOpenMobileAdsAdMobAppLovinConsentSignalConsumer::ApplyConsentSignals(
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

	__block bool bApplied = true;
	void (^ApplySignals)(void) = ^{
		if (bApplyGdpr)
		{
			[ALPrivacySettings setHasUserConsent:bHasUserConsent];
			bApplied = [ALPrivacySettings isUserConsentSet]
				&& [ALPrivacySettings hasUserConsent] == bHasUserConsent;
		}
		if (bApplied && bApplyUsPrivacy)
		{
			[ALPrivacySettings setDoNotSell:bDoNotSell];
			bApplied = [ALPrivacySettings isDoNotSellSet]
				&& [ALPrivacySettings isDoNotSell] == bDoNotSell;
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
	if (!bApplied)
	{
		FOpenMobileAdsConsentSignalApplyResult Result;
		Result.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NativeFailure,
			EOpenMobileAdsFailureStage::Consent,
			NAME_None,
			TEXT("iOS could not apply AppLovin privacy signals."),
			TEXT("AdMob")
		);
		return Result;
	}

	return FOpenMobileAdsConsentSignalApplyResult::Applied(
		SignalMask,
		SignalMask
	);
}

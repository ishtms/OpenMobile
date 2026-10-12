#include "IOS/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"

#include "OpenMobileAdsErrors.h"

#import <Foundation/Foundation.h>
#import <ChartboostSDK/Chartboost.h>

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);

	/** Keeps Chartboost GDPR state untouched till consent applicability or status is known. */
	bool RequiresGdprValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitGdprDecision();
	}

	/** Derives Chartboost US privacy only from an explicit opt-out choice or processing mode. */
	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitUsPrivacyDecision();
	}
}

FOpenMobileAdsConsentSignalApplyResult
FOpenMobileAdsAdMobChartboostConsentSignalConsumer::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask
)
{
	SignalMask &= FOpenMobileAdsConsentSignals::AllSignalMask;
	const bool bApplyGdpr = (SignalMask & GdprSignal) != 0
		&& RequiresGdprValue(Signals);
	const BOOL bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted;
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
			const CHBGDPRConsent Consent = bHasUserConsent
				? CHBGDPRConsentBehavioral
				: CHBGDPRConsentNonBehavioral;
			[Chartboost addDataUseConsent:[CHBGDPRDataUseConsent gdprConsent:Consent]];
			CHBGDPRDataUseConsent* Applied =
				[Chartboost dataUseConsentForPrivacyStandard:CHBPrivacyStandardGDPR];
			bApplied = Applied && Applied.consent == Consent;
		}
		if (bApplied && bApplyUsPrivacy)
		{
			const CHBCCPAConsent Consent = bDoNotSell
				? CHBCCPAConsentOptOutSale
				: CHBCCPAConsentOptInSale;
			[Chartboost addDataUseConsent:[CHBCCPADataUseConsent ccpaConsent:Consent]];
			CHBCCPADataUseConsent* Applied =
				[Chartboost dataUseConsentForPrivacyStandard:CHBPrivacyStandardCCPA];
			bApplied = Applied && Applied.consent == Consent;
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
			TEXT("iOS could not apply Chartboost privacy signals."),
			TEXT("AdMob")
		);
		return Result;
	}

	return FOpenMobileAdsConsentSignalApplyResult::Applied(
		SignalMask,
		SignalMask & ~((bApplyGdpr ? 0 : GdprSignal)
			| (bApplyUsPrivacy ? 0 : UsPrivacySignal))
	);
}

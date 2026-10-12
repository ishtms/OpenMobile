#include "IOS/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <VungleAdsSDK/VungleAdsSDK-Swift.h>

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	constexpr int32 SupportedSignals = GdprSignal | UsPrivacySignal;

	/** Applies Liftoff GDPR state only when the normalized snapshot provides a value. */
	bool RequiresGdprValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitGdprDecision();
	}

	/** Leaves Liftoff US privacy untouched when neither choice nor processing mode is known. */
	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitUsPrivacyDecision();
	}
}

int32 FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer::
GetSupportedConsentSignalMask() const
{
	return SupportedSignals;
}

int32 FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer::
GetRuntimeUpdatableConsentSignalMask() const
{
	return SupportedSignals;
}

FOpenMobileAdsConsentSignalApplyResult
FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask
)
{
	SignalMask &= SupportedSignals;
	const bool bApplyGdpr = (SignalMask & GdprSignal) != 0
		&& RequiresGdprValue(Signals);
	const BOOL bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted;
	const bool bApplyUsPrivacy = (SignalMask & UsPrivacySignal) != 0
		&& RequiresUsPrivacyValue(Signals);
	const BOOL bOptedIn = !(
		Signals.UsPrivacy.Choice == EOpenMobileAdsUsPrivacyChoice::OptedOut
		|| Signals.UsPrivacy.DataProcessingMode
			== EOpenMobileAdsDataProcessingMode::Restricted
	);

	void (^ApplySignals)(void) = ^{
		if (bApplyGdpr)
		{
			[VunglePrivacySettings setGDPRStatus:bHasUserConsent];
		}
		if (bApplyUsPrivacy)
		{
			[VunglePrivacySettings setCCPAStatus:bOptedIn];
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

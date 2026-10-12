#include "Android/OpenMobileAdsAdMobAppLovinConsentSignalConsumer.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileAdsErrors.h"

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);

	/** Avoids turning an unknown GDPR state into an AppLovin consent answer. */
	bool RequiresGdprValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitGdprDecision();
	}

	/** Sends do-not-sell only when US privacy state carries a real choice or mode. */
	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.HasExplicitUsPrivacyDecision();
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
	const bool bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted;
	const bool bApplyUsPrivacy = (SignalMask & UsPrivacySignal) != 0
		&& RequiresUsPrivacyValue(Signals);
	const bool bDoNotSell =
		Signals.UsPrivacy.Choice == EOpenMobileAdsUsPrivacyChoice::OptedOut
		|| Signals.UsPrivacy.DataProcessingMode
			== EOpenMobileAdsDataProcessingMode::Restricted;

	if (bApplyGdpr || bApplyUsPrivacy)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		static jmethodID ApplyMethod = nullptr;
		if (Env && !ApplyMethod)
		{
			ApplyMethod = FJavaWrapper::FindMethod(
				Env,
				FJavaWrapper::GameActivityClassID,
				"AndroidThunkJava_ApplyOpenMobileAdsAdMobAppLovinConsentSignals",
				"(ZZZZ)Z",
				false
			);
		}
		const bool bApplied = Env && ApplyMethod
			&& FJavaWrapper::CallBooleanMethod(
				Env,
				FJavaWrapper::GameActivityThis,
				ApplyMethod,
				static_cast<jboolean>(bApplyGdpr),
				static_cast<jboolean>(bHasUserConsent),
				static_cast<jboolean>(bApplyUsPrivacy),
				static_cast<jboolean>(bDoNotSell)
			);
		if (!bApplied)
		{
			FOpenMobileAdsConsentSignalApplyResult Result;
			Result.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Android could not apply AppLovin privacy signals."),
				TEXT("AdMob")
			);
			return Result;
		}
	}

	return FOpenMobileAdsConsentSignalApplyResult::Applied(
		SignalMask,
		SignalMask & ~((bApplyGdpr ? 0 : GdprSignal)
			| (bApplyUsPrivacy ? 0 : UsPrivacySignal))
	);
}

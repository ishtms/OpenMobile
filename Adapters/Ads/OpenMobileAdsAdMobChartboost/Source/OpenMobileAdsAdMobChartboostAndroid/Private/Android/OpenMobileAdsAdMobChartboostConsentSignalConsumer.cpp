#include "Android/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileAdsErrors.h"

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);

	/** Protects Chartboost from receiving a guessed GDPR value when state is still unknown. */
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

	/** Applies Chartboost's US privacy flag only when the snapshot contains a decision. */
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
FOpenMobileAdsAdMobChartboostConsentSignalConsumer::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals& Signals,
	int32 SignalMask
)
{
	SignalMask &= FOpenMobileAdsConsentSignals::AllSignalMask;
	const bool bApplyGdpr = (SignalMask & GdprSignal) != 0
		&& RequiresGdprValue(Signals);
	const bool bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted
		|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Obtained;
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
				"AndroidThunkJava_ApplyOpenMobileAdsAdMobChartboostConsentSignals",
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
				TEXT("Android could not apply Chartboost privacy signals."),
				TEXT("AdMob")
			);
			return Result;
		}
	}

	return FOpenMobileAdsConsentSignalApplyResult::Applied(
		SignalMask,
		SignalMask
	);
}

#include "Android/OpenMobileAdsAdMobUnityConsentSignalConsumer.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileAdsErrors.h"

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

	/** Writes Unity GDPR metadata only after the service has an applicable consent answer. */
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

	/** Writes Unity US privacy metadata only from an explicit choice or processing mode. */
	bool RequiresUsPrivacyValue(const FOpenMobileAdsConsentSignals& Signals)
	{
		return Signals.UsPrivacy.Applicability
				== EOpenMobileAdsUsPrivacyApplicability::Applicable
			|| Signals.UsPrivacy.Choice
				!= EOpenMobileAdsUsPrivacyChoice::Unknown
			|| Signals.UsPrivacy.DataProcessingMode
				!= EOpenMobileAdsDataProcessingMode::Unspecified;
	}

	/** Derives Unity's non-behavioral flag from explicit child or under-age treatment. */
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
	const bool bHasUserConsent =
		Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Granted
		|| Signals.ConsentStatus == EOpenMobileAdsConsentStatus::Obtained;
	const bool bApplyUsPrivacy = (SignalMask & UsPrivacySignal) != 0
		&& RequiresUsPrivacyValue(Signals);
	const bool bDoNotSell =
		Signals.UsPrivacy.Choice == EOpenMobileAdsUsPrivacyChoice::OptedOut
		|| Signals.UsPrivacy.DataProcessingMode
			== EOpenMobileAdsDataProcessingMode::Restricted;
	const bool bApplyNonBehavioral = RequiresNonBehavioralValue(
		Signals,
		SignalMask
	);
	const bool bNonBehavioral =
		((SignalMask & ChildDirectedSignal) != 0
			&& Signals.ChildDirectedTreatment == EOpenMobileAdsAgeTreatment::Yes)
		|| ((SignalMask & UnderAgeSignal) != 0
			&& Signals.UnderAgeOfConsent == EOpenMobileAdsAgeTreatment::Yes);

	if (bApplyGdpr || bApplyUsPrivacy || bApplyNonBehavioral)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		static jmethodID ApplyMethod = nullptr;
		if (Env && !ApplyMethod)
		{
			ApplyMethod = FJavaWrapper::FindMethod(
				Env,
				FJavaWrapper::GameActivityClassID,
				"AndroidThunkJava_ApplyOpenMobileAdsAdMobUnityConsentSignals",
				"(ZZZZZZ)Z",
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
				static_cast<jboolean>(bDoNotSell),
				static_cast<jboolean>(bApplyNonBehavioral),
				static_cast<jboolean>(bNonBehavioral)
			);
		if (!bApplied)
		{
			FOpenMobileAdsConsentSignalApplyResult Result;
			Result.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Consent,
				NAME_None,
				TEXT("Android could not apply Unity Ads privacy signals."),
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

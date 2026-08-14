#include "Android/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OpenMobileAdsErrors.h"

namespace
{
	constexpr int32 GdprSignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	constexpr int32 UsPrivacySignal =
		static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	constexpr int32 SupportedSignals = GdprSignal | UsPrivacySignal;

	/** Sends Liftoff a GDPR value only after the service has resolved real consent state. */
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

	/** Avoids overwriting Liftoff US privacy state when the snapshot remains unspecified. */
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

int32 FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer::
GetSupportedConsentSignalMask() const
{
	return SupportedSignals;
}

int32 FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer::
GetConfirmableConsentSignalMask() const
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
				"AndroidThunkJava_ApplyOpenMobileAdsAdMobLiftoffMonetizeConsentSignals",
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
				TEXT("Android could not apply Liftoff Monetize privacy signals."),
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

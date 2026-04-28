#include "OpenMobileAdsErrors.h"

namespace OpenMobileAdsErrorPrivate
{
	FString NormalizeCode(const FString& Code)
	{
		FString Result = Code.ToLower();
		Result.ReplaceInline(TEXT("-"), TEXT("_"));
		Result.ReplaceInline(TEXT(" "), TEXT("_"));
		return Result;
	}

	void SetUnknownMapping(
		EOpenMobileAdsErrorDomain Domain,
		FOpenMobileAdsError& Error
	)
	{
		Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
		switch (Domain)
		{
		case EOpenMobileAdsErrorDomain::Provider:
			Error.Explanation = TEXT("The ads provider returned an unrecognized error.");
			Error.LikelyCause = TEXT("The provider SDK returned a new or provider-specific error code.");
			break;
		case EOpenMobileAdsErrorDomain::Mediation:
			Error.Explanation = TEXT("The mediation layer returned an unrecognized error.");
			Error.LikelyCause = TEXT("A mediation adapter or mediated network returned a provider-specific error.");
			break;
		case EOpenMobileAdsErrorDomain::Consent:
			Error.Explanation = TEXT("The consent provider returned an unrecognized error.");
			Error.LikelyCause = TEXT("The consent SDK returned a new or provider-specific error code.");
			break;
		case EOpenMobileAdsErrorDomain::Packaging:
			Error.Explanation = TEXT("The packaged ads integration returned an unrecognized error.");
			Error.LikelyCause = TEXT("A required native setting or packaged dependency may be missing.");
			break;
		}
		Error.SuggestedCorrection = TEXT("Check the sanitized native diagnostics and update the owning provider plugin if the code is new.");
	}

	bool MapProvider(const FString& Code, FOpenMobileAdsError& Error)
	{
		if (Code == TEXT("no_fill"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NoFill;
			Error.Explanation = TEXT("No ad was available for this request.");
			Error.LikelyCause = TEXT("The provider had no eligible inventory for the request.");
			Error.SuggestedCorrection = TEXT("Retry later and confirm the placement, targeting, and account setup.");
			Error.bRetryable = true;
			return true;
		}
		if (Code == TEXT("network_error") || Code == TEXT("timeout"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = TEXT("The ad request could not reach the provider.");
			Error.LikelyCause = Code == TEXT("timeout")
				? TEXT("The provider did not respond before its timeout.")
				: TEXT("The device or provider network connection failed.");
			Error.SuggestedCorrection = TEXT("Check connectivity and retry with the configured retry policy.");
			Error.bRetryable = true;
			return true;
		}
		if (Code == TEXT("invalid_request"))
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderFailure;
			Error.Explanation = TEXT("The provider rejected the ad request as invalid.");
			Error.LikelyCause = TEXT("The placement identifier or request configuration is invalid for this provider.");
			Error.SuggestedCorrection = TEXT("Check the provider placement settings and request options.");
			return true;
		}
		if (Code == TEXT("not_initialized"))
		{
			Error.Code = EOpenMobileAdsErrorCode::InvalidState;
			Error.Explanation = TEXT("The ads provider is not initialized.");
			Error.LikelyCause = TEXT("An ad operation began before provider initialization completed.");
			Error.SuggestedCorrection = TEXT("Wait for successful provider initialization before requesting ads.");
			Error.bRetryable = true;
			return true;
		}
		return false;
	}

	bool MapMediation(const FString& Code, FOpenMobileAdsError& Error)
	{
		if (Code == TEXT("adapter_not_ready"))
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The selected mediation adapter is not ready.");
			Error.LikelyCause = TEXT("The adapter is still initializing or failed to initialize.");
			Error.SuggestedCorrection = TEXT("Check adapter initialization status and packaged adapter dependencies.");
			Error.bRetryable = true;
			return true;
		}
		if (Code == TEXT("no_fill"))
		{
			return MapProvider(Code, Error);
		}
		if (Code == TEXT("adapter_error"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = TEXT("A mediation adapter failed while handling the ad request.");
			Error.LikelyCause = TEXT("The adapter rejected the request or returned a native failure.");
			Error.SuggestedCorrection = TEXT("Check the adapter diagnostics, version, and provider account setup.");
			return true;
		}
		return false;
	}

	bool MapConsent(const FString& Code, FOpenMobileAdsError& Error)
	{
		if (Code == TEXT("consent_required"))
		{
			Error.Code = EOpenMobileAdsErrorCode::PrivacyBlocked;
			Error.Explanation = TEXT("Ads cannot be requested until the required consent flow completes.");
			Error.LikelyCause = TEXT("The current privacy status requires a user decision.");
			Error.SuggestedCorrection = TEXT("Complete the configured consent flow before requesting ads.");
			return true;
		}
		if (Code == TEXT("form_unavailable"))
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The required consent form is unavailable.");
			Error.LikelyCause = TEXT("The form was not loaded or is not configured for this app.");
			Error.SuggestedCorrection = TEXT("Check consent provider configuration and retry form loading.");
			Error.bRetryable = true;
			return true;
		}
		if (Code == TEXT("consent_error"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = TEXT("The consent provider could not complete the request.");
			Error.LikelyCause = TEXT("The consent SDK returned a native failure.");
			Error.SuggestedCorrection = TEXT("Check consent diagnostics and retry when allowed.");
			return true;
		}
		if (Code == TEXT("ump_network") || Code == TEXT("ump_timeout"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = Code == TEXT("ump_timeout")
				? TEXT("The consent provider timed out.")
				: TEXT("The consent provider could not reach its service.");
			Error.LikelyCause = Code == TEXT("ump_timeout")
				? TEXT("The consent request or form did not finish before the provider timeout.")
				: TEXT("The device is offline or the consent service request failed.");
			Error.SuggestedCorrection = TEXT("Check connectivity and retry the consent operation.");
			Error.bRetryable = true;
			return true;
		}
		if (Code == TEXT("ump_configuration"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NotConfigured;
			Error.Explanation = TEXT("Google UMP is not configured for this application.");
			Error.LikelyCause = TEXT("The app ID or Privacy and messaging configuration is invalid.");
			Error.SuggestedCorrection = TEXT("Check the AdMob app ID and published privacy messages.");
			return true;
		}
		if (Code == TEXT("ump_invalid_operation"))
		{
			Error.Code = EOpenMobileAdsErrorCode::InvalidState;
			Error.Explanation = TEXT("Google UMP rejected the consent operation in its current state.");
			Error.LikelyCause = TEXT("A form is unavailable, already used, or presented from an invalid screen.");
			Error.SuggestedCorrection = TEXT("Refresh consent information and retry from an active screen.");
			return true;
		}
		if (Code == TEXT("ump_internal"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = TEXT("Google UMP returned an internal error.");
			Error.LikelyCause = TEXT("The consent SDK could not complete its internal operation.");
			Error.SuggestedCorrection = TEXT("Inspect native diagnostics and retry when appropriate.");
			return true;
		}
		return false;
	}

	bool MapPackaging(const FString& Code, FOpenMobileAdsError& Error)
	{
		if (Code == TEXT("missing_app_id"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NotConfigured;
			Error.Explanation = TEXT("The packaged app is missing the provider app identifier.");
			Error.LikelyCause = TEXT("The provider app ID was empty or was not added to the native package.");
			Error.SuggestedCorrection = TEXT("Set the provider app ID and inspect the merged manifest or property list.");
			return true;
		}
		if (Code == TEXT("missing_sdk") || Code == TEXT("missing_adapter"))
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = Code == TEXT("missing_sdk")
				? TEXT("The packaged app is missing the provider SDK.")
				: TEXT("The packaged app is missing a required mediation adapter.");
			Error.LikelyCause = TEXT("The owning provider or adapter plugin did not stage its native dependency.");
			Error.SuggestedCorrection = TEXT("Enable the owning plugin and inspect the packaged native dependencies.");
			return true;
		}
		if (Code == TEXT("manifest_error") || Code == TEXT("plist_error"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NotConfigured;
			Error.Explanation = TEXT("The packaged app has invalid provider metadata.");
			Error.LikelyCause = TEXT("Required native configuration is missing, malformed, or conflicting.");
			Error.SuggestedCorrection = TEXT("Inspect the merged native metadata and the owning provider plugin settings.");
			return true;
		}
		return false;
	}
}

EOpenMobileAdsRetryClassification FOpenMobileAdsErrorClassifier::Classify(
	const FOpenMobileAdsError& Error
)
{
	switch (Error.Code)
	{
	case EOpenMobileAdsErrorCode::NoFill:
		return EOpenMobileAdsRetryClassification::Retryable;
	case EOpenMobileAdsErrorCode::NativeFailure:
	case EOpenMobileAdsErrorCode::ProviderFailure:
		return Error.bRetryable
			? EOpenMobileAdsRetryClassification::Retryable
			: EOpenMobileAdsRetryClassification::Terminal;
	case EOpenMobileAdsErrorCode::FrequencyCap:
		return Error.bRetryable
			? EOpenMobileAdsRetryClassification::ConditionallyRetryable
			: EOpenMobileAdsRetryClassification::Terminal;
	case EOpenMobileAdsErrorCode::Offline:
	case EOpenMobileAdsErrorCode::PrivacyBlocked:
	case EOpenMobileAdsErrorCode::Busy:
	case EOpenMobileAdsErrorCode::NotReady:
	case EOpenMobileAdsErrorCode::InvalidState:
	case EOpenMobileAdsErrorCode::ProviderUnavailable:
		return EOpenMobileAdsRetryClassification::ConditionallyRetryable;
	default:
		return EOpenMobileAdsRetryClassification::Terminal;
	}
}

FOpenMobileAdsError FOpenMobileAdsErrorMapper::FromNative(
	const FOpenMobileAdsErrorMappingContext& Context,
	const TArray<FString>& SensitiveValues
)
{
	FOpenMobileAdsError Error;
	Error.Stage = Context.Stage;
	Error.Placement = Context.Placement;
	Error.Provider = Context.Provider;
	Error.NativeDiagnostics.NativeCode = Context.NativeCode;
	Error.NativeDiagnostics.NativeMessage = FOpenMobileAdsLog::Redact(
		Context.NativeMessage,
		SensitiveValues
	);
	Error.NativeDiagnostics.Provider = Context.Provider;
	Error.NativeDiagnostics.Network = Context.Network;
	Error.NativeDiagnostics.Adapter = Context.Adapter;

	const FString Code = OpenMobileAdsErrorPrivate::NormalizeCode(Context.NativeCode);
	bool bMapped = false;
	switch (Context.Domain)
	{
	case EOpenMobileAdsErrorDomain::Provider:
		bMapped = OpenMobileAdsErrorPrivate::MapProvider(Code, Error);
		break;
	case EOpenMobileAdsErrorDomain::Mediation:
		bMapped = OpenMobileAdsErrorPrivate::MapMediation(Code, Error);
		break;
	case EOpenMobileAdsErrorDomain::Consent:
		bMapped = OpenMobileAdsErrorPrivate::MapConsent(Code, Error);
		break;
	case EOpenMobileAdsErrorDomain::Packaging:
		bMapped = OpenMobileAdsErrorPrivate::MapPackaging(Code, Error);
		break;
	}
	if (!bMapped)
	{
		OpenMobileAdsErrorPrivate::SetUnknownMapping(Context.Domain, Error);
	}
	return Error;
}

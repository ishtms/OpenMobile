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

	bool MapExternalProviderState(
		const FString& Message,
		FOpenMobileAdsError& Error
	)
	{
		const FString NormalizedMessage = Message.ToLower();
		if (
			NormalizedMessage.Contains(TEXT("publisher data"))
			&& (
				NormalizedMessage.Contains(TEXT("not found"))
				|| NormalizedMessage.Contains(TEXT("cannot be found"))
			)
		)
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The provider could not find publisher data for this ad unit.");
			Error.LikelyCause = TEXT("The provider account or ad unit is not ready in the provider console.");
			Error.SuggestedCorrection = TEXT("Verify the ad unit in the provider console and use official test ads while the provider processes new account data.");
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::ProviderAccount;
			Error.ProviderDocumentationUrl = TEXT("https://support.google.com/admob/answer/9905175");
			return true;
		}
		if (
			NormalizedMessage.Contains(TEXT("account not approved"))
			|| NormalizedMessage.Contains(TEXT("account wasn't approved"))
			|| NormalizedMessage.Contains(TEXT("account being assessed"))
		)
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The provider account is not ready to serve ads.");
			Error.LikelyCause = TEXT("The provider is still assessing the account or did not approve it.");
			Error.SuggestedCorrection = TEXT("Use official test ads for SDK validation and check account status in the provider console.");
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::ProviderAccount;
			Error.ProviderDocumentationUrl = TEXT("https://support.google.com/admob/answer/9905175");
			return true;
		}
		if (
			NormalizedMessage.Contains(TEXT("app not ready"))
			|| NormalizedMessage.Contains(TEXT("app not approved"))
		)
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The provider app is not ready to serve ads.");
			Error.LikelyCause = TEXT("The provider app readiness review has not completed.");
			Error.SuggestedCorrection = TEXT("Use official test ads for SDK validation and check app readiness in the provider console.");
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::ProviderAppReadiness;
			Error.ProviderDocumentationUrl = TEXT("https://support.google.com/admob/answer/12206349");
			return true;
		}
		if (
			NormalizedMessage.Contains(TEXT("provider policy"))
			|| NormalizedMessage.Contains(TEXT("policy block"))
			|| NormalizedMessage.Contains(TEXT("ad serving is disabled"))
		)
		{
			Error.Code = EOpenMobileAdsErrorCode::ProviderUnavailable;
			Error.Explanation = TEXT("The provider has blocked or limited ad serving.");
			Error.LikelyCause = TEXT("The provider reports an account, app, or traffic policy restriction.");
			Error.SuggestedCorrection = TEXT("Use official test ads for SDK validation and inspect the provider policy status separately.");
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::ProviderPolicy;
			Error.ProviderDocumentationUrl = TEXT("https://support.google.com/admob/troubleshooter/12205649");
			return true;
		}
		if (
			NormalizedMessage.Contains(TEXT("no ads meet"))
			&& NormalizedMessage.Contains(TEXT("ecpm floor"))
		)
		{
			Error.Code = EOpenMobileAdsErrorCode::NoFill;
			Error.Explanation = TEXT("No eligible ad met the provider eCPM floor.");
			Error.LikelyCause = TEXT("Available inventory did not meet the configured provider floor.");
			Error.SuggestedCorrection = TEXT("Retry through the no-fill policy or review the provider-console floor separately from SDK validation.");
			Error.bRetryable = true;
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::LiveInventory;
			Error.ProviderDocumentationUrl = TEXT("https://support.google.com/admob/answer/3418058");
			return true;
		}
		return false;
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
			Error.ExternalBlockReason = EOpenMobileAdsExternalBlockReason::LiveInventory;
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
		if (Code == TEXT("invalid_state"))
		{
			Error.Code = EOpenMobileAdsErrorCode::InvalidState;
			Error.Explanation = TEXT("The ads provider rejected the operation in its current state.");
			Error.LikelyCause = TEXT("An equivalent request is active or the native ad object was already used.");
			Error.SuggestedCorrection = TEXT("Wait for the active operation to finish before retrying.");
			return true;
		}
		if (Code == TEXT("missing_app_id"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NotConfigured;
			Error.Explanation = TEXT("The ads provider application identifier is missing.");
			Error.LikelyCause = TEXT("The packaged application does not contain the configured provider app ID.");
			Error.SuggestedCorrection = TEXT("Set the provider app ID and inspect the packaged native metadata.");
			return true;
		}
		if (Code == TEXT("internal_error"))
		{
			Error.Code = EOpenMobileAdsErrorCode::NativeFailure;
			Error.Explanation = TEXT("The ads provider returned an internal load error.");
			Error.LikelyCause = TEXT("The native SDK could not complete the ad request.");
			Error.SuggestedCorrection = TEXT("Inspect native diagnostics and retry with the configured policy.");
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
	if (
		Context.Domain == EOpenMobileAdsErrorDomain::Provider
		|| Context.Domain == EOpenMobileAdsErrorDomain::Mediation
	)
	{
		bMapped = OpenMobileAdsErrorPrivate::MapExternalProviderState(
			Context.NativeMessage,
			Error
		);
	}
	switch (Context.Domain)
	{
	case EOpenMobileAdsErrorDomain::Provider:
		bMapped = bMapped || OpenMobileAdsErrorPrivate::MapProvider(Code, Error);
		break;
	case EOpenMobileAdsErrorDomain::Mediation:
		bMapped = bMapped || OpenMobileAdsErrorPrivate::MapMediation(Code, Error);
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

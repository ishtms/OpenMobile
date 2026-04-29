#include "IOpenMobileAdsProvider.h"

namespace OpenMobileAdsProviderPrivate
{
	FOpenMobileAdsError MakeUnsupportedError(
		FName Provider,
		FName Placement,
		EOpenMobileAdsFailureStage Stage
	)
	{
		return FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::UnsupportedFormat,
			Stage,
			Placement,
			TEXT("The selected provider does not implement this placement operation."),
			Provider,
			TEXT("Choose a provider and format that advertise support for this operation.")
		);
	}
}

FOpenMobileAdsProviderCapabilities IOpenMobileAdsProvider::GetCapabilities() const
{
	FOpenMobileAdsProviderCapabilities Capabilities;
	Capabilities.Provider = GetProviderName();
	return Capabilities;
}

FOpenMobileAdsProviderRequestPolicy IOpenMobileAdsProvider::GetRequestPolicy(
	const FOpenMobileAdsProviderRequestContext&
) const
{
	return FOpenMobileAdsProviderRequestPolicy();
}

bool IOpenMobileAdsProvider::ResetConsentForTesting(
	FOpenMobileAdsError& OutError
)
{
	OutError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderUnavailable,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The selected consent provider does not support test resets."),
		GetProviderName(),
		TEXT("Use a consent provider with a development reset implementation.")
	);
	return false;
}

FOpenMobileAdsConsentSignalApplyResult
IOpenMobileAdsProvider::ApplyConsentSignals(
	const FOpenMobileAdsConsentSignals&,
	int32
)
{
	return FOpenMobileAdsConsentSignalApplyResult();
}

bool IOpenMobileAdsProvider::RefreshConsent(
	const FOpenMobileAdsConsentRequest& Request,
	TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderUnavailable,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The selected ads provider does not supply a consent provider."),
		GetProviderName(),
		TEXT("Enable a provider plugin with a supported consent implementation.")
	);
	return false;
}

bool IOpenMobileAdsProvider::PresentRequiredConsentForm(
	const FOpenMobileAdsConsentRequest& Request,
	TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderUnavailable,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The selected ads provider cannot present a required consent form."),
		GetProviderName(),
		TEXT("Enable and configure the provider's consent implementation.")
	);
	return false;
}

bool IOpenMobileAdsProvider::PresentPrivacyOptionsForm(
	const FOpenMobileAdsConsentRequest& Request,
	TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderUnavailable,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The selected ads provider cannot present a privacy-options form."),
		GetProviderName(),
		TEXT("Enable and configure a consent provider with privacy-options support.")
	);
	return false;
}

bool IOpenMobileAdsProvider::Initialize(
	const FOpenMobileAdsInitializationRequest& Request,
	TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Initialization,
		NAME_None,
		TEXT("The selected ads provider does not implement SDK initialization."),
		GetProviderName(),
		TEXT("Update the provider plugin to implement the current OpenMobile Ads initialization contract.")
	);
	return false;
}

bool IOpenMobileAdsProvider::Load(
	const FOpenMobileAdsLoadRequest& Request,
	TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = OpenMobileAdsProviderPrivate::MakeUnsupportedError(
		GetProviderName(),
		Request.Placement.Placement,
		EOpenMobileAdsFailureStage::Load
	);
	return false;
}

bool IOpenMobileAdsProvider::Show(
	const FOpenMobileAdsShowRequest& Request,
	TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = OpenMobileAdsProviderPrivate::MakeUnsupportedError(
		GetProviderName(),
		Request.Placement,
		EOpenMobileAdsFailureStage::Show
	);
	return false;
}

bool IOpenMobileAdsProvider::Hide(
	const FOpenMobileAdsHideRequest& Request,
	TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = OpenMobileAdsProviderPrivate::MakeUnsupportedError(
		GetProviderName(),
		Request.Placement,
		EOpenMobileAdsFailureStage::Hide
	);
	return false;
}

bool IOpenMobileAdsProvider::Destroy(
	const FOpenMobileAdsDestroyRequest& Request,
	TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
	FOpenMobileAdsError& OutError
)
{
	OutError = OpenMobileAdsProviderPrivate::MakeUnsupportedError(
		GetProviderName(),
		Request.Placement,
		EOpenMobileAdsFailureStage::Teardown
	);
	return false;
}

FOpenMobileAdsProviderSelection FOpenMobileAdsProviderResolver::Resolve(
	const TArray<IOpenMobileAdsProvider*>& Providers,
	FName PreferredProvider
)
{
	TArray<IOpenMobileAdsProvider*> RegisteredProviders;
	TArray<IOpenMobileAdsProvider*> SupportedProviders;
	RegisteredProviders.Reserve(Providers.Num());
	SupportedProviders.Reserve(Providers.Num());
	for (IOpenMobileAdsProvider* Provider : Providers)
	{
		if (!Provider)
		{
			continue;
		}
		RegisteredProviders.Add(Provider);
		if (Provider->IsSupported())
		{
			SupportedProviders.Add(Provider);
		}
	}

	FOpenMobileAdsProviderSelection Result;
	if (!PreferredProvider.IsNone())
	{
		TArray<IOpenMobileAdsProvider*> MatchingProviders;
		for (IOpenMobileAdsProvider* Provider : RegisteredProviders)
		{
			if (Provider->GetProviderName() == PreferredProvider)
			{
				MatchingProviders.Add(Provider);
			}
		}
		if (MatchingProviders.Num() > 1)
		{
			Result.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderConflict,
				EOpenMobileAdsFailureStage::ProviderSelection,
				NAME_None,
				TEXT("More than one enabled ads provider uses the preferred provider name."),
				PreferredProvider,
				TEXT("Disable the duplicate provider plugin or give each provider a unique name.")
			);
			return Result;
		}
		if (MatchingProviders.IsEmpty())
		{
			Result.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::ProviderUnavailable,
				EOpenMobileAdsFailureStage::ProviderSelection,
				NAME_None,
				FString::Printf(
					TEXT("The preferred ads provider '%s' is not available on this platform."),
					*PreferredProvider.ToString()
				),
				PreferredProvider,
				TEXT("Enable the provider plugin, verify platform support, or select another provider.")
			);
			return Result;
		}
		if (!MatchingProviders[0]->IsSupported())
		{
			Result.Error = FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::UnsupportedPlatform,
				EOpenMobileAdsFailureStage::ProviderSelection,
				NAME_None,
				FString::Printf(
					TEXT("The preferred ads provider '%s' does not support this platform."),
					*PreferredProvider.ToString()
				),
				PreferredProvider,
				TEXT("Run on a supported target platform or select another provider.")
			);
			return Result;
		}
		Result.Provider = MatchingProviders[0];
		return Result;
	}

	if (SupportedProviders.Num() == 1)
	{
		Result.Provider = SupportedProviders[0];
		return Result;
	}

	if (SupportedProviders.IsEmpty())
	{
		const bool bHasRegisteredProviders = !RegisteredProviders.IsEmpty();
		Result.Error = FOpenMobileAdsError::Make(
			bHasRegisteredProviders
				? EOpenMobileAdsErrorCode::UnsupportedPlatform
				: EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::ProviderSelection,
			NAME_None,
			bHasRegisteredProviders
				? TEXT("Enabled ads providers do not support this platform.")
				: TEXT("No ads provider is enabled."),
			NAME_None,
			bHasRegisteredProviders
				? TEXT("Run on a supported target platform or enable a compatible provider.")
				: TEXT("Enable and configure one ads provider plugin for the target platform.")
		);
		return Result;
	}

	SupportedProviders.Sort(
		[](const IOpenMobileAdsProvider& Left, const IOpenMobileAdsProvider& Right)
		{
			return Left.GetProviderName().LexicalLess(Right.GetProviderName());
		}
	);
	TArray<FString> ProviderNames;
	ProviderNames.Reserve(SupportedProviders.Num());
	for (const IOpenMobileAdsProvider* Provider : SupportedProviders)
	{
		ProviderNames.Add(Provider->GetProviderName().ToString());
	}
	Result.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderConflict,
		EOpenMobileAdsFailureStage::ProviderSelection,
		NAME_None,
		FString::Printf(
			TEXT("Multiple ads providers are available: %s."),
			*FString::Join(ProviderNames, TEXT(", "))
		),
		NAME_None,
		TEXT("Set PreferredProvider in OpenMobile Ads settings.")
	);
	return Result;
}

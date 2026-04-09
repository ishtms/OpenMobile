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
	TArray<IOpenMobileAdsProvider*> SupportedProviders;
	SupportedProviders.Reserve(Providers.Num());
	for (IOpenMobileAdsProvider* Provider : Providers)
	{
		if (Provider && Provider->IsSupported())
		{
			SupportedProviders.Add(Provider);
		}
	}

	FOpenMobileAdsProviderSelection Result;
	if (!PreferredProvider.IsNone())
	{
		for (IOpenMobileAdsProvider* Provider : SupportedProviders)
		{
			if (Provider->GetProviderName() != PreferredProvider)
			{
				continue;
			}
			if (Result.Provider)
			{
				Result.Provider = nullptr;
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
			Result.Provider = Provider;
		}
		if (!Result.Provider)
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
		}
		return Result;
	}

	if (SupportedProviders.Num() == 1)
	{
		Result.Provider = SupportedProviders[0];
		return Result;
	}

	if (SupportedProviders.IsEmpty())
	{
		Result.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderUnavailable,
			EOpenMobileAdsFailureStage::ProviderSelection,
			NAME_None,
			TEXT("No enabled ads provider supports this platform."),
			NAME_None,
			TEXT("Enable and configure one ads provider plugin for the target platform.")
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

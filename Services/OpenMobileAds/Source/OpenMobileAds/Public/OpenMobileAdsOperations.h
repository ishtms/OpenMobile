#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsOperations.generated.h"

struct OPENMOBILEADS_API FOpenMobileAdsDevelopmentConfiguration
{
	bool bEnabled = false;
	bool bUseTestDevices = false;
	bool bUseTestAdUnitIds = false;
	bool bEnableConsentDebug = false;
	bool bEnableVerboseDiagnostics = false;
	TArray<FString> TestDeviceIdentifiers;

	static bool IsValidTestDeviceIdentifier(const FString& Identifier)
	{
		if (Identifier.IsEmpty() || Identifier.Len() > 256)
		{
			return false;
		}
		for (const TCHAR Character : Identifier)
		{
			if (Character < TEXT('!') || Character > TEXT('~'))
			{
				return false;
			}
		}
		return true;
	}

	static TArray<FString> MergeTestDeviceIdentifiers(
		const TArray<FString>& GlobalIdentifiers,
		const TArray<FString>& ProviderIdentifiers = {}
	)
	{
		TArray<FString> Result;
		Result.Reserve(GlobalIdentifiers.Num() + ProviderIdentifiers.Num());
		auto AppendUnique = [&Result](const TArray<FString>& Identifiers)
		{
			for (const FString& Identifier : Identifiers)
			{
				if (
					!IsValidTestDeviceIdentifier(Identifier)
					|| Result.ContainsByPredicate(
						[&Identifier](const FString& Existing)
						{
							return Existing.Equals(Identifier, ESearchCase::IgnoreCase);
						}
					)
				)
				{
					continue;
				}
				Result.Add(Identifier);
			}
		};
		AppendUnique(GlobalIdentifiers);
		AppendUnique(ProviderIdentifiers);
		return Result;
	}

	static FOpenMobileAdsDevelopmentConfiguration FromMode(
		bool bEnabled,
		const TArray<FString>& ConfiguredTestDeviceIdentifiers = {}
	)
	{
		FOpenMobileAdsDevelopmentConfiguration Configuration;
		Configuration.bEnabled = bEnabled;
		Configuration.bUseTestDevices = bEnabled;
		Configuration.bUseTestAdUnitIds = bEnabled;
		Configuration.bEnableConsentDebug = bEnabled;
		Configuration.bEnableVerboseDiagnostics = bEnabled;
		if (bEnabled)
		{
			Configuration.TestDeviceIdentifiers = MergeTestDeviceIdentifiers(
				ConfiguredTestDeviceIdentifiers
			);
		}
		return Configuration;
	}
};

struct OPENMOBILEADS_API FOpenMobileAdsInitializationRequest
{
	FGuid RequestId;

	EOpenMobileAdsPlatform Platform = EOpenMobileAdsPlatform::Unsupported;

	FOpenMobileAdsDevelopmentConfiguration Development;

	FOpenMobileAdsPrivacyConfiguration Privacy;

	FOpenMobileAdsProviderRequestContext PrivacyContext;

	FOpenMobileAdsRequestConfiguration RequestConfiguration;
};

struct OPENMOBILEADS_API FOpenMobileAdsConsentRequest
{
	FGuid RequestId;

	EOpenMobileAdsPlatform Platform = EOpenMobileAdsPlatform::Unsupported;

	FOpenMobileAdsDevelopmentConfiguration Development;

	FOpenMobileAdsPrivacyConfiguration Privacy;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsLoadOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bForceReload = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsShowOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FString ServerVerificationCustomData;
};

struct OPENMOBILEADS_API FOpenMobileAdsLoadRequest
{
	FGuid RequestId;

	FOpenMobileAdsResolvedPlacement Placement;

	FOpenMobileAdsLoadOptions Options;

	FOpenMobileAdsProviderRequestContext PrivacyContext;
};

struct OPENMOBILEADS_API FOpenMobileAdsShowRequest
{
	FGuid RequestId;

	FGuid CachedAdId;

	FName Placement;

	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	FOpenMobileAdsShowOptions Options;
};

struct OPENMOBILEADS_API FOpenMobileAdsDestroyRequest
{
	FGuid RequestId;

	FName Placement;

	bool bAllPlacements = false;
};

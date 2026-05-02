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
	EOpenMobileAdsDebugGeography DebugGeography =
		EOpenMobileAdsDebugGeography::Disabled;

	EOpenMobileAdsDebugGeography GetEffectiveDebugGeography() const
	{
		if (
			!bEnabled
			|| !bEnableConsentDebug
			|| TestDeviceIdentifiers.IsEmpty()
		)
		{
			return EOpenMobileAdsDebugGeography::Disabled;
		}
		switch (DebugGeography)
		{
		case EOpenMobileAdsDebugGeography::Eea:
		case EOpenMobileAdsDebugGeography::RegulatedUsState:
		case EOpenMobileAdsDebugGeography::Other:
			return DebugGeography;
		default:
			return EOpenMobileAdsDebugGeography::Disabled;
		}
	}

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
		const TArray<FString>& ConfiguredTestDeviceIdentifiers = {},
		EOpenMobileAdsDebugGeography ConfiguredDebugGeography =
			EOpenMobileAdsDebugGeography::Disabled
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
			Configuration.DebugGeography = ConfiguredDebugGeography;
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
struct OPENMOBILEADS_API FOpenMobileAdsAppOpenPresentationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bApplicationReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bColdStartLoadingScreenVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bPresentationSuppressed = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsShowOptions
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (
			ToolTip = "Set only after presenting the rewarded-interstitial introduction with clear reward messaging and a skip option."
		)
	)
	bool bRewardedInterstitialIntroductionPresented = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Optional per-show user identifier sent only to the rewarded provider's server-verification callback. Do not put private credentials here.")
	)
	FString ServerVerificationUserId;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Optional per-show correlation data sent only to the rewarded provider's server-verification callback. Do not put private credentials here.")
	)
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

	FOpenMobileAdsBannerLayout BannerLayout;

	FOpenMobileAdsServerVerificationSettings ServerVerification;

	FOpenMobileAdsShowOptions Options;
};

struct OPENMOBILEADS_API FOpenMobileAdsHideRequest
{
	FGuid RequestId;

	FGuid CachedAdId;

	FName Placement;

	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Banner;

	bool bPreserveCachedAd = false;
};

struct OPENMOBILEADS_API FOpenMobileAdsDestroyRequest
{
	FGuid RequestId;

	FName Placement;

	bool bAllPlacements = false;
};

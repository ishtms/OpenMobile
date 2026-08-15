#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsAdMobSettings.generated.h"

/** Keeps AdMob app IDs, legacy rewarded IDs, and provider test devices separate from placements. */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "OpenMobile - AdMob"))
class OPENMOBILEADSADMOB_API UOpenMobileAdsAdMobSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Keeps provider settings inside the shared OpenMobile Project Settings category. */
	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	/** Gives AdMob its own provider section instead of mixing SDK values into core Ads settings. */
	virtual FName GetSectionName() const override { return TEXT("OpenMobile - AdMob"); }

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidAppId = TEXT("ca-app-pub-3940256099942544~3347511713");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Legacy",
		meta = (
			DisplayName = "Legacy Convenience Rewarded Ad Unit ID (Android)",
			ToolTip = "Used only by the deprecated convenience rewarded flow. Named placements own every production ad-unit ID."
		)
	)
	FString AndroidRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "iOS",
		meta = (DisplayName = "iOS App ID", ToolTip = "AdMob iOS app ID. UE 5.8 direct Xcode builds must also keep iOS Additional Plist Data in sync; packaged builds use this value through the provider UPL.")
	)
	FString IOSAppId = TEXT("ca-app-pub-3940256099942544~1458002511");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Legacy",
		meta = (
			DisplayName = "Legacy Convenience Rewarded Ad Unit ID (iOS)",
			ToolTip = "Used only by the deprecated convenience rewarded flow. Named placements own every production ad-unit ID."
		)
	)
	FString IOSRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/1712485313");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "Development",
		meta = (
			DisplayName = "AdMob Test Device Identifiers",
			ToolTip = "Opaque AdMob test-device identifiers merged with the global list while Development/Test Mode is enabled. Shipping builds reject non-empty values."
		)
	)
	TArray<FString> TestDeviceIdentifiers;

	/** Merges global and AdMob device IDs through the same validation and duplicate rules. */
	TArray<FString> ResolveTestDeviceIdentifiers(
		const TArray<FString>& GlobalIdentifiers
	) const
	{
		return FOpenMobileAdsDevelopmentConfiguration::MergeTestDeviceIdentifiers(
			GlobalIdentifiers,
			TestDeviceIdentifiers
		);
	}

	/** Identifies Google's official sample prefix so production requests can't use test inventory. */
	static bool IsGoogleSampleIdentifier(const FString& Identifier)
	{
		return Identifier.TrimStartAndEnd().StartsWith(
			TEXT("ca-app-pub-3940256099942544")
		);
	}

	/** Returns the packaged app ID for the selected platform and nothing for unsupported targets. */
	FString GetAppId(EOpenMobileAdsPlatform Platform) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return AndroidAppId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return IOSAppId;
		}
		return FString();
	}

	/** Returns the configured rewarded unit without substituting Google's sample value. */
	FString GetRewardedAdUnitId(EOpenMobileAdsPlatform Platform) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return AndroidRewardedAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return IOSRewardedAdUnitId;
		}
		return FString();
	}

	/** Validates only the current platform app ID before provider initialization. */
	bool IsConfigurationCompatibleWithMode(
		EOpenMobileAdsPlatform Platform,
		bool bDevelopmentTestMode,
		FString& OutError
	) const
	{
		FString AppId = GetAppId(Platform);
		AppId.TrimStartAndEndInline();
		bool bContainsWhitespace = false;
		for (const TCHAR Character : AppId)
		{
			if (FChar::IsWhitespace(Character))
			{
				bContainsWhitespace = true;
				break;
			}
		}
		if (
			AppId.IsEmpty()
			|| bContainsWhitespace
			|| !AppId.StartsWith(TEXT("ca-app-pub-"))
			|| !AppId.Contains(TEXT("~"))
		)
		{
			OutError = TEXT("AdMob requires a valid app ID for the current platform.");
			return false;
		}
		if (!bDevelopmentTestMode && IsGoogleSampleIdentifier(AppId))
		{
			OutError = TEXT("AdMob production mode cannot use Google's sample app ID.");
			return false;
		}
		return true;
	}

	/** Uses Google's rewarded sample in test mode and rejects that same value in production. */
	FString ResolveRewardedAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/5224354917")
				: IsGoogleSampleIdentifier(AndroidRewardedAdUnitId)
					? FString()
					: AndroidRewardedAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/1712485313")
				: IsGoogleSampleIdentifier(IOSRewardedAdUnitId)
					? FString()
					: IOSRewardedAdUnitId;
		}
		return FString();
	}

	/** Returns Google's interstitial sample only when official test IDs are active. */
	FString ResolveInterstitialAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (!bUseTestAdUnitId)
		{
			return FString();
		}
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return TEXT("ca-app-pub-3940256099942544/1033173712");
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return TEXT("ca-app-pub-3940256099942544/4411468910");
		}
		return FString();
	}

	/** Supports rewarded interstitials only through Google's test unit till project configuration exposes one. */
	FString ResolveRewardedInterstitialAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (!bUseTestAdUnitId)
		{
			return FString();
		}
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return TEXT("ca-app-pub-3940256099942544/5354046379");
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return TEXT("ca-app-pub-3940256099942544/6978759866");
		}
		return FString();
	}

	/** Supports App Open only through Google's test unit till project configuration exposes one. */
	FString ResolveAppOpenAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (!bUseTestAdUnitId)
		{
			return FString();
		}
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return TEXT("ca-app-pub-3940256099942544/9257395921");
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return TEXT("ca-app-pub-3940256099942544/5575463023");
		}
		return FString();
	}

	/** Returns Google's banner sample only when official test IDs are active. */
	FString ResolveBannerAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (!bUseTestAdUnitId)
		{
			return FString();
		}
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return TEXT("ca-app-pub-3940256099942544/6300978111");
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return TEXT("ca-app-pub-3940256099942544/2435281174");
		}
		return FString();
	}
};

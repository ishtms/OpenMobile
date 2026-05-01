#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsAdMobSettings.generated.h"

/** AdMob-owned build, ad-unit, and test-device configuration. */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "OpenMobile - AdMob"))
class OPENMOBILEADSADMOB_API UOpenMobileAdsAdMobSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	virtual FName GetSectionName() const override { return TEXT("OpenMobile - AdMob"); }

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidAppId = TEXT("ca-app-pub-3940256099942544~3347511713");

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidInterstitialAdUnitId = TEXT("ca-app-pub-3940256099942544/1033173712");

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidBannerAdUnitId = TEXT("ca-app-pub-3940256099942544/6300978111");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "iOS",
		meta = (ToolTip = "AdMob iOS app ID. UE 5.8 direct Xcode builds must also keep iOS Additional Plist Data in sync; packaged builds use this value through the provider UPL.")
	)
	FString IOSAppId = TEXT("ca-app-pub-3940256099942544~1458002511");

	UPROPERTY(Config, EditAnywhere, Category = "iOS")
	FString IOSRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/1712485313");

	UPROPERTY(Config, EditAnywhere, Category = "iOS")
	FString IOSInterstitialAdUnitId = TEXT("ca-app-pub-3940256099942544/4411468910");

	UPROPERTY(Config, EditAnywhere, Category = "iOS")
	FString IOSBannerAdUnitId = TEXT("ca-app-pub-3940256099942544/2435281174");

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

	TArray<FString> ResolveTestDeviceIdentifiers(
		const TArray<FString>& GlobalIdentifiers
	) const
	{
		return FOpenMobileAdsDevelopmentConfiguration::MergeTestDeviceIdentifiers(
			GlobalIdentifiers,
			TestDeviceIdentifiers
		);
	}

	static bool IsGoogleSampleIdentifier(const FString& Identifier)
	{
		return Identifier.TrimStartAndEnd().StartsWith(
			TEXT("ca-app-pub-3940256099942544")
		);
	}

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

	FString GetInterstitialAdUnitId(EOpenMobileAdsPlatform Platform) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return AndroidInterstitialAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return IOSInterstitialAdUnitId;
		}
		return FString();
	}

	FString GetBannerAdUnitId(EOpenMobileAdsPlatform Platform) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return AndroidBannerAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return IOSBannerAdUnitId;
		}
		return FString();
	}

	bool IsConfigurationCompatibleWithMode(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAds,
		FString& OutError
	) const
	{
		if (bUseTestAds)
		{
			return true;
		}
		if (
			IsGoogleSampleIdentifier(GetAppId(Platform))
			|| IsGoogleSampleIdentifier(GetRewardedAdUnitId(Platform))
			|| IsGoogleSampleIdentifier(GetInterstitialAdUnitId(Platform))
			|| IsGoogleSampleIdentifier(GetBannerAdUnitId(Platform))
		)
		{
			OutError = TEXT("AdMob production mode cannot use Google sample identifiers.");
			return false;
		}
		return true;
	}

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

	FString ResolveInterstitialAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/1033173712")
				: IsGoogleSampleIdentifier(AndroidInterstitialAdUnitId)
					? FString()
					: AndroidInterstitialAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/4411468910")
				: IsGoogleSampleIdentifier(IOSInterstitialAdUnitId)
					? FString()
					: IOSInterstitialAdUnitId;
		}
		return FString();
	}

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

	FString ResolveBannerAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/6300978111")
				: IsGoogleSampleIdentifier(AndroidBannerAdUnitId)
					? FString()
					: AndroidBannerAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/2435281174")
				: IsGoogleSampleIdentifier(IOSBannerAdUnitId)
					? FString()
					: IOSBannerAdUnitId;
		}
		return FString();
	}
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsAdMobSettings.generated.h"

/** AdMob-owned build, ad-unit, and test-device configuration. */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "AdMob"))
class OPENMOBILEADSADMOB_API UOpenMobileAdsAdMobSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("OpenMobile - AdMob"); }

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidAppId = TEXT("ca-app-pub-3940256099942544~3347511713");

	UPROPERTY(Config, EditAnywhere, Category = "Android")
	FString AndroidRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "iOS",
		meta = (ToolTip = "AdMob iOS app ID. UE 5.8 direct Xcode builds must also keep iOS Additional Plist Data in sync; packaged builds use this value through the provider UPL.")
	)
	FString IOSAppId = TEXT("ca-app-pub-3940256099942544~1458002511");

	UPROPERTY(Config, EditAnywhere, Category = "iOS")
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
};

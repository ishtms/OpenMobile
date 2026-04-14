#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsAdMobSettings.generated.h"

/** AdMob-owned build and ad-unit configuration. Values are identifiers, not secrets. */
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

	FString ResolveRewardedAdUnitId(
		EOpenMobileAdsPlatform Platform,
		bool bUseTestAdUnitId
	) const
	{
		if (Platform == EOpenMobileAdsPlatform::Android)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/5224354917")
				: AndroidRewardedAdUnitId;
		}
		if (Platform == EOpenMobileAdsPlatform::IOS)
		{
			return bUseTestAdUnitId
				? TEXT("ca-app-pub-3940256099942544/1712485313")
				: IOSRewardedAdUnitId;
		}
		return FString();
	}
};

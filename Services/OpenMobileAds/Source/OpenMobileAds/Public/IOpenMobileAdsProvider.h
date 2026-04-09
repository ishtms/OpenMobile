#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileCoreTypes.h"

DECLARE_DELEGATE(FOpenMobileRewardedAdLoadedCallback);
DECLARE_DELEGATE(FOpenMobileRewardedAdShownCallback);
DECLARE_DELEGATE_TwoParams(FOpenMobileRewardedAdEarnedCallback, int32, FString);
DECLARE_DELEGATE(FOpenMobileRewardedAdClosedCallback);
DECLARE_DELEGATE_OneParam(FOpenMobileRewardedAdFailedCallback, FOpenMobileError);

struct OPENMOBILEADS_API FOpenMobileRewardedAdCallbacks
{
	FOpenMobileRewardedAdLoadedCallback OnLoaded;
	FOpenMobileRewardedAdShownCallback OnShown;
	FOpenMobileRewardedAdEarnedCallback OnEarned;
	FOpenMobileRewardedAdClosedCallback OnClosed;
	FOpenMobileRewardedAdFailedCallback OnFailed;
};

/** Public, versioned SPI implemented by independently enabled ad-provider plugins. */
class OPENMOBILEADS_API IOpenMobileAdsProvider : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsProvider() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Ads.Provider"));
		return FeatureName;
	}

	virtual FName GetProviderName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsSupported() const = 0;

	virtual bool RequestAndShowRewardedAd(
		FOpenMobileRewardedAdCallbacks&& Callbacks,
		FOpenMobileError& OutError
	) = 0;
};

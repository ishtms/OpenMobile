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

	static FOpenMobileAdsDevelopmentConfiguration FromMode(bool bEnabled)
	{
		FOpenMobileAdsDevelopmentConfiguration Configuration;
		Configuration.bEnabled = bEnabled;
		Configuration.bUseTestDevices = bEnabled;
		Configuration.bUseTestAdUnitIds = bEnabled;
		Configuration.bEnableConsentDebug = bEnabled;
		Configuration.bEnableVerboseDiagnostics = bEnabled;
		return Configuration;
	}
};

struct OPENMOBILEADS_API FOpenMobileAdsInitializationRequest
{
	FGuid RequestId;

	EOpenMobileAdsPlatform Platform = EOpenMobileAdsPlatform::Unsupported;

	FOpenMobileAdsDevelopmentConfiguration Development;

	FOpenMobileAdsPrivacyConfiguration Privacy;

	FOpenMobileAdsRequestConfiguration RequestConfiguration;
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

#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsOperations.generated.h"

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

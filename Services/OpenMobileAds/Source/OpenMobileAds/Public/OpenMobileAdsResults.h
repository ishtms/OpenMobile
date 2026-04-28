#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsResults.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;

	static FOpenMobileAdsOperationResult Accepted(FGuid InRequestId)
	{
		FOpenMobileAdsOperationResult Result;
		Result.bAccepted = true;
		Result.RequestId = InRequestId;
		return Result;
	}

	static FOpenMobileAdsOperationResult Rejected(FOpenMobileAdsError InError)
	{
		FOpenMobileAdsOperationResult Result;
		Result.Error = MoveTemp(InError);
		return Result;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileAdsCanShowBlockReason : uint8
{
	None,
	UnknownPlacement,
	Disabled,
	NotInitialized,
	ProviderUnavailable,
	UnsupportedFormat,
	PrivacyBlocked,
	Loading,
	NotLoaded,
	Expired,
	FrequencyCap,
	Cooldown,
	Offline,
	LifecycleConflict
};

UENUM(BlueprintType)
enum class EOpenMobileAdsFrequencyCapScope : uint8
{
	None,
	Session,
	RollingWindow
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsCanShowResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanShow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsCanShowBlockReason BlockReason = EOpenMobileAdsCanShowBlockReason::NotLoaded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime NextEligibleAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsFrequencyCapScope FrequencyCapScope =
		EOpenMobileAdsFrequencyCapScope::None;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPlacementStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdPlacementState State = EOpenMobileAdPlacementState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid ActiveRequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid CachedAdId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime CachedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime ExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError LastError;
};

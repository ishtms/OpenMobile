#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsRevenue.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsEvents.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsEventType : uint8
{
	ProviderRegistered,
	ProviderUnregistered,
	PlacementStateChanged,
	LoadStarted,
	Loaded,
	LoadFailed,
	ShowAccepted,
	Shown,
	Impression,
	Clicked,
	Dismissed,
	RewardEarned,
	RevenuePaid,
	Refreshed,
	Destroyed,
	Failed,
	Expired
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Type;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Normalized reward amount from 1 through MAX_int64. Zero is not grantable.")
	)
	int64 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bServerVerified = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString VerificationId;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsEventType Type = EOpenMobileAdsEventType::PlacementStateChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdPlacementState PlacementState = EOpenMobileAdPlacementState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Network;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid CachedAdId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime CacheExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bHasReward = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsReward Reward;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bHasRevenue = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsRevenue Revenue;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsNativeEvent,
	const FOpenMobileAdsEvent&
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsDynamicEvent,
	const FOpenMobileAdsEvent&,
	Event
);

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
	Expired,
	Hidden
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

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "True when the placement requested a separate backend server-verification callback. This local event is not proof that the backend received or verified it.")
	)
	bool bServerVerificationRequested = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Local provider callbacks cannot assert backend verification. Use Server Verification Requested and grant on the backend."
		)
	)
	bool bServerVerified = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Backend transaction identifiers never enter the Unreal reward event."
		)
	)
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

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Service-owned identity shared by one impression and its revenue reports.")
	)
	FGuid ImpressionId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime CacheExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bHasReward = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsReward Reward;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "True when Revenue contains a provider-reported amount. Reported zero is valid.")
	)
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

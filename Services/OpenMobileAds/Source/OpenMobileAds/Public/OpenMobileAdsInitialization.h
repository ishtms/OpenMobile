#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsInitialization.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsInitializationComponentType : uint8
{
	Provider,
	Network,
	Adapter
};

UENUM(BlueprintType)
enum class EOpenMobileAdsInitializationState : uint8
{
	NotStarted,
	Initializing,
	Ready,
	Failed,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsInitializationComponentStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsInitializationComponentType Type =
		EOpenMobileAdsInitializationComponentType::Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Parent;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsInitializationState State =
		EOpenMobileAdsInitializationState::NotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Version;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	double LatencyMilliseconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bHasCapabilities = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsProviderCapabilities Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsInitializationStatusSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsServiceState ServiceState = EOpenMobileAdsServiceState::Uninitialized;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime StartedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime LastUpdated;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	double LatencyMilliseconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bPartialSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	TArray<FOpenMobileAdsInitializationComponentStatus> Components;

	const FOpenMobileAdsInitializationComponentStatus* FindComponent(
		EOpenMobileAdsInitializationComponentType Type,
		FName Name,
		FName Parent = NAME_None
	) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsInitializationStatusNativeEvent,
	const FOpenMobileAdsInitializationStatusSnapshot&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsInitializationStatusDynamicEvent,
	const FOpenMobileAdsInitializationStatusSnapshot&,
	Status
);

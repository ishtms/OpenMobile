#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsRevenue.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsRevenuePrecision : uint8
{
	Unknown,
	Estimated,
	PublisherProvided,
	Precise
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsRevenue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	int64 ValueMicros = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString CurrencyCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsRevenuePrecision Precision = EOpenMobileAdsRevenuePrecision::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Network;
};

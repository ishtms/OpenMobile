#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsRevenue.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsRevenuePrecision : uint8
{
	Unknown,
	Estimated,
	PublisherProvided UMETA(DisplayName = "Publisher Provided"),
	Precise UMETA(DisplayName = "Exact")
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsRevenue
{
	GENERATED_BODY()

	static constexpr int64 MicrosPerMajorUnit = 1000000;

	static bool TryConvertMajorUnitsToMicros(
		double MajorUnits,
		int64& OutValueMicros
	);

	static bool TryScaleToMicros(
		int64 ProviderValue,
		int64 MicrosPerProviderUnit,
		int64& OutValueMicros
	);

	static bool TryNormalizeCurrencyCode(
		const FString& ProviderCurrencyCode,
		FString& OutCurrencyCode
	);

	static EOpenMobileAdsRevenuePrecision NormalizePrecision(
		EOpenMobileAdsRevenuePrecision ProviderPrecision
	);

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Revenue in micro-units, where 1,000,000 micros equal one major currency unit.")
	)
	int64 ValueMicros = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Uppercase three-letter ISO 4217 currency code, or empty when unavailable or invalid.")
	)
	FString CurrencyCode;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported certainty for the revenue amount. Unknown is used for missing or unrecognized provider values.")
	)
	EOpenMobileAdsRevenuePrecision Precision = EOpenMobileAdsRevenuePrecision::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Network;
};

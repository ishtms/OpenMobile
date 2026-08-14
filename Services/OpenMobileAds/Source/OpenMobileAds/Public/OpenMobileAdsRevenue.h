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
struct OPENMOBILEADS_API FOpenMobileAdsRevenueSource
{
	GENERATED_BODY()

	/** Keeps a source absent only when the provider supplied no network, adapter, or instance identity. */
	bool IsEmpty() const
	{
		return SourceName.IsEmpty()
			&& SourceId.IsEmpty()
			&& AdapterClassName.IsEmpty()
			&& InstanceName.IsEmpty()
			&& InstanceId.IsEmpty();
	}

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported display name for the source that served the ad, or empty when unavailable.")
	)
	FString SourceName;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported stable source identifier, or empty when unavailable.")
	)
	FString SourceId;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported adapter class or adapter identifier, or empty when unavailable.")
	)
	FString AdapterClassName;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported source instance name, or empty when unavailable.")
	)
	FString InstanceName;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported stable source instance identifier, or empty when unavailable.")
	)
	FString InstanceId;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsEcpmValue
{
	GENERATED_BODY()

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "eCPM in micro-units per one thousand impressions. Presence is reported by the containing structure.")
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
		meta = (ToolTip = "Provider-reported certainty for this eCPM value.")
	)
	EOpenMobileAdsRevenuePrecision Precision = EOpenMobileAdsRevenuePrecision::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsEcpm
{
	GENERATED_BODY()

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "True only when the provider supplied a valid non-negative eCPM value.")
	)
	bool bHasProviderReported = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Provider-reported eCPM. Read only when Has Provider Reported is true.")
	)
	FOpenMobileAdsEcpmValue ProviderReported;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "True when the service safely derived eCPM from impression-level revenue.")
	)
	bool bHasDerived = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Service-derived eCPM calculated as impression revenue multiplied by one thousand. Read only when Has Derived is true.")
	)
	FOpenMobileAdsEcpmValue Derived;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsRevenue
{
	GENERATED_BODY()

	static constexpr int64 MicrosPerMajorUnit = 1000000;

	/** Converts a finite major-unit value to integer micros without rounding past the int64 range. */
	static bool TryConvertMajorUnitsToMicros(
		double MajorUnits,
		int64& OutValueMicros
	);

	/** Scales a provider integer using checked arithmetic before storing canonical micros. */
	static bool TryScaleToMicros(
		int64 ProviderValue,
		int64 MicrosPerProviderUnit,
		int64& OutValueMicros
	);

	/** Accepts only trimmed three-letter currency codes and normalizes their case. */
	static bool TryNormalizeCurrencyCode(
		const FString& ProviderCurrencyCode,
		FString& OutCurrencyCode
	);

	/** Converts unknown provider precision values to the stable fallback instead of exposing invalid enums. */
	static EOpenMobileAdsRevenuePrecision NormalizePrecision(
		EOpenMobileAdsRevenuePrecision ProviderPrecision
	);

	/** Trims optional mediation source fields so updates compare consistently. */
	void NormalizeSource();
	/** Normalizes provider and derived eCPM values independently without claiming either one exists. */
	void NormalizeEcpm();

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

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "One-based revision of the accepted revenue report for this impression. Zero means no report.")
	)
	int32 Revision = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "True when this changed report supersedes the previous revision for the impression. The amount is not a delta.")
	)
	bool bIsUpdate = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (
			DisplayName = "eCPM",
			ToolTip = "Provider-reported and service-derived eCPM values with independent presence flags."
		)
	)
	FOpenMobileAdsEcpm Ecpm;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsRevenueSource Source;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Network;
};

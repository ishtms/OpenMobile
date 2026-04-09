#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentStatus : uint8
{
	Unknown,
	Required,
	Granted,
	Denied,
	NotRequired
};

UENUM(BlueprintType)
enum class EOpenMobileAdsAgeTreatment : uint8
{
	Unspecified,
	No,
	Yes
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPrivacySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentStatus ConsentStatus = EOpenMobileAdsConsentStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanRequestAds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Source;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime LastUpdated;
};
